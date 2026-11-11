#include <string.h>
#include "seedlessds/platform.h"
#include <stdint.h>
#include <errno.h>
#include "wifi/wifi_core.h"
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "present_hook.h"
#include "frontend/video_out_gl.h"
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "seedlessds/nds.h"

static int (*core_lock)(void *);
static int (*core_unlock)(void *);
static int (*core_signal)(void *);

void (*recon_present_flip_hook)(const void *page, unsigned idx);

static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t output_condition;
static pthread_once_t output_once = PTHREAD_ONCE_INIT;
static int output_pacing;
static uint64_t output_issued;
static uint64_t output_completed;
static uint64_t output_completed_ns;
static uint64_t output_deferred_token;
static uint64_t output_deferred_deadline_ns;
static uint64_t output_gpu_completed_token;
static uint64_t output_latch_token;
static uint64_t output_latch_ns;
static uint64_t output_observed_latch_ns;
static struct {
    uint64_t token;
    uint64_t input_ns;
    uint64_t handoff_ns;
} output_inputs[16];
static uint64_t output_budget_history[60];
static uint64_t output_budget_ns;
static uint64_t output_budget_token;
static uint64_t output_budget_updated_ns;
static uint64_t output_budget_rendered_ns;
static unsigned output_budget_count;
static unsigned output_budget_index;
uint64_t recon_output_page_token[2];
__thread uint64_t recon_output_render_token;

#define OUTPUT_PERIOD_NS 16666667u

static uint64_t output_clock_ns(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}

static void output_condition_init(void)
{
    pthread_condattr_t attributes;
    pthread_condattr_init(&attributes);
#ifndef __APPLE__
    pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
#endif
    pthread_cond_init(&output_condition, &attributes);
    pthread_condattr_destroy(&attributes);
}

static void output_phase_reset(void)
{
    output_deferred_token = 0;
    output_deferred_deadline_ns = 0;
    output_gpu_completed_token = 0;
    output_latch_token = 0;
    output_latch_ns = 0;
    output_observed_latch_ns = 0;
    memset(output_inputs, 0, sizeof output_inputs);
    output_budget_ns = 0;
    output_budget_token = 0;
    output_budget_updated_ns = 0;
    output_budget_rendered_ns = 0;
    output_budget_count = 0;
    output_budget_index = 0;
}

void nds_set_output_pacing(nds_t *nds, int enabled)
{
    (void)nds;
    pthread_once(&output_once, output_condition_init);
    pthread_mutex_lock(&output_mutex);
    if (output_pacing != (enabled != 0)) {
        output_completed = output_issued;
        output_completed_ns = 0;
        output_phase_reset();
        __atomic_store_n(&output_pacing, enabled != 0, __ATOMIC_RELEASE);
        pthread_cond_broadcast(&output_condition);
    }
    pthread_mutex_unlock(&output_mutex);
}

void recon_output_publish_page(unsigned page)
{
    recon_output_page_token[page] = 0;
    if (!__atomic_load_n(&output_pacing, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&output_mutex);
    if (output_pacing) recon_output_page_token[page] = ++output_issued;
    pthread_mutex_unlock(&output_mutex);
}

void recon_output_sample_input(void)
{
    if (!__atomic_load_n(&output_pacing, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&output_mutex);
    if (output_pacing && output_issued != UINT64_MAX) {
        uint64_t token = output_issued + 1u;
        unsigned index = token % 16u;
        output_inputs[index].token = token;
        output_inputs[index].input_ns = output_clock_ns();
        output_inputs[index].handoff_ns = 0;
    }
    pthread_mutex_unlock(&output_mutex);
}

void nds_record_output_ready(nds_t *nds, uint64_t token)
{
    (void)nds;
    if (!token) return;
    pthread_mutex_lock(&output_mutex);
    unsigned index = token % 16u;
    if (output_pacing && token > output_completed && token <= output_issued
        && output_inputs[index].token == token && output_inputs[index].input_ns
        && !output_inputs[index].handoff_ns) {
        uint64_t now = output_clock_ns();
        if (now >= output_inputs[index].input_ns)
            output_inputs[index].handoff_ns = now;
    }
    pthread_mutex_unlock(&output_mutex);
}

static void output_budget_observe(uint64_t token, uint64_t rendered_ns, uint64_t buffer_released_ns, uint64_t now)
{
    unsigned index = token % 16u;
    uint64_t input_ns = output_inputs[index].input_ns;
    if (token <= output_budget_token || output_inputs[index].token != token
        || !input_ns || rendered_ns <= input_ns || rendered_ns > now
        || now - input_ns > 8u * OUTPUT_PERIOD_NS)
        return;
    if (now < output_budget_updated_ns || now - output_budget_updated_ns > 8u * OUTPUT_PERIOD_NS) {
        output_budget_count = 0;
        output_budget_index = 0;
        output_budget_rendered_ns = 0;
    }
    uint64_t duration = rendered_ns - input_ns;
    uint64_t handoff_ns = output_inputs[index].handoff_ns;
    uint64_t overlap = 0;
    if (token - output_budget_token == 1u && handoff_ns >= input_ns
        && handoff_ns <= rendered_ns && output_budget_rendered_ns > handoff_ns
        && output_budget_rendered_ns < rendered_ns)
        overlap = output_budget_rendered_ns - handoff_ns;
    if (handoff_ns >= input_ns && handoff_ns <= rendered_ns
        && buffer_released_ns > handoff_ns && buffer_released_ns < rendered_ns
        && buffer_released_ns - handoff_ns > overlap)
        overlap = buffer_released_ns - handoff_ns;
    duration -= overlap;
    output_budget_history[output_budget_index] = duration;
    output_budget_index = (output_budget_index + 1u) % 60u;
    if (output_budget_count < 60u) output_budget_count++;
    output_budget_ns = 0;
    for (unsigned i = 0; i < output_budget_count; i++)
        if (output_budget_history[i] > output_budget_ns) output_budget_ns = output_budget_history[i];
    output_budget_token = token;
    output_budget_updated_ns = now;
    output_budget_rendered_ns = rendered_ns;
}

static int output_wait_until(uint64_t deadline)
{
#ifdef __APPLE__
    uint64_t now = output_clock_ns();
    if (now >= deadline) return 1;
    uint64_t wait = deadline - now;
    struct timespec remaining = {(time_t)(wait / 1000000000u), (long)(wait % 1000000000u)};
    return pthread_cond_timedwait_relative_np(&output_condition, &output_mutex, &remaining);
#else
    struct timespec absolute = {(time_t)(deadline / 1000000000u), (long)(deadline % 1000000000u)};
    return pthread_cond_timedwait(&output_condition, &output_mutex, &absolute);
#endif
}

static int output_wait_with_wireless(uint64_t deadline)
{
    if (!wifi_wm_host_pending())
        return output_wait_until(deadline);

    uint64_t now = output_clock_ns();
    if (now >= deadline) return ETIMEDOUT;
    uint64_t slice = deadline - now > 200000u ? now + 200000u : deadline;
    int result = output_wait_until(slice);
    if (result != 0 && result != ETIMEDOUT) return result;
    pthread_mutex_unlock(&output_mutex);
    wifi_wm_poll_host();
    pthread_mutex_lock(&output_mutex);
    return output_clock_ns() >= deadline ? ETIMEDOUT : 0;
}

static void output_phase_observe(uint64_t token, uint64_t latch_ns)
{
    if (token == output_latch_token && latch_ns == output_observed_latch_ns) return;
    if (token <= output_latch_token) {
        output_phase_reset();
        return;
    }
    output_latch_token = token;
    output_latch_ns = latch_ns;
    output_observed_latch_ns = latch_ns;
}

static uint64_t output_phase_deadline(uint64_t target, uint64_t now)
{
    if (!output_completed_ns || !output_latch_ns || target < output_latch_token
        || target - output_latch_token > 8u)
        return 0;
    uint64_t span = target - output_latch_token;
    if (span > (UINT64_MAX - output_latch_ns) / OUTPUT_PERIOD_NS) return 0;
    uint64_t projected = output_latch_ns + span * OUTPUT_PERIOD_NS;
    uint64_t budget = output_budget_ns && now >= output_budget_updated_ns
        && now - output_budget_updated_ns <= 8u * OUTPUT_PERIOD_NS
        ? output_budget_ns + OUTPUT_PERIOD_NS / 4u : OUTPUT_PERIOD_NS;
    if (budget >= OUTPUT_PERIOD_NS) {
        uint64_t advance = budget - OUTPUT_PERIOD_NS;
        return projected > advance ? projected - advance : 0;
    }
    uint64_t delay = OUTPUT_PERIOD_NS - budget;
    return projected <= UINT64_MAX - delay ? projected + delay : 0;
}

static uint64_t output_phase_limit(uint64_t now, uint64_t wait_ns, uint64_t desired)
{
    uint64_t earliest = now + wait_ns;
    uint64_t latest = now + 2u * OUTPUT_PERIOD_NS - 1u;
    if (desired < earliest) return earliest;
    return desired < latest ? desired : latest;
}

int recon_wait_output_phase(uint64_t work_us, uint64_t nominal_wait_us, uint32_t nominal_debt_ticks, int allow_overlap)
{
    if (!__atomic_load_n(&output_pacing, __ATOMIC_ACQUIRE)
        || nominal_wait_us > (OUTPUT_PERIOD_NS + 999u) / 1000u)
        return 0;
    pthread_mutex_lock(&output_mutex);
    uint64_t target = output_issued;
    uint64_t now = output_clock_ns();
    if (!output_pacing || !output_completed_ns || !output_latch_ns
        || output_completed > target || now < output_completed_ns
        || now - output_completed_ns > 8u * OUTPUT_PERIOD_NS
        || now < output_observed_latch_ns || now - output_observed_latch_ns > 8u * OUTPUT_PERIOD_NS
        || now > UINT64_MAX - 2u * OUTPUT_PERIOD_NS) {
        pthread_mutex_unlock(&output_mutex);
        return 0;
    }
    uint64_t wait_ns = nominal_wait_us >= (OUTPUT_PERIOD_NS + 999u) / 1000u
        ? OUTPUT_PERIOD_NS : nominal_wait_us * 1000u;
    uint64_t desired = output_phase_deadline(target, now);
    if (!desired) {
        pthread_mutex_unlock(&output_mutex);
        return 0;
    }
    uint64_t limit = work_us >= (OUTPUT_PERIOD_NS + 999u) / 1000u
        ? now + wait_ns : output_phase_limit(now, wait_ns, desired);
    uint64_t pending_limit = now + 2u * OUTPUT_PERIOD_NS - 1u;
    if (allow_overlap && nominal_debt_ticks && target && output_completed == target - 1u) {
        output_deferred_token = target;
        output_deferred_deadline_ns = pending_limit;
        pthread_mutex_unlock(&output_mutex);
        return 0;
    }
    int waited = 0;
    while (output_pacing && output_completed_ns && output_latch_ns
           && output_completed <= target) {
        unsigned index = target % 16u;
        uint64_t input_ns = output_inputs[index].input_ns;
        if (output_inputs[index].token == target && input_ns && input_ns <= now
            && now - input_ns <= 8u * OUTPUT_PERIOD_NS) {
            uint64_t correction_limit = OUTPUT_PERIOD_NS / 60u;
            if (output_gpu_completed_token && output_gpu_completed_token < target
                && target - output_gpu_completed_token == 1u)
                correction_limit = OUTPUT_PERIOD_NS / 4u;
            uint64_t correction_ns = (uint64_t)nominal_debt_ticks * 1000u / 3u;
            if (correction_ns > correction_limit) correction_ns = correction_limit;
            uint64_t recovery_limit = input_ns + OUTPUT_PERIOD_NS - correction_ns;
            if (recovery_limit < now + wait_ns) recovery_limit = now + wait_ns;
            if (recovery_limit < limit) limit = recovery_limit;
        }
        uint64_t deadline = output_completed < target ? pending_limit : limit;
        if (output_clock_ns() >= deadline) break;
        waited = 1;
        if (output_wait_with_wireless(deadline) != 0) break;
    }
    pthread_mutex_unlock(&output_mutex);
    return waited;
}

void nds_complete_output_frame(nds_t *nds, uint64_t token, int consumed,
                               uint64_t observed_token, uint64_t phase_ns, uint64_t rendered_token,
                               uint64_t rendered_ns, uint64_t interval_ns, uint64_t buffer_released_ns,
                               uint64_t completed_gpu_token)
{
    (void)nds;
    if (!token) return;
    pthread_mutex_lock(&output_mutex);
    if (token > output_completed && token <= output_issued) {
        output_completed = token;
        output_gpu_completed_token = consumed && completed_gpu_token && completed_gpu_token < token
            && token - completed_gpu_token == 1u ? completed_gpu_token : 0;
        output_completed_ns = consumed ? output_clock_ns() : 0;
        unsigned index = token % 16u;
        if (consumed && output_inputs[index].token == token && !output_inputs[index].handoff_ns)
            output_inputs[index].handoff_ns = output_completed_ns;
        if (consumed && observed_token && observed_token <= token && token - observed_token <= 8u
            && interval_ns && interval_ns <= UINT64_MAX / 8u && phase_ns
            && phase_ns <= output_completed_ns && output_completed_ns - phase_ns <= 8u * interval_ns) {
            output_phase_observe(observed_token, phase_ns);
            if (output_latch_token == observed_token && rendered_token
                && rendered_token <= token && token - rendered_token <= 8u)
                output_budget_observe(rendered_token, rendered_ns, buffer_released_ns, output_completed_ns);
        } else output_phase_reset();
        pthread_cond_broadcast(&output_condition);
    }
    pthread_mutex_unlock(&output_mutex);
}

void nds_complete_output_gpu(nds_t *nds, uint64_t token, uint64_t completed_gpu_token)
{
    (void)nds;
    if (!token || !completed_gpu_token || completed_gpu_token >= token
        || token - completed_gpu_token != 1u) return;
    pthread_mutex_lock(&output_mutex);
    if (output_pacing && output_completed == token && output_completed_ns && output_latch_ns
        && output_gpu_completed_token != completed_gpu_token) {
        output_gpu_completed_token = completed_gpu_token;
        pthread_cond_broadcast(&output_condition);
    }
    pthread_mutex_unlock(&output_mutex);
}

uint64_t nds_fx_frame_token(nds_t *nds)
{
    (void)nds;
    return recon_output_render_token;
}

#undef OUTPUT_PERIOD_NS

static void output_wait_previous(void)
{
    if (!__atomic_load_n(&output_pacing, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&output_mutex);
    while (output_pacing && output_deferred_token && output_completed < output_deferred_token
           && output_clock_ns() < output_deferred_deadline_ns) {
        if (output_wait_with_wireless(output_deferred_deadline_ns) != 0) break;
    }
    output_deferred_token = 0;
    output_deferred_deadline_ns = 0;
    pthread_mutex_unlock(&output_mutex);
}

int platform_doublebuffer_flip_signal(void) {
    if (!core_lock) {
        core_lock   = nds_platform_default()->threads.mutex_lock;
        core_unlock = nds_platform_default()->threads.mutex_unlock;
        core_signal = nds_platform_default()->threads.cond_signal;
    }
    video_out_gl_t *g = VIDEO_OUT_GL;
    void *mutex = g->mutex;
    void *cond  = g->cond;

    output_wait_previous();
    core_lock(mutex);
    g->page_parity = (~g->page_parity) & 1;
    recon_page_scale_tag[(~g->page_parity) & 1u] = recon_scale_out;
    recon_output_publish_page((~g->page_parity) & 1u);
    if (recon_present_flip_hook) {
        uint32_t idx = g->page_parity;
        recon_present_flip_hook(g->page[idx], idx);
    }

    if (recon_present_mode & RECON_PRESENT_PREDICATE) recon_present_flips++;
    core_signal(cond);
    return core_unlock(mutex);
}

static int screen_redraw_pending;

void nds_request_screen_redraw(nds_t *nds)
{
    (void)nds;
    video_out_gl_t *g = VIDEO_OUT_GL;
    const nds_platform_t *platform = nds_platform_default();
    platform->threads.mutex_lock(g->mutex);
    if (recon_present_mode & RECON_PRESENT_PREDICATE) {
        screen_redraw_pending = 1;
        platform->threads.cond_signal(g->cond);
    }
    platform->threads.mutex_unlock(g->mutex);
}

int nds_wait_screen_update(nds_t *nds)
{
    (void)nds;
    video_out_gl_t *g = VIDEO_OUT_GL;
    const nds_platform_t *platform = nds_platform_default();
    platform->threads.mutex_lock(g->mutex);
    if (!(recon_present_mode & RECON_PRESENT_PREDICATE)) {
        screen_redraw_pending = 0;
        platform->threads.mutex_unlock(g->mutex);
        return -1;
    }
    while (recon_present_seen == recon_present_flips && !screen_redraw_pending)
        platform->threads.cond_wait(g->cond, g->mutex);
    int frame_ready = recon_present_seen != recon_present_flips;
    if (frame_ready) recon_present_seen = recon_present_flips;
    screen_redraw_pending = 0;
    platform->threads.mutex_unlock(g->mutex);
    return frame_ready;
}

int platform_doublebuffer_wait(void) {

    void *m = VIDEO_OUT_GL->mutex;
    void *cond = VIDEO_OUT_GL->cond;

    nds_platform_default()->threads.mutex_lock(m);

    if (recon_present_mode & RECON_PRESENT_PREDICATE) {
        while (recon_present_seen == recon_present_flips) {
            nds_platform_default()->threads.cond_wait(cond, m);
        }
        recon_present_seen = recon_present_flips;
        return nds_platform_default()->threads.mutex_unlock(m);
    }
    nds_platform_default()->threads.cond_wait(cond, m);
    return nds_platform_default()->threads.mutex_unlock(m);
}
static int (*core_lock_2)(void *);
static int (*core_unlock_2)(void *);
static int (*core_signal_2)(void *);

int platform_doublebuffer_signal(void) {
    if (!core_lock_2) {
        core_lock_2   = nds_platform_default()->threads.mutex_lock;
        core_unlock_2 = nds_platform_default()->threads.mutex_unlock;
        core_signal_2 = nds_platform_default()->threads.cond_signal;
    }
    video_out_gl_t *g = VIDEO_OUT_GL;
    core_lock_2(g->mutex);

    if (recon_present_mode & RECON_PRESENT_PREDICATE) recon_present_flips++;
    core_signal_2(g->cond);
    return core_unlock_2(g->mutex);
}

__attribute__((weak)) void recon_crash_stack_thread(void);

struct thread_start { void *(*routine)(void *); void *arg; };

static void *recon_thread_trampoline(void *p)
{
    struct thread_start a = *(struct thread_start *)p;
    free(p);
    if (recon_crash_stack_thread) recon_crash_stack_thread();
    return a.routine(a.arg);
}
int recon_pthread_create_1(void *thread, const void *attr, void *(*routine)(void *), void *arg) __asm__("recon_pthread_create");

int recon_pthread_create_1(void *thread, const void *attr, void *(*routine)(void *), void *arg)
{
    struct thread_start *a = (struct thread_start *)malloc(sizeof *a);
    if (!a) return pthread_create((pthread_t *)thread, (const pthread_attr_t *)attr, routine, arg);
    a->routine = routine; a->arg = arg;
    {
        int r = pthread_create((pthread_t *)thread, (const pthread_attr_t *)attr, recon_thread_trampoline, a);
        if (r != 0) { free(a); return pthread_create((pthread_t *)thread, (const pthread_attr_t *)attr, routine, arg); }
        return r;
    }
}

time_t recon_clock_time(time_t *t)
{
    long v;
    if (nds_clock_override(&v)) {
        if (t) *t = (time_t)v;
        return (time_t)v;
    }
    return time(t);
}

struct tm *recon_clock_localtime(const time_t *t)
{
    long v;
    if (nds_clock_override(&v)) {
        static struct tm fixed;
        time_t tv = (time_t)v;
        gmtime_r(&tv, &fixed);
        return &fixed;
    }
    return localtime(t);
}

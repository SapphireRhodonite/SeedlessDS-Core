#include <string.h>
#include "seedlessds/platform.h"
#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "present_hook.h"
#include "frontend/video_out_gl.h"
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

static int (*core_lock)(void *);
static int (*core_unlock)(void *);
static int (*core_signal)(void *);

void (*recon_present_flip_hook)(const void *page, unsigned idx);

int platform_doublebuffer_flip_signal(void) {
    if (!core_lock) {
        core_lock   = nds_platform_default()->threads.mutex_lock;
        core_unlock = nds_platform_default()->threads.mutex_unlock;
        core_signal = nds_platform_default()->threads.cond_signal;
    }
    video_out_gl_t *g = VIDEO_OUT_GL;
    void *mutex = g->mutex;
    void *cond  = g->cond;

    core_lock(mutex);
    g->page_parity = (~g->page_parity) & 1;
    recon_page_scale_tag[(~g->page_parity) & 1u] = recon_scale_out;
    if (recon_present_flip_hook) {
        uint32_t idx = g->page_parity;
        recon_present_flip_hook(g->page[idx], idx);
    }

    if (recon_present_mode & RECON_PRESENT_PREDICATE) recon_present_flips++;
    core_signal(cond);
    return core_unlock(mutex);
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

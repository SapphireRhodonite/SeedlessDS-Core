#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include "hires_runtime.h"
#include "seedlessds/control.h"
#include "core_internals.h"

unsigned long nds_output_reads;
unsigned long nds_output_frames;

static pthread_mutex_t control_mutex = PTHREAD_MUTEX_INITIALIZER;
static nds_control_status_t state;
static int enabled;
static int pause_requested;
static int no_frameskip;
static long fixed_clock = -1;
static unsigned compose_threads = ~0u;
static int capture_fd = -1;
static int audio_fd = -1;
static uint64_t capture_first, capture_last, capture_seen = UINT64_MAX;
static uint64_t audio_first, audio_last, audio_index;
static int capture_raw;
static unsigned long previous_polygons;
static unsigned state_slot;
static int state_load;
static int state_pending;

static void close_output(int *fd)
{
    if (*fd >= 0) {
        if (close(*fd) != 0 && state.error == 0) state.error = errno;
        *fd = -1;
    }
}

static int write_output(int fd, const void *data, size_t bytes, uint64_t *total)
{
    const unsigned char *p = data;
    while (bytes) {
        ssize_t n = write(fd, p, bytes);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            state.error = n < 0 ? errno : EIO;
            return 0;
        }
        p += n;
        bytes -= (size_t)n;
        *total += (uint64_t)n;
    }
    return 1;
}

void nds_control_begin(int64_t clock_seconds, int disable_frameskip, unsigned threads)
{
    pthread_mutex_lock(&control_mutex);
    close_output(&capture_fd);
    close_output(&audio_fd);
    memset(&state, 0, sizeof state);
    state.active = 1;
    state.epoch = 1;
    state_pending = 0;
    state.first_3d = UINT64_MAX;
    state.first_geometry = UINT64_MAX;
    capture_seen = UINT64_MAX;
    audio_index = 0;
    previous_polygons = recon_poly_band;
    __atomic_store_n(&fixed_clock, (long)clock_seconds, __ATOMIC_RELEASE);
    __atomic_store_n(&compose_threads, threads, __ATOMIC_RELEASE);
    __atomic_store_n(&no_frameskip, disable_frameskip != 0, __ATOMIC_RELEASE);
    __atomic_store_n(&pause_requested, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&enabled, 1, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&control_mutex);
}

void nds_control_end(void)
{
    __atomic_store_n(&enabled, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&pause_requested, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&fixed_clock, -1, __ATOMIC_RELEASE);
    __atomic_store_n(&no_frameskip, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&compose_threads, ~0u, __ATOMIC_RELEASE);
    pthread_mutex_lock(&control_mutex);
    close_output(&capture_fd);
    close_output(&audio_fd);
    state.active = 0;
    state.paused = 0;
    state.target = 0;
    pthread_mutex_unlock(&control_mutex);
}

void nds_control_status(nds_control_status_t *out)
{
    pthread_mutex_lock(&control_mutex);
    *out = state;
    out->output_frames = __atomic_load_n(&nds_output_frames, __ATOMIC_RELAXED);
    out->output_reads = __atomic_load_n(&nds_output_reads, __ATOMIC_RELAXED);
    out->state_busy = state_pending || state.state_request != state.state_completed || state_write_pending();
    out->audio_position = audio_index;
    pthread_mutex_unlock(&control_mutex);
}

void nds_control_frame_boundary(void)
{
    if (!__atomic_load_n(&enabled, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&control_mutex);
    ++state.frames;
    if (state.target && state.frames >= state.target)
        __atomic_store_n(&pause_requested, 1, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&control_mutex);
}

int nds_control_pause_requested(void)
{
    return __atomic_load_n(&pause_requested, __ATOMIC_ACQUIRE);
}

void nds_control_pause_acknowledge(int paused)
{
    if (!__atomic_load_n(&enabled, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&control_mutex);
    state.paused = paused != 0;
    pthread_mutex_unlock(&control_mutex);
}

void nds_control_pause(int paused)
{
    pthread_mutex_lock(&control_mutex);
    state.target = 0;
    if (!paused) state.paused = 0;
    __atomic_store_n(&pause_requested, paused != 0, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&control_mutex);
}

int nds_control_run_to(uint64_t frame)
{
    pthread_mutex_lock(&control_mutex);
    int ok = state.active && frame > state.frames;
    if (ok) {
        state.target = frame;
        state.paused = 0;
        __atomic_store_n(&pause_requested, 0, __ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&control_mutex);
    return ok;
}

int nds_control_step(uint64_t frames)
{
    pthread_mutex_lock(&control_mutex);
    int ok = state.active && state.paused && __atomic_load_n(&pause_requested, __ATOMIC_ACQUIRE)
        && frames > 0 && frames <= 10000
        && state.frames <= UINT64_MAX - frames;
    if (ok) {
        state.target = state.frames + frames;
        state.paused = 0;
        __atomic_store_n(&pause_requested, 0, __ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&control_mutex);
    return ok;
}

int nds_control_capture(int fd, uint64_t first, uint64_t last, int raw)
{
    pthread_mutex_lock(&control_mutex);
    int ok = state.active && capture_fd < 0 && first <= last && last <= UINT32_MAX
        && first >= __atomic_load_n(&nds_output_frames, __ATOMIC_RELAXED);
    if (ok) {
        capture_fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
        ok = capture_fd >= 0;
        if (ok) {
            capture_first = first;
            capture_last = last;
            capture_raw = raw != 0;
            capture_seen = UINT64_MAX;
            state.capture_records = state.capture_bytes = 0;
            state.capture_complete = 0;
            state.error = 0;
            __atomic_store_n(&no_frameskip, 1, __ATOMIC_RELEASE);
        }
    }
    pthread_mutex_unlock(&control_mutex);
    return ok;
}

int nds_control_audio(int fd, uint64_t first, uint64_t last)
{
    pthread_mutex_lock(&control_mutex);
    int ok = state.active && audio_fd < 0 && first <= last && first >= audio_index;
    if (ok) {
        audio_fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
        ok = audio_fd >= 0;
        if (ok) {
            audio_first = first;
            audio_last = last;
            state.audio_blocks = state.audio_bytes = 0;
            state.audio_complete = 0;
            state.error = 0;
        }
    }
    pthread_mutex_unlock(&control_mutex);
    return ok;
}

void nds_control_cancel_capture(void)
{
    pthread_mutex_lock(&control_mutex);
    close_output(&capture_fd);
    close_output(&audio_fd);
    pthread_mutex_unlock(&control_mutex);
}

uint64_t nds_control_request_state(unsigned slot, int load)
{
    pthread_mutex_lock(&control_mutex);
    uint64_t ticket = 0;
    if (state.active && state.paused && __atomic_load_n(&pause_requested, __ATOMIC_ACQUIRE)
        && slot <= 9 && !state_pending && state.state_request == state.state_completed
        && !state_write_pending()) {
        state_slot = slot;
        state_load = load != 0;
        state_pending = 1;
        ticket = ++state.state_request;
    }
    pthread_mutex_unlock(&control_mutex);
    return ticket;
}

void nds_control_poll_state(void *machine)
{
    if (!__atomic_load_n(&enabled, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&control_mutex);
    int pending = state_pending;
    unsigned slot = state_slot;
    int load = state_load;
    uint64_t ticket = state.state_request;
    state_pending = 0;
    pthread_mutex_unlock(&control_mutex);
    if (!pending) return;
    int result = load ? state_load_slot(machine, slot, 0, 0, 0) : state_save_slot_with_screens(slot);
    pthread_mutex_lock(&control_mutex);
    state.state_result = result;
    state.state_completed = ticket;
    if (load && result == 0) ++state.epoch;
    pthread_mutex_unlock(&control_mutex);
}

__attribute__((weak)) unsigned long nds_module_base(void)
{
    Dl_info info;
    return dladdr((void *)&nds_module_base, &info) ? (unsigned long)(uintptr_t)info.dli_fbase : 0;
}

__attribute__((weak)) int nds_clock_override(long *value)
{
    long clock = __atomic_load_n(&fixed_clock, __ATOMIC_ACQUIRE);
    if (clock < 0) return 0;
    if (value) *value = clock;
    return 1;
}

__attribute__((weak)) unsigned long nds_output_frame_index(void)
{
    return __atomic_load_n(&nds_output_frames, __ATOMIC_RELAXED);
}

__attribute__((weak)) unsigned long nds_output_read_count(void)
{
    return __atomic_load_n(&nds_output_reads, __ATOMIC_RELAXED);
}

__attribute__((weak)) void nds_output_read_begin(void)
{
    if (__atomic_load_n(&enabled, __ATOMIC_ACQUIRE))
        __atomic_add_fetch(&nds_output_reads, 1, __ATOMIC_RELAXED);
}

__attribute__((weak)) int nds_capture_requires_frames(void)
{
    return __atomic_load_n(&no_frameskip, __ATOMIC_ACQUIRE);
}

__attribute__((weak)) void nds_capture_screens(const void *page, const volatile uint32_t *mode)
{
    if (!__atomic_load_n(&enabled, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&control_mutex);
    uint64_t frame = __atomic_load_n(&nds_output_frames, __ATOMIC_RELAXED);
    if (capture_fd < 0 || frame < capture_first || frame == capture_seen) {
        pthread_mutex_unlock(&control_mutex);
        return;
    }
    if (frame != capture_first + state.capture_records / 2) {
        state.error = ERANGE;
        close_output(&capture_fd);
        pthread_mutex_unlock(&control_mutex);
        return;
    }
    capture_seen = frame;
    unsigned scale = recon_native ? recon_scale_output() : 2u;
    if (scale < 1u) scale = 2u;
    uint64_t stride = recon_native ? (uint64_t)RECON_SCREEN_BYTES : UINT64_C(0xC0000);
    unsigned char row[256 * 8 * 3];
    for (unsigned screen = 0; screen < 2 && !state.error; ++screen) {
        unsigned n = mode[screen] ? scale : 1u;
        if (n > 8u) { state.error = EOVERFLOW; break; }
        uint32_t header[3] = { (uint32_t)frame, screen, mode[screen] ? recon_scale_out : 1u };
        if (!write_output(capture_fd, header, sizeof header, &state.capture_bytes)) break;
        const unsigned char *pixels = (const unsigned char *)page + screen * stride;
        unsigned width = capture_raw ? 256u * n : 256u;
        unsigned height = capture_raw ? 192u * n : 192u;
        for (unsigned y = 0; y < height && !state.error; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                if (capture_raw) {
                    size_t offset = ((size_t)y * width + x) * 4;
                    row[x * 3] = pixels[offset + 2];
                    row[x * 3 + 1] = pixels[offset + 1];
                    row[x * 3 + 2] = pixels[offset];
                } else {
                    unsigned r = 0, g = 0, b = 0;
                    for (unsigned dy = 0; dy < n; ++dy)
                        for (unsigned dx = 0; dx < n; ++dx) {
                            size_t offset = ((size_t)(y * n + dy) * (256u * n) + x * n + dx) * 4;
                            r += pixels[offset + 2]; g += pixels[offset + 1]; b += pixels[offset];
                        }
                    row[x * 3] = (unsigned char)(r / (n * n));
                    row[x * 3 + 1] = (unsigned char)(g / (n * n));
                    row[x * 3 + 2] = (unsigned char)(b / (n * n));
                }
            }
            if (!write_output(capture_fd, row, width * 3u, &state.capture_bytes)) break;
        }
        if (!state.error) ++state.capture_records;
    }
    if (state.error || frame == capture_last) {
        close_output(&capture_fd);
        state.capture_complete = !state.error
            && state.capture_records == (capture_last - capture_first + 1) * 2;
    }
    pthread_mutex_unlock(&control_mutex);
}

__attribute__((weak)) void nds_capture_audio(const void *pcm, unsigned samples)
{
    if (!__atomic_load_n(&enabled, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&control_mutex);
    uint64_t frame = audio_index++;
    if (audio_fd >= 0 && frame >= audio_first && frame <= audio_last) {
        if (write_output(audio_fd, pcm, (size_t)samples * 2u, &state.audio_bytes)) ++state.audio_blocks;
        if (state.error || frame == audio_last) {
            close_output(&audio_fd);
            state.audio_complete = !state.error && state.audio_blocks == audio_last - audio_first + 1;
        }
    }
    pthread_mutex_unlock(&control_mutex);
}

__attribute__((weak)) unsigned nds_configured_2d_threads(unsigned fallback)
{
    unsigned configured = __atomic_load_n(&compose_threads, __ATOMIC_ACQUIRE);
    return configured == ~0u ? fallback : configured;
}

__attribute__((weak)) void nds_trace_3d_dispatch(void)
{
    if (!__atomic_load_n(&enabled, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&control_mutex);
    uint64_t frame = __atomic_load_n(&nds_output_frames, __ATOMIC_RELAXED);
    unsigned long polygons = recon_poly_band - previous_polygons;
    previous_polygons = recon_poly_band;
    if (state.first_3d == UINT64_MAX) state.first_3d = frame;
    if (state.first_geometry == UINT64_MAX && polygons >= RECON_POLY_THRESHOLD) state.first_geometry = frame;
    if (polygons > state.max_polygons) state.max_polygons = polygons;
    ++state.dispatches;
    pthread_mutex_unlock(&control_mutex);
}

__attribute__((weak)) void nds_trace_2d_start(unsigned threads, int boss_only, int shut_down)
{ (void)threads; (void)boss_only; (void)shut_down; }
__attribute__((weak)) void nds_trace_2d_line(void) { }
__attribute__((weak)) void nds_trace_2d_serial(void) { }
__attribute__((weak)) void nds_trace_2d_fixed(void) { }
__attribute__((weak)) void nds_trace_2d_race(void) { }
__attribute__((weak)) void nds_trace_2d_report(unsigned generation) { (void)generation; }
__attribute__((weak)) void nds_trace_drain_wait(int engine, unsigned in_flight_0, unsigned in_flight_1,
    unsigned head, unsigned tail, unsigned sleeping, unsigned active, unsigned threads, unsigned long turns)
{
    (void)engine; (void)in_flight_0; (void)in_flight_1; (void)head; (void)tail;
    (void)sleeping; (void)active; (void)threads; (void)turns;
}

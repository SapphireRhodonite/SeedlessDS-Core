#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stdarg.h>
#include <stddef.h>
#include "core/nds_state.h"
#include <stdio.h>
#include "core_internals.h"

static const char *const benchmark_phase_names[7] = { "Warmup", "Complete", "Video 2D", "Video 3D", "Video Geometry", "Screen Update", "Audio" };

#define BM(p) ((nds_benchmark_t *)(p))

void benchmark_step(unsigned char *o) {
    void (*clock)(uint64_t *)   = time_now_microseconds;
    void (*sleep)(unsigned)    = (void (*)(unsigned))sym_libc_usleep;
    void (*dump)(unsigned char *) = mirror_fatal_reset_longjmp;
    int (*rearm)(unsigned char *, uint32_t, void *, void *, uint32_t) = state_load_slot;
    int (*line)(char *, long, long, ...) = benchmark_format_run_time;

    uint32_t mask = BM(o)->pass_mask;
    if (mask == 0)
        return;

    uint32_t frames = BM(o)->frame_counter;
    if (frames == 0) {

        uint32_t i = BM(o)->pass_index;
        uint32_t bit = 1u << (i & 31);
        if ((bit & mask) == 0) {
            do {
                i++;
                bit = 1u << (i & 31);
            } while ((bit & mask) == 0);
            BM(o)->pass_index = i;
        }

        void *machine = BM(o)->machine;
        BM(o)->pass_flags = bit;
        ((nds_t *)machine)->config.fast_forward = 1;
        if (BM(o)->full_run == 0)
            BM(o)->pass_flags = bit | 0x20;

        rearm(machine, BM(o)->state_slot, 0, 0, 0);
        sleep(2000000);
        {

            void (*clear)(void *) = (void (*)(void *))sym_libc_fflush;
            clear(stdout);
        }
        clock(&BM(o)->start_time);

        frames = BM(o)->frame_counter;
    }
    if (frames != BM(o)->frames_per_pass) {
        BM(o)->frame_counter = frames + 1;
        return;
    }

    uint64_t now;
    char buf[128];
    clock(&now);
    uint32_t idx = BM(o)->pass_index;
    BM(o)->elapsed[idx] = now - BM(o)->start_time;

    mask = BM(o)->pass_mask;
    idx++;
    BM(o)->pass_index = idx;
    BM(o)->frame_counter = 0xFFFFFFFFu;

    if ((mask >> (idx & 31)) != 0)
        goto leave;
    if (mask < 4)
        goto emit;

    {
        const char *const *table = benchmark_phase_names;
        uint32_t k = 3, bit;

        if ((4u & mask) != 0)
            goto print;
    find:
        bit = 1u << (k & 31);
        {
            int past = bit > mask;
            k++;
            if (past) goto emit;
        }
    test:
        if ((bit & mask) == 0)
            goto find;
    print:
        line(buf, 0, 0, table[k - 1]);
        mask = BM(o)->pass_mask;
        bit = 1u << (k & 31);
        {
            int fits = bit <= mask;
            k++;
            if (fits) goto test;
        }
    }

emit:
    dump((unsigned char *)BM(o)->machine);
leave:
    BM(o)->frame_counter = BM(o)->frame_counter + 1;
}

void benchmark_init(unsigned char *obj, nds_t *machine, uint32_t b, uint32_t c,
                        uint32_t unused, uint32_t f) {
    (void)unused;
    BM(obj)->machine = machine;
    BM(obj)->frames_per_pass = b;
    BM(obj)->pass_mask = c;
    BM(obj)->full_run = f;

    BM(obj)->pass_index = 0;
    BM(obj)->frame_counter = 0;
    BM(obj)->pass_flags = 0;
    BM(obj)->enabled = 1;
}

int benchmark_format_run_time(char *dest, long a, long b, ...) {
    (void)a; (void)b;

    const char *format = "%s run time:";

    va_list ap;
    va_start(ap, b);
    int r = fortify_vsprintf(dest, 0x80, format, ap);
    va_end(ap);
    return r;
}

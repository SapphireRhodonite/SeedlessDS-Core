#include "hires_runtime.h"
#include <stdint.h>
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include "frontend/video_out_gl.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"



void gpu2d_worker_hook_noop(void) {
}
static int (*core_lock)(void *);
static int (*core_unlock)(void *);
static int (*core_signal)(void *);

int gpu2d_worker_signal_request_bit(uint32_t bit) {
    if (!core_lock) {
        core_lock   = nds_platform_default()->threads.mutex_lock;
        core_unlock = nds_platform_default()->threads.mutex_unlock;
        core_signal = nds_platform_default()->threads.cond_signal;
    }
    video_out_gl_t *g = VIDEO_OUT_GL;
    void *mutex = g->mutex;

    core_lock(mutex);
    uint32_t mask = g->upload_request;
    mask |= (uint32_t)1u << bit;
    g->upload_request = (unsigned char)mask;
    core_signal(g->cond);
    return core_unlock(mutex);
}
extern void video_out_gl_renderer_release(void *param_1, int32_t param_2);
typedef int  (*fn_mutex_op)(void *);
typedef int  (*fn_cond_op)(void *);
typedef void (*fn_free)(void *);

void gpu2d_worker_shutdown(void) {

    void *mutex = VIDEO_OUT_GL->mutex;
    void *cond  = VIDEO_OUT_GL->cond;

    fn_mutex_op p_lock     = nds_platform_default()->threads.mutex_lock;
    fn_cond_op  p_signal   = nds_platform_default()->threads.cond_signal;
    fn_mutex_op p_unlock   = nds_platform_default()->threads.mutex_unlock;
    fn_mutex_op p_mdestroy = nds_platform_default()->threads.mutex_destroy;
    fn_cond_op  p_cdestroy = nds_platform_default()->threads.cond_destroy;
    fn_free     p_free     = (fn_free)sym_libc_free;

    p_lock(mutex);
    p_signal(cond);
    p_unlock(mutex);
    p_mdestroy(mutex);
    p_cdestroy(cond);

    void *save = VIDEO_OUT_GL->page_memory;
    if (save != NULL) {
        p_free(save);
        VIDEO_OUT_GL->page_memory = NULL;
    }

    video_out_gl_renderer_release(&VIDEO_OUT_GL->renderer[0], 0);
    video_out_gl_renderer_release(&VIDEO_OUT_GL->renderer[1], 0);
}
static int (*core_lock_3)(void *);
static int (*core_unlock_3)(void *);
static int (*core_signal_3)(void *);
static int (*core_wait)(void *, void *);

void *gpu2d_worker_thread_loop(unsigned char *base) {
    if (!core_lock_3) {
        core_lock_3   = nds_platform_default()->threads.mutex_lock;
        core_unlock_3 = nds_platform_default()->threads.mutex_unlock;
        core_signal_3 = nds_platform_default()->threads.cond_signal;
        core_wait   = nds_platform_default()->threads.cond_wait;
    }
    gpu_output_t *output = GPU_OUTPUT_OF(base);
    unsigned char *flag = &output->worker_start;
    void *mutex  = output->worker_mutex;
    void *cond   = output->worker_cond;
    void *mutex2 = output->worker_mutex_done;
    void *cond2  = output->worker_cond_done;
    gpu2d_engine_t *area = &((gpu_t *)base)->engine[1];

    core_lock_3(mutex);
    for (;;) {
        while (flag[0] == 0)
            core_wait(cond, mutex);

        flag[0] = 0;
        core_unlock_3(mutex);

        { extern int recon_compose_b_shared(gpu2d_engine_t *, uint32_t, uint32_t, void *);
          if (!recon_compose_b_shared(area, 0, 191, 0))
              gpu2d_line_process_range(area, 0, 191, 0); }

        core_lock_3(mutex2);
        flag[1] = 1;
        core_signal_3(cond2);
        core_unlock_3(mutex2);

        core_lock_3(mutex);
    }
}
static int (*core_pthread_create)(void *, void *, void *, void *);
static int (*core_mutex_init)(void *, void *);
static int (*core_cond_init)(void *, void *);

void gpu2d_worker_init(unsigned char *obj, unsigned char *ctx) {
    if (!core_pthread_create) {
        core_pthread_create = (int (*)(void *, void *, void *, void *))
                              platform_thread_create;
        core_mutex_init = (int (*)(void *, void *))platform_mutex_init;
        core_cond_init  = (int (*)(void *, void *))platform_cond_init;
    }

    gpu_t *gpu = (gpu_t *)obj;
    gpu->vram.bus = (bus_t *)ctx;
    unsigned char *root = (unsigned char *)((bus_t *)ctx)->machine;

    unsigned char *a = ((bus_t *)ctx)->palette;
    unsigned char *b = ((bus_t *)ctx)->oam;
    gpu->vram.engine_palette[0] = a;
    gpu->vram.engine_oam[0] = b;
    gpu->vram.engine_palette[1] = a + 0x400;
    gpu->vram.engine_oam[1] = b + 0x400;
    gpu->vram.config = &((nds_t *)root)->config;

    vram_map_t *vram = (vram_map_t *)obj;
    bus_t *bus = (bus_t *)ctx;
    for (int i = 0; i < 9; i++)
        vram->bank_data[i] = bus->vram_bank[i];

    for (int i = 0; i < 7; i++)
        vram->bank_control_reg[i] = &bus->io_mirror[0].vramcnt[i];
    vram->bank_control_reg[7] = &bus->io_mirror[0].vramcnt_hi[0];
    vram->bank_control_reg[8] = &bus->io_mirror[0].vramcnt_hi[1];
    gpu_output_t *output = GPU_OUTPUT_OF(obj);
    output->capture.bank_bits = 0;
    memset(output->capture.unmapped_2, 0, 4);

    core_pthread_create(&output->worker_thread, 0, gpu2d_worker_thread_loop, obj);

    core_mutex_init(output->worker_mutex, 0);
    core_mutex_init(output->worker_mutex_done, 0);
    core_cond_init (output->worker_cond, 0);
    core_cond_init (output->worker_cond_done, 0);

    output->worker_start = 0;
    output->worker_done = 0;

    gpu2d_engine_init_slot(&gpu->engine[0], 0, gpu);
    gpu2d_engine_init_slot(&gpu->engine[1], 1, gpu);

    gpu3d_texture_cache_t *dst = &gpu->texture_cache;
    gpu3d_context_subblock_init(
        GPU3D_OF(obj), gpu->vram.bus->machine, dst);
    gpu3d_raster_texture_cache_init(dst, obj);

    gpu3d_raster_pipeline_init((gpu_t *)obj);

    output->capture_shadow[0] = 0;
    output->capture_shadow[1] = 0;
    output->capture_shadow[2] = 0;
    output->capture_shadow[3] = 0;
}






static void reset_banks_by_mask(uint8_t *engine_idx, uint16_t mask,
                                   uint32_t index_initial)
{
    uint32_t idx = index_initial;

    while (mask != 0) {
        if ((mask & 1u) != 0) {
            uint32_t attr;
            vram_map_t *vram = (vram_map_t *)engine_idx;
            vram->bank[idx].control = UINT32_MAX;
            attr = *vram->bank_control_reg[idx];
            vram_vramcnt_bank_remap(vram, vram->bank_data[idx], idx, attr, 0);
        }

        mask = (uint16_t)(mask >> 1);
        ++idx;
    }
}

void gpu2d_worker_reset_engine_entries(uint8_t *engine_idx, uint8_t *state, uint32_t value)
{

    gpu2d_engine_load_state_registers(&((gpu_t *)engine_idx)->engine[0], state, value);
    gpu2d_engine_load_state_registers(&((gpu_t *)engine_idx)->engine[1], state, value);
    state_read_gpu3d_block((unsigned char *)GPU3D_OF((gpu_t *)engine_idx), state, value);

    uint64_t cursor = rd64(state + 0x20u);
    wr64(state + 0x20u, cursor + 2u);

    vram_map_t *vram = (vram_map_t *)engine_idx;
    for (uint32_t idx = 0; idx != 9u; ++idx) {
        if (vram->bank[idx].control != 0) {
            vram->changed_banks = 0;
            vram_vramcnt_bank_remap(vram, vram->bank_data[idx], idx, 0, 1);
            reset_banks_by_mask(engine_idx, vram->changed_banks, 0);
        }
    }

    for (uint32_t idx = 0; idx != 9u; ++idx) {
        uint32_t attr = *vram->bank_control_reg[idx];

        if (vram->bank[idx].control != attr) {
            vram->changed_banks = 0;
            vram_vramcnt_bank_remap(vram, vram->bank_data[idx], idx, attr, 1);
            reset_banks_by_mask(engine_idx, vram->changed_banks, 0);
        }
    }
    gpu_output_t *output = GPU_OUTPUT_OF(engine_idx);
    memset(output->bank_texture_bits, 0, 4);
    output->capture.bank_bits = 0;
    memset(output->capture.unmapped_2, 0, 4);
    ((gpu_t *)engine_idx)->texture_cache.dirty_pending[0] = 0;
    ((gpu_t *)engine_idx)->texture_cache.dirty_pending[1] = 0;
    gpu3d_raster_texture_cache_flush_entries(&((gpu_t *)engine_idx)->texture_cache);
}
extern void state_write_gpu3d_block(unsigned char *param_1, unsigned char *param_2, uint32_t param_3);

void gpu2d_worker_save_state_chain(const unsigned char *param_1, unsigned char *param_2, uint32_t param_3)
{

    gpu2d_engine_save_state_registers(&((gpu_t *)param_1)->engine[0], param_2, param_3);

    gpu2d_engine_save_state_registers(&((gpu_t *)param_1)->engine[1], param_2, param_3);

    state_write_gpu3d_block((unsigned char *)GPU3D_OF((gpu_t *)param_1), param_2, param_3);

    unsigned long cursor;
    memcpy(&cursor, param_2 + 0x20, sizeof(cursor));
    cursor += 2;
    memcpy(param_2 + 0x20, &cursor, sizeof(cursor));
}



static uint8_t nibble_pairs_to_bits(uint32_t first, uint32_t second)
{
    uint32_t low = (first | (first >> 4)) & UINT32_C(0x0f0f0f0f);
    uint32_t height = (second | (second << 4)) & UINT32_C(0xf0f0f0f0);
    uint32_t value = low | height;

    value |= value >> 2;
    value |= value >> 1;
    value &= UINT32_C(0x11111111);
    value |= value >> 7;
    value |= value >> 14;
    return (uint8_t)value;
}

void gpu2d_worker_pack_1bpp_pairs(const uint8_t *source, uint8_t *dest,
                         uint32_t param_3, uint32_t param_4)
{

    uint32_t start = param_3 >> 3;
    uint32_t end = param_4 >> 3;
    uint32_t limit;
    uint64_t count;
    uint64_t primary = 0;

    if (start > end)
        return;

    limit = end + 1u;
    count = (uint64_t)limit - start;

    if (count >= 4) {
        uintptr_t start_dest = (uintptr_t)dest + start;
        uintptr_t final_source = (uintptr_t)source
            + ((uint64_t)limit * 8u - (param_3 & UINT32_C(0xfffffff8)));
        uintptr_t final_dest = (uintptr_t)dest + limit;

        if (start_dest >= final_source ||
            final_dest <= (uintptr_t)source) {
            primary = count & ~UINT64_C(3);
        }
    }

    if (primary != 0) {
        const uint8_t *origin = source;
        uint8_t *output = dest + start;

        for (uint64_t i = 0; i != primary; i += 4) {
            uint32_t a0 = rd32(origin + 0);
            uint32_t b0 = rd32(origin + 4);
            uint32_t a1 = rd32(origin + 8);
            uint32_t b1 = rd32(origin + 12);
            uint32_t a2 = rd32(origin + 16);
            uint32_t b2 = rd32(origin + 20);
            uint32_t a3 = rd32(origin + 24);
            uint32_t b3 = rd32(origin + 28);

            wr8(output + 0, nibble_pairs_to_bits(a0, b0));
            wr8(output + 1, nibble_pairs_to_bits(a1, b1));
            wr8(output + 2, nibble_pairs_to_bits(a2, b2));
            wr8(output + 3, nibble_pairs_to_bits(a3, b3));
            origin += 32;
            output += 4;
        }
    }

    {
        const uint8_t *origin = source + primary * 8;
        uint8_t *output = dest + start + primary;

        for (uint64_t i = primary; i != count; ++i) {
            uint32_t first = rd32(origin + 0);
            uint32_t second = rd32(origin + 4);

            wr8(output, nibble_pairs_to_bits(first, second));
            origin += 8;
            ++output;
        }
    }
}



static uint8_t sparse_flags_from_words(uint32_t a, uint32_t b, uint32_t c,
                                uint32_t d)
{
    uint32_t v;

    v = ((a >> 15) & 0x10001u) | ((b >> 13) & 0x40004u);
    v |= ((c >> 11) & 0x100010u) | ((d >> 9) & 0x400040u);
    v |= v >> 15;
    return (uint8_t)v;
}

void gpu2d_worker_extract_sparse_flags(const uint8_t *entry, uint8_t *output,
                         uint32_t start_bits, uint32_t end_bits)
{

    uint32_t start = start_bits >> 3;
    uint32_t end = end_bits >> 3;
    uint32_t end_plus_one;
    uint64_t count;
    uint64_t width = 0;
    const uint8_t *source = entry;
    uint8_t *dest;

    if (start > end)
        return;

    end_plus_one = end + 1u;
    count = (uint64_t)end_plus_one - (uint64_t)start;
    dest = output + start;

    if (count >= 4u) {
        uintptr_t begin_output = (uintptr_t)output + start;
        uintptr_t end_entry = (uintptr_t)entry + (count << 4);
        uintptr_t end_output = (uintptr_t)output + end_plus_one;

        if (begin_output >= end_entry ||
            end_output <= (uintptr_t)entry) {
            width = count & UINT64_C(0xfffffffffffffffc);
        }
    }

    while (width != 0) {
        uint32_t a0 = rd32(source);
        uint32_t b0 = rd32(source + 4);
        uint32_t c0 = rd32(source + 8);
        uint32_t d0 = rd32(source + 12);
        uint32_t a1 = rd32(source + 16);
        uint32_t b1 = rd32(source + 20);
        uint32_t c1 = rd32(source + 24);
        uint32_t d1 = rd32(source + 28);
        uint32_t a2 = rd32(source + 32);
        uint32_t b2 = rd32(source + 36);
        uint32_t c2 = rd32(source + 40);
        uint32_t d2 = rd32(source + 44);
        uint32_t a3 = rd32(source + 48);
        uint32_t b3 = rd32(source + 52);
        uint32_t c3 = rd32(source + 56);
        uint32_t d3 = rd32(source + 60);

        wr8(dest, sparse_flags_from_words(a0, b0, c0, d0));
        wr8(dest + 1, sparse_flags_from_words(a1, b1, c1, d1));
        wr8(dest + 2, sparse_flags_from_words(a2, b2, c2, d2));
        wr8(dest + 3, sparse_flags_from_words(a3, b3, c3, d3));

        source += 64;
        dest += 4;
        count -= 4;
        width -= 4;
    }

    while (count != 0) {
        uint32_t a = rd32(source);
        uint32_t b = rd32(source + 4);
        uint32_t c = rd32(source + 8);
        uint32_t d = rd32(source + 12);

        wr8(dest, sparse_flags_from_words(a, b, c, d));
        source += 16;
        dest++;
        count--;
    }
}



static uint8_t sign_bits_from_words(uint32_t a, uint32_t b, uint32_t c,
                                uint32_t d)
{
    uint32_t v;

    v = ((a >> 15) & 0x10001u) | ((b >> 13) & 0x40004u);
    v |= ((c >> 11) & 0x100010u) | ((d >> 9) & 0x400040u);
    v |= v >> 15;
    return (uint8_t)v;
}

void gpu2d_worker_pack_sign_bits(const uint8_t *source, uint8_t *dest)
{

    uintptr_t end_source = (uintptr_t)source + 0x200u;
    uintptr_t end_dest = (uintptr_t)dest + 0x20u;

    if (end_source <= (uintptr_t)dest ||
        end_dest <= (uintptr_t)source) {
        for (uint32_t i = 0; i != 32; i += 4) {
            const uint8_t *origin = source + (size_t)i * 16;
            uint32_t a0 = rd32(origin + 0);
            uint32_t b0 = rd32(origin + 4);
            uint32_t c0 = rd32(origin + 8);
            uint32_t d0 = rd32(origin + 12);
            uint32_t a1 = rd32(origin + 16);
            uint32_t b1 = rd32(origin + 20);
            uint32_t c1 = rd32(origin + 24);
            uint32_t d1 = rd32(origin + 28);
            uint32_t a2 = rd32(origin + 32);
            uint32_t b2 = rd32(origin + 36);
            uint32_t c2 = rd32(origin + 40);
            uint32_t d2 = rd32(origin + 44);
            uint32_t a3 = rd32(origin + 48);
            uint32_t b3 = rd32(origin + 52);
            uint32_t c3 = rd32(origin + 56);
            uint32_t d3 = rd32(origin + 60);

            wr8(dest + i + 0, sign_bits_from_words(a0, b0, c0, d0));
            wr8(dest + i + 1, sign_bits_from_words(a1, b1, c1, d1));
            wr8(dest + i + 2, sign_bits_from_words(a2, b2, c2, d2));
            wr8(dest + i + 3, sign_bits_from_words(a3, b3, c3, d3));
        }
    } else {
        for (uint32_t i = 0; i != 32; ++i) {
            const uint8_t *origin = source + (size_t)i * 16;
            uint32_t a = rd32(origin + 0);
            uint32_t b = rd32(origin + 4);
            uint32_t c = rd32(origin + 8);
            uint32_t d = rd32(origin + 12);

            wr8(dest + i, sign_bits_from_words(a, b, c, d));
        }
    }
}
#undef OFF_MUTEX
#undef OFF_COND
#undef OBJ_RENDER_1
#undef OBJ_RENDER_2

#define OBJ_SIZE      0x81420u
#define MAX_THREADS    8
#define RING       64u
#define ENGINES      2




typedef struct { unsigned char *obj, *template; volatile int active; unsigned generation; volatile unsigned in_flight; } engine_t;
static engine_t engines[ENGINES];

typedef struct { unsigned char *shadow[ENGINES]; unsigned copy_generation[ENGINES]; unsigned id; } worker_t;
static pthread_t threads[MAX_THREADS];
static worker_t tr[MAX_THREADS + 1];
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv_ir = PTHREAD_COND_INITIALIZER;
static unsigned n_threads;
static volatile unsigned any_active;
static volatile unsigned sleeping;
static int shutdown = -1, boss_only;
static recon_b2_job *ring[RING];
static recon_b2_job *draft[ENGINES];
static volatile unsigned head __attribute__((aligned(64)));
static volatile unsigned tail   __attribute__((aligned(64)));
static volatile int lock_prod __attribute__((aligned(64)));

static inline void prod_close(void) { while (__atomic_exchange_n(&lock_prod, 1, __ATOMIC_ACQUIRE)) sched_yield(); }

static inline void prod_open(void)  { __atomic_store_n(&lock_prod, 0, __ATOMIC_RELEASE); }

static void run_job(worker_t *t, recon_b2_job *w)
{
    unsigned m = w->engine_idx;

    if (t->copy_generation[m] == ~0u) {
        memcpy(t->shadow[m], engines[m].template, OBJ_SIZE);
        t->copy_generation[m] = engines[m].generation;
    }
    gpu2d_line_finish_work(t->shadow[m], w);
}
#define SPINS_BEFORE_SLEEP 400u

static void *loop(void *p)
{
    worker_t *t = (worker_t *)p;
    unsigned turns = 0;
    for (;;) {
        unsigned c = __atomic_load_n(&tail, __ATOMIC_ACQUIRE);
        if (c == __atomic_load_n(&head, __ATOMIC_ACQUIRE)) {
            if (++turns < SPINS_BEFORE_SLEEP) { __asm__ __volatile__("yield"); continue; }
            turns = 0;
            pthread_mutex_lock(&mtx);
            __atomic_fetch_add(&sleeping, 1u, __ATOMIC_SEQ_CST);
            while (__atomic_load_n(&tail, __ATOMIC_SEQ_CST) == __atomic_load_n(&head, __ATOMIC_SEQ_CST))
                pthread_cond_wait(&cv_ir, &mtx);
            __atomic_fetch_sub(&sleeping, 1u, __ATOMIC_SEQ_CST);
            pthread_mutex_unlock(&mtx);
            continue;
        }
        turns = 0;
        unsigned r = c % RING;
        recon_b2_job *w = ring[r];
        if (!__atomic_load_n(&w->ready, __ATOMIC_ACQUIRE)) { sched_yield(); continue; }
        if (!__atomic_compare_exchange_n(&tail, &c, c + 1u, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) continue;
        unsigned mine = w->engine_idx;
        run_job(t, w);
        __atomic_store_n(&w->busy, 0, __ATOMIC_RELEASE);
#ifdef RECON_DIAG
        if (w->engine_idx != mine) nds_trace_2d_race();
#endif
        __atomic_fetch_sub(&engines[mine].in_flight, 1u, __ATOMIC_ACQ_REL);
    }
}

static int start_once_1(void);
static pthread_mutex_t mtx_start = PTHREAD_MUTEX_INITIALIZER;

static int start_threads(void)
{

    if (__atomic_load_n(&shutdown, __ATOMIC_ACQUIRE) >= 0) return !shutdown;
    pthread_mutex_lock(&mtx_start);
    int r = start_once_1();
    pthread_mutex_unlock(&mtx_start);
    return r;
}

static int start_once_1(void)
{
    if (shutdown >= 0) return !shutdown;
#ifndef RECON_COMPOSE_A_DEFAULT
#define RECON_COMPOSE_A_DEFAULT 255u
#endif
    unsigned n = nds_configured_2d_threads((unsigned)RECON_COMPOSE_A_DEFAULT);
    if (n == 0) { shutdown = 1; return 0; }
    if (n == 255u) {

        long c = sysconf(_SC_NPROCESSORS_ONLN);
        n = (c >= 6) ? (unsigned)(c - 1) : 0u;
        if (n > 7u) n = 7u;
        if (n == 0u) { shutdown = 1; return 0; }
    }
    boss_only = (n >= 100u);
    if (boss_only) n = 0;
    if (n > MAX_THREADS) n = MAX_THREADS;
    for (unsigned i = 0; i < RING; i++) {
        void *m = 0;
        if (posix_memalign(&m, 64, sizeof(recon_b2_job)) != 0) { shutdown = 1; return 0; }
        ring[i] = (recon_b2_job *)m; ring[i]->busy = ring[i]->ready = 0;
    }
    for (unsigned m = 0; m < ENGINES; m++) {
        void *q = 0;
        if (posix_memalign(&q, 64, sizeof(recon_b2_job)) != 0) { shutdown = 1; return 0; }
        draft[m] = (recon_b2_job *)q;
    }
    for (unsigned m = 0; m < ENGINES; m++) {
        void *q = 0; if (posix_memalign(&q, 64, OBJ_SIZE) != 0) { shutdown = 1; return 0; }
        engines[m].template = (unsigned char *)q;
    }
    for (unsigned i = 0; i <= n; i++) {
        tr[i].id = i;
        for (unsigned m = 0; m < ENGINES; m++) {
            tr[i].copy_generation[m] = ~0u;
            void *q = 0; if (posix_memalign(&q, 64, OBJ_SIZE) != 0) q = 0;
            tr[i].shadow[m] = (unsigned char *)q;
            if (!tr[i].shadow[m]) { shutdown = 1; return 0; }
        }
    }
    for (unsigned i = 0; i < n; i++)
        if (pthread_create(&threads[i], 0, loop, &tr[i]) != 0) break; else n_threads++;
    __atomic_store_n(&shutdown, (n_threads == 0 && !boss_only), __ATOMIC_RELEASE);
#ifdef RECON_DIAG
    nds_trace_2d_start(n_threads, boss_only, shutdown);
#endif
    return !shutdown;
}

static int frame_distributable(unsigned char *obj, unsigned char *cap)
{
    if (cap == 0 || cap[81] == 0) return 1;
    unsigned char *core = (unsigned char *)rd64(obj);
    unsigned bank = cap[78] & 3u;
    unsigned vramcnt = core[0x1b070 + 0x240 + bank];
    if (vramcnt & 0x80u) {
        unsigned mst = vramcnt & 7u;
        if (mst == 1u || mst == 2u) return 0;
        if (mst == 0u) {
            if (rd32(cap + 72) != 0u) return 0;
            if (rd16(cap + 76) != 256u) return 0;
        }
    }

    return 1;
}

static int engine_of(const void *ctx)
{
    for (int m = 0; m < ENGINES; m++)
        if (engines[m].active && engines[m].obj == (const unsigned char *)ctx) return m;
    return -1;
}

int recon_b2_active(const void *ctx) { return engine_of(ctx) >= 0; }

recon_b2_job *recon_b2_take(const void *ctx)
{
    if (boss_only) return ring[0];
    int m = engine_of(ctx);
    return draft[m < 0 ? 0 : m];
}

void recon_b2_enqueue(recon_b2_job *w, const void *ctx)
{
    int m = engine_of(ctx);
    w->engine_idx = (unsigned)(m < 0 ? 0 : m);
#ifdef RECON_DIAG
    nds_trace_2d_line();
#endif
    if (boss_only) { run_job(&tr[0], w); return; }
    __atomic_fetch_add(&engines[w->engine_idx].in_flight, 1u, __ATOMIC_ACQ_REL);
    prod_close();

    recon_b2_job *d;
    for (;;) {
        d = ring[head % RING];
        if (!__atomic_load_n(&d->busy, __ATOMIC_ACQUIRE)) break;
        prod_open(); sched_yield(); prod_close();
    }
    unsigned r = head % RING;
    memcpy(d, w, (size_t)(d->buf - (uint8_t *)d) + sizeof d->buf);
    d->prep.buf = (uint8_t *)(((uintptr_t)d->buf + 15) & ~(uintptr_t)15);
    d->slot = r; d->busy = 1;

    if (rd_ptr(w->cap + 64) == w->row_source) wr_ptr(d->cap + 64, d->row_source);
    if (d->prep.three == w->three_copy) d->prep.three = d->three_copy;
    __atomic_store_n(&d->ready, 1, __ATOMIC_RELEASE);
    __atomic_store_n(&head, head + 1u, __ATOMIC_SEQ_CST);
    prod_open();
    if (__atomic_load_n(&sleeping, __ATOMIC_SEQ_CST) != 0u) {
        pthread_mutex_lock(&mtx);
        pthread_cond_signal(&cv_ir);
        pthread_mutex_unlock(&mtx);
    }
}

static void drain_engine(int m)
{
    if (boss_only || m < 0) return;
#ifdef RECON_DIAG
    unsigned long turns = 0;
    while (__atomic_load_n(&engines[m].in_flight, __ATOMIC_ACQUIRE) != 0) {
        sched_yield();
        if (++turns % 2000000UL == 0UL && turns <= 10000000UL)
            nds_trace_drain_wait(m, __atomic_load_n(&engines[0].in_flight, __ATOMIC_ACQUIRE),
                              __atomic_load_n(&engines[1].in_flight, __ATOMIC_ACQUIRE),
                              __atomic_load_n(&head, __ATOMIC_SEQ_CST),
                              __atomic_load_n(&tail, __ATOMIC_SEQ_CST),
                              __atomic_load_n(&sleeping, __ATOMIC_SEQ_CST),
                              __atomic_load_n(&any_active, __ATOMIC_ACQUIRE), n_threads, turns);
    }
#else
    while (__atomic_load_n(&engines[m].in_flight, __ATOMIC_ACQUIRE) != 0) sched_yield();
#endif
}

void recon_b2_drain(const void *ctx) { drain_engine(engine_of(ctx)); }

static int dispatch(int m, unsigned char *obj, uint32_t start, uint32_t end, void *arg)
{
    if (recon_scale_out <= 2u || start > end) return 0;

    if (end - start + 1u < 16u) return 0;
    if (!start_threads()) return 0;
    if (!frame_distributable(obj, (unsigned char *)arg)) {
#ifdef RECON_DIAG
        nds_trace_2d_serial();
#endif
        return 0;
    }
    engines[m].obj = obj;
    if (engines[m].generation == 0u) memcpy(engines[m].template, obj, OBJ_SIZE);
    engines[m].generation++;
    engines[m].active = 1;
    __atomic_fetch_add(&any_active, 1u, __ATOMIC_ACQ_REL);
    gpu2d_line_process_range((gpu2d_engine_t *)obj, start, end, arg);
    drain_engine(m);
    engines[m].active = 0;
    __atomic_fetch_sub(&any_active, 1u, __ATOMIC_ACQ_REL);
#ifdef RECON_DIAG
    if (m == 0 && (engines[0].generation & 255u) == 0u) nds_trace_2d_report(engines[0].generation);
#endif
    return 1;
}

int recon_compose_a_shared(gpu2d_engine_t *obj, uint32_t start, uint32_t end, void *arg) { return dispatch(0, (unsigned char *)obj, start, end, arg); }

int recon_compose_b_shared(gpu2d_engine_t *obj, uint32_t start, uint32_t end, void *arg) { return dispatch(1, (unsigned char *)obj, start, end, arg); }
#undef OBJ_SIZE
#undef MAX_THREADS
#undef RING
#undef ENGINES
#undef SPINS_BEFORE_SLEEP

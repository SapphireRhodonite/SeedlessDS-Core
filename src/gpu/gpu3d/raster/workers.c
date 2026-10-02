#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "present_hook.h"
#include <stddef.h>
#include "../../gpu.h"
#include "raster.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"

unsigned char raster_downsample_line[RASTER_DOWNSAMPLE_LINE_BYTES];
uint32_t raster_reciprocal_30[RASTER_RECIPROCAL_TABLE_WORDS];
uint32_t raster_reciprocal_31[RASTER_RECIPROCAL_TABLE_WORDS];
uint64_t raster_frame_thread[2];

void gpu3d_raster_frame_dispatch(gpu_t *gpu, uint32_t mode) {

    int (*mtx_lock)(void *)   = nds_platform_default()->threads.mutex_lock;
    int (*mtx_unlock)(void *) = nds_platform_default()->threads.mutex_unlock;
    int (*cnd_signal)(void *) = nds_platform_default()->threads.cond_signal;
    int (*cnd_wait)(void *, void *) = nds_platform_default()->threads.cond_wait;

    unsigned char *machine = (unsigned char *)gpu;
    gpu3d_t *g3 = GPU3D_OF(gpu);
    gpu3d_raster_t *state = &gpu->raster;
    { void recon_scale_3d_read(void); recon_scale_3d_read(); }
    unsigned char *buf = recon_buf3d_front(machine);

    uint32_t mark = (state->disp3dcnt >> 8) & 0xff;
    gpu3d_texture_cache_t *ctx = g3->texture_cache;

    uint32_t has = 0;
    if (mark & 0x40)
        has = ((uint16_t)(ctx->dirty_pending[0] >> 16) != 0);

    uint32_t pend = gpu3d_raster_texture_cache_purge_dirty(ctx);

    unsigned char *g = (unsigned char *)gpu->vram.config;
    unsigned char *dest;

    if (*(uint32_t *)(g + 1128) != 0) {
        if (state->frame_back == buf) {
            dest = recon_buf3d_back(machine);
            state->frame_front = dest;
        } else {
            state->frame_front = buf;
            dest = buf;
        }
    } else {
        uint32_t v = g3->disp3dcnt;
        state->disp3dcnt = v;
        state->alpha_test_ref = (v & 4) ? g3->alpha_test_ref : 0;
        dest = state->frame_front;
    }

    if (mode == 0 && ((pend | has) != 0 || g3->render_dirty != 0)) {

        state->frame_last = dest;
        recon_scale_render_tag = recon_scale_3d;

        uint64_t which = (uint64_t)g3->bank ^ 1u;
        g3->render_dirty = 0;

        uint32_t sel = state->disp3dcnt & 0xff;
        gpu3d_polygon_list_t *a = &g3->opaque[which];
        gpu3d_polygon_list_t *b = &g3->translucent[which];

        unsigned char *C0 = recon_ctx3d_base(machine);
        const unsigned long BLK = recon_block_stride;
        gpu3d_band_header_t *boss_hdr = GPU3D_BAND_HEADER(C0);
        gpu3d_bank_vertex_t *com = g3->vertex_bank[which].vertex;

        if (sel & 0x80)
            gpu3d_raster_block_summarize_and_delta(g3, buf);

        gpu3d_raster_band_bucket_classify_by_depth(
            g3, gpu->bucket[0], a, com, 0);
        gpu3d_raster_band_bucket_classify(
            g3, gpu->bucket[1], b, com, 1);

        g = (unsigned char *)gpu->vram.config;
        uint32_t batch = *(uint32_t *)(g + 1180);
        boss_hdr->pass = (uint8_t)batch;
        uint32_t threads = recon_threads_3d(*(uint32_t *)(g + 1168));

        unsigned char *boss = C0;

        if (threads > 1) {
            gpu3d_band_header_t *w = GPU3D_BAND_HEADER(C0 + BLK);
            uint64_t left = (uint64_t)threads - 2;
            for (;;) {
                w->pass = (uint8_t)batch;
                w->threads = (uint8_t)threads;
                mtx_lock(w->mutex);
                w->start = 1;
                cnd_signal(w->cond);
                mtx_unlock(w->mutex);
                if (left == 0) break;
                g = (unsigned char *)gpu->vram.config;
                w = (gpu3d_band_header_t *)((unsigned char *)w + BLK);
                left--;
                batch = *(uint32_t *)(g + 1180);
            }
            boss_hdr->threads = (uint8_t)threads;
            gpu3d_raster_band_draw_dispatch(boss);

            if (threads >= 2) {
                for (uint64_t i = 1; i != threads; i++) {
                    gpu3d_band_header_t *p = GPU3D_BAND_HEADER(C0 + i * BLK);
                    mtx_lock(p->mutex_done);
                    if (p->done == 0) {
                        do { cnd_wait(p->cond_done, p->mutex_done); } while (p->done == 0);
                    }
                    p->done = 0;
                    mtx_unlock(p->mutex_done);
                }
            }
        } else {
            boss_hdr->threads = (uint8_t)threads;
            gpu3d_raster_band_draw_dispatch(boss);
        }

        uint32_t end = state->disp3dcnt;
        if ((end & 0x20) && *(uint32_t *)((unsigned char *)gpu->vram.config + 1180) == 0) {
            uint32_t k = (end >> 6) & 3;
            if (k < 2)       gpu3d_raster_band_flush_plain(gpu);
            else if (k == 2) gpu3d_raster_band_flush_fogged(gpu);
            else             gpu3d_raster_band_compose_set_alpha(gpu);
        }
        return;
    }

    if (*(uint32_t *)(g + 1128) == 0) return;
    void *prev = state->frame_last;
    if ((void *)dest == prev) return;
    ((void *(*)(void *, const void *, unsigned long))sym_libc_memcpy)(
               dest, prev, 0x30000);
}

unsigned char *gpu3d_raster_scanline_lookup_half(const gpu_t *ctx, uint32_t idx) {
    const unsigned char *sub = (const unsigned char *)ctx->vram.config;
    uint32_t flag;
    memcpy(&flag, sub + 1128, 4);
    unsigned char *table = (flag == 0) ? ctx->raster.frame_front : ctx->raster.frame_back;
    uint32_t row = idx << 8;
    return table + (uint64_t)row * 4;
}

void gpu3d_raster_scanline_scatter(const unsigned char *base,
                        unsigned char *dst_a,
                        unsigned char *dst_b,
                        uint32_t rows,
                        uint32_t value,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *act)
{

    if (rows == 0) return;

    uint32_t f = 0;
    const unsigned char *p_cnt = base + 0x630;
    const unsigned char *p_hue = base + 0x580;
    uint32_t mask = value << 24;

    uint16_t n16, hue16;
    memcpy(&n16,   p_cnt, 2); p_cnt += 4;
    memcpy(&hue16, p_hue, 2); p_hue += 4;

    uint64_t n   = n16;
    uint64_t hue = hue16;

    for (;;) {
        if (n != 0) {
            uint64_t shift = hue << 2;
            uint64_t off  = 0;
            uint64_t lim  = n << 2;

            unsigned char *wb = dst_b + shift;
            unsigned char *wa = dst_a + shift;
            const unsigned char *p = act;

            do {
                unsigned char c = *p++;
                if (c != 0) {
                    uint32_t vb, va;
                    memcpy(&vb, src_b + off, 4);
                    memcpy(&va, src_a + off, 4);
                    vb |= mask;
                    memcpy(wb + off, &vb, 4);
                    memcpy(wa + off, &va, 4);
                }
                off += 4;
            } while (lim != off);

            act   += n;
            src_a += off;
            src_b += off;
        }

        f += 1;
        dst_a += 0x800;
        dst_b += 0x800;
        if (f == rows) return;

        memcpy(&n16,   p_cnt, 2); p_cnt += 4;
        memcpy(&hue16, p_hue, 2); p_hue += 4;
        n   = n16;
        hue = hue16;
    }
}

static void recon_reduce_hires(unsigned char *machine, gpu3d_raster_t *state)
{

                const unsigned nm = recon_scale_3d / 2u;
                const unsigned sw = RECON_3D_WIDTH;
                const unsigned dw = 512u, dh = 384u;
                const uint32_t *src = (const uint32_t *)state->frame_front;
                const unsigned char *fr = recon_buf3d_front(machine);
                uint32_t *dst = (uint32_t *)((const unsigned char *)src == fr ? ((gpu_t *)machine)->frame[0] : ((gpu_t *)machine)->frame[1]);
                if (src && (const uint32_t *)dst != src) {

                    const unsigned sp = sw;
                    const unsigned np = nm * nm;
                    for (unsigned y = 0; y < dh; y++)
                        for (unsigned x = 0; x < dw; x++) {
                            unsigned c0 = 0, c1 = 0, c2 = 0, c3 = 0;
                            const uint32_t *row =
                                src + (size_t)(y * nm) * sp + (size_t)(x * nm);
                            for (unsigned j = 0; j < nm; j++) {
                                for (unsigned i = 0; i < nm; i++) {
                                    uint32_t v = row[(size_t)j * sp + i];
                                    c0 +=  v        & 0xffu;
                                    c1 += (v >>  8) & 0xffu;
                                    c2 += (v >> 16) & 0xffu;
                                    c3 += (v >> 24) & 0xffu;
                                }
                            }
                            dst[(size_t)y * dw + x] = (c0/np) | ((c1/np) << 8)
                                              | ((c2/np) << 16) | ((c3/np) << 24);
                        }

                    memcpy((void *)src, dst, (size_t)dw * dh * 4u);
                    memcpy((uint32_t *)(void *)src + (size_t)dw * dh,
                           ((gpu_t *)machine)->frame[1], (size_t)dw * dh * 4u);
                }

}

void gpu3d_raster_frame_dispatch_hires(gpu_t *gpu, uint32_t mode) {
    nds_trace_3d_dispatch();

    int (*mtx_lock)(void *)   = nds_platform_default()->threads.mutex_lock;
    int (*mtx_unlock)(void *) = nds_platform_default()->threads.mutex_unlock;
    int (*cnd_signal)(void *) = nds_platform_default()->threads.cond_signal;
    int (*cnd_wait)(void *, void *) = nds_platform_default()->threads.cond_wait;

    unsigned char *machine = (unsigned char *)gpu;
    gpu3d_t *g3 = GPU3D_OF(gpu);
    gpu3d_raster_t *state = &gpu->raster;
    { void recon_scale_3d_read(void); recon_scale_3d_read(); }
    unsigned char *buf = recon_buf3d_front(machine);

    uint32_t mark = (state->disp3dcnt >> 8) & 0xff;
    gpu3d_texture_cache_t *ctx = g3->texture_cache;

    uint32_t has = 0;
    if (mark & 0x40)
        has = ((uint16_t)(ctx->dirty_pending[0] >> 16) != 0);

    uint32_t pend = gpu3d_raster_texture_cache_purge_dirty(ctx);

    unsigned char *g = (unsigned char *)gpu->vram.config;
    unsigned char *dest;

    if (*(uint32_t *)(g + 1128) != 0) {
        if (state->frame_back == buf) {
            dest = recon_buf3d_back(machine);
            state->frame_front = dest;
        } else {
            state->frame_front = buf;
            dest = buf;
        }
    } else {
        uint32_t v = g3->disp3dcnt;
        state->disp3dcnt = v;
        state->alpha_test_ref = (v & 4) ? g3->alpha_test_ref : 0;
        dest = state->frame_front;

        if (dest == 0 && recon_scale_3d > 2u) {
            dest = recon_buf3d_front(machine);
            state->frame_front = dest;
        }
    }


    if (mode == 0 && ((pend | has) != 0 || g3->render_dirty != 0)) {

        state->frame_last = dest;
        recon_scale_render_tag = recon_scale_3d;

        uint64_t which = (uint64_t)g3->bank ^ 1u;
        recon_which_3d = (unsigned)which;
        g3->render_dirty = 0;

        uint32_t sel = state->disp3dcnt & 0xff;
        gpu3d_polygon_list_t *a = &g3->opaque[which];
        gpu3d_polygon_list_t *b = &g3->translucent[which];

        unsigned char *C0 = recon_ctx3d_base(machine);
        const unsigned long BLK = recon_block_stride;
        gpu3d_band_header_t *boss_hdr = GPU3D_BAND_HEADER(C0);
        gpu3d_bank_vertex_t *com = g3->vertex_bank[which].vertex;

        recon_bands_set();
        if (RECON_BANDS != 12u
            && recon_threads_3d(*(uint32_t *)((unsigned char *)gpu->vram.config + 1168)) < 2u) {
            recon_bands_3d = 12u; recon_band_rows = 192u * recon_scale_3d / 12u; }
        if (sel & 0x80)
            gpu3d_raster_block_summarize_and_delta(g3, buf);

        gpu3d_raster_band_bucket_classify_by_depth_hires(
            g3, recon_buckets(machine, 0), a, com, 0);
        gpu3d_raster_band_bucket_classify_hires(
            g3, recon_buckets(machine, 1), b, com, 1);

        g = (unsigned char *)gpu->vram.config;
        uint32_t batch = *(uint32_t *)(g + 1180);
        boss_hdr->pass = (uint8_t)batch;
        uint32_t threads = recon_threads_3d(*(uint32_t *)(g + 1168));

        unsigned char *boss = C0;

        recon_band_reset();
        if (threads > 1) {
            gpu3d_band_header_t *w = GPU3D_BAND_HEADER(C0 + BLK);
            uint64_t left = (uint64_t)threads - 2;
            for (;;) {
                w->pass = (uint8_t)batch;
                w->threads = (uint8_t)threads;
                mtx_lock(w->mutex);
                w->start = 1;
                cnd_signal(w->cond);
                mtx_unlock(w->mutex);
                if (left == 0) break;
                g = (unsigned char *)gpu->vram.config;
                w = (gpu3d_band_header_t *)((unsigned char *)w + BLK);
                left--;
                batch = *(uint32_t *)(g + 1180);
            }
            boss_hdr->threads = (uint8_t)threads;
            gpu3d_raster_band_pending_process(boss);

            if (threads >= 2) {
                for (uint64_t i = 1; i != threads; i++) {
                    gpu3d_band_header_t *p = GPU3D_BAND_HEADER(C0 + i * BLK);
                    mtx_lock(p->mutex_done);
                    if (p->done == 0) {
                        do { cnd_wait(p->cond_done, p->mutex_done); } while (p->done == 0);
                    }
                    p->done = 0;
                    mtx_unlock(p->mutex_done);
                }
            }
        } else {
            boss_hdr->threads = (uint8_t)threads;
            gpu3d_raster_band_pending_process(boss);
        }

        uint32_t end = state->disp3dcnt;
        if ((end & 0x20) && *(uint32_t *)((unsigned char *)gpu->vram.config + 1180) == 0) {
            uint32_t k = (end >> 6) & 3;

            if (k < 2)       gpu3d_raster_band_flush_scaled(gpu);
            else if (k == 2) gpu3d_raster_band_compose_block_pairs(gpu);
            else             gpu3d_raster_band_pair_compose(gpu);

            recon_pass++;

            recon_native_read();
            if (recon_scale_3d > 2u && !recon_native) recon_reduce_hires(machine, state);
            return;
        }

        recon_native_read();
        if (recon_scale_3d > 2u && !recon_native) recon_reduce_hires(machine, state);
        return;
    }

    if (*(uint32_t *)(g + 1128) == 0) return;
    void *prev = state->frame_last;
    if ((void *)dest == prev) return;
    ((void *(*)(void *, const void *, unsigned long))sym_libc_memcpy)(
               dest, prev, recon_buf3d_bytes());

}

unsigned char *gpu3d_raster_scanline_lookup(const gpu_t *ctx, uint32_t idx) {
    const unsigned char *sub = (const unsigned char *)ctx->vram.config;

    uint32_t flag;
    memcpy(&flag, sub + 1128, sizeof(flag));

    unsigned char *table = (flag == 0) ? ctx->raster.frame_front : ctx->raster.frame_back;

    uint32_t row = idx << 9;
    return table + (uint64_t)row * 4;
}

void gpu3d_raster_frame_dispatch_select(gpu_t *gpu, uint32_t mode) {
    uint32_t flag = *(uint32_t *)((unsigned char *)gpu->vram.config + 1184);

    if (flag == 0)
        gpu3d_raster_frame_dispatch(gpu, mode);
    else
        gpu3d_raster_frame_dispatch_hires(gpu, mode);
}

extern void gpu3d_raster_frame_dispatch_6(gpu_t *machine, uint32_t mode) __asm__("gpu3d_raster_frame_dispatch");
extern void gpu3d_raster_frame_dispatch_hires_6(gpu_t *machine, uint32_t mode) __asm__("gpu3d_raster_frame_dispatch_hires");
typedef int  (*fn_mtx)(void *);
typedef int  (*fn_wait)(void *, void *);


void gpu3d_raster_frame_thread_run(void *ctx)
{
    gpu_t *c = (gpu_t *)ctx;
    gpu3d_raster_t *b = &c->raster;
    void *mA = b->mutex_work, *cA = b->cond_work;
    void *mB = b->mutex_done, *cB = b->cond_done;
    void *mP = b->mutex_idle, *cP = b->cond_idle;

    fn_mtx  lock   = nds_platform_default()->threads.mutex_lock;
    fn_mtx  unlock = nds_platform_default()->threads.mutex_unlock;
    fn_mtx  signal = nds_platform_default()->threads.cond_signal;
    fn_wait wait   = nds_platform_default()->threads.cond_wait;

    for (;;) {

        while (rd32((uint8_t *)c->vram.config + 1128) == 0) {
            lock(mP);
            b->frame_idle = 1;
            wait(cP, mP);
            unlock(mP);
        }

        for (;;) {
            b->frame_idle = 0;
            lock(mA);
            while (b->frame_request == 0)
                wait(cA, mA);
            b->frame_request = 0;
            b->frame_busy = 1;
            unlock(mA);

            uint32_t which = b->frame_mode;
            if (rd32((uint8_t *)c->vram.config + 1184) != 0)
                gpu3d_raster_frame_dispatch_hires(c, which);
            else
                gpu3d_raster_frame_dispatch(c, which);

            lock(mB);
            b->frame_busy = 0;
            signal(cB);
            unlock(mB);

            if (rd32((uint8_t *)c->vram.config + 1128) == 0) break;
        }
    }
}
typedef int (*fn_un_arg)(void *);

int gpu3d_raster_frame_thread_submit(void *ctx, uint32_t selector)
{

    gpu_t *c = (gpu_t *)ctx;
    gpu3d_raster_t *r = &c->raster;
    gpu3d_t *g3 = GPU3D_OF(c);
    fn_un_arg signal = nds_platform_default()->threads.cond_signal;
    fn_un_arg lock = nds_platform_default()->threads.mutex_lock;
    fn_un_arg unlock = nds_platform_default()->threads.mutex_unlock;
    uint32_t state;

    if (r->frame_idle != 0)
        signal(r->cond_idle);

    lock(r->mutex_work);

    r->frame_request = 1;
    r->frame_mode = (uint8_t)selector;

    state = g3->disp3dcnt;
    r->disp3dcnt = state;
    if ((state & (1u << 2)) == 0)
        state = 0;
    else
        state = g3->alpha_test_ref;
    r->alpha_test_ref = state;

    signal(r->cond_work);

    return unlock(r->mutex_work);
}

typedef int (*fn_mtx_8)(void *);
typedef int (*fn_wait_8)(void *, void *);

int gpu3d_raster_frame_thread_publish(void *ctx)
{

    gpu_t *c = (gpu_t *)ctx;
    gpu3d_raster_t *fb = &c->raster;
    void *mtx = fb->mutex_done;
    void *cnd = fb->cond_done;

    fn_mtx_8  lock   = nds_platform_default()->threads.mutex_lock;
    fn_mtx_8  unlock = nds_platform_default()->threads.mutex_unlock;
    fn_wait_8 wait = nds_platform_default()->threads.cond_wait;

    lock(mtx);

    if (fb->frame_busy == 1) {
        do {
            wait(cnd, mtx);

        } while (fb->frame_busy == 1);
    }

    fb->frame_back = fb->frame_front;
    recon_scale_commit_output(recon_scale_render_tag);

    return unlock(mtx);
}

typedef int  (*fn_mtx_9)(void *);
typedef int  (*fn_wait_9)(void *, void *);
extern void gpu3d_raster_band_draw_dispatch(unsigned char *machine);


void gpu3d_raster_band_thread_run(void *ctx)
{
    uint8_t *c = (uint8_t *)ctx;
    gpu3d_band_header_t *s = GPU3D_BAND_HEADER(c);

    const uint8_t *count = (const uint8_t *)s->gpu->vram.config;

    void *mA = s->mutex, *mB = s->mutex_done;
    void *cA = s->cond, *cB = s->cond_done;

    fn_mtx_9  lock   = nds_platform_default()->threads.mutex_lock;
    fn_mtx_9  unlock = nds_platform_default()->threads.mutex_unlock;
    fn_mtx_9  signal = nds_platform_default()->threads.cond_signal;
    fn_wait_9 wait   = nds_platform_default()->threads.cond_wait;

    lock(mA);
    for (;;) {

        while (s->start == 0)
            wait(cA, mA);

        s->start = 0;
        unlock(mA);

        if (rd32(count + 1184) == 0) gpu3d_raster_band_draw_dispatch((unsigned char *)ctx);
        else                         gpu3d_raster_band_pending_process(ctx);

        lock(mB);
        s->done = 1;
        signal(cB);
        unlock(mB);

        lock(mA);

    }
}

static int (*core_create)(void *, void *, void *, void *);
static int (*core_minit)(void *, void *);
static int (*core_cinit)(void *, void *);

long gpu3d_raster_pipeline_init(gpu_t *obj) {
    if (!core_create) {
        core_create = (int (*)(void *, void *, void *, void *))platform_thread_create;
        core_minit  = (int (*)(void *, void *))platform_mutex_init;
        core_cinit  = (int (*)(void *, void *))platform_cond_init;
    }

    uint32_t *t1 = &raster_reciprocal_30[1];
    uint32_t *t2 = &raster_reciprocal_31[1];
    for (uint32_t k = 0; k < RASTER_RECIPROCAL_FILLED; k++) {
        t1[k] = (k + RASTER_RECIPROCAL_30) / (k + 1u);
        t2[k] = (k + RASTER_RECIPROCAL_31) / (k + 1u);
    }

    void *routine = gpu3d_raster_band_thread_run;
    gpu3d_t *common = GPU3D_OF(obj);

    recon_scale_ceiling_read();
    unsigned char *C0 = recon_ctx3d_base((unsigned char *)obj);
    gpu3d_band_header_t *D = GPU3D_BAND_HEADER(C0);
    const unsigned long BLK = recon_block_stride;

    D->index = 0;
    D->gpu = obj;
    D->gpu3d = common;
    {
        gpu3d_band_header_t *A = (gpu3d_band_header_t *)(obj->band_context[0] + GPU3D_BAND_HEADER_OFFSET);
        A->gpu = obj;
        A->gpu3d = common;
    }

    const int nw = (recon_scale_ceiling > 2u) ? (int)RECON_HIRES_THREADS - 1 : 3;
    for (int i = 0; i < nw; i++) {
        gpu3d_band_header_t *c = GPU3D_BAND_HEADER(C0 + (unsigned long)(i + 1) * BLK);
        unsigned char *a = C0 + (unsigned long)(i + 1) * BLK;
        c->gpu = obj;
        c->gpu3d = common;
        c->index = (unsigned char)(i + 1);

        core_minit(c->mutex, 0);
        core_minit(c->mutex_done, 0);
        core_cinit(c->cond, 0);
        core_cinit(c->cond_done, 0);

        c->start = 0;
        c->done = 0;

        core_create(&c->thread, 0, routine, a);
    }

    recon_threads_created = (unsigned)nw + 1u;

    core_minit(obj->raster.mutex_work, 0);
    core_minit(obj->raster.mutex_done, 0);
    core_minit(obj->raster.mutex_idle, 0);
    core_cinit(obj->raster.cond_work, 0);
    core_cinit(obj->raster.cond_done, 0);
    core_cinit(obj->raster.cond_idle, 0);

    obj->raster.frame_request = 0;
    obj->raster.frame_busy = 0;
    obj->raster.frame_idle = 0;

    return core_create(raster_frame_thread, 0, gpu3d_raster_frame_thread_run, obj);
}



void gpu3d_raster_plane_box_average(void *param_1, const void *param_2)
{

    uint8_t *dest = (uint8_t *)param_1;
    const uint8_t *origin = (const uint8_t *)param_2;
    uintptr_t d = (uintptr_t)dest;
    uintptr_t s = (uintptr_t)origin;
    const uint32_t mask = UINT32_C(0x1f3f3f3f);
    uint32_t i;

    if (s + UINT64_C(0x1000) <= d || d + UINT64_C(0x400) <= s) {
        for (i = 0; i != 0x400u; i += 0x20u) {
            uint32_t a0 = rd32(origin + i + 0u);
            uint32_t a1 = rd32(origin + i + 4u);
            uint32_t a2 = rd32(origin + i + 8u);
            uint32_t a3 = rd32(origin + i + 12u);
            uint32_t a4 = rd32(origin + i + 16u);
            uint32_t a5 = rd32(origin + i + 20u);
            uint32_t a6 = rd32(origin + i + 24u);
            uint32_t a7 = rd32(origin + i + 28u);
            uint32_t b0 = rd32(origin + i + 0x400u);
            uint32_t b1 = rd32(origin + i + 0x404u);
            uint32_t b2 = rd32(origin + i + 0x408u);
            uint32_t b3 = rd32(origin + i + 0x40cu);
            uint32_t b4 = rd32(origin + i + 0x410u);
            uint32_t b5 = rd32(origin + i + 0x414u);
            uint32_t b6 = rd32(origin + i + 0x418u);
            uint32_t b7 = rd32(origin + i + 0x41cu);
            uint32_t c0 = rd32(origin + i + 0x800u);
            uint32_t c1 = rd32(origin + i + 0x804u);
            uint32_t c2 = rd32(origin + i + 0x808u);
            uint32_t c3 = rd32(origin + i + 0x80cu);
            uint32_t c4 = rd32(origin + i + 0x810u);
            uint32_t c5 = rd32(origin + i + 0x814u);
            uint32_t c6 = rd32(origin + i + 0x818u);
            uint32_t c7 = rd32(origin + i + 0x81cu);
            uint32_t e0 = rd32(origin + i + 0xc00u);
            uint32_t e1 = rd32(origin + i + 0xc04u);
            uint32_t e2 = rd32(origin + i + 0xc08u);
            uint32_t e3 = rd32(origin + i + 0xc0cu);
            uint32_t e4 = rd32(origin + i + 0xc10u);
            uint32_t e5 = rd32(origin + i + 0xc14u);
            uint32_t e6 = rd32(origin + i + 0xc18u);
            uint32_t e7 = rd32(origin + i + 0xc1cu);

            a0 = ((a0 + b0 + c0 + e0) >> 2) & mask;
            a1 = ((a1 + b1 + c1 + e1) >> 2) & mask;
            a2 = ((a2 + b2 + c2 + e2) >> 2) & mask;
            a3 = ((a3 + b3 + c3 + e3) >> 2) & mask;
            a4 = ((a4 + b4 + c4 + e4) >> 2) & mask;
            a5 = ((a5 + b5 + c5 + e5) >> 2) & mask;
            a6 = ((a6 + b6 + c6 + e6) >> 2) & mask;
            a7 = ((a7 + b7 + c7 + e7) >> 2) & mask;

            wr32(dest + i + 0u, a0);
            wr32(dest + i + 4u, a1);
            wr32(dest + i + 8u, a2);
            wr32(dest + i + 12u, a3);
            wr32(dest + i + 16u, a4);
            wr32(dest + i + 20u, a5);
            wr32(dest + i + 24u, a6);
            wr32(dest + i + 28u, a7);
        }
    } else {
        for (i = 0; i != 0x400u; i += 4u) {
            uint32_t sum = rd32(origin + i);
            sum += rd32(origin + i + 0x400u);
            sum += rd32(origin + i + 0x800u);
            sum += rd32(origin + i + 0xc00u);
            wr32(dest + i, (sum >> 2) & mask);
        }
    }
}

#define OFF_FLAGA     0x4a0
#define OFF_FLAGB     0x468

void *gpu3d_raster_scanline_lookup_filtered(gpu_t *param_1, int param_2)
{

    gpu_t *p1 = param_1;
    unsigned char *c2 = (unsigned char *)p1->vram.config;

    uint32_t flagA, flagB;
    memcpy(&flagA, c2 + OFF_FLAGA, sizeof(flagA));
    memcpy(&flagB, c2 + OFF_FLAGB, sizeof(flagB));

    uint32_t p2 = (uint32_t)param_2;
    unsigned char *table = (flagB == 0) ? p1->raster.frame_front : p1->raster.frame_back;

    if (flagA == 0) {

        uint32_t idx = p2 << 8;
        uint64_t byteoff = (uint64_t)idx << 2;
        return table + byteoff;
    }

    recon_native_read();
    unsigned nn = 4u;
    if (recon_native && recon_scale_out > 2u)
        nn = recon_scale_out * recon_scale_out;
    uint32_t idx = p2 * (256u * nn);
    uint64_t byteoff = (uint64_t)idx << 2;
    unsigned char *puVar6 = table + byteoff;

    if (flagA & 1u) {

        return puVar6;
    }

    unsigned char *dst = raster_downsample_line;

    for (uint32_t i = 0; i < 0x400u; i += 4) {
        uint32_t acc[64];
        for (unsigned k = 0; k < nn; k++)
            memcpy(&acc[k], puVar6 + i + (unsigned long)k * 1024u, 4);
        unsigned n = nn;
        while (n > 1u) {
            for (unsigned k = 0; k + 3u < n; k += 4u) {
                uint32_t sum = acc[k + 1] + acc[k];
                sum = sum + acc[k + 2];
                sum = sum + acc[k + 3];
                acc[k / 4u] = (sum >> 2) & 0x1f3f3f3fu;
            }
            n /= 4u;
        }
        memcpy(dst + i, &acc[0], 4);
    }

    return dst;
}
#undef OFF_FLAGA
#undef OFF_FLAGB

static void *(*core_memset)(void *, int, size_t);

void gpu3d_raster_double_buffer_init(unsigned char *base) {
    if (!core_memset)
        core_memset = (void *(*)(void *, int, size_t))sym_libc_memset;

    unsigned char *arena = base;
    gpu3d_raster_t *desc = &((gpu_t *)arena)->raster;
    recon_scale_align_ceiling();
    unsigned long  size   = recon_buf3d_bytes();
    unsigned char *b0    = recon_buf3d_front(arena);
    unsigned char *b1    = recon_buf3d_back(arena);

    core_memset(b0, 0, size);
    core_memset(b1, 0, size);

    desc->frame_back = b1;
    desc->frame_last = b0;
    desc->frame_front = b0;
}

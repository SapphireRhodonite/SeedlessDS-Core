#include "core_internals.h"
#include <stdint.h>
#define GPU_HUD_RING_SLOTS 20u
static char     gpu_hud_text[128];
static uint32_t gpu_hud_frames_lost;
static uint32_t gpu_hud_frames_total;
static uint64_t gpu_hud_prev_instructions[2];
static uint64_t gpu_hud_ring[GPU_HUD_RING_SLOTS];

static const uint32_t gpu_capture_widths[4]  = { 128u, 256u, 256u, 256u };
static const uint32_t gpu_capture_heights[4] = { 128u,  64u, 128u, 192u };
static const double gpu_hud_fps_numerator   = 0x1.e331eaaaaaaabp+24;
static const double gpu_hud_frame_period_us = 0x1.046aaaaaaaaabp+14;
static uint32_t gpu_hud_ring_index = GPU_HUD_RING_SLOTS;

#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stdarg.h>
#include <stddef.h>
#include "core/nds_state.h"
#include "mem_access.h"

#define DMA_N     4
#define DMA_STRIDE  40
#define LINE     20
#define LAST_LINE    0xbf
#define EVENT_QUEUE      0x318
#define EVENT    0x48
#define DELAY   0x4a4

static void notify(uint8_t *b, arm_t *cpu) {
    uint32_t f = b[0];
    b[0] = (uint8_t)(f | 2u);
    if (!(f & 0x10u)) return;
    io_mirror_t *r = cpu->io_mirror;
    uint32_t v = r->irq.if_pending | 2u;
    r->irq.if_pending = (v);
    if ((uint8_t)cpu->halt_flags & 6) return;

    cpu->irq_pending = r->irq.ie & v & (0u - r->irq.ime);
}

void gpu_vblank_dispatch(void *machine, void *argument) {
    (void)argument;
    uint8_t *ctx = machine;

    uint8_t *ia = (uint8_t *)&((nds_t *)ctx)->bus.io_mirror[0].dispstat;
    uint8_t *ib = (uint8_t *)&((nds_t *)ctx)->bus.io_mirror[1].dispstat;

    notify(ia, &((nds_t *)ctx)->arm9);
    notify(ib, &((nds_t *)ctx)->arm7);

    uint32_t line = rd16(ctx + LINE);

    if (line <= LAST_LINE) {
        nds_runtime_t *end = &((nds_t *)ctx)->runtime;
        uint8_t *ctrl = (uint8_t *)&((nds_t *)ctx)->bus.dma[0].channels[0].dst;
        uint8_t *engine  = (uint8_t *)&((nds_t *)ctx)->bus.dma[0];
        uint8_t *vid  = (uint8_t *)&((nds_t *)ctx)->gpu;

        if ((((const uint8_t *)&((nds_t *)ctx)->gpu.engine[0].dispcnt)[2] & 2) || end->force_line_finish != 0)
            gpu2d_line_finish(vid, line);

        for (uint32_t i = 0; i < DMA_N; i++) {
            uint8_t *c = ctrl + (size_t)i * DMA_STRIDE;
            if ((int32_t)rd32(c + 4) >= 0) continue;
            if (c[8] != 2) continue;
            uint8_t *s = (uint8_t *)&((nds_t *)ctx)->bus.dma[0].channels[i];
            if ((rd32(c) >> 23) == 0xc)

                gpu2d_line_finish(vid, rd16(ctx + LINE));
            dma_channel_run_transfer((dma_t *)(engine), (dma_channel_t *)(s));
        }

        if (end->force_vcount_192 != 0 && rd16(ctx + LINE) == LAST_LINE) {
            wr16(ia + 2, 0xc0);
            wr16(ib + 2, 0xc0);
        }
    }

    uint8_t *head = ctx + EVENT_QUEUE;
    uint8_t *self = ctx + EVENT;
    uint8_t *sig = (uint8_t *)rd_ptr(head);
    uint8_t *prev = 0;
    uint32_t rest = DELAY;
    uint32_t adjust;

    if (sig == 0) {
        adjust = 0;
    } else if (rd32(sig) > DELAY - 1u) {
        prev = 0;
        adjust = 1;
    } else {
        uint32_t d = rd32(sig);
        for (;;) {
            prev = sig;
            sig = (uint8_t *)rd_ptr(prev + 24);
            rest -= d;
            if (sig == 0) { adjust = 0; break; }
            d = rd32(sig);
            if (rest <= d) { adjust = 1; break; }
        }
    }

    uint8_t *hook = (prev != 0) ? (prev + 24) : head;
    wr32(ctx + 72, rest);
    wr_ptr(ctx + 96, sig);
    wr_ptr(ctx + 104, prev);
    wr_ptr(hook, self);

    if (adjust) {
        uint32_t d = rd32(sig);
        wr_ptr(sig + 32, self);
        wr32(sig, d - rest);
    }
}
#undef DMA_N
#undef DMA_STRIDE
#undef LINE
#undef LAST_LINE
#undef EVENT_QUEUE
#undef EVENT
#undef DELAY

#include "present_hook.h"
#define N2        0xfba68
#define IO9_DISPCAPCNT    0x1b0d4
#define A_SIGN   0x1b374
typedef void    *(*fn_mem)(uint64_t, uint64_t);
extern void    *video_out_screen_buffer(unsigned int screen);
extern int      video_out_screen_line_size(unsigned int screen);
extern void     spu_channel_set_mode_and_resize(uint32_t slot, uint32_t mode);
extern void     gpu2d_obj_scan_oam(gpu2d_engine_t *R);


void gpu_frame_open(uint8_t *ctx) {

    uint8_t *nA = (uint8_t *)rd_ptr(ctx);
    uint8_t *n2 = (uint8_t *)rd_ptr(nA + N2);
    int32_t  sign = (int16_t)rd16(nA + A_SIGN);
    uint32_t ctrl  = rd32(nA + IO9_DISPCAPCNT);
    uint8_t *mark_p = &((nds_t *)n2)->runtime.skip_frame;
    uint32_t mark = mark_p[0];
    gpu_t *gpu = (gpu_t *)ctx;
    uint8_t *fa = &gpu->engine[0].no_framebuffer;

    bus_access_tables_init((bus_t *)nA);

    uint32_t sel = (sign < 0) ? 0u : (((nds_t *)n2)->config.fix_main_screen == 0 ? 1u : 0u);
    uint32_t other = sel ^ 1u;

    spu_channel_set_mode_and_resize(sel,  ((nds_t *)n2)->config.hires_3d & 1u);
    spu_channel_set_mode_and_resize(other, ((nds_t *)n2)->config.hires_3d & 1u);

    void    *p1 = video_out_screen_buffer(sel);
    void    *p2 = video_out_screen_buffer(other);
    uint32_t i1 = (uint32_t)video_out_screen_line_size(sel);
    uint32_t i2 = (uint32_t)video_out_screen_line_size(other);

    uint32_t force = ((nds_t *)n2)->config.frameskip_safe;
    uint32_t band   = ((nds_t *)n2)->benchmark.pass_flags;

    fa[1] = fa[0];
    gpu->engine[1].unmapped_2 = gpu->engine[1].no_framebuffer;

    uint32_t skip = mark;
    if (recon_scale_out > 2u && (int32_t)ctrl < 0) skip = 0u;

    { extern int nds_capture_requires_frames(void); if (nds_capture_requires_frames()) skip = 0u; }
    uint32_t wants = ((force != 0) && ((int32_t)ctrl < 0)) ? 0u : skip;
    wants |= (band & 4u);

    gpu_output_t *output = GPU_OUTPUT_OF(gpu);
    gpu_capture_t *cap = &output->capture;
    void    *save1, *save2;
    uint32_t flagb;

    if (wants == 0) {
        gpu2d_obj_scan_oam(&gpu->engine[0]);
        gpu2d_engine_reset_bg_cache_ptrs(&gpu->engine[0]);
        fa[0] = 0;
        save1 = p1;
        save2 = p2;
        if (skip != 0) { save2 = 0; flagb = 1; }
        else if ((uint8_t)((nds_t *)n2)->benchmark.pass_flags & 4) { save2 = 0; flagb = 1; }
        else {
            gpu2d_obj_scan_oam(&gpu->engine[1]);
            gpu2d_engine_reset_bg_cache_ptrs(&gpu->engine[1]);
            flagb = 0;
        }
    } else {
        save1 = 0;
        save2 = p2;
        fa[0] = 1;
        if (skip != 0) { save2 = 0; flagb = 1; }
        else if ((uint8_t)((nds_t *)n2)->benchmark.pass_flags & 4) { save2 = 0; flagb = 1; }
        else {
            gpu2d_obj_scan_oam(&gpu->engine[1]);
            gpu2d_engine_reset_bg_cache_ptrs(&gpu->engine[1]);
            flagb = 0;
        }
    }

    gpu->engine[1].no_framebuffer = (uint8_t)flagb;
    gpu->engine[0].framebuffer = save1;
    gpu->engine[1].framebuffer = save2;
    gpu->engine[0].framebuffer_line_size = i1;
    gpu->engine[1].framebuffer_line_size = i2;
    gpu->engine[0].deferred_read = 0;
    gpu->engine[0].deferred_count = 0;
    gpu->engine[1].deferred_read = 0;
    gpu->engine[1].deferred_count = 0;
    cap->lines_done = 0;
    cap->source_a_mode = 0;
    cap->source_b_mode = 0;
    cap->active = 0;

    if ((int32_t)ctrl >= 0) return;

    uint64_t idx = (ctrl >> 16) & 3u;
    cap->active = 1;

    if (gpu->vram.bank[idx].map_kind != 6) return;
    if (gpu->vram.bank[idx].first_slot < 0x200u) return;

    cap->bank_bits = cap->bank_bits | (0xffu << ((idx * 8) & 31u));
    cap->source = gpu->vram.bank_data[idx];
    cap->read_offset = (ctrl >> 4) & 0xc000u;

    uint32_t class = (ctrl >> 29) & 3u;
    if (class != 1) {
        cap->source_a_mode = (uint8_t)((ctrl & 0x1000000u) ? 2u : 1u);
        if (class == 0) goto follows;
    }
    cap->source_b_mode = (uint8_t)((ctrl & 0x2000000u) ? 4u : 3u);

follows:
    cap->blend = 0;
    if (class >= 2) {
        uint32_t a = ctrl & 0x1fu;
        uint32_t b = (ctrl >> 8) & 0x1fu;
        cap->blend = 1;
        cap->eva = (uint8_t)a;
        cap->evb = (uint8_t)b;
        if (a > 0x10u) cap->eva = 0x10;
        if (b > 0x10u) cap->evb = 0x10;
    }

    cap->bank = (uint8_t)idx;
    uint64_t k = (ctrl >> 20) & 3u;
    cap->width = (uint16_t)gpu_capture_widths[k];
    cap->display_mode = (uint8_t)((gpu->engine[0].dispcnt >> 18) & 3u);
    cap->height = (uint8_t)gpu_capture_heights[k];

    if (!(gpu->vram.config->hires_3d & 1)) return;

    void *buf = output->capture_shadow[idx];
    if (!buf) {

        recon_native_read();

        size_t size = (size_t)0x20000u * (size_t)recon_shadow_stride_max();
        buf = ((fn_mem)sym_libc_memalign)(0x10, size);
        output->capture_shadow[idx] = buf;
    }
    cap->shadow = buf;
}
#undef N2
#undef IO9_DISPCAPCNT
#undef A_SIGN

#include "present_hook.h"
#include "core_internals.h"
extern void gpu2d_line_finish(uint8_t *ctx, uint32_t line);
extern void pagetable_clip_write(unsigned char *machine, uint32_t dir, uint32_t len);
extern void bus_region_init_mirror_pair_and_flush(uint8_t *ctx);
extern void time_now_microseconds(uint64_t *dest);
extern int  gpu2d_worker_signal_request_bit(uint32_t bit);
extern int  platform_doublebuffer_flip_signal(void);
#define FRAME_MARK_A   0x84350
#define FRAME_MARK_B   0x2f30
#define LAST_LINE    0xbf
#define N2        0xfba68
#define N2_B      0xfba88
#define IO9_DISPCAPCNT  0x1b0d4
#define A_SIGN   0x1b374

void gpu_frame_close(uint8_t *ctx) {

    if (recon_present_mode & RECON_PRESENT_EMU_PRIORITY) {
        static int set;
        if (!set) {
            typedef int (*fn_setprio)(int, unsigned, int); typedef int (*fn_gettid)(void);
            fn_setprio sp = (fn_setprio)sym_libc_setpriority; fn_gettid gt = (fn_gettid)sym_libc_gettid;
            if (sp && gt) sp(0 , (unsigned)gt(), recon_present_nice_emu);
            set = 1;
        }
    }

    uint8_t *nA = (uint8_t *)rd_ptr(ctx);
    uint8_t *n2 = (uint8_t *)rd_ptr(nA + N2);
    gpu_t *gpu = (gpu_t *)ctx;
    gpu_output_t *output = GPU_OUTPUT_OF(gpu);
    gpu_capture_t *vid = &output->capture;
    uint8_t *flags = (uint8_t *)&((nds_t *)n2)->benchmark.pass_flags;
    nds_config_t *active = &((nds_t *)n2)->config;

    gpu2d_line_finish(ctx, LAST_LINE);

    if (vid->active != 0) {
        vid->active = 0;
        uint8_t *a = (uint8_t *)rd_ptr(ctx);
        wr32(a + IO9_DISPCAPCNT, rd32(a + IO9_DISPCAPCNT) & 0x7fffffffu);

        if (gpu->vram.config->hires_3d & 1) {
            uint32_t width = vid->width;
            uint32_t height  = vid->height;
            uint32_t base  = vid->read_offset;
            uint32_t idx   = vid->bank;

            uint32_t r = (base >> 13) & 0x3ffffu;
            uint32_t n = (height * width) >> 13;

            uint32_t m = ~(0xffffffffu << (n & 31u));
            uint32_t rot = (m << ((base >> 13) & 31u))
                         | (m >> ((8u - r) & 31u));

            uint32_t v = output->bank_texture_bits[idx];
            if (v == 0) {
                if (gpu->vram.bank[idx].map_kind == 6) {
                    uint8_t *a2 = (uint8_t *)rd_ptr(ctx);
                    uint32_t p1 = gpu->vram.bank[idx].first_slot, p2 = gpu->vram.bank[idx].mapped_kb;

                    pagetable_clip_write(rd_ptr(a2 + N2_B),
                                       0x6000000u + (p1 << 14), p2 << 10);
                    idx = vid->bank;
                    v = output->bank_texture_bits[idx];
                } else {
                    v = 0;
                }
            }
            output->bank_texture_bits[idx] = (uint8_t)(v | rot);
        }
    }

    bus_region_init_mirror_pair_and_flush(rd_ptr(ctx));

    uint8_t *end_a = ctx + FRAME_MARK_A;
    uint8_t *end_b = ctx + FRAME_MARK_B;

    if (!(flags[0] & 0x20)) {
        if (active->show_fps != 0) {
            uint8_t *ring = (uint8_t *)gpu_hud_ring;
            uint8_t *count = (uint8_t *)&gpu_hud_ring_index;
            uint8_t *total  = (uint8_t *)&gpu_hud_frames_total;
            uint8_t *lost   = (uint8_t *)&gpu_hud_frames_lost;
            uint8_t *t1 = (uint8_t *)&((nds_t *)n2)->arm9.debug.instruction_count;
            uint8_t *t2 = (uint8_t *)&((nds_t *)n2)->arm7.debug.instruction_count;

            uint32_t g = rd32(count) - 1u;
            wr32(count, g);

            time_now_microseconds((uint64_t *)(ring + (size_t)g * 8));

            g = rd32(count);
            uint32_t c = rd32(total);
            uint32_t other = (g + 19u) % GPU_HUD_RING_SLOTS;
            uint64_t now = rd64(ring + (size_t)g * 8);
            uint64_t before = rd64(ring + (size_t)other * 8);
            uint32_t updated = c + 1u;
            wr32(total, updated);

            uint32_t delta = (uint32_t)now - (uint32_t)before;

            uint32_t p;
            if (end_b[0] != 0 && end_a[0] != 0) {
                p = rd32(lost);
            } else {
                p = rd32(lost) + 1u;
                wr32(lost, p);
            }

            double fps = gpu_hud_fps_numerator / (double)delta;
            double pct = ((double)p * 100.0) / (double)updated;

            float ffps = (float)fps, fpct = (float)pct;
            vid->hud_fps = ffps;
            vid->hud_percent = fpct;

            double ms = gpu_hud_frame_period_us;
            uint64_t a1 = rd64(t1), a2 = rd64(t2);
            uint8_t *prev = (uint8_t *)gpu_hud_prev_instructions;
            uint64_t p1 = rd64(prev), p2 = rd64(prev + 8);
            gpu_hud_stats_format(0, 0, 0, (double)ffps, (double)fpct,
                                 (double)(a1 - p1) / ms,
                                 (double)(a2 - p2) / ms);

            wr64(prev,     rd64(t1));
            wr64(prev + 8, rd64(t2));

            if (rd32(count) == 0) {
                wr32(lost, 0);
                wr32(total, 0);
                wr32(count, GPU_HUD_RING_SLOTS);
            }
        }

        if (!(end_b[0] != 0 && end_a[0] != 0)) {
            uint8_t *a = (uint8_t *)rd_ptr(ctx);
            uint32_t state;
            if ((int16_t)rd16(a + A_SIGN) < 0) {
                state = 0;
            } else {
                uint8_t *nn = (uint8_t *)rd_ptr(a + N2);
                state = (((nds_t *)nn)->config.fix_main_screen == 0) ? 1u : 0u;
            }

            if (end_b[0] == 0) gpu2d_worker_signal_request_bit(state);
            if (end_a[0] == 0) gpu2d_worker_signal_request_bit(state ^ 1u);
            platform_doublebuffer_flip_signal();
        }
    }

    if (active->threaded_3d != 0 && !(flags[0] & 8))

        gpu3d_raster_frame_thread_publish((gpu_t *)ctx);

    gpu3d_gxfifo_frame_end(GPU3D_OF(ctx));
    recon_scale_frame_close(ctx);
}
#undef FRAME_MARK_A
#undef FRAME_MARK_B
#undef LAST_LINE
#undef N2
#undef N2_B
#undef IO9_DISPCAPCNT
#undef A_SIGN

void gpu_hud_stats_format(long x0_unused, long x1_unused, long x2_unused, ...) {
    (void)x0_unused; (void)x1_unused; (void)x2_unused;

    char *dest = gpu_hud_text;
    const char *format = "%05.1lf%% %05.1lf%% %.2lfm/%.2lfm";

    va_list ap;
    va_start(ap, x2_unused);
    fortify_vsprintf(dest, sizeof gpu_hud_text, format, ap);
    va_end(ap);
}

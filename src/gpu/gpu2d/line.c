#include "core_internals.h"
#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "dma.h"
#include "present_hook.h"
#include "gpu/gpu3d/raster/raster.h"
#include "seedlessds/platform.h"
#include "mem_access.h"

#define BANK_STEP   0x80000
#define BLOCK_STEP  0x4000
#define TAG_STEP    0x100
#define LAST       0xbf
typedef int  (*fn_mtx)(void *);
typedef int  (*fn_wait)(void *, void *);
extern void *gpu2d_line_process_range_0(unsigned char *obj, uint32_t start, uint32_t end, void *arg) __asm__("gpu2d_line_process_range");






void gpu2d_line_finish(uint8_t *ctx, uint32_t line) {
    gpu3d_raster_frame_finish_pending();

    gpu_output_t *output = GPU_OUTPUT_OF(ctx);
    vram_map_t *vram = (vram_map_t *)ctx;
    uint32_t banks = vram->slot_touched_groups;
    if (banks != 0) {
        bus_t *core = vram->bus;
        uint8_t **label  = vram->slot_base;
        uint64_t off_b  = 0;
        uint64_t i      = 0;

        for (;;) {
            if (banks & 1) {
                uint32_t sub = vram->slot_touched[i];
                if (sub != 0) {
                    uint8_t **t = label;
                    uint64_t off = off_b;
                    for (;;) {
                        if (sub & 1) {
                            uint32_t tag  = (uint32_t)(uintptr_t)*t;

                            uint32_t base = (uint32_t)(uintptr_t)core->vram_region;
                            uint8_t *window = core->vram_window;

                            mirror_alias_blocks_16k(core, window + off, BLOCK_STEP,
                                               (uint32_t)off + (tag - base));
                        }
                        t += 1;
                        sub >>= 1;
                        off += BLOCK_STEP;
                        if (sub == 0) break;
                    }
                }
                vram->slot_touched[i] = 0;
            }
            i++;
            banks >>= 1;
            off_b += BANK_STEP;
            label  += TAG_STEP / 8;
            if (banks == 0) break;
        }
        vram->slot_touched_groups = 0;
    }

    uint32_t count = output->capture.lines_done;

    if (line == LAST && count == 0) {
        extern int recon_compose_pair_shared(gpu_t *, void *);
        if (recon_compose_pair_shared((gpu_t *)ctx, output->capture_shadow)) {
            output->capture.lines_done = (uint16_t)(line + 1);
            return;
        }

        fn_mtx    lock = nds_platform_default()->threads.mutex_lock;
        fn_mtx    unlock = nds_platform_default()->threads.mutex_unlock;
        fn_mtx    notify  = nds_platform_default()->threads.cond_signal;
        fn_wait wait = nds_platform_default()->threads.cond_wait;

        void *m1 = output->worker_mutex;
        lock(m1);
        output->worker_start = 1;
        notify(output->worker_cond);
        unlock(m1);


        { extern int recon_compose_a_shared(gpu2d_engine_t *, uint32_t, uint32_t, void *);
          if (!recon_compose_a_shared(&((gpu_t *)ctx)->engine[0], output->capture.lines_done, LAST, output->capture_shadow))
              gpu2d_line_process_range(&((gpu_t *)ctx)->engine[0], output->capture.lines_done, LAST, output->capture_shadow); }

        void *m2 = output->worker_mutex_done;
        lock(m2);
        while (output->worker_done == 0)
            wait(output->worker_cond_done, m2);
        output->worker_done = 0;
        unlock(m2);
    } else {

        { extern int recon_compose_a_shared(gpu2d_engine_t *, uint32_t, uint32_t, void *);
          extern int recon_compose_b_shared(gpu2d_engine_t *, uint32_t, uint32_t, void *);
          if (!recon_compose_a_shared(&((gpu_t *)ctx)->engine[0], count, line, output->capture_shadow))
              gpu2d_line_process_range(&((gpu_t *)ctx)->engine[0], count, line, output->capture_shadow);

          uint32_t count2 = output->capture.lines_done;
          if (!recon_compose_b_shared(&((gpu_t *)ctx)->engine[1], count2, line, 0))
              gpu2d_line_process_range(&((gpu_t *)ctx)->engine[1], count2, line, 0); }
    }

    output->capture.lines_done = (uint16_t)(line + 1);
}
#undef BANK_STEP
#undef BLOCK_STEP
#undef TAG_STEP
#undef LAST

#ifdef __ARM_NEON
#include <arm_neon.h>
#include "core_internals.h"
#endif
#define E_MODE     1184
#define E_CONV     1188
#define E_GHOST     1200
#define CAP_STEP   40
#define FIXED_PAL   0x1b0d8
#define PLANE      0x100
#define GROUP      0x300
#define WIDTH      256
extern unsigned int video_out_pixel_size(void);
extern uint32_t gpu2d_compose_render_line(void *ctx, uint8_t *dst, uint32_t line,
                                   gpu_output_t *cap, uint32_t doubled);
extern void *gpu3d_raster_scanline_lookup_filtered(gpu_t *param_1, int param_2);
extern void gpu2d_compose_split_rgb_planes(uint8_t *dest, const uint16_t *origin);
extern void gpu2d_compose_extract_alpha_plane_bf00(const unsigned char *pl, unsigned char *dst);
extern void gpu2d_compose_extract_alpha_plane_bf60(const unsigned char *a, const unsigned char *b,
                                    unsigned char *dst);
extern void gpu2d_compose_extract_alpha_plane_bfdc(const unsigned char *pl, unsigned char *dst);
extern void gpu2d_compose_extract_alpha_plane_c074(const unsigned char *a, const unsigned char *b,
                                    unsigned char *dst);
extern void gpu2d_compose_extract_alpha_plane_c110(const unsigned char *pl, unsigned char *dst,
                                    uint32_t mult, uint32_t sum);
extern void gpu2d_compose_extract_alpha_plane_c200(const unsigned char *a, const unsigned char *b,
                                    unsigned char *dst, uint32_t mult, uint32_t sum);
typedef void    *(*fn_memset)(void *, int, uint64_t);
typedef void    *(*fn_chk)(void *, const void *, uint64_t, uint64_t);







static uint32_t channel(uint32_t s) {
    return ((s >> 14) & 0xfcu) | (((s >> 8) & 0x3fu) << 10) | ((s & 0x3fu) << 18);
}

static uint16_t blend(const uint8_t *pl, uint32_t i, uint32_t mult, uint32_t sum) {
    uint32_t r = mult * pl[i] + sum;
    uint32_t g = mult * pl[PLANE + i] + sum;
    uint32_t b = mult * pl[2 * PLANE + i] + sum;
    return (uint16_t)(((r << 5) & 0xf800u) | (g & 0xffe0u) | (b >> 6));
}

static void fade(uint8_t *dst, const uint8_t *a, const uint8_t *b,
                    uint32_t mult, uint32_t sum) {
    for (uint32_t i = 0; i < WIDTH; i++) {
        if (b) {
            wr16(dst + (size_t)i * 4,     blend(a, i, mult, sum));
            wr16(dst + (size_t)i * 4 + 2, blend(b, i, mult, sum));
        } else {
            wr16(dst + (size_t)i * 2, blend(a, i, mult, sum));
        }
    }
}
#define MAX_SCALE    8u
#define MAX_GROUPS (MAX_SCALE * MAX_SCALE)

static void a565_local(uint8_t r, uint8_t v, uint8_t z, uint8_t *low, uint8_t *height) {
    *height = (uint8_t)(((uint8_t)(r << 2) & 0xe0u) | (uint8_t)(v >> 3));
    *low = (uint8_t)(((uint8_t)(v << 5) & 0xe0u) | ((uint8_t)(z >> 1) & 0x1fu));
}

#ifdef __ARM_NEON
static __attribute__((noinline)) int expand_native_row_rgba4(uint8_t *dst,
        const uint8_t *source, uint32_t merge, uint32_t mult, uint32_t sum)
{
    uintptr_t destination = (uintptr_t)dst;
    uintptr_t origin = (uintptr_t)source;
    if (destination >= origin ? destination - origin < 3u * PLANE
                              : origin - destination < WIDTH * 16u)
        return 0;

    uint32_t *output = (uint32_t *)dst;
    uint16x8_t bias = vdupq_n_u16((uint16_t)sum);
    for (unsigned i = 0; i < WIDTH; i += 16u) {
        uint8x16_t red = vshlq_n_u8(vld1q_u8(source + i), 2);
        uint8x16_t green = vshlq_n_u8(vld1q_u8(source + PLANE + i), 2);
        uint8x16_t blue = vshlq_n_u8(vld1q_u8(source + 2u * PLANE + i), 2);
        if (merge) {
            uint16x8_t red_lo = vaddq_u16(bias, vmulq_n_u16(vmovl_u8(vget_low_u8(red)), (uint8_t)mult));
            uint16x8_t red_hi = vaddq_u16(bias, vmulq_n_u16(vmovl_high_u8(red), (uint8_t)mult));
            red = vcombine_u8(vshrn_n_u16(red_lo, 5), vshrn_n_u16(red_hi, 5));
            uint16x8_t green_lo = vaddq_u16(bias, vmulq_n_u16(vmovl_u8(vget_low_u8(green)), (uint8_t)mult));
            uint16x8_t green_hi = vaddq_u16(bias, vmulq_n_u16(vmovl_high_u8(green), (uint8_t)mult));
            green = vcombine_u8(vshrn_n_u16(green_lo, 5), vshrn_n_u16(green_hi, 5));
            uint16x8_t blue_lo = vaddq_u16(bias, vmulq_n_u16(vmovl_u8(vget_low_u8(blue)), (uint8_t)mult));
            uint16x8_t blue_hi = vaddq_u16(bias, vmulq_n_u16(vmovl_high_u8(blue), (uint8_t)mult));
            blue = vcombine_u8(vshrn_n_u16(blue_lo, 5), vshrn_n_u16(blue_hi, 5));
        }
        uint8x16_t rg_low = vzip1q_u8(red, green);
        uint8x16_t rg_high = vzip2q_u8(red, green);
        uint8x16_t ba_low = vzip1q_u8(blue, vdupq_n_u8(0xff));
        uint8x16_t ba_high = vzip2q_u8(blue, vdupq_n_u8(0xff));
        uint32x4_t pixels;
        uint32x4x4_t repeated;
        pixels = vreinterpretq_u32_u16(vzip1q_u16(vreinterpretq_u16_u8(rg_low), vreinterpretq_u16_u8(ba_low)));
        repeated.val[0] = pixels; repeated.val[1] = pixels;
        repeated.val[2] = pixels; repeated.val[3] = pixels;
        vst4q_u32(output + 0u, repeated);
        pixels = vreinterpretq_u32_u16(vzip2q_u16(vreinterpretq_u16_u8(rg_low), vreinterpretq_u16_u8(ba_low)));
        repeated.val[0] = pixels; repeated.val[1] = pixels;
        repeated.val[2] = pixels; repeated.val[3] = pixels;
        vst4q_u32(output + 16u, repeated);
        pixels = vreinterpretq_u32_u16(vzip1q_u16(vreinterpretq_u16_u8(rg_high), vreinterpretq_u16_u8(ba_high)));
        repeated.val[0] = pixels; repeated.val[1] = pixels;
        repeated.val[2] = pixels; repeated.val[3] = pixels;
        vst4q_u32(output + 32u, repeated);
        pixels = vreinterpretq_u32_u16(vzip2q_u16(vreinterpretq_u16_u8(rg_high), vreinterpretq_u16_u8(ba_high)));
        repeated.val[0] = pixels; repeated.val[1] = pixels;
        repeated.val[2] = pixels; repeated.val[3] = pixels;
        vst4q_u32(output + 48u, repeated);
        output += 64;
    }
    return 1;

}
#endif

static void row_n(uint8_t *dst, uint8_t *const *g, unsigned n, unsigned ef,
                   unsigned r, uint32_t bpp,
                   uint32_t merge, uint32_t mult, uint32_t sum) {
#ifdef __ARM_NEON
    if (bpp == 4u && n == 4u && ef <= 1u &&
        expand_native_row_rgba4(dst, g[0], merge, mult, sum))
        return;
#endif
    uint32_t m = (uint8_t)mult;
    uint32_t sm = (uint16_t)sum;

    const unsigned f = n / (ef ? ef : 1u);
    const unsigned half = ef / 2u;

    const unsigned base_row = (ef > 1u) ? ((r / f) * ef) : 0u;

    if (f == 1u && ef > 1u) {

        if (ef & 1u) {
            static __thread uint8_t single_scale_buffer[WIDTH * 2u * 4u];

            uint8_t *single_scale_output = single_scale_buffer;
            __asm__ __volatile__("" : "+r"(single_scale_output));
            for (unsigned c = 0; c < ef; c++) {
                const uint8_t *pu = g[base_row + c];
                uint8_t *z = single_scale_output;
                if (merge) {
                    if (bpp == 2) fade(z, pu, pu, mult, sum);
                    else          gpu2d_compose_extract_alpha_plane_c200(pu, pu, z, mult, sum);
                } else {
                    if (bpp == 2) gpu2d_compose_extract_alpha_plane_c074(pu, pu, z);
                    else          gpu2d_compose_extract_alpha_plane_bf60(pu, pu, z);
                }
                if (bpp == 4) {
                    uint32_t *o = (uint32_t *)dst;
                    const uint32_t *zz = (const uint32_t *)z;
                    for (unsigned k = 0; k < WIDTH; k++)
                        o[(size_t)ef * k + c] = zz[2u * k];
                } else {
                    uint16_t *o = (uint16_t *)dst;
                    const uint16_t *zz = (const uint16_t *)z;
                    for (unsigned k = 0; k < WIDTH; k++)
                        o[(size_t)ef * k + c] = zz[2u * k];
                }
            }
            return;
        }

        static __thread uint8_t zipbuf[MAX_SCALE / 2u][WIDTH * 2u * 4u];
        const unsigned half = ef / 2u;
        uint8_t *zp[MAX_SCALE / 2u];
        {   uint8_t *zipbuf_p = zipbuf[0];
            __asm__ __volatile__("" : "+r"(zipbuf_p));
            for (unsigned c = 0; c < MAX_SCALE / 2u; c++) zp[c] = zipbuf_p + (size_t)c * (WIDTH * 2u * 4u); }
        for (unsigned c = 0; c < half; c++) {
            const uint8_t *pa = g[base_row + c];
            const uint8_t *pb = g[base_row + half + c];
            uint8_t *z = zp[c];
            if (merge) {
                if (bpp == 2) fade(z, pa, pb, mult, sum);
                else          gpu2d_compose_extract_alpha_plane_c200(pa, pb, z, mult, sum);
            } else {
                if (bpp == 2) gpu2d_compose_extract_alpha_plane_c074(pa, pb, z);
                else          gpu2d_compose_extract_alpha_plane_bf60(pa, pb, z);
            }
        }
        const unsigned nz = WIDTH * 2u;
        if (bpp == 4) {
            uint32_t *o = (uint32_t *)dst;
#ifdef __ARM_NEON
            if (half == 2u) {
                const uint32_t *z0 = (const uint32_t *)zp[0];
                const uint32_t *z1 = (const uint32_t *)zp[1];
                for (unsigned j = 0; j < nz; j += 4u) {
                    uint32x4x2_t v;
                    v.val[0] = vld1q_u32(z0 + j);
                    v.val[1] = vld1q_u32(z1 + j);
                    vst2q_u32(o + j * 2u, v);
                }
                return;
            }
            if (half == 4u) {
                const uint32_t *z0 = (const uint32_t *)zp[0];
                const uint32_t *z1 = (const uint32_t *)zp[1];
                const uint32_t *z2 = (const uint32_t *)zp[2];
                const uint32_t *z3 = (const uint32_t *)zp[3];
                for (unsigned j = 0; j < nz; j += 4u) {
                    uint32x4x4_t v;
                    v.val[0] = vld1q_u32(z0 + j);
                    v.val[1] = vld1q_u32(z1 + j);
                    v.val[2] = vld1q_u32(z2 + j);
                    v.val[3] = vld1q_u32(z3 + j);
                    vst4q_u32(o + j * 4u, v);
                }
                return;
            }
#endif
            for (unsigned j = 0; j < nz; j++)
                for (unsigned c = 0; c < half; c++)
                    o[j * half + c] = ((const uint32_t *)zp[c])[j];
        } else {
            uint16_t *o = (uint16_t *)dst;
#ifdef __ARM_NEON
            if (half == 2u) {
                const uint16_t *z0 = (const uint16_t *)zp[0];
                const uint16_t *z1 = (const uint16_t *)zp[1];
                for (unsigned j = 0; j < nz; j += 8u) {
                    uint16x8x2_t v;
                    v.val[0] = vld1q_u16(z0 + j);
                    v.val[1] = vld1q_u16(z1 + j);
                    vst2q_u16(o + j * 2u, v);
                }
                return;
            }
            if (half == 4u) {
                const uint16_t *z0 = (const uint16_t *)zp[0];
                const uint16_t *z1 = (const uint16_t *)zp[1];
                const uint16_t *z2 = (const uint16_t *)zp[2];
                const uint16_t *z3 = (const uint16_t *)zp[3];
                for (unsigned j = 0; j < nz; j += 8u) {
                    uint16x8x4_t v;
                    v.val[0] = vld1q_u16(z0 + j);
                    v.val[1] = vld1q_u16(z1 + j);
                    v.val[2] = vld1q_u16(z2 + j);
                    v.val[3] = vld1q_u16(z3 + j);
                    vst4q_u16(o + j * 4u, v);
                }
                return;
            }
#endif
            for (unsigned j = 0; j < nz; j++)
                for (unsigned c = 0; c < half; c++)
                    o[j * half + c] = ((const uint16_t *)zp[c])[j];
        }
        return;
    }

    unsigned jf = 0, rep = 0;
    for (unsigned j = 0; j < WIDTH * n; j++) {
        const uint8_t *p;
        if (ef <= 1u) {
            p = g[0] + jf;
        } else {
            unsigned kk = jf >> 1;
            p = g[base_row + (jf & 1u) * half + (kk >> 8)] + (kk & 255u);
        }
        if (++rep == f) { rep = 0; jf++; }
        uint32_t c0 = p[0], c1 = p[PLANE], c2 = p[2 * PLANE];
        if (bpp == 2) {
            uint16_t v;
            if (merge) {

                uint32_t rr = mult * c0 + sum;
                uint32_t gg = mult * c1 + sum;
                uint32_t bb = mult * c2 + sum;
                v = (uint16_t)(((rr << 5) & 0xf800u) | (gg & 0xffe0u) | (bb >> 6));
            } else {
                uint8_t height, low;
                a565_local((uint8_t)c0, (uint8_t)c1, (uint8_t)c2, &low, &height);
                v = (uint16_t)(low | ((uint32_t)height << 8));
            }
            wr16(dst + (size_t)j * 2, v);
        } else {
            uint8_t r, vv, a;
            if (merge) {
                uint32_t q0 = (uint8_t)(c0 << 2), q1 = (uint8_t)(c1 << 2), q2 = (uint8_t)(c2 << 2);
                r  = (uint8_t)((uint16_t)(sm + (uint16_t)(q0 * m)) >> 5);
                vv = (uint8_t)((uint16_t)(sm + (uint16_t)(q1 * m)) >> 5);
                a  = (uint8_t)((uint16_t)(sm + (uint16_t)(q2 * m)) >> 5);
            } else {
                r = (uint8_t)(c0 << 2); vv = (uint8_t)(c1 << 2); a = (uint8_t)(c2 << 2);
            }
            dst[(size_t)j * 4 + 0] = r;
            dst[(size_t)j * 4 + 1] = vv;
            dst[(size_t)j * 4 + 2] = a;
            dst[(size_t)j * 4 + 3] = 0xff;
        }
    }
}
static __thread uint8_t wide_scene[2u * MAX_GROUPS * GROUP + 16];
static __thread uint8_t wide_copy[WIDTH * MAX_SCALE * 4u];

static uint32_t half_of_row(const uint8_t *ctx, uint32_t bpp, unsigned wr)
{
    uint32_t width_l = ((const gpu2d_engine_t *)ctx)->framebuffer_line_size;

    uint32_t mid = (bpp == 2) ? ((width_l >> 2) * 2u) : ((width_l >> 3) * 4u);
    if (wr != 2u && wr != 0u) mid = (width_l / (wr * bpp)) * bpp;
    return mid;
}

#ifdef __ARM_NEON
static __attribute__((noinline)) int expand_native_rows_rgba4(uint8_t *dst,
        uint8_t *const *groups, uint32_t stride, uint32_t merge,
        uint32_t mult, uint32_t sum)
{
    if (stride < WIDTH * 16u) return 0;
    const uint8_t *source = groups[0];
    uintptr_t destination = (uintptr_t)dst;
    uintptr_t origin = (uintptr_t)source;
    uintptr_t metadata = (uintptr_t)groups;
    uint64_t output_size = (uint64_t)stride * 3u + WIDTH * 16u;
    if (destination >= origin ? destination - origin < 3u * PLANE
                              : (uint64_t)(origin - destination) < output_size)
        return 0;
    if (destination >= metadata ? destination - metadata < sizeof(groups[0])
                                : (uint64_t)(metadata - destination) < output_size)
        return 0;

    uint16x8_t bias = vdupq_n_u16((uint16_t)sum);
    for (unsigned i = 0; i < WIDTH; i += 16u) {
        uint8x16_t red = vshlq_n_u8(vld1q_u8(source + i), 2);
        uint8x16_t green = vshlq_n_u8(vld1q_u8(source + PLANE + i), 2);
        uint8x16_t blue = vshlq_n_u8(vld1q_u8(source + 2u * PLANE + i), 2);
        if (merge) {
            uint16x8_t red_lo = vaddq_u16(bias, vmulq_n_u16(vmovl_u8(vget_low_u8(red)), (uint8_t)mult));
            uint16x8_t red_hi = vaddq_u16(bias, vmulq_n_u16(vmovl_high_u8(red), (uint8_t)mult));
            red = vcombine_u8(vshrn_n_u16(red_lo, 5), vshrn_n_u16(red_hi, 5));
            uint16x8_t green_lo = vaddq_u16(bias, vmulq_n_u16(vmovl_u8(vget_low_u8(green)), (uint8_t)mult));
            uint16x8_t green_hi = vaddq_u16(bias, vmulq_n_u16(vmovl_high_u8(green), (uint8_t)mult));
            green = vcombine_u8(vshrn_n_u16(green_lo, 5), vshrn_n_u16(green_hi, 5));
            uint16x8_t blue_lo = vaddq_u16(bias, vmulq_n_u16(vmovl_u8(vget_low_u8(blue)), (uint8_t)mult));
            uint16x8_t blue_hi = vaddq_u16(bias, vmulq_n_u16(vmovl_high_u8(blue), (uint8_t)mult));
            blue = vcombine_u8(vshrn_n_u16(blue_lo, 5), vshrn_n_u16(blue_hi, 5));
        }
        uint8x16_t rg_low = vzip1q_u8(red, green);
        uint8x16_t rg_high = vzip2q_u8(red, green);
        uint8x16_t ba_low = vzip1q_u8(blue, vdupq_n_u8(0xff));
        uint8x16_t ba_high = vzip2q_u8(blue, vdupq_n_u8(0xff));
        uint32x4x4_t pixels;
        pixels.val[0] = vreinterpretq_u32_u16(vzip1q_u16(vreinterpretq_u16_u8(rg_low), vreinterpretq_u16_u8(ba_low)));
        pixels.val[1] = vreinterpretq_u32_u16(vzip2q_u16(vreinterpretq_u16_u8(rg_low), vreinterpretq_u16_u8(ba_low)));
        pixels.val[2] = vreinterpretq_u32_u16(vzip1q_u16(vreinterpretq_u16_u8(rg_high), vreinterpretq_u16_u8(ba_high)));
        pixels.val[3] = vreinterpretq_u32_u16(vzip2q_u16(vreinterpretq_u16_u8(rg_high), vreinterpretq_u16_u8(ba_high)));
        for (unsigned row = 0; row < 4u; row++) {
            uint32_t *output = (uint32_t *)(dst + (size_t)row * stride + (size_t)i * 16u);
            uint32x4x4_t repeated;
            repeated.val[0] = pixels.val[0]; repeated.val[1] = pixels.val[0];
            repeated.val[2] = pixels.val[0]; repeated.val[3] = pixels.val[0];
            vst4q_u32(output + 0u, repeated);
            repeated.val[0] = pixels.val[1]; repeated.val[1] = pixels.val[1];
            repeated.val[2] = pixels.val[1]; repeated.val[3] = pixels.val[1];
            vst4q_u32(output + 16u, repeated);
            repeated.val[0] = pixels.val[2]; repeated.val[1] = pixels.val[2];
            repeated.val[2] = pixels.val[2]; repeated.val[3] = pixels.val[2];
            vst4q_u32(output + 32u, repeated);
            repeated.val[0] = pixels.val[3]; repeated.val[1] = pixels.val[3];
            repeated.val[2] = pixels.val[3]; repeated.val[3] = pixels.val[3];
            vst4q_u32(output + 48u, repeated);
        }
    }
    return 1;
}
#endif

static void output_row(uint8_t *ctx, uint8_t *dst, uint8_t *pl, uint8_t *const *g,
                        unsigned wr, unsigned ef, uint32_t bpp, uint32_t doubled,
                        uint32_t mid)
{
    uint32_t br = ((const gpu2d_engine_t *)ctx)->master_bright;
    uint32_t mode = br >> 14;
    uint32_t f2 = (br & 0x1fu) << 1;


    uint32_t bytes = (doubled ? (WIDTH * wr) : WIDTH) * bpp;

    if (mode == 1 || mode == 2) {
        if (f2 >= 32u) {
            int v = (mode == 1) ? 0xff : 0x00;
            ((fn_memset)sym_libc_memset)(dst, v, bytes);
            if (doubled)
                for (unsigned r = 1; r < wr; r++)
                    ((fn_memset)sym_libc_memset)(dst + (size_t)r * mid, v, bytes);
        } else if (f2 == 0) {
            mode = 0;
        } else {
            uint32_t mult = 32u - f2;
            uint32_t sum = (mode == 1) ? (63u * f2 + 16u) : 16u;

            if (!doubled) {
                if (bpp == 2) fade(dst, pl, 0, mult, sum);
                else          gpu2d_compose_extract_alpha_plane_c110(pl, dst, mult, sum);
            } else if (wr == 2u) {

                if (bpp == 2) {
                    fade(dst, g[0], g[1], mult, sum);
                    fade(dst + mid, g[2], g[3], mult, sum);
                } else {
                    gpu2d_compose_extract_alpha_plane_c200(g[0], g[1], dst, mult, sum);
                    gpu2d_compose_extract_alpha_plane_c200(g[2], g[3], dst + mid, mult, sum);
                }
            } else {
#ifdef __ARM_NEON
                if (wr == 4u && ef <= 1u && bpp == 4u &&
                    expand_native_rows_rgba4(dst, g, mid, 1u, mult, sum))
                    return;
#endif
                for (unsigned r = 0; r < wr; r++)
                    row_n(dst + (size_t)r * mid, g, wr, ef, r, bpp, 1u, mult, sum);
            }
        }
    }
    if (mode == 0 || mode == 3) {
        if (!doubled) {
            if (bpp == 2) gpu2d_compose_extract_alpha_plane_bfdc(pl, dst);
            else          gpu2d_compose_extract_alpha_plane_bf00(pl, dst);
        } else if (wr == 2u) {

            if (bpp == 2) {
                gpu2d_compose_extract_alpha_plane_c074(g[0], g[1], dst);
                gpu2d_compose_extract_alpha_plane_c074(g[2], g[3], dst + mid);
            } else {
                gpu2d_compose_extract_alpha_plane_bf60(g[0], g[1], dst);
                gpu2d_compose_extract_alpha_plane_bf60(g[2], g[3], dst + mid);
            }
        } else {
#ifdef __ARM_NEON
            if (wr == 4u && ef <= 1u && bpp == 4u &&
                expand_native_rows_rgba4(dst, g, mid, 0u, 0u, 0u))
                return;
#endif
            for (unsigned r = 0; r < wr; r++)
                row_n(dst + (size_t)r * mid, g, wr, ef, r, bpp, 0u, 0u, 0u);
        }
    }
}

#ifdef __ARM_NEON
static inline uint32x4x2_t rgb555_pixels8(uint16x8_t input, uint16x8_t alpha)
{
    uint16x8_t red_green = vsriq_n_u16(vshlq_n_u16(input, 6), vshlq_n_u16(input, 11), 8);
    red_green = vbicq_u16(red_green, vdupq_n_u16(0x0700u));
    uint16x8_t blue = vandq_u16(vshrq_n_u16(input, 7), vdupq_n_u16(0x00f8u));
    uint16x8x2_t packed = vzipq_u16(red_green, vorrq_u16(blue, alpha));
    uint32x4x2_t result;
    result.val[0] = vreinterpretq_u32_u16(packed.val[0]);
    result.val[1] = vreinterpretq_u32_u16(packed.val[1]);
    return result;
}

static inline int vram_output_is_separate(const uint8_t *dst,
        const uint8_t *source, uint32_t stride)
{
    uintptr_t output = (uintptr_t)dst;
    uintptr_t input = (uintptr_t)source;
    uint64_t output_size = (uint64_t)stride * 3u + 4096u;
    return input >= output ? (uint64_t)(input - output) >= output_size
                           : (uint64_t)(output - input) >= 8192u;
}

static __attribute__((noinline)) void output_vram_row_rgba4(uint8_t *dst,
        const uint8_t *source, uint32_t stride)
{
    const uint16_t *input = (const uint16_t *)source;
    const uint16x8_t zero = vdupq_n_u16(0u);
    const uint16x8_t alpha = vcombine_u16(vdup_n_u16(0xff00u), vdup_n_u16(0u));
    for (unsigned row = 0; row < 4u; row++) {
        const uint16_t *s = input + row * 4u * WIDTH;
        uint32_t *out = (uint32_t *)(dst + (size_t)row * stride);
#pragma clang loop unroll_count(2)
        for (unsigned x = 0; x < WIDTH; x += 8u) {
            uint32x4x2_t p0 = rgb555_pixels8(vld1q_u16(s + x), alpha);
            uint32x4x2_t p1 = rgb555_pixels8(vld1q_u16(s + WIDTH + x), alpha);
            uint32x4x2_t p2 = rgb555_pixels8(vld1q_u16(s + 2u * WIDTH + x), zero);
            uint32x4x2_t p3 = rgb555_pixels8(vld1q_u16(s + 3u * WIDTH + x), zero);
            uint32x4x4_t first = {{p0.val[0], p1.val[0], p2.val[0], p3.val[0]}};
            uint32x4x4_t second = {{p0.val[1], p1.val[1], p2.val[1], p3.val[1]}};
            vst4q_u32(out + x * 4u, first);
            vst4q_u32(out + (x + 4u) * 4u, second);
        }
    }
}
#endif

void gpu2d_line_finish_work(uint8_t *shadow, recon_b2_job *t)
{
    memcpy(shadow, t->obj0, sizeof t->obj0);
    memcpy(&((gpu2d_engine_t *)shadow)->sprite_screen, t->obj21400, sizeof t->obj21400);
    wr_ptr(shadow + 24, &t->fill);
    gpu_output_t *cap = (gpu_output_t *)t->cap;
    uint8_t *pl = wide_scene; __asm__ __volatile__("" : "+r"(pl));
    pl += (16 - ((uintptr_t)pl & 15)) & 15;
    const unsigned NS = t->NS, wr = t->scale;
    const uint32_t bpp = t->bpp, doubled = t->doubled, disp = t->disp;
    uint8_t *g[MAX_GROUPS];
    for (unsigned i = 0; i < NS; i++) g[i] = pl;
    unsigned ef = 1u;
    uint32_t high = 0u;
    t->prep.buf = (uint8_t *)(((uintptr_t)t->buf + 15) & ~(uintptr_t)15);
    if (gpu2d_compose_render_line_body(shadow, pl, cap, &t->prep, &high) != 0) {
        for (unsigned i = 1; i < NS; i++) g[i] = pl + (size_t)i * GROUP;
        ef = high ? wr : 1u;
    }
    if (ef == 1u && wr > 2u && doubled && t->has_cap) {
        uint32_t k = (disp >> 18) & 3u;
        if (cap->bank_texture_bits[k] & (1u << ((t->line >> 5) & 31))) {
            uint8_t *v = cap->capture_shadow[k]
                       + (size_t)(t->line * NS) * 512u;
            if (v) {
                for (unsigned i = 1; i < NS; i++) {
                    g[i] = pl + (size_t)i * GROUP;
                    gpu2d_compose_split_rgb_planes(g[i], (const uint16_t *)(v + (size_t)i * 0x200));
                }
                ef = wr;
            }
        }
    }
    output_row(shadow, t->dst, pl, g, wr, ef, bpp, doubled, t->mid);
}

void gpu2d_line_render_full(uint8_t *ctx, uint8_t *dst, uint32_t line, gpu_output_t *cap) {

    uint32_t bpp = video_out_pixel_size();
    gpu_t *root = ((const gpu2d_engine_t *)ctx)->gpu;
    uint32_t disp = ((const gpu2d_engine_t *)ctx)->dispcnt;
    uint8_t *mem = (uint8_t *)root->vram.bus;
    uint8_t *state = (uint8_t *)root->vram.config;

    uint8_t raw[0x1820 + 16];
    uint8_t *pl = raw;
    pl += (16 - ((uintptr_t)pl & 15)) & 15;
    uint8_t copy[0x1000];
    uint8_t pal[0x200];

    if (((const gpu2d_engine_t *)ctx)->index == 0) {
        uint32_t conv = rd32(state + E_CONV);
        if (conv != 0) {

            const uint8_t *src = (const uint8_t *)gpu3d_raster_scanline_lookup_filtered(root, (int)line);

            uint32_t width = state[E_MODE] & 1u;
            recon_native_read();
            unsigned n = width ? ((recon_native) ? recon_scale_out : 2u) : 1u;
            uint8_t *o = dst;
            for (unsigned b = 0; b < n; b++) {
                const uint8_t *q = src + (size_t)b * (256u * n * 4u);
                for (uint32_t i = 0; i < 0x100; i++)
                    for (unsigned j = 0; j < n; j++) {
                        wr32(o, channel(rd32(q + ((size_t)i + (size_t)j * 256u) * 4)));
                        o += 4;
                    }
            }
            return;
        }
    }

    dma_t *arg = &((bus_t *)mem)->dma[0];
    uint32_t width2 = rd32(state + E_MODE) & 1u;
    uint32_t has_cap = (cap != 0) ? (cap->capture.source_a_mode != 0) : 0u;

    recon_native_read();
    unsigned wr = recon_scale_output();
    if (wr < 1u || wr > MAX_SCALE) wr = 2u;
    unsigned NS = width2 ? wr * wr : 1u;
    if (NS > 4u) {

        pl = wide_scene; __asm__ __volatile__("" : "+r"(pl));
        pl += (16 - ((uintptr_t)pl & 15)) & 15;
    }

    void *dev = 0;
    for (uint32_t k = 0; k < 4; k++) {
        dma_channel_t *u = &arg->channels[k];
        if ((int32_t)u->cnt >= 0) continue;
        if (u->start_mode != 4) continue;
        dev = dma_main_memory_display_step((uint8_t *)(arg), u,
                                   (line == 0xbf) ? 0xbfu : line);
        if (line == 0xbf) {
            uint32_t v = u->cnt & 0x7fffffffu;
            u->cnt = v;
            u->regs->cnt = v;
        }
    }

    if (line != 0) {
        gpu2d_engine_t *e = (gpu2d_engine_t *)ctx;
        e->bg[2].current_x += e->bg[2].pb;
        e->bg[2].current_y += e->bg[2].pd;
        e->bg[3].current_x += e->bg[3].pb;
        e->bg[3].current_y += e->bg[3].pd;
    } else {

        gpu2d_engine_t *e = (gpu2d_engine_t *)ctx;
        uint32_t a = e->win0v;
        if (a >= 0xc000u) e->window_flags |= 1u;
        if ((a & 0xc0u) == 0xc0u) e->window_flags &= (uint8_t)0xfe;
        uint32_t b = e->win1v;
        if (b >= 0xc000u) e->window_flags |= 2u;
        if ((b & 0xc0u) == 0xc0u) e->window_flags &= (uint8_t)0xfd;
        e->bg[2].current_x = e->bg[2].ref_x;
        e->bg[2].current_y = e->bg[2].ref_y;
        e->bg[3].current_x = e->bg[3].ref_x;
        e->bg[3].current_y = e->bg[3].ref_y;
        e->bg[2].affine_dirty = 1;
        e->bg[3].affine_dirty = 1;
    }

    if (has_cap && cap->capture.source_b_mode != 0) {
        if (cap->capture.source_b_mode == 3) {
            uint8_t *v = ((const gpu2d_engine_t *)ctx)->display_vram_bank + (size_t)(line << 8) * 2;
            cap->capture.source_b_line = v;
            uint8_t *b = (uint8_t *)rd64(ctx) + (size_t)cap->capture.display_mode * 16;
            cap->capture.source_b_line = (rd32(b + 16) == 6) ? v : 0;
        } else if (dev != 0) {
            cap->capture.source_b_line = (uint8_t *)dev;
        } else {
            uint8_t *b = rd_ptr(rd_ptr(ctx));
            uint16_t c = rd16(b + FIXED_PAL);
            for (uint32_t i = 0; i < 0x100; i++) wr16(pal + (size_t)i * 2, c);
            cap->capture.source_b_line = pal;
            dev = pal;
        }
        if (cap->capture.blend == 0 && cap->capture.height > line) {
            memcpy(cap->capture.write_line, cap->capture.source_b_line,
                   (size_t)cap->capture.width * 2);
        }
    }

    uint8_t *g[MAX_GROUPS];
    for (unsigned i = 0; i < NS; i++) g[i] = pl;

    unsigned ef = 1u;
    uint32_t doubled = width2;

    if (recon_b2_active(ctx) && (((disp >> 16) & 3u) != 1u || !doubled || rd32(state + E_GHOST) != 0))
        recon_b2_drain(ctx);
    switch ((disp >> 16) & 3u) {
    case 0:

        if (has_cap) gpu2d_compose_render_line(ctx, pl, line, cap, doubled);
        ((fn_memset)sym_libc_memset)(pl, 0xff, GROUP);
        break;

    case 1:

        if (doubled && recon_b2_active(ctx) && rd32(state + E_GHOST) == 0) {
            recon_b2_job *t = recon_b2_take(ctx);
            uint8_t *tb = (uint8_t *)(((uintptr_t)t->buf + 15) & ~(uintptr_t)15);
            gpu2d_compose_render_line_prologue(ctx, line, cap, doubled, tb, &t->prep);

            if (t->prep.three == raster_downsample_line) {
#ifdef RECON_DIAG
                nds_trace_2d_fixed();
#endif
                memcpy(t->three_copy, t->prep.three, sizeof t->three_copy);
                t->prep.three = t->three_copy;
            }
            t->dst = dst; t->line = line; t->doubled = doubled; t->has_cap = has_cap;
            t->bpp = bpp; t->scale = wr; t->NS = NS; t->disp = disp;
            t->mid = (bpp == 2) ? ((((const gpu2d_engine_t *)ctx)->framebuffer_line_size >> 2) * 2u) : ((((const gpu2d_engine_t *)ctx)->framebuffer_line_size >> 3) * 4u);
            if (wr != 2u && wr != 0u) t->mid = (((const gpu2d_engine_t *)ctx)->framebuffer_line_size / (wr * bpp)) * bpp;
            memcpy(t->obj0, ctx, sizeof t->obj0);
            memcpy(t->obj21400, &((gpu2d_engine_t *)ctx)->sprite_screen, sizeof t->obj21400);
            t->fill = rd16((const void *)rd64(ctx + 24));
            if (cap) {
                memcpy(t->cap, cap, sizeof t->cap);

                uint8_t *fb = cap->capture.source_b_line;
                if (fb) { memcpy(t->row_source, fb, (size_t)cap->capture.width * 2u > sizeof t->row_source ? sizeof t->row_source : (size_t)cap->capture.width * 2u);
                          ((gpu_output_t *)t->cap)->capture.source_b_line = t->row_source; }
            } else memset(t->cap, 0, sizeof t->cap);
            recon_b2_enqueue(t, ctx);
            return;
        }

        if (gpu2d_compose_render_line(ctx, pl, line, cap, doubled) != 0) {
            for (unsigned i = 1; i < NS; i++) g[i] = pl + (size_t)i * GROUP;

            ef = recon_sublines_hires ? wr : 1u;
        }

        if (ef == 1u && wr > 2u && doubled && has_cap) {
            uint32_t k = (disp >> 18) & 3u;
            if (cap->bank_texture_bits[k] & (1u << ((line >> 5) & 31))) {
                uint8_t *v = cap->capture_shadow[k]
                           + (size_t)(line * NS) * 512u;
                if (v) {
                    for (unsigned i = 1; i < NS; i++) {
                        g[i] = pl + (size_t)i * GROUP;

                        gpu2d_compose_split_rgb_planes(g[i], (const uint16_t *)(v + (size_t)i * 0x200));
                    }
                    ef = wr;
                }
            }
        }
        break;

    case 2: {

        uint32_t k = (disp >> 18) & 3u;
        int has_shadow = doubled && cap
            && (cap->bank_texture_bits[k] & (1u << ((line >> 5) & 31)));
        unsigned s0v = 0u;
        uint8_t *v = 0;
        if (has_shadow) {
            s0v = recon_s0_shadow();
            const unsigned shadow_stride = recon_shadow_stride();
            v = cap->capture_shadow[k]
              + (size_t)(line * shadow_stride) * 512u;

            ef = wr;
#ifdef __ARM_NEON
            uint32_t bright = ((const gpu2d_engine_t *)ctx)->master_bright;
            uint32_t bright_mode = bright >> 14;
            if (wr == 4u && bpp == 4u && s0v == 0u && shadow_stride == 16u
                && rd32(state + E_GHOST) == 0u
                && (bright_mode == 0u || bright_mode == 3u
                    || ((bright_mode == 1u || bright_mode == 2u) && (bright & 31u) == 0u))) {
                uint32_t stride = half_of_row(ctx, bpp, wr);
                if (vram_output_is_separate(dst, v, stride)) {
                    output_vram_row_rgba4(dst, v, stride);
                    if (has_cap)
                        gpu2d_compose_render_line(ctx, pl + (size_t)NS * GROUP, line, cap, doubled);
                    return;
                }
            }
#endif
        }

        gpu2d_compose_split_rgb_planes(pl, (const uint16_t *)(((const gpu2d_engine_t *)ctx)->display_vram_bank
                                   + (size_t)(line << 8) * 2));
        if (has_shadow) {
            for (unsigned i = s0v; i < NS; i++) {
                g[i] = pl + (size_t)i * GROUP;

                gpu2d_compose_split_rgb_planes(g[i], (const uint16_t *)(v + (size_t)(i - s0v) * 0x200));
            }
        }

        if (has_cap) gpu2d_compose_render_line(ctx, pl + (size_t)NS * GROUP, line, cap, doubled);
        break;
    }

    default: {

        if (has_cap) doubled = gpu2d_compose_render_line(ctx, pl, line, cap, doubled);
        uint8_t *source = (uint8_t *)dev;
        if (source != 0) {

            gpu2d_compose_split_rgb_planes(pl, (const uint16_t *)source);
        } else {
            uint8_t *b = rd_ptr(rd_ptr(ctx));
            uint32_t c = rd32(b + FIXED_PAL);
            uint8_t r = (uint8_t)((c << 1) & 0x3eu);
            uint8_t v = (uint8_t)((c >> 4) & 0x3eu);
            uint8_t a = (uint8_t)((c >> 9) & 0x3eu);
            memset(pl, r, PLANE);
            memset(pl + PLANE, v, PLANE);
            memset(pl + 2 * PLANE, a, PLANE);
        }
        break;
    }
    }

    uint32_t pix = doubled ? (WIDTH * wr) : WIDTH;
    int ghost = (rd32(state + E_GHOST) != 0);
    uint8_t *cop = copy;
    uint64_t cop_size = sizeof copy;
    if ((uint64_t)pix * bpp > cop_size) { cop = wide_copy; __asm__ __volatile__("" : "+r"(cop)); cop_size = sizeof wide_copy; }
    if (ghost)
        ((fn_chk)fortify_memcpy)(cop, dst, (uint64_t)pix * bpp, cop_size);

    output_row(ctx, dst, pl, g, wr, ef, bpp, doubled, half_of_row(ctx, bpp, wr));

    if (!ghost || ((const gpu2d_engine_t *)ctx)->unmapped_2 != 0 || pix == 0) return;
    if (bpp == 2) {
        for (uint32_t i = 0; i < pix; i++) {
            uint32_t a = rd16(dst + (size_t)i * 2);
            uint32_t b = rd16(cop + (size_t)i * 2);
            uint32_t r = ((a >> 11) + (b >> 11)) >> 1;
            uint32_t v = (((a >> 5) & 0x3fu) + ((b >> 5) & 0x3fu)) >> 1;
            uint32_t z = ((a & 0x1fu) + (b & 0x1fu)) >> 1;
            wr16(dst + (size_t)i * 2, (uint16_t)((r << 11) | (v << 5) | z));
        }
    } else {
        for (uint32_t i = 0; i < pix; i++) {
            uint32_t a = rd32(dst + (size_t)i * 4);
            uint32_t b = rd32(cop + (size_t)i * 4);
            wr32(dst + (size_t)i * 4, ((a >> 1) + (b >> 1)) & 0xfefefeffu);
        }
    }
}
#undef E_MODE
#undef E_CONV
#undef E_GHOST
#undef CAP_STEP
#undef FIXED_PAL
#undef PLANE
#undef GROUP
#undef WIDTH
#undef MAX_SCALE
#undef MAX_GROUPS

static int64_t sdiv64_arm(int64_t num, int64_t den)
{
    if (den == 0)  return 0;
    if (den == -1) return (int64_t)(0u - (uint64_t)num);
    return num / den;
}

static int32_t sdiv32_arm(int32_t num, int32_t den)
{
    if (den == 0)  return 0;
    if (den == -1) return (int32_t)(0u - (uint32_t)num);
    return num / den;
}

static int64_t div_fixed32_ceil(int32_t v, int32_t den)
{

    uint64_t num = (uint64_t)(uint32_t)v << 32;
    int64_t  d   = (int64_t)den;

    if (v < 0) {
        if (den < 0)
            num = num + (uint64_t)d + 1u;
    } else {
        if (den >= 0)
            num = num + (uint64_t)d - 1u;
    }
    return sdiv64_arm((int64_t)num, d);
}

static int32_t sub_wrap32(int32_t p, int32_t q)
{
    return (int32_t)((uint32_t)p - (uint32_t)q);
}

void gpu2d_line_compute_span_range3(int32_t x, int32_t a, int32_t b, int32_t c,
                        int64_t *out_a, int64_t *out_b, int64_t *out_c)
{

    if (a != 0) {
        int32_t v1, v2;

        if (a > 0) {
            v1 = sub_wrap32((int32_t)((uint32_t)a - 1u), x);
            v2 = sub_wrap32(b, x);
        } else {
            v1 = sub_wrap32(
                     (int32_t)((uint32_t)a + (uint32_t)b + 1u), x);
            v2 = sub_wrap32(0, x);
        }

        int64_t q1 = div_fixed32_ceil(v1, a);
        int64_t q2 = div_fixed32_ceil(v2, a);

        int64_t diff = (int64_t)((uint64_t)q2 - (uint64_t)q1);

        int64_t q3 = div_fixed32_ceil((int32_t)(0u - (uint32_t)c), a);

        *out_a = q1;
        *out_c = diff;
        *out_b = q3;
        return;
    }

    if (c != 0) {
        int32_t p, q;

        if (c > 0) {
            p = sub_wrap32(b, x);
            q = sub_wrap32(0, x);
        } else {
            p = sub_wrap32(0, x);
            q = sub_wrap32(b, x);
        }

        int32_t qA = sdiv32_arm(p, c);
        int32_t qB = sdiv32_arm(q, c);

        uint64_t height = (uint64_t)(uint32_t)qA << 39;

        uint32_t term = 0u - ((uint32_t)qB << 7);

        uint64_t r_a = 0u - height;

        uint64_t r_c = height + ((uint64_t)term << 32);

        uint64_t mid = (uint64_t)1 << 39;
        r_c = r_c + mid;

        *out_a = (int64_t)r_a;
        *out_c = (int64_t)r_c;
        *out_b = (int64_t)mid;
        return;
    }

    uint32_t outside = (uint32_t)(x < 0) | (uint32_t)(x > b);

    int64_t  r_a = (outside & 1u) ? -1 : 0;
    uint64_t r_c = (outside != 0) ? 0u : ((uint64_t)1 << 39);
    int64_t  r_b = 0;

    *out_a = r_a;
    *out_c = (int64_t)r_c;
    *out_b = r_b;
}

void *gpu2d_line_process_range(gpu2d_engine_t *obj, uint32_t start, uint32_t end, void *arg) {

    gpu2d_deferred_write_t *table = obj->deferred;

    uint32_t ia = obj->deferred_read, ib = obj->deferred_count;
    gpu2d_deferred_write_t *entry = table + ia;
    table[ib].line = 255;
    if (start <= end) {
        uint32_t limit = entry->line;
        uint32_t line = start;
        for (;;) {
            unsigned char *buf = obj->framebuffer;
            if (buf) {
                uint32_t step = obj->framebuffer_line_size;
                gpu2d_line_render_full(
                    (uint8_t *)obj, buf + (uint64_t)(step * line), line, arg);
            }
            while (line >= limit) {
                gpu2d_engine_write_register(obj, entry);
                limit = entry[1].line;
                entry += 1;
            }
            if (line >= end) break;
            line++;
        }
        ib = obj->deferred_count;
    }

    uint32_t updated = (uint32_t)(entry - table);
    obj->deferred_read = ib;
    obj->deferred_count = updated;
    return obj;
}

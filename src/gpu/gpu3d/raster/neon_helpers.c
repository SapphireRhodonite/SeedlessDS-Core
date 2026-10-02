#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "../../gpu.h"
#include <math.h>
#include <stddef.h>
#include "core_internals.h"

void gpu3d_raster_fog_density_line(const unsigned char *src, unsigned char *dst, const unsigned char *tables, uint32_t pair);
void gpu3d_raster_words_copy_mask29(uint32_t *dest, const uint32_t *origin);

void gpu3d_raster_curve_table_convert(const unsigned char *entry, unsigned char *output,
                        const unsigned char *table, uint32_t param) {
    const unsigned char *pend = table + 0x20;
    uint32_t shift = param & 0xffffu;
    uint32_t bias = param >> 16;
    int32_t cap = 0x7fff;
    for (uint64_t i = 0; i < 0x100; i++) {
        uint32_t v;
        memcpy(&v, entry + i * 4, 4);
        int32_t t = (int32_t)((v >> 9) & 0x7fffu);
        t = t - (int32_t)bias;
        t = (int32_t)((uint32_t)t & ~(uint32_t)(t >> 31));
        t = (int32_t)((uint32_t)t << (shift & 31u));
        if (!(t < cap)) t = cap;
        int64_t idx = (int64_t)(t >> 10);
        int32_t p = (int8_t)pend[idx];
        uint32_t base = table[idx];
        uint32_t frac = (uint32_t)t & 0x3ffu;
        uint32_t prod = (uint32_t)p * frac;
        output[i] = (unsigned char)(base + (prod >> 10));
    }
}

static int recon_overlap(uintptr_t dest, uintptr_t source,
                         uintptr_t dest_size, uintptr_t source_size)
{
    return source + source_size > dest && dest + dest_size > source;
}

static uint32_t recon_blend_pixel(uint32_t pixel, uint8_t coverage,
                                   uint32_t red_target,
                                   uint32_t green_target,
                                   uint32_t blue_target,
                                   uint32_t alpha_target)
{
    uint32_t red = pixel & 0x3fu;
    uint32_t green = (pixel >> 8) & 0x3fu;
    uint32_t blue = (pixel >> 16) & 0x3fu;
    uint32_t alpha = (pixel >> 24) & 0x7fu;
    uint32_t factor = coverage == 0x7fu ? 0x80u : (uint32_t)coverage;

    if ((int32_t)pixel >= 0)
        factor = 0;

    red += (factor * (red_target - red)) >> 7;
    green += (factor * (green_target - green)) >> 7;
    blue += (factor * (blue_target - blue)) >> 7;
    alpha += (factor * (alpha_target - alpha)) >> 7;

    return red | (green << 8) | (blue << 16) | (alpha << 24);
}

void gpu3d_raster_fog_blend_color_words(void *param_1, const void *param_2,
                         const uint8_t *param_3, uint32_t param_4)
{

    uint8_t *dest = (uint8_t *)param_1;
    const uint8_t *source = (const uint8_t *)param_2;
    uint32_t red_target = param_4 & 0x3fu;
    uint32_t green_target = (param_4 >> 8) & 0x3fu;
    uint32_t blue_target = (param_4 >> 16) & 0x3fu;
    uint32_t alpha_target = (param_4 >> 24) & 0x1fu;
    int overlap_source = recon_overlap((uintptr_t)dest, (uintptr_t)source,
                                      0x400u, 0x400u);
    int overlaps_coverage = recon_overlap((uintptr_t)dest, (uintptr_t)param_3,
                                         0x400u, 0x100u);

    if (!overlap_source && !overlaps_coverage) {
        for (uint32_t idx = 0; idx != 0x100u; idx += 4) {
            uint8_t coverage0 = param_3[idx];
            uint8_t coverage1 = param_3[idx + 1];
            uint8_t coverage2 = param_3[idx + 2];
            uint32_t entry[4];
            uint32_t output[4];
            uint8_t coverage3;

            memcpy(entry, source + idx * 4u, sizeof(entry));
            coverage3 = param_3[idx + 3];

            output[0] = recon_blend_pixel(entry[0], coverage0,
                                            red_target, green_target,
                                            blue_target, alpha_target);
            output[1] = recon_blend_pixel(entry[1], coverage1,
                                            red_target, green_target,
                                            blue_target, alpha_target);
            output[2] = recon_blend_pixel(entry[2], coverage2,
                                            red_target, green_target,
                                            blue_target, alpha_target);
            output[3] = recon_blend_pixel(entry[3], coverage3,
                                            red_target, green_target,
                                            blue_target, alpha_target);
            memcpy(dest + idx * 4u, output, sizeof(output));
        }
    } else {
        for (uint32_t idx = 0; idx != 0x100u; ++idx) {
            uint8_t coverage = param_3[idx];
            uint32_t pixel;
            uint32_t result;

            memcpy(&pixel, source + idx * 4u, sizeof(pixel));
            result = recon_blend_pixel(pixel, coverage, red_target,
                                           green_target, blue_target,
                                           alpha_target);
            memcpy(dest + idx * 4u, &result, sizeof(result));
        }
    }
}

void gpu3d_raster_fog_apply_frame(unsigned char *machine, unsigned char *frame)
{

    gpu3d_band_header_t *hdr = (gpu3d_band_header_t *)(machine + GPU3D_BAND_HEADER_OFFSET);
    if (hdr->band_dirty == 0) {
        gpu3d_raster_words_copy_mask29((uint32_t *)frame, (const uint32_t *)machine);
        return;
    }

    gpu3d_raster_t *state = &hdr->gpu->raster;
    if (state->frame_ready == 0) {
        gpu3d_raster_words_copy_mask29((uint32_t *)frame, (const uint32_t *)machine);
        return;
    }

    gpu3d_t *config = hdr->gpu3d;
    uint32_t offset = (state->disp3dcnt >> 8) & 0xfu;
    uint32_t height = (uint32_t)(config->fog_offset & 0x7fffu) +
                     (0x400u >> offset);
    uint32_t param = offset | (height << 16);

    unsigned char stack[272];
    unsigned char *tmp = (unsigned char *)(uintptr_t)
        (((uintptr_t)stack + 15u) & ~(uintptr_t)15u);

    for (uint32_t block = 0; block != 16; block++) {
        gpu3d_raster_fog_density_line(
            GPU3D_BAND(machine)->plane1 + block * GPU3D_BAND_ROW_BYTES, tmp,
            config->fog_table, param);
        gpu3d_raster_blend_fog_line(
            frame + block * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(machine)->plane0 + block * GPU3D_BAND_ROW_BYTES, tmp,
            config->fog_color);
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_row_ramp_generate(int *param_1, float *param_2, float *param_3, int param_4)
{

    const unsigned char *base = (const unsigned char *)param_1;
    long row = 0;

    do {
        int32_t w8, w9;
        uint16_t w10;

        memcpy(&w8, base + (size_t)row * 4 + 0x000, sizeof(w8));
        memcpy(&w9, base + (size_t)row * 4 + 1u * RECON_3D_ROW_ARRAY, sizeof(w9));
        memcpy(&w10, base + (size_t)row * 4 + 9u * RECON_3D_ROW_ARRAY, sizeof(w10));

        int32_t sum = (int32_t)((uint32_t)w8 + (uint32_t)w9);

        float fVar7  = (float)sum * (float)w10;
        float fVar9  = (float)w8;
        float fVar18 = (float)w9;

#ifdef __ARM_NEON

        const float32x4_t t_lo = { 0.0f, 1.0f, 2.0f, 3.0f };
        const float32x4_t t_hi = { 4.0f, 5.0f, 6.0f, 7.0f };
        const float32x4_t d18 = vdupq_n_f32(fVar18);
        const float32x4_t f7  = vdupq_n_f32(fVar7);
        float32x4_t q5  = vfmsq_f32(f7, t_lo, d18);
        float32x4_t q6  = vfmsq_f32(f7, t_hi, d18);
        float32x4_t q16 = vmulq_n_f32(t_lo, fVar9);
        float32x4_t q17 = vmulq_n_f32(t_hi, fVar9);

        const float32x4_t p3 = vdupq_n_f32(8.0f * fVar18);
        const float32x4_t p2 = vdupq_n_f32(8.0f * fVar9);

        int32_t remaining = (int32_t)w10;
        do {
            vst1q_f32(param_3 + 0, q5);
            vst1q_f32(param_3 + 4, q6);
            param_3 += 8;
            vst1q_f32(param_2 + 0, q16);
            vst1q_f32(param_2 + 4, q17);
            param_2 += 8;

            q5  = vsubq_f32(q5,  p3);
            q6  = vsubq_f32(q6,  p3);
            q16 = vaddq_f32(q16, p2);
            q17 = vaddq_f32(q17, p2);

            remaining -= 8;
        } while (remaining > 0);
#else
        static const float table_low[4] = {0.0f, 1.0f, 2.0f, 3.0f};
        static const float table_high[4] = {4.0f, 5.0f, 6.0f, 7.0f};
        float v5[4], v6[4], v16[4], v17[4];
        int k;
        for (k = 0; k < 4; k++) {

            v5[k]  = fmaf(-table_low[k], fVar18, fVar7);
            v6[k]  = fmaf(-table_high[k], fVar18, fVar7);
            v16[k] = table_low[k] * fVar9;
            v17[k] = table_high[k] * fVar9;
        }

        float step3 = 8.0f * fVar18;
        float step2 = 8.0f * fVar9;

        int32_t remaining = (int32_t)w10;
        do {
            memcpy(param_3 + 0, v5, sizeof(v5));
            memcpy(param_3 + 4, v6, sizeof(v6));
            param_3 += 8;

            memcpy(param_2 + 0, v16, sizeof(v16));
            memcpy(param_2 + 4, v17, sizeof(v17));
            param_2 += 8;

            for (k = 0; k < 4; k++) {
                v5[k]  -= step3;
                v6[k]  -= step3;
                v16[k] += step2;
                v17[k] += step2;
            }

            remaining -= 8;
        } while (remaining > 0);

#endif
        param_3 += remaining;
        param_2 += remaining;

        row += 1;
        param_4 -= 1;
    } while (param_4 != 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifndef __ARM_NEON
static inline float frecpe(float b) {
    float r;
    __asm__("frecpe %s0, %s1" : "=w"(r) : "w"(b));
    return r;
}

static inline float frecps(float a, float b) {
    float r;
    __asm__("frecps %s0, %s1, %s2" : "=w"(r) : "w"(a), "w"(b));
    return r;
}
#endif

void gpu3d_raster_perspective_divide_fix15(int16_t *dst, const float *numer, const float *denom,
                         int32_t n) {
#ifdef __ARM_NEON

    do {
        float32x4_t r0, r1, r2, r3;
        int32x4_t f0, f1, f2, f3;
        float32x4_t b0 = vld1q_f32(denom + 0),  b1 = vld1q_f32(denom + 4);
        float32x4_t b2 = vld1q_f32(denom + 8),  b3 = vld1q_f32(denom + 12);
        r0 = vrecpeq_f32(b0); r0 = vmulq_f32(r0, vrecpsq_f32(r0, b0)); r0 = vmulq_f32(r0, vrecpsq_f32(r0, b0));
        r1 = vrecpeq_f32(b1); r1 = vmulq_f32(r1, vrecpsq_f32(r1, b1)); r1 = vmulq_f32(r1, vrecpsq_f32(r1, b1));
        r2 = vrecpeq_f32(b2); r2 = vmulq_f32(r2, vrecpsq_f32(r2, b2)); r2 = vmulq_f32(r2, vrecpsq_f32(r2, b2));
        r3 = vrecpeq_f32(b3); r3 = vmulq_f32(r3, vrecpsq_f32(r3, b3)); r3 = vmulq_f32(r3, vrecpsq_f32(r3, b3));
        f0 = vcvtq_n_s32_f32(vmulq_f32(vld1q_f32(numer + 0),  r0), 15);
        f1 = vcvtq_n_s32_f32(vmulq_f32(vld1q_f32(numer + 4),  r1), 15);
        f2 = vcvtq_n_s32_f32(vmulq_f32(vld1q_f32(numer + 8),  r2), 15);
        f3 = vcvtq_n_s32_f32(vmulq_f32(vld1q_f32(numer + 12), r3), 15);
        vst1q_s16(dst,     vcombine_s16(vmovn_s32(f0), vmovn_s32(f1)));
        vst1q_s16(dst + 8, vcombine_s16(vmovn_s32(f2), vmovn_s32(f3)));
        numer += 16; denom += 16; dst += 16; n -= 16;
    } while (n > 0);
    return;
#else
    do {
        int16_t t[16];
        for (int k = 0; k < 16; k++) {
            float a = numer[k];
            float b = denom[k];

            float r = frecpe(b);
            r = r * frecps(r, b);
            r = r * frecps(r, b);
            float q = a * r;
            int32_t fx = (int32_t)(q * 32768.0f);
            t[k] = (int16_t)fx;
        }
        memcpy(dst, t, sizeof(t));
        numer += 16;
        denom += 16;
        dst += 16;
        n -= 16;
    } while (n > 0);
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_row_fill_constants(unsigned char *tables, uint32_t *dst, uint32_t rows,
                        uint32_t base) {

    const uint32_t *pa = (const uint32_t *)(tables + 4u * RECON_3D_ROW_ARRAY);
    const uint32_t *pb = (const uint32_t *)(tables + 5u * RECON_3D_ROW_ARRAY);
    const uint16_t *len = (const uint16_t *)(tables + 9u * RECON_3D_ROW_ARRAY);

    uint32_t *other = dst + (uint64_t)base;

    do {
        int32_t n = (int32_t)*len;  len += 2;
        uint32_t vb = *pb++;
        uint32_t va = *pa++;

        int16_t a[4], b[4];
        for (int k = 0; k < 4; k++) {
            a[k] = (int16_t)((k & 1) ? (va >> 16) : va);
            b[k] = (int16_t)((k & 1) ? (vb >> 16) : vb);
        }
        int32_t v[4];
        for (int k = 0; k < 4; k++)
            v[k] = ((int32_t)a[k] << 15) + ((b[k] > 0) ? 0x400 : 0);

        uint32_t *q = dst, *r = other;
#ifdef __ARM_NEON
        {

            const uint32x4_t vv = { (uint32_t)v[0], (uint32_t)v[1],
                                    (uint32_t)v[2], (uint32_t)v[3] };
            const uint32x4_t vr = vdupq_n_u32(vb);
            do {
                vst1q_u32(q,      vv); vst1q_u32(q + 4,  vv);
                vst1q_u32(q + 8,  vv); vst1q_u32(q + 12, vv);
                q += 16;
                vst1q_u32(r, vr); vst1q_u32(r + 4, vr);
                r += 8;
                n -= 8;
        } while (n > 0);
        }
#else
        do {
            for (int c = 0; c < 4; c++)
                for (int k = 0; k < 4; k++) q[c * 4 + k] = (uint32_t)v[k];
            q += 16;
            for (int k = 0; k < 8; k++) r[k] = vb;
            r += 8;
            n -= 8;
        } while (n > 0);
#endif

        dst = q + (int64_t)n * 2;
        other = r + (int64_t)n;
    } while (--rows != 0);
}

void gpu3d_raster_planes_interleave_rgba(unsigned char *dst, const unsigned char *src,
                        uint32_t stride, int32_t n, uint32_t quarter) {

    unsigned char c = (unsigned char)quarter;
    const unsigned char *p0 = src;
    const unsigned char *p1 = src + (uint64_t)stride;
    const unsigned char *p2 = src + (uint64_t)stride * 2;

    do {
        for (int k = 0; k < 16; k++) {
            dst[k * 4 + 0] = p0[k];
            dst[k * 4 + 1] = p1[k];
            dst[k * 4 + 2] = p2[k];
            dst[k * 4 + 3] = c;
        }
        p0 += 16; p1 += 16; p2 += 16;
        dst += 64;
        n -= 16;
    } while (n > 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void gpu3d_raster_pixel_blend_alpha(uint8_t *param_1, const uint8_t *param_2,
                         int32_t param_3, uint8_t *param_4)
{

#ifdef __ARM_NEON

    {
        const uint8x16_t m1f = vdupq_n_u8(0x1f);
        uint8_t *p0 = param_1, *p3 = param_4;
        const uint8_t *p1 = param_2;
        int32_t n = param_3;
        do {
            uint8x16x4_t A = vld4q_u8(p0);
            uint8x16x4_t B = vld4q_u8(p1); p1 += 64;
            uint8x16_t a0  = A.val[3];
            uint8x16_t a1m = vandq_u8(B.val[3], m1f);
            uint8x16_t z   = vceqzq_u8(a1m);
            uint8x16_t wv  = vbslq_u8(z, m1f, a0);
            uint8x16_t wn  = vbicq_u8(vsubq_u8(m1f, a0), z);
            for (int q = 0; q < 3; q++) {
                uint16x8_t lo = vmovl_u8(vget_low_u8(A.val[q]));
                uint16x8_t hi = vmovl_high_u8(A.val[q]);
                lo = vmlal_u8(lo, vget_low_u8(A.val[q]), vget_low_u8(wv));
                lo = vmlal_u8(lo, vget_low_u8(B.val[q]), vget_low_u8(wn));
                hi = vmlal_high_u8(hi, A.val[q], wv);
                hi = vmlal_high_u8(hi, B.val[q], wn);
                A.val[q] = vshrn_high_n_u16(vshrn_n_u16(lo, 5), hi, 5);
            }
            vst1q_u8(p3, a0); p3 += 16;
            A.val[3] = vmaxq_u8(a0, a1m);
            vst4q_u8(p0, A); p0 += 64;
            n -= 16;
        } while (n > 0);
        return;
    }
#else

    uint8_t *p0 = param_1;
    const uint8_t *p1 = param_2;
    uint8_t *p3 = param_4;
    int32_t n = param_3;

    do {
        int i;
        for (i = 0; i < 16; i++) {
            uint8_t r0 = p0[i * 4 + 0];
            uint8_t g0 = p0[i * 4 + 1];
            uint8_t b0 = p0[i * 4 + 2];
            uint8_t a0 = p0[i * 4 + 3];

            uint8_t r1 = p1[i * 4 + 0];
            uint8_t g1 = p1[i * 4 + 1];
            uint8_t b1 = p1[i * 4 + 2];
            uint8_t a1 = p1[i * 4 + 3];

            uint8_t a1m = (uint8_t)(a1 & 0x1f);
            uint8_t wOld, wNew;

            if (a1m == 0) {
                wOld = 0x1f;
                wNew = 0;
            } else {
                wOld = a0;
                wNew = (uint8_t)(0x1f - a0);
            }

            uint32_t sr = (uint32_t)r0 * (uint32_t)(1u + wOld) +
                          (uint32_t)r1 * (uint32_t)wNew;
            uint32_t sg = (uint32_t)g0 * (uint32_t)(1u + wOld) +
                          (uint32_t)g1 * (uint32_t)wNew;
            uint32_t sb = (uint32_t)b0 * (uint32_t)(1u + wOld) +
                          (uint32_t)b1 * (uint32_t)wNew;

            uint8_t newR = (uint8_t)(sr >> 5);
            uint8_t newG = (uint8_t)(sg >> 5);
            uint8_t newB = (uint8_t)(sb >> 5);
            uint8_t newA = (a0 > a1m) ? a0 : a1m;

            p3[i] = a0;

            p0[i * 4 + 0] = newR;
            p0[i * 4 + 1] = newG;
            p0[i * 4 + 2] = newB;
            p0[i * 4 + 3] = newA;
        }

        p0 += 64;
        p1 += 64;
        p3 += 16;
        n -= 16;
    } while (n > 0);
#endif
}

void gpu3d_raster_pixel_alpha_max(uint8_t *param_1, const uint8_t *param_2,
                         int32_t param_3, uint8_t *param_4)
{

    uint8_t       *p0 = param_1;
    const uint8_t *p1 = param_2;
    uint8_t       *p3 = param_4;
    int32_t        n  = param_3;

    do {

        uint8_t r0[16], g0[16], b0[16], a0[16];
        uint8_t a1m[16];
        int i;

        for (i = 0; i < 16; i++) {
            r0[i] = p0[i * 4 + 0];
            g0[i] = p0[i * 4 + 1];
            b0[i] = p0[i * 4 + 2];
            a0[i] = p0[i * 4 + 3];

            a1m[i] = (uint8_t)(p1[i * 4 + 3] & 0x1f);
        }

        for (i = 0; i < 16; i++)
            p3[i] = a0[i];

        for (i = 0; i < 16; i++) {
            uint8_t newA = (a0[i] > a1m[i]) ? a0[i] : a1m[i];
            p0[i * 4 + 0] = r0[i];
            p0[i * 4 + 1] = g0[i];
            p0[i * 4 + 2] = b0[i];
            p0[i * 4 + 3] = newA;
        }

        p0 += 64;
        p1 += 64;
        p3 += 16;
        n  -= 16;
    } while (n > 0);
}

void gpu3d_raster_byte_fill(unsigned char *dst, uint32_t value, int32_t n) {
    unsigned char b = (unsigned char)value;
    do {
        for (int k = 0; k < 16; k++) dst[k] = b;
        dst += 16;
        n -= 16;
    } while (n > 0);
}

void gpu3d_raster_word_fill(uint32_t *dst, uint32_t value, int32_t n) {
    do {
        for (int k = 0; k < 16; k++) dst[k] = value;
        dst += 16;
        n -= 16;
    } while (n > 0);
}


#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize ("fp-contract=off")
#endif

static void write_row(float **output, float x0, float y0,
                           float step_x, float step_y, unsigned char n) {
    float *p = *output;
    int32_t c = (int32_t)(unsigned int)n;

    float px0 = x0,                  py0 = y0;
    float px1 = x0 + step_x,         py1 = y0 + step_y;
    float px2 = x0 + 2.0f * step_x,  py2 = y0 + 2.0f * step_y;
    float px3 = x0 + 3.0f * step_x,  py3 = y0 + 3.0f * step_y;
    const float sx = 4.0f * step_x,  sy = 4.0f * step_y;

    for (;;) {
        p[0] = px0; p[1] = py0;
        p[2] = px1; p[3] = py1;
        p[4] = px2; p[5] = py2;
        p[6] = px3; p[7] = py3;
        p += 8;

        px0 += sx; px1 += sx; px2 += sx; px3 += sx;
        py0 += sy; py1 += sy; py2 += sy; py3 += sy;

        int32_t prev = c;
        c = prev - 4;
        if (!(c != 0 && prev > 4))
            break;
    }

    p += (long)c * 2;
    *output = p;
}

static void process_row(float **output, const gpu3d_bank_vertex_t *pA,
                           const gpu3d_bank_vertex_t *pB, int with_param5,
                           int32_t param_5, unsigned char cnt) {
    int32_t iA = pA->w;
    int32_t iB = pB->w;
    uint16_t hA = pA->y;
    uint16_t hB = pB->y;

    int32_t dAB   = (int32_t)((uint32_t)iA - (uint32_t)iB);
    int32_t dHalf = (int32_t)((uint32_t)hB - (uint32_t)hA);

    float fA = (float)iA;
    float fD = (float)dAB;
    float fB = (float)iB;
    float fH = (float)dHalf;

    float x0, y0;
    if (with_param5) {
        float fp5 = (float)param_5;
        x0 = (fB * 0.0f) + (fp5 * fA);
        y0 = (fB * fH) + (fp5 * fD);
    } else {
        x0 = fB * 0.0f;
        y0 = fB * fH;
    }

    write_row(output, x0, y0, fA, fD, cnt);
}

void gpu3d_raster_ramp_pairs_generate(float *param_1, const gpu3d_bank_vertex_t *const *param_2, unsigned char *param_3,
                         int param_4, int32_t param_5) {

    const gpu3d_bank_vertex_t *const *pp = param_2;
    const unsigned char *pc = param_3;
    float *out = param_1;

    const gpu3d_bank_vertex_t *pA = pp[0];
    const gpu3d_bank_vertex_t *pB = pp[1];
    pp += 2;
    unsigned char cnt = *pc++;
    process_row(&out, pA, pB, 1, param_5, cnt);

    int32_t remaining = param_4 - 1;
    while (remaining != 0) {
        pA = pp[0];
        pB = pp[1];
        pp += 2;
        cnt = *pc++;
        process_row(&out, pA, pB, 0, 0, cnt);
        remaining -= 1;
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_rgba_force_opaque(unsigned char *px, int64_t n) {
    unsigned char *p = px;
    do {
#ifdef __ARM_NEON

        {
            const uint32x4_t height = vdupq_n_u32(0x80000000u);
            for (int k = 0; k < 16; k += 4) {
                uint32_t *w = (uint32_t *)(p + (size_t)k * 4);
                vst1q_u32(w, vorrq_u32(vld1q_u32(w), height));
            }
        }
#else
        for (int k = 0; k < 16; k++)
            p[k * 4 + 3] |= 0x80;
#endif
        p += 64;
        n -= 16;
    } while (n > 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "mem_access.h"

void gpu3d_raster_rows_copy_table(unsigned char *dst, unsigned char *src,
                        unsigned char *tables, uint32_t rows) {

    const uint16_t *orig = (const uint16_t *)(tables + 8u * RECON_3D_ROW_ARRAY);
    const uint16_t *len = (const uint16_t *)(tables + 9u * RECON_3D_ROW_ARRAY);

    do {
        uint32_t idx = *orig;  orig += 2;
        int32_t n = (int32_t)*len; len += 2;

        const unsigned char *p = src + (uint64_t)idx * 4;
        unsigned char *q = dst;
        do {

#ifdef __ARM_NEON
            uint32x4x2_t a = vld1q_u32_x2((const uint32_t *)(const void *)p);
            uint32x4x2_t b = vld1q_u32_x2((const uint32_t *)(const void *)(p + 32));
            vst1q_u32_x2((uint32_t *)(void *)q, a);
            vst1q_u32_x2((uint32_t *)(void *)(q + 32), b);
#else
            memcpy(q, p, 64);
#endif
            p += 64;
            q += 64;
            n -= 16;
        } while (n > 0);

        dst = q + (int64_t)n * 4;

        src += RECON_ROW_STEP / 2u;
    } while (--rows != 0);
}

void gpu3d_raster_rows_copy_table_triple(uint32_t *d0, uint32_t *d1, unsigned char *d2,
                        const uint32_t *s0, const uint32_t *s1,
                        const unsigned char *s2,
                        unsigned char *tables, uint32_t rows) {

    const uint16_t *orig = (const uint16_t *)(tables + 8u * RECON_3D_ROW_ARRAY);
    const uint16_t *len = (const uint16_t *)(tables + 9u * RECON_3D_ROW_ARRAY);

    do {
        uint32_t idx = *orig;  orig += 2;
        int32_t n = (int32_t)*len; len += 2;

        const unsigned char *p2 = s2 + (uint64_t)idx;
        const uint32_t *p1 = s1 + (uint64_t)idx;
        const uint32_t *p0 = s0 + (uint64_t)idx;

        uint32_t *q0 = d0, *q1 = d1;
        unsigned char *q2 = d2;
        do {
            for (int k = 0; k < 8; k++) q1[k] = p1[k];
            for (int k = 0; k < 8; k++) q0[k] = p0[k];
            for (int k = 0; k < 8; k++) q2[k] = p2[k];
            p0 += 8; p1 += 8; p2 += 8;
            q0 += 8; q1 += 8; q2 += 8;
            n -= 8;
        } while (n > 0);

        d0 = q0 + (int64_t)n;
        d1 = q1 + (int64_t)n;
        d2 = q2 + (int64_t)n;

        s1 += RECON_3D_WIDTH / 2u;
        s0 += RECON_3D_WIDTH / 2u;
        s2 += RECON_3D_WIDTH / 2u;
    } while (--rows != 0);
}

void gpu3d_raster_rows_copy_masked_alpha(unsigned char *tables, uint32_t *d0, uint32_t *d1,
                        uint32_t rows, uint32_t mark,
                        const uint32_t *s0, const uint32_t *s1,
                        const unsigned char *mask) {

    const uint16_t *t = (const uint16_t *)(tables + 8u * RECON_3D_ROW_ARRAY);
    uint32_t high = mark << 24;

    do {
        uint32_t idx = t[0];
        uint32_t n = t[RECON_3D_ROW_ARRAY / 2u];
        t += 2;

        uint32_t *q0 = d0 + (uint64_t)idx;
        uint32_t *q1 = d1 + (uint64_t)idx;

        do {
            unsigned char m = *mask++;
            uint32_t a = *s1++;
            uint32_t b = *s0++;
            a |= high;
            if (m != 0) { *q1 = a; *q0 = b; }
            q1++; q0++;
        } while (--n != 0);

        d0 += RECON_3D_WIDTH / 2u;
        d1 += RECON_3D_WIDTH / 2u;
    } while (--rows != 0);
}

#define OFF_IDX 0x580
#define OFF_CNT 0x630


void gpu3d_raster_span_copy_alpha_256(const unsigned char *param_1, uint32_t *param_2, uint32_t *param_3,
                         int32_t param_4, unsigned char param_5,
                         const uint32_t *param_6, const uint32_t *param_7)
{

    const unsigned char *ctx_idx = param_1 + OFF_IDX;
    const unsigned char *ctx_cnt = param_1 + OFF_CNT;

    uint32_t alpha = (uint32_t)param_5 << 24;

    uint32_t *dst_plane = param_2;
    uint32_t *dst_alpha  = param_3;
    const uint32_t *src_plane = param_6;
    const uint32_t *src_alpha  = param_7;

    uint32_t w7 = 0;
    int32_t rep = param_4;

    do {
        uint16_t idx = rd16(ctx_idx);
        uint16_t cnt = rd16(ctx_cnt);
        ctx_idx += 4;
        ctx_cnt += 4;

        w7 = w7 + (uint32_t)idx;
        dst_alpha  += w7;
        dst_plane += w7;

        w7 = (uint32_t)0x100 - (uint32_t)cnt;
        w7 = w7 - (uint32_t)idx;

        int32_t rem = (int32_t)cnt - 8;
        if (rem >= 0) {
            do {
                uint32_t block_a[8];
                uint32_t block_p[8];

                memcpy(block_a, src_alpha, sizeof block_a);
                src_alpha += 8;
                memcpy(block_p, src_plane, sizeof block_p);
                src_plane += 8;

                rem -= 8;

                for (int k = 0; k < 8; k++) {
                    block_a[k] |= alpha;
                }

                memcpy(dst_alpha, block_a, sizeof block_a);
                dst_alpha += 8;
                memcpy(dst_plane, block_p, sizeof block_p);
                dst_plane += 8;
            } while (rem > 0);
        }
        rem += 8;

        uint32_t peek_a[8];
        uint32_t peek_p[8];
        memcpy(peek_a, src_alpha, sizeof peek_a);
        memcpy(peek_p, src_plane, sizeof peek_p);
        for (int k = 0; k < 8; k++) {
            peek_a[k] |= alpha;
        }

        memcpy(dst_alpha, peek_a, (size_t)rem * 4);
        memcpy(dst_plane, peek_p, (size_t)rem * 4);
        dst_alpha  += rem;
        dst_plane += rem;
        src_alpha  += rem;
        src_plane += rem;

        rep -= 1;
    } while (rep != 0);
}

void gpu3d_raster_span_copy_alpha_256_dc00(const unsigned char *param_1, uint32_t *dst_a, uint32_t *dst_b,
                             unsigned char *dst_c, int32_t spans,
                             const uint32_t *src_a, const uint32_t *src_b,
                             const unsigned char *src_c)
{
    const unsigned char *ctx_idx = param_1 + OFF_IDX;
    const unsigned char *ctx_cnt = param_1 + OFF_CNT;

    uint32_t w8 = 0;
    int32_t rep = spans;

    do {
        uint16_t idx = rd16(ctx_idx);
        uint16_t cnt = rd16(ctx_cnt);
        ctx_idx += 4;
        ctx_cnt += 4;

        w8 = w8 + (uint32_t)idx;
        dst_b += w8;
        dst_a += w8;
        dst_c += w8;

        w8 = (uint32_t)0x100 - (uint32_t)cnt;
        w8 = w8 - (uint32_t)idx;

        int32_t rem = (int32_t)cnt - 8;
        if (rem >= 0) {
            do {
                uint32_t blo_b[8], blo_a[8];
                unsigned char blo_c[8];

                memcpy(blo_b, src_b, sizeof blo_b); src_b += 8;
                memcpy(blo_a, src_a, sizeof blo_a); src_a += 8;
                memcpy(blo_c, src_c, sizeof blo_c); src_c += 8;

                rem -= 8;

                memcpy(dst_b, blo_b, sizeof blo_b); dst_b += 8;
                memcpy(dst_a, blo_a, sizeof blo_a); dst_a += 8;
                memcpy(dst_c, blo_c, sizeof blo_c); dst_c += 8;
            } while (rem > 0);
        }
        rem += 8;

        uint32_t peek_b[8], peek_a[8];
        unsigned char peek_c[8];
        memcpy(peek_b, src_b, sizeof peek_b);
        memcpy(peek_a, src_a, sizeof peek_a);
        memcpy(peek_c, src_c, sizeof peek_c);

        memcpy(dst_b, peek_b, (size_t)rem * 4);
        memcpy(dst_a, peek_a, (size_t)rem * 4);

        {
            int32_t k;
            for (k = 0; k < rem; k++) {
                dst_c[k] = peek_c[(rem == 5 && k == 4) ? 5 : k];
            }
        }

        dst_b += rem; dst_a += rem; dst_c += rem;
        src_b += rem; src_a += rem; src_c += rem;

        rep -= 1;
    } while (rep != 0);
}
#undef OFF_IDX
#undef OFF_CNT

void gpu3d_raster_blit_row_stride_256(unsigned char *dst, unsigned char *src,
                        uint32_t width, uint32_t rows) {
    do {
        int32_t r = (int32_t)width;
        do {
            unsigned char tmp[64];
            __builtin_memcpy(tmp, src, 64);
            __builtin_memcpy(dst, tmp, 64);
            dst += 64;
            src += 64;
            r -= 16;
        } while (r > 0);

        src -= (uint64_t)width * 4;
        dst += (int64_t)r * 4;
        src += (int64_t)r * 4;
        src += 0x400;
    } while (--rows != 0);
}

void gpu3d_raster_rows_copy_fixed_triple_256(uint32_t *d0, uint32_t *d1, unsigned char *d2,
                        const uint32_t *s0, const uint32_t *s1,
                        const unsigned char *s2,
                        uint32_t width, uint32_t rows) {

    int32_t jump = 256 - (int32_t)((width + 7) & 0xfffffff8u);

    do {
        int32_t n = (int32_t)width;
        do {
            for (int k = 0; k < 8; k++) d1[k] = s1[k];
            for (int k = 0; k < 8; k++) d0[k] = s0[k];
            for (int k = 0; k < 8; k++) d2[k] = s2[k];
            s1 += 8; s0 += 8; s2 += 8;
            d1 += 8; d0 += 8; d2 += 8;
            n -= 8;
        } while (n > 0);

        d0 += n;  d1 += n;  d2 += n;
        s1 += jump;  s0 += jump;  s2 += jump;
    } while (--rows != 0);
}

void gpu3d_raster_words_copy_mask29(uint32_t *dest, const uint32_t *origin) {
    for (int i = 0; i < 4096; i++)
        dest[i] = origin[i] & 0x1fffffffu;
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#ifndef __ARM_NEON

static int16_t saturate16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static int16_t shift_sat(int16_t v, int s) {
    if (s >= 0) {
        if (s >= 16) return (v == 0) ? 0 : (v > 0 ? 32767 : -32768);
        return saturate16((int32_t)v << s);
    }
    int d = -s;
    if (d >= 16) return (v < 0) ? -1 : 0;
    return (int16_t)(v >> d);
}
#endif

void gpu3d_raster_fog_density_line(const unsigned char *src, unsigned char *dst,
                        const unsigned char *tables, uint32_t pair) {

#ifdef __ARM_NEON

    {
        uint8x16x2_t tb, tp;
        tb.val[0] = vld1q_u8(tables);      tb.val[1] = vld1q_u8(tables + 16);
        tp.val[0] = vld1q_u8(tables + 32); tp.val[1] = vld1q_u8(tables + 48);
        uint16x8_t vses = vdupq_n_u16((uint16_t)(pair >> 16));
        int16x8_t  vdes = vdupq_n_s16((int16_t)pair);

        const unsigned char *fu = (const unsigned char *)src;
        unsigned char *sa = dst;
        const uint16x8_t m7f = vdupq_n_u16(0x7fff), m3ff = vdupq_n_u16(0x03ff);
        for (int g = 0; g < 8; g++) {
            uint32x4_t s0 = vreinterpretq_u32_u8(vld1q_u8(fu + 0)),  s1 = vreinterpretq_u32_u8(vld1q_u8(fu + 16));
            uint32x4_t s2 = vreinterpretq_u32_u8(vld1q_u8(fu + 32)),  s3 = vreinterpretq_u32_u8(vld1q_u8(fu + 48));
            uint32x4_t s4 = vreinterpretq_u32_u8(vld1q_u8(fu + 64)), s5 = vreinterpretq_u32_u8(vld1q_u8(fu + 80));
            uint32x4_t s6 = vreinterpretq_u32_u8(vld1q_u8(fu + 96)), s7 = vreinterpretq_u32_u8(vld1q_u8(fu + 112));
            fu += 128;
            uint16x8_t q0 = vshrn_high_n_u32(vshrn_n_u32(s0, 9), s1, 9);
            uint16x8_t q1 = vshrn_high_n_u32(vshrn_n_u32(s2, 9), s3, 9);
            uint16x8_t q2 = vshrn_high_n_u32(vshrn_n_u32(s4, 9), s5, 9);
            uint16x8_t q3 = vshrn_high_n_u32(vshrn_n_u32(s6, 9), s7, 9);
            q0 = vandq_u16(q0, m7f); q1 = vandq_u16(q1, m7f);
            q2 = vandq_u16(q2, m7f); q3 = vandq_u16(q3, m7f);
            q0 = vqsubq_u16(q0, vses); q1 = vqsubq_u16(q1, vses);
            q2 = vqsubq_u16(q2, vses); q3 = vqsubq_u16(q3, vses);
            q0 = vreinterpretq_u16_s16(vqshlq_s16(vreinterpretq_s16_u16(q0), vdes));
            q1 = vreinterpretq_u16_s16(vqshlq_s16(vreinterpretq_s16_u16(q1), vdes));
            q2 = vreinterpretq_u16_s16(vqshlq_s16(vreinterpretq_s16_u16(q2), vdes));
            q3 = vreinterpretq_u16_s16(vqshlq_s16(vreinterpretq_s16_u16(q3), vdes));

            uint8x16_t x0 = vshrn_high_n_u16(vshrn_n_u16(q0, 8), q1, 8);
            uint8x16_t x1 = vshrn_high_n_u16(vshrn_n_u16(q2, 8), q3, 8);

            int16x8_t w0 = vreinterpretq_s16_u16(vandq_u16(q0, m3ff));
            int16x8_t w1 = vreinterpretq_s16_u16(vandq_u16(q1, m3ff));
            int16x8_t w2 = vreinterpretq_s16_u16(vandq_u16(q2, m3ff));
            int16x8_t w3 = vreinterpretq_s16_u16(vandq_u16(q3, m3ff));
            x0 = vshrq_n_u8(x0, 2); x1 = vshrq_n_u8(x1, 2);
            uint8x16_t b0 = vqtbl2q_u8(tb, x0), b1 = vqtbl2q_u8(tb, x1);
            int8x16_t  p0 = vreinterpretq_s8_u8(vqtbl2q_u8(tp, x0));
            int8x16_t  p1 = vreinterpretq_s8_u8(vqtbl2q_u8(tp, x1));
            int16x8_t  e0 = vshll_n_s8(vget_low_s8(p0), 5), e1 = vshll_high_n_s8(p0, 5);
            int16x8_t  e2 = vshll_n_s8(vget_low_s8(p1), 5), e3 = vshll_high_n_s8(p1, 5);
            e0 = vqdmulhq_s16(e0, w0); e1 = vqdmulhq_s16(e1, w1);
            e2 = vqdmulhq_s16(e2, w2); e3 = vqdmulhq_s16(e3, w3);
            int8x16_t o0 = vmovn_high_s16(vmovn_s16(e0), e1);
            int8x16_t o1 = vmovn_high_s16(vmovn_s16(e2), e3);
            vst1q_u8(sa,      vaddq_u8(b0, vreinterpretq_u8_s8(o0)));
            vst1q_u8(sa + 16, vaddq_u8(b1, vreinterpretq_u8_s8(o1)));
            sa += 32;
        }
        return;
    }
#else

    const unsigned char *tA = tables;
    const unsigned char *tB = tables + 32;
    uint16_t diff = (uint16_t)(pair >> 16);
    int shift = (int)(int8_t)(uint8_t)pair;

    for (int n = 0; n < 256; n++) {
        uint32_t e = *(const uint32_t *)(src + n * 4);

        uint16_t h = (uint16_t)(e >> 9);
        h = (uint16_t)(h & ~0x8000u);
        h = (uint16_t)((h >= diff) ? (h - diff) : 0);
        h = (uint16_t)shift_sat((int16_t)h, shift);

        uint8_t idx = (uint8_t)((uint8_t)(h >> 8) >> 2);
        int16_t m = (int16_t)(h & 0x3ffu);

        uint8_t base = (idx < 32) ? tA[idx] : 0;
        uint8_t coef = (idx < 32) ? tB[idx] : 0;

        int16_t k = saturate16((int32_t)(int8_t)coef << 5);
        int16_t p = saturate16(((int32_t)k * (int32_t)m * 2) >> 16);

        dst[n] = (uint8_t)(base + (uint8_t)p);
    }
#endif
}

void gpu3d_raster_palette_blit_256(unsigned char *dst, const unsigned char *src,
                        const unsigned char *idx, const unsigned char *pal) {

    unsigned char t0[16], t1[16], t2[16];
    for (int i = 0; i < 8; i++) {
        t0[2 * i] = pal[i];      t0[2 * i + 1] = 0;
        t1[2 * i] = pal[8 + i];  t1[2 * i + 1] = 0;
        t2[2 * i] = pal[16 + i]; t2[2 * i + 1] = 0;
    }

    for (int base = 0; base < 256; base += 32) {
        unsigned char pix[128];
        __builtin_memcpy(pix, src + base * 4, 128);

        for (int j = 0; j < 32; j++) {
            unsigned char k = (unsigned char)(idx[base + j] * 2);
            if (k < 16) {
                pix[j * 4 + 0] = t0[k];
                pix[j * 4 + 1] = t1[k];
                pix[j * 4 + 2] = t2[k];
            }
            unsigned char height = (unsigned char)(idx[base + j] >> 3);
            pix[j * 4 + 3] = (unsigned char)((pix[j * 4 + 3] & 0x1f)
                                           | (height & 0xe0));
        }

        __builtin_memcpy(dst + base * 4, pix, 128);
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_rows_copy_one_plane_wide(unsigned char *dst, unsigned char *src,
                        unsigned char *tables, uint32_t rows) {

    const uint16_t *orig = (const uint16_t *)(tables + 8u * RECON_3D_ROW_ARRAY);
    const uint16_t *len = (const uint16_t *)(tables + 9u * RECON_3D_ROW_ARRAY);

    do {
        uint32_t idx = *orig;  orig += 2;
        int32_t n = (int32_t)*len; len += 2;

        const uint32_t *p = (const uint32_t *)(src + (uint64_t)idx * 4);
        uint32_t *q = (uint32_t *)dst;
        do {
#ifdef __ARM_NEON

            uint32x4_t c0 = vld1q_u32(p);
            uint32x4_t c1 = vld1q_u32(p + 4);
            uint32x4_t c2 = vld1q_u32(p + 8);
            uint32x4_t c3 = vld1q_u32(p + 12);
            vst1q_u32(q,      c0);
            vst1q_u32(q + 4,  c1);
            vst1q_u32(q + 8,  c2);
            vst1q_u32(q + 12, c3);
#else
            for (int k = 0; k < 16; k++) q[k] = p[k];
#endif
            p += 16;
            q += 16;
            n -= 16;
        } while (n > 0);

        dst = (unsigned char *)q + (int64_t)n * 4;
        src += RECON_ROW_STEP;
    } while (--rows != 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"

void gpu3d_raster_rows_copy_three_planes_wide(uint32_t *d0, uint32_t *d1, unsigned char *d2,
                         const uint32_t *s0, const uint32_t *s1,
                         const unsigned char *s2,
                         unsigned char *tables, uint32_t rows) {
#ifdef __ARM_NEON

    {
        const unsigned char *t0 = tables + 8u * RECON_3D_ROW_ARRAY;
        const unsigned char *t1 = tables + 9u * RECON_3D_ROW_ARRAY;
        uint32_t *p0 = d0, *p1 = d1;
        unsigned char *p2 = d2;
        const uint32_t *q0 = s0, *q1 = s1;
        const unsigned char *q2 = s2;
        uint32_t f = rows;
        do {
            uint32_t off = *(const uint16_t *)(const void *)t0; t0 += 4;
            int32_t  cnt = (int32_t)*(const uint16_t *)(const void *)t1; t1 += 4;
            const uint32_t *r1 = q1 + off;
            const uint32_t *r0 = q0 + off;
            const unsigned char *r2 = q2 + off;
            do {
                uint32x4x2_t a = vld1q_u32_x2(r1); r1 += 8;
                uint32x4x2_t b = vld1q_u32_x2(r0); r0 += 8;
                uint8x8_t    c = vld1_u8(r2);      r2 += 8;
                vst1q_u32_x2(p1, a); p1 += 8;
                vst1q_u32_x2(p0, b); p0 += 8;
                vst1_u8(p2, c);      p2 += 8;
                cnt -= 8;
            } while (cnt > 0);
            p0 += cnt;
            p1 += cnt;
            p2 += cnt;
            q1 += RECON_ROW_STEP / 4u;
            q0 += RECON_ROW_STEP / 4u;
            q2 += RECON_3D_WIDTH;

        } while (--f);
        return;
    }
#else

    const unsigned char *tab_orig = tables + 8u * RECON_3D_ROW_ARRAY;
    const unsigned char *tab_len = tables + 9u * RECON_3D_ROW_ARRAY;

    do {
        uint16_t idx16, len16;
        memcpy(&idx16, tab_orig, sizeof(uint16_t));
        tab_orig += 4;
        memcpy(&len16, tab_len, sizeof(uint16_t));
        tab_len += 4;
        uint32_t idx = idx16;
        int32_t n = (int32_t)len16;

        const unsigned char *p2 = s2 + (uint64_t)idx;
        const uint32_t *p1 = s1 + (uint64_t)idx;
        const uint32_t *p0 = s0 + (uint64_t)idx;

        uint32_t *q0 = d0, *q1 = d1;
        unsigned char *q2 = d2;
        do {
            for (int k = 0; k < 8; k++) q1[k] = p1[k];
            for (int k = 0; k < 8; k++) q0[k] = p0[k];
            for (int k = 0; k < 8; k++) q2[k] = p2[k];
            p0 += 8; p1 += 8; p2 += 8;
            q0 += 8; q1 += 8; q2 += 8;
            n -= 8;
        } while (n > 0);

        d0 = q0 + (int64_t)n;
        d1 = q1 + (int64_t)n;
        d2 = q2 + (int64_t)n;
        s1 += RECON_ROW_STEP / 4u;
        s0 += RECON_ROW_STEP / 4u;
        s2 += RECON_3D_WIDTH;
    } while (--rows != 0);
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_rows_copy_gap_masked_alpha(const unsigned char *base, unsigned char *dst_a,
                        unsigned char *dst_b, uint32_t rows, uint32_t alpha,
                        const unsigned char *src_a, const unsigned char *src_b,
                        const unsigned char *mask) {

    const unsigned char *tab = base + 8u * RECON_3D_ROW_ARRAY;
    uint32_t orval = alpha << 24;
    for (;;) {
        uint16_t hh, nn;
        memcpy(&hh, tab + 0, 2);
        memcpy(&nn, tab + RECON_3D_ROW_ARRAY, 2);
        uint32_t gap = hh, n = nn;
        tab += 4;
        unsigned char *fa = dst_a + (uint64_t)gap * 4;
        unsigned char *fb = dst_b + (uint64_t)gap * 4;
#ifdef __ARM_NEON

        {
            const uint32x4_t vor = vdupq_n_u32(orval);
            while (n >= 4u) {
                uint32_t m0 = mask[0], m1 = mask[1];
                uint32_t m2 = mask[2], m3 = mask[3];
                if ((m0 | m1 | m2 | m3) == 0u) {
                    mask += 4; src_a += 16; src_b += 16;
                    fa += 16; fb += 16; n -= 4u;
                    continue;
                }
                if (m0 && m1 && m2 && m3) {
                    uint32x4_t vb = vorrq_u32(vld1q_u32((const uint32_t *)src_b), vor);
                    uint32x4_t va = vld1q_u32((const uint32_t *)src_a);
                    vst1q_u32((uint32_t *)fb, vb);
                    vst1q_u32((uint32_t *)fa, va);
                    mask += 4; src_a += 16; src_b += 16;
                    fa += 16; fb += 16; n -= 4u;
                    continue;
                }
                for (unsigned k = 0; k < 4u; k++) {
                    uint32_t m = *mask++;
                    uint32_t vb, va;
                    memcpy(&vb, src_b, 4); src_b += 4;
                    memcpy(&va, src_a, 4); src_a += 4;
                    vb |= orval;
                    if (m != 0) { memcpy(fb, &vb, 4); memcpy(fa, &va, 4); }
                    fb += 4; fa += 4;
                }
                n -= 4u;
            }
        }
        while (n != 0u) {
            uint32_t m = *mask++;
            uint32_t vb, va;
            memcpy(&vb, src_b, 4); src_b += 4;
            memcpy(&va, src_a, 4); src_a += 4;
            vb |= orval;
            if (m != 0) { memcpy(fb, &vb, 4); memcpy(fa, &va, 4); }
            fb += 4; fa += 4;
            n--;
        }
#else
        for (;;) {
            uint32_t m = *mask++;
            uint32_t vb, va;
            memcpy(&vb, src_b, 4); src_b += 4;
            memcpy(&va, src_a, 4); src_a += 4;
            vb |= orval;
            if (m != 0) {
                memcpy(fb, &vb, 4);
                memcpy(fa, &va, 4);
            }
            fb += 4;
            fa += 4;
            n = n - 1u;
            if (n == 0) break;
        }
#endif
        dst_a += RECON_ROW_STEP;
        dst_b += RECON_ROW_STEP;
        rows = rows - 1u;
        if (rows == 0) break;
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#define OFF_IDX (8u * RECON_3D_ROW_ARRAY)
#define OFF_CNT (9u * RECON_3D_ROW_ARRAY)
#define CARRY_WIDTH ((uint32_t)RECON_3D_WIDTH)


void gpu3d_raster_span_copy_alpha_512(const unsigned char *param_1, uint32_t *param_2, uint32_t *param_3,
                         int32_t param_4, unsigned char param_5,
                         const uint32_t *param_6, const uint32_t *param_7)
{

    const unsigned char *ctx_idx = param_1 + OFF_IDX;
    const unsigned char *ctx_cnt = param_1 + OFF_CNT;

    uint32_t alpha = (uint32_t)param_5 << 24;

    uint32_t *dst_plane = param_2;
    uint32_t *dst_alpha  = param_3;
    const uint32_t *src_plane = param_6;
    const uint32_t *src_alpha  = param_7;

    uint32_t w7 = 0;
    int32_t rep = param_4;

    do {
        uint16_t idx = rd16(ctx_idx);
        uint16_t cnt = rd16(ctx_cnt);
        ctx_idx += 4;
        ctx_cnt += 4;

        w7 = w7 + (uint32_t)idx;
        dst_alpha  += w7;
        dst_plane += w7;

        w7 = (uint32_t)CARRY_WIDTH - (uint32_t)cnt;
        w7 = w7 - (uint32_t)idx;

        int32_t rem = (int32_t)cnt - 8;
        if (rem >= 0) {
#ifdef __ARM_NEON

            {
                const uint32x4_t va = vdupq_n_u32(alpha);
                do {
                    uint32x4_t a0 = vld1q_u32(src_alpha);
                    uint32x4_t a1 = vld1q_u32(src_alpha + 4);   src_alpha  += 8;
                    uint32x4_t p0 = vld1q_u32(src_plane);
                    uint32x4_t p1 = vld1q_u32(src_plane + 4);  src_plane += 8;

                    rem -= 8;

                    vst1q_u32(dst_alpha,     vorrq_u32(a0, va));
                    vst1q_u32(dst_alpha + 4, vorrq_u32(a1, va));  dst_alpha  += 8;
                    vst1q_u32(dst_plane,     p0);
                    vst1q_u32(dst_plane + 4, p1);                dst_plane += 8;
                } while (rem > 0);
            }
#else
            do {
                uint32_t block_a[8];
                uint32_t block_p[8];

                memcpy(block_a, src_alpha, sizeof block_a);
                src_alpha += 8;
                memcpy(block_p, src_plane, sizeof block_p);
                src_plane += 8;

                rem -= 8;

                for (int k = 0; k < 8; k++) {
                    block_a[k] |= alpha;
                }

                memcpy(dst_alpha, block_a, sizeof block_a);
                dst_alpha += 8;
                memcpy(dst_plane, block_p, sizeof block_p);
                dst_plane += 8;
            } while (rem > 0);
#endif
        }
        rem += 8;

        uint32_t peek_a[8];
        uint32_t peek_p[8];
        memcpy(peek_a, src_alpha, sizeof peek_a);
        memcpy(peek_p, src_plane, sizeof peek_p);
        for (int k = 0; k < 8; k++) {
            peek_a[k] |= alpha;
        }

        memcpy(dst_alpha, peek_a, (size_t)rem * 4);
        memcpy(dst_plane, peek_p, (size_t)rem * 4);
        dst_alpha  += rem;
        dst_plane += rem;
        src_alpha  += rem;
        src_plane += rem;

        rep -= 1;
    } while (rep != 0);
}
#define OFF_IDX_EFE8 (8u * RECON_3D_ROW_ARRAY)
#define OFF_CNT_EFE8 (9u * RECON_3D_ROW_ARRAY)
#define CARRY_WIDTH_EFE8 ((uint32_t)RECON_3D_WIDTH)

static const unsigned char lanes_efe8[9][8] = {
    { 0 },
    { 0 },
    { 0, 1 },
    { 0, 1, 2 },
    { 0, 1, 2, 3 },
    { 0, 1, 2, 3, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5, 6 },
    { 0, 1, 2, 3, 4, 5, 6, 7 }
};

void gpu3d_raster_span_copy_triple_512(const unsigned char *param_1, uint32_t *param_2, uint32_t *param_3,
                        uint8_t *param_4, int32_t param_5,
                        const uint32_t *param_6, const uint32_t *param_7,
                        const uint8_t *param_8)
{

    const unsigned char *ctx_idx = param_1 + OFF_IDX_EFE8;
    const unsigned char *ctx_cnt = param_1 + OFF_CNT_EFE8;

    uint32_t *dst_a = param_2;
    uint32_t *dst_b = param_3;
    uint8_t  *dst_c = param_4;
    const uint32_t *src_a = param_6;
    const uint32_t *src_b = param_7;
    const uint8_t  *src_c = param_8;

    uint32_t w8 = 0;
    int32_t rep = param_5;

    do {
        uint16_t idx = rd16(ctx_idx);
        uint16_t cnt = rd16(ctx_cnt);
        ctx_idx += 4;
        ctx_cnt += 4;

        w8 = w8 + (uint32_t)idx;
        dst_b += w8;
        dst_a += w8;
        dst_c += w8;

        w8 = (uint32_t)CARRY_WIDTH_EFE8 - (uint32_t)cnt;
        w8 = w8 - (uint32_t)idx;

        int32_t rem = (int32_t)cnt - 8;
        if (rem >= 0) {
#ifdef __ARM_NEON

            do {
                uint32x4_t b0 = vld1q_u32(src_b);
                uint32x4_t b1 = vld1q_u32(src_b + 4);   src_b += 8;
                uint32x4_t a0 = vld1q_u32(src_a);
                uint32x4_t a1 = vld1q_u32(src_a + 4);   src_a += 8;
                uint8x8_t  c0 = vld1_u8(src_c);         src_c += 8;

                rem -= 8;

                vst1q_u32(dst_b, b0); vst1q_u32(dst_b + 4, b1); dst_b += 8;
                vst1q_u32(dst_a, a0); vst1q_u32(dst_a + 4, a1); dst_a += 8;
                vst1_u8(dst_c, c0);                             dst_c += 8;
            } while (rem > 0);
#else
            do {
                uint32_t block_b[8];
                uint32_t block_a[8];
                uint8_t  block_c[8];

                memcpy(block_b, src_b, sizeof block_b); src_b += 8;
                memcpy(block_a, src_a, sizeof block_a); src_a += 8;
                memcpy(block_c, src_c, sizeof block_c); src_c += 8;

                rem -= 8;

                memcpy(dst_b, block_b, sizeof block_b); dst_b += 8;
                memcpy(dst_a, block_a, sizeof block_a); dst_a += 8;
                memcpy(dst_c, block_c, sizeof block_c); dst_c += 8;
            } while (rem > 0);
#endif
        }
        rem += 8;

        uint32_t peek_b[8];
        uint32_t peek_a[8];
        uint8_t  peek_c[8];
        memcpy(peek_b, src_b, sizeof peek_b);
        memcpy(peek_a, src_a, sizeof peek_a);
        memcpy(peek_c, src_c, sizeof peek_c);

        memcpy(dst_b, peek_b, (size_t)rem * 4);
        memcpy(dst_a, peek_a, (size_t)rem * 4);

        for (int32_t k = 0; k < rem; k++)
            dst_c[k] = peek_c[lanes_efe8[rem][k]];

        dst_b += rem;
        dst_a += rem;
        dst_c += rem;
        src_b += rem;
        src_a += rem;
        src_c += rem;

        rep -= 1;
    } while (rep != 0);
}
#undef OFF_IDX
#undef OFF_CNT
#undef CARRY_WIDTH
#undef OFF_IDX_EFE8
#undef OFF_CNT_EFE8
#undef CARRY_WIDTH_EFE8

void gpu3d_raster_blit_row_stride_512(unsigned char *dst, unsigned char *src,
                        uint32_t width, uint32_t rows) {
    do {
        int32_t r = (int32_t)width;
        do {
            unsigned char tmp[64];
            __builtin_memcpy(tmp, src, 64);
            __builtin_memcpy(dst, tmp, 64);
            dst += 64;
            src += 64;
            r -= 16;
        } while (r > 0);

        src -= (uint64_t)width * 4;
        dst += (int64_t)r * 4;
        src += (int64_t)r * 4;

        src += RECON_ROW_STEP;
    } while (--rows != 0);
}

void gpu3d_raster_rows_copy_fixed_triple_512(uint32_t *d0, uint32_t *d1, unsigned char *d2,
                        const uint32_t *s0, const uint32_t *s1,
                        const unsigned char *s2,
                        uint32_t width, uint32_t rows) {

    int32_t jump = (int32_t)RECON_3D_WIDTH - (int32_t)((width + 7) & 0xfffffff8u);

    do {
        int32_t n = (int32_t)width;
        do {
            for (int k = 0; k < 8; k++) d1[k] = s1[k];
            for (int k = 0; k < 8; k++) d0[k] = s0[k];
            for (int k = 0; k < 8; k++) d2[k] = s2[k];
            s1 += 8; s0 += 8; s2 += 8;
            d1 += 8; d0 += 8; d2 += 8;
            n -= 8;
        } while (n > 0);

        d0 += n;  d1 += n;  d2 += n;
        s1 += jump;  s0 += jump;  s2 += jump;
    } while (--rows != 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"

void gpu3d_raster_fog_blend_color_line_512(unsigned char *dst, const unsigned char *src,
                        const unsigned char *factor, uint32_t color) {

#ifdef __ARM_NEON

    {
        uint8x16_t k0 = vdupq_n_u8((uint8_t)color);
        uint8x16_t k1 = vdupq_n_u8((uint8_t)(color >> 8));
        uint8x16_t k2 = vdupq_n_u8((uint8_t)(color >> 16));
        uint8x16_t k3 = vdupq_n_u8((uint8_t)(color >> 24));
        const uint8x16_t m7f = vdupq_n_u8(0x7f);
        for (int base = 0; base < (int)RECON_3D_WIDTH; base += 16) {
            uint8x16x4_t v = vld4q_u8(src + base * 4);
            uint8x16_t f  = vld1q_u8(factor + base);
            uint8x16_t eq = vceqq_u8(f, m7f);
            uint8x16_t fn = vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(f)));
            uint8x16_t pu = vcgtq_u8(v.val[3], m7f);
            fn = vaddq_u8(fn, eq);
            v.val[3] = vandq_u8(v.val[3], m7f);
            fn = vandq_u8(fn, pu);
            int8x16_t fs = vreinterpretq_s8_u8(fn);
            uint8x16_t d[4];
            d[0] = vsubq_u8(v.val[0], k0);
            d[1] = vsubq_u8(v.val[1], k1);
            d[2] = vsubq_u8(v.val[2], k2);
            d[3] = vsubq_u8(v.val[3], k3);
            for (int q = 0; q < 4; q++) {
                int8x16_t ds = vreinterpretq_s8_u8(d[q]);
                uint16x8_t lo = vreinterpretq_u16_s16(
                        vmull_s8(vget_low_s8(ds), vget_low_s8(fs)));
                uint16x8_t hi = vreinterpretq_u16_s16(vmull_high_s8(ds, fs));
                v.val[q] = vaddq_u8(v.val[q],
                                    vshrn_high_n_u16(vshrn_n_u16(lo, 7), hi, 7));
            }
            vst4q_u8(dst + base * 4, v);
        }
        return;
    }
#else

    uint8_t c[4];
    c[0] = (uint8_t)color; color >>= 8;
    c[1] = (uint8_t)color; color >>= 8;
    c[2] = (uint8_t)color; color >>= 8;
    c[3] = (uint8_t)color;

    for (int base = 0; base < (int)RECON_3D_WIDTH; base += 16) {
        unsigned char pix[64];
        __builtin_memcpy(pix, src + base * 4, 64);

        for (int j = 0; j < 16; j++) {
            uint8_t f0 = factor[base + j];
            uint8_t eq = (f0 == 0x7f) ? 0xff : 0;
            uint8_t f  = (uint8_t)((uint8_t)(-(int)f0) + eq);

            uint8_t alpha = pix[j * 4 + 3];
            uint8_t gate = (alpha > 0x7f) ? 0xff : 0;
            f = (uint8_t)(f & gate);

            alpha = (uint8_t)(alpha & 0x7f);
            pix[j * 4 + 3] = alpha;

            for (int p = 0; p < 4; p++) {
                uint8_t v = pix[j * 4 + p];
                int8_t d = (int8_t)(uint8_t)(v - c[p]);
                int8_t g = (int8_t)f;
                pix[j * 4 + p] = (uint8_t)(v + (uint8_t)(((int32_t)d * g) >> 7));
            }
        }

        __builtin_memcpy(dst + base * 4, pix, 64);
    }
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"

void gpu3d_raster_fog_blend_color_split_parity(unsigned char *dst_pair, const unsigned char *src,
                         const unsigned char *factor, uint32_t w3_color)
{

#ifdef __ARM_NEON

    {
        uint8x16_t k0 = vdupq_n_u8((uint8_t)w3_color);
        uint8x16_t k1 = vdupq_n_u8((uint8_t)(w3_color >> 8));
        uint8x16_t k2 = vdupq_n_u8((uint8_t)(w3_color >> 16));
        uint8x16_t k3 = vdupq_n_u8((uint8_t)(w3_color >> 24));
        const uint8x16_t m7f = vdupq_n_u8(0x7f);
        const unsigned char *fu = src, *fa = factor;

    unsigned char *dp = dst_pair, *di = dst_pair + RECON_ROW_STEP / 2u;

        for (int g = 0; g < (int)RECON_3D_GROUPS; g++) {
            uint8x16x4_t v = vld4q_u8(fu); fu += 64;
            uint8x16_t f = vld1q_u8(fa);   fa += 16;
            uint8x16_t eq = vceqq_u8(f, m7f);
            uint8x16_t fn = vreinterpretq_u8_s8(
                                vnegq_s8(vreinterpretq_s8_u8(f)));
            uint8x16_t pu = vcgtq_u8(v.val[3], m7f);
            fn = vaddq_u8(fn, eq);
            v.val[3] = vandq_u8(v.val[3], m7f);
            fn = vandq_u8(fn, pu);
            int8x16_t fs = vreinterpretq_s8_u8(fn);
            uint8x16_t d[4];
            d[0] = vsubq_u8(v.val[0], k0);
            d[1] = vsubq_u8(v.val[1], k1);
            d[2] = vsubq_u8(v.val[2], k2);
            d[3] = vsubq_u8(v.val[3], k3);
            for (int q = 0; q < 4; q++) {
                int8x16_t ds = vreinterpretq_s8_u8(d[q]);
                uint16x8_t lo = vreinterpretq_u16_s16(
                        vmull_s8(vget_low_s8(ds), vget_low_s8(fs)));
                uint16x8_t hi = vreinterpretq_u16_s16(
                        vmull_high_s8(ds, fs));
                uint8x16_t r = vshrn_high_n_u16(vshrn_n_u16(lo, 7), hi, 7);
                v.val[q] = vaddq_u8(v.val[q], r);
            }
            uint8x8x4_t pair, odd;
            for (int q = 0; q < 4; q++) {
                uint16x8_t h = vreinterpretq_u16_u8(v.val[q]);
                pair.val[q]   = vmovn_u16(h);
                odd.val[q] = vshrn_n_u16(h, 8);
            }
            vst4_u8(dp, pair);   dp += 32;
            vst4_u8(di, odd); di += 32;
        }
        return;
    }
#else

    unsigned char *dst_odd = dst_pair + RECON_ROW_STEP / 2u;

    uint8_t color[4];
    color[0] = (uint8_t)(w3_color & 0xffu);
    color[1] = (uint8_t)((w3_color >> 8) & 0xffu);
    color[2] = (uint8_t)((w3_color >> 16) & 0xffu);
    color[3] = (uint8_t)((w3_color >> 24) & 0xffu);

    unsigned int steps = RECON_3D_WIDTH;

    while (steps != 0)
    {
        unsigned char plane[4][16];
        int p, j;

        for (j = 0; j < 16; j++)
            for (p = 0; p < 4; p++)
                plane[p][j] = src[j * 4 + p];
        src += 64;

        unsigned char f[16];
        for (j = 0; j < 16; j++)
            f[j] = factor[j];
        factor += 16;

        for (j = 0; j < 16; j++)
        {
            uint8_t f0 = f[j];
            uint8_t eq = (f0 == 0x7fu) ? 0xffu : 0x00u;
            uint8_t fj = (uint8_t)((uint8_t)(-(int)f0) + eq);

            uint8_t p3_raw = plane[3][j];
            uint8_t gate = (p3_raw > 0x7fu) ? 0xffu : 0x00u;
            fj = (uint8_t)(fj & gate);

            uint8_t p3 = (uint8_t)(p3_raw & 0x7fu);
            plane[3][j] = p3;

            for (p = 0; p < 4; p++)
            {
                uint8_t v = plane[p][j];
                int8_t d = (int8_t)(uint8_t)(v - color[p]);
                int8_t g = (int8_t)fj;
                int16_t prod = (int16_t)((int32_t)d * (int32_t)g);
                uint8_t shr7 = (uint8_t)(((uint16_t)prod) >> 7);
                plane[p][j] = (uint8_t)(v + shr7);
            }
        }

        for (j = 0; j < 8; j++)
        {
            for (p = 0; p < 4; p++)
            {
                dst_pair[j * 4 + p]   = plane[p][j * 2];
                dst_odd[j * 4 + p] = plane[p][j * 2 + 1];
            }
        }
        dst_pair += 32;
        dst_odd += 32;

        steps -= 0x10;
    }
#endif
}

void gpu3d_raster_fog_blend_alpha_line(unsigned char *dst, const unsigned char *src,
                         const unsigned char *tbl, uint32_t w3) {

    unsigned char cVar1 = (unsigned char)(w3 >> 24);

    for (int n = 0; n < (int)RECON_3D_WIDTH; n++) {
        unsigned char b0 = src[n * 4 + 0];
        unsigned char b1 = src[n * 4 + 1];
        unsigned char b2 = src[n * 4 + 2];
        unsigned char d  = src[n * 4 + 3];
        unsigned char s  = tbl[n];

        unsigned char eq_mask = (s == 0x7f) ? 0xff : 0x00;

        unsigned char neg_s = (unsigned char)(0 - s);
        unsigned char big_s = (unsigned char)(neg_s + eq_mask);

        unsigned char sign_mask = (d & 0x80u) ? 0xff : 0x00;

        unsigned char d_masked = (unsigned char)(d & 0x7fu);

        unsigned char s_eff = (unsigned char)(big_s & sign_mask);

        unsigned char diff = (unsigned char)(d_masked - cVar1);

        int32_t prod = (int32_t)(int8_t)diff * (int32_t)(int8_t)s_eff;

        uint16_t bits16 = (uint16_t)(int16_t)prod;
        unsigned char shr = (unsigned char)(bits16 >> 7);

        unsigned char out_d = (unsigned char)(d_masked + shr);

        dst[n * 4 + 0] = b0;
        dst[n * 4 + 1] = b1;
        dst[n * 4 + 2] = b2;
        dst[n * 4 + 3] = out_d;
    }
}

void gpu3d_raster_fog_blend_alpha_split_parity(unsigned char *dst, const unsigned char *src,
                         const unsigned char *table, uint32_t param4) {

    uint8_t level_global = (uint8_t)(param4 >> 24);

    for (unsigned int turn = 0; turn < RECON_3D_WIDTH / 32u; turn++) {
        uint8_t pixels[128];
        uint8_t levels[32];

        __builtin_memcpy(pixels, src + turn * 128, sizeof(pixels));
        __builtin_memcpy(levels, table + turn * 32, sizeof(levels));

        uint8_t output[32];
        for (unsigned int group = 0; group < 32u; group++) {
            uint8_t d_raw = pixels[group * 4 + 3];
            uint8_t level = levels[group];
            uint8_t equal_7f = (level == 0x7f) ? 0xff : 0;
            uint8_t factor = (uint8_t)((uint8_t)(0 - level) + equal_7f);
            uint8_t mask_sign = (d_raw > 0x7f) ? 0xff : 0;
            uint8_t d = (uint8_t)(d_raw & 0x7f);

            factor = (uint8_t)(factor & mask_sign);
            uint8_t difference = (uint8_t)(d - level_global);
            int16_t product = (int16_t)((int32_t)(int8_t)difference *
                                         (int32_t)(int8_t)factor);
            uint8_t correction = (uint8_t)((uint16_t)product >> 7);
            output[group] = (uint8_t)(d + correction);
        }

        for (unsigned int lane = 0; lane < 16; lane++) {

            unsigned int group_pair = (lane < 8)
                ? lane * 2 : 16 + (lane - 8) * 2;
            uint8_t *out_pair = dst + turn * 64 + lane * 4;

            __builtin_memcpy(out_pair, pixels + group_pair * 4, 3);
            out_pair[3] = output[group_pair];

            unsigned int group_odd = (lane < 8)
                ? lane * 2 + 1 : 17 + (lane - 8) * 2;
            uint8_t *out_odd = dst + RECON_ROW_STEP / 2u + turn * 64 + lane * 4;

            __builtin_memcpy(out_odd, pixels + group_odd * 4, 3);
            out_odd[3] = output[group_odd];
        }
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_palette_blit_deinterleave_512(unsigned char *dst, const unsigned char *src,
                         const unsigned char *idx, const unsigned char *pal) {

    unsigned char t0[16], t1[16], t2[16];
    for (int i = 0; i < 8; i++) {
        t0[2 * i] = pal[i];      t0[2 * i + 1] = 0;
        t1[2 * i] = pal[8 + i];  t1[2 * i + 1] = 0;
        t2[2 * i] = pal[16 + i]; t2[2 * i + 1] = 0;
    }

    unsigned char *dst_pair = dst;

    unsigned char *dst_odd = dst + RECON_ROW_STEP / 2u;

#ifdef __ARM_NEON

    {
        const uint8x16_t T0 = vld1q_u8(t0), T1 = vld1q_u8(t1), T2 = vld1q_u8(t2);
        const uint8x16_t M1F = vdupq_n_u8(0x1f), ME0 = vdupq_n_u8(0xe0);
        unsigned char *dp = dst_pair, *di = dst_odd;
        for (unsigned base = 0; base < RECON_3D_WIDTH; base += 32u) {
            uint8x16x4_t a0 = vld4q_u8(src + (size_t)base * 4u);
            uint8x16x4_t a1 = vld4q_u8(src + (size_t)base * 4u + 64u);
            uint8x16_t i0 = vld1q_u8(idx + base);
            uint8x16_t i1 = vld1q_u8(idx + base + 16u);

            uint8x16_t h0 = vshrq_n_u8(i0, 3), h1 = vshrq_n_u8(i1, 3);
            uint8x16_t k0 = vaddq_u8(i0, i0),  k1 = vaddq_u8(i1, i1);

            a0.val[0] = vqtbx1q_u8(a0.val[0], T0, k0);
            a0.val[1] = vqtbx1q_u8(a0.val[1], T1, k0);
            a0.val[2] = vqtbx1q_u8(a0.val[2], T2, k0);
            a0.val[3] = vorrq_u8(vandq_u8(a0.val[3], M1F), vandq_u8(h0, ME0));
            a1.val[0] = vqtbx1q_u8(a1.val[0], T0, k1);
            a1.val[1] = vqtbx1q_u8(a1.val[1], T1, k1);
            a1.val[2] = vqtbx1q_u8(a1.val[2], T2, k1);
            a1.val[3] = vorrq_u8(vandq_u8(a1.val[3], M1F), vandq_u8(h1, ME0));

            uint8x16x4_t pair, odd;
            for (int c = 0; c < 4; c++) {
                pair.val[c]   = vuzp1q_u8(a0.val[c], a1.val[c]);
                odd.val[c] = vuzp2q_u8(a0.val[c], a1.val[c]);
            }
            vst4q_u8(dp, pair);    dp += 64;
            vst4q_u8(di, odd);  di += 64;
        }
        return;
    }
#endif
    for (int base = 0; base < (int)RECON_3D_WIDTH; base += 32) {
        unsigned char pix[128];
        memcpy(pix, src + (size_t)base * 4, 128);

        for (int j = 0; j < 32; j++) {
            unsigned char idxv = idx[base + j];
            unsigned char height = (unsigned char)(idxv >> 3);
            unsigned char k = (unsigned char)(idxv * 2);
            if (k < 16) {
                pix[j * 4 + 0] = t0[k];
                pix[j * 4 + 1] = t1[k];
                pix[j * 4 + 2] = t2[k];
            }
            pix[j * 4 + 3] = (unsigned char)((pix[j * 4 + 3] & 0x1f) | (height & 0xe0));
        }

        unsigned char pair[64], odd[64];
        for (int j = 0; j < 16; j++) {
            memcpy(pair + j * 4, pix + (2 * j) * 4, 4);
            memcpy(odd + j * 4, pix + (2 * j + 1) * 4, 4);
        }
        memcpy(dst_pair, pair, 64);
        memcpy(dst_odd, odd, 64);
        dst_pair += 64;
        dst_odd += 64;
    }
}

#include <stdint.h>
#include "hires_runtime.h"
#include "../gpu3d.h"
#include <math.h>
#include <string.h>
#include "mem_access.h"

void gpu3d_raster_build_reciprocal_shift_table(uint32_t *param_1, uint32_t *param_2,
                         int32_t *param_3, uint32_t param_4) {

    if (param_4 == 0)
        return;

    uint32_t remaining = param_4;
    int32_t v = *param_3;

    for (;;) {
        uint32_t shift;
        uint32_t recip;

        if (v == 0) {
            shift = 0x20;
            recip = 1;
        } else {
            shift = (uint32_t)__builtin_clz((uint32_t)v);
            uint64_t v_norm = (uint64_t)(uint32_t)((uint32_t)v << (shift & 0x1f));
            uint64_t r64 = 0;
            if (v_norm != 0)
                r64 = ((v_norm | 0x4000000000000000ULL) - 1) / v_norm;
            recip = (uint32_t)r64;
        }

        *param_1++ = recip;
        *param_2++ = shift;

        remaining--;
        param_3++;
        if (remaining == 0)
            return;

        v = *param_3;
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_scale_vertex_row(int32_t *dst, unsigned char *params,
                        const int16_t *src, uint32_t rows) {

    const uint16_t *len = (const uint16_t *)(params + 9u * RECON_3D_ROW_ARRAY);
    const int32_t *p = (const int32_t *)params;

    do {
        int32_t n = (int32_t)*len;  len += 2;

        int32_t scale = p[RECON_3D_ROW_ARRAY / 4u];
        int32_t shift = *p++;

#ifdef __ARM_NEON

        const int32x2_t wr = vdup_n_s32(scale);
        const int32x4_t vshift = vdupq_n_s32(shift);
        do {
            int16x8_t s = vld1q_s16(src);
            int32x4_t s_lo = vmovl_s16(vget_low_s16(s));
            int32x4_t s_hi = vmovl_s16(vget_high_s16(s));
            int64x2_t q0 = vmull_s32(vget_low_s32(s_lo),  wr);
            int64x2_t q1 = vmull_s32(vget_high_s32(s_lo), wr);
            int64x2_t q2 = vmull_s32(vget_low_s32(s_hi),  wr);
            int64x2_t q3 = vmull_s32(vget_high_s32(s_hi), wr);
            vst1q_s32(dst,     vaddq_s32(vcombine_s32(vshrn_n_s64(q0, 15),
                                                      vshrn_n_s64(q1, 15)), vshift));
            vst1q_s32(dst + 4, vaddq_s32(vcombine_s32(vshrn_n_s64(q2, 15),
                                                      vshrn_n_s64(q3, 15)), vshift));
            src += 8;
            dst += 8;
            n -= 8;
        } while (n > 0);
#else
        do {
            for (int j = 0; j < 8; j++) {
                int64_t prod = (int64_t)scale * (int64_t)src[j];
                dst[j] = (int32_t)(prod >> 15) + shift;
            }
            src += 8;
            dst += 8;
            n -= 8;
        } while (n > 0);
#endif

        src += n;
        dst += n;
    } while (--rows != 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#include "raster.h"
#endif

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

void gpu3d_raster_divide_perspective_fixed(int16_t *dst, const float *src, int32_t n) {
#ifdef __ARM_NEON

    do {
        float32x4x2_t p0 = vld2q_f32(src);
        float32x4x2_t p1 = vld2q_f32(src + 8);
        float32x4_t r0 = vrecpeq_f32(p0.val[1]);
        float32x4_t r1 = vrecpeq_f32(p1.val[1]);
        r0 = vmulq_f32(r0, vrecpsq_f32(r0, p0.val[1]));
        r1 = vmulq_f32(r1, vrecpsq_f32(r1, p1.val[1]));
        r0 = vmulq_f32(r0, vrecpsq_f32(r0, p0.val[1]));
        r1 = vmulq_f32(r1, vrecpsq_f32(r1, p1.val[1]));
        int32x4_t q0 = vcvtq_n_s32_f32(vmulq_f32(p0.val[0], r0), 15);
        int32x4_t q1 = vcvtq_n_s32_f32(vmulq_f32(p1.val[0], r1), 15);
        vst1q_s16(dst, vcombine_s16(vmovn_s32(q0), vmovn_s32(q1)));
        src += 16;
        dst += 8;
        n -= 8;
    } while (n > 0);
#else
    do {
        int16_t t[8];
        for (int k = 0; k < 8; k++) {
            float a = src[k * 2];
            float b = src[k * 2 + 1];

            float r = frecpe(b);
            r = r * frecps(r, b);
            r = r * frecps(r, b);
            float q = a * r;
            int32_t fx = (int32_t)(q * 32768.0f);
            t[k] = (int16_t)fx;
        }
        for (int k = 0; k < 8; k++) dst[k] = t[k];
        src += 16;
        dst += 8;
        n -= 8;
    } while (n > 0);
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void gpu3d_raster_interp_lerp_rows(const gpu3d_bank_vertex_t *const *param_1, int32_t *param_2, const int16_t *param_3,
                         const uint8_t *param_4, int32_t param_5)
{

    int32_t rows = param_5;

    do {

        const gpu3d_bank_vertex_t *ptr_start = param_1[0];
        const gpu3d_bank_vertex_t *ptr_end = param_1[1];
        param_1 += 2;

        int32_t start = ptr_start->w;
        int32_t end = ptr_end->w;
        int32_t delta = (int32_t)((uint32_t)end - (uint32_t)start);

        int32_t remainder = (int32_t)(uint32_t)*param_4++;

#ifdef __ARM_NEON

        {
            const int32x4_t vd = vdupq_n_s32(delta), vi = vdupq_n_s32(start);
            do {
                int16x8_t c = vld1q_s16(param_3);
                int32x4_t c0 = vmovl_s16(vget_low_s16(c)), c1 = vmovl_high_s16(c);
                int32x4_t r0 = vcombine_s32(vshrn_n_s64(vmull_s32(vget_low_s32(vd), vget_low_s32(c0)), 15),
                                            vshrn_n_s64(vmull_high_s32(vd, c0), 15));
                int32x4_t r1 = vcombine_s32(vshrn_n_s64(vmull_s32(vget_low_s32(vd), vget_low_s32(c1)), 15),
                                            vshrn_n_s64(vmull_high_s32(vd, c1), 15));
                vst1q_s32(param_2,     vaddq_s32(r0, vi));
                vst1q_s32(param_2 + 4, vaddq_s32(r1, vi));
                param_3 += 8;
                param_2 += 8;
                remainder -= 8;
            } while (remainder > 0);
        }
#else
        do {
            for (int j = 0; j < 8; j++) {
                int16_t coef;
                memcpy(&coef, param_3 + j, sizeof(coef));
                int64_t prod = (int64_t)delta * (int64_t)coef;
                uint32_t shifted = (uint32_t)((uint64_t)prod >> 15);
                uint32_t val = shifted + (uint32_t)start;
                int32_t val_i32 = (int32_t)val;
                memcpy(param_2 + j, &val_i32, sizeof(val_i32));
            }
            param_3 += 8;
            param_2 += 8;
            remainder -= 8;
        } while (remainder > 0);
#endif

        param_3 += remainder;
        param_2 += remainder;

        rows -= 1;
    } while (rows != 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void gpu3d_raster_interp_five_ramps_rows(const gpu3d_bank_vertex_t *const *param_1, uint8_t *param_2,
                         const int16_t *param_3, const uint8_t *param_4,
                         int32_t param_5)
{

    uint16_t v26_in[8];
    __asm__ volatile("str q26, %0" : "=m"(v26_in));

    uint8_t *out_pos    = param_2 + 4u * RECON_3D_ROW_ARRAY;
    uint8_t *out_codeAB = param_2 + 6u * RECON_3D_ROW_ARRAY;
    uint8_t *out_codeC  = param_2 + 8u * RECON_3D_ROW_ARRAY;

    int32_t rows = param_5;
    do {

        const gpu3d_bank_vertex_t *structA = param_1[0];
        const gpu3d_bank_vertex_t *structB = param_1[1];
        param_1 += 2;

        int32_t remainder = (int32_t)(uint32_t)*param_4++;

        uint16_t codeA = structA->color;
        uint16_t codeB = structB->color;

        uint32_t posA_raw = (uint32_t)structA->s | ((uint32_t)structA->t << 16);
        uint32_t posB_raw = (uint32_t)structB->s | ((uint32_t)structB->t << 16);

        int16_t posA_lo = (int16_t)(uint16_t)(posA_raw);
        int16_t posA_hi = (int16_t)(uint16_t)(posA_raw >> 16);
        int16_t posB_lo = (int16_t)(uint16_t)(posB_raw);
        int16_t posB_hi = (int16_t)(uint16_t)(posB_raw >> 16);

        int16_t deltaLo = (int16_t)((uint16_t)posB_lo - (uint16_t)posA_lo);
        int16_t deltaHi = (int16_t)((uint16_t)posB_hi - (uint16_t)posA_hi);

        uint32_t rndLo = (deltaLo > 0) ? 0x0800u : 0u;
        uint32_t rndHi = (deltaHi > 0) ? 0x0800u : 0u;

        uint32_t baseLo = ((uint32_t)(int32_t)posA_lo << 15) + rndLo;
        uint32_t baseHi = ((uint32_t)(int32_t)posA_hi << 15) + rndHi;

        uint32_t rawA0 = (uint32_t)codeA & 0x1Fu;
        uint32_t rawA1 = ((uint32_t)codeA >> 5) & 0x1Fu;
        uint32_t rawA2 = ((uint32_t)codeA >> 10) & 0x1Fu;
        uint32_t rawB0 = (uint32_t)codeB & 0x1Fu;
        uint32_t rawB1 = ((uint32_t)codeB >> 5) & 0x1Fu;
        uint32_t rawB2 = ((uint32_t)codeB >> 10) & 0x1Fu;

        int16_t tA0 = (int16_t)(2u * rawA0 + (rawA0 != 0));
        int16_t tA1 = (int16_t)(2u * rawA1 + (rawA1 != 0));
        int16_t tA2 = (int16_t)(2u * rawA2 + (rawA2 != 0));
        int16_t tB0 = (int16_t)(2u * rawB0 + (rawB0 != 0));
        int16_t tB1 = (int16_t)(2u * rawB1 + (rawB1 != 0));
        int16_t tB2 = (int16_t)(2u * rawB2 + (rawB2 != 0));

        int16_t slope0 = (int16_t)((int16_t)(tB0 - tA0) * 8);
        int16_t slope1 = (int16_t)((int16_t)(tB1 - tA1) * 8);
        int16_t slope2 = (int16_t)((int16_t)(tB2 - tA2) * 8);

        uint32_t base0 = ((uint32_t)(uint16_t)tA0 << 15 << 3) + RASTER_TEXCOORD_BIAS;
        uint32_t base1 = ((uint32_t)(uint16_t)tA1 << 15 << 3) + RASTER_TEXCOORD_BIAS;
        uint32_t base2 = ((uint32_t)(uint16_t)tA2 << 15 << 3) + RASTER_TEXCOORD_BIAS;

#ifdef __ARM_NEON

        {
            const int32x4_t vbLo = vdupq_n_s32((int32_t)baseLo), vbHi = vdupq_n_s32((int32_t)baseHi);
            const int32x4_t vb0 = vdupq_n_s32((int32_t)base0), vb1 = vdupq_n_s32((int32_t)base1),
                            vb2 = vdupq_n_s32((int32_t)base2);
            const int16x4_t v26 = vld1_s16((const int16_t *)v26_in);
            do {
                int16x4_t coef = vld1_s16(param_3);
                int16x4x2_t o;
                o.val[0] = vshrn_n_s32(vmlal_n_s16(vbLo, coef, deltaLo), 15);
                o.val[1] = vshrn_n_s32(vmlal_n_s16(vbHi, coef, deltaHi), 15);
                vst2_s16((int16_t *)out_pos, o);
                o.val[0] = vshrn_n_s32(vmlal_n_s16(vb0, coef, slope0), 15);
                o.val[1] = vshrn_n_s32(vmlal_n_s16(vb1, coef, slope1), 15);
                vst2_s16((int16_t *)out_codeAB, o);
                o.val[0] = v26;
                o.val[1] = vshrn_n_s32(vmlal_n_s16(vb2, coef, slope2), 15);
                vst2_s16((int16_t *)out_codeC, o);
                param_3    += 4;
                out_pos    += 16;
                out_codeAB += 16;
                out_codeC  += 16;
                remainder  -= 4;
            } while (remainder > 0);
        }
#else
        do {
            for (int i = 0; i < 4; i++) {
                int16_t coef;
                memcpy(&coef, param_3 + i, sizeof(coef));

                uint32_t accPos0 = baseLo + (uint32_t)((int32_t)coef * (int32_t)deltaLo);
                uint32_t accPos1 = baseHi + (uint32_t)((int32_t)coef * (int32_t)deltaHi);
                uint32_t accC0   = base0  + (uint32_t)((int32_t)coef * (int32_t)slope0);
                uint32_t accC1   = base1  + (uint32_t)((int32_t)coef * (int32_t)slope1);
                uint32_t accC2   = base2  + (uint32_t)((int32_t)coef * (int32_t)slope2);

                int16_t outPos0 = (int16_t)(uint16_t)(accPos0 >> 15);
                int16_t outPos1 = (int16_t)(uint16_t)(accPos1 >> 15);
                int16_t outC0   = (int16_t)(uint16_t)(accC0 >> 15);
                int16_t outC1   = (int16_t)(uint16_t)(accC1 >> 15);
                int16_t outC2   = (int16_t)(uint16_t)(accC2 >> 15);

                memcpy(out_pos    + 4 * i + 0, &outPos0, 2);
                memcpy(out_pos    + 4 * i + 2, &outPos1, 2);
                memcpy(out_codeAB + 4 * i + 0, &outC0,   2);
                memcpy(out_codeAB + 4 * i + 2, &outC1,   2);

                memcpy(out_codeC  + 4 * i + 0, &v26_in[i], 2);
                memcpy(out_codeC  + 4 * i + 2, &outC2,     2);
            }
            param_3    += 4;
            out_pos    += 16;
            out_codeAB += 16;
            out_codeC  += 16;
            remainder  -= 4;
        } while (remainder > 0);
#endif

        param_3    += remainder;
        out_pos    += remainder * 4;
        out_codeAB += remainder * 4;
        out_codeC  += remainder * 4;

        rows -= 1;
    } while (rows != 0);
}

#ifndef __ARM_NEON
typedef struct {
    unsigned char code[16];
    unsigned char plus[16];
    unsigned char pri[16];
    unsigned char s2[4][16];
    unsigned char d0[4][16];
    unsigned char s1[4][16];
    unsigned char d1[4][16];
} LanesD3C8;

static void read4(const unsigned char *p, unsigned char d[4][16]) {
    for (unsigned i = 0; i < 16; i++)
        for (unsigned c = 0; c < 4; c++)
            d[c][i] = p[4 * i + c];
}

static void write4(unsigned char *p, const unsigned char d[4][16]) {
    for (unsigned i = 0; i < 16; i++)
        for (unsigned c = 0; c < 4; c++)
            p[4 * i + c] = d[c][i];
}
#endif

static void kernel_d3c8(unsigned char *cap0, unsigned char *cap1_o_null,
                        uint32_t color, const unsigned char *org0,
                        unsigned char *org1_o_cap, unsigned char *prio,
                        uint32_t c5, const unsigned char *code,
                        const unsigned char *plus, int32_t n,
                        int family_b, int group_with_m, int with80)
{
    unsigned char k5 = (unsigned char)c5;

    unsigned char col[4];
    col[0] = (unsigned char)(color);
    col[1] = (unsigned char)(color >> 8);
    col[2] = (unsigned char)(color >> 16);
    col[3] = (unsigned char)((unsigned char)(color >> 24) | k5);

#ifdef __ARM_NEON

    const uint8x16_t v1f = vdupq_n_u8(0x1f);
    const uint8x16_t vk5 = vdupq_n_u8(k5);
    const uint8x16_t v80 = vdupq_n_u8(0x80);
    uint8x16x4_t vcol;
    vcol.val[0] = vdupq_n_u8(col[0]); vcol.val[1] = vdupq_n_u8(col[1]);
    vcol.val[2] = vdupq_n_u8(col[2]); vcol.val[3] = vdupq_n_u8(col[3]);
    do {
        uint8x16_t vcode = vld1q_u8(code);
        uint8x16_t vm   = vld1q_u8(plus);
        uint8x16_t eq   = vceqq_u8(vcode, v1f);
        uint8x16_t keep = vbicq_u8(vm, eq);
        uint8x16_t both = vandq_u8(vm, eq);
        uint8x16x4_t s2 = vld4q_u8(org0);
        uint8x16x4_t d0 = vld4q_u8(cap0);
        uint8x16x4_t s1, d1;
        if (family_b) {
            d1 = vld4q_u8(org1_o_cap);
            s1 = vcol;
        } else {
            s1 = vld4q_u8(org1_o_cap);
            d1 = vld4q_u8(cap1_o_null);
        }
        uint8x16_t vpri = vld1q_u8(prio);
        uint8x16_t m3 = vm;
        if (with80) {
            d0.val[3] = vorrq_u8(d0.val[3], v80);
            if (!family_b)
                m3 = vandq_u8(vm, vornq_u8(eq, v80));
        }
        if (!family_b)
            d1.val[3] = vorrq_u8(d1.val[3], vk5);
        uint8x16_t M = group_with_m ? vm : both;
        uint8x16x4_t o0, o1;
        o0.val[0] = vbslq_u8(vm, d0.val[0], s2.val[0]);
        o0.val[1] = vbslq_u8(vm, d0.val[1], s2.val[1]);
        o0.val[2] = vbslq_u8(vm, d0.val[2], s2.val[2]);
        o0.val[3] = vbslq_u8(m3, d0.val[3], s2.val[3]);
        uint8x16_t o4 = vbslq_u8(keep, vk5, vpri);
        if (family_b) {
            o1.val[0] = vbslq_u8(M, s1.val[0], d1.val[0]);
            o1.val[1] = vbslq_u8(M, s1.val[1], d1.val[1]);
            o1.val[2] = vbslq_u8(M, s1.val[2], d1.val[2]);
            o1.val[3] = vbslq_u8(both, s1.val[3], d1.val[3]);
        } else {
            o1.val[0] = vbslq_u8(M, d1.val[0], s1.val[0]);
            o1.val[1] = vbslq_u8(M, d1.val[1], s1.val[1]);
            o1.val[2] = vbslq_u8(M, d1.val[2], s1.val[2]);
            o1.val[3] = vbslq_u8(both, d1.val[3], s1.val[3]);
        }
        vst4q_u8(cap0, o0);
        vst1q_u8(prio, o4);
        vst4q_u8(family_b ? org1_o_cap : cap1_o_null, o1);

        cap0 += 64;
        org0 += 64;
        if (family_b) {
            org1_o_cap += 64;
        } else {
            cap1_o_null += 64;
            org1_o_cap  += 64;
        }
        prio += 16;
        code  += 16;
        plus  += 16;
        n    -= 16;
    } while (n > 0);
#else
    do {
        LanesD3C8 L;
        unsigned char o0[4][16], o1[4][16], o4[16];

        for (unsigned i = 0; i < 16; i++) {
            L.code[i] = code[i];
            L.plus[i] = plus[i];
            L.pri[i] = prio[i];
        }
        read4(org0, L.s2);
        read4(cap0, L.d0);
        if (family_b) {
            read4(org1_o_cap, L.d1);
        } else {
            read4(org1_o_cap, L.s1);
            read4(cap1_o_null, L.d1);
        }

        for (unsigned i = 0; i < 16; i++) {
            unsigned char m    = L.plus[i];
            unsigned char eq   = (L.code[i] == 0x1fu) ? 0xffu : 0x00u;
            unsigned char keep = (unsigned char)(m & (unsigned char)~eq);
            unsigned char both = (unsigned char)(m & eq);
            unsigned char M    = group_with_m ? m : both;
            unsigned char d03  = L.d0[3][i];
            unsigned char m3   = m;

            if (with80) {
                d03 = (unsigned char)(d03 | 0x80u);

                if (!family_b)
                    m3 = (unsigned char)(m & (unsigned char)(eq | 0x7fu));
            }

            for (unsigned c = 0; c < 3; c++)
                o0[c][i] = (unsigned char)((L.s2[c][i] & (unsigned char)~m)
                                           | (L.d0[c][i] & m));
            o0[3][i] = (unsigned char)((L.s2[3][i] & (unsigned char)~m3)
                                       | (d03 & m3));

            o4[i] = (unsigned char)((L.pri[i] & (unsigned char)~keep)
                                    | (k5 & keep));

            if (family_b) {

                for (unsigned c = 0; c < 3; c++)
                    o1[c][i] = (unsigned char)((L.d1[c][i] & (unsigned char)~M)
                                               | (col[c] & M));
                o1[3][i] = (unsigned char)((L.d1[3][i] & (unsigned char)~both)
                                           | (col[3] & both));
            } else {
                unsigned char d13 = (unsigned char)(L.d1[3][i] | k5);
                for (unsigned c = 0; c < 3; c++)
                    o1[c][i] = (unsigned char)((L.s1[c][i] & (unsigned char)~M)
                                               | (L.d1[c][i] & M));
                o1[3][i] = (unsigned char)((L.s1[3][i] & (unsigned char)~both)
                                           | (d13 & both));
            }
        }

        write4(cap0, o0);
        for (unsigned i = 0; i < 16; i++) prio[i] = o4[i];
        write4(family_b ? org1_o_cap : cap1_o_null, o1);

        cap0 += 64;
        org0 += 64;
        if (family_b) {
            org1_o_cap += 64;
        } else {
            cap1_o_null += 64;
            org1_o_cap  += 64;
        }
        prio += 16;
        code  += 16;
        plus  += 16;
        n    -= 16;
    } while (n > 0);
#endif
}

void gpu3d_raster_interp_five_ramps_rows_d50c(unsigned char *cap0, unsigned char *cap1,
                             const unsigned char *org0, unsigned char *org1,
                             unsigned char *prio, uint32_t c5,
                             const unsigned char *code, const unsigned char *plus,
                             int32_t n) {
    kernel_d3c8(cap0, cap1, 0, org0, org1, prio, c5, code, plus, n, 0, 0, 0);
}

void gpu3d_raster_interp_five_ramps_rows_d580(unsigned char *cap0, unsigned char *cap1,
                             const unsigned char *org0, unsigned char *org1,
                             unsigned char *prio, uint32_t c5,
                             const unsigned char *code, const unsigned char *plus,
                             int32_t n) {
    kernel_d3c8(cap0, cap1, 0, org0, org1, prio, c5, code, plus, n, 0, 1, 0);
}

void gpu3d_raster_interp_five_ramps_rows_d5f8(unsigned char *cap0, unsigned char *cap1,
                             const unsigned char *org0, unsigned char *org1,
                             unsigned char *prio, uint32_t c5,
                             const unsigned char *code, const unsigned char *plus,
                             int32_t n) {
    kernel_d3c8(cap0, cap1, 0, org0, org1, prio, c5, code, plus, n, 0, 0, 1);
}

void gpu3d_raster_interp_five_ramps_rows_d688(unsigned char *cap0, unsigned char *cap1,
                             const unsigned char *org0, unsigned char *org1,
                             unsigned char *prio, uint32_t c5,
                             const unsigned char *code, const unsigned char *plus,
                             int32_t n) {
    kernel_d3c8(cap0, cap1, 0, org0, org1, prio, c5, code, plus, n, 0, 1, 1);
}

void gpu3d_raster_interp_five_ramps_rows_d718(unsigned char *cap0, uint32_t color,
                             const unsigned char *org0, unsigned char *cap1,
                             unsigned char *prio, uint32_t c5,
                             const unsigned char *code, const unsigned char *plus,
                             int32_t n) {
    kernel_d3c8(cap0, 0, color, org0, cap1, prio, c5, code, plus, n, 1, 0, 0);
}

void gpu3d_raster_interp_five_ramps_rows_d7a8(unsigned char *cap0, uint32_t color,
                             const unsigned char *org0, unsigned char *cap1,
                             unsigned char *prio, uint32_t c5,
                             const unsigned char *code, const unsigned char *plus,
                             int32_t n) {
    kernel_d3c8(cap0, 0, color, org0, cap1, prio, c5, code, plus, n, 1, 1, 0);
}

void gpu3d_raster_interp_five_ramps_rows_d838(unsigned char *cap0, uint32_t color,
                             const unsigned char *org0, unsigned char *cap1,
                             unsigned char *prio, uint32_t c5,
                             const unsigned char *code, const unsigned char *plus,
                             int32_t n) {
    kernel_d3c8(cap0, 0, color, org0, cap1, prio, c5, code, plus, n, 1, 0, 1);
}

void gpu3d_raster_interp_five_ramps_rows_d8e0(unsigned char *cap0, uint32_t color,
                             const unsigned char *org0, unsigned char *cap1,
                             unsigned char *prio, uint32_t c5,
                             const unsigned char *code, const unsigned char *plus,
                             int32_t n) {
    kernel_d3c8(cap0, 0, color, org0, cap1, prio, c5, code, plus, n, 1, 1, 1);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#define PER_ROUND 16

#ifndef __ARM_NEON
static uint32_t zeros(uint32_t v) {
    uint32_t n = 0;
    if (v == 0) return 32;
    while (!(v & 0x80000000u)) { v <<= 1; n++; }
    return n;
}

static uint32_t shl_sat(uint32_t v, uint32_t s) {
    return (s >= 32) ? 0u : (v << s);
}

static uint32_t urecpe(uint32_t x) {
    if (!(x & 0x80000000u)) return 0xffffffffu;
    uint32_t q = x >> 23;
    uint32_t b = (1u << 19) / (q * 2u + 1u);
    return ((b + 1u) / 2u) << 23;
}
#endif

void gpu3d_raster_compute_reciprocal_block(uint32_t *recip, uint32_t *zeros_out,
                        const uint32_t *ent, uint32_t n)
{
#ifdef __ARM_NEON

#define R630_MULSHR(A, B) vcombine_u32(vshrn_n_u64(vmull_u32(vget_low_u32(A), vget_low_u32(B)), 31),                                        vshrn_n_u64(vmull_high_u32(A, B), 31))
#define R630_NEG(T) vreinterpretq_u32_s32(vnegq_s32(vreinterpretq_s32_u32(T)))
    do {
        uint32x4_t x0 = vld1q_u32(ent), x1 = vld1q_u32(ent + 4),
                   x2 = vld1q_u32(ent + 8), x3 = vld1q_u32(ent + 12);
        uint32x4_t c0 = vclzq_u32(x0), c1 = vclzq_u32(x1),
                   c2 = vclzq_u32(x2), c3 = vclzq_u32(x3);
        x0 = vshlq_u32(x0, vreinterpretq_s32_u32(c0));
        x1 = vshlq_u32(x1, vreinterpretq_s32_u32(c1));
        x2 = vshlq_u32(x2, vreinterpretq_s32_u32(c2));
        x3 = vshlq_u32(x3, vreinterpretq_s32_u32(c3));
        vst1q_u32(zeros_out, c0);      vst1q_u32(zeros_out + 4, c1);
        vst1q_u32(zeros_out + 8, c2);  vst1q_u32(zeros_out + 12, c3);
        uint32x4_t r0 = vshrq_n_u32(vrecpeq_u32(x0), 1), r1 = vshrq_n_u32(vrecpeq_u32(x1), 1),
                   r2 = vshrq_n_u32(vrecpeq_u32(x2), 1), r3 = vshrq_n_u32(vrecpeq_u32(x3), 1);
        uint32x4_t t0, t1, t2, t3;
        for (int k = 0; k < 3; k++) {
            t0 = R630_MULSHR(x0, r0); t1 = R630_MULSHR(x1, r1);
            t2 = R630_MULSHR(x2, r2); t3 = R630_MULSHR(x3, r3);
            t0 = R630_NEG(t0); t1 = R630_NEG(t1); t2 = R630_NEG(t2); t3 = R630_NEG(t3);
            r0 = R630_MULSHR(r0, t0); r1 = R630_MULSHR(r1, t1);
            r2 = R630_MULSHR(r2, t2); r3 = R630_MULSHR(r3, t3);
        }
        t0 = R630_MULSHR(x0, r0); t1 = R630_MULSHR(x1, r1);
        t2 = R630_MULSHR(x2, r2); t3 = R630_MULSHR(x3, r3);
        vst1q_u32(recip,      vsubq_u32(r0, vcgtzq_s32(vreinterpretq_s32_u32(t0))));
        vst1q_u32(recip + 4,  vsubq_u32(r1, vcgtzq_s32(vreinterpretq_s32_u32(t1))));
        vst1q_u32(recip + 8,  vsubq_u32(r2, vcgtzq_s32(vreinterpretq_s32_u32(t2))));
        vst1q_u32(recip + 12, vsubq_u32(r3, vcgtzq_s32(vreinterpretq_s32_u32(t3))));
        ent       += PER_ROUND;
        zeros_out += PER_ROUND;
        recip     += PER_ROUND;
        n -= PER_ROUND;
    } while ((int32_t)n > 0);
#undef R630_MULSHR
#undef R630_NEG
#else
    do {
        for (int i = 0; i < PER_ROUND; i++) {
            uint32_t v  = ent[i];
            uint32_t cz = zeros(v);
            uint32_t x  = shl_sat(v, cz);
            zeros_out[i] = cz;

            uint32_t r = urecpe(x) >> 1;

            for (int k = 0; k < 3; k++) {
                uint32_t t = (uint32_t)(((uint64_t)x * r) >> 31);
                r = (uint32_t)(((uint64_t)r * (uint64_t)(0u - t)) >> 31);
            }

            uint32_t t = (uint32_t)(((uint64_t)x * r) >> 31);
            if ((int32_t)t > 0) r += 1;

            recip[i] = r;
        }
        ent       += PER_ROUND;
        zeros_out += PER_ROUND;
        recip     += PER_ROUND;
        n -= PER_ROUND;
    } while ((int32_t)n > 0);
#endif
}
#undef PER_ROUND

#define BIAS    192
#define ROUNDING 0x7fff




static uint64_t ushl64(uint64_t v, int s) {
    if (s >= 0) return (s >= 64) ? 0u : (v << s);
    int r = -s;
    return (r >= 64) ? 0u : (v >> r);
}

void gpu3d_raster_advance_vertex_accumulators(gpu3d_t *ctx, const uint32_t *rec, const uint32_t *zeros) {

    uint32_t w4 = ctx->viewport_origin[1];
    uint32_t w5 = ctx->viewport_size[1];
    uint32_t w6 = ctx->viewport_size[0];
    uint32_t w7 = ctx->viewport_origin[0];

    w4 = BIAS - w4;
    w4 = w4 - w5;

    uint32_t m3, sh3;
    if (w5 & 0x3f) { m3 = w5;      sh3 = 15; }
    else           { m3 = w5 >> 6; sh3 = 9;  }

    int      uses_m2;
    uint32_t sh2;
    if (w6 == 0x100) { uses_m2 = 0; sh2 = 7;  }
    else             { uses_m2 = 1; sh2 = 15; }

    uint32_t n = ctx->emitted_count;

    uint8_t *pb = (uint8_t *)ctx->screen_x;
    uint8_t *pc = (uint8_t *)ctx->screen_y;
    uint8_t *pa = (uint8_t *)ctx->screen_z;
    uint8_t *pd = (uint8_t *)ctx->screen_w;

    do {
        for (int i = 0; i < 4; i++) {
            uint32_t e  = zeros[i];
            uint32_t rc = rec[i];
            uint32_t d  = rd32(pd + i * 4);
            uint32_t a  = rd32(pa + i * 4);
            uint32_t b  = rd32(pb + i * 4);
            uint32_t c  = rd32(pc + i * 4);

            int s = (int8_t)(uint8_t)(e - 48u);

            a = a + d;
            b = b + d;
            c = d - c;

            uint64_t p = (uint64_t)a * rc;
            p = p - ((p + ROUNDING) >> 15);
            p = ushl64(p, s);
            wr32(pa + i * 4, (uint32_t)p);

            uint32_t bb = uses_m2 ? (uint32_t)(b * w6) : b;
            uint64_t q = ushl64((uint64_t)bb * rc, s);
            wr32(pb + i * 4, (uint32_t)(q >> sh2) + w7);

            uint32_t cc = (uint32_t)(c * m3);
            uint64_t r = ushl64((uint64_t)cc * rc, s);
            wr32(pc + i * 4, (uint32_t)(r >> sh3) + w4);
        }
        rec   += 4;
        zeros += 4;
        pd += 16; pa += 16; pb += 16; pc += 16;
        n -= 4;
    } while ((int32_t)n > 0);
}
#undef BIAS
#undef ROUNDING

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#define BIAS    0xc0
#define ROUNDING 0x7fff




static uint64_t ushl64_7(uint64_t v, int s) {
    if (s >= 0) return (s >= 64) ? 0u : (v << s);
    int r = -s;
    return (r >= 64) ? 0u : (v >> r);
}

void gpu3d_raster_advance_vertex_accumulators_scaled(gpu3d_t *ctx, const uint32_t *rec, const uint32_t *zeros) {

    uint32_t w4raw = ctx->viewport_origin[1];
    uint32_t w5    = ctx->viewport_size[1];
    uint32_t w6    = ctx->viewport_size[0];
    uint32_t w7    = ctx->viewport_origin[0];

    uint32_t bias_c = BIAS - w4raw;
    bias_c = bias_c - w5;
    bias_c *= recon_scale_geometry;

    uint32_t bias_b = w7 * recon_scale_geometry;

    uint32_t m3, sh3;
    if (w5 & 0x3f) { m3 = w5;      sh3 = 15u; }
    else           { m3 = w5 >> 6; sh3 =  9u; }

    int      uses_m2;
    uint32_t sh2;
    if (w6 == 0x100) { uses_m2 = 0; sh2 =  7u; }
    else             { uses_m2 = 1; sh2 = 15u; }

    uint32_t n = ctx->emitted_count;

    uint8_t *pb = (uint8_t *)ctx->screen_x;
    uint8_t *pc = (uint8_t *)ctx->screen_y;
    uint8_t *pa = (uint8_t *)ctx->screen_z;
    uint8_t *pd = (uint8_t *)ctx->screen_w;

#ifdef __ARM_NEON

    if (recon_scale_geometry == 2u) {
        const uint32x4_t v48 = vdupq_n_u32(48), vred = vdupq_n_u32(ROUNDING);
        const uint32x4_t vw6 = vdupq_n_u32(w6), vm3 = vdupq_n_u32(m3);
        const uint32x4_t vsb = vdupq_n_u32(bias_b), vsc = vdupq_n_u32(bias_c);
        do {
            uint32x4_t e = vld1q_u32(zeros), rc = vld1q_u32(rec);
            uint32x4_t d = vld1q_u32((const uint32_t *)pd);
            uint32x4_t a = vaddq_u32(vld1q_u32((const uint32_t *)pa), d);
            uint32x4_t b = vaddq_u32(vld1q_u32((const uint32_t *)pb), d);
            uint32x4_t c = vsubq_u32(d, vld1q_u32((const uint32_t *)pc));
            int64x2_t s0 = vreinterpretq_s64_u64(vsubl_u32(vget_low_u32(e), vget_low_u32(v48)));
            int64x2_t s1 = vreinterpretq_s64_u64(vsubl_high_u32(e, v48));
            uint64x2_t p0 = vmull_u32(vget_low_u32(a), vget_low_u32(rc)), p1 = vmull_high_u32(a, rc);
            p0 = vsubq_u64(p0, vshrq_n_u64(vaddw_u32(p0, vget_low_u32(vred)), 15));
            p1 = vsubq_u64(p1, vshrq_n_u64(vaddw_high_u32(p1, vred), 15));
            if (uses_m2) b = vmulq_u32(b, vw6);
            c = vmulq_u32(c, vm3);
            uint64x2_t q0 = vshlq_u64(vmull_u32(vget_low_u32(b), vget_low_u32(rc)), s0);
            uint64x2_t q1 = vshlq_u64(vmull_high_u32(b, rc), s1);
            uint64x2_t r0 = vshlq_u64(vmull_u32(vget_low_u32(c), vget_low_u32(rc)), s0);
            uint64x2_t r1 = vshlq_u64(vmull_high_u32(c, rc), s1);
            p0 = vshlq_u64(p0, s0); p1 = vshlq_u64(p1, s1);
            vst1q_u32((uint32_t *)pa, vcombine_u32(vmovn_u64(p0), vmovn_u64(p1)));
            uint32x4_t ob = uses_m2 ? vcombine_u32(vshrn_n_u64(q0, 14), vshrn_n_u64(q1, 14))
                                   : vcombine_u32(vshrn_n_u64(q0, 6),  vshrn_n_u64(q1, 6));
            uint32x4_t oc = (sh3 == 15u) ? vcombine_u32(vshrn_n_u64(r0, 14), vshrn_n_u64(r1, 14))
                                         : vcombine_u32(vshrn_n_u64(r0, 8),  vshrn_n_u64(r1, 8));
            vst1q_u32((uint32_t *)pb, vaddq_u32(ob, vsb));
            vst1q_u32((uint32_t *)pc, vaddq_u32(oc, vsc));
            rec   += 4;
            zeros += 4;
            pd += 16; pa += 16; pb += 16; pc += 16;
            n -= 4;
        } while ((int32_t)n > 0);
        return;
    }
#endif
    do {
        for (int i = 0; i < 4; i++) {
            uint32_t e  = zeros[i];
            uint32_t rc = rec[i];
            uint32_t d  = rd32(pd + i * 4);
            uint32_t a  = rd32(pa + i * 4);
            uint32_t b  = rd32(pb + i * 4);
            uint32_t c  = rd32(pc + i * 4);

            int s = (int8_t)(uint8_t)(e - 48u);

            a = a + d;
            b = b + d;
            c = d - c;

            uint64_t p = (uint64_t)a * rc;
            p = p - ((p + ROUNDING) >> 15);
            p = ushl64_7(p, s);
            wr32(pa + i * 4, (uint32_t)p);

            uint32_t bb = uses_m2 ? (uint32_t)(b * w6) : b;
            uint64_t q = ushl64_7((uint64_t)bb * rc, s);
            wr32(pb + i * 4, (uint32_t)((q * recon_scale_geometry) >> sh2) + bias_b);

            uint32_t cc = (uint32_t)(c * m3);
            uint64_t r = ushl64_7((uint64_t)cc * rc, s);
            wr32(pc + i * 4, (uint32_t)((r * recon_scale_geometry) >> sh3) + bias_c);
        }
        rec   += 4;
        zeros += 4;
        pd += 16; pa += 16; pb += 16; pc += 16;
        n -= 4;
    } while ((int32_t)n > 0);
}
#undef BIAS
#undef ROUNDING

#include <stdint.h>
#include "hires_runtime.h"
#include "core/nds_state.h"
#include "blob_symbols.h"
#include <string.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#define WIDTH 256
#define PLANE 0x100
void gpu2d_blend_apply_brightness(const gpu2d_engine_t *state, uint8_t *dest,
                        const uint8_t *source, const uint16_t *mask) {

    uint16_t field_y, control;
    field_y = state->bldy;
    control = state->bldcnt;
    int y = (int)field_y + (int)field_y;
    if (y > 32) y = 32;

    int weight = 32 - y;
    if (control & 0x40) y = 0;
    int end = y * 63 + 16;

#ifdef __ARM_NEON

    {
        static const uint8_t BITS[16] = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
        static const uint8_t SEL[16]  = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 };
        const uint8x16_t v8  = vld1q_u8(BITS);
        const uint8x16_t v30 = vld1q_u8(SEL);
        const uint8x16_t v28 = vdupq_n_u8((uint8_t)weight);
        const uint16x8_t v29 = vdupq_n_u16((uint16_t)end);
        const uint8x16_t v31 = vdupq_n_u8(32);
        const uint8_t *f0 = source, *f1 = source + PLANE, *f2 = source + 2 * PLANE;
        uint8_t *d0 = dest, *d1 = dest + PLANE, *d2 = dest + 2 * PLANE;
        const uint8_t *m = (const uint8_t *)mask;
        for (int turn = 0; turn < WIDTH / 32; turn++) {
            uint16_t h0, h1; __builtin_memcpy(&h0, m, 2); __builtin_memcpy(&h1, m + 2, 2); m += 4;
            uint8x16_t v0 = vreinterpretq_u8_u16(vdupq_n_u16(h0));
            uint8x16_t v1 = vreinterpretq_u8_u16(vdupq_n_u16(h1));
            v0 = vqtbl1q_u8(v0, v30); v1 = vqtbl1q_u8(v1, v30);
            v0 = vtstq_u8(v0, v8);    v1 = vtstq_u8(v1, v8);
            int16x8_t v16 = vmovl_s8(vget_low_s8(vreinterpretq_s8_u8(v0)));
            int16x8_t v18 = vmovl_s8(vget_low_s8(vreinterpretq_s8_u8(v1)));
            int16x8_t v17 = vmovl_high_s8(vreinterpretq_s8_u8(v0));
            int16x8_t v19 = vmovl_high_s8(vreinterpretq_s8_u8(v1));
            uint16x8_t a16 = vandq_u16(v29, vreinterpretq_u16_s16(v16));
            uint16x8_t a17 = vandq_u16(v29, vreinterpretq_u16_s16(v17));
            uint16x8_t a18 = vandq_u16(v29, vreinterpretq_u16_s16(v18));
            uint16x8_t a19 = vandq_u16(v29, vreinterpretq_u16_s16(v19));
            uint8x16_t p0 = vbslq_u8(v0, v28, v31);
            uint8x16_t p1 = vbslq_u8(v1, v28, v31);
            uint8x16_t s2 = vld1q_u8(f0), s3 = vld1q_u8(f0 + 16);
            uint8x16_t s4 = vld1q_u8(f1), s5 = vld1q_u8(f1 + 16);
            uint8x16_t s6 = vld1q_u8(f2), s7 = vld1q_u8(f2 + 16);
            f0 += 32; f1 += 32; f2 += 32;
            uint16x8_t v20 = vmlal_u8(a16, vget_low_u8(s2), vget_low_u8(p0));
            uint16x8_t v22 = vmlal_u8(a18, vget_low_u8(s3), vget_low_u8(p1));
            uint16x8_t v24 = vmlal_u8(a16, vget_low_u8(s4), vget_low_u8(p0));
            uint16x8_t v26 = vmlal_u8(a18, vget_low_u8(s5), vget_low_u8(p1));
            uint16x8_t w16 = vmlal_u8(a16, vget_low_u8(s6), vget_low_u8(p0));
            uint16x8_t w18 = vmlal_u8(a18, vget_low_u8(s7), vget_low_u8(p1));
            uint16x8_t v21 = vmlal_high_u8(a17, s2, p0);
            uint16x8_t v23 = vmlal_high_u8(a19, s3, p1);
            uint16x8_t v25 = vmlal_high_u8(a17, s4, p0);
            uint16x8_t v27 = vmlal_high_u8(a19, s5, p1);
            uint16x8_t w17 = vmlal_high_u8(a17, s6, p0);
            uint16x8_t w19 = vmlal_high_u8(a19, s7, p1);
            vst1q_u8(d0,      vcombine_u8(vshrn_n_u16(v20, 5), vshrn_n_u16(v21, 5)));
            vst1q_u8(d0 + 16, vcombine_u8(vshrn_n_u16(v22, 5), vshrn_n_u16(v23, 5)));
            vst1q_u8(d1,      vcombine_u8(vshrn_n_u16(v24, 5), vshrn_n_u16(v25, 5)));
            vst1q_u8(d1 + 16, vcombine_u8(vshrn_n_u16(v26, 5), vshrn_n_u16(v27, 5)));
            vst1q_u8(d2,      vcombine_u8(vshrn_n_u16(w16, 5), vshrn_n_u16(w17, 5)));
            vst1q_u8(d2 + 16, vcombine_u8(vshrn_n_u16(w18, 5), vshrn_n_u16(w19, 5)));
            d0 += 32; d1 += 32; d2 += 32;
        }
    }
#else
    for (int c = 0; c < 3; c++) {
        const uint8_t *F = source  + c * PLANE;
        uint8_t       *S = dest + c * PLANE;

        for (int i = 0; i < WIDTH; i++) {
            int is_set = (mask[i >> 4] >> (i & 15)) & 1;
            uint16_t acc = (uint16_t)(is_set ? end : 0);
            uint8_t  p   = (uint8_t)(is_set ? weight : 32);
            acc = (uint16_t)(acc + (uint16_t)(F[i] * p));
            S[i] = (uint8_t)(acc >> 5);
        }
    }
#endif
}
#undef WIDTH
#undef PLANE

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#define WIDTH 256
#define PLANE 0x100
#define LIMIT  63

void gpu2d_blend_apply_alpha(uint8_t *dest, const uint8_t *sources,
                        const uint8_t *weightA, const uint8_t *weightB) {

#ifdef __ARM_NEON

    {
        const uint8x16_t cap = vdupq_n_u8(LIMIT);
        for (int i = 0; i < WIDTH; i += 16) {
            uint8x16_t wa = vld1q_u8(weightA + i);
            uint8x16_t wb = vld1q_u8(weightB + i);
            for (int c = 0; c < 3; c++) {
                uint8x16_t a = vld1q_u8(sources + c * PLANE + i);
                uint8x16_t b = vld1q_u8(sources + 3 * PLANE + c * PLANE + i);
                uint16x8_t lo = vmull_u8(vget_low_u8(a), vget_low_u8(wa));
                lo = vmlal_u8(lo, vget_low_u8(b), vget_low_u8(wb));
                uint16x8_t hi = vmull_high_u8(a, wa);
                hi = vmlal_high_u8(hi, b, wb);
                uint8x16_t r = vrshrn_high_n_u16(vrshrn_n_u16(lo, 5), hi, 5);
                vst1q_u8(dest + c * PLANE + i, vminq_u8(r, cap));
            }
        }
        return;
    }
#else

    for (int c = 0; c < 3; c++) {
        const uint8_t *A = sources + c * PLANE;
        const uint8_t *B = sources + 3 * PLANE + c * PLANE;
        uint8_t *S       = dest + c * PLANE;

        for (int i = 0; i < WIDTH; i++) {
            uint16_t acc = (uint16_t)((uint16_t)(A[i] * weightA[i])
                                    + (uint16_t)(B[i] * weightB[i]));
            uint8_t r = (uint8_t)((acc + 16) >> 5);
            S[i] = r > LIMIT ? LIMIT : r;
        }
    }
#endif
}
#undef WIDTH
#undef PLANE
#undef LIMIT

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void gpu2d_blend_weights_from_alpha_line(unsigned char *out_plus, unsigned char *out_inv,
                         const unsigned char *data, const unsigned char *mask)
{

    extern const unsigned char gpu2d_lane_bit_masks[16];
    const unsigned char *bittab = gpu2d_lane_bit_masks;

#ifdef __ARM_NEON

    {
        static const uint8_t SEL[16] = { 0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1 };
        const uint8x16_t vsel = vld1q_u8(SEL), vbits = vld1q_u8(bittab);
        const uint8x16_t v1f = vdupq_n_u8(0x1f), v1 = vdupq_n_u8(1);
        for (int cnt = 0x100; cnt > 0; cnt -= 0x40) {
            uint8x16x4_t d = vld1q_u8_x4(data); data += 64;
            uint8x16x4_t oa, ob;
            for (int g = 0; g < 4; g++) {
                uint16x8_t m = vdupq_n_u16((uint16_t)(mask[2 * g] | (mask[2 * g + 1] << 8)));
                uint8x16_t sel = vtstq_u8(vqtbl1q_u8(vreinterpretq_u8_u16(m), vsel), vbits);
                uint8x16_t v = vbslq_u8(sel, d.val[g], v1f);
                oa.val[g] = vaddq_u8(v, v1);
                ob.val[g] = vsubq_u8(v1f, v);
            }
            mask += 8;
            vst1q_u8_x4(out_plus, oa); out_plus += 64;
            vst1q_u8_x4(out_inv, ob);  out_inv  += 64;
        }
        return;
    }
#endif
    int count = 0x100;

    while (count > 0) {
        unsigned char bufD[64];
        unsigned char bufM[8];
        unsigned char outA[64];
        unsigned char outB[64];
        int g, i;

        memcpy(bufD, data, 64);
        memcpy(bufM, mask, 8);
        data += 64;
        mask += 8;

        for (g = 0; g < 4; g++) {

            unsigned char lo = bufM[g * 2 + 0];
            unsigned char hi = bufM[g * 2 + 1];

            for (i = 0; i < 16; i++) {
                unsigned char src  = (i < 8) ? lo : hi;
                unsigned char bit  = (unsigned char)(src & bittab[i]);
                unsigned char orig = bufD[g * 16 + i];
                unsigned char v    = bit ? orig : 0x1f;

                outA[g * 16 + i] = (unsigned char)(v + 1u);
                outB[g * 16 + i] = (unsigned char)(0x1fu - v);
            }
        }

        memcpy(out_plus, outA, 64);
        memcpy(out_inv, outB, 64);
        out_plus += 64;
        out_inv  += 64;

        count -= 0x40;
    }
}

void gpu2d_blend_weights_from_alpha_masked(unsigned char *bufA, unsigned char *bufB,
                         const unsigned char *src, const unsigned char *mask)
{

    extern const unsigned char gpu2d_lane_bit_masks[16];
    const unsigned char *bittab = gpu2d_lane_bit_masks;

    int count = 0x100;

    while (count > 0) {
        unsigned char curA[64];
        unsigned char curB[64];
        unsigned char alpha[64];
        unsigned char mbuf[8];
        int g, i;

        memcpy(curA, bufA, 64);
        memcpy(curB, bufB, 64);
        memcpy(alpha, src, 64);
        memcpy(mbuf, mask, 8);
        src  += 64;
        mask += 8;

        for (g = 0; g < 4; g++) {

            unsigned char lo = mbuf[g * 2 + 0];
            unsigned char hi = mbuf[g * 2 + 1];

            for (i = 0; i < 16; i++) {
                unsigned char msrc = (i < 8) ? lo : hi;
                unsigned char bit  = (unsigned char)(msrc & bittab[i]);
                unsigned char a    = alpha[g * 16 + i];
                unsigned char plus = (unsigned char)(a + 1u);
                unsigned char inv  = (unsigned char)(0x1fu - a);

                if (bit) {
                    curA[g * 16 + i] = plus;
                }

                if (bit) {
                    curB[g * 16 + i] = inv;
                }
            }
        }

        memcpy(bufA, curA, 64);
        memcpy(bufB, curB, 64);
        bufA += 64;
        bufB += 64;

        count -= 0x40;
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#define WIDTH 256

void gpu2d_line_apply_flat_color_masked(uint16_t *dest, const uint16_t *origin, int color,
                        const uint32_t *mask) {

    uint16_t c = (uint16_t)color;
#ifdef __ARM_NEON

    {
        static const uint16_t BITS[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
        const uint16x8_t v0 = vld1q_u16(BITS);
        const uint16x8_t v1 = vshlq_n_u16(v0, 8);
        const int in_site = (origin == dest);
        for (int i = 0; i < WIDTH; i += 32) {
            uint32_t word = mask[i >> 5];
            if (word == 0) {
                if (!in_site) vst1q_u16_x4(dest + i, vld1q_u16_x4(origin + i));
                continue;
            }
            uint16x8_t lo = vdupq_n_u16((uint16_t)word);
            uint16x8_t hi = vdupq_n_u16((uint16_t)(word >> 16));
            uint16x8x4_t d = vld1q_u16_x4(origin + i);
            const uint16x8_t vc = vdupq_n_u16(c);
            d.val[0] = vbslq_u16(vtstq_u16(lo, v0), vc, d.val[0]);
            d.val[1] = vbslq_u16(vtstq_u16(lo, v1), vc, d.val[1]);
            d.val[2] = vbslq_u16(vtstq_u16(hi, v0), vc, d.val[2]);
            d.val[3] = vbslq_u16(vtstq_u16(hi, v1), vc, d.val[3]);
            vst1q_u16_x4(dest + i, d);
        }
        return;
    }
#else

    for (int i = 0; i < WIDTH; i++) {
        uint32_t word = mask[i >> 5];
        if (word & (1u << (i & 31))) dest[i] = c;
        else                            dest[i] = origin[i];
    }
#endif
}
#undef WIDTH

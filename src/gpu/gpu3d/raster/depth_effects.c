#include "hires_runtime.h"
#include <stdint.h>
#include <string.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>
#include "mem_access.h"

#ifndef __ARM_NEON


#endif
#define OFF_P0  0x000
#define OFF_P1  0x0b0
#define OFF_P2  0x160
#define OFF_P3  0x210
#define OFF_P4  0x2c0
#define OFF_P5  0x370
#define OFF_P6  0x420
#define OFF_P7  0x4d0
#define OFF_P8  0x580
#define OFF_P9  0x630
#ifdef __ARM_NEON

static void swap_pair16v(uint8_t *pp, uint8_t *pq, int is_key, uint16x8_t *mask) {
    uint16x8x2_t P = vld2q_u16((const uint16_t *)(const void *)pp);
    uint16x8x2_t Q = vld2q_u16((const uint16_t *)(const void *)pq);
    if (is_key) {
        P.val[0] = vandq_u16(P.val[0], vdupq_n_u16(0x7fff));
        Q.val[0] = vandq_u16(Q.val[0], vdupq_n_u16(0x7fff));
        *mask = vcgeq_u16(P.val[0], Q.val[0]);
    }
    uint16x8_t m = *mask;
    uint16x8_t pe = vbslq_u16(m, Q.val[0], P.val[0]);
    uint16x8_t po = vbslq_u16(m, Q.val[1], P.val[1]);
    uint16x8_t qe = vbslq_u16(m, P.val[0], Q.val[0]);
    uint16x8_t qo = vbslq_u16(m, P.val[1], Q.val[1]);
    if (is_key) {
        uint16x8_t cap = vdupq_n_u16((uint16_t)0x100u);
        pe = vminq_u16(pe, cap);
        qe = vminq_u16(qe, cap);
    }
    uint16x8x2_t OP, OQ;
    OP.val[0] = pe;                  OP.val[1] = po;
    OQ.val[0] = vsubq_u16(qe, pe);   OQ.val[1] = vsubq_u16(qo, po);
    vst2q_u16((uint16_t *)(void *)pp, OP);
    vst2q_u16((uint16_t *)(void *)pq, OQ);
}

static void swap_pair32v(uint8_t *pp, uint8_t *pq, uint16x8_t mask) {
    int16x8_t ms = vreinterpretq_s16_u16(mask);
    uint32x4_t m0 = vreinterpretq_u32_s32(vmovl_s16(vget_low_s16(ms)));
    uint32x4_t m1 = vreinterpretq_u32_s32(vmovl_high_s16(ms));
    uint32x4_t p0 = vld1q_u32((const uint32_t *)(const void *)pp);
    uint32x4_t p1 = vld1q_u32((const uint32_t *)(const void *)(pp + 16));
    uint32x4_t q0 = vld1q_u32((const uint32_t *)(const void *)pq);
    uint32x4_t q1 = vld1q_u32((const uint32_t *)(const void *)(pq + 16));
    uint32x4_t sp0 = vbslq_u32(m0, q0, p0), sq0 = vbslq_u32(m0, p0, q0);
    uint32x4_t sp1 = vbslq_u32(m1, q1, p1), sq1 = vbslq_u32(m1, p1, q1);
    vst1q_u32((uint32_t *)(void *)pp, sp0);
    vst1q_u32((uint32_t *)(void *)(pp + 16), sp1);
    vst1q_u32((uint32_t *)(void *)pq, vsubq_u32(sq0, sp0));
    vst1q_u32((uint32_t *)(void *)(pq + 16), vsubq_u32(sq1, sp1));
}
#endif
#ifndef __ARM_NEON

static void swap_pair16(uint8_t *pp, uint8_t *pq, int is_key, uint16_t mask8[8]) {
    uint16_t pe[8], po[8], qe[8], qo[8];
    for (int i = 0; i < 8; i++) {
        pe[i] = rd16(pp + i * 4);
        po[i] = rd16(pp + i * 4 + 2);
        qe[i] = rd16(pq + i * 4);
        qo[i] = rd16(pq + i * 4 + 2);
    }
    if (is_key) {
        for (int i = 0; i < 8; i++) {
            pe[i] &= 0x7fffu;
            qe[i] &= 0x7fffu;
            mask8[i] = (pe[i] >= qe[i]) ? 0xffffu : 0u;
        }
    }
    for (int i = 0; i < 8; i++) {
        uint16_t m    = mask8[i];
        uint16_t swpe = m ? qe[i] : pe[i];
        uint16_t swpo = m ? qo[i] : po[i];
        uint16_t swqe = m ? pe[i] : qe[i];
        uint16_t swqo = m ? po[i] : qo[i];
        if (is_key) {
            if (swpe > 0x100u) swpe = 0x100u;
            if (swqe > 0x100u) swqe = 0x100u;
        }
        wr16(pp + i * 4,     swpe);
        wr16(pp + i * 4 + 2, swpo);
        wr16(pq + i * 4,     (uint16_t)(swqe - swpe));
        wr16(pq + i * 4 + 2, (uint16_t)(swqo - swpo));
    }
}

static void swap_pair32(uint8_t *pp, uint8_t *pq, const uint16_t mask8[8]) {
    uint32_t pv[8], qv[8];
    for (int i = 0; i < 8; i++) {
        pv[i] = rd32(pp + i * 4);
        qv[i] = rd32(pq + i * 4);
    }
    for (int i = 0; i < 8; i++) {
        uint32_t m   = mask8[i] ? 0xffffffffu : 0u;
        uint32_t swp = m ? qv[i] : pv[i];
        uint32_t swq = m ? pv[i] : qv[i];
        wr32(pp + i * 4, swp);
        wr32(pq + i * 4, swq - swp);
    }
}
#endif

void gpu3d_raster_swap_linked_pair_channels(uint8_t *ctx, int32_t n) {

#ifdef __ARM_NEON
    {
        uint8_t *q0 = ctx + OFF_P0, *q1 = ctx + OFF_P1;
        uint8_t *q2 = ctx + OFF_P2, *q3 = ctx + OFF_P3;
        uint8_t *q4 = ctx + OFF_P4, *q5 = ctx + OFF_P5;
        uint8_t *q6 = ctx + OFF_P6, *q7 = ctx + OFF_P7;
        uint8_t *q8 = ctx + OFF_P8, *q9 = ctx + OFF_P9;
        int32_t k = n;
        uint16x8_t m = vdupq_n_u16(0);
        do {
            swap_pair16v(q8, q9, 1, &m);
            swap_pair16v(q4, q5, 0, &m);
            swap_pair32v(q0, q1, m);
            swap_pair16v(q6, q7, 0, &m);
            swap_pair32v(q2, q3, m);
            q0 += 32; q1 += 32; q2 += 32; q3 += 32; q4 += 32;
            q5 += 32; q6 += 32; q7 += 32; q8 += 32; q9 += 32;
            k -= 8;
        } while (k > 0);
        return;
    }
#else

    uint8_t *p0  = ctx + OFF_P0;
    uint8_t *p1  = ctx + OFF_P1;
    uint8_t *p2  = ctx + OFF_P2;
    uint8_t *p3  = ctx + OFF_P3;
    uint8_t *p4  = ctx + OFF_P4;
    uint8_t *p5  = ctx + OFF_P5;
    uint8_t *p6  = ctx + OFF_P6;
    uint8_t *p7  = ctx + OFF_P7;
    uint8_t *p8  = ctx + OFF_P8;
    uint8_t *p9  = ctx + OFF_P9;

    do {
        uint16_t mask[8];

        swap_pair16(p8, p9, 1, mask);
        swap_pair16(p4, p5, 0, mask);

        swap_pair32(p0, p1, mask);
        swap_pair16(p6, p7, 0, mask);

        swap_pair32(p2, p3, mask);

        p0 += 32; p1 += 32; p2 += 32; p3 += 32; p4 += 32;
        p5 += 32; p6 += 32; p7 += 32; p8 += 32; p9 += 32;

        n -= 8;
    } while (n > 0);
#endif
}
#undef OFF_P0
#undef OFF_P1
#undef OFF_P2
#undef OFF_P3
#undef OFF_P4
#undef OFF_P5
#undef OFF_P6
#undef OFF_P7
#undef OFF_P8
#undef OFF_P9

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"

void gpu3d_raster_blend_fog_line(unsigned char *dst, const unsigned char *src,
                        const unsigned char *factor, uint32_t color) {

#ifdef __ARM_NEON

    {
        uint8x16_t k0 = vdupq_n_u8((uint8_t)color);
        uint8x16_t k1 = vdupq_n_u8((uint8_t)(color >> 8));
        uint8x16_t k2 = vdupq_n_u8((uint8_t)(color >> 16));
        uint8x16_t k3 = vdupq_n_u8((uint8_t)(color >> 24));
        const uint8x16_t m7f = vdupq_n_u8(0x7f);
        for (int base = 0; base < 256; base += 16) {
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

    for (int base = 0; base < 256; base += 16) {
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

void gpu3d_raster_set_alpha_plane(unsigned char *dst, const unsigned char *src,
                        const unsigned char *level, uint32_t param4) {

#ifdef __ARM_NEON

    {
        const uint8x16_t m7f = vdupq_n_u8(0x7f);
        uint8x16_t ng = vdupq_n_u8((uint8_t)(param4 >> 24));
        for (int base = 0; base < 256; base += 16) {
            uint8x16x4_t v = vld4q_u8(src + base * 4);
            uint8x16_t nv = vld1q_u8(level + base);
            uint8x16_t eq = vceqq_u8(nv, m7f);
            uint8x16_t f  = vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(nv)));
            uint8x16_t pu = vcgtq_u8(v.val[3], m7f);
            f = vaddq_u8(f, eq);
            f = vandq_u8(f, pu);
            uint8x16_t b3 = vandq_u8(v.val[3], m7f);
            int8x16_t  d  = vreinterpretq_s8_u8(vsubq_u8(b3, ng));
            int8x16_t  fs = vreinterpretq_s8_u8(f);
            uint16x8_t lo = vreinterpretq_u16_s16(
                    vmull_s8(vget_low_s8(d), vget_low_s8(fs)));
            uint16x8_t hi = vreinterpretq_u16_s16(vmull_high_s8(d, fs));
            v.val[3] = vaddq_u8(b3, vshrn_high_n_u16(vshrn_n_u16(lo, 7), hi, 7));
            vst4q_u8(dst + base * 4, v);
        }
        return;
    }
#else

    uint8_t level_global = (uint8_t)(param4 >> 24);

    for (int base = 0; base < 256; base += 16) {
        unsigned char reg[64];
        __builtin_memcpy(reg, src + base * 4, 64);

        for (int j = 0; j < 16; j++) {
            uint8_t nv = level[base + j];
            uint8_t eq = (nv == 0x7f) ? 0xff : 0;
            uint8_t f  = (uint8_t)((uint8_t)(-(int)nv) + eq);

            uint8_t b3_raw = reg[j * 4 + 3];
            uint8_t gate = (b3_raw > 0x7f) ? 0xff : 0;
            f = (uint8_t)(f & gate);

            uint8_t b3 = (uint8_t)(b3_raw & 0x7f);

            int8_t diff = (int8_t)(uint8_t)(b3 - level_global);
            int8_t fac  = (int8_t)f;
            int16_t prod = (int16_t)((int32_t)diff * (int32_t)fac);
            uint8_t shr7 = (uint8_t)(((uint16_t)prod) >> 7);

            reg[j * 4 + 3] = (uint8_t)(b3 + shr7);

        }

        __builtin_memcpy(dst + base * 4, reg, 64);
    }
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#ifdef __ARM_NEON

static inline uint8x16_t e358_wins(uint8x16_t n0, uint8x16_t n1,
                                   uint8x16_t n2, uint8x16_t n3,
                                   uint8x16_t c0, uint8x16_t c1,
                                   uint8x16_t c2, uint8x16_t c3) {
    n3 = vandq_u8(n3, vdupq_n_u8(0x3f));
    uint8x16_t e2 = vceqq_u8(c2, n2);
    uint8x16_t e1 = vceqq_u8(c1, n1);
    uint8x16_t g2 = vcgtq_u8(n2, c2);
    uint8x16_t g1 = vcgtq_u8(n1, c1);
    uint8x16_t g0 = vcgtq_u8(n0, c0);
    uint8x16_t t1 = vandq_u8(e2, g1);
    uint8x16_t t0 = vandq_u8(vandq_u8(e2, e1), g0);
    uint8x16_t r  = vorrq_u8(vorrq_u8(g2, t1), t0);
    return vbicq_u8(r, vceqq_u8(c3, n3));
}
#endif
#ifndef __ARM_NEON

static int wins(const unsigned char *n, uint8_t c0, uint8_t c1,
                    uint8_t c2, uint8_t c3) {
    uint8_t n0 = n[0], n1 = n[1], n2 = n[2];
    uint8_t n3 = (uint8_t)(n[3] & 0x3f);
    int g = (n2 > c2)
         || (n2 == c2 && n1 > c1)
         || (n2 == c2 && n1 == c1 && n0 > c0);
    if (c3 == n3) g = 0;
    return g;
}
#endif

void gpu3d_raster_test_neighbor_edge_dual(unsigned char *dst, const unsigned char *va,
                        const unsigned char *center, const unsigned char *vb,
                        uint32_t edge) {

    unsigned char brd[4];
    brd[0] = (uint8_t)edge;
    brd[1] = (uint8_t)(edge >> 8);
    brd[2] = (uint8_t)(edge >> 16);
    brd[3] = (uint8_t)(edge >> 24);

#ifdef __ARM_NEON
    {
        uint8x16_t b0 = vdupq_n_u8(brd[0]), b1 = vdupq_n_u8(brd[1]);
        uint8x16_t b2 = vdupq_n_u8(brd[2]), b3 = vdupq_n_u8(brd[3]);
        const unsigned char *pc = center;
        const unsigned char *pi = center + 0x3c;
        const unsigned char *pd = center + 4;
        for (int g = 0; g < 16; g++) {
            uint8x16x4_t v = vld4q_u8(pc); pc += 64;
            uint8x16_t c0 = v.val[0], c1 = v.val[1], c2 = v.val[2];
            uint8x16_t c3 = veorq_u8(vandq_u8(v.val[3], vdupq_n_u8(0x7f)),
                                     vdupq_n_u8(0x40));
            uint8x16_t i0, i1, i2, i3, d0, d1, d2, d3;
            if (g == 0) {
                i0 = vextq_u8(b0, c0, 15); i1 = vextq_u8(b1, c1, 15);
                i2 = vextq_u8(b2, c2, 15); i3 = vextq_u8(b3, v.val[3], 15);
            } else {
                uint8x16x4_t t = vld4q_u8(pi); pi += 64;
                i0 = t.val[0]; i1 = t.val[1]; i2 = t.val[2]; i3 = t.val[3];
            }
            if (g == 15) {
                d0 = vextq_u8(c0, b0, 1); d1 = vextq_u8(c1, b1, 1);
                d2 = vextq_u8(c2, b2, 1); d3 = vextq_u8(v.val[3], b3, 1);
            } else {
                uint8x16x4_t t = vld4q_u8(pd); pd += 64;
                d0 = t.val[0]; d1 = t.val[1]; d2 = t.val[2]; d3 = t.val[3];
            }
            uint8x16x4_t a = vld4q_u8(va); va += 64;
            uint8x16x4_t e = vld4q_u8(vb); vb += 64;

            uint8x16_t lives = vmvnq_u8(e358_wins(i0, i1, i2, i3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, e358_wins(d0, d1, d2, d3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, e358_wins(a.val[0], a.val[1], a.val[2], a.val[3],
                                            c0, c1, c2, c3));
            lives = vbicq_u8(lives, e358_wins(e.val[0], e.val[1], e.val[2], e.val[3],
                                            c0, c1, c2, c3));
            vst1q_u8(dst, vorrq_u8(vshrq_n_u8(c3, 3), lives)); dst += 16;
        }
        return;
    }
#else

    for (int p = 0; p < 256; p++) {
        const unsigned char *c = center + p * 4;
        uint8_t c0 = c[0], c1 = c[1], c2 = c[2];
        uint8_t c3 = (uint8_t)(((uint8_t)(c[3] & 0x7f)) ^ 0x40);

        const unsigned char *left = (p == 0)   ? brd : (center + (p - 1) * 4);
        const unsigned char *right = (p == 255) ? brd : (center + (p + 1) * 4);

        int lives = !wins(left, c0, c1, c2, c3)
                && !wins(right, c0, c1, c2, c3)
                && !wins(va + p * 4, c0, c1, c2, c3)
                && !wins(vb + p * 4, c0, c1, c2, c3);

        dst[p] = (uint8_t)((c3 >> 3) | (lives ? 0xff : 0x00));
    }
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#ifdef __ARM_NEON

static inline uint8x16_t winner_bordered(uint8x16_t n0, uint8x16_t n1,
                                       uint8x16_t n2, uint8x16_t n3,
                                       uint8x16_t c0, uint8x16_t c1,
                                       uint8x16_t c2, uint8x16_t c3) {
    n3 = vandq_u8(n3, vdupq_n_u8(0x3f));
    uint8x16_t e2 = vceqq_u8(c2, n2);
    uint8x16_t e1 = vceqq_u8(c1, n1);
    uint8x16_t g2 = vcgtq_u8(n2, c2);
    uint8x16_t g1 = vcgtq_u8(n1, c1);
    uint8x16_t g0 = vcgtq_u8(n0, c0);
    uint8x16_t t1 = vandq_u8(e2, g1);
    uint8x16_t t0 = vandq_u8(vandq_u8(e2, e1), g0);
    uint8x16_t r  = vorrq_u8(vorrq_u8(g2, t1), t0);
    return vbicq_u8(r, vceqq_u8(c3, n3));
}
#endif
#ifndef __ARM_NEON

static int wins_6c0(const unsigned char *n, uint8_t c0, uint8_t c1,
                   uint8_t c2, uint8_t c3) {
    uint8_t n0 = n[0], n1 = n[1], n2 = n[2];
    uint8_t n3 = (uint8_t)(n[3] & 0x3f);
    int g = (n2 > c2)
         || (n2 == c2 && n1 > c1)
         || (n2 == c2 && n1 == c1 && n0 > c0);
    if (c3 == n3) g = 0;
    return g;
}
#endif

void gpu3d_raster_test_neighbor_edge_bordered(unsigned char *dst, const unsigned char *center,
                        const unsigned char *neighbor, uint32_t edge) {

    unsigned char brd[4];
    brd[0] = (uint8_t)edge;
    brd[1] = (uint8_t)(edge >> 8);
    brd[2] = (uint8_t)(edge >> 16);
    brd[3] = (uint8_t)(edge >> 24);

#ifdef __ARM_NEON
    {
        uint8x16_t b0 = vdupq_n_u8(brd[0]), b1 = vdupq_n_u8(brd[1]);
        uint8x16_t b2 = vdupq_n_u8(brd[2]), b3 = vdupq_n_u8(brd[3]);
        const unsigned char *pc = center;
        const unsigned char *pi = center + 0x3c;
        const unsigned char *pd = center + 4;
        const unsigned char *pv = neighbor;
        for (int g = 0; g < 16; g++) {
            uint8x16x4_t v = vld4q_u8(pc); pc += 64;
            uint8x16_t c0 = v.val[0], c1 = v.val[1], c2 = v.val[2];
            uint8x16_t c3 = veorq_u8(vandq_u8(v.val[3], vdupq_n_u8(0x7f)),
                                     vdupq_n_u8(0x40));
            uint8x16_t i0, i1, i2, i3, d0, d1, d2, d3;
            if (g == 0) {
                i0 = vextq_u8(b0, c0, 15); i1 = vextq_u8(b1, c1, 15);
                i2 = vextq_u8(b2, c2, 15); i3 = vextq_u8(b3, v.val[3], 15);
            } else {
                uint8x16x4_t t = vld4q_u8(pi); pi += 64;
                i0 = t.val[0]; i1 = t.val[1]; i2 = t.val[2]; i3 = t.val[3];
            }
            if (g == 15) {
                d0 = vextq_u8(c0, b0, 1); d1 = vextq_u8(c1, b1, 1);
                d2 = vextq_u8(c2, b2, 1); d3 = vextq_u8(v.val[3], b3, 1);
            } else {
                uint8x16x4_t t = vld4q_u8(pd); pd += 64;
                d0 = t.val[0]; d1 = t.val[1]; d2 = t.val[2]; d3 = t.val[3];
            }
            uint8x16x4_t w = vld4q_u8(pv); pv += 64;

            uint8x16_t lives = vmvnq_u8(winner_bordered(i0, i1, i2, i3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered(d0, d1, d2, d3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered(b0, b1, b2, b3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered(w.val[0], w.val[1], w.val[2], w.val[3],
                                                c0, c1, c2, c3));
            vst1q_u8(dst, vorrq_u8(vshrq_n_u8(c3, 3), lives)); dst += 16;
        }
        return;
    }
#else
    for (int p = 0; p < 256; p++) {
        const unsigned char *c = center + p * 4;
        uint8_t c0 = c[0], c1 = c[1], c2 = c[2];
        uint8_t c3 = (uint8_t)(((uint8_t)(c[3] & 0x7f)) ^ 0x40);

        const unsigned char *left = (p == 0)   ? brd : (center + (p - 1) * 4);
        const unsigned char *right = (p == 255) ? brd : (center + (p + 1) * 4);

        int lives = !wins_6c0(left, c0, c1, c2, c3)
                && !wins_6c0(right, c0, c1, c2, c3)
                && !wins_6c0(brd, c0, c1, c2, c3)
                && !wins_6c0(neighbor + p * 4, c0, c1, c2, c3);

        dst[p] = (uint8_t)((c3 >> 3) | (lives ? 0xff : 0x00));
    }
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#ifdef __ARM_NEON

static inline uint8x16_t winner_bordered_alt(uint8x16_t n0, uint8x16_t n1,
                                       uint8x16_t n2, uint8x16_t n3,
                                       uint8x16_t c0, uint8x16_t c1,
                                       uint8x16_t c2, uint8x16_t c3) {
    n3 = vandq_u8(n3, vdupq_n_u8(0x3f));
    uint8x16_t e2 = vceqq_u8(c2, n2);
    uint8x16_t e1 = vceqq_u8(c1, n1);
    uint8x16_t g2 = vcgtq_u8(n2, c2);
    uint8x16_t g1 = vcgtq_u8(n1, c1);
    uint8x16_t g0 = vcgtq_u8(n0, c0);
    uint8x16_t t1 = vandq_u8(e2, g1);
    uint8x16_t t0 = vandq_u8(vandq_u8(e2, e1), g0);
    uint8x16_t r  = vorrq_u8(vorrq_u8(g2, t1), t0);
    return vbicq_u8(r, vceqq_u8(c3, n3));
}
#endif
#ifndef __ARM_NEON

static int wins_a28(const unsigned char *n, uint8_t c0, uint8_t c1,
                   uint8_t c2, uint8_t c3) {
    uint8_t n0 = n[0], n1 = n[1], n2 = n[2];
    uint8_t n3 = (uint8_t)(n[3] & 0x3f);
    int g = (n2 > c2)
         || (n2 == c2 && n1 > c1)
         || (n2 == c2 && n1 == c1 && n0 > c0);
    if (c3 == n3) g = 0;
    return g;
}
#endif

void gpu3d_raster_test_neighbor_edge_bordered_alt(unsigned char *dst, const unsigned char *center,
                        const unsigned char *neighbor, uint32_t edge) {

    unsigned char brd[4];
    brd[0] = (uint8_t)edge;
    brd[1] = (uint8_t)(edge >> 8);
    brd[2] = (uint8_t)(edge >> 16);
    brd[3] = (uint8_t)(edge >> 24);

#ifdef __ARM_NEON
    {
        uint8x16_t b0 = vdupq_n_u8(brd[0]), b1 = vdupq_n_u8(brd[1]);
        uint8x16_t b2 = vdupq_n_u8(brd[2]), b3 = vdupq_n_u8(brd[3]);
        const unsigned char *pc = center;
        const unsigned char *pi = center + 0x3c;
        const unsigned char *pd = center + 4;
        const unsigned char *pv = neighbor;
        for (int g = 0; g < 16; g++) {
            uint8x16x4_t v = vld4q_u8(pc); pc += 64;
            uint8x16_t c0 = v.val[0], c1 = v.val[1], c2 = v.val[2];
            uint8x16_t c3 = veorq_u8(vandq_u8(v.val[3], vdupq_n_u8(0x7f)),
                                     vdupq_n_u8(0x40));
            uint8x16_t i0, i1, i2, i3, d0, d1, d2, d3;
            if (g == 0) {
                i0 = vextq_u8(b0, c0, 15); i1 = vextq_u8(b1, c1, 15);
                i2 = vextq_u8(b2, c2, 15); i3 = vextq_u8(b3, v.val[3], 15);
            } else {
                uint8x16x4_t t = vld4q_u8(pi); pi += 64;
                i0 = t.val[0]; i1 = t.val[1]; i2 = t.val[2]; i3 = t.val[3];
            }
            if (g == 15) {
                d0 = vextq_u8(c0, b0, 1); d1 = vextq_u8(c1, b1, 1);
                d2 = vextq_u8(c2, b2, 1); d3 = vextq_u8(v.val[3], b3, 1);
            } else {
                uint8x16x4_t t = vld4q_u8(pd); pd += 64;
                d0 = t.val[0]; d1 = t.val[1]; d2 = t.val[2]; d3 = t.val[3];
            }
            uint8x16x4_t w = vld4q_u8(pv); pv += 64;

            uint8x16_t lives = vmvnq_u8(winner_bordered_alt(i0, i1, i2, i3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_alt(d0, d1, d2, d3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_alt(w.val[0], w.val[1], w.val[2], w.val[3],
                                                c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_alt(b0, b1, b2, b3, c0, c1, c2, c3));
            vst1q_u8(dst, vorrq_u8(vshrq_n_u8(c3, 3), lives)); dst += 16;
        }
        return;
    }
#else
    for (int p = 0; p < 256; p++) {
        const unsigned char *c = center + p * 4;
        uint8_t c0 = c[0], c1 = c[1], c2 = c[2];
        uint8_t c3 = (uint8_t)(((uint8_t)(c[3] & 0x7f)) ^ 0x40);

        const unsigned char *left = (p == 0)   ? brd : (center + (p - 1) * 4);
        const unsigned char *right = (p == 255) ? brd : (center + (p + 1) * 4);

        int lives = !wins_a28(left, c0, c1, c2, c3)
                && !wins_a28(right, c0, c1, c2, c3)
                && !wins_a28(neighbor + p * 4, c0, c1, c2, c3)
                && !wins_a28(brd, c0, c1, c2, c3);

        dst[p] = (uint8_t)((c3 >> 3) | (lives ? 0xff : 0x00));
    }
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>
#ifndef __ARM_NEON

#endif
#define OFF_P0  (0u * RECON_3D_ROW_ARRAY)
#define OFF_P1  (1u * RECON_3D_ROW_ARRAY)
#define OFF_P2  (2u * RECON_3D_ROW_ARRAY)
#define OFF_P3  (3u * RECON_3D_ROW_ARRAY)
#define OFF_P4  (4u * RECON_3D_ROW_ARRAY)
#define OFF_P5  (5u * RECON_3D_ROW_ARRAY)
#define OFF_P6  (6u * RECON_3D_ROW_ARRAY)
#define OFF_P7  (7u * RECON_3D_ROW_ARRAY)
#define OFF_P8  (8u * RECON_3D_ROW_ARRAY)
#define OFF_P9  (9u * RECON_3D_ROW_ARRAY)
#define CLAMP16 RECON_3D_WIDTH
#ifdef __ARM_NEON

static void swap_pair16v_6(uint8_t *pp, uint8_t *pq, int is_key, uint16x8_t *mask) {
    uint16x8x2_t P = vld2q_u16((const uint16_t *)(const void *)pp);
    uint16x8x2_t Q = vld2q_u16((const uint16_t *)(const void *)pq);
    if (is_key) {
        P.val[0] = vandq_u16(P.val[0], vdupq_n_u16(0x7fff));
        Q.val[0] = vandq_u16(Q.val[0], vdupq_n_u16(0x7fff));
        *mask = vcgeq_u16(P.val[0], Q.val[0]);
    }
    uint16x8_t m = *mask;
    uint16x8_t pe = vbslq_u16(m, Q.val[0], P.val[0]);
    uint16x8_t po = vbslq_u16(m, Q.val[1], P.val[1]);
    uint16x8_t qe = vbslq_u16(m, P.val[0], Q.val[0]);
    uint16x8_t qo = vbslq_u16(m, P.val[1], Q.val[1]);
    if (is_key) {
        uint16x8_t cap = vdupq_n_u16((uint16_t)CLAMP16);
        pe = vminq_u16(pe, cap);
        qe = vminq_u16(qe, cap);
    }
    uint16x8x2_t OP, OQ;
    OP.val[0] = pe;                  OP.val[1] = po;
    OQ.val[0] = vsubq_u16(qe, pe);   OQ.val[1] = vsubq_u16(qo, po);
    vst2q_u16((uint16_t *)(void *)pp, OP);
    vst2q_u16((uint16_t *)(void *)pq, OQ);
}

static void swap_pair32v_6(uint8_t *pp, uint8_t *pq, uint16x8_t mask) {
    int16x8_t ms = vreinterpretq_s16_u16(mask);
    uint32x4_t m0 = vreinterpretq_u32_s32(vmovl_s16(vget_low_s16(ms)));
    uint32x4_t m1 = vreinterpretq_u32_s32(vmovl_high_s16(ms));
    uint32x4_t p0 = vld1q_u32((const uint32_t *)(const void *)pp);
    uint32x4_t p1 = vld1q_u32((const uint32_t *)(const void *)(pp + 16));
    uint32x4_t q0 = vld1q_u32((const uint32_t *)(const void *)pq);
    uint32x4_t q1 = vld1q_u32((const uint32_t *)(const void *)(pq + 16));
    uint32x4_t sp0 = vbslq_u32(m0, q0, p0), sq0 = vbslq_u32(m0, p0, q0);
    uint32x4_t sp1 = vbslq_u32(m1, q1, p1), sq1 = vbslq_u32(m1, p1, q1);
    vst1q_u32((uint32_t *)(void *)pp, sp0);
    vst1q_u32((uint32_t *)(void *)(pp + 16), sp1);
    vst1q_u32((uint32_t *)(void *)pq, vsubq_u32(sq0, sp0));
    vst1q_u32((uint32_t *)(void *)(pq + 16), vsubq_u32(sq1, sp1));
}
#endif
#ifndef __ARM_NEON

static void swap_pair16_6(uint8_t *pp, uint8_t *pq, int is_key, uint16_t mask8[8]) {
    uint16_t pe[8], po[8], qe[8], qo[8];
    for (int i = 0; i < 8; i++) {
        pe[i] = rd16(pp + i * 4);
        po[i] = rd16(pp + i * 4 + 2);
        qe[i] = rd16(pq + i * 4);
        qo[i] = rd16(pq + i * 4 + 2);
    }
    if (is_key) {
        for (int i = 0; i < 8; i++) {
            pe[i] &= 0x7fffu;
            qe[i] &= 0x7fffu;
            mask8[i] = (pe[i] >= qe[i]) ? 0xffffu : 0u;
        }
    }
    for (int i = 0; i < 8; i++) {
        uint16_t m    = mask8[i];
        uint16_t swpe = m ? qe[i] : pe[i];
        uint16_t swpo = m ? qo[i] : po[i];
        uint16_t swqe = m ? pe[i] : qe[i];
        uint16_t swqo = m ? po[i] : qo[i];
        if (is_key) {
            if (swpe > CLAMP16) swpe = (uint16_t)CLAMP16;
            if (swqe > CLAMP16) swqe = (uint16_t)CLAMP16;
        }
        wr16(pp + i * 4,     swpe);
        wr16(pp + i * 4 + 2, swpo);
        wr16(pq + i * 4,     (uint16_t)(swqe - swpe));
        wr16(pq + i * 4 + 2, (uint16_t)(swqo - swpo));
    }
}

static void swap_pair32_6(uint8_t *pp, uint8_t *pq, const uint16_t mask8[8]) {
    uint32_t pv[8], qv[8];
    for (int i = 0; i < 8; i++) {
        pv[i] = rd32(pp + i * 4);
        qv[i] = rd32(pq + i * 4);
    }
    for (int i = 0; i < 8; i++) {
        uint32_t m   = mask8[i] ? 0xffffffffu : 0u;
        uint32_t swp = m ? qv[i] : pv[i];
        uint32_t swq = m ? pv[i] : qv[i];
        wr32(pp + i * 4, swp);
        wr32(pq + i * 4, swq - swp);
    }
}
#endif

void gpu3d_raster_swap_linked_pair_channels_x2(uint8_t *ctx, int32_t n) {

#ifdef __ARM_NEON
    {
        uint8_t *q0 = ctx + OFF_P0, *q1 = ctx + OFF_P1;
        uint8_t *q2 = ctx + OFF_P2, *q3 = ctx + OFF_P3;
        uint8_t *q4 = ctx + OFF_P4, *q5 = ctx + OFF_P5;
        uint8_t *q6 = ctx + OFF_P6, *q7 = ctx + OFF_P7;
        uint8_t *q8 = ctx + OFF_P8, *q9 = ctx + OFF_P9;
        int32_t k = n;
        uint16x8_t m = vdupq_n_u16(0);
        do {
            swap_pair16v_6(q8, q9, 1, &m);
            swap_pair16v_6(q4, q5, 0, &m);
            swap_pair32v_6(q0, q1, m);
            swap_pair16v_6(q6, q7, 0, &m);
            swap_pair32v_6(q2, q3, m);
            q0 += 32; q1 += 32; q2 += 32; q3 += 32; q4 += 32;
            q5 += 32; q6 += 32; q7 += 32; q8 += 32; q9 += 32;
            k -= 8;
        } while (k > 0);
        return;
    }
#else

    uint8_t *p0  = ctx + OFF_P0;
    uint8_t *p1  = ctx + OFF_P1;
    uint8_t *p2  = ctx + OFF_P2;
    uint8_t *p3  = ctx + OFF_P3;
    uint8_t *p4  = ctx + OFF_P4;
    uint8_t *p5  = ctx + OFF_P5;
    uint8_t *p6  = ctx + OFF_P6;
    uint8_t *p7  = ctx + OFF_P7;
    uint8_t *p8  = ctx + OFF_P8;
    uint8_t *p9  = ctx + OFF_P9;

    do {
        uint16_t mask[8];

        swap_pair16_6(p8, p9, 1, mask);
        swap_pair16_6(p4, p5, 0, mask);

        swap_pair32_6(p0, p1, mask);
        swap_pair16_6(p6, p7, 0, mask);

        swap_pair32_6(p2, p3, mask);

        p0 += 32; p1 += 32; p2 += 32; p3 += 32; p4 += 32;
        p5 += 32; p6 += 32; p7 += 32; p8 += 32; p9 += 32;

        n -= 8;
    } while (n > 0);
#endif
}
#undef OFF_P0
#undef OFF_P1
#undef OFF_P2
#undef OFF_P3
#undef OFF_P4
#undef OFF_P5
#undef OFF_P6
#undef OFF_P7
#undef OFF_P8
#undef OFF_P9
#undef CLAMP16



void gpu3d_raster_deinterleave_words_blocks(unsigned char *dst,
                          const unsigned char *src)
{

    const uint32_t mask = 0x1fffffffu;
    const unsigned char *pSrc = src;
    unsigned char *pDst = dst;

    unsigned char *pDstHigh = dst + RECON_3D_WIDTH * 2u;
    unsigned int blocks = RECON_BAND_ROWS;

    do {
        unsigned int words = RECON_3D_WIDTH;

        do {
            uint32_t group[32];
            int i;

            for (i = 0; i < 32; i++) {
                group[i] = rd32(pSrc + i * 4);
            }
            pSrc += 128;

            for (i = 0; i < 16; i++) {
                wr32(pDst + i * 4, group[i * 2] & mask);
            }
            pDst += 64;

            for (i = 0; i < 16; i++) {
                wr32(pDstHigh + i * 4, group[i * 2 + 1] & mask);
            }
            pDstHigh += 64;

            words -= 0x20;
        } while (words != 0);

        blocks -= 1;
        pDst += RECON_3D_WIDTH * 2u;
        pDstHigh += RECON_3D_WIDTH * 2u;
    } while (blocks != 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#ifndef __ARM_NEON

static int16_t recon_f4e0_saturate16(int32_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static int16_t recon_f4e0_shift_sat(int16_t v, int s)
{
    if (s >= 0) {
        if (s >= 16) return (v == 0) ? 0 : (v > 0 ? 32767 : -32768);
        return recon_f4e0_saturate16((int32_t)v << s);
    }
    {
        int d = -s;
        if (d >= 16) return (v < 0) ? -1 : 0;
        return (int16_t)(v >> d);
    }
}
#endif

void gpu3d_raster_fog_density_line_512(const unsigned int *param_1, unsigned char *param_2,
                         const unsigned char *param_3, uint32_t param_4)
{

#ifdef __ARM_NEON

    {
        uint8x16x2_t tb, tp;
        tb.val[0] = vld1q_u8(param_3);      tb.val[1] = vld1q_u8(param_3 + 16);
        tp.val[0] = vld1q_u8(param_3 + 32); tp.val[1] = vld1q_u8(param_3 + 48);
        uint16x8_t vses = vdupq_n_u16((uint16_t)(param_4 >> 16));
        int16x8_t  vdes = vdupq_n_s16((int16_t)param_4);

        const unsigned char *fu = (const unsigned char *)param_1;
        unsigned char *sa = param_2;
        const uint16x8_t m7f = vdupq_n_u16(0x7fff), m3ff = vdupq_n_u16(0x03ff);

        for (int g = 0; g < (int)(RECON_3D_WIDTH / 32u); g++) {
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

    const unsigned char *table_base = param_3;
    const unsigned char *table_pend = param_3 + 32;
    uint16_t bias = (uint16_t)(param_4 >> 16);

    int shift = (int)(int8_t)(uint8_t)param_4;

    int n;
    for (n = 0; n < (int)RECON_3D_WIDTH; n++) {
        uint32_t word = param_1[n];

        uint16_t t0 = (uint16_t)((word >> 9) & 0x7fffu);
        uint16_t t1 = (uint16_t)((t0 >= bias) ? (t0 - bias) : 0);
        uint16_t r  = (uint16_t)recon_f4e0_shift_sat((int16_t)t1, shift);

        uint8_t idx  = (uint8_t)((r >> 10) & 0x3fu);
        int16_t weight = (int16_t)(r & 0x3ffu);

        uint8_t base = (idx < 32) ? table_base[idx] : 0;
        uint8_t pend = (idx < 32) ? table_pend[idx] : 0;

        int16_t te = recon_f4e0_saturate16((int32_t)(int8_t)pend << 5);
        int16_t prod = recon_f4e0_saturate16(((int32_t)te * (int32_t)weight * 2) >> 16);

        param_2[n] = (unsigned char)(base + (uint8_t)prod);
    }
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#ifdef __ARM_NEON

static inline uint8x16_t f948_wins(uint8x16_t n0, uint8x16_t n1,
                                   uint8x16_t n2, uint8x16_t n3,
                                   uint8x16_t c0, uint8x16_t c1,
                                   uint8x16_t c2, uint8x16_t c3) {
    n3 = vandq_u8(n3, vdupq_n_u8(0x3f));
    uint8x16_t e2 = vceqq_u8(c2, n2);
    uint8x16_t e1 = vceqq_u8(c1, n1);
    uint8x16_t g2 = vcgtq_u8(n2, c2);
    uint8x16_t g1 = vcgtq_u8(n1, c1);
    uint8x16_t g0 = vcgtq_u8(n0, c0);
    uint8x16_t t1 = vandq_u8(e2, g1);
    uint8x16_t t0 = vandq_u8(vandq_u8(e2, e1), g0);
    uint8x16_t r  = vorrq_u8(vorrq_u8(g2, t1), t0);
    return vbicq_u8(r, vceqq_u8(c3, n3));
}
#endif
#ifndef __ARM_NEON

static int wins_f948(const unsigned char *n, uint8_t c0, uint8_t c1,
                    uint8_t c2, uint8_t c3) {
    uint8_t n0 = n[0], n1 = n[1], n2 = n[2];
    uint8_t n3 = (uint8_t)(n[3] & 0x3f);
    int g = (n2 > c2)
         || (n2 == c2 && n1 > c1)
         || (n2 == c2 && n1 == c1 && n0 > c0);
    if (c3 == n3) g = 0;
    return g;
}
#endif

void gpu3d_raster_test_neighbor_edge_dual_x2(unsigned char *dst, const unsigned char *va,
                        const unsigned char *center, const unsigned char *vb,
                        uint32_t edge) {

    unsigned char brd[4];
    brd[0] = (uint8_t)edge;
    brd[1] = (uint8_t)(edge >> 8);
    brd[2] = (uint8_t)(edge >> 16);
    brd[3] = (uint8_t)(edge >> 24);

#ifdef __ARM_NEON
    {
        uint8x16_t b0 = vdupq_n_u8(brd[0]), b1 = vdupq_n_u8(brd[1]);
        uint8x16_t b2 = vdupq_n_u8(brd[2]), b3 = vdupq_n_u8(brd[3]);
        const unsigned char *pc = center;
        const unsigned char *pi = center + 0x3c;
        const unsigned char *pd = center + 4;
        const int G = (int)RECON_3D_GROUPS;
        for (int g = 0; g < G; g++) {
            uint8x16x4_t v = vld4q_u8(pc); pc += 64;
            uint8x16_t c0 = v.val[0], c1 = v.val[1], c2 = v.val[2];
            uint8x16_t c3 = veorq_u8(vandq_u8(v.val[3], vdupq_n_u8(0x7f)),
                                     vdupq_n_u8(0x40));
            uint8x16_t i0, i1, i2, i3, d0, d1, d2, d3;
            if (g == 0) {
                i0 = vextq_u8(b0, c0, 15); i1 = vextq_u8(b1, c1, 15);
                i2 = vextq_u8(b2, c2, 15); i3 = vextq_u8(b3, v.val[3], 15);
            } else {
                uint8x16x4_t t = vld4q_u8(pi); pi += 64;
                i0 = t.val[0]; i1 = t.val[1]; i2 = t.val[2]; i3 = t.val[3];
            }
            if (g == G - 1) {
                d0 = vextq_u8(c0, b0, 1); d1 = vextq_u8(c1, b1, 1);
                d2 = vextq_u8(c2, b2, 1); d3 = vextq_u8(v.val[3], b3, 1);
            } else {
                uint8x16x4_t t = vld4q_u8(pd); pd += 64;
                d0 = t.val[0]; d1 = t.val[1]; d2 = t.val[2]; d3 = t.val[3];
            }
            uint8x16x4_t a = vld4q_u8(va); va += 64;
            uint8x16x4_t e = vld4q_u8(vb); vb += 64;

            uint8x16_t lives = vmvnq_u8(f948_wins(i0, i1, i2, i3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, f948_wins(d0, d1, d2, d3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, f948_wins(a.val[0], a.val[1], a.val[2], a.val[3],
                                            c0, c1, c2, c3));
            lives = vbicq_u8(lives, f948_wins(e.val[0], e.val[1], e.val[2], e.val[3],
                                            c0, c1, c2, c3));
            vst1q_u8(dst, vorrq_u8(vshrq_n_u8(c3, 3), lives)); dst += 16;
        }
        return;
    }
#else

    for (int p = 0; p < (int)RECON_3D_WIDTH; p++) {
        const unsigned char *c = center + p * 4;
        uint8_t c0 = c[0], c1 = c[1], c2 = c[2];
        uint8_t c3 = (uint8_t)(((uint8_t)(c[3] & 0x7f)) ^ 0x40);

        const unsigned char *left = (p == 0)   ? brd : (center + (p - 1) * 4);
        const unsigned char *right = (p == 511) ? brd : (center + (p + 1) * 4);

        int lives = !wins_f948(left, c0, c1, c2, c3)
                && !wins_f948(right, c0, c1, c2, c3)
                && !wins_f948(va + p * 4, c0, c1, c2, c3)
                && !wins_f948(vb + p * 4, c0, c1, c2, c3);

        dst[p] = (uint8_t)((c3 >> 3) | (lives ? 0xff : 0x00));
    }
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#ifdef __ARM_NEON

static inline uint8x16_t winner_bordered_x2(uint8x16_t n0, uint8x16_t n1,
                                       uint8x16_t n2, uint8x16_t n3,
                                       uint8x16_t c0, uint8x16_t c1,
                                       uint8x16_t c2, uint8x16_t c3) {
    n3 = vandq_u8(n3, vdupq_n_u8(0x3f));
    uint8x16_t e2 = vceqq_u8(c2, n2);
    uint8x16_t e1 = vceqq_u8(c1, n1);
    uint8x16_t g2 = vcgtq_u8(n2, c2);
    uint8x16_t g1 = vcgtq_u8(n1, c1);
    uint8x16_t g0 = vcgtq_u8(n0, c0);
    uint8x16_t t1 = vandq_u8(e2, g1);
    uint8x16_t t0 = vandq_u8(vandq_u8(e2, e1), g0);
    uint8x16_t r  = vorrq_u8(vorrq_u8(g2, t1), t0);
    return vbicq_u8(r, vceqq_u8(c3, n3));
}
#endif
#ifndef __ARM_NEON

static int wins_fcb0(const unsigned char *n, uint8_t c0, uint8_t c1,
                    uint8_t c2, uint8_t c3) {
    uint8_t n0 = n[0], n1 = n[1], n2 = n[2];
    uint8_t n3 = (uint8_t)(n[3] & 0x3f);
    int g = (n2 > c2)
         || (n2 == c2 && n1 > c1)
         || (n2 == c2 && n1 == c1 && n0 > c0);
    if (c3 == n3) g = 0;
    return g;
}
#endif
#define WIDTH_FCB0 ((int)RECON_3D_WIDTH)

void gpu3d_raster_test_neighbor_edge_bordered_x2(unsigned char *dst, const unsigned char *center,
                        const unsigned char *neighbor, uint32_t edge) {

    unsigned char brd[4];
    brd[0] = (uint8_t)edge;
    brd[1] = (uint8_t)(edge >> 8);
    brd[2] = (uint8_t)(edge >> 16);
    brd[3] = (uint8_t)(edge >> 24);

#ifdef __ARM_NEON
    {
        uint8x16_t b0 = vdupq_n_u8(brd[0]), b1 = vdupq_n_u8(brd[1]);
        uint8x16_t b2 = vdupq_n_u8(brd[2]), b3 = vdupq_n_u8(brd[3]);
        const unsigned char *pc = center;
        const unsigned char *pi = center + 0x3c;
        const unsigned char *pd = center + 4;
        const unsigned char *pv = neighbor;
        const int G = (int)RECON_3D_GROUPS;
        for (int g = 0; g < G; g++) {
            uint8x16x4_t v = vld4q_u8(pc); pc += 64;
            uint8x16_t c0 = v.val[0], c1 = v.val[1], c2 = v.val[2];
            uint8x16_t c3 = veorq_u8(vandq_u8(v.val[3], vdupq_n_u8(0x7f)),
                                     vdupq_n_u8(0x40));
            uint8x16_t i0, i1, i2, i3, d0, d1, d2, d3;
            if (g == 0) {
                i0 = vextq_u8(b0, c0, 15); i1 = vextq_u8(b1, c1, 15);
                i2 = vextq_u8(b2, c2, 15); i3 = vextq_u8(b3, v.val[3], 15);
            } else {
                uint8x16x4_t t = vld4q_u8(pi); pi += 64;
                i0 = t.val[0]; i1 = t.val[1]; i2 = t.val[2]; i3 = t.val[3];
            }
            if (g == G - 1) {
                d0 = vextq_u8(c0, b0, 1); d1 = vextq_u8(c1, b1, 1);
                d2 = vextq_u8(c2, b2, 1); d3 = vextq_u8(v.val[3], b3, 1);
            } else {
                uint8x16x4_t t = vld4q_u8(pd); pd += 64;
                d0 = t.val[0]; d1 = t.val[1]; d2 = t.val[2]; d3 = t.val[3];
            }
            uint8x16x4_t w = vld4q_u8(pv); pv += 64;

            uint8x16_t lives = vmvnq_u8(winner_bordered_x2(i0, i1, i2, i3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_x2(d0, d1, d2, d3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_x2(b0, b1, b2, b3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_x2(w.val[0], w.val[1], w.val[2], w.val[3],
                                                c0, c1, c2, c3));
            vst1q_u8(dst, vorrq_u8(vshrq_n_u8(c3, 3), lives)); dst += 16;
        }
        return;
    }
#else
    for (int p = 0; p < WIDTH_FCB0; p++) {
        const unsigned char *c = center + p * 4;
        uint8_t c0 = c[0], c1 = c[1], c2 = c[2];
        uint8_t c3 = (uint8_t)(((uint8_t)(c[3] & 0x7f)) ^ 0x40);

        const unsigned char *left = (p == 0)              ? brd : (center + (p - 1) * 4);
        const unsigned char *right = (p == WIDTH_FCB0 - 1)   ? brd : (center + (p + 1) * 4);

        int lives = !wins_fcb0(left, c0, c1, c2, c3)
                && !wins_fcb0(right, c0, c1, c2, c3)
                && !wins_fcb0(brd, c0, c1, c2, c3)
                && !wins_fcb0(neighbor + p * 4, c0, c1, c2, c3);

        dst[p] = (uint8_t)((c3 >> 3) | (lives ? 0xff : 0x00));
    }
#endif
}
#undef WIDTH_FCB0

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#ifdef __ARM_NEON

static inline uint8x16_t winner_bordered_x2_alt(uint8x16_t n0, uint8x16_t n1,
                                       uint8x16_t n2, uint8x16_t n3,
                                       uint8x16_t c0, uint8x16_t c1,
                                       uint8x16_t c2, uint8x16_t c3) {
    n3 = vandq_u8(n3, vdupq_n_u8(0x3f));
    uint8x16_t e2 = vceqq_u8(c2, n2);
    uint8x16_t e1 = vceqq_u8(c1, n1);
    uint8x16_t g2 = vcgtq_u8(n2, c2);
    uint8x16_t g1 = vcgtq_u8(n1, c1);
    uint8x16_t g0 = vcgtq_u8(n0, c0);
    uint8x16_t t1 = vandq_u8(e2, g1);
    uint8x16_t t0 = vandq_u8(vandq_u8(e2, e1), g0);
    uint8x16_t r  = vorrq_u8(vorrq_u8(g2, t1), t0);
    return vbicq_u8(r, vceqq_u8(c3, n3));
}
#endif
#ifndef __ARM_NEON

static int wins_a28_11(const unsigned char *n, uint8_t c0, uint8_t c1,
                   uint8_t c2, uint8_t c3) {
    uint8_t n0 = n[0], n1 = n[1], n2 = n[2];
    uint8_t n3 = (uint8_t)(n[3] & 0x3f);
    int g = (n2 > c2)
         || (n2 == c2 && n1 > c1)
         || (n2 == c2 && n1 == c1 && n0 > c0);
    if (c3 == n3) g = 0;
    return g;
}
#endif

void gpu3d_raster_test_neighbor_edge_bordered_x2_alt(unsigned char *dst, const unsigned char *center,
                        const unsigned char *neighbor, uint32_t edge) {

    unsigned char brd[4];
    brd[0] = (uint8_t)edge;
    brd[1] = (uint8_t)(edge >> 8);
    brd[2] = (uint8_t)(edge >> 16);
    brd[3] = (uint8_t)(edge >> 24);

#ifdef __ARM_NEON
    {
        uint8x16_t b0 = vdupq_n_u8(brd[0]), b1 = vdupq_n_u8(brd[1]);
        uint8x16_t b2 = vdupq_n_u8(brd[2]), b3 = vdupq_n_u8(brd[3]);
        const unsigned char *pc = center;
        const unsigned char *pi = center + 0x3c;
        const unsigned char *pd = center + 4;
        const unsigned char *pv = neighbor;
        const int G = (int)RECON_3D_GROUPS;
        for (int g = 0; g < G; g++) {
            uint8x16x4_t v = vld4q_u8(pc); pc += 64;
            uint8x16_t c0 = v.val[0], c1 = v.val[1], c2 = v.val[2];
            uint8x16_t c3 = veorq_u8(vandq_u8(v.val[3], vdupq_n_u8(0x7f)),
                                     vdupq_n_u8(0x40));
            uint8x16_t i0, i1, i2, i3, d0, d1, d2, d3;
            if (g == 0) {
                i0 = vextq_u8(b0, c0, 15); i1 = vextq_u8(b1, c1, 15);
                i2 = vextq_u8(b2, c2, 15); i3 = vextq_u8(b3, v.val[3], 15);
            } else {
                uint8x16x4_t t = vld4q_u8(pi); pi += 64;
                i0 = t.val[0]; i1 = t.val[1]; i2 = t.val[2]; i3 = t.val[3];
            }
            if (g == G - 1) {
                d0 = vextq_u8(c0, b0, 1); d1 = vextq_u8(c1, b1, 1);
                d2 = vextq_u8(c2, b2, 1); d3 = vextq_u8(v.val[3], b3, 1);
            } else {
                uint8x16x4_t t = vld4q_u8(pd); pd += 64;
                d0 = t.val[0]; d1 = t.val[1]; d2 = t.val[2]; d3 = t.val[3];
            }
            uint8x16x4_t w = vld4q_u8(pv); pv += 64;

            uint8x16_t lives = vmvnq_u8(winner_bordered_x2_alt(i0, i1, i2, i3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_x2_alt(d0, d1, d2, d3, c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_x2_alt(w.val[0], w.val[1], w.val[2], w.val[3],
                                                c0, c1, c2, c3));
            lives = vbicq_u8(lives, winner_bordered_x2_alt(b0, b1, b2, b3, c0, c1, c2, c3));
            vst1q_u8(dst, vorrq_u8(vshrq_n_u8(c3, 3), lives)); dst += 16;
        }
        return;
    }
#else

    for (int p = 0; p < (int)RECON_3D_WIDTH; p++) {
        const unsigned char *c = center + p * 4;
        uint8_t c0 = c[0], c1 = c[1], c2 = c[2];
        uint8_t c3 = (uint8_t)(((uint8_t)(c[3] & 0x7f)) ^ 0x40);

        const unsigned char *left = (p == 0)   ? brd : (center + (p - 1) * 4);
        const unsigned char *right = (p == 511) ? brd : (center + (p + 1) * 4);

        int lives = !wins_a28_11(left, c0, c1, c2, c3)
                && !wins_a28_11(right, c0, c1, c2, c3)
                && !wins_a28_11(neighbor + p * 4, c0, c1, c2, c3)
                && !wins_a28_11(brd, c0, c1, c2, c3);

        dst[p] = (uint8_t)((c3 >> 3) | (lives ? 0xff : 0x00));
    }
#endif
}

#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "../../gpu.h"
#include "core_internals.h"
#include "mem_access.h"

void gpu3d_raster_span_gap_run_normalize(unsigned char *base,
                        uint32_t turns)
{

    unsigned char *x0 = base;
    uint32_t w1 = turns;
    uint32_t w2 = 0, w3 = 0, w4 = 0, w8 = 0, w9 = 0, w10 = 0, w11 = 0;
    uint32_t w12 = 0, w13 = 0, w14 = 0, w15 = 0, w16 = 0, w17 = 0;
    uint16_t h;
    uint32_t p;

    if (w1 == 0) return;

    w8 = 0x100u;

    memcpy(&h, x0 + 1584, 2); w9  = h;
    memcpy(&h, x0 + 1408, 2); w10 = h;
    if (w9 >= w10) goto L18c;
    goto L1a8;

L0f0:

    memcpy(&p, x0 +    0, 4); w9  = p;
    memcpy(&p, x0 +  176, 4); w10 = p;
    memcpy(&p, x0 +  352, 4); w11 = p;
    memcpy(&p, x0 +  528, 4); w12 = p;
    memcpy(&h, x0 +  704, 2); w13 = h;
    memcpy(&h, x0 +  880, 2); w14 = h;
    memcpy(&h, x0 +  706, 2); w15 = h;
    memcpy(&h, x0 +  882, 2); w16 = h;
    memcpy(&h, x0 + 1056, 2); w17 = h;
    memcpy(&h, x0 + 1232, 2); w2  = h;
    memcpy(&h, x0 + 1058, 2); w3  = h;
    memcpy(&h, x0 + 1234, 2); w4  = h;

    w9 = w10 - w9;
    memcpy(&h, x0 + 1408, 2); w10 = h;
    w11 = w12 - w11;
    memcpy(&h, x0 + 1584, 2); w12 = h;
    w13 = w14 - w13;
    memcpy(&h, x0 + 1410, 2); w14 = h;
    w15 = w16 - w15;
    memcpy(&h, x0 + 1586, 2); w16 = h;
    w17 = w2 - w17;
    w2  = w4 - w3;
    w10 = w12 - w10;
    w12 = w16 - w14;

    p = w9;            memcpy(x0 +  176, &p, 4);
    p = w11;           memcpy(x0 +  528, &p, 4);
    h = (uint16_t)w13; memcpy(x0 +  880, &h, 2);
    h = (uint16_t)w15; memcpy(x0 +  882, &h, 2);
    h = (uint16_t)w17; memcpy(x0 + 1232, &h, 2);
    h = (uint16_t)w2;  memcpy(x0 + 1234, &h, 2);

    w1 = w1 - 1u;

    h = (uint16_t)w10; memcpy(x0 + 1584, &h, 2);
    h = (uint16_t)w12; memcpy(x0 + 1586, &h, 2);

    x0 += 4;

    if (w1 == 0) return;

    memcpy(&h, x0 + 1584, 2); w9  = h;
    memcpy(&h, x0 + 1408, 2); w10 = h;
    if (w9 < w10) goto L1a8;

L18c:
    w10 = w10 & 0xffffu;
    if (w10 >= 0x101u) goto L204;

L198:
    w9 = w9 & 0xffffu;
    if (w9 < 0x101u) goto L0f0;
    goto L214;

L1a8:

    memcpy(&p, x0 +  176, 4); w9  = p;
    memcpy(&p, x0 +    0, 4); w10 = p;
    memcpy(&p, x0 +  352, 4); w11 = p;
    memcpy(&p, x0 +  528, 4); w12 = p;
    p = w9;                   memcpy(x0 +    0, &p, 4);
    memcpy(&p, x0 +  704, 4); w9  = p;
    p = w10;                  memcpy(x0 +  176, &p, 4);
    memcpy(&p, x0 +  880, 4); w10 = p;
    p = w12;                  memcpy(x0 +  352, &p, 4);
    memcpy(&p, x0 + 1056, 4); w12 = p;
    p = w11;                  memcpy(x0 +  528, &p, 4);
    memcpy(&p, x0 + 1232, 4); w11 = p;
    p = w10;                  memcpy(x0 +  704, &p, 4);
    memcpy(&p, x0 + 1584, 4); w10 = p;
    p = w9;                   memcpy(x0 +  880, &p, 4);
    memcpy(&p, x0 + 1408, 4); w9  = p;
    p = w11;                  memcpy(x0 + 1056, &p, 4);
    p = w12;                  memcpy(x0 + 1232, &p, 4);
    p = w10;                  memcpy(x0 + 1408, &p, 4);
    p = w9;                   memcpy(x0 + 1584, &p, 4);

    w10 = w10 & 0xffffu;
    if (w10 < 0x101u) goto L198;

L204:
    h = (uint16_t)w8; memcpy(x0 + 1408, &h, 2);
    w9 = w9 & 0xffffu;
    if (w9 < 0x101u) goto L0f0;

L214:
    h = (uint16_t)w8; memcpy(x0 + 1584, &h, 2);
    goto L0f0;
}

#define OFF_NIBBLE_TABLE 0x10e57cu

static inline int32_t sbfx32(uint32_t val, unsigned lsb, unsigned width)
{
    uint32_t field = (val >> lsb) & ((width >= 32) ? 0xffffffffu : ((1u << width) - 1u));
    uint32_t signbit = 1u << (width - 1);
    return (int32_t)((field ^ signbit) - signbit);
}
#define SCRATCH_SZ 0x7450u

void gpu3d_raster_span_edge_ramp_dispatch(uint8_t *param_1, const gpu3d_polygon_t *param_2,
                         const gpu3d_bank_vertex_t *param_3,
                         uint32_t param_4, uint32_t param_5)
{

    unsigned char scratch[SCRATCH_SZ];
    const gpu3d_bank_vertex_t *local_b8[16];

    const gpu3d_polygon_t *p2 = param_2;

    uint32_t uVar18 = p2->vertex_count;
    gpu3d_band_header_t *coreTbl = (gpu3d_band_header_t *)((uintptr_t)param_1 + GPU3D_BAND_HEADER_OFFSET);
    gpu_t *lVar16 = coreTbl->gpu;

    uint32_t uVar19 = uVar18 & 0xfu;
    const gpu3d_bank_vertex_t *piVar11_0;

    if (uVar19 == 0) {
        piVar11_0 = (const gpu3d_bank_vertex_t *)scratch;
    } else {
        uint16_t idxBase = p2->first_vertex;
        uint32_t nibbles = rd32((const unsigned char *)gpu3d_vertex_pattern_table
                                   + 4u * ((uVar18 >> 16) & 0x7fu));
        for (uint32_t i = 0; i < uVar19; i++) {
            local_b8[i] = param_3 + idxBase + (nibbles & 0xfu);
            nibbles >>= 4;
        }
        piVar11_0 = local_b8[0];
    }
    local_b8[uVar19] = piVar11_0;

    uint32_t uVar21 = (uVar18 >> 8) & 0xffu;
    const gpu3d_bank_vertex_t *node0 = local_b8[0];
    uint32_t uVar28 = node0->y;

    uint8_t *tail_x0 = param_1;

    const unsigned char *tail_x1 = scratch;
    uint32_t tail_w4 = 0, tail_w5 = 0, tail_w6 = 0;

    if (((uVar18 >> 14) & 1u) != 0) {
        goto flow_timing;
    }

    {
        uint32_t uVar18s = (uVar18 >> 23) & 0x1ffu;
        uint32_t bVar8 = (uVar28 < param_4) ? 1u : 0u;
        uint32_t iVar6_pre = param_4 - uVar28;
        uint32_t iVar15 = bVar8 ? (0u - iVar6_pre) : 0u;
        uint32_t iVar6 = bVar8 ? iVar6_pre : 0u;
        uint32_t iVar22 = (uVar18s < param_5) ? 0u : (uVar18s - param_5);
        uint32_t uVar26 = iVar15 + (uVar18s - uVar28) - iVar22;
        uint32_t uVar17 = (uVar18s > param_5) ? param_5 : uVar18s;

        if ((int32_t)uVar26 < 1) {
            goto epilogue;
        }

        unsigned char flagA = (unsigned char)lVar16->raster.disp3dcnt;

        tail_w5 = uVar26;
        tail_w6 = uVar21;

        if ((flagA & 0x20u) == 0) {

            gpu3d_raster_line_window_select_and_ramp(param_1, scratch + 0x000u,
                                scratch + 0x6e0u,
                                local_b8, param_4, uVar17, 1, uVar21);
            gpu3d_raster_line_window_select_and_ramp(param_1, scratch + 0x0b0u,
                                scratch + 0x6e0u,
                                &local_b8[uVar19],
                                param_4, uVar17, -1, uVar21);

            uint32_t clip2 = (param_4 < uVar28) ? param_4 : uVar28;
            uint32_t cnt = iVar22 + param_4 + uVar28 - clip2 - uVar18s;

            {
                unsigned char *a = scratch + 0x580u;
                unsigned char *b = scratch + 0x630u;
                for (;;) {
                    uint16_t va = rd16(a), vb = rd16(b);
                    int follows = (cnt != 0xffffffffu);
                    cnt = cnt + 1u;
                    va &= 0x7fffu; vb &= 0x7fffu;
                    wr16(a, va); wr16(b, vb);
                    a += 4; b += 4;
                    if (!follows) break;
                }
            }

            gpu3d_raster_swap_linked_pair_channels(scratch, (int32_t)uVar26);

            tail_w4 = (iVar6 + uVar28) - param_4;
        } else {

            uint32_t piVar11_off = (uVar28 < param_4) ? 4u : 0u;
            tail_x1 = scratch + piVar11_off;
            uint32_t iVarY15 = param_4 - bVar8;
            uint32_t boolp5 = (param_5 < uVar18s) ? 1u : 0u;
            uint32_t uVar17b = uVar17 + boolp5;
            uint32_t iVar23 = uVar26 + bVar8 + boolp5;
            gpu3d_raster_line_window_select_and_ramp(param_1, scratch + 0x000u,
                                scratch + 0x6e0u,
                                local_b8, iVarY15, uVar17b, 1, uVar21);
            gpu3d_raster_line_window_select_and_ramp(param_1, scratch + 0x0b0u,
                                scratch + 0x6e0u,
                                &local_b8[uVar19],
                                iVarY15, uVar17b, -1, uVar21);

            if (iVar23 != 0) {
                uint32_t clip2b = (param_4 < uVar28) ? param_4 : uVar28;
                uint32_t rampCnt = clip2b + uVar18s + bVar8 + boolp5
                                    - iVar22 - param_4 - uVar28;
                unsigned char *a = scratch + 0x580u;
                unsigned char *b = scratch + 0x630u;
                uint32_t cnt = rampCnt;
                for (;;) {
                    uint16_t ra = rd16(a), rb = rd16(b);
                    uint32_t am = ra & 0x7fffu, bm = rb & 0x7fffu;
                    if (bm >= am) {
                        uint32_t bm2 = ((rb & 0x8100u) == 0) ? bm + 1u : bm;
                        wr16(a, (uint16_t)am); wr16(b, (uint16_t)bm2);
                    } else {
                        uint32_t am2 = ((ra & 0x8100u) == 0) ? am + 1u : am;
                        wr16(a, (uint16_t)am2); wr16(b, (uint16_t)bm);
                    }
                    a += 4; b += 4;
                    cnt -= 1u;
                    if (cnt == 0u) break;
                }
            }

            gpu3d_raster_swap_linked_pair_channels(scratch, (int32_t)iVar23);

            uint32_t w8v, w9v, w10v;
            if (uVar28 < param_4) {
                w10v = rd16(scratch + piVar11_off + 1404);
                w9v  = rd16(scratch + piVar11_off + 1580);
                w8v  = rd16(scratch + piVar11_off + 1408);
                w9v += w10v;
            } else {
                w8v  = rd16(scratch + piVar11_off + 1408);
                w9v  = rd16(scratch + piVar11_off + 1588);
                w10v = w8v;
                w9v += w8v;
            }
            (void)w10v;

            uint32_t w11bool = (uVar18s <= param_5) ? 1u : 0u;
            size_t x12off = (size_t)piVar11_off + 0x630u;

            if (uVar26 == w11bool) {

                size_t x8off = (size_t)piVar11_off + 0x6e0u;
                if (uVar18s <= param_5) {
                    uint32_t w9u = rd16(scratch + x12off);
                    wr16(scratch + x8off + 2, 0);
                    w9u += 1u;
                    wr16(scratch + x8off, (uint16_t)w9u);
                }
            } else {
                uint32_t w14a = rd16(scratch + x12off);

                uint32_t w13min = (param_4 < uVar28) ? param_4 : uVar28;
                uint32_t w11s = (iVar22 + param_4 + uVar28) + w11bool - w13min - uVar18s;
                uint32_t w13v = w8v + w14a;
                uint32_t r12  = w11s + 1u;

                size_t x11off = (size_t)piVar11_off + 0x584u;
                uint32_t r8c = w8v, r10c = w10v, r13c = w13v, r9c = w9v, r14c = w14a;

                for (;;) {
                    uint32_t v15 = rd16(scratch + x11off);
                    uint32_t v17 = r8c + 1u;
                    uint32_t v16 = rd16(scratch + x11off + 176);

                    uint32_t nv10 = (v17 < r10c) ? r10c : (r8c + 1u);
                    uint32_t nv0  = r13c - 1u;
                    nv10 = ((int32_t)nv10 < (int32_t)v15) ? v15 : nv10;
                    uint32_t nv9 = ((int32_t)nv0 > (int32_t)r9c) ? r9c : nv0;
                    v16 = v16 + v15;
                    nv9 = ((int32_t)nv9 > (int32_t)v16) ? v16 : nv9;
                    uint32_t w14masked = r14c & 0xffffu;
                    nv10 = nv10 - r8c;
                    nv9  = r13c - nv9;
                    nv10 = (nv10 > w14masked) ? w14masked : nv10;
                    nv9  = (nv9 > w14masked) ? w14masked : nv9;

                    wr16(scratch + x11off + 348, (uint16_t)nv10);
                    wr16(scratch + x11off + 350, (uint16_t)nv9);

                    if (r12 == 0) break;

                    uint32_t nextw14 = rd16(scratch + x11off + 176);
                    r12 += 1;
                    x11off += 4;
                    r9c = r13c;
                    r10c = r8c;
                    r13c = v16;
                    r8c = v15;
                    r14c = nextw14;
                }

                size_t x12off2 = x11off + 0xb0u;
                size_t x8off2  = x11off + 0x160u;
                if (uVar18s <= param_5) {
                    uint32_t w9u = rd16(scratch + x12off2);
                    wr16(scratch + x8off2 + 2, 0);
                    w9u += 1u;
                    wr16(scratch + x8off2, (uint16_t)w9u);
                }
            }

            tail_w4 = (iVar6 + uVar28) - param_4;
        }
    }

    gpu3d_raster_bands_group_runs(tail_x0, (unsigned char *)tail_x1, param_2,
                        scratch + 0x840u,
                        (int)tail_w4, (int)tail_w5, tail_w6,
                        local_b8[0]);
    goto epilogue;

flow_timing:
    {
        const gpu3d_bank_vertex_t *node1 = local_b8[1];
        uint32_t node1_f6 = node1->y;
        const gpu3d_bank_vertex_t *pxA, *pxB, *pxC;

        if (node1_f6 == uVar28) {
            uint32_t node1_f4 = node1->x;
            uint32_t node0_f4 = local_b8[0]->x;
            if (node1_f4 > node0_f4) {
                pxA = local_b8[0];
                pxC = local_b8[1];
                pxB = local_b8[3];
            } else {
                pxA = local_b8[1];
                pxB = local_b8[2];
                pxC = local_b8[0];
            }
        } else {
            uint32_t node3_f4 = local_b8[3]->x;
            uint32_t node0_f4 = local_b8[0]->x;
            if (node3_f4 <= node0_f4) {
                pxA = local_b8[3];
                pxB = local_b8[2];
                pxC = local_b8[0];
            } else {
                pxA = local_b8[0];
                pxC = local_b8[3];
                pxB = local_b8[1];
            }
        }

        gpu3d_t *objB = coreTbl->gpu3d;
        unsigned char flagB = objB->swap_params_previous;

        uint32_t f4_A  = pxA->x;
        uint32_t f12_A = pxA->s;
        uint32_t f14_A = pxA->t;
        uint32_t f6_B  = pxB->y;
        uint32_t f4_C  = pxC->x;

        uint32_t w19v;
        if ((flagB & 0x2u) != 0) {
            w19v = (uint32_t)pxA->w;
        } else {
            uint32_t f8_A = pxA->z;
            w19v = f8_A << 9;
        }
        uint32_t w25v = (uint32_t)sbfx32(f14_A, 4, 12);
        uint32_t w12v = f6_B - uVar28;

        if (uVar28 < param_4) {
            uint32_t w13v = param_4 - uVar28;
            w25v += w13v;
            w12v -= w13v;
            uVar28 = param_4;
        }

        uint32_t w11clamp = (f6_B < param_5) ? 0u : (f6_B - param_5);
        uint32_t w20t = w12v - w11clamp;
        if (w20t == 0u) {
            goto epilogue;
        }

        uint32_t colorIn = pxA->color;
        uint32_t w26t = uVar21;
        uint32_t w24t = f4_C - f4_A;
        uint32_t w27t = (uint32_t)sbfx32(f12_A, 4, 12);
        uint32_t w22t = uVar28 - param_4;
        uint32_t w21t = f4_A;

        uint32_t colorOut = gpu3d_geometry_color_expand_bgr555_to_rgb8(colorIn);

        uint32_t mulv = w20t * w24t;

        if (mulv < 0x201u) {
            gpu3d_raster_span_row_compose(param_1, param_2, scratch + 0x840u,
                    w21t, w22t, w27t, w25v, w24t,
                    w20t, w19v, colorOut, w26t);
        } else {
            uint32_t stepMax = (w24t != 0u) ? ((w24t + 0x1ffu) / w24t) : 0u;
            uint32_t accW22 = w22t, accW25 = w25v, remW20 = w20t;
            for (;;) {
                uint32_t step = (remW20 < stepMax) ? remW20 : stepMax;
                gpu3d_raster_span_row_compose(param_1, param_2, scratch + 0x840u,
                        w21t, accW22, w27t, accW25, w24t,
                        step, w19v, colorOut, w26t);
                accW22 += step;
                remW20 -= step;
                accW25 += step;
                if (remW20 == 0u) break;
            }
        }
    }

epilogue:
    return;
}
#undef OFF_NIBBLE_TABLE
#undef SCRATCH_SZ

static inline int neighbor_is_foreign(uint32_t field, uint32_t curval, uint32_t idcur)
{
    uint32_t valn = field & 0xFFFFFFu;
    uint32_t idn  = (field >> 24) & 0x3Fu;
    return (curval < valn) && (idcur != idn);
}

void gpu3d_raster_span_edge_neighbor_mask(uint8_t *output, const uint32_t *param_2,
                         const uint32_t *param_3, const uint32_t *param_4,
                         uint32_t param_5)
{

    for (int k = 0; k < 256; k++) {
        uint32_t cur = param_3[k];

        if ((cur & 0x40000000u) == 0u) {
            output[k] = 0xFFu;
            continue;
        }

        uint32_t curval = cur & 0xFFFFFFu;
        uint32_t idcur  = ((cur >> 24) & 0x7Fu) ^ 0x40u;

        uint32_t pred = (k == 0)   ? param_5 : param_3[k - 1];
        uint32_t succ = (k == 255) ? param_5 : param_3[k + 1];
        uint32_t b2 = param_2[k];
        uint32_t b4 = param_4[k];

        int bad = neighbor_is_foreign(pred, curval, idcur) |
                   neighbor_is_foreign(succ, curval, idcur) |
                   neighbor_is_foreign(b2,   curval, idcur) |
                   neighbor_is_foreign(b4,   curval, idcur);

        uint8_t base = bad ? 0x00u : 0xFFu;
        output[k] = (uint8_t)(base | (idcur >> 3));
    }
}

void gpu3d_raster_span_edge_color_table_build(unsigned char *dst, const unsigned char *src,
                        const unsigned char *type, const unsigned char *pal) {
    for (uint64_t i = 0; i < 0x100; i++) {
        uint32_t t = type[i];
        uint32_t v;
        if (t <= 7u) {
            uint32_t c0 = pal[t];
            const unsigned char *q = pal + t;
            uint32_t c1 = q[8], c2 = q[16];
            v = c0;
            v = (v & ~0x0000ff00u) | ((c1 & 0xffu) << 8);
            v = (v & ~0x00ff0000u) | ((c2 & 0xffu) << 16);
            v |= 0x1f000000u;
        } else {
            memcpy(&v, src + i * 4, 4);
        }
        v &= 0x1fffffffu;
        memcpy(dst + i * 4, &v, 4);
    }
}

static inline uint32_t a85c_ld16(const unsigned char *p, unsigned off)
{
    uint16_t v;
    memcpy(&v, p + off, 2);
    return v;
}

void gpu3d_raster_span_normalize_ranges(unsigned char *state,
                        uint32_t count)
{

    if (count == 0) return;

    const uint32_t cap = 0x200;

    uint32_t a = a85c_ld16(state, 1584);
    uint32_t b = a85c_ld16(state, 1408);

    for (;;) {
        if (a < b) {

            uint32_t t9, t10, t11, t12;

            t9  = rd32_at(state, 176);
            t10 = rd32_at(state, 0);
            t11 = rd32_at(state, 352);
            t12 = rd32_at(state, 528);

            wr32_at(state, 0, t9);
            t9  = rd32_at(state, 704);
            wr32_at(state, 176, t10);
            t10 = rd32_at(state, 880);
            wr32_at(state, 352, t12);
            t12 = rd32_at(state, 1056);
            wr32_at(state, 528, t11);
            t11 = rd32_at(state, 1232);
            wr32_at(state, 704, t10);
            t10 = rd32_at(state, 1584);
            wr32_at(state, 880, t9);
            t9  = rd32_at(state, 1408);
            wr32_at(state, 1056, t11);
            wr32_at(state, 1232, t12);
            wr32_at(state, 1408, t10);
            wr32_at(state, 1584, t9);

            b = t10 & 0xffffu;
            a = t9  & 0xffffu;
        } else {
            b &= 0xffffu;
            a &= 0xffffu;
        }

        if (b >= 0x201u) wr16_at(state, 1408, cap);
        if (a >= 0x201u) wr16_at(state, 1584, cap);

        {
            uint32_t v9, v10, v11, v12, v13, v14, v15, v16, v17, v2, v3, v4;

            v9  = rd32_at(state, 0);
            v10 = rd32_at(state, 176);
            v11 = rd32_at(state, 352);
            v12 = rd32_at(state, 528);
            v13 = a85c_ld16(state, 704);
            v14 = a85c_ld16(state, 880);
            v15 = a85c_ld16(state, 706);
            v16 = a85c_ld16(state, 882);
            v17 = a85c_ld16(state, 1056);
            v2  = a85c_ld16(state, 1232);
            v3  = a85c_ld16(state, 1058);
            v4  = a85c_ld16(state, 1234);

            v9  = v10 - v9;
            v10 = a85c_ld16(state, 1408);
            v11 = v12 - v11;
            v12 = a85c_ld16(state, 1584);
            v13 = v14 - v13;
            v14 = a85c_ld16(state, 1410);
            v15 = v16 - v15;
            v16 = a85c_ld16(state, 1586);
            v17 = v2 - v17;
            v2  = v4 - v3;
            v10 = v12 - v10;
            v12 = v16 - v14;

            wr32_at(state, 176, v9);
            wr32_at(state, 528, v11);
            wr16_at(state, 880, v13);
            wr16_at(state, 882, v15);
            wr16_at(state, 1232, v17);
            wr16_at(state, 1234, v2);
            count -= 1;
            wr16_at(state, 1584, v10);
            wr16_at(state, 1586, v12);
        }

        state += 4;
        if (count == 0) return;

        a = a85c_ld16(state, 1584);
        b = a85c_ld16(state, 1408);
    }
}





static int32_t sbfx12(uint32_t v)
{
    uint32_t f = (v >> 4) & 0xfffu;
    return (int32_t)((f ^ 0x800u) - 0x800u);
}

void gpu3d_raster_span_edge_ramp_dispatch_hires(uint8_t *param_1, const gpu3d_polygon_t *param_2,
                         const gpu3d_bank_vertex_t *param_3,
                         uint32_t param_4, uint32_t param_5)
{

    const unsigned scratch_size = 0x7450u + (recon_scale_3d - 1u) * 0x6738u
                               + 12u * (RECON_3D_ROW_ARRAY - 0xb0u);
    unsigned char scratch[scratch_size];
    const gpu3d_bank_vertex_t *nodes[16];
    const gpu3d_polygon_t *p2 = param_2;
    gpu3d_band_header_t *table_ptr = GPU3D_BAND_HEADER(param_1);
    uint32_t flags = p2->vertex_count;
    uint32_t count_nodes = flags & 0xfu;
    const gpu3d_bank_vertex_t *first;

    if (count_nodes == 0) {
        first = (const gpu3d_bank_vertex_t *)scratch;
    } else {
        uint16_t base = p2->first_vertex;
        uint32_t bits = rd32((const unsigned char *)gpu3d_vertex_pattern_table
                              + 4u * ((flags >> 16) & 0x7fu));
        for (uint32_t i = 0; i < count_nodes; ++i) {
            nodes[i] = param_3 + base + (bits & 0xfu);
            bits >>= 4;
        }
        first = nodes[0];
    }
    nodes[count_nodes] = first;

    uint32_t flag = (flags >> 8) & 0xffu;
    uint32_t limit0 = nodes[0]->y;

    if ((flags & 0x4000u) == 0) {
        uint32_t height = recon_height_get((const unsigned char *)p2, (flags >> 23) & 0x1ffu);
        uint32_t smaller = limit0 < param_4;
        uint32_t excess = param_4 - limit0;
        uint32_t start = smaller ? excess : 0u;
        uint32_t negative = smaller ? 0u - excess : 0u;
        uint32_t clip = height < param_5 ? 0u : height - param_5;
        uint32_t width = negative + (height - limit0) - clip;
        uint32_t until = height > param_5 ? param_5 : height;

        if ((int32_t)width < 1)
            return;

        unsigned char flag_core = (unsigned char)table_ptr->gpu->raster.disp3dcnt;
        if ((flag_core & 0x20u) == 0) {
            gpu3d_raster_line_window_select_and_ramp(param_1, scratch,
                                scratch + 10u * RECON_3D_ROW_ARRAY, nodes,
                                param_4, until, 1, flag);
            gpu3d_raster_line_window_select_and_ramp(param_1, scratch + RECON_3D_ROW_ARRAY,
                                scratch + 10u * RECON_3D_ROW_ARRAY,
                                &nodes[count_nodes], param_4, until, -1, flag);

            uint32_t min = param_4 < limit0 ? param_4 : limit0;
            uint32_t n = clip + param_4 + limit0 - min - height;
            unsigned char *a = scratch + 8u * RECON_3D_ROW_ARRAY;
            unsigned char *b = scratch + 9u * RECON_3D_ROW_ARRAY;
            for (;;) {
                uint16_t va = rd16(a);
                uint16_t vb = rd16(b);
                uint32_t follows = n != UINT32_MAX;
                n++;
                wr16(a, va & 0x7fffu);
                wr16(b, vb & 0x7fffu);
                a += 4;
                b += 4;
                if (!follows)
                    break;
            }
            gpu3d_raster_swap_linked_pair_channels_x2(scratch, (int32_t)width);

            gpu3d_raster_span_run_batch_flush_hires(param_1, scratch, param_2,
                                scratch + 12u * RECON_3D_ROW_ARRAY,
                                (int)((start + limit0) - param_4), (int)width,
                                flag, nodes[0]);
            return;
        }

        unsigned char *cursor = scratch + (smaller ? 4u : 0u);
        uint32_t len = param_4 - smaller;
        uint32_t extra = param_5 < height;
        uint32_t until2 = until + extra;
        uint32_t total = width + smaller + extra;
        gpu3d_raster_line_window_select_and_ramp(param_1, scratch,
                            scratch + 10u * RECON_3D_ROW_ARRAY, nodes,
                            len, until2, 1, flag);
        gpu3d_raster_line_window_select_and_ramp(param_1, scratch + RECON_3D_ROW_ARRAY,
                            scratch + 10u * RECON_3D_ROW_ARRAY,
                            &nodes[count_nodes], len, until2, -1, flag);

        if (total != 0) {
            uint32_t min = param_4 < limit0 ? param_4 : limit0;
            uint32_t n = min + height + smaller + extra - clip - param_4 - limit0;
            unsigned char *a = scratch + 8u * RECON_3D_ROW_ARRAY;
            unsigned char *b = scratch + 9u * RECON_3D_ROW_ARRAY;
            for (;;) {
                uint16_t ra = rd16(a);
                uint16_t rb = rd16(b);
                uint32_t aa = ra & 0x7fffu;
                uint32_t bb = rb & 0x7fffu;
                if (bb >= aa) {

                    if ((rb & (0x8000u | RECON_3D_WIDTH)) == 0)
                        ++bb;
                    wr16(a, (uint16_t)aa);
                    wr16(b, (uint16_t)bb);
                } else {
                    if ((ra & (0x8000u | RECON_3D_WIDTH)) == 0)
                        ++aa;
                    wr16(a, (uint16_t)aa);
                    wr16(b, (uint16_t)bb);
                }
                a += 4;
                b += 4;
                if (--n == 0)
                    break;
            }
        }
        gpu3d_raster_swap_linked_pair_channels_x2(scratch, (int32_t)total);

        uint32_t r8, r9, r10;
        if (limit0 < param_4) {
            r10 = rd16(cursor + 8u * RECON_3D_ROW_ARRAY - 4u);
            r9 = rd16(cursor + 9u * RECON_3D_ROW_ARRAY - 4u) + r10;
            r8 = rd16(cursor + 8u * RECON_3D_ROW_ARRAY);
        } else {
            r8 = rd16(cursor + 8u * RECON_3D_ROW_ARRAY);
            r9 = rd16(cursor + 9u * RECON_3D_ROW_ARRAY + 4u) + r8;
            r10 = r8;
        }

        uint32_t equal = height <= param_5;
        if (width != equal) {
            uint32_t r14 = rd16(cursor + 9u * RECON_3D_ROW_ARRAY);

            uint32_t min_p4 = param_4 < limit0 ? param_4 : limit0;
            uint32_t r11 = clip + param_4 + limit0 + equal - min_p4 - height;
            uint32_t counter = r11 + 1;
            size_t pos = (size_t)(cursor - scratch) + 8u * RECON_3D_ROW_ARRAY + 4u;
            uint32_t r13 = r8 + r14;
            for (;;) {
                uint32_t v15 = rd16(scratch + pos);
                uint32_t v16 = rd16(scratch + pos + RECON_3D_ROW_ARRAY) + v15;
                uint32_t new10 = r8 + 1u < r10 ? r10 : r8 + 1u;
                uint32_t new0 = r13 - 1u;
                if ((int32_t)new10 < (int32_t)v15)
                    new10 = v15;
                if ((int32_t)new0 > (int32_t)r9)
                    new0 = r9;
                if ((int32_t)new0 > (int32_t)v16)
                    new0 = v16;
                new10 -= r8;
                new0 = r13 - new0;
                r14 &= 0xffffu;
                if (new10 > r14)
                    new10 = r14;
                if (new0 > r14)
                    new0 = r14;
                wr16(scratch + pos + 2u * RECON_3D_ROW_ARRAY - 4u, (uint16_t)new10);
                wr16(scratch + pos + 2u * RECON_3D_ROW_ARRAY - 2u, (uint16_t)new0);
                if (counter == 0)
                    break;
                r14 = rd16(scratch + pos + RECON_3D_ROW_ARRAY);
                ++counter;
                pos += 4;
                r9 = r13;
                r10 = r8;
                r13 = v16;
                r8 = v15;
            }
            if (height <= param_5) {
                uint32_t v = rd16(scratch + pos + RECON_3D_ROW_ARRAY);
                wr16(scratch + pos + 2u * RECON_3D_ROW_ARRAY + 2u, 0);
                wr16(scratch + pos + 2u * RECON_3D_ROW_ARRAY, (uint16_t)(v + 1u));
            }
        } else if (height <= param_5) {
            uint32_t v = rd16(cursor + 9u * RECON_3D_ROW_ARRAY);
            wr16(cursor + 10u * RECON_3D_ROW_ARRAY + 2u, 0);
            wr16(cursor + 10u * RECON_3D_ROW_ARRAY, (uint16_t)(v + 1u));
        }
        gpu3d_raster_span_run_batch_flush_hires(param_1, scratch + (smaller ? 4u : 0u), param_2,
                            scratch + 12u * RECON_3D_ROW_ARRAY,

                            (int)((start + limit0) - param_4), (int)width,
                            flag, nodes[0]);
        return;
    }

    const gpu3d_bank_vertex_t *n1 = nodes[1];
    const gpu3d_bank_vertex_t *a;
    const gpu3d_bank_vertex_t *b;
    const gpu3d_bank_vertex_t *c;
    if (n1->y == limit0) {
        if (n1->x > nodes[0]->x) {
            a = nodes[0];
            b = nodes[3];
            c = n1;
        } else {
            a = n1;
            b = nodes[2];
            c = nodes[0];
        }
    } else if (nodes[3]->x
               <= nodes[0]->x) {
        a = nodes[3];
        b = nodes[2];
        c = nodes[0];
    } else {
        a = nodes[0];
        b = n1;
        c = nodes[3];
    }

    uint32_t f4a = a->x;
    uint32_t f6b = b->y;
    uint32_t f4c = c->x;
    uint32_t f12a = a->s;
    uint32_t f14a = a->t;
    uint32_t value19 = (table_ptr->gpu3d->swap_params_previous & 2u)

                     ? (uint32_t)a->w : (uint32_t)a->z * RECON_3D_WIDTH;
    uint32_t value25 = (uint32_t)sbfx12(f14a);
    uint32_t width = f6b - limit0;
    if (limit0 < param_4) {
        uint32_t d = param_4 - limit0;
        value25 += d;
        width -= d;
        limit0 = param_4;
    }
    uint32_t cut = f6b < param_5 ? 0u : f6b - param_5;
    uint32_t total = width - cut;
    if (total == 0)
        return;

    uint32_t scale = f4c - f4a;
    uint32_t color = gpu3d_geometry_color_expand_bgr555_to_rgb8(a->color);
    uint32_t lateral = limit0 - param_4;
    uint32_t bias = (uint32_t)sbfx12(f12a);

    if (total * scale < 512u * recon_scale_3d + 1u) {
        gpu3d_raster_span_row_compose_hires(param_1, param_2, scratch + 12u * RECON_3D_ROW_ARRAY, f4a,
               lateral, bias, value25, scale, total, value19, color, flag);
        return;
    }
    uint32_t max = (scale + (512u * recon_scale_3d - 1u)) / scale;
    while (total != 0) {
        uint32_t step = total < max ? total : max;
        gpu3d_raster_span_row_compose_hires(param_1, param_2, scratch + 12u * RECON_3D_ROW_ARRAY, f4a,
               lateral, bias, value25, scale, step, value19, color, flag);
        lateral += step;
        total -= step;
        value25 += step;
    }
}


void gpu3d_raster_span_run_batch_flush_hires(uint8_t *a0, unsigned char *base, const gpu3d_polygon_t *a2,
                         uint8_t *a3, int cursor_start, int count,
                         uint32_t flag, const gpu3d_bank_vertex_t *a7)
{

    unsigned char *ptr   = base;
    unsigned char *table = base + 9u * RECON_3D_ROW_ARRAY;
    int cursor   = cursor_start;
    int left   = count;
    int nrun     = 0;
    uint32_t sum = 0;
    uint32_t current = 0;
    uint16_t v;

A:
    memcpy(&v, table, 2);
    if (v != 0) goto B;

    ptr    += 4;
    cursor += 1;
    left -= 1;
    table  += 4;
    if (left != 0) goto A;
    return;

B:
    sum = 0;
    current = v;
    if (sum + current < 256u * recon_scale_3d + 1u) goto G;
    goto C;

C:
    gpu3d_raster_span_band_compose_dispatch_hires(a0, ptr, a2, cursor, nrun, a3, (int)sum, flag, a7);
    ptr    += (unsigned)nrun * 4u;
    cursor += nrun;
    nrun    = 0;
    sum    = 0;
    goto G;

D:
    table += 4;
    memcpy(&v, table, 2);
    if (v == 0) goto E;
    current = v;
    if (sum + current < 256u * recon_scale_3d + 1u) goto G;
    goto C;

G:
    sum += current;
    left -= 1;
    nrun   += 1;
    if (left != 0) goto D;
    if (sum == 0) goto D;
    goto H;

E:
    if (sum == 0) goto A;

    gpu3d_raster_span_band_compose_dispatch_hires(a0, ptr, a2, cursor, nrun, a3, (int)sum, flag, a7);
    ptr    += (unsigned)nrun * 4u;
    cursor += nrun;
    nrun    = 0;
    memcpy(&v, table, 2);
    if (v != 0) goto B;
    ptr    += 4;
    cursor += 1;
    left -= 1;
    table  += 4;
    if (left != 0) goto A;
    return;

H:
    gpu3d_raster_span_band_compose_dispatch_hires(a0, ptr, a2, cursor, nrun, a3, (int)sum, flag, a7);
    return;
}

extern void gpu3d_raster_rgba_force_opaque(unsigned char *px, int64_t n);
extern void gpu3d_raster_blit_row_stride_256(unsigned char *dst, unsigned char *src,
                                uint32_t width, uint32_t rows);
extern void gpu3d_raster_rows_copy_fixed_triple_256(uint32_t *d0, uint32_t *d1, unsigned char *d2,
                                const uint32_t *s0, const uint32_t *s1,
                                const unsigned char *s2, uint32_t width,
                                uint32_t rows);


extern void gpu3d_raster_interp_five_ramps_rows_d718(unsigned char *cap0, uint32_t color,
                                    const unsigned char *org0, unsigned char *cap1,
                                    unsigned char *prio, uint32_t c5,
                                    const unsigned char *code, const unsigned char *plus,
                                    int32_t n);
extern void gpu3d_raster_interp_five_ramps_rows_d7a8(unsigned char *cap0, uint32_t color,
                                    const unsigned char *org0, unsigned char *cap1,
                                    unsigned char *prio, uint32_t c5,
                                    const unsigned char *code, const unsigned char *plus,
                                    int32_t n);
extern void gpu3d_raster_interp_five_ramps_rows_d838(unsigned char *cap0, uint32_t color,
                                    const unsigned char *org0, unsigned char *cap1,
                                    unsigned char *prio, uint32_t c5,
                                    const unsigned char *code, const unsigned char *plus,
                                    int32_t n);
extern void gpu3d_raster_interp_five_ramps_rows_d8e0(unsigned char *cap0, uint32_t color,
                                    const unsigned char *org0, unsigned char *cap1,
                                    unsigned char *prio, uint32_t c5,
                                    const unsigned char *code, const unsigned char *plus,
                                    int32_t n);

void gpu3d_raster_span_row_compose(uint8_t *p1, const gpu3d_polygon_t *p2, uint8_t *p3, uint32_t p4,
                         int32_t p5, int32_t p6, int32_t p7, uint32_t p8,
                         int32_t p9, uint32_t p10, uint32_t p11, uint32_t p12)
{

    uint32_t uVar32  = p8;
    int32_t  iVar33   = p9 * (int32_t)uVar32;
    uint32_t p4lo     = p4;
    uint32_t p5shl8   = ((uint32_t)p5) << 8;
    uint32_t uVar79   = (uint32_t)(iVar33 + 0x16) & 0xfffffff0u;

    uint8_t *lVar48  = p1 + (uint64_t)p5shl8 * 4 + (uint64_t)p4lo * 4;
    uint8_t *lVar41  = GPU3D_BAND(lVar48)->plane1;
    uint8_t *lVar45  = p3 + (uint64_t)uVar79 * 4;
    uint8_t *pcVar52 = p3 + (uint64_t)uVar79 * 13;
    uint8_t *lVar4   = p3 + (uint64_t)uVar79 * 14;
    uint8_t *addr20000 = GPU3D_BAND(p1)->plane2 + (uint64_t)p5shl8 + (uint64_t)p4lo;

    uint32_t uVar7  = p2->polygon_attr;
    uint8_t  byte5  = (uint8_t)(p2->polygon_attr >> 8);

    uint32_t local_6c;

    if ((p12 & 1) == 0) {

        gpu3d_raster_blit_row_stride_256(p3, lVar41, uVar32, p9);
    } else {

        gpu3d_raster_rows_copy_fixed_triple_256((uint32_t *)p3, (uint32_t *)lVar45, lVar4,
                            (const uint32_t *)lVar41, (const uint32_t *)lVar48,
                            (const unsigned char *)addr20000, uVar32, p9);
    }
    if ((byte5 >> 6) & 1) {

        gpu3d_raster_span_key_tolerance_mask(pcVar52, p10, (const uint32_t *)(p3), iVar33, &local_6c);
    } else {

        (void)gpu3d_raster_span_value_over_mask(pcVar52, p10, (const uint32_t *)p3, iVar33,
                                  &local_6c);
    }
    if (local_6c == 0) return;

    const gpu3d_texture_entry_t *lVar46 = p2->texture;
    uint32_t  uVar11   = lVar46->width;
    uint16_t  field48  = lVar46->palette_count;
    uint8_t  *lVar35   = lVar46->data;
    uint32_t *puVar50  = (uint32_t *)(p3 + (uint64_t)uVar79 * 8);

    if (field48 != 0) {

        uint8_t *lut2 = lVar46->palette;
        uint32_t rowOff = uVar11 * (uint32_t)p7;
        uint8_t *srcRow = lVar35 + rowOff + (int64_t)p6;
        if (p9 != 0 && uVar32 != 0) {
            uint32_t *dst = puVar50;
            for (int32_t row = 0; row < p9; row++) {
                for (uint32_t col = 0; col < uVar32; col++)
                    *dst++ = rd32(lut2 + (size_t)srcRow[col] * 4);
                srcRow += uVar11;
            }
        }
    } else {

        uint32_t rowOff = uVar11 * (uint32_t)p7;
        uint8_t *srcRow = lVar35 + (int64_t)rowOff * 4 + (int64_t)p6 * 4;
        if (p9 != 0 && uVar32 != 0) {
            uint32_t *dst = puVar50;
            for (int32_t row = 0; row < p9; row++) {
                for (uint32_t col = 0; col < uVar32; col++)
                    *dst++ = rd32(srcRow + (size_t)col * 4);
                srcRow += (uint64_t)uVar11 * 4;
            }
        }
    }

    gpu_t *lVar49 = GPU3D_BAND_HEADER(p1)->gpu;
    gpu3d_band_header_t *header = (gpu3d_band_header_t *)(p1 + GPU3D_BAND_HEADER_OFFSET);
    uint32_t smallField = lVar49->raster.alpha_test_ref;

    gpu3d_raster_span_component_gate_filter(pcVar52, (const uint8_t *)puVar50, (uint8_t)smallField,
                        iVar33, &local_6c);
    if (local_6c == 0) return;

    if (p11 != 0x3f3f3fu && iVar33 != 0) {
        uint32_t addR = p11 & 0x3f;
        uint32_t addG = (p11 >> 8) & 0x3f;
        uint32_t addB = (p11 >> 16) & 0x3f;
        for (int32_t i = 0; i < iVar33; i++) {
            uint32_t px = puVar50[i];
            uint32_t R = px & 0xff, G = (px >> 8) & 0xff, B = (px >> 16) & 0xff;
            uint32_t A = px & 0xff000000u;
            uint32_t Rp = (R + addR + R * addR) >> 6;
            uint32_t Gp = (G + addG + G * addG) >> 6;
            uint32_t Bp = (B + addB + B * addB) >> 6;
            puVar50[i] = A | Rp | (Gp << 8) | (Bp << 16);
        }
    }

    uint32_t sixbits = (uVar7 >> 24) & 0x3f;
    uint32_t attrWord = (p10 | (sixbits << 24)) | ((uVar7 << 15) & 0x40000000u);

    if ((p12 & 1) == 0) {

        if (uVar7 & 0x8000u) {
            header->band_dirty = 1;

            gpu3d_raster_rgba_force_opaque((unsigned char *)puVar50, iVar33);
        }
        if (p9 != 0 && uVar32 != 0) {
            uint8_t        *pixelPlane = lVar48;
            const uint8_t  *flags = pcVar52;
            const uint32_t *src   = puVar50;
            for (int32_t row = 0; row < p9; row++) {
                for (uint32_t col = 0; col < uVar32; col++) {
                    if (flags[col] != 0) {
                        wr32(pixelPlane + (size_t)col * 4, src[col]);
                        wr32(pixelPlane + GPU3D_BAND_PLANE_SIZE + (size_t)col * 4, attrWord);
                    }
                }
                pixelPlane += 0x400;
                flags += uVar32;
                src += uVar32;
            }
        }
        return;
    }

    uint8_t  style = lVar46->format;
    uint32_t coef   = (uVar7 >> 16) & 0x1f;
    uint8_t *pWork12 = p3 + (uint64_t)uVar79 * 12;

    if (iVar33 != 0) {
        if (style == 6 || style == 1) {
            for (int32_t i = 0; i < iVar33; i++) {
                uint32_t px  = puVar50[i];
                uint32_t idx = (px >> 24) & 0x1f;
                uint32_t nb  = (idx + coef + idx * coef) >> 5;
                puVar50[i] = (px & 0x00ffffffu) | (nb << 24);
            }
        } else {
            uint32_t mask = 0xffffffu | (coef << 24);
            for (int32_t i = 0; i < iVar33; i++) puVar50[i] &= mask;
        }
    }

    if (lVar49->raster.disp3dcnt & 0x08) {

        gpu3d_raster_pixel_blend_alpha((uint8_t *)(puVar50), lVar45, iVar33, pWork12);
    } else {

        gpu3d_raster_pixel_alpha_max((uint8_t *)(puVar50), lVar45, iVar33, pWork12);
    }

    gpu3d_raster_span_layer_color_mask_clear(pcVar52, lVar4, pWork12, iVar33, sixbits);

    uint32_t sel = ((uVar7 >> 14) & 2) | ((uVar7 >> 11) & 1);
    switch (sel) {
    case 0:
        gpu3d_raster_interp_five_ramps_rows_d718((unsigned char *)puVar50, attrWord,
                              (const unsigned char *)lVar45, (unsigned char *)p3,
                              (unsigned char *)lVar4, sixbits,
                              (const unsigned char *)pWork12,
                              (const unsigned char *)pcVar52, iVar33);
        break;
    case 1:
        gpu3d_raster_interp_five_ramps_rows_d7a8((unsigned char *)puVar50, attrWord,
                              (const unsigned char *)lVar45, (unsigned char *)p3,
                              (unsigned char *)lVar4, sixbits,
                              (const unsigned char *)pWork12,
                              (const unsigned char *)pcVar52, iVar33);
        break;
    case 2:
        header->band_dirty = 1;
        gpu3d_raster_interp_five_ramps_rows_d838((unsigned char *)puVar50, attrWord,
                              (const unsigned char *)lVar45, (unsigned char *)p3,
                              (unsigned char *)lVar4, sixbits,
                              (const unsigned char *)pWork12,
                              (const unsigned char *)pcVar52, iVar33);
        break;
    default:
        header->band_dirty = 1;
        gpu3d_raster_interp_five_ramps_rows_d8e0((unsigned char *)puVar50, attrWord,
                              (const unsigned char *)lVar45, (unsigned char *)p3,
                              (unsigned char *)lVar4, sixbits,
                              (const unsigned char *)pWork12,
                              (const unsigned char *)pcVar52, iVar33);
        break;
    }

    if (uVar32 != 0 && p9 != 0) {
        uint8_t        *pixelPlane = lVar48;
        uint8_t        *thirdPlane = addr20000;
        const uint32_t *src        = puVar50;
        const uint8_t  *attrSrc    = p3;
        const uint8_t  *thirdSrc   = lVar4;
        for (int32_t row = 0; row < p9; row++) {
            for (uint32_t col = 0; col < uVar32; col++) {
                wr32(pixelPlane + (size_t)col * 4, src[col]);
                wr32(pixelPlane + GPU3D_BAND_PLANE_SIZE + (size_t)col * 4, rd32(attrSrc + (size_t)col * 4));
                thirdPlane[col] = thirdSrc[col];
            }
            pixelPlane += 0x400;
            thirdPlane += 0x100;
            src += uVar32;
            attrSrc += (size_t)uVar32 * 4;
            thirdSrc += uVar32;
        }
    }
}

extern void gpu3d_raster_interp_five_ramps_rows_d50c(unsigned char *cap0, unsigned char *cap1,
                                    const unsigned char *org0, unsigned char *org1,
                                    unsigned char *prio, uint32_t c5,
                                    const unsigned char *code, const unsigned char *plus,
                                    int32_t n);
extern void gpu3d_raster_interp_five_ramps_rows_d580(unsigned char *cap0, unsigned char *cap1,
                                    const unsigned char *org0, unsigned char *org1,
                                    unsigned char *prio, uint32_t c5,
                                    const unsigned char *code, const unsigned char *plus,
                                    int32_t n);
extern void gpu3d_raster_interp_five_ramps_rows_d5f8(unsigned char *cap0, unsigned char *cap1,
                                    const unsigned char *org0, unsigned char *org1,
                                    unsigned char *prio, uint32_t c5,
                                    const unsigned char *code, const unsigned char *plus,
                                    int32_t n);
extern void gpu3d_raster_interp_five_ramps_rows_d688(unsigned char *cap0, unsigned char *cap1,
                                    const unsigned char *org0, unsigned char *org1,
                                    unsigned char *prio, uint32_t c5,
                                    const unsigned char *code, const unsigned char *plus,
                                    int32_t n);
extern void gpu3d_raster_span_copy_alpha_256_dc00(const unsigned char *ctx, uint32_t *dst_a, uint32_t *dst_b,
                                    unsigned char *dst_c, int32_t spans,
                                    const uint32_t *src_a, const uint32_t *src_b,
                                    const unsigned char *src_c);


void gpu3d_raster_span_band_compose_dispatch(uint8_t *param1, uint8_t *param2, const gpu3d_polygon_t *param3,
                         uint32_t param4, uint32_t param5, uint8_t *param6,
                         int32_t param7, uint32_t param8, const gpu3d_bank_vertex_t *param9)
{

    uint8_t *p1 = param1;
    uint8_t *p2 = param2;
    const gpu3d_polygon_t *p3 = param3;
    uint8_t *p6 = param6;

    gpu3d_band_header_t *hdr = (gpu3d_band_header_t *)(p1 + GPU3D_BAND_HEADER_OFFSET);
    gpu_t *lVar15 = hdr->gpu;
    gpu3d_t *uVar16 = hdr->gpu3d;
    uint32_t u14 = (((uint32_t)param7 * 2u) + 0x1du) & 0xfffffff0u;

    uint8_t *puVar34 = p6 + u14;

    uint8_t *lVar3   = p6 + (uint64_t)u14 * 12u;
    uint8_t *lVar25  = p6 + (uint64_t)u14 * 14u;
    uint8_t *lVar2   = p6 + (uint64_t)u14 * 17u;
    uint32_t uVar31  = u14 * 16u;

    uint32_t uVar19b = u14 * 3u;

    uint32_t uVar20 = param4 << 8;
    uint8_t *lVar26 = p1 + (uint64_t)uVar20 * 4u;

    uint8_t *puVar1 = lVar26 + RECON_3D_PLANE;
    uint8_t *pcVar33 = p6 + uVar31;

    int32_t iVar36 = 0;
    uint32_t local_cnt = 0;

    if ((param8 & (1u << 5)) == 0) {
        uint8_t *dest = p6 + (uint64_t)(u14 * 2u) * 4u;
        gpu3d_raster_row_ramp_generate
            ((int *)(param2), (float *)(param6), (float *)(dest), param5);
        gpu3d_raster_perspective_divide_fix15
            ((int16_t *)(param6), (const float *)(param6), (const float *)(dest), param7);
    } else {
        gpu2d_compose_gen_row_ramp_halfword
            ((uint16_t *)(param6), param2, param5,
             (void *)recon_steps_table(
                 raster_reciprocal_31, 0x80000000u));
    }

    if ((param8 & (1u << 3)) == 0) {
        if ((param8 & (1u << 4)) == 0) {

            gpu2d_compose_gen_row_ramp_q30
                ((uint32_t *)(lVar3), param2, param5,
                 (void *)recon_steps_table(
                     raster_reciprocal_30, 0x40000000u));
            iVar36 = 0;

            if ((param8 & 1u) == 0) goto L_3;
            goto L_2;
        }

        iVar36 = (int32_t)((uint32_t)param9->z << 9);
        goto L_1;
    } else {
        if ((param8 & (1u << 4)) != 0) {
            iVar36 = param9->w;
            goto L_1;
        }
        gpu3d_raster_scale_vertex_row
            ((int32_t *)(lVar3), param2, (const int16_t *)(param6), param5);
        iVar36 = 0;
        if ((param8 & 1u) != 0) goto L_2;
        goto L_3;
    }

L_1: ;
    gpu3d_raster_word_fill
        ((uint32_t *)(lVar3), iVar36, param7);
    if ((param8 & 1u) == 0) goto L_3;

L_2: ;
    gpu3d_raster_rows_copy_table_triple
        ((uint32_t *)(puVar34), (uint32_t *)(lVar25), lVar2, (const uint32_t *)(puVar1), (const uint32_t *)(lVar26),
         p1 + uVar20 + RECON_3D_PLANE2, param2, param5);
    if ((p3->polygon_attr & 0x4000u) != 0) goto L160770;
    goto L_4;

L_3: ;
    gpu3d_raster_rows_copy_table
        (puVar34, puVar1, param2, param5);
    if ((p3->polygon_attr & 0x4000u) == 0) goto L_4;

L160770: ;
    if ((param8 & (1u << 4)) == 0) {
        gpu3d_raster_depth_test_equal_lines
            (pcVar33, (const int32_t *)(lVar3), (const uint32_t *)(puVar34), param7, &local_cnt);
    } else {

        gpu3d_raster_span_key_tolerance_mask
            (pcVar33, iVar36, (const uint32_t *)(puVar34), param7, &local_cnt);
    }
    goto L607b4_join;

L_4: ;
    if ((param8 & (1u << 4)) == 0) {
        gpu3d_raster_span_value_over_mask_array
            (pcVar33, (const uint32_t *)(lVar3), (const uint32_t *)(puVar34), param7, &local_cnt);
    } else {
        gpu3d_raster_span_value_over_mask
            (pcVar33, iVar36, (const uint32_t *)(puVar34), param7, &local_cnt);
    }

L607b4_join: ;

    uint32_t uVar29 = p3->polygon_attr;
    uint32_t uVar5  = uVar29;
    uint32_t uVar8  = (uVar29 >> 24) & 0x3fu;

    uint32_t savedFlagVal = 0;

    if (((~uVar5) & 0x30u) != 0) {
        savedFlagVal = uVar8;
        if (local_cnt == 0) goto L61158;
        goto L60854;
    }
    if (uVar8 == 0) goto L_5;

    {
        uint32_t w13 = param4;
        savedFlagVal = uVar8;
        uint32_t mask = ~(uint32_t)(0xffffffffu << (param5 & 31u));
        mask = (uint32_t)(mask << (w13 & 31u));
        uint32_t bitmap = hdr->band_mark;
        bitmap &= ~mask;
        hdr->band_mark = bitmap;

        uint32_t sum = 0;
        if (param7 != 0) {
            uint32_t cnt = (uint32_t)param7;
            uint8_t *pc = pcVar33;
            uint8_t *pu = puVar34;
            do {
                uint32_t w14 = rd32(pu); pu += 4;
                uint32_t w15 = *pc;
                uint32_t w16 = w14 ^ uVar5;
                int tst_ne = (w16 & 0x3f000000u) != 0;
                int gt = tst_ne ? ((int32_t)w14 > -1) : 1;
                uint32_t w14b = gt ? 0u : w15;
                cnt -= 1;
                *pc = (uint8_t)w14b; pc += 1;
                sum -= (uint32_t)(int32_t)(int8_t)w14b;
            } while (cnt != 0);
        }
        local_cnt = sum;
        if (sum == 0) goto L61158;
        goto L60854;
    }

L_5: ;
    {
        uint32_t uVar14b = hdr->band_mark;
        uint32_t maskVal = ~(uint32_t)(0xffffffffu << (param5 & 31u));
        maskVal = (uint32_t)(maskVal << (param4 & 31u));
        if ((maskVal & ~uVar14b) != 0) {
            uint32_t newVal = uVar14b | maskVal;
            hdr->band_mark = newVal;
            if (param5 != 0) {
                uint32_t shifted = uVar14b >> (param4 & 31u);
                uint32_t cnt = param5;
                uint8_t *p21 = puVar1;
                for (;;) {
                    if ((shifted & 1u) == 0u) {
                        for (int i = 0; i < 256; i++) {
                            uint32_t v = rd32(p21 + (size_t)i * 4);
                            v &= 0x7fffffffu;
                            wr32(p21 + (size_t)i * 4, v);
                        }
                    }
                    shifted >>= 1;
                    cnt -= 1;
                    p21 += 0x400;
                    if (cnt == 0) break;
                }
            }
        }
    }

    if (param7 != 0) {

        uint8_t *pu = puVar34;
        uint8_t *pc = pcVar33;
        int32_t cnt7 = param7;
        for (;;) {
            if (*pc == 0) {
                uint32_t v = rd32(pu);
                v |= 0x80000000u;
                wr32(pu, v);
            }
            pu += 4; pc += 1; cnt7 -= 1;
            if (cnt7 == 0) break;
        }
    }

    if (param5 == 0) goto L61158;

    {
        uint8_t *arrA = p2 + 9u * RECON_3D_ROW_ARRAY;
        uint8_t *arrB = p2 + 8u * RECON_3D_ROW_ARRAY;
        uint8_t *dst_i = puVar1;
        for (uint32_t i = 0; i < param5; i++, dst_i += 0x400) {
            uint16_t wA = rd16(arrA); arrA += 4;
            uint16_t wB = rd16(arrB); arrB += 4;
            if (wA == 0) continue;

            uint32_t destOffWords = 0;
            uint32_t remain = wA;
            if (wA >= 8) {
                uint8_t *lim1 = puVar34 + (uint64_t)wA * 4u;
                uint8_t *base_row = GPU3D_BAND(p1)->plane1 + ((uint64_t)uVar20 + (uint64_t)i * 256u) * 4u;
                uint8_t *lim2 = base_row + (uint64_t)wB * 4u;
                int overlap_unsafe = 0;
                if ((uintptr_t)lim2 >= (uintptr_t)lim1) {
                    overlap_unsafe = 1;
                } else {
                    uint8_t *lim3 = base_row + (uint64_t)((uint32_t)wA + (uint32_t)wB) * 4u;
                    if ((uintptr_t)puVar34 >= (uintptr_t)lim3) overlap_unsafe = 1;
                }
                if (overlap_unsafe) {
                    uint32_t block = remain & ~7u;
                    uint8_t *src = puVar34;
                    uint8_t *dstp = dst_i + (uint64_t)wB * 4u;
                    for (uint32_t k = 0; k < block; k++) {
                        wr32(dstp + (size_t)k * 4, rd32(src + (size_t)k * 4));
                    }
                    puVar34 += (uint64_t)block * 4u;
                    destOffWords = block;
                    remain -= block;
                    if (remain == 0) continue;
                }
            }
            {
                uint8_t *dstp = dst_i + ((uint64_t)destOffWords + (uint64_t)wB) * 4u;
                while (remain != 0) {
                    uint32_t v = rd32(puVar34); puVar34 += 4;
                    wr32(dstp, v); dstp += 4;
                    remain -= 1;
                }
            }
        }
    }
    goto L61158;

L60854: ;
    {
        gpu3d_raster_t *lVar15p34eb40 = &lVar15->raster;
        uint8_t *lVar15p1056c0 = recon_buf3d_front((unsigned char *)lVar15);
        uint32_t field5b = (p3->polygon_attr >> 16) & 0x1fu;
        uint8_t *x27 = p6 + uVar19b;
        uint8_t *x19cur;

        const uint8_t *f16 = NULL;

        if ((param8 & (1u << 2)) != 0) {

            uint16_t h1056v = rd16(p2 + 6u * RECON_3D_ROW_ARRAY);
            gpu3d_raster_byte_fill
                (x27, (uint32_t)(h1056v >> 3), param7);
            uint16_t h1058v = rd16(p2 + 6u * RECON_3D_ROW_ARRAY + 2u);
            gpu3d_raster_byte_fill
                (x27 + u14, (uint32_t)(h1058v >> 3), param7);
            uint16_t h1410v = rd16(p2 + 8u * RECON_3D_ROW_ARRAY + 2u);
            gpu3d_raster_byte_fill
                (x27 + u14 * 2u, (uint32_t)(h1410v >> 3), param7);
            x27 += uVar19b;
            if ((param8 & (1u << 1)) != 0) goto L608c0common;
            x19cur = x27;
            goto L6098c_join;
        }

        gpu3d_raster_span_edge_stream_fill
            (param2, (uint16_t *)(x27), param5, u14);
        spu_mixer_accumulate_triple
            (x27, (int16_t *)(x27), (int32_t *)(param6), param7, u14);
        x27 += uVar19b;
        if ((param8 & (1u << 1)) == 0) {
            x19cur = x27;
            goto L6098c_join;
        }

    L608c0common: ;
        gpu3d_raster_row_fill_constants
            (param2, (uint32_t *)(x27), param5, u14);
        spu_mixer_accumulate_pair
            ((int16_t *)(x27), (int32_t *)(x27), (const int16_t *)(param6), param7, u14);
        {
            const gpu3d_texture_entry_t *pfield = p3->texture;
            gpu3d_raster_texture_wrap_dispatch
                ((uint8_t *)param3, (uint32_t *)(x27), (const int16_t *)(x27), param7, pcVar33);
            uint16_t f72 = pfield->palette_count;
            f16 = pfield->data;
 if (f72 != 0) {                const uint32_t *f24 = (const uint32_t *)(pfield->palette);
                gpu3d_raster_texel_table_lookup_chained
                    ((uint32_t *)(x27), (const uint32_t *)(x27), f16, f24, param7);
                goto L_7;
            }
            goto L_6;
        }

    L_6:

        gpu3d_raster_texel_table_lookup
            ((uint32_t *)(x27), (const uint32_t *)(x27), (const uint32_t *)(f16), param7);
    L_7:

        gpu3d_raster_texel_blend_dispatch
            (lVar15p1056c0, uVar16, param3, x27, x27,
             p6 + uVar19b, u14, field5b, param7);
        {
            uint32_t f4b = lVar15p34eb40->alpha_test_ref;

            gpu3d_raster_span_component_gate_filter
                (pcVar33, x27, f4b, param7, &local_cnt);
            x19cur = x27;
            if (local_cnt == 0) goto L61158;
            if ((param8 & 1u) == 0) goto L_8;
            goto L_9;
        }

    L6098c_join: ;
        {
            uint32_t f4 = lVar15p34eb40->alpha_test_ref;
            if (field5b <= f4) goto L61158;

            gpu2d_palette_indices_to_rgba_line
                (lVar15p1056c0, uVar16, param3, (uint32_t *)(x19cur),
                 p6 + uVar19b, u14, field5b, param7);

            if ((param8 & 1u) != 0) goto L_9;

        }

    L_8: ;
        {
            if ((p3->polygon_attr & 0x8000u) != 0) {
                hdr->band_dirty = 1;
                gpu3d_raster_rgba_force_opaque(x19cur, param7);
            }
            uint8_t flagbyte = (uint8_t)lVar15p34eb40->disp3dcnt;
            if ((flagbyte & (1u << 5)) == 0) goto L61114;

            uint8_t *x9c = lVar3 + 3;
            uint32_t w10 = rd16(p2 + 10u * RECON_3D_ROW_ARRAY);
            if (w10 != 0) {
                for (uint32_t k = 0; k < w10; k++)
                    wr8(x9c + (size_t)k * 4u, 0x40);
                x9c += (size_t)w10 * 4u;
            }

            uint32_t w11 = rd16(p2 + 10u * RECON_3D_ROW_ARRAY + 2u);
            uint32_t w12c = rd16(p2 + 9u * RECON_3D_ROW_ARRAY);
            uint32_t w8lim = param5 - 1u;
            uint32_t off0 = ((uint32_t)w12c - (w11 + w10)) << 2;
            uint8_t *x15 = x9c + (int64_t)(int32_t)off0;

            if (w8lim != 0) {
                uint8_t *x14 = p2 + 10u * RECON_3D_ROW_ARRAY;
                uint32_t w9 = 0;
                uint8_t *x10a = p2 + 9u * RECON_3D_ROW_ARRAY;
                uint8_t *x13 = x14;
                uint8_t *x17 = x15;
                uint32_t w16;
                x13 += 4; w16 = rd16(x13);
                if ((uint32_t)(w11 + w16) != 0) goto R6105c;
            R61024:
                x17 = x15;
            R61028:
                {
                    x10a += 4;
                    uint32_t w15v = rd16(x10a);
                    w11 = rd16(x14 + 6);
                    w9 += 1;
                    uint32_t w14v = w11 + w16;
                    w14v = ((uint32_t)w15v - w14v) << 2;
                    x15 = x17 + (int64_t)(int32_t)w14v;
                    x14 = x13;
                    if (w9 == w8lim) goto R610b8;
                    x13 += 4; w16 = rd16(x13);
                    if ((uint32_t)(w11 + w16) == 0) goto R61024;
                }
            R6105c:
                w11 = w11 + w16;
                for (uint32_t k = 0; k < w11; k++)
                    wr8(x15 + (size_t)k * 4u, 0x40);
                x17 = x15 + (size_t)w11 * 4u;
                goto R61028;
            }
        R610b8:
            if (w11 != 0) {
                for (uint32_t k = 0; k < w11; k++)
                    wr8(x15 + (size_t)k * 4u, 0x40);
            }
        }

    L61114: ;
        {

            if (local_cnt == (uint32_t)param7) {
                gpu3d_raster_span_copy_alpha_256
                    (param2, (uint32_t *)(lVar26), (uint32_t *)(puVar1), param5, savedFlagVal,
                     (const uint32_t *)(x19cur), (const uint32_t *)(lVar3));
            } else {
                gpu3d_raster_rows_copy_masked_alpha
                    (param2, (uint32_t *)(lVar26), (uint32_t *)(puVar1), param5, savedFlagVal,
                     (const uint32_t *)(x19cur), (const uint32_t *)(lVar3), pcVar33);
            }
        }
        goto L61158;

    L_9: ;
        {
            uint8_t *frameP1 = GPU3D_BAND(p1)->plane2 + uVar20;
            uint8_t *x26v = x19cur;
            uint8_t flagB = (uint8_t)lVar15p34eb40->disp3dcnt;
            uint8_t *x21ptr = p6 + uVar19b;
            uint8_t *x19b;

            if ((flagB & (1u << 3)) != 0) {
                gpu3d_raster_pixel_blend_alpha
                    (x19cur, lVar25, param7, x21ptr);
            } else {
                gpu3d_raster_pixel_alpha_max
                    (x19cur, lVar25, param7, x21ptr);
            }
            x19b = lVar2;
            gpu3d_raster_span_layer_color_mask_clear
                (pcVar33, x19b, x21ptr, param7, savedFlagVal);

            uint32_t f4c = p3->polygon_attr;
            uint32_t idx2 = (uint32_t)((f4c >> 14) & 2u);
            idx2 = (idx2 & ~1u) | ((f4c >> 11) & 1u);

            static const uint8_t ramp_kind[4] = { 0u, 40u, 12u, 26u };
            uint8_t entry = ramp_kind[idx2 & 3u];
            uint8_t *lVar3cur = lVar3;

            switch (entry) {
            case 0:
                gpu3d_raster_interp_five_ramps_rows_d50c((unsigned char *)x26v,
                                      (unsigned char *)lVar3cur,
                                      (const unsigned char *)lVar25,
                                      (unsigned char *)puVar34,
                                      (unsigned char *)x19b, savedFlagVal,
                                      (const unsigned char *)x21ptr,
                                      (const unsigned char *)pcVar33, param7);
                break;
            case 12:
                hdr->band_dirty = 1;
                gpu3d_raster_interp_five_ramps_rows_d5f8((unsigned char *)x26v,
                                      (unsigned char *)lVar3cur,
                                      (const unsigned char *)lVar25,
                                      (unsigned char *)puVar34,
                                      (unsigned char *)x19b, savedFlagVal,
                                      (const unsigned char *)x21ptr,
                                      (const unsigned char *)pcVar33, param7);
                break;
            case 26:
                hdr->band_dirty = 1;
                gpu3d_raster_interp_five_ramps_rows_d688((unsigned char *)x26v,
                                      (unsigned char *)lVar3cur,
                                      (const unsigned char *)lVar25,
                                      (unsigned char *)puVar34,
                                      (unsigned char *)x19b, savedFlagVal,
                                      (const unsigned char *)x21ptr,
                                      (const unsigned char *)pcVar33, param7);
                break;
            case 40:
                gpu3d_raster_interp_five_ramps_rows_d580((unsigned char *)x26v,
                                      (unsigned char *)lVar3cur,
                                      (const unsigned char *)lVar25,
                                      (unsigned char *)puVar34,
                                      (unsigned char *)x19b, savedFlagVal,
                                      (const unsigned char *)x21ptr,
                                      (const unsigned char *)pcVar33, param7);
                break;
            default:
                break;
            }

            gpu3d_raster_span_copy_alpha_256_dc00(param2, (uint32_t *)lVar26,
                                    (uint32_t *)puVar1, (unsigned char *)frameP1,
                                    (int32_t)param5, (const uint32_t *)x26v,
                                    (const uint32_t *)lVar3cur,
                                    (const unsigned char *)x19b);
            goto L61158;
        }
    }

L61158:
    return;
}



void gpu3d_raster_span_row_compose_hires(uint8_t *param_1, const gpu3d_polygon_t *param_2, uint8_t *param_3,
                         uint32_t param_4, int32_t param_5, int32_t param_6,
                         int32_t param_7, uint32_t param_8, int32_t param_9,
                         uint32_t param_10, uint32_t param_11, uint32_t param_12)
{

    uint32_t columns = param_8;
    uint32_t rows = (uint32_t)param_9;
    uint32_t count = rows * columns;
    uint32_t rounded = (count + 0x16u) & 0xfffffff0u;
    uint32_t p4_low = param_4;
    uint32_t p5_x512 = ((uint32_t)param_5) * RECON_3D_WIDTH;
    gpu3d_band_header_t *hdr = GPU3D_BAND_HEADER(param_1);
    gpu_t *ctx = hdr->gpu;
    const gpu3d_texture_entry_t *description = param_2->texture;
    uint8_t *plane = param_1 + (uint64_t)p5_x512 * 4 + (uint64_t)p4_low * 4;
    uint8_t *plane_attr = plane + RECON_3D_PLANE;
    uint8_t *work4 = param_3 + (uint64_t)rounded * 4;
    uint8_t *mask = param_3 + (uint64_t)rounded * 13;
    uint8_t *layer = param_3 + (uint64_t)rounded * 14;
    uint8_t *work8 = param_3 + (uint64_t)rounded * 8;
    uint8_t *third_plane = param_1 + (uint64_t)p5_x512 + p4_low + RECON_3D_PLANE2;
    uint32_t local_6c;

    uint32_t control_initial = param_2->polygon_attr;

    if ((param_12 & 1u) == 0) {
        gpu3d_raster_blit_row_stride_512(param_3, plane_attr, columns, rows);
    } else {
        gpu3d_raster_rows_copy_fixed_triple_512((uint32_t *)(param_3), (uint32_t *)(work4), layer, (const uint32_t *)(plane_attr),
                                   (const uint32_t *)(plane), third_plane, columns, rows);
    }

    if ((param_2->polygon_attr & 0x4000u) != 0) {
        gpu3d_raster_span_key_tolerance_mask(mask, param_10, (const uint32_t *)(param_3), count,
                                  &local_6c);
    } else {
        gpu3d_raster_span_value_over_mask(mask, param_10, (const uint32_t *)(param_3), count,
                                  &local_6c);
    }
    if (local_6c == 0)
        return;

    uint32_t step = description->width;
    uint16_t uses_lut = description->palette_count;
    uint8_t *origin = description->data;

    if (uses_lut != 0) {
        uint8_t *lut = description->palette;
        uint8_t *row_origin = origin + step * (uint32_t)param_7 +
                               (int64_t)param_6;
        if (rows != 0 && columns != 0) {
            for (uint32_t row = 0; row != rows; row++) {
                for (uint32_t column = 0; column != columns; column++) {
                    uint32_t value = rd32(lut + (uint64_t)row_origin[column] * 4);
                    wr32(work8 + (uint64_t)(row * columns + column) * 4,
                          value);
                }
                row_origin += step;
            }
        }
    } else {
        uint8_t *row_origin = origin + (uint64_t)step * (uint32_t)param_7 * 4 +
                               (int64_t)param_6 * 4;
        if (rows != 0 && columns != 0) {
            for (uint32_t row = 0; row != rows; row++) {
                for (uint32_t column = 0; column != columns; column++) {
                    wr32(work8 + (uint64_t)(row * columns + column) * 4,
                          rd32(row_origin + (uint64_t)column * 4));
                }
                row_origin += (uint64_t)step * 4;
            }
        }
    }

    gpu3d_raster_span_component_gate_filter(mask, work8,
                                ctx->raster.alpha_test_ref, count, &local_6c);
    if (local_6c == 0)
        return;

    if (param_11 != 0x003f3f3fu && count != 0) {
        uint32_t sum_r = param_11 & 0x3fu;
        uint32_t sum_g = (param_11 >> 8) & 0x3fu;
        uint32_t sum_b = (param_11 >> 16) & 0x3fu;
        for (uint32_t i = 0; i != count; i++) {
            uint8_t *pixel = work8 + (uint64_t)i * 4;
            uint32_t original = rd32(pixel);
            uint32_t r = original & 0xffu;
            uint32_t g = (original >> 8) & 0xffu;
            uint32_t b = (original >> 16) & 0xffu;
            uint32_t updated_r = (r + sum_r + r * sum_r) >> 6;
            uint32_t updated_g = (g + sum_g + g * sum_g) >> 6;
            uint32_t updated_b = (b + sum_b + b * sum_b) >> 6;
            wr32(pixel, (original & 0xff000000u) | updated_r |
                         (updated_g << 8) | (updated_b << 16));
        }
    }

    uint32_t control_attr = param_2->polygon_attr;
    uint32_t six_bits = (control_initial >> 24) & 0x3fu;
    uint32_t attr = (param_10 | (six_bits << 24)) |
                         ((control_attr << 15) & 0x40000000u);

    if ((param_12 & 1u) == 0) {
        if ((control_attr & 0x8000u) != 0) {
            hdr->band_dirty = 1; recon_band_publish_dirty();
            gpu3d_raster_rgba_force_opaque(work8, count);
        }
        if (rows != 0 && columns != 0) {
            for (uint32_t row = 0; row != rows; row++) {
                uint8_t *row_plane = plane + (uint64_t)row * RECON_ROW_STEP;
                uint8_t *row_mask = mask + (uint64_t)row * columns;
                uint8_t *row_work = work8 + (uint64_t)row * columns * 4;
                for (uint32_t column = 0; column != columns; column++) {
                    if (row_mask[column] != 0) {
                        wr32(row_plane + (uint64_t)column * 4,
                              rd32(row_work + (uint64_t)column * 4));
                        wr32(row_plane + (uint64_t)column * 4 + RECON_3D_PLANE,
                              attr);
                    }
                }
            }
        }
        return;
    }

    uint8_t style = description->format;
    uint32_t coef = (control_initial >> 16) & 0x1fu;
    uint8_t *work12 = param_3 + (uint64_t)rounded * 12;
    if (count != 0) {
        for (uint32_t i = 0; i != count; i++) {
            uint8_t *pixel = work8 + (uint64_t)i * 4;
            uint32_t value = rd32(pixel);
            if (style == 6 || style == 1) {
                uint32_t height = (value >> 24) & 0x1fu;
                height = (height + coef + height * coef) >> 5;
                wr32(pixel, (value & 0x00ffffffu) | (height << 24));
            } else {
                wr32(pixel, value & (0x00ffffffu | (coef << 24)));
            }
        }
    }

    if ((ctx->raster.disp3dcnt & 8u) != 0)
        gpu3d_raster_pixel_blend_alpha(work8, work4, count, work12);
    else
        gpu3d_raster_pixel_alpha_max(work8, work4, count, work12);
    gpu3d_raster_span_layer_color_mask_clear(mask, layer, work12, count, six_bits);

    uint32_t control_dispatch = param_2->polygon_attr;
    switch (((control_dispatch >> 14) & 2u) |
            ((control_dispatch >> 11) & 1u)) {
    case 0:
        gpu3d_raster_interp_five_ramps_rows_d718(work8, attr, work4, param_3,
                              layer, six_bits, work12, mask, (int32_t)count);
        break;
    case 1:
        gpu3d_raster_interp_five_ramps_rows_d7a8(work8, attr, work4, param_3,
                              layer, six_bits, work12, mask, (int32_t)count);
        break;
    case 2:
        hdr->band_dirty = 1; recon_band_publish_dirty();
        gpu3d_raster_interp_five_ramps_rows_d838(work8, attr, work4, param_3,
                              layer, six_bits, work12, mask, (int32_t)count);
        break;
    default:
        hdr->band_dirty = 1; recon_band_publish_dirty();
        gpu3d_raster_interp_five_ramps_rows_d8e0(work8, attr, work4, param_3,
                              layer, six_bits, work12, mask, (int32_t)count);
        break;
    }

    if (rows != 0 && columns != 0) {
        for (uint32_t row = 0; row != rows; row++) {
            uint8_t *row_plane = plane + (uint64_t)row * RECON_ROW_STEP;
            uint8_t *row_third = third_plane + (uint64_t)row * (RECON_ROW_STEP / 4u);
            uint8_t *row_work = work8 + (uint64_t)row * columns * 4;
            uint8_t *row_attrs = param_3 + (uint64_t)row * columns * 4;
            uint8_t *row_layer = layer + (uint64_t)row * columns;
            for (uint32_t column = 0; column != columns; column++) {
                wr32(row_plane + (uint64_t)column * 4,
                      rd32(row_work + (uint64_t)column * 4));
                wr32(row_plane + (uint64_t)column * 4 + RECON_3D_PLANE,
                      rd32(row_attrs + (uint64_t)column * 4));
                row_third[column] = row_layer[column];
            }
        }
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
void gpu3d_raster_span_copy_triple_512(const unsigned char *param_1, uint32_t *param_2, uint32_t *param_3,
                        uint8_t *param_4, int32_t param_5,
                        const uint32_t *param_6, const uint32_t *param_7,
                        const uint8_t *param_8);






static uint8_t *put_40(uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        wr8(p, 0x40);
        p += 4;
    }
    return p;
}

static void clear_bit31(uint8_t *p)
{
    uint32_t n = (uint32_t)RECON_ROW_STEP, i = 0;
#ifdef __ARM_NEON
    const uint32x4_t m = vdupq_n_u32(0x7fffffffu);
    for (; i + 16u <= n; i += 16u)
        vst1q_u32((uint32_t *)(p + i), vandq_u32(vld1q_u32((const uint32_t *)(p + i)), m));
#endif
    for (; i != n; i += 4)
        wr32(p + i, rd32(p + i) & 0x7fffffffu);
}

void gpu3d_raster_span_band_compose_dispatch_hires(uint8_t *state, uint8_t *entry, const gpu3d_polygon_t *order,
                         uint32_t column, uint32_t width, uint8_t *zone,
                         uint32_t rows, uint32_t flags, const gpu3d_bank_vertex_t *extra)
{

    uint32_t aligned = (rows * 2u + 0x1du) & 0xfffffff0u;
    if (recon_scale_3d > 2u && rows < aligned)
        memset(zone + (size_t)aligned * 16u + rows, 0, aligned - rows);
    uint32_t control;
    uint32_t control_saved;
    uint32_t mode;
    uint32_t state_line = 0;
    uint8_t *line;
    uint8_t *marks;
    uint8_t *pixels;
    uint8_t *page;

    gpu3d_band_header_t *hdr = GPU3D_BAND_HEADER(state);
    gpu_t *root = hdr->gpu;
    uint8_t *root_secondary = (uint8_t *)hdr->gpu3d;

    if ((flags & 0x20u) == 0) {
        uint8_t *end = zone + (uint64_t)(aligned * 2u) * 4u;
        gpu3d_raster_row_ramp_generate
            ((int *)(entry), (float *)(zone), (float *)(end), width);
        gpu3d_raster_perspective_divide_fix15
            ((int16_t *)(zone), (const float *)(zone), (const float *)(end), rows);
    } else {
        gpu2d_compose_gen_row_ramp_halfword
            ((uint16_t *)(zone), entry, width,
             (void *)recon_steps_table(raster_reciprocal_31,
                                       0x80000000u));
    }

    control = order->polygon_attr;
    control_saved = control;
    line = zone + aligned;
    marks = zone + (uint64_t)aligned * 16u;

    uint8_t *tmp = zone + (uint64_t)aligned * 12u;

    page = state + (uint64_t)column * (uint64_t)RECON_ROW_STEP;
    pixels = page + RECON_3D_PLANE;

        if ((flags & 8u) == 0) {
        if ((flags & 0x10u) == 0) {

            gpu2d_compose_gen_row_ramp_q30
                ((uint32_t *)(tmp), entry, width,
                 (void *)recon_steps_table(raster_reciprocal_30,
                                           0x40000000u));
            state_line = 0;
        } else {
            state_line = (uint32_t)extra->z << 9;
            gpu3d_raster_word_fill
                ((uint32_t *)(tmp), state_line, rows);
        }

        if ((flags & 1u) != 0) {
            gpu3d_raster_rows_copy_three_planes_wide
                ((uint32_t *)(zone + aligned), (uint32_t *)(zone + (uint64_t)aligned * 14u),
                 zone + (uint64_t)aligned * 17u, (const uint32_t *)(pixels), (const uint32_t *)(page),

                 state + ((uint64_t)column * RECON_3D_WIDTH) + RECON_3D_PLANE2, entry, width);

            if ((order->polygon_attr & 0x4000u) != 0) {
                if ((flags & 0x10u) == 0) {
                    gpu3d_raster_depth_test_equal_lines
                        (marks, (const int32_t *)(tmp), (const uint32_t *)(zone + aligned),
                         rows, &state_line);
                } else {
                    gpu3d_raster_span_key_tolerance_mask
                        (marks, state_line, (const uint32_t *)(zone + aligned), rows,
                         &state_line);
                }
            } else if ((flags & 0x10u) == 0) {
                gpu3d_raster_span_value_over_mask_array
                    (marks, (const uint32_t *)(tmp), (const uint32_t *)(zone + aligned),
                     rows, &state_line);
            } else {
                gpu3d_raster_span_value_over_mask
                    (marks, state_line, (const uint32_t *)(zone + aligned), rows,
                     &state_line);
            }
        } else {
            gpu3d_raster_rows_copy_one_plane_wide
                (zone + aligned, pixels, entry, width);
            if ((order->polygon_attr & 0x4000u) != 0) {
                if ((flags & 0x10u) == 0) {
                    gpu3d_raster_depth_test_equal_lines
                        (marks, (const int32_t *)(tmp), (const uint32_t *)(zone + aligned),
                         rows, &state_line);
                } else {
                    gpu3d_raster_span_key_tolerance_mask
                        (marks, state_line, (const uint32_t *)(zone + aligned), rows,
                         &state_line);
                }
            } else if ((flags & 0x10u) == 0) {
                gpu3d_raster_span_value_over_mask_array
                    (marks, (const uint32_t *)(tmp), (const uint32_t *)(zone + aligned),
                     rows, &state_line);
            } else {
                gpu3d_raster_span_value_over_mask
                    (marks, state_line, (const uint32_t *)(zone + aligned), rows,
                     &state_line);
            }
        }
    } else {
        if ((flags & 0x10u) != 0) {
            state_line = (uint32_t)extra->w;
            gpu3d_raster_word_fill
                ((uint32_t *)(tmp), state_line, rows);
        } else {
            gpu3d_raster_scale_vertex_row
                ((int32_t *)(tmp), entry, (const int16_t *)(zone), width);
            state_line = 0;
        }

        if ((flags & 1u) != 0) {
            gpu3d_raster_rows_copy_three_planes_wide
                ((uint32_t *)(zone + aligned), (uint32_t *)(zone + (uint64_t)aligned * 14u),
                 zone + (uint64_t)aligned * 17u, (const uint32_t *)(pixels), (const uint32_t *)(page),

                 state + ((uint64_t)column * RECON_3D_WIDTH) + RECON_3D_PLANE2, entry, width);
            if ((order->polygon_attr & 0x4000u) != 0) {
                if ((flags & 0x10u) == 0) {
                    gpu3d_raster_depth_test_equal_lines
                        (marks, (const int32_t *)(tmp), (const uint32_t *)(zone + aligned),
                         rows, &state_line);
                } else {
                    gpu3d_raster_span_key_tolerance_mask
                        (marks, state_line, (const uint32_t *)(zone + aligned), rows,
                         &state_line);
                }
            } else if ((flags & 0x10u) == 0) {
                gpu3d_raster_span_value_over_mask_array
                    (marks, (const uint32_t *)(tmp), (const uint32_t *)(zone + aligned),
                     rows, &state_line);
            } else {
                gpu3d_raster_span_value_over_mask
                    (marks, state_line, (const uint32_t *)(zone + aligned), rows,
                     &state_line);
            }
        } else {
            gpu3d_raster_rows_copy_one_plane_wide
                (zone + aligned, pixels, entry, width);
            if ((order->polygon_attr & 0x4000u) != 0) {
                if ((flags & 0x10u) == 0) {
                    gpu3d_raster_depth_test_equal_lines
                        (marks, (const int32_t *)(tmp), (const uint32_t *)(zone + aligned),
                         rows, &state_line);
                } else {
                    gpu3d_raster_span_key_tolerance_mask
                        (marks, state_line, (const uint32_t *)(zone + aligned), rows,
                         &state_line);
                }
            } else if ((flags & 0x10u) == 0) {
                gpu3d_raster_span_value_over_mask_array
                    (marks, (const uint32_t *)(tmp), (const uint32_t *)(zone + aligned),
                     rows, &state_line);
            } else {
                gpu3d_raster_span_value_over_mask
                    (marks, state_line, (const uint32_t *)(zone + aligned), rows,
                     &state_line);
            }
        }
    }

    control = order->polygon_attr;

    mode = (control_saved >> 24) & 0x3fu;
    if ((~control & 0x30u) == 0) {
        if (mode == 0) {

            uint8_t *map_width = (recon_scale_3d > 2u)
                ? (state + RECON_3D_PLANE2 + RECON_PLANE2_USED) : NULL;
            const uint32_t nrows = (uint32_t)RECON_BAND_ROWS;
            uint32_t old = hdr->band_mark;
            uint32_t mask = ~(0xffffffffu << (width & 31u));
            mask <<= column & 31u;
            int has_updated;
            if (map_width) {
                has_updated = 0;
                for (uint32_t i = 0; i < width; i++) {
                    uint32_t f = column + i;
                    if (f < nrows && !(map_width[f >> 3] & (1u << (f & 7u))))
                        { has_updated = 1; break; } }
            } else {
                has_updated = ((mask & ~old) != 0);
            }
            if (has_updated) {
                if (!map_width)
                    hdr->band_mark = old | mask;
                if (width != 0) {
                    uint32_t map = old >> (column & 31u);

                    uint8_t *table = page + RECON_3D_PLANE + 0x10;
                    uint8_t *screen = pixels;
                    for (uint32_t i = 0; i < width; ++i) {
                        int seen;
                        if (map_width) {
                            uint32_t f = column + i;
                            seen = (f < nrows) &&
                                    (map_width[f >> 3] & (1u << (f & 7u)));
                            if (f < nrows)
                                map_width[f >> 3] |= (uint8_t)(1u << (f & 7u));
                        } else {
                            seen = (map & 1u) != 0;
                        }
                        if (!seen) {
                            clear_bit31(screen);
                            clear_bit31(table);
                        }
                        map >>= 1;
                        screen += RECON_ROW_STEP;
                        table += RECON_ROW_STEP;
                    }
                }
            }

            if (rows != 0) {
                uint8_t *src = line;
                uint8_t *tag = marks;
                for (uint32_t i = 0; i < rows; ++i) {
                    if (rd8(tag) == 0)
                        wr32(src, rd32(src) | 0x80000000u);
                    src += 4;
                    ++tag;
                }

            }

            if (width == 0)
                return;
            {
                uint8_t *lens = entry + 9u * RECON_3D_ROW_ARRAY;
                uint8_t *indices = entry + 8u * RECON_3D_ROW_ARRAY;
                uint8_t *origin = line;
                uint8_t *dest = pixels;
                for (uint32_t i = 0; i < width; ++i) {
                    uint16_t n = rd16(lens);
                    uint16_t idx = rd16(indices);
                    lens += 4;
                    indices += 4;
                    for (uint32_t j = 0; j < n; ++j) {
                        wr32(dest + ((uint64_t)idx + j) * 4u,
                              rd32(origin));
                        origin += 4;
                    }
                    dest += RECON_ROW_STEP;
                }
            }
            return;
        }

        {
            uint32_t mask = ~(0xffffffffu << (width & 31u));
            mask <<= column & 31u;
            if (recon_scale_3d > 2u) {
                uint8_t *mp = state + RECON_3D_PLANE2 + RECON_PLANE2_USED;
                const uint32_t nf = (uint32_t)RECON_BAND_ROWS;
                for (uint32_t i = 0; i < width; i++) {
                    uint32_t f = column + i;
                    if (f < nf) mp[f >> 3] &= (uint8_t)~(1u << (f & 7u)); }
            } else {
                hdr->band_mark = hdr->band_mark & ~mask;
            }
            state_line = 0;
            for (uint32_t i = 0; i < rows; ++i) {
                uint32_t word = rd32(zone + (uint64_t)aligned + i * 4u);
                uint8_t mark = rd8(marks + i);
                uint32_t test = word ^ control;

                uint8_t value = ((test & 0x3f000000u) == 0 ||
                                  (int32_t)word > -1) ? 0 : mark;
                wr8(marks + i, value);
                state_line -= (int8_t)value;
            }
            if (state_line == 0)
                return;
        }
    } else if (state_line == 0) {
        return;
    }

    {
        uint8_t *work = zone + (uint64_t)aligned * 3u;
        uint8_t *list = work + (uint64_t)aligned * 3u;
        gpu3d_raster_t *base_a = &root->raster;
        uint8_t *base_b = (uint8_t *)recon_buf3d_front((unsigned char *)root);
        uint32_t idx = (control_saved >> 16) & 0x1fu;

        if ((flags & 4u) == 0) {
            gpu3d_raster_span_edge_stream_fill
                (entry, (uint16_t *)(work), width, aligned);
            spu_mixer_accumulate_triple
                (work, (int16_t *)(work), (int32_t *)(zone), rows, aligned);
        } else {
            gpu3d_raster_byte_fill
                (work, rd16(entry + 6u * RECON_3D_ROW_ARRAY) >> 3, rows);

            gpu3d_raster_byte_fill
                (work + aligned, rd16(entry + 6u * RECON_3D_ROW_ARRAY + 2u) >> 3, rows);
            gpu3d_raster_byte_fill
                (work + (uint64_t)aligned * 2u, rd16(entry + 8u * RECON_3D_ROW_ARRAY + 2u) >> 3, rows);
        }

        if ((flags & 2u) != 0) {
            gpu3d_raster_row_fill_constants
                (entry, (uint32_t *)(list), width, aligned);
            spu_mixer_accumulate_pair
                ((int16_t *)(list), (int32_t *)(list), (const int16_t *)(zone), rows, aligned);
            gpu3d_raster_texture_wrap_dispatch
                ((uint8_t *)order, (uint32_t *)(list), (const int16_t *)(list), rows, marks);
            {
                const gpu3d_texture_entry_t *internal = order->texture;
                if (internal->palette_count != 0) {
                    gpu3d_raster_texel_table_lookup_chained
                        ((uint32_t *)(list), (const uint32_t *)(list), internal->data,
                         (const uint32_t *)(internal->palette), rows);
                } else {
                    gpu3d_raster_texel_table_lookup
                        ((uint32_t *)(list), (const uint32_t *)(list), (const uint32_t *)(internal->data), rows);
                }
            }

            gpu3d_raster_texel_blend_dispatch
                (base_b, (const gpu3d_t *)(root_secondary), (const gpu3d_polygon_t *)((uint8_t *)order), list, list, work,
                 aligned, idx, rows);

            gpu3d_raster_span_component_gate_filter
                (marks, list, base_a->alpha_test_ref, rows, &state_line);
            if (state_line == 0)
                return;
        } else {

            if (idx <= base_a->alpha_test_ref)
                return;

            gpu2d_palette_indices_to_rgba_line
                (base_b, (const gpu3d_t *)(root_secondary), (const gpu3d_polygon_t *)((uint8_t *)order), (uint32_t *)(list), work, aligned,
                 idx, rows);
        }

        if ((flags & 1u) != 0) {

            uint8_t *dest = state + ((uint64_t)column * RECON_3D_WIDTH) + RECON_3D_PLANE2;
            if ((base_a->disp3dcnt & 8u) == 0) {
                gpu3d_raster_pixel_alpha_max
                    (list, zone + (uint64_t)aligned * 14u, rows, work);
            } else {
                gpu3d_raster_pixel_blend_alpha
                    (list, zone + (uint64_t)aligned * 14u, rows, work);
            }
            gpu3d_raster_span_layer_color_mask_clear
                (marks, zone + (uint64_t)aligned * 17u, work, rows, mode);

            uint32_t selector = order->polygon_attr;
            switch ((((selector >> 14) & 2u) | ((selector >> 11) & 1u))) {
            case 0:
                gpu3d_raster_interp_five_ramps_rows_d50c(list, tmp,
                                      zone + (uint64_t)aligned * 14u, line,
                                      zone + (uint64_t)aligned * 17u, mode,
                                      work, marks, (int32_t)rows);
                break;

            case 1:
                gpu3d_raster_interp_five_ramps_rows_d580(list, tmp,
                                      zone + (uint64_t)aligned * 14u, line,
                                      zone + (uint64_t)aligned * 17u, mode,
                                      work, marks, (int32_t)rows);
                break;
            case 2:
                hdr->band_dirty = 1;
                gpu3d_raster_interp_five_ramps_rows_d5f8(list, tmp,
                                      zone + (uint64_t)aligned * 14u, line,
                                      zone + (uint64_t)aligned * 17u, mode,
                                      work, marks, (int32_t)rows);
                break;
            default:
                hdr->band_dirty = 1;
                gpu3d_raster_interp_five_ramps_rows_d688(list, tmp,
                                      zone + (uint64_t)aligned * 14u, line,
                                      zone + (uint64_t)aligned * 17u, mode,
                                      work, marks, (int32_t)rows);
                break;
            }
            gpu3d_raster_span_copy_triple_512(entry,
                               (uint32_t *)(void *)page,
                               (uint32_t *)(void *)pixels,
                               dest, (int32_t)width,
                               (const uint32_t *)(const void *)list,
                               (const uint32_t *)(const void *)tmp,
                               (const uint8_t *)(zone + (uint64_t)aligned * 17u));
            return;
        }

        if ((order->polygon_attr & 0x8000u) != 0) {
            hdr->band_dirty = 1;
            gpu3d_raster_rgba_force_opaque(list, rows);
        }
        if ((base_a->disp3dcnt & 0x20u) == 0) {
            if (state_line == rows) {
                gpu3d_raster_span_copy_alpha_512
                    (entry, (uint32_t *)(page), (uint32_t *)(pixels), width, mode, (const uint32_t *)(list), (const uint32_t *)(tmp));
            } else {
                gpu3d_raster_rows_copy_gap_masked_alpha
                    (entry, page, pixels, width, mode, list, tmp, marks);
            }
            return;
        }

        {

            uint32_t n = rd16(entry + 10u * RECON_3D_ROW_ARRAY);

            uint8_t *output = put_40(tmp + 3, n);
            uint16_t start = rd16(entry + 10u * RECON_3D_ROW_ARRAY + 2u);
            uint16_t base = rd16(entry + 9u * RECON_3D_ROW_ARRAY);
            uint32_t left = width - 1u;
            const uint8_t *a_base = entry + 10u * RECON_3D_ROW_ARRAY;
            const uint8_t *b_cursor = entry + 9u * RECON_3D_ROW_ARRAY;
            uint32_t first = start;

            uint8_t *dest = output +
                (int32_t)(((uint32_t)base - ((uint32_t)start + n)) << 2);

            if (left != 0) {
                const uint8_t *cursor = a_base + 4;
                uint32_t a = rd16(cursor);
                uint32_t done = 0;
                for (;;) {

                    uint8_t *end = (first + a != 0)
                        ? put_40(dest, first + a) : dest;
                    uint32_t b = rd16(b_cursor + 4);
                    b_cursor += 4;

                    first = rd16(a_base + 6);
                    ++done;

                    dest = end +
                        (int32_t)(((uint32_t)b - (first + a)) << 2);
                    a_base = cursor;
                    if (done == left)
                        break;
                    cursor += 4;
                    a = rd16(cursor);
                }
            }

            if (first != 0)
                put_40(dest, first);
        }

        if (state_line == rows) {
            gpu3d_raster_span_copy_alpha_512
                (entry, (uint32_t *)(page), (uint32_t *)(pixels), width, mode, (const uint32_t *)(list), (const uint32_t *)(tmp));
        } else {
            gpu3d_raster_rows_copy_gap_masked_alpha
                (entry, page, pixels, width, mode, list, tmp, marks);
        }
    }
}

void gpu3d_raster_depth_test_equal_lines(uint8_t *param_1, const int32_t *param_2,
                         const uint32_t *param_3, int32_t param_4,
                         uint32_t *param_5)
{

    uint32_t lane_acc[8];
    memset(lane_acc, 0, sizeof(lane_acc));

    const int32_t *p2 = param_2;
    const uint32_t *p3 = param_3;
    uint8_t *p1 = param_1;

    int32_t L = (param_4 > 8) ? (param_4 - 1) / 8 : 0;

    for (int32_t blk = 0; blk < L; blk++) {
        uint8_t mask[8];
        for (int k = 0; k < 8; k++) {
            int32_t a; uint32_t b;
            memcpy(&a, &p2[k], sizeof(a));
            memcpy(&b, &p3[k], sizeof(b));
            b &= 0x00FFFFFFu;
            uint32_t diff = (uint32_t)a - b;
            uint32_t adiff = (diff & 0x80000000u) ? (0u - diff) : diff;
            mask[k] = (adiff < 0x100u) ? 0xFFu : 0x00u;
        }
        p2 += 8;
        p3 += 8;
        memcpy(p1, mask, 8);
        p1 += 8;
        for (int k = 0; k < 8; k++)
            lane_acc[k] += (uint32_t)(mask[k] >> 7);
    }

    uint8_t tail_mask[8];
    for (int k = 0; k < 8; k++) {
        int32_t a; uint32_t b;
        memcpy(&a, &p2[k], sizeof(a));
        memcpy(&b, &p3[k], sizeof(b));
        b &= 0x00FFFFFFu;
        uint32_t diff = (uint32_t)a - b;
        uint32_t adiff = (diff & 0x80000000u) ? (0u - diff) : diff;
        tail_mask[k] = (adiff < 0x100u) ? 0xFFu : 0x00u;
    }
    memcpy(p1, tail_mask, 8);

    int32_t r = param_4 - 8 * L;

    int32_t deficit = 8 - r;

    for (int k = 0; k < 8; k++) {
        if (k >= deficit)
            lane_acc[k] += (uint32_t)(tail_mask[k - deficit] >> 7);
    }

    uint32_t total = 0;
    for (int k = 0; k < 8; k++)
        total += lane_acc[k];

    memcpy(param_5, &total, sizeof(total));
}

void gpu3d_raster_span_key_tolerance_mask(uint8_t *param_1, uint32_t param_2,
                         const uint32_t *param_3, int32_t param_4,
                         uint32_t *param_5)
{

    uint32_t lane_acc[8];
    memset(lane_acc, 0, sizeof(lane_acc));

    const uint32_t *p3 = param_3;
    uint8_t *p1 = param_1;

    int32_t L = (param_4 > 8) ? (param_4 - 1) / 8 : 0;

    for (int32_t blk = 0; blk < L; blk++) {
        uint8_t mask[8];
        for (int k = 0; k < 8; k++) {
            uint32_t b;
            memcpy(&b, &p3[k], sizeof(b));
            b &= 0x00FFFFFFu;
            uint32_t diff = param_2 - b;
            uint32_t adiff = (diff & 0x80000000u) ? (0u - diff) : diff;
            mask[k] = (adiff < 0x100u) ? 0xFFu : 0x00u;
        }
        p3 += 8;
        memcpy(p1, mask, 8);
        p1 += 8;
        for (int k = 0; k < 8; k++)
            lane_acc[k] += (uint32_t)(mask[k] >> 7);
    }

    uint8_t tail_mask[8];
    for (int k = 0; k < 8; k++) {
        uint32_t b;
        memcpy(&b, &p3[k], sizeof(b));
        b &= 0x00FFFFFFu;
        uint32_t diff = param_2 - b;
        uint32_t adiff = (diff & 0x80000000u) ? (0u - diff) : diff;
        tail_mask[k] = (adiff < 0x100u) ? 0xFFu : 0x00u;
    }
    memcpy(p1, tail_mask, 8);

    int32_t r = param_4 - 8 * L;

    int32_t deficit = 8 - r;

    for (int k = 0; k < 8; k++) {
        if (k >= deficit)
            lane_acc[k] += (uint32_t)(tail_mask[k - deficit] >> 7);
    }

    uint32_t total = 0;
    for (int k = 0; k < 8; k++)
        total += lane_acc[k];

    memcpy(param_5, &total, sizeof(total));
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void *gpu3d_raster_span_value_over_mask_array(uint8_t *mask, const uint32_t *thresholds,
                          const uint32_t *values, int32_t n, uint32_t *count)
{

#ifdef __ARM_NEON

    {

        const uint32x4_t m24 = vdupq_n_u32(0x00ffffffu);
        uint16x4_t acc = vdup_n_u16(0);
        uint8_t *out = mask;
        const uint32_t *pv = values, *pu = thresholds;
        int32_t w3 = n;
        for (;;) {
            uint32x4_t a0 = vandq_u32(vld1q_u32(pv),     m24);
            uint32x4_t a1 = vandq_u32(vld1q_u32(pv + 4), m24);
            uint32x4_t u0 = vld1q_u32(pu), u1 = vld1q_u32(pu + 4);
            pv += 8; pu += 8;
            uint8x8_t m = vmovn_u16(vcombine_u16(vmovn_u32(vcgtq_u32(a0, u0)),
                                                 vmovn_u32(vcgtq_u32(a1, u1))));
            vst1_u8(out, m); out += 8;
            w3 -= 8;
            if (w3 <= 0) {
                int64_t bits = (int64_t)(-w3) * 8;
                uint64x1_t d = vshl_u64(vreinterpret_u64_u8(m), vdup_n_s64(bits));
                acc = vpadal_u8(acc, vshr_n_u8(vreinterpret_u8_u64(d), 7));
                break;
            }
            acc = vpadal_u8(acc, vshr_n_u8(m, 7));
        }
        *count = vaddlv_u16(acc);
        return out;
    }
#else

    uint8_t mask_cur[8];
    uint8_t mask_prev[8];
    uint32_t acc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t a[8], b[8];
    int i;

    uint8_t *out = mask;
    const uint32_t *p_val = values;
    const uint32_t *p_thresh = thresholds;

    memcpy(a, p_val, sizeof a);
    p_val += 8;
    memcpy(b, p_thresh, sizeof b);
    p_thresh += 8;
    for (i = 0; i < 8; i++) {
        uint32_t v = a[i] & 0x00ffffffu;
        mask_cur[i] = (v > b[i]) ? 0xff : 0x00;
    }

    int32_t w3 = n - 8;

    if (w3 > 0) {
        do {
            memcpy(mask_prev, mask_cur, 8);
            for (i = 0; i < 8; i++)
                acc[i] += (uint32_t)(mask_prev[i] >> 7);

            memcpy(a, p_val, sizeof a);
            p_val += 8;
            memcpy(b, p_thresh, sizeof b);
            p_thresh += 8;
            for (i = 0; i < 8; i++) {
                uint32_t v = a[i] & 0x00ffffffu;
                mask_cur[i] = (v > b[i]) ? 0xff : 0x00;
            }

            memcpy(out, mask_prev, 8);
            out += 8;
            w3 -= 8;
        } while (w3 > 0);
    }

    int32_t overshoot = -w3;
    uint8_t shifted[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    if (overshoot < 8) {
        for (i = 0; i < 8; i++) {
            int src = i - overshoot;
            if (src >= 0)
                shifted[i] = mask_cur[src];
        }
    }

    memcpy(out, mask_cur, 8);
    out += 8;

    for (i = 0; i < 8; i++)
        acc[i] += (uint32_t)(shifted[i] >> 7);

    uint32_t sum = 0;
    for (i = 0; i < 8; i++)
        sum += acc[i];

    memcpy(count, &sum, sizeof sum);

    return out;
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"

unsigned gpu3d_raster_span_value_over_mask(uint8_t *mask, uint32_t threshold,
                            const uint32_t *values, int n, uint32_t *count) {

#ifdef __ARM_NEON

    {
        int bl = (n + 7) / 8;
        if (bl < 1) bl = 1;
        const uint32x4_t m24 = vdupq_n_u32(0x00ffffffu);
        uint32x4_t um = vdupq_n_u32(threshold);

        uint16x4_t acc = vdup_n_u16(0);
        for (int b = 0; b < bl; b++) {
            uint32x4_t a = vld1q_u32(values + b * 8);
            uint32x4_t c = vld1q_u32(values + b * 8 + 4);
            a = vandq_u32(a, m24); c = vandq_u32(c, m24);
            uint16x8_t h = vcombine_u16(vmovn_u32(vcgtq_u32(a, um)),
                                        vmovn_u32(vcgtq_u32(c, um)));
            uint8x8_t m = vmovn_u16(h);
            vst1_u8(mask + b * 8, m);
            if (b + 1 < bl) {
                acc = vpadal_u8(acc, vshr_n_u8(m, 7));
            } else {
                int64_t bits = (int64_t)(bl * 8 - n) * 8;
                uint64x1_t d = vshl_u64(vreinterpret_u64_u8(m), vdup_n_s64(bits));
                acc = vpadal_u8(acc, vshr_n_u8(vreinterpret_u8_u64(d), 7));
            }
        }
        {
            unsigned total = vaddlv_u16(acc);
            *count = total;
            return total;
        }
    }
#else

    int blocks = (n + 7) / 8;
    if (blocks < 1) blocks = 1;

    unsigned total = 0;
    int valid = n;

    for (int b = 0; b < blocks; b++) {
        for (int k = 0; k < 8; k++) {
            uint32_t v = values[b * 8 + k] & 0x00ffffffu;
            uint8_t m = (v > threshold) ? 0xff : 0x00;
            mask[b * 8 + k] = m;
            if (m && (b * 8 + k) < valid) total++;
        }
    }

    *count = total;
    return total;
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void gpu3d_raster_span_component_gate_filter(uint8_t *param_1, const uint8_t *param_2,
                         uint8_t param_3, int32_t param_4, uint32_t *param_5)
{

#ifdef __ARM_NEON

    {
        uint8x16_t thresh = vdupq_n_u8(param_3);
        uint8x16_t acc = vdupq_n_u8(0);
        uint8_t *pl = param_1, *pe = param_1;
        const uint8_t *pf = param_2;
        int32_t w3 = param_4;
        for (;;) {
            w3 -= 16;
            uint8x16_t v6 = vld1q_u8(pl); pl += 16;
            uint8x16x4_t v = vld4q_u8(pf); pf += 64;
            uint8x16_t v5 = vandq_u8(v6, vcgtq_u8(v.val[3], thresh));
            vst1q_u8(pe, v5); pe += 16;
            if (w3 <= 0) {
                int32_t w6 = -w3 * 8;
                int32_t w5 = w6 - 64; if (w5 < 0) w5 = 0;
                int64x2_t sh = vsetq_lane_s64((int64_t)w6,
                                   vdupq_n_s64((int64_t)w5), 1);
                v5 = vreinterpretq_u8_u64(
                        vshlq_u64(vreinterpretq_u64_u8(v5), sh));
                acc = vsubq_u8(acc, v5);
                break;
            }
            acc = vsubq_u8(acc, v5);
        }
        *param_5 = vaddlvq_u8(acc);
        return;
    }
#else

    uint8_t acc[16];
    memset(acc, 0, sizeof(acc));

    uint8_t *p1 = param_1;
    const uint8_t *p2 = param_2;

    int32_t nblocks = (param_4 <= 0) ? 1 : (param_4 + 15) / 16;

    for (int32_t blk = 0; blk < nblocks; blk++) {
        uint8_t v6[16];
        memcpy(v6, p1, 16);
        p1 += 16;

        uint8_t comp3[16];
        for (int k = 0; k < 16; k++)
            comp3[k] = p2[k * 4 + 3];
        p2 += 64;

        uint8_t result[16];
        for (int k = 0; k < 16; k++) {
            uint8_t mask = (comp3[k] > param_3) ? 0xFFu : 0x00u;
            result[k] = (uint8_t)(v6[k] & mask);
        }

        memcpy(p1 - 16, result, 16);

        uint8_t *tail_src = result;
        uint8_t contrib[16];
        if (blk == nblocks - 1) {
            int32_t deficit = 16 * nblocks - param_4;
            if (deficit < 0)  deficit = 0;
            if (deficit > 16) deficit = 16;
            int32_t w6 = deficit * 8;
            int32_t w5 = w6 - 64;
            if (w5 < 0) w5 = 0;

            for (int half = 0; half < 2; half++) {
                int32_t bits = half ? w6 : w5;
                int32_t byteshift = (bits <= 0) ? 0 : (bits >= 64 ? 8 : bits / 8);
                for (int i = 0; i < 8; i++) {
                    int lane = half * 8 + i;
                    contrib[lane] = (i >= byteshift) ? tail_src[half * 8 + (i - byteshift)] : 0;
                }
            }
        } else {
            memcpy(contrib, result, 16);
        }

        for (int k = 0; k < 16; k++)
            acc[k] = (uint8_t)(acc[k] - contrib[k]);
    }

    uint32_t total = 0;
    for (int k = 0; k < 16; k++)
        total += acc[k];

    memcpy(param_5, &total, sizeof(total));
#endif
}

void gpu3d_raster_span_layer_color_mask_clear(unsigned char *mask, const unsigned char *layer,
                        const unsigned char *other, int32_t n, uint32_t color) {

    unsigned char c = (unsigned char)color;
    do {
        for (int k = 0; k < 32; k++) {
            unsigned char hit = (layer[k] == c) ? 0xff : 0;
            unsigned char cover    = (other[k] == 0x1f) ? 0xff : 0;
            hit &= (unsigned char)~cover;
            mask[k] &= (unsigned char)~hit;
        }
        layer += 32;
        other += 32;
        mask += 32;
        n -= 0x20;
    } while (n > 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_span_edge_stream_fill(unsigned char *param_1, uint16_t *param_2, int32_t param_3, unsigned long param_4)
{

    unsigned char *base = param_1;
    unsigned char *pA = base + 6u * RECON_3D_ROW_ARRAY;
    unsigned char *pB = base + 8u * RECON_3D_ROW_ARRAY;
    unsigned char *pC = base + 7u * RECON_3D_ROW_ARRAY;
    unsigned char *pD = base + 9u * RECON_3D_ROW_ARRAY;

    unsigned char *out = (unsigned char *)param_2;

    unsigned long stride = (unsigned long)(uint32_t)param_4;

    unsigned char *w0 = out + 0 * stride;
    unsigned char *w1 = out + 1 * stride;
    unsigned char *w2p = out + 2 * stride;
    unsigned char *w3p = out + 3 * stride;
    unsigned char *w5 = out + 5 * stride;
    unsigned char *w7 = out + 7 * stride;

    int32_t outer = param_3;
    do {
        uint32_t wA, wB, wC, wD;
        memcpy(&wA, pA, 4); pA += 4;
        memcpy(&wB, pB, 4); pB += 4;
        memcpy(&wC, pC, 4); pC += 4;
        memcpy(&wD, pD, 4);
        uint16_t nHalf;
        memcpy(&nHalf, pD, 2);
        pD += 4;

        int32_t iVar14 = (int32_t)((wA & 0xffffu) << 15);
        int32_t iVar15 = (int32_t)(((wA >> 16) & 0xffffu) << 15);
        int32_t iVar16 = (int32_t)(((wB >> 16) & 0xffffu) << 15);
        uint16_t u17 = (uint16_t)(wC & 0xffffu);
        uint16_t u18 = (uint16_t)((wC >> 16) & 0xffffu);
        uint16_t u19 = (uint16_t)((wD >> 16) & 0xffffu);

        int32_t n = (int32_t)(uint32_t)nHalf;
#ifdef __ARM_NEON

        const int32x4_t v14 = vdupq_n_s32(iVar14);
        const int32x4_t v15 = vdupq_n_s32(iVar15);
        const int32x4_t v16v = vdupq_n_s32(iVar16);
        const uint16x8_t v17 = vdupq_n_u16(u17);
        const uint16x8_t v18 = vdupq_n_u16(u18);
        const uint16x8_t v19 = vdupq_n_u16(u19);
        do {
            vst1q_s32((int32_t *)w3p, v14);  vst1q_s32((int32_t *)w3p + 4, v14);  w3p += 32;
            vst1q_s32((int32_t *)w5,  v15);  vst1q_s32((int32_t *)w5  + 4, v15);  w5  += 32;
            vst1q_s32((int32_t *)w7,  v16v); vst1q_s32((int32_t *)w7  + 4, v16v); w7  += 32;
            vst1q_u16((uint16_t *)w0,  v17); w0  += 16;
            vst1q_u16((uint16_t *)w1,  v18); w1  += 16;
            vst1q_u16((uint16_t *)w2p, v19); w2p += 16;
            n -= 8;
        } while (n > 0);
#else
        do {
            for (int k = 0; k < 8; k++) memcpy(w3p + (size_t)k * 4, &iVar14, 4);
            w3p += 32;
            for (int k = 0; k < 8; k++) memcpy(w5 + (size_t)k * 4, &iVar15, 4);
            w5 += 32;
            for (int k = 0; k < 8; k++) memcpy(w7 + (size_t)k * 4, &iVar16, 4);
            w7 += 32;
            for (int k = 0; k < 8; k++) memcpy(w0 + (size_t)k * 2, &u17, 2);
            w0 += 16;
            for (int k = 0; k < 8; k++) memcpy(w1 + (size_t)k * 2, &u18, 2);
            w1 += 16;
            for (int k = 0; k < 8; k++) memcpy(w2p + (size_t)k * 2, &u19, 2);
            w2p += 16;
            n -= 8;
        } while (n > 0);
#endif

        w3p += (long)n * 4;
        w5  += (long)n * 4;
        w7  += (long)n * 4;
        w0  += (long)n * 2;
        w1  += (long)n * 2;
        w2p += (long)n * 2;

        outer -= 1;
    } while (outer != 0);
}

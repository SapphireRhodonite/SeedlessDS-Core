#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "../../gpu.h"
#include <stddef.h>
#include "raster.h"
#include "core_internals.h"
#include "mem_access.h"

unsigned char raster_bucket_list_1x[RASTER_BUCKET_ROWS_1X * RASTER_BUCKET_ROW_BYTES];
int16_t raster_bucket_count_1x[RASTER_BUCKET_ROWS_1X];
unsigned char raster_bucket_list_2x[RASTER_BUCKET_ROWS_2X * RASTER_BUCKET_ROW_BYTES];
int16_t raster_bucket_count_2x[RASTER_BUCKET_ROWS_2X];








#define OFF_TABLE   0x3f2e120u
#define OFF_CALL_A  0x8d1b0u
#define OFF_CALL_B  0x8d2c4u
#define OFF_CALL_C  0x8d320u
#define OFF_CALL_D  0x8d3c8u
#define OFF_DST1 (8u * RECON_3D_ROW_ARRAY)
#define OFF_DST2 (2u * RECON_3D_ROW_ARRAY)
#define MAX_MATCHES 16

static inline uint32_t table_u32(const unsigned char *table_base, int64_t span)
{

    uint32_t v;
    memcpy(&v, table_base + span * 4, sizeof(v));
    return v;
}

void gpu3d_raster_line_window_select_and_ramp(uint8_t *param_1, void *param_2, void *param_3,
                         const gpu3d_bank_vertex_t *const *param_4, uint32_t param_5, uint32_t param_6,
                         int32_t param_7, uint64_t param_8)
{
    (void)param_1;
    unsigned char *ctx  = param_2;

    const gpu3d_bank_vertex_t *pairs[2 * MAX_MATCHES];

    unsigned char widths[MAX_MATCHES];
    uint32_t count = 0, total_sum = 0, lead_gap = 0;

    uint16_t f6_node0 = param_4[0]->y;

    if (f6_node0 < param_6) {
        const gpu3d_bank_vertex_t *const *running = param_4 + (int64_t)param_7 * 2;
        const gpu3d_bank_vertex_t *nodeA = param_4[0];
        const gpu3d_bank_vertex_t *nodeB = param_4[param_7];
        uint32_t f6_A = f6_node0;

        for (;;) {
            uint16_t f6_B = nodeB->y;

            int32_t low_gap  = (f6_A < param_5) ? ((int32_t)f6_A - (int32_t)param_5) : 0;
            int32_t height_gap  = (f6_B >= param_6) ? ((int32_t)f6_B - (int32_t)param_6) : 0;
            int32_t overlap = (int32_t)f6_B - (int32_t)f6_A + low_gap - height_gap;

            if (overlap >= 1) {

                widths[count] = (unsigned char)overlap;
                pairs[2 * count + 0] = nodeA;
                pairs[2 * count + 1] = nodeB;

                uint32_t gap = (f6_A < param_5) ? (param_5 - f6_A) : 0;
                total_sum += (uint32_t)overlap;
                if (count == 0) lead_gap = gap;
                count++;
            }

            const gpu3d_bank_vertex_t *nodeC = *running;
            nodeA = nodeB;
            running += param_7;
            nodeB = nodeC;
            f6_A = f6_B;

            if (f6_B >= param_6) break;
        }
    }

    gpu3d_raster_ramp_pairs_generate(param_3, pairs, widths, count, lead_gap);
    gpu3d_raster_divide_perspective_fixed(param_3, param_3, total_sum);
    gpu3d_raster_interp_lerp_rows(pairs, param_2, param_3, widths, count);
    gpu3d_raster_interp_five_ramps_rows(pairs, param_2, param_3, widths, count);

    if ((((uint32_t)param_8) & 0x18u) != 0) {

        if (count != 0) {
            uint16_t *dst1 = (uint16_t *)(ctx + OFF_DST1);

            const unsigned char *table = (const unsigned char *)
                recon_steps_table(raster_reciprocal_30, 0x40000000u);

            for (uint32_t j = 0; j < count; j++) {
                const gpu3d_bank_vertex_t *A = pairs[2 * j + 0];
                const gpu3d_bank_vertex_t *B = pairs[2 * j + 1];
                uint32_t W = widths[j];

                uint16_t a6 = A->y;
                uint16_t b6 = B->y;
                uint16_t a4 = A->x;
                uint16_t b4 = B->x;

                int64_t span = (int64_t)b6 - (int64_t)a6;
                uint32_t tbl = table_u32(table, span);
                int64_t d4 = (int64_t)b4 - (int64_t)a4;
                uint32_t flag_eq4 = (d4 == 0) ? 1u : 0u;

                int64_t prod = d4 * (int64_t)(uint64_t)tbl;
                int64_t prod_b = (d4 < 0) ? (prod + 0xfff) : prod;
                uint32_t step = (uint32_t)((uint64_t)prod_b >> 12);

                if (W == 0) continue;

                uint32_t base0 = (j == 0) ? (uint32_t)(lead_gap * step) : 0u;
                uint32_t acc = base0 + ((uint32_t)a4 << 18);
                uint32_t flag_scaled = flag_eq4 << 15;

                for (uint32_t i = 0; i < W; i++) {
                    uint16_t val = (uint16_t)(flag_scaled | (acc >> 18));
                    memcpy(dst1, &val, sizeof(val));
                    dst1 += 2;
                    acc += step;
                }
            }
        }
    } else {

        if (count != 0) {
            uint16_t *dst1 = (uint16_t *)(ctx + OFF_DST1);
            uint32_t *dst2 = (uint32_t *)(ctx + OFF_DST2);

            const unsigned char *table = (const unsigned char *)
                recon_steps_table(raster_reciprocal_30, 0x40000000u);

            for (uint32_t j = 0; j < count; j++) {
                const gpu3d_bank_vertex_t *A = pairs[2 * j + 0];
                const gpu3d_bank_vertex_t *B = pairs[2 * j + 1];
                uint32_t W = widths[j];

                uint16_t a6 = A->y;
                uint16_t b6 = B->y;
                uint16_t a4 = A->x;
                uint16_t b4 = B->x;
                uint16_t a8 = A->z;
                uint16_t b8 = B->z;

                int64_t span = (int64_t)b6 - (int64_t)a6;
                uint32_t tbl = table_u32(table, span);

                int64_t d4 = (int64_t)b4 - (int64_t)a4;
                uint32_t flag_eq4 = (d4 == 0) ? 1u : 0u;

                int32_t d8 = (int32_t)b8 - (int32_t)a8;
                int32_t d8_shift32 = (int32_t)(((uint32_t)d8) << 9);
                int64_t d8ext = (int64_t)d8_shift32;

                int64_t prodA = d4 * (int64_t)(uint64_t)tbl;
                int64_t prodA_b = (d4 < 0) ? (prodA + RASTER_STEP4_FRACTION_MASK) : prodA;
                uint32_t stepA = (uint32_t)((uint64_t)prodA_b >> 12);

                int64_t prodB = d8ext * (int64_t)(uint64_t)tbl;
                int64_t prodB_b = (d8 < 0) ? (prodB + RASTER_STEP8_NEGATIVE_BIAS) : prodB;

                uint32_t acc1_base = (uint32_t)a4 << 18;
                uint64_t acc2_base = (uint64_t)a8 << 39;

                if (j == 0) {
                    acc2_base = (uint64_t)((int64_t)acc2_base + prodB_b * (int64_t)lead_gap);
                    acc1_base = acc1_base + lead_gap * stepA;
                }

                if (W == 0) continue;

                uint32_t flag_scaled = flag_eq4 << 15;
                uint32_t acc1 = acc1_base;
                uint64_t acc2 = acc2_base;

                for (uint32_t i = 0; i < W; i++) {
                    uint16_t v1 = (uint16_t)(flag_scaled | (acc1 >> 18));
                    uint32_t v2 = (uint32_t)(acc2 >> 30);
                    memcpy(dst1, &v1, sizeof(v1));
                    memcpy(dst2, &v2, sizeof(v2));
                    dst1 += 2;
                    dst2 += 1;
                    acc1 += stepA;
                    acc2 += (uint64_t)prodB_b;
                }
            }
        }
    }

}
#undef OFF_TABLE
#undef OFF_CALL_A
#undef OFF_CALL_B
#undef OFF_CALL_C
#undef OFF_CALL_D
#undef OFF_DST1
#undef OFF_DST2
#undef MAX_MATCHES



static uint32_t adjust_word(uint32_t word, uint32_t factor)
{
    uint32_t idx = (word >> 24) & 0x1fu;
    uint32_t result = idx + factor + idx * factor;
    uint32_t height = (result << 19) & 0xff000000u;

    return (word & 0x00ffffffu) | height;
}

void gpu3d_raster_pixel_high_byte_adjust(void *param_1, uint32_t param_2, uint32_t param_3)
{

    uint8_t *p = (uint8_t *)param_1;
    uint32_t n_items = param_3;

    if (n_items == 0)
        return;

    uint32_t per_vector = n_items & 0xfffffff8u;
    if (n_items >= 8) {
        uint32_t left = per_vector;

        while (left != 0) {
            uint32_t block[8];

            memcpy(block, p, sizeof(block));
            for (uint32_t i = 0; i != 8; ++i)
                block[i] = adjust_word(block[i], param_2);
            memcpy(p, block, sizeof(block));

            p += 32;
            left -= 8;
        }

        if (n_items == per_vector)
            return;
    }

    uint32_t rest = n_items - per_vector;
    do {
        uint32_t idx = (uint32_t)(rd8(p + 3) & 0x1fu);
        uint32_t sum = idx + param_2;
        uint32_t result = idx * param_2 + sum;

        wr8(p + 3, (uint8_t)(result >> 5));
        p += 4;
        --rest;
    } while (rest != 0);
}



void gpu3d_raster_word_high_byte_mask(unsigned int *param_1, unsigned char param_2, int param_3)
{

    uint32_t n = (uint32_t)param_3;

    if (n == 0) {
        return;
    }

    uint32_t mask = 0x00ffffffu | ((uint32_t)param_2 << 24);

    unsigned char *dst = (unsigned char *)param_1;
    for (uint32_t i = 0; i < n; i++) {

        size_t off = (size_t)i * 4u;
        uint32_t w = rd32(dst + off);
        w &= mask;
        wr32(dst + off, w);
    }
}

void gpu3d_raster_band_row_clip_neighbors(unsigned char *base,
                        uint32_t until,
                        uint32_t flags)
{

    uint32_t w0 = 0, w1 = 0, w8 = 0, w9 = 0, w10 = 0, w11 = 0, w12 = 0;
    uint32_t w13 = 0, w14 = 0, w15 = 0, w16 = 0, w17 = 0;
    uint16_t h = 0;

    w1 = until;

    if (flags & 1u) {
        memcpy(&h, base + 0x57c, 2); w13 = h;
        memcpy(&h, base + 0x62c, 2); w9  = h;
        memcpy(&h, base + 0x580, 2); w8  = h;
        w10 = w9 + w13;
    } else {
        memcpy(&h, base + 0x580, 2); w8 = h;
        memcpy(&h, base + 0x634, 2); w9 = h;
        w13 = w8;
        w10 = w9 + w8;
    }

    w9  = (flags >> 1) & 1u;
    w11 = w9 ^ 1u;

    unsigned char *x12 = base + 0x630;
    int equal = (w1 == w11);
    w9 = flags & 2u;

    unsigned char *x8;

    if (equal) {
        x8 = base + 0x6e0;
    } else {
        memcpy(&h, x12, 2); w16 = h;
        w11 = ~w11;
        w12 = w11 + w1;
        unsigned char *x11 = base + 0x584;
        w14 = w8 + w16;

        for (;;) {
            memcpy(&h, x11, 2);        w15 = h;
            w0 = w8 + 1u;
            memcpy(&h, x11 + 176, 2);  w17 = h;

            w13 = (w0 < w13) ? w13 : (w8 + 1u);

            w1 = w14 - 1u;

            w13 = ((int32_t)w13 < (int32_t)w15) ? w15 : w13;

            w17 = w17 + w15;

            w10 = ((int32_t)w1 > (int32_t)w10) ? w10 : w1;

            w16 = w16 & 0xffffu;
            w13 = w13 - w8;

            w10 = ((int32_t)w10 > (int32_t)w17) ? w17 : w10;

            w10 = w14 - w10;

            w13 = (w13 > w16) ? w16 : w13;

            w10 = (w10 > w16) ? w16 : w10;

            h = (uint16_t)w13; memcpy(x11 + 348, &h, 2);
            h = (uint16_t)w10; memcpy(x11 + 350, &h, 2);

            if (w12 == 0) break;

            memcpy(&h, x11 + 176, 2); w16 = h;
            w12 -= 1u;
            x11 += 4;

            {
                uint32_t n10 = w14;
                uint32_t n13 = w8;
                uint32_t n14 = w17;
                uint32_t n8  = w15;
                w10 = n10; w13 = n13; w14 = n14; w8 = n8;
            }
        }

        x12 = x11 + 0xb0;
        x8  = x11 + 0x160;
    }

    if (w9 == 0) {
        uint16_t zero = 0;
        memcpy(&h, x12, 2); w9 = h;
        memcpy(x8 + 2, &zero, 2);
        w9 = w9 + 1u;
        h = (uint16_t)w9;
        memcpy(x8, &h, 2);
    }

    (void)w0;
}

void gpu3d_raster_band_bucket_append(unsigned char *obj, uint32_t v) {
    uint32_t n;
    memcpy(&n, obj + 4096, 4);
    uint16_t h = (uint16_t)v;
    memcpy(obj + (uint64_t)n * 2, &h, 2);
    n += 1;
    memcpy(obj + 4096, &n, 4);
}

void gpu3d_raster_block_summarize_and_delta(gpu3d_t *a, unsigned char *b) {
    unsigned char *bl  = a->fog_table;

    uint32_t *summary  = &((gpu3d_raster_t *)recon_cfg3d(b))->frame_ready;

    *summary = 0xffffffffu;

    uint32_t p0 = *(uint32_t *)(bl);
    uint32_t p1 = *(uint32_t *)(bl + 4);
    uint32_t p2 = *(uint32_t *)(bl + 8);
    uint32_t p3 = *(uint32_t *)(bl + 12);
    uint32_t p4 = *(uint32_t *)(bl + 16);
    uint32_t p5 = *(uint32_t *)(bl + 20);
    uint32_t p6 = *(uint32_t *)(bl + 24);
    uint32_t p7 = *(uint32_t *)(bl + 28);

    uint32_t all = p1 & p0 & p2 & p3 & p4 & p5 & p6 & p7;
    uint32_t any = p1 | p0 | p2 | p3 | p4 | p5 | p6 | p7;

    if (all == any) {
        uint32_t height = p7 >> 8;
        uint32_t mask = ~(p7 & (p7 >> 16)) | 0xffff00u;
        if ((mask & height) == 0)
            *summary = height;
    }

    for (int i = 0; i <= 30; i++)
        bl[32 + i] = (unsigned char)(bl[i + 1] - bl[i]);
}

void gpu3d_raster_band_row_scatter_dual_masked(const unsigned char *base,
                        unsigned char *dst_a,
                        unsigned char *dst_b,
                        uint32_t rows,
                        uint32_t alpha,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *mask)
{

    if (rows == 0) return;

    uint32_t f = 0;
    const unsigned char *p_cnt = base + 0x630;
    const unsigned char *p_hue = base + 0x580;

    uint32_t orval = alpha << 24;

    uint16_t n16, hue16;
    memcpy(&n16,   p_cnt, 2); p_cnt += 4;
    memcpy(&hue16, p_hue, 2); p_hue += 4;

    uint64_t n   = n16;
    uint64_t hue = hue16;

    for (;;) {
        if (n != 0) {
            uint64_t shift = hue << 2;
            uint64_t off  = 0;

            unsigned char *wb = dst_b + shift;
            unsigned char *wa = dst_a + shift;

            uint64_t lim = n << 2;
            const unsigned char *pm = mask;

            unsigned char c = *pm++;

            for (;;) {
                if (c != 0) {
                    uint32_t vb, va;

                    memcpy(&vb, src_b + off, 4);
                    memcpy(&va, src_a + off, 4);

                    vb |= orval;

                    memcpy(wb + off, &vb, 4);
                    memcpy(wa + off, &va, 4);
                }

                off += 4;
                if (lim == off) break;
                c = *pm++;
            }

            mask += n;
            src_a   += off;
            src_b   += off;
        }

        f += 1;
        dst_a += 0x400;
        dst_b += 0x400;
        if (f == rows) return;

        memcpy(&n16,   p_cnt, 2); p_cnt += 4;
        memcpy(&hue16, p_hue, 2); p_hue += 4;
        n   = n16;
        hue = hue16;
    }
}




void gpu3d_raster_band_row_copy_dual_tagged(uint8_t *state, uint8_t *dest_a,
                         uint8_t *dest_b, uint32_t pages,
                         uint8_t mark, uint8_t *origin_a,
                         uint8_t *origin_b)
{

    const uint8_t *table_a = state + 0x630;
    const uint8_t *table_b = state + 0x580;
    uint32_t mask = (uint32_t)mark << 24;
    uint64_t page = 0;

    if (pages == 0)
        return;

    do {
        uint64_t width_a = rd16(table_a);
        uint64_t width_b = rd16(table_b);
        uint64_t advanced = 0;

        table_a += 4;
        table_b += 4;

        if (width_a != 0) {
            if (width_a >= 8) {
                uint64_t offset_b = width_b * 4;
                uint64_t end = (width_a + width_b) * 4;
                uintptr_t d2_start = (uintptr_t)(dest_b + offset_b);
                uintptr_t d1_start = (uintptr_t)(dest_a + offset_b);
                uintptr_t d1_end = (uintptr_t)(dest_a + end);
                uintptr_t d2_end = (uintptr_t)(dest_b + end);
                uintptr_t oa_end = (uintptr_t)(origin_a + width_a * 4);
                uintptr_t ob_end = (uintptr_t)(origin_b + width_a * 4);
                int overlap =
                    (d2_start < d1_end && d1_start < d2_end) ||
                    (d2_start < oa_end && (uintptr_t)origin_a < d2_end) ||
                    (d2_start < ob_end && (uintptr_t)origin_b < d2_end) ||
                    (d1_start < oa_end && (uintptr_t)origin_a < d1_end) ||
                    (d1_start < ob_end && (uintptr_t)origin_b < d1_end);

                if (!overlap) {
                    advanced = width_a & ~UINT64_C(7);
                    for (uint64_t i = 0; i < advanced; i += 8) {
                        uint32_t vb[8];
                        uint32_t va[8];

                        for (uint64_t j = 0; j != 8; ++j)
                            vb[j] = rd32(origin_b + (i + j) * 4);
                        for (uint64_t j = 0; j != 8; ++j)
                            va[j] = rd32(origin_a + (i + j) * 4);
                        for (uint64_t j = 0; j != 8; ++j)
                            wr32(dest_b + (width_b + i + j) * 4,
                                  vb[j] | mask);
                        for (uint64_t j = 0; j != 8; ++j)
                            wr32(dest_a + (width_b + i + j) * 4, va[j]);
                    }
                }
            }

            for (uint64_t i = advanced; i < width_a; ++i) {
                uint32_t vb = rd32(origin_b + (i - advanced) * 4);
                uint32_t va = rd32(origin_a + (i - advanced) * 4);

                wr32(dest_b + (width_b + i) * 4, vb | mask);
                wr32(dest_a + (width_b + i) * 4, va);
            }

            origin_a += width_a * 4;
            origin_b += width_a * 4;
        }

        ++page;
        dest_a += 0x400;
        dest_b += 0x400;
    } while ((uint32_t)page != pages);
}

void gpu3d_raster_band_row_scatter_triple(const unsigned char *base,
                        unsigned char *dst_a,
                        unsigned char *dst_b,
                        unsigned char *dst_c,
                        uint32_t rows,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *src_c)
{

    if (rows == 0) return;

    uint32_t f = 0;
    const unsigned char *p_cnt = base + 0x630;
    const unsigned char *p_hue = base + 0x580;

    uint16_t n16, hue16;
    memcpy(&n16,   p_cnt, 2); p_cnt += 4;
    memcpy(&hue16, p_hue, 2); p_hue += 4;

    uint64_t n   = n16;
    uint64_t hue = hue16;

    for (;;) {
        if (n != 0) {
            uint64_t shift = hue << 2;
            int64_t  back = 0;
            uint64_t i = 0;

            unsigned char *wa = dst_a + shift;
            unsigned char *wb = dst_b + shift;
            unsigned char *wc = dst_c + hue;

            do {
                uint64_t off = i << 2;
                uint32_t v;

                memcpy(&v, src_a + off, 4);
                back -= 4;
                memcpy(wa + off, &v, 4);

                memcpy(&v, src_b + off, 4);
                memcpy(wb + off, &v, 4);

                wc[i] = src_c[i];

                i += 1;
            } while (n != i);

            src_a -= back;
            src_b -= back;
            src_c += i;
        }

        f += 1;
        dst_a += 0x400;
        dst_b += 0x400;
        dst_c += 0x100;
        if (f == rows) return;

        memcpy(&n16,   p_cnt, 2); p_cnt += 4;
        memcpy(&hue16, p_hue, 2); p_hue += 4;
        n   = n16;
        hue = hue16;
    }
}

void gpu3d_raster_band_row_scatter_fill_masked(unsigned char *dst_a,
                        unsigned char *dst_b,
                        uint32_t width,
                        uint32_t rows,
                        const unsigned char *src,
                        uint32_t value,
                        const unsigned char *mask)
{

    unsigned none = (rows == 0) | (width == 0);

    if ((width & 3) != 0) {
        if (none) return;

        uint32_t f = 0;
        uint64_t step = (uint64_t)(uint32_t)(width - 1) + 1;
        uint64_t lim  = (uint64_t)width;

        for (;;) {
            int64_t  back = 0;
            uint64_t i     = 0;
            unsigned char c = mask[i];

            for (;;) {
                if (c != 0) {
                    uint64_t off = i << 2;
                    uint32_t v;

                    memcpy(&v, src + off, 4);
                    memcpy(dst_b + off, &value, 4);
                    memcpy(dst_a + off, &v, 4);
                }

                i += 1;
                back -= 4;
                if (lim == i) break;
                c = mask[i];
            }

            f += 1;
            mask += step;
            dst_a += 0x400;
            dst_b += 0x400;
            src   -= back;
            if (f == rows) return;
        }
    } else {
        if (none) return;

        uint32_t f = 0;

        for (;;) {
            uint64_t i = 0;

            for (;;) {
                const unsigned char *p = mask + i;
                unsigned char c;
                uint32_t v, idx32;
                uint64_t off;

                c = p[0];
                if (c != 0) {
                    memcpy(&v, src, 4);
                    off = (uint64_t)(uint32_t)i << 2;
                    memcpy(dst_b + off, &value, 4);
                    memcpy(dst_a + off, &v, 4);
                }

                c = p[1];
                if (c != 0) {
                    memcpy(&v, src + 4, 4);
                    idx32 = (uint32_t)i + 1;
                    off = (uint64_t)idx32 << 2;
                    memcpy(dst_b + off, &value, 4);
                    memcpy(dst_a + off, &v, 4);
                }

                c = p[2];
                if (c != 0) {
                    memcpy(&v, src + 8, 4);
                    idx32 = (uint32_t)i + 2;
                    off = (uint64_t)idx32 << 2;
                    memcpy(dst_b + off, &value, 4);
                    memcpy(dst_a + off, &v, 4);
                }

                c = p[3];
                if (c != 0) {
                    memcpy(&v, src + 12, 4);
                    idx32 = (uint32_t)i + 3;
                    off = (uint64_t)idx32 << 2;
                    memcpy(dst_b + off, &value, 4);
                    memcpy(dst_a + off, &v, 4);
                }

                i += 4;
                src += 0x10;
                if ((uint32_t)i >= width) break;
            }

            f += 1;
            dst_a += 0x400;
            dst_b += 0x400;
            mask += i;
            if (f == rows) return;
        }
    }
}

void gpu3d_raster_band_row_scatter_triple_plain(unsigned char *dst_a,
                        unsigned char *dst_b,
                        unsigned char *dst_c,
                        uint32_t width,
                        uint32_t rows,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *src_c)
{

    if (rows == 0) return;
    if (width == 0) return;

    uint32_t row = 0;
    uint64_t n = width;

    do {
        int64_t retro = 0;
        uint64_t i = 0;

        do {
            uint64_t off = i << 2;
            uint32_t v;

            memcpy(&v, src_a + off, 4);
            retro -= 4;
            memcpy(dst_a + off, &v, 4);

            memcpy(&v, src_b + off, 4);
            memcpy(dst_b + off, &v, 4);

            dst_c[i] = src_c[i];

            i += 1;
        } while (n != i);

        row += 1;
        dst_a += 0x400;
        dst_b += 0x400;
        dst_c += 0x100;
        src_c += i;
        src_b -= retro;
        src_a -= retro;
    } while (row != rows);
}




void gpu3d_raster_band_page_range_copy(uint8_t *dest, const uint8_t *pages,
                        const uint8_t *table, uint32_t num_pages)
{

    if (num_pages == 0)
        return;

    for (uint32_t page = 0; page < num_pages; ++page) {
        uint16_t n_items = rd16(table + 0x630 + (size_t)page * 4);
        const uint8_t *origin_page;
        const uint8_t *origin;
        uint16_t idx;
        uint32_t in_blocks = 0;

        if (n_items == 0)
            continue;

        idx = rd16(table + 0x580 + (size_t)page * 4);
        origin_page = pages + (size_t)page * 0x400;
        origin = origin_page + (size_t)idx * 4;

        if (n_items >= 8) {
            uint32_t last = (uint32_t)n_items - 1;
            uintptr_t end_origin = (uintptr_t)pages +
                ((uint64_t)page << 10) + 4 +
                ((uint64_t)idx + last) * 4;
            uintptr_t end_dest = (uintptr_t)dest +
                (uint64_t)last * 4 + 4;

            if ((uintptr_t)dest >= end_origin ||
                (uintptr_t)origin >= end_dest) {
                in_blocks = (uint32_t)n_items & ~7u;

                for (uint32_t left = in_blocks; left != 0; left -= 8) {
                    uint8_t tmp[32];

                    memcpy(tmp, origin, sizeof(tmp));
                    memcpy(dest, tmp, sizeof(tmp));
                    origin += sizeof(tmp);
                    dest += sizeof(tmp);
                }

                if (in_blocks == n_items)
                    continue;
            }
        }

        n_items = (uint16_t)((uint32_t)n_items - in_blocks);
        do {
            uint32_t word = rd32(origin);

            origin += 4;
            wr32(dest, word);
            dest += 4;
            --n_items;
        } while (n_items != 0);
    }
}

void gpu3d_raster_band_row_gather_triple(unsigned char *dst_a,
                        unsigned char *dst_b,
                        unsigned char *dst_c,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *src_c,
                        const unsigned char *base,
                        uint32_t rows)
{

    if (rows == 0) return;

    uint32_t f = 0;
    const unsigned char *p_cnt = base + 0x630;
    const unsigned char *p_hue = base + 0x580;

    uint16_t n16, hue16;
    memcpy(&n16,   p_cnt, 2); p_cnt += 4;
    memcpy(&hue16, p_hue, 2); p_hue += 4;

    uint64_t n   = n16;
    uint64_t hue = hue16;

    for (;;) {
        if (n != 0) {
            uint64_t shift = hue << 2;
            int64_t  back = 0;
            uint64_t i = 0;

            const unsigned char *rc = src_c + hue;
            const unsigned char *rb = src_b + shift;
            const unsigned char *ra = src_a + shift;

            do {
                uint64_t off = i << 2;
                uint32_t v;

                memcpy(&v, ra + off, 4);
                back -= 4;
                memcpy(dst_a + off, &v, 4);

                memcpy(&v, rb + off, 4);
                memcpy(dst_b + off, &v, 4);

                dst_c[i] = rc[i];

                i += 1;
            } while ((uint32_t)n != (uint32_t)i);

            dst_a -= back;
            dst_b -= back;
            dst_c += i;
        }

        f += 1;
        src_a += 0x400;
        src_b += 0x400;
        src_c += 0x100;
        if (f == rows) return;

        memcpy(&n16,   p_cnt, 2); p_cnt += 4;
        memcpy(&hue16, p_hue, 2); p_hue += 4;
        n   = n16;
        hue = hue16;
    }
}



static void copy32(void *dst, const void *src)
{
    uint8_t tmp[32];
    memcpy(tmp, src, sizeof(tmp));
    memcpy(dst, tmp, sizeof(tmp));
}

void gpu3d_raster_band_row_copy(void *param_1, const void *param_2,
                        uint32_t param_3, uint32_t param_4)
{

    uint8_t *dst = (uint8_t *)param_1;
    const uint8_t *src = (const uint8_t *)param_2;
    uint64_t row = 0;
    uint64_t count = param_3;
    uint64_t block = count & 0xfffffff8ULL;

    if (param_4 == 0 || param_3 == 0)
        return;

    for (;;) {
        uint8_t *end_dst;

        if (param_3 >= 8 &&
            ((uintptr_t)dst >= (uintptr_t)src + count * 4 ||
             (uintptr_t)src >= (uintptr_t)dst + count * 4)) {
            uint64_t offset = 0;
            uint64_t left = block;

            end_dst = dst + block * 4;
            do {
                copy32(dst + offset, src + offset);
                left -= 8;
                offset += 32;
            } while (left != 0);

            if (block == count)
                goto next_row;

            {
                uint64_t idx = block;
                do {
                    uint32_t value = rd32(src + idx * 4);
                    idx++;
                    wr32(end_dst, value);
                    end_dst += 4;
                } while (idx != count);
            }
        } else {
            uint64_t idx = 0;
            end_dst = dst;
            do {
                uint32_t value = rd32(src + idx * 4);
                idx++;
                wr32(end_dst, value);
                end_dst += 4;
            } while (idx != count);
        }

next_row:
        row++;
        src += 0x400;
        dst = end_dst;
        if ((uint32_t)row == param_4)
            return;
    }
}

void gpu3d_raster_band_row_gather_triple_plain(unsigned char *dst_a,
                        unsigned char *dst_b,
                        unsigned char *dst_c,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *src_c,
                        int32_t width,
                        uint32_t rows)
{

    if (rows == 0) return;
    if (width <= 0) return;

    uint32_t a32  = (uint32_t)width;
    uint32_t w9a  = (a32 - 1u) & 0xfffffff8u;
    uint32_t w10a = (a32 + 7u) & 0xfffffff8u;
    uint32_t w12  = a32 - w9a;
    uint32_t w9b  = 256u - w10a;
    uint32_t w10b = w12 - 8u;

    int64_t  x10 = (int64_t)(int32_t)w10b;
    uint64_t x9  = (uint64_t)w9b;
    int64_t  x11 = (int64_t)((uint64_t)x10 << 2);
    uint64_t x12 = x9 << 2;

    uint32_t f = 0;

    for (;;) {
        uint64_t x13 = 0;
        uint32_t w14 = a32;

        do {
            const unsigned char *ra = src_a + x13;
            unsigned char       *wa = dst_a + x13;
            const unsigned char *rb = src_b + x13;
            unsigned char       *wb = dst_b + x13;
            int j;

            w14 -= 8u;

            x13 += 0x20u;

            for (j = 0; j < 8; j++) {
                uint32_t v;

                memcpy(&v, ra + 4 * j, 4);
                memcpy(wa + 4 * j, &v, 4);

                memcpy(&v, rb + 4 * j, 4);
                memcpy(wb + 4 * j, &v, 4);

                dst_c[j] = src_c[j];
            }

            src_c += 8;
            dst_c += 8;
        } while ((int32_t)w14 > 0);

        f += 1;

        dst_a = (unsigned char *)((uintptr_t)dst_a
                                  + (uintptr_t)x11
                                  + (uintptr_t)x13);
        dst_b = (unsigned char *)((uintptr_t)dst_b
                                  + (uintptr_t)x11
                                  + (uintptr_t)x13);
        src_a = (const unsigned char *)((uintptr_t)src_a
                                        + (uintptr_t)x12
                                        + (uintptr_t)x13);
        src_b = (const unsigned char *)((uintptr_t)src_b
                                        + (uintptr_t)x12
                                        + (uintptr_t)x13);

        dst_c = (unsigned char *)((uintptr_t)dst_c
                                  + (uintptr_t)x10);
        src_c = (const unsigned char *)((uintptr_t)src_c
                                        + (uintptr_t)x9);

        if (f == rows) return;
    }
}


void gpu3d_raster_bands_group_runs(uint8_t *a0, unsigned char *base, const gpu3d_polygon_t *a2,
                         uint8_t *a3, int cursor_start, int count,
                         uint32_t flag, const gpu3d_bank_vertex_t *a7)
{

    unsigned char *ptr   = base;
    unsigned char *table = base + 0x630;
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
    if (sum + current < 0x101u) goto G;
    goto C;

C:
    gpu3d_raster_span_band_compose_dispatch(a0, ptr, a2, cursor, nrun, a3, (int)sum, flag, a7);
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
    if (sum + current < 0x101u) goto G;
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

    gpu3d_raster_span_band_compose_dispatch(a0, ptr, a2, cursor, nrun, a3, (int)sum, flag, a7);
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
    gpu3d_raster_span_band_compose_dispatch(a0, ptr, a2, cursor, nrun, a3, (int)sum, flag, a7);
    return;
}

void gpu3d_raster_band_bucket_classify_by_depth(gpu3d_t *machine, gpu3d_band_bucket_t *list,
                        gpu3d_polygon_list_t *objs, gpu3d_bank_vertex_t *ent,
                        uint32_t mode) {

    int16_t  *cnt = raster_bucket_count_1x;
    unsigned char *lst = raster_bucket_list_1x;

    for (int k = 0; k < 12; k++)
        list[k].count = 0;

    uint32_t total = objs->count;
    if (total != 0) {
        uint32_t with4 = mode | 4;
        int32_t cache_a = -1, cache_b = -1;
        void *cache_v = 0;

        for (uint64_t i = 0; i < total; i++) {
            gpu3d_polygon_t *o = &objs->polygon[i];
            uint32_t *pd = &o->vertex_count;
            uint32_t d = *pd;
            uint32_t count = d & 0xf;
            if ((uint32_t)(count - 2) > 6) continue;

            uint32_t j = o->first_vertex;
            gpu3d_bank_vertex_t *e = &ent[j];

            int32_t smaller = e->y;
            int32_t larger = smaller;
            uint32_t y0 = e->w, y1 = y0;
            uint32_t z0 = e->z, z1 = z0;
            uint32_t w0 = e->color, w1 = w0;
            uint32_t turn = 0;

            gpu3d_bank_vertex_t *p = &ent[j + 1];
            for (uint32_t n = 1; n != count; n++) {
                uint32_t raw = p->y;
                uint32_t a = p->w;
                uint32_t bb = p->z;
                uint32_t c = p->color;

                int32_t v = (raw < 0xc0) ? (int32_t)raw : 0xc0;
                y0 &= a;  y1 |= a;
                if (v < smaller) { turn = n; smaller = v; }
                z0 &= bb; z1 |= bb;
                w0 &= c;  w1 |= c;
                p->y = (uint16_t)v;
                if (v > larger) larger = v;
                p++;
            }
            if (smaller == larger) continue;

            uint32_t bits = (w0 == w1) ? with4 : mode;
            uint32_t mark;
            if (machine->swap_params_previous & 2) {
                if (y0 == y1) bits |= 0x30;
                mark = bits | 8;
            } else {
                if (z0 == z1) bits |= 0x10;
                mark = (y0 == y1) ? (bits | 0x20) : bits;
            }

            uint32_t class = o->texture_param;
            if (class & 0x1c000000) {
                uint32_t neg = ~o->polygon_attr;
                if (neg & 0x30) {
                    uint32_t sub = o->palette_base;
                    mark |= 2;
                    if (cache_a != (int32_t)class || cache_b != (int32_t)sub) {
                        cache_v = gpu3d_raster_texture_cache_lookup_or_create(machine->texture_cache, class, sub);
                        d = *pd;
                        cache_a = (int32_t)class;
                        cache_b = (int32_t)sub;
                    }
                    o->texture = cache_v;
                }
            }

            uint32_t base = (mark << 8) | ((uint32_t)larger << 23) | d;
            *pd = base;

            if (count != 4) {
                *pd = (turn << 16) | (count << 19) | base;
            } else {
                uint32_t extra;
                if (d & 0x40) {
                    if (turn == 2 && smaller == ent[j + 3].y) turn = 3;
                    extra = 0x480000;
                } else {
                    extra = 0x200000;
                }
                *pd = base | extra | (turn << 16);
                gpu3d_poly_detect_axis_rect_fastpath((unsigned char *)o, (unsigned char *)e);
            }

            int16_t c = cnt[smaller];
            *(uint16_t *)(lst + (uint64_t)smaller * 4096 + (uint64_t)c * 2) =
                (uint16_t)i;
            cnt[smaller] = (int16_t)(c + 1);
        }
    }

    for (int64_t d = 0xc0; ; d--) {
        int16_t c = cnt[d];
        while (c >= 1) {
            uint32_t idx = *(uint16_t *)(lst + (uint64_t)d * 4096
                                             + (uint64_t)(c - 1) * 2);
            c--;
            uint32_t cap = objs->polygon[idx].vertex_count >> 23;
            if (cap >= 0xc0) cap = 0xc0;
            int16_t k = cnt[cap];
            *(uint16_t *)(lst + (uint64_t)cap * 4096 + (uint64_t)k * 2) =
                (uint16_t)idx;
            cnt[cap] = (int16_t)(k + 1);
        }
        cnt[d] = 0;
        if (d == 0) break;
    }

    for (uint64_t d = 1; d <= 0xc0; d++) {
        int16_t c = cnt[d];
        if (c >= 1) {
            uint32_t high = 0xfffu >> (11u - (uint32_t)((d - 1) >> 4));
            do {
                c--;
                uint32_t idx = *(uint16_t *)(lst + d * 4096 + (uint64_t)c * 2);
                gpu3d_polygon_t *o = &objs->polygon[idx];
                uint64_t j = (uint64_t)((uint16_t)(o->vertex_count >> 16) & 7)
                           + o->first_vertex;
                uint32_t y = ent[j].y;
                uint32_t mask = (0xfffu << (y >> 4)) & high;

                for (int k = 0; k < 12; k++) {
                    if (!(mask & (1u << k))) continue;
                    uint32_t *cc = &list[k].count;
                    uint16_t *l = (uint16_t *)list[k].list;
                    l[*cc] = (uint16_t)idx;
                    *cc += 1;
                }
            } while (c > 0);
        }
        cnt[d] = 0;
    }

    cnt[0] = 0;
}

void gpu3d_raster_band_bucket_classify(gpu3d_t *machine, gpu3d_band_bucket_t *list,
                        gpu3d_polygon_list_t *objs, gpu3d_bank_vertex_t *ent,
                        uint32_t mode) {

    if (mode == 0 || !(machine->swap_params_previous & 1)) {
        gpu3d_raster_band_bucket_classify_by_depth(
            machine, list, objs, ent, mode);
        return;
    }

    for (int k = 0; k < 12; k++)
        list[k].count = 0;

    uint32_t total = objs->count;
    if (total == 0) return;

    uint32_t with4 = mode | 4;
    int32_t cache_a = -1, cache_b = -1;
    void *cache_v = 0;

    for (uint64_t i = 0; i < total; i++) {
        gpu3d_polygon_t *o = &objs->polygon[i];
        uint32_t *pd = &o->vertex_count;
        uint32_t d = *pd;
        uint32_t count = d & 0xf;
        if ((uint32_t)(count - 2) > 6) continue;

        uint32_t j = o->first_vertex;
        gpu3d_bank_vertex_t *e = &ent[j];

        int32_t smaller = e->y;
        int32_t larger = smaller;
        uint32_t y0 = e->w, y1 = y0;
        uint32_t z0 = e->z, z1 = z0;
        uint32_t w0 = e->color, w1 = w0;
        uint32_t turn = 0;

        gpu3d_bank_vertex_t *p = &ent[j + 1];
        for (uint32_t n = 1; n != count; n++) {
            int32_t v  = p->y;
            uint32_t a = p->w;
            uint32_t bb = p->z;
            uint32_t c = p->color;
            p++;

            if (smaller > v) { turn = n; smaller = v; }
            y0 &= a;  y1 |= a;
            z0 &= bb; z1 |= bb;
            w0 &= c;  w1 |= c;
            if ((uint32_t)larger < (uint32_t)v) larger = v;
        }
        if (smaller == larger) continue;

        int32_t cap = (larger < 0xc0) ? larger : 0xc0;
        uint32_t bits = (w0 == w1) ? with4 : mode;

        uint32_t mark;
        if (machine->swap_params_previous & 2) {
            if (y0 == y1) bits |= 0x30;
            mark = bits | 8;
        } else {
            if (z0 == z1) bits |= 0x10;
            mark = (y0 == y1) ? (bits | 0x20) : bits;
        }

        uint32_t class = o->texture_param;
        if (class & 0x1c000000) {
            uint32_t neg = ~o->polygon_attr;
            if (neg & 0x30) {
                uint32_t sub = o->palette_base;
                mark |= 2;
                if (cache_a != (int32_t)class || cache_b != (int32_t)sub) {
                    cache_v = gpu3d_raster_texture_cache_lookup_or_create(
                                  machine->texture_cache, class, sub);
                    d = *pd;
                    cache_a = (int32_t)class;
                    cache_b = (int32_t)sub;
                }
                o->texture = cache_v;
            }
        }

        uint32_t base = (mark << 8) | ((uint32_t)cap << 23) | d;
        smaller = smaller & ~(smaller >> 31);
        *pd = base;

        if (count != 4) {
            *pd = (turn << 16) | (count << 19) | base;
        } else {
            uint32_t extra;
            if (d & 0x40) {
                if (turn == 2 && smaller == ent[j + 3].y) turn = 3;
                extra = 0x480000;
            } else {
                extra = 0x200000;
            }
            *pd = base | extra | (turn << 16);
            gpu3d_poly_detect_axis_rect_fastpath((unsigned char *)o, (unsigned char *)e);
        }

        int32_t aj = cap - 1;
        if (aj < 0) aj = cap + 14;
        uint32_t mask = (0xfffu >> (11 - (uint32_t)(aj >> 4)))
                         & (0xfffu << ((uint32_t)smaller >> 4));

        for (int k = 0; k < 12; k++) {
            if (!(mask & (1u << k))) continue;
            uint32_t *cnt = &list[k].count;
            uint16_t *l = (uint16_t *)list[k].list;
            l[*cnt] = (uint16_t)i;
            *cnt += 1;
        }
    }
}



void gpu3d_raster_band_bucket_dispatch(void *param_1, const gpu3d_band_bucket_t *param_2,
                         uint32_t param_3, uint32_t param_4,
                         uint8_t *param_5, void *param_6)
{

    uint32_t count = param_2->count;
    if (count == 0)
        return;

    uint64_t idx = 0;
    do {
        uint16_t label = rd16(param_2->list + (idx << 1));
        void *block = param_5 + ((uint64_t)label << 5);

        gpu3d_raster_span_edge_ramp_dispatch(param_1, block, param_6,
                                     param_3, param_4);

        count = param_2->count;
        idx++;
    } while (idx < (uint64_t)count);
}

void gpu3d_raster_band_attr_tables_build(unsigned char *machine, uint32_t base, uint32_t rows) {

    gpu3d_band_header_t *cfg = (gpu3d_band_header_t *)(machine + GPU3D_BAND_HEADER_OFFSET);
    gpu_t *a = cfg->gpu;
    gpu3d_raster_t *state = &a->raster;

    if (!((state->disp3dcnt >> 8) & 0x40)) {
        uint32_t n = rows << 8;
        gpu3d_raster_word_fill((uint32_t *)machine, state->clear_color_rgb, n);
        gpu3d_raster_word_fill((uint32_t *)GPU3D_BAND(machine)->plane1, state->clear_depth_word, n);
        return;
    }

    gpu3d_t *b = cfg->gpu3d;
    uint32_t turn = b->clear_image_offset;
    unsigned char *t1 = a->vram.texture[2];
    unsigned char *t2 = a->vram.texture[3];
    uint32_t fixed = state->clear_depth_word & 0x3f000000u;
    uint32_t shift = base + (turn >> 8);

    if (rows == 0) return;
    unsigned char *out = machine;

    if (t1 != 0 && t2 != 0) {

        for (uint32_t f = 0; f < rows; f++) {
            uint64_t o = (uint64_t)((uint32_t)((f + shift) & 0xff) << 8) * 2;
            for (uint64_t i = 0; i < 256; i++) {
                uint64_t k = (uint64_t)((turn + i) & 0xff) * 2;
                uint32_t v = gpu3d_geometry_color_expand_bgr555_alpha_bit15(
                                 *(uint16_t *)(t1 + o + k));
                uint32_t w = *(uint16_t *)(t2 + o + k);
                *(uint32_t *)(out + i * 4) = ((w << 16) & 0x80000000u) | v;
                *(uint32_t *)(out + GPU3D_BAND_PLANE_SIZE + i * 4) =
                    (((w & 0x7fffu) << 9) | fixed);
            }
            out += 0x400;
        }
        return;
    }

    if (t1 != 0) {

        uint32_t second = fixed | 0xfffe00u;
        for (uint32_t f = 0; f < rows; f++) {
            uint64_t o = (uint64_t)((uint32_t)((f + shift) & 0xff) << 8) * 2;
            for (uint64_t i = 0; i < 256; i++) {
                uint64_t k = (uint64_t)((turn + i) & 0xff) * 2;
                uint32_t v = gpu3d_geometry_color_expand_bgr555_alpha_bit15(
                                 *(uint16_t *)(t1 + o + k));
                *(uint32_t *)(out + i * 4) = v | 0x80000000u;
                *(uint32_t *)(out + GPU3D_BAND_PLANE_SIZE + i * 4) = second;
            }
            out += 0x400;
        }
        return;
    }

    if (t2 != 0) {

        for (uint32_t f = 0; f < rows; f++) {
            uint64_t o = (uint64_t)((uint32_t)((f + shift) & 0xff) << 8) * 2;
            for (uint64_t i = 0; i < 256; i++) {
                uint32_t w = *(uint16_t *)(t2 + o + (uint64_t)((turn + i) & 0xff) * 2);
                *(uint32_t *)(out + i * 4) = (w << 16) & 0x80000000u;
                *(uint32_t *)(out + GPU3D_BAND_PLANE_SIZE + i * 4) =
                    (((w & 0x7fffu) << 9) | fixed);
            }
            out += 0x400;
        }
        return;
    }

    uint32_t second = fixed | 0xfffe00u;
    for (uint32_t f = 0; f < rows; f++) {
        for (uint64_t i = 0; i < 256; i++) {
            *(uint32_t *)(out + i * 4) = 0x80000000u;
            *(uint32_t *)(out + GPU3D_BAND_PLANE_SIZE + i * 4) = second;
        }
        out += 0x400;
    }
}

extern void gpu3d_raster_byte_fill(unsigned char *dst, uint32_t value, int32_t n);

void gpu3d_raster_band_aux_buffers_clear(unsigned char *param_1, uint64_t param_2, int32_t param_3)
{
    (void)param_2;

    if (param_3 == 0) return;

    unsigned char *block = GPU3D_BAND(param_1)->plane2;
    int32_t counter = param_3;

    do {
        gpu3d_raster_byte_fill(block, 0xff, 0x100);
        counter--;
        block += 0x100;
    } while (counter != 0);
}



void gpu3d_raster_priority_buffer_copy_masked(unsigned char *dst, unsigned char *src)
{

    uintptr_t d = (uintptr_t)dst;
    uintptr_t s = (uintptr_t)src;

    if ((s + RASTER_BAND_COPY_BYTES <= d) || (d + RASTER_BAND_COPY_BYTES <= s)) {

        unsigned long off = 0;
        do {
            unsigned char *ps = src + off;
            unsigned char *pd = dst + off;

            uint32_t w0 = rd32(ps + 0);
            uint32_t w1 = rd32(ps + 4);
            uint32_t w2 = rd32(ps + 8);
            uint32_t w3 = rd32(ps + 12);
            uint32_t w4 = rd32(ps + 16);
            uint32_t w5 = rd32(ps + 20);
            uint32_t w6 = rd32(ps + 24);
            uint32_t w7 = rd32(ps + 28);

            off += 0x20;

            wr32(pd + 0,  w0 & 0x3fffffffU);
            wr32(pd + 4,  w1 & 0x3fffffffU);
            wr32(pd + 8,  w2 & 0x3fffffffU);
            wr32(pd + 12, w3 & 0x3fffffffU);
            wr32(pd + 16, w4 & 0x3fffffffU);
            wr32(pd + 20, w5 & 0x3fffffffU);
            wr32(pd + 24, w6 & 0x3fffffffU);
            wr32(pd + 28, w7 & 0x3fffffffU);
        } while (off != 0x4000UL);
    } else {

        unsigned long off = 0;
        do {
            uint32_t w = rd32(src + off);
            wr32(dst + off, w & 0x3fffffffU);
            off += 4;
        } while (off != 0x4000UL);
    }
}


void gpu3d_raster_band_line_priority_mark(unsigned char *dst,
                        const unsigned char *a,
                        const unsigned char *b,
                        uint32_t p4)
{

    const uint32_t p4_low = p4 & 0xffffffu;
    const uint32_t p4_k6  = (p4 >> 24) & 0x3fu;

    uint32_t w11;
    {
        uint32_t v = rd32(a);

        if (((v >> 30) & 1u) == 0u) {
            w11 = 0xffu;
        } else {
            uint32_t n_a1 = rd32(a + 4);
            uint32_t n_b0 = rd32(b);

            uint32_t key = ((v >> 24) & 0x7fu) ^ 0x40u;
            uint32_t low = v & 0xffffffu;

            uint32_t t_p4 = (uint32_t)(low <  p4_low)              &
                            (uint32_t)(key != p4_k6);
            uint32_t t_a1 = (uint32_t)(low < (n_a1 & 0xffffffu))   &
                            (uint32_t)(key != ((n_a1 >> 24) & 0x3fu));
            uint32_t t_b0 = (uint32_t)(low < (n_b0 & 0xffffffu))   &
                            (uint32_t)(key != ((n_b0 >> 24) & 0x3fu));

            uint32_t cover = t_b0 | (t_p4 | t_a1);

            w11 = (cover != 0u) ? 0u : 0xffu;
            w11 = w11 | (key >> 3);
        }
    }
    dst[0] = (unsigned char)w11;
    dst += 1;

    uint32_t cur = rd32(a + 4);
    uint64_t i = 0;

    for (;;) {
        uint32_t out;

        if (((cur >> 30) & 1u) != 0u) {
            uint32_t n_pre = rd32(a + i * 4);
            uint32_t n_pos = rd32(a + i * 4 + 8);
            uint32_t n_b   = rd32(b + 4 + i * 4);

            uint32_t low = cur & 0xffffffu;
            uint32_t key = ((cur >> 24) & 0x7fu) ^ 0x40u;

            uint32_t tA = (uint32_t)(low < (n_pre & 0xffffffu)) &
                          (uint32_t)(key != ((n_pre >> 24) & 0x3fu));
            uint32_t tB = (uint32_t)(low < (n_pos & 0xffffffu)) &
                          (uint32_t)(key != ((n_pos >> 24) & 0x3fu));
            uint32_t tC = (uint32_t)(low <  p4_low)             &
                          (uint32_t)(key != p4_k6);
            uint32_t tD = (uint32_t)(low < (n_b   & 0xffffffu)) &
                          (uint32_t)(key != ((n_b   >> 24) & 0x3fu));

            uint32_t cover = tD | (tC | (tA | tB));

            out = ((cover != 0u) ? 0u : 0xffu) | (key >> 3);
        } else {
            out = 0xffu;
        }

        dst[i] = (unsigned char)out;

        cur = rd32(a + i * 4 + 8);
        i += 1;
        if ((uint32_t)i == 0xfeu)
            break;
    }

    {
        uint32_t w11c = 0xffu;

        if (((cur >> 30) & 1u) != 0u) {
            uint32_t n_pre = rd32(a + i * 4);
            uint32_t n_b   = rd32(b + i * 4 + 4);

            uint32_t low = cur & 0xffffffu;
            uint32_t key = ((cur >> 24) & 0x7fu) ^ 0x40u;

            uint32_t tA = (uint32_t)(low < (n_pre & 0xffffffu)) &
                          (uint32_t)(key != ((n_pre >> 24) & 0x3fu));
            uint32_t tC = (uint32_t)(low <  p4_low)             &
                          (uint32_t)(key != p4_k6);
            uint32_t tD = (uint32_t)(low < (n_b   & 0xffffffu)) &
                          (uint32_t)(key != ((n_b   >> 24) & 0x3fu));

            uint32_t cover = tD | (tC | tA);

            w11c = ((cover != 0u) ? 0u : 0xffu) | (key >> 3);
        }

        dst[i] = (unsigned char)w11c;
    }
}


void gpu3d_raster_band_line_priority_mark_swapped(unsigned char *dst,
                        const unsigned char *sec,
                        const unsigned char *pri,
                        uint32_t ext)
{

    uint32_t w8 = rd32_at(pri, 0);
    uint32_t w9;
    uint32_t w11;

    if ((w8 >> 30) & 1u) {
        uint32_t w12 = rd32_at(pri, 4);
        uint32_t w13 = rd32_at(sec, 0);
        uint32_t w10;

        w11 = ((w8 >> 24) & 0x7fu) ^ 0x40u;
        w9  = (ext >> 24) & 0x3fu;
        w10 = w8 & 0xffffffu;
        w8  = ext & 0xffffffu;

        uint32_t t_ext = (uint32_t)(w10 < w8) & (uint32_t)(w11 != w9);
        uint32_t t_pri = (uint32_t)(w10 < (w12 & 0xffffffu))
                       & (uint32_t)(w11 != ((w12 >> 24) & 0x3fu));
        uint32_t t_sec = (uint32_t)(w10 < (w13 & 0xffffffu))
                       & (uint32_t)(w11 != ((w13 >> 24) & 0x3fu));
        uint32_t cond = t_sec | (t_ext | t_pri);

        w11 = (cond != 0u ? 0u : 0xffu) | (w11 >> 3);
    } else {
        w9  = (ext >> 24) & 0x3fu;
        w8  = ext & 0xffffffu;
        w11 = 0xffu;
    }

    dst[0] = (unsigned char)w11;
    dst += 1;

    uint32_t w13 = rd32_at(pri, 4);
    uint64_t i = 0;
    uint32_t bit30 = (w13 >> 30) & 1u;

    for (;;) {
        uint32_t byte;

        if (bit30) {
            uint32_t w17 = rd32_at(pri, i * 4);
            uint32_t w16 = rd32_at(pri, i * 4 + 8);
            uint32_t w14 = w13 & 0xffffffu;
            uint32_t tag = ((w13 >> 24) & 0x7fu) ^ 0x40u;
            uint32_t w15 = rd32_at(sec + 4, i * 4);

            uint32_t t_left = (uint32_t)(w14 < (w17 & 0xffffffu))
                           & (uint32_t)(tag != ((w17 >> 24) & 0x3fu));
            uint32_t t_right = (uint32_t)(w14 < (w16 & 0xffffffu))
                           & (uint32_t)(tag != ((w16 >> 24) & 0x3fu));
            uint32_t t_sec = (uint32_t)(w14 < (w15 & 0xffffffu))
                           & (uint32_t)(tag != ((w15 >> 24) & 0x3fu));
            uint32_t t_ext = (uint32_t)(w14 < w8) & (uint32_t)(tag != w9);
            uint32_t cond = t_ext | (t_sec | (t_left | t_right));

            byte = (cond != 0u ? 0u : 0xffu) | (tag >> 3);
        } else {
            byte = 0xffu;
        }

        dst[i] = (unsigned char)byte;

        w13 = rd32_at(pri, i * 4 + 8);
        i += 1;
        bit30 = (w13 >> 30) & 1u;
        if ((uint32_t)i == 0xfeu) break;
    }

    w11 = 0xffu;
    if ((w13 >> 30) & 1u) {
        uint32_t w15 = rd32_at(pri, i * 4);
        uint32_t w12 = w13 & 0xffffffu;
        uint32_t tag = ((w13 >> 24) & 0x7fu) ^ 0x40u;
        uint32_t w14 = rd32_at(sec + i * 4, 4);

        uint32_t t_left = (uint32_t)(w12 < (w15 & 0xffffffu))
                       & (uint32_t)(tag != ((w15 >> 24) & 0x3fu));
        uint32_t t_ext = (uint32_t)(w12 < w8) & (uint32_t)(tag != w9);
        uint32_t t_sec = (uint32_t)(w12 < (w14 & 0xffffffu))
                       & (uint32_t)(tag != ((w14 >> 24) & 0x3fu));
        uint32_t cond = t_sec | (t_ext | t_left);

        w11 = (cond != 0u ? 0u : 0xffu) | (tag >> 3);
    }

    dst[i] = (unsigned char)w11;
}

void gpu3d_raster_band_compose_set_alpha(gpu_t *machine) {

    gpu_t *gpu = machine;
    gpu3d_t *ctx = ((gpu3d_band_header_t *)(gpu->band_context[0] + GPU3D_BAND_HEADER_OFFSET))->gpu3d;
    unsigned char *layer = gpu->band_rows + GPU3D_BAND_ROWS_BACK;
    unsigned char *bl0 = gpu->band_rows;
    unsigned char *bl1 = gpu->band_rows + 0x400;
    unsigned char *bl2 = gpu->band_rows + 0x800;
    unsigned char *bl3 = gpu->band_rows + 0xc00;
    gpu3d_raster_t *cfg = &gpu->raster;

    uint32_t *count = &ctx->fog_color;
    unsigned char *tab2  = ctx->edge_color_expanded[0];
    unsigned char *table = ctx->fog_table;

    unsigned char raw[288 + 8];
    unsigned char *scratch = raw;
    if (((uintptr_t)scratch & 8) != 0) scratch += 8;

    uint32_t field = cfg->disp3dcnt;
    uint32_t width = cfg->clear_depth_word;
    unsigned char *output = cfg->frame_front + 16u * GPU3D_BAND_ROW_BYTES;

    uint32_t shift = (field >> 8) & 0xf;
    uint32_t height = (ctx->fog_offset & 0x7fff) + (0x400u >> shift);
    uint32_t pair  = (shift & 0xffffu) | (height << 16);

    for (uint64_t d = 0; d != 0xb000; d += 0x1000) {
        unsigned char *b = bl0 + d;
        unsigned char *a = bl1 + d;
        unsigned char *before = output - 0x400;

        gpu3d_raster_fog_density_line(a, scratch, table, pair);
        gpu3d_raster_set_alpha_plane(layer, layer, scratch, *count);
        unsigned char *c = bl2 + d;
        gpu3d_raster_test_neighbor_edge_dual(scratch, b, a, c, width);
        gpu3d_raster_palette_blit_256(before, layer, scratch, tab2);

        unsigned char *layer2 = layer + 0x400;
        gpu3d_raster_fog_density_line(c, scratch, table, pair);
        gpu3d_raster_set_alpha_plane(layer2, layer2, scratch, *count);
        gpu3d_raster_test_neighbor_edge_dual(scratch, a, c, bl3 + d, width);
        gpu3d_raster_palette_blit_256(output, layer2, scratch, tab2);

        output += 0x4000;
        layer   += 0x800;
    }
}

void gpu3d_raster_band_draw_dispatch(unsigned char *machine) {

    gpu3d_band_header_t *cfg = (gpu3d_band_header_t *)(machine + GPU3D_BAND_HEADER_OFFSET);
    uint32_t row_height = cfg->threads;
    if (row_height > 12) return;

    gpu_t *a = cfg->gpu;
    gpu3d_t *b = cfg->gpu3d;

    uint64_t which = (uint64_t)b->bank ^ 1u;
    gpu3d_bank_vertex_t *desc2 = b->vertex_bank[which].vertex;
    uint32_t *cnt2 = &b->translucent[which].count;

    unsigned char *bufs[16];
    for (int k = 0; k < 16; k++) bufs[k] = GPU3D_BAND(machine)->plane2 + k * 0x100;

    gpu3d_raster_t *mark = &a->raster;
    unsigned char *rows = a->raster.frame_front;
    gpu3d_band_bucket_t *list1 = a->bucket[0];
    gpu3d_band_bucket_t *list2 = a->bucket[1];

    uint32_t strips = 12u / row_height;

    for (uint32_t f = 0; ; ) {
        uint32_t base = cfg->index;
        uint32_t idx = f * row_height + base;
        uint32_t y0 = idx << 4;
        uint32_t y1 = y0 + 16;

        gpu3d_raster_band_attr_tables_build(machine, y0, 16);
        cfg->band_mark = 0xffffffffu;
        cfg->band_dirty = 0;

        uint32_t *n1 = &list1[idx].count;
        if (*n1 != 0) {
            const uint16_t *l = (const uint16_t *)list1[idx].list;
            for (uint64_t i = 0; i < *n1; i++)
                gpu3d_raster_span_edge_ramp_dispatch(machine, &b->opaque[which].polygon[l[i]],
                      desc2, y0, y1);
        }

        if (*cnt2 != 0) {
            for (int k = 0; k < 16; k++) gpu3d_raster_byte_fill(bufs[k], 255, 256);

            uint32_t *n2 = &list2[idx].count;
            if (*n2 != 0) {
                const uint16_t *l =
                    (const uint16_t *)list2[idx].list;
                for (uint64_t i = 0; i < *n2; i++)
                    gpu3d_raster_span_edge_ramp_dispatch(machine, &b->translucent[which].polygon[l[i]],
                          desc2, y0, y1);
            }
        }

        uint32_t state = mark->disp3dcnt;
        uint32_t height = (cfg->pass == 0) ? ((state >> 3) & 4) : 0;
        uint32_t sel = (height & ~3u) | ((state >> 6) & 3);
        uint32_t k = sel - 2;
        unsigned char *frame = rows + (uint64_t)(uint32_t)(idx << 12) * 4;

        if (k > 5) {
            gpu3d_raster_words_copy_mask29((uint32_t *)frame, (const uint32_t *)machine);
        } else {
            static const uint8_t dest[6] = { 0, 1, 2, 2, 3, 4 };
            switch (dest[k]) {
            case 0: gpu3d_raster_fog_apply_frame(machine, frame); break;
            case 1: gpu3d_raster_page_compose_fog_alpha(machine, frame); break;
            case 2: gpu3d_raster_band_compose_plain(
                        machine, frame, idx); break;
            case 3: gpu3d_raster_band_compose_fogged(
                        machine, frame, idx); break;
            default: gpu3d_raster_band_compose_fog_alpha(
                        machine, frame, idx); break;
            }
        }

        f++;
        if (f >= strips) return;
        row_height = cfg->threads;
    }
}

void gpu3d_raster_band_bucket_classify_by_depth_hires(gpu3d_t *machine, gpu3d_band_bucket_t *list,
                        gpu3d_polygon_list_t *objs, gpu3d_bank_vertex_t *ent,
                        uint32_t mode) {

    int16_t  *cnt = recon_buckets_cnt(raster_bucket_count_2x);
    unsigned char *lst = recon_buckets_lst(raster_bucket_list_2x);

    for (unsigned k = 0; k < RECON_BANDS; k++)
        list[k].count = 0;

    uint32_t total = objs->count;
    if (total != 0) {
        uint32_t with4 = mode | 4;
        int32_t cache_a = -1, cache_b = -1;
        void *cache_v = 0;

        for (uint64_t i = 0; i < total; i++) {
            gpu3d_polygon_t *o = &objs->polygon[i];
            uint32_t *pd = &o->vertex_count;
            uint32_t d = *pd;
            uint32_t count = d & 0xf;
            if ((uint32_t)(count - 2) > 6) continue;

            uint32_t j = o->first_vertex;
            gpu3d_bank_vertex_t *e = &ent[j];

            int32_t smaller = e->y;
            int32_t larger = smaller;
            uint32_t y0 = e->w, y1 = y0;
            uint32_t z0 = e->z, z1 = z0;
            uint32_t w0 = e->color, w1 = w0;
            uint32_t turn = 0;

            gpu3d_bank_vertex_t *p = &ent[j + 1];
            for (uint32_t n = 1; n != count; n++) {
                uint32_t raw = p->y;
                uint32_t a = p->w;
                uint32_t bb = p->z;
                uint32_t c = p->color;

                int32_t v = (raw < (int32_t)RECON_3D_HEIGHT)
                              ? (int32_t)raw : (int32_t)RECON_3D_HEIGHT;
                y0 &= a;  y1 |= a;
                if (v < smaller) { turn = n; smaller = v; }
                z0 &= bb; z1 |= bb;
                w0 &= c;  w1 |= c;
                p->y = (uint16_t)v;
                if (v > larger) larger = v;
                p++;
            }
            if (smaller == larger) continue;

            uint32_t bits = (w0 == w1) ? with4 : mode;
            uint32_t mark;
            if (machine->swap_params_previous & 2) {
                if (y0 == y1) bits |= 0x30;
                mark = bits | 8;
            } else {
                if (z0 == z1) bits |= 0x10;
                mark = (y0 == y1) ? (bits | 0x20) : bits;
            }

            uint32_t class = o->texture_param;
            if (class & 0x1c000000) {
                uint32_t neg = ~o->polygon_attr;
                if (neg & 0x30) {
                    uint32_t sub = o->palette_base;
                    mark |= 2;
                    if (cache_a != (int32_t)class || cache_b != (int32_t)sub) {
                        cache_v = gpu3d_raster_texture_cache_lookup_or_create(machine->texture_cache, class, sub);
                        d = *pd;
                        cache_a = (int32_t)class;
                        cache_b = (int32_t)sub;
                    }
                    o->texture = cache_v;
                }
            }

            recon_height_set((unsigned char *)objs, (unsigned)i, (unsigned)larger);
            uint32_t base = (mark << 8) | (((uint32_t)larger & 0x1ffu) << 23) | d;
            *pd = base;

            if (count != 4) {
                *pd = (turn << 16) | (count << 19) | base;
            } else {
                uint32_t extra;
                if (d & 0x40) {
                    if (turn == 2 && smaller == ent[j + 3].y) turn = 3;
                    extra = 0x480000;
                } else {
                    extra = 0x200000;
                }
                *pd = base | extra | (turn << 16);
                gpu3d_poly_detect_axis_rect_fastpath((unsigned char *)o, (unsigned char *)e);
            }

            int16_t c = cnt[smaller];
            *(uint16_t *)(lst + (uint64_t)smaller * 4096 + (uint64_t)c * 2) =
                (uint16_t)i;
            cnt[smaller] = (int16_t)(c + 1);
        }
    }

    for (int64_t d = RECON_3D_HEIGHT; ; d--) {
        int16_t c = cnt[d];
        while (c >= 1) {
            uint32_t idx = *(uint16_t *)(lst + (uint64_t)d * 4096
                                             + (uint64_t)(c - 1) * 2);
            c--;
            uint32_t cap = recon_height_get((const unsigned char *)&objs->polygon[idx],
                objs->polygon[idx].vertex_count >> 23);
            if (cap >= (int32_t)RECON_3D_HEIGHT) cap = (int32_t)RECON_3D_HEIGHT;
            int16_t k = cnt[cap];
            *(uint16_t *)(lst + (uint64_t)cap * 4096 + (uint64_t)k * 2) =
                (uint16_t)idx;
            cnt[cap] = (int16_t)(k + 1);
        }
        cnt[d] = 0;
        if (d == 0) break;
    }

    for (uint64_t d = 1; d <= RECON_3D_HEIGHT; d++) {
        int16_t c = cnt[d];
        if (c >= 1) {

            const uint32_t all = (RECON_BANDS >= 32u) ? 0xffffffffu : ((1u << RECON_BANDS) - 1u);
            uint32_t high = all >> ((RECON_BANDS - 1u) - (uint32_t)((d - 1) / RECON_BAND_ROWS));
            do {
                c--;
                uint32_t idx = *(uint16_t *)(lst + d * 4096 + (uint64_t)c * 2);
                gpu3d_polygon_t *o = &objs->polygon[idx];
                uint64_t j = (uint64_t)((uint16_t)(o->vertex_count >> 16) & 7)
                           + o->first_vertex;
                uint32_t y = ent[j].y;
                uint32_t mask = (all << (y / RECON_BAND_ROWS)) & high;

                for (unsigned k = 0; k < RECON_BANDS; k++) {
                    if (!(mask & (1u << k))) continue;
                    uint32_t *cc = &list[k].count;
                    uint16_t *l = (uint16_t *)list[k].list;
                    l[*cc] = (uint16_t)idx;
                    *cc += 1;
                }
            } while (c > 0);
        }
        cnt[d] = 0;
    }

    cnt[0] = 0;
}

void gpu3d_raster_band_bucket_classify_hires(gpu3d_t *machine, gpu3d_band_bucket_t *list,
                        gpu3d_polygon_list_t *objs, gpu3d_bank_vertex_t *ent,
                        uint32_t mode) {

    if (mode == 0 || !(machine->swap_params_previous & 1)) {
        gpu3d_raster_band_bucket_classify_by_depth_hires(
            machine, list, objs, ent, mode);
        return;
    }

    for (unsigned k = 0; k < RECON_BANDS; k++)
        list[k].count = 0;

    uint32_t total = objs->count;
    if (total == 0) return;

    uint32_t with4 = mode | 4;
    int32_t cache_a = -1, cache_b = -1;
    void *cache_v = 0;

    for (uint64_t i = 0; i < total; i++) {
        gpu3d_polygon_t *o = &objs->polygon[i];
        uint32_t *pd = &o->vertex_count;
        uint32_t d = *pd;
        uint32_t count = d & 0xf;
        if ((uint32_t)(count - 2) > 6) continue;

        uint32_t j = o->first_vertex;
        gpu3d_bank_vertex_t *e = &ent[j];

        int32_t smaller = e->y;
        int32_t larger = smaller;
        uint32_t y0 = e->w, y1 = y0;
        uint32_t z0 = e->z, z1 = z0;
        uint32_t w0 = e->color, w1 = w0;
        uint32_t turn = 0;

        gpu3d_bank_vertex_t *p = &ent[j + 1];
        for (uint32_t n = 1; n != count; n++) {
            int32_t v  = p->y;
            uint32_t a = p->w;
            uint32_t bb = p->z;
            uint32_t c = p->color;
            p++;

            if (smaller > v) { turn = n; smaller = v; }
            y0 &= a;  y1 |= a;
            z0 &= bb; z1 |= bb;
            w0 &= c;  w1 |= c;
            if ((uint32_t)larger < (uint32_t)v) larger = v;
        }
        if (smaller == larger) continue;

        int32_t cap = (larger < (int32_t)RECON_3D_HEIGHT)
                         ? larger : (int32_t)RECON_3D_HEIGHT;
        uint32_t bits = (w0 == w1) ? with4 : mode;

        uint32_t mark;
        if (machine->swap_params_previous & 2) {
            if (y0 == y1) bits |= 0x30;
            mark = bits | 8;
        } else {
            if (z0 == z1) bits |= 0x10;
            mark = (y0 == y1) ? (bits | 0x20) : bits;
        }

        uint32_t class = o->texture_param;
        if (class & 0x1c000000) {
            uint32_t neg = ~o->polygon_attr;
            if (neg & 0x30) {
                uint32_t sub = o->palette_base;
                mark |= 2;
                if (cache_a != (int32_t)class || cache_b != (int32_t)sub) {
                    cache_v = gpu3d_raster_texture_cache_lookup_or_create(
                                  machine->texture_cache, class, sub);
                    d = *pd;
                    cache_a = (int32_t)class;
                    cache_b = (int32_t)sub;
                }
                o->texture = cache_v;
            }
        }

        recon_height_set((unsigned char *)objs, (unsigned)i, (unsigned)cap);
        uint32_t base = (mark << 8) | ((uint32_t)cap << 23) | d;
        smaller = smaller & ~(smaller >> 31);
        *pd = base;

        if (count != 4) {
            *pd = (turn << 16) | (count << 19) | base;
        } else {
            uint32_t extra;
            if (d & 0x40) {
                if (turn == 2 && smaller == ent[j + 3].y) turn = 3;
                extra = 0x480000;
            } else {
                extra = 0x200000;
            }
            *pd = base | extra | (turn << 16);
            gpu3d_poly_detect_axis_rect_fastpath((unsigned char *)o, (unsigned char *)e);
        }

        int32_t aj = cap - 1;
        if (aj < 0) aj = cap + (int32_t)RECON_WRAP_ROWS;

        const uint32_t all = (RECON_BANDS >= 32u) ? 0xffffffffu : ((1u << RECON_BANDS) - 1u);
        uint32_t mask = (all >> ((RECON_BANDS - 1u) - (uint32_t)(aj / RECON_BAND_ROWS)))
                         & (all << ((uint32_t)smaller / RECON_BAND_ROWS));

        for (unsigned k = 0; k < RECON_BANDS; k++) {
            if (!(mask & (1u << k))) continue;
            uint32_t *cnt = &list[k].count;
            uint16_t *l = (uint16_t *)list[k].list;
            l[*cnt] = (uint16_t)i;
            *cnt += 1;
        }
    }
}



void gpu3d_raster_band_entry_dispatch(uint8_t *param_1, const gpu3d_band_bucket_t *param_2,
                         uint32_t param_3, uint32_t param_4,
                         uint8_t *param_5, const gpu3d_bank_vertex_t *param_6)
{

    if (param_2->count != 0) {
        uint64_t idx = 0;

        do {
            uint16_t entry = rd16(param_2->list + idx * 2u);

            gpu3d_raster_span_edge_ramp_dispatch_hires(
                param_1,
                (const gpu3d_polygon_t *)(param_5 + ((uint64_t)entry << 5)),
                param_6, param_3, param_4);
            idx++;
        } while (idx < param_2->count);
    }
}

extern uint32_t gpu3d_geometry_color_expand_bgr555_alpha_bit15(uint64_t param_1);
extern void gpu3d_raster_word_fill(uint32_t *dst, uint32_t value, int32_t n);

void gpu3d_raster_band_attr_tables_build_wide(unsigned char *machine, int32_t base, int32_t rows) {

    gpu3d_band_header_t *cfg = GPU3D_BAND_HEADER(machine);
    gpu_t *a = cfg->gpu;
    gpu3d_raster_t *state = &a->raster;

    uint8_t flag = (uint8_t)(state->disp3dcnt >> 8);

    if (!(flag & 0x40)) {

        uint32_t val_front = state->clear_color_rgb, val_bg = state->clear_depth_word;

        uint32_t n = (uint32_t)rows * RECON_3D_WIDTH;
        gpu3d_raster_word_fill((uint32_t *)machine, val_front, (int32_t)n);
        gpu3d_raster_word_fill((uint32_t *)(machine + RECON_3D_PLANE), val_bg, (int32_t)n);
        return;
    }

    gpu3d_t *b = cfg->gpu3d;
    uint16_t turn = b->clear_image_offset;
    unsigned char *t1 = a->vram.texture[2];
    unsigned char *t2 = a->vram.texture[3];
    uint32_t fixed_full = state->clear_depth_word;
    uint32_t fixed = fixed_full & 0x3f000000u;
    uint32_t turn_row = (uint32_t)(turn >> 8);


    uint32_t rows_u = (uint32_t)rows;

    if (t1 != NULL && t2 != NULL) {
        unsigned char *row_base = machine;
        for (uint32_t f = 0; f != rows_u; f++) {

            uint64_t hy = (uint64_t)(f + (uint32_t)base) * 2ull
                        + (uint64_t)recon_scale_3d * (uint64_t)turn_row;
            uint32_t o32 = (uint32_t)((hy / (2ull * (uint64_t)recon_scale_3d))
                                      & 0xffull) << 8;
            uint64_t o = (uint64_t)o32 << 1;
            unsigned char *p1 = t1 + o;
            unsigned char *p2 = t2 + o;
            uint32_t turn_run = turn;
            uint64_t off = 0;
            do {
                uint32_t k = (turn_run << 1) & 0x1fe;
                uint16_t v_raw, w;
                memcpy(&v_raw, p1 + k, 2);
                memcpy(&w, p2 + k, 2);
                uint32_t v = gpu3d_geometry_color_expand_bgr555_alpha_bit15((uint64_t)v_raw);

                uint32_t front = (((uint32_t)w << 16) & 0x80000000u) | v;
                uint32_t bg = (((uint32_t)w & 0x7fffu) << 9) | fixed;

                unsigned char *out = row_base + off;

                for (unsigned rep = 0; rep < recon_scale_3d; rep++) {
                    memcpy(out + (size_t)rep * 4u, &front, 4);
                    memcpy(out + RECON_3D_PLANE + (size_t)rep * 4u, &bg, 4);
                }

                turn_run += 1;
                off += 4u * recon_scale_3d;
            } while (off != RECON_ROW_STEP);
            row_base += RECON_ROW_STEP;
        }
        return;
    }

    if (t1 != NULL) {

        uint32_t bg_const = fixed | 0xfffe00u;
        unsigned char *row_base = machine;
        for (uint32_t f = 0; f != rows_u; f++) {

            uint64_t hy = (uint64_t)(f + (uint32_t)base) * 2ull
                        + (uint64_t)recon_scale_3d * (uint64_t)turn_row;
            uint32_t o32 = (uint32_t)((hy / (2ull * (uint64_t)recon_scale_3d))
                                      & 0xffull) << 8;
            unsigned char *p1 = t1 + ((uint64_t)o32 << 1);
            uint32_t turn_run = turn;
            uint64_t off = 0;
            do {
                uint32_t idx = turn_run & 0xff;
                uint16_t v_raw;
                memcpy(&v_raw, p1 + (size_t)idx * 2, 2);
                uint32_t v = gpu3d_geometry_color_expand_bgr555_alpha_bit15((uint64_t)v_raw);
                uint32_t front = v | 0x80000000u;

                unsigned char *out = row_base + off;

                for (unsigned rep = 0; rep < recon_scale_3d; rep++) {
                    memcpy(out + (size_t)rep * 4u, &front, 4);
                    memcpy(out + RECON_3D_PLANE + (size_t)rep * 4u, &bg_const, 4);
                }

                turn_run += 1;
                off += 4u * recon_scale_3d;
            } while (off != RECON_ROW_STEP);
            row_base += RECON_ROW_STEP;
        }
        return;
    }

    if (t2 != NULL) {

        unsigned char *row_base = machine;
        for (uint32_t f = 0; f != rows_u; f++) {

            uint64_t hy = (uint64_t)(f + (uint32_t)base) * 2ull
                        + (uint64_t)recon_scale_3d * (uint64_t)turn_row;
            uint32_t o32 = (uint32_t)((hy / (2ull * (uint64_t)recon_scale_3d))
                                      & 0xffull) << 8;
            unsigned char *p2 = t2 + ((uint64_t)o32 << 1);
            uint32_t turn_run = turn;
            uint64_t off = 0;
            do {
                uint32_t idx = turn_run & 0xff;
                uint16_t w;
                memcpy(&w, p2 + (size_t)idx * 2, 2);
                uint32_t front = ((uint32_t)w << 16) & 0x80000000u;
                uint32_t bg = (((uint32_t)w & 0x7fffu) << 9) | fixed;

                unsigned char *out = row_base + off;

                for (unsigned rep = 0; rep < recon_scale_3d; rep++) {
                    memcpy(out + (size_t)rep * 4u, &front, 4);
                    memcpy(out + RECON_3D_PLANE + (size_t)rep * 4u, &bg, 4);
                }

                turn_run += 1;
                off += 4u * recon_scale_3d;
            } while (off != RECON_ROW_STEP);
            row_base += RECON_ROW_STEP;
        }
        return;
    }

    {
        uint32_t front_const = 0x80000000u;
        uint32_t bg_const = fixed | 0xfffe00u;
        unsigned char *row_base = machine;
        for (uint32_t f = 0; f != rows_u; f++) {
            uint64_t off = 0;
            do {
                unsigned char *out = row_base + off;
                memcpy(out + 0, &front_const, 4);
                memcpy(out + 4, &front_const, 4);
                memcpy(out + 8, &front_const, 4);
                memcpy(out + 12, &front_const, 4);
                memcpy(out + RECON_3D_PLANE, &bg_const, 4);

                memcpy(out + RECON_3D_PLANE + 4, &bg_const, 4);
                memcpy(out + RECON_3D_PLANE + 8, &bg_const, 4);
                memcpy(out + RECON_3D_PLANE + 12, &bg_const, 4);
                off += 0x10;
            } while (off != RECON_ROW_STEP);
            row_base += RECON_ROW_STEP;
        }
    }
}


void gpu3d_raster_band_aux_buffers_clear_wide(unsigned char *param1, void *param2, int32_t param3) {
    (void)param2;

    if (param3 != 0) {
        unsigned char *p = GPU3D_BAND(param1)->plane2;
        do {
            gpu3d_raster_byte_fill(p, 0xff, 0x200);
            param3 -= 1;
            p += 0x200;
        } while (param3 != 0);
    }
}



static void copy_even_grouped(uint8_t *output, const uint8_t *entry)
{
    uint32_t block;

    for (block = 0; block != 31; ++block) {
        const uint8_t *p = entry + (size_t)block * 64u;
        uint32_t a0 = rd32(p + 0);
        uint32_t d1 = rd32(p + 4);
        uint32_t a2 = rd32(p + 8);
        uint32_t d3 = rd32(p + 12);
        uint32_t a4 = rd32(p + 16);
        uint32_t d5 = rd32(p + 20);
        uint32_t a6 = rd32(p + 24);
        uint32_t d7 = rd32(p + 28);
        uint32_t a8 = rd32(p + 32);
        uint32_t d9 = rd32(p + 36);
        uint32_t a10 = rd32(p + 40);
        uint32_t d11 = rd32(p + 44);
        uint32_t a12 = rd32(p + 48);
        uint32_t d13 = rd32(p + 52);
        uint32_t a14 = rd32(p + 56);
        uint32_t d15 = rd32(p + 60);
        uint8_t *d = output + (size_t)block * 32u;

        (void)d1; (void)d3; (void)d5; (void)d7;
        (void)d9; (void)d11; (void)d13; (void)d15;

        wr32(d + 0, a0 & UINT32_C(0x3fffffff));
        wr32(d + 4, a2 & UINT32_C(0x3fffffff));
        wr32(d + 8, a4 & UINT32_C(0x3fffffff));
        wr32(d + 12, a6 & UINT32_C(0x3fffffff));
        wr32(d + 16, a8 & UINT32_C(0x3fffffff));
        wr32(d + 20, a10 & UINT32_C(0x3fffffff));
        wr32(d + 24, a12 & UINT32_C(0x3fffffff));
        wr32(d + 28, a14 & UINT32_C(0x3fffffff));
    }

    for (block = 496; block < 512; block += 2) {
        wr32(output + 0x3e0u + (size_t)(block - 496) * 2u,
              rd32(entry + (size_t)block * 4u) & UINT32_C(0x3fffffff));
    }
}

static void copy_odd_grouped(uint8_t *output, const uint8_t *entry)
{
    uint32_t block;

    for (block = 0; block != 31; ++block) {
        const uint8_t *p = entry + (size_t)block * 64u;
        uint32_t a9 = rd32(p + 36);
        uint32_t d10 = rd32(p + 40);
        uint32_t a11 = rd32(p + 44);
        uint32_t d12 = rd32(p + 48);
        uint32_t a13 = rd32(p + 52);
        uint32_t d14 = rd32(p + 56);
        uint32_t a15 = rd32(p + 60);
        uint32_t d16 = rd32(p + 64);
        uint32_t a1 = rd32(p + 4);
        uint32_t d2 = rd32(p + 8);
        uint32_t a3 = rd32(p + 12);
        uint32_t d4 = rd32(p + 16);
        uint32_t a5 = rd32(p + 20);
        uint32_t d6 = rd32(p + 24);
        uint32_t a7 = rd32(p + 28);
        uint32_t d8 = rd32(p + 32);
        uint8_t *d = output + (size_t)block * 32u;

        (void)d10; (void)d12; (void)d14; (void)d16;
        (void)d2; (void)d4; (void)d6; (void)d8;

        wr32(d + 0, a1 & UINT32_C(0x3fffffff));
        wr32(d + 4, a3 & UINT32_C(0x3fffffff));
        wr32(d + 8, a5 & UINT32_C(0x3fffffff));
        wr32(d + 12, a7 & UINT32_C(0x3fffffff));
        wr32(d + 16, a9 & UINT32_C(0x3fffffff));
        wr32(d + 20, a11 & UINT32_C(0x3fffffff));
        wr32(d + 24, a13 & UINT32_C(0x3fffffff));
        wr32(d + 28, a15 & UINT32_C(0x3fffffff));
    }

    for (block = 497; block < 512; block += 2) {
        wr32(output + 0x3e0u + (size_t)(block - 497) * 2u,
              rd32(entry + (size_t)block * 4u) & UINT32_C(0x3fffffff));
    }
}

uint8_t *gpu3d_raster_band_page_deinterleave(uint8_t *output, const uint8_t *entry)
{

    uint32_t page;

    for (page = 0; page != 32; ++page) {
        const uint8_t *origin = entry + (size_t)page * 0x800u;
        uint8_t *last_pair;
        uint32_t idx;

        if ((uintptr_t)output >= (uintptr_t)origin + 0x7fcu ||
            (uintptr_t)origin >= (uintptr_t)output + 0x400u) {
            copy_even_grouped(output, origin);
            output += 0x400u;
        } else {
            for (idx = 0; idx < 512; idx += 2) {
                uint32_t v = rd32(origin + (size_t)idx * 4u);
                wr32(output, v & UINT32_C(0x3fffffff));
                output += 4;
            }
        }

        last_pair = output - 4;

        if ((uintptr_t)output >= (uintptr_t)origin + 0x800u ||
            (uintptr_t)(origin + 4) >= (uintptr_t)last_pair + 0x404u) {
            copy_odd_grouped(output, origin);
            output += 0x400u;
        } else {

            for (idx = 1; idx < 512; idx += 2) {
                uint32_t v = rd32(origin + (size_t)idx * 4u);
                wr32(output, v & UINT32_C(0x3fffffff));
                output += 4;
            }
        }
    }

    return output;
}


typedef void *(*fnp_memcpy)(void *, const void *, size_t);


void gpu3d_raster_bands_prepare_blocks(void *param_1, void *param_2, uint32_t param_3)
{

    uint8_t *ctx = (uint8_t *)param_1;
    uint8_t *output = (uint8_t *)param_2;
    gpu3d_band_header_t *hdr = GPU3D_BAND_HEADER(ctx);
    gpu_t *table;
    gpu3d_t *palette_base;
    const uint8_t *table_transform;
    const uint8_t *palette;
    const uint8_t *adjust_height;
    uint32_t edge;
    uint32_t param;
    uint32_t field;
    uint32_t config;
    uint8_t tmp[8u * 256u] __attribute__((aligned(16)));
    static fnp_memcpy copy;

    if (hdr->band_dirty == 0) {
        gpu3d_raster_band_compose_scaled_plain(param_1, param_2, param_3);
        return;
    }

    table = hdr->gpu;
    if (table->raster.frame_ready == 0) {
        gpu3d_raster_band_compose_scaled_plain(param_1, param_2, param_3);
        return;
    }

    palette_base = hdr->gpu3d;
    config = table->raster.disp3dcnt;
    edge = table->raster.clear_depth_word;
    table_transform = (unsigned char *)palette_base->fog_table;
    palette = (unsigned char *)palette_base->edge_color_expanded[0];
    adjust_height = (unsigned char *)&palette_base->fog_color;

    field = (config >> 8) & 0x0fu;
    param = field | (((0x400u >> field)
                 + (uint32_t)(palette_base->fog_offset & 0x7fffu)) << 16);

    if (param_3 != 0) {
        uint32_t prev = param_3 - 1u;
        unsigned char *base = recon_tables_base((unsigned char *)table);

        uint64_t step_a = (uint64_t)(uint32_t)(prev * RECON_ROW_STEP) << 2;
        uint64_t step_b = (uint64_t)(uint32_t)(prev * (RECON_ROW_STEP / 2u)) << 2;

        if (!copy)
            copy = (fnp_memcpy)sym_libc_memcpy;
        copy(base + step_a + RECON_ARENA_A, ctx + RECON_3D_PLANE, 2u * RECON_ROW_STEP);
        copy(base + step_b + RECON_ARENA_B, ctx, RECON_ROW_STEP);
    } else {
        gpu3d_raster_fog_density_line_512((const unsigned int *)(ctx + RECON_3D_PLANE),
                                      tmp, table_transform, param);

        gpu3d_raster_fog_blend_alpha_line(ctx, ctx, tmp,
                                      rd32(adjust_height));
        gpu3d_raster_test_neighbor_edge_bordered_x2(tmp, ctx + RECON_3D_PLANE,
                                      ctx + RECON_3D_PLANE + RECON_ROW_STEP, edge);
        gpu3d_raster_palette_blit_deinterleave_512(output, ctx, tmp, palette);
    }

    for (uint32_t offset = 0; offset != RECON_3D_SPAN;
         offset += RECON_ROW_STEP) {
        gpu3d_raster_fog_density_line_512((const unsigned int *)(ctx + offset + RECON_3D_PLANE + RECON_ROW_STEP),
                                      tmp, table_transform, param);

        gpu3d_raster_fog_blend_alpha_line(ctx + offset + RECON_ROW_STEP,
                                      ctx + offset + RECON_ROW_STEP, tmp,
                                      rd32(adjust_height));
        gpu3d_raster_test_neighbor_edge_dual_x2(tmp, ctx + offset + RECON_3D_PLANE,
                                      ctx + offset + RECON_3D_PLANE + RECON_ROW_STEP,
                                      ctx + offset + RECON_3D_PLANE + 2u * RECON_ROW_STEP, edge);
        gpu3d_raster_palette_blit_deinterleave_512(output + offset + RECON_ROW_STEP,
                                      ctx + offset + RECON_ROW_STEP, tmp, palette);
    }

    if (param_3 == RECON_BANDS - 1u) {
        gpu3d_raster_fog_density_line_512((const unsigned int *)(ctx + RECON_3D_PLANE + RECON_ROW_STEP * RECON_WRAP_ROWS),
                                      tmp, table_transform, param);

        gpu3d_raster_fog_blend_alpha_line(ctx + RECON_LAST_ROW, ctx + RECON_LAST_ROW,
                                      tmp, rd32(adjust_height));
        gpu3d_raster_test_neighbor_edge_bordered_x2_alt(tmp, ctx + RECON_3D_PLANE + RECON_ROW_STEP * RECON_WRAP_ROWS,

                                      ctx + RECON_3D_PLANE + RECON_LAST_ROW, edge);
        gpu3d_raster_palette_blit_deinterleave_512(output + RECON_LAST_ROW, ctx + RECON_LAST_ROW,
                                      tmp, palette);
    } else {
        unsigned char *base = recon_tables_base((unsigned char *)table);
        uint64_t step_a = (uint64_t)(uint32_t)(param_3 * RECON_ROW_STEP) << 2;
        uint64_t step_b = (uint64_t)(uint32_t)(param_3 * (RECON_ROW_STEP / 2u)) << 2;

        if (!copy)
            copy = (fnp_memcpy)sym_libc_memcpy;
        copy(base + step_a + RECON_ARENA_A0, ctx + RECON_3D_PLANE + RECON_ROW_STEP * RECON_WRAP_ROWS, 2u * RECON_ROW_STEP);
        copy(base + step_b + RECON_ARENA_B0, ctx + RECON_LAST_ROW, RECON_ROW_STEP);
    }
}


void gpu3d_raster_band_pair_compose(gpu_t *machine)
{

    unsigned char *tb = recon_tables_base((unsigned char *)machine);

    gpu3d_t *ctx = GPU3D_BAND_HEADER(recon_ctx3d_base((unsigned char *)machine))->gpu3d;

    uint8_t *layer = tb + RECON_ARENA_B0;
    uint8_t *block0 = tb + RECON_ARENA_A0;
    uint8_t *block1 = block0 + RECON_ROW_STEP;
    uint8_t *block2 = block0 + 2u * RECON_ROW_STEP;
    uint8_t *block3 = block0 + 3u * RECON_ROW_STEP;
    gpu3d_raster_t *cfg = &machine->raster;

    uint32_t field = cfg->disp3dcnt;
    uint16_t height_base = ctx->fog_offset;
    uint32_t width = cfg->clear_depth_word;
    uint8_t *output = cfg->frame_front + RECON_3D_BAND;

    uint32_t shift = (field >> 8) & 15u;
    uint32_t height = (uint32_t)(height_base & 0x7fffu) + (0x400u >> shift);
    uint32_t param = shift | (height << 16);

    _Alignas(16) uint8_t scratch[8u * 256u];

    const uint64_t step_slot = 4u * RECON_ROW_STEP;
    for (uint64_t d = 0; d != (RECON_BANDS - 1u) * step_slot; d += step_slot) {
        uint8_t *b0 = block0 + d;
        uint8_t *b1 = block1 + d;
        uint8_t *b2 = block2 + d;
        uint8_t *b3 = block3 + d;
        uint8_t *before = output - RECON_ROW_STEP;

        gpu3d_raster_fog_density_line_512((const unsigned int *)(b1), scratch,
                                 (unsigned char *)ctx->fog_table, param);
        gpu3d_raster_fog_blend_alpha_line(layer, layer, scratch, ctx->fog_color);
        gpu3d_raster_test_neighbor_edge_dual_x2(scratch, b0, b1, b2, width);
        gpu3d_raster_palette_blit_deinterleave_512(before, layer, scratch, (unsigned char *)ctx->edge_color_expanded[0]);

        gpu3d_raster_fog_density_line_512((const unsigned int *)(b2), scratch,
                                 (unsigned char *)ctx->fog_table, param);
        gpu3d_raster_fog_blend_alpha_line(layer + RECON_ROW_STEP, layer + RECON_ROW_STEP, scratch,
                                  ctx->fog_color);
        gpu3d_raster_test_neighbor_edge_dual_x2(scratch, b1, b2, b3, width);
        gpu3d_raster_palette_blit_deinterleave_512(output, layer + RECON_ROW_STEP, scratch,
                                 (unsigned char *)ctx->edge_color_expanded[0]);

        output += RECON_3D_BAND;
        layer += 2u * RECON_ROW_STEP;
    }
}




static unsigned pair_can_fog(gpu_t *table, gpu3d_t *state, uint32_t side, uint32_t idx)
{
    for (unsigned m = 0; m < 2u; m++) {
        uint32_t b = (idx & ~1u) + m;
        for (unsigned l = 0; l < 2u; l++) {
            if (l == 1u && state->translucent[side].count == 0u) break;
            gpu3d_band_bucket_t *hdr = recon_buckets((uint8_t *)table, l) + b;
            uint32_t n = hdr->count;
            gpu3d_polygon_list_t *base = l ? &state->translucent[side] : &state->opaque[side];
            for (uint32_t e = 0; e < n; e++) {
                uint32_t entry = rd16(hdr->list + (uintptr_t)e * 2u);
                if (base->polygon[entry].polygon_attr & 0x8000u) return 1u;
            }
        }
    }
    return 0u;
}

void gpu3d_raster_band_pending_process(void *param_1)
{

    uint8_t *ctx = (uint8_t *)param_1;
    gpu3d_band_header_t *hdr = GPU3D_BAND_HEADER(ctx);
    uint8_t width = hdr->threads;

    if (width > RECON_BANDS)
        return;

    gpu_t *table = hdr->gpu;
    gpu3d_t *state = hdr->gpu3d;
    uint32_t side = (uint32_t)(state->bank ^ 1u);
    gpu3d_bank_vertex_t *params_list = state->vertex_bank[side].vertex;
    uint32_t batches = width == 0u ? 0u : RECON_BANDS / (uint32_t)width;

    _Alignas(16) uint8_t tmp[8u * 256u];

    const int dynamic = recon_dynamic_share() && width > 1u;
    uint32_t turn = 0u;
    do {
        uint8_t width_current = hdr->threads;
        uint32_t idx;
        if (dynamic) {
            idx = recon_band_take();
            if (idx >= RECON_BANDS) break;
        } else {
            idx = turn * (uint32_t)width_current +
                     (uint32_t)hdr->index;
        }

        uint32_t tail_b[2] = { idx, 0u }; unsigned tail_force[2] = { 0xffffu, 0xffffu }; int tail_n = 1;
        for (int cq = 0; cq < tail_n; cq++) {
        idx = tail_b[cq];
        recon_band_current = (dynamic && RECON_BANDS == 24u) ? idx : 0xffffu;
        uint32_t start = idx * RECON_BAND_ROWS;
        uint32_t end = start + RECON_BAND_ROWS;
        recon_bands_n++;
        recon_bands_map |= 1u << (idx & 31u);

        gpu3d_raster_band_attr_tables_build_wide(ctx, start, RECON_BAND_ROWS);

        hdr->band_mark = 0xffffffffu;
        hdr->band_dirty = 0;

        if (recon_scale_3d > 2u)
            memset(ctx + RECON_3D_PLANE2 + RECON_PLANE2_USED, 0xff,
                   (size_t)RECON_BAND_ROWS / 8u);

        uint32_t *header_a = &recon_buckets((uint8_t *)table, 0)[idx].count;
        if (*header_a != 0u) {
            uint32_t element = 0u;
            do {
                uint8_t *list = recon_buckets((uint8_t *)table, 0)[idx].list;
                uint16_t entry = rd16(list + (uintptr_t)element * 2u);

                gpu3d_polygon_t *dest = &state->opaque[side].polygon[entry];
                gpu3d_raster_span_edge_ramp_dispatch_hires(ctx, dest,
                                               params_list, start, end);
                recon_poly_band++;
                ++element;
            } while (element < *header_a);
        }

        uint32_t *active_b = &state->translucent[side].count;
        if (*active_b != 0u) {

            for (uint32_t offset = 0u;
                 offset != RECON_PLANE2_USED;
                 offset += RECON_3D_WIDTH) {
                gpu3d_raster_byte_fill(ctx + RECON_3D_PLANE2 + offset,
                                          0xff, RECON_3D_WIDTH);
            }

            uint32_t *header_b = &recon_buckets((uint8_t *)table, 1)[idx].count;
            if (*header_b != 0u) {
                uint32_t element = 0u;
                do {
                    uint8_t *list = recon_buckets((uint8_t *)table, 1)[idx].list;
                    uint16_t entry = rd16(list + (uintptr_t)element * 2u);
                    gpu3d_polygon_t *dest = &state->translucent[side].polygon[entry];
                    gpu3d_raster_span_edge_ramp_dispatch_hires(ctx, dest,
                                                   params_list, start, end);
                    recon_poly_band++;
                    ++element;
                } while (element < *header_b);
            }
        }

        if (dynamic && RECON_BANDS == 24u && pair_can_fog(table, state, side, idx)) {
            unsigned dirty = hdr->band_dirty != 0u;
            if (tail_force[cq] != 0xffffu) {

                if (tail_force[cq]) hdr->band_dirty = 1u;
            } else {
                recon_band_publish(idx, dirty);
                if (!dirty && cq == 0 && tail_n == 1 && !(idx & 1u) && recon_band_steal(idx + 1u)) {
                    tail_b[0] = idx + 1u; tail_b[1] = idx; tail_force[1] = 0u; tail_n = 2;
                    cq = -1;
                    continue;
                }
                if (!dirty && recon_band_sibling_dirty(idx))
                    hdr->band_dirty = 1u;
            }

        }
        uint32_t config = table->raster.disp3dcnt;
        uint32_t selector = hdr->pass == 0u
            ? ((config >> 3) & 4u) : 0u;
        selector = (selector & ~3u) | ((config >> 6) & 3u);

        if (recon_scale_3d > 2u &&
            table->raster.frame_front == 0)
            table->raster.frame_front =
                recon_buf3d_front((unsigned char *)table);
        uint8_t *page = (uint8_t *)(table->raster.frame_front +
                                       (uintptr_t)idx * RECON_3D_BAND);

        switch (selector - 2u) {
        case 0u:
        case 1u: {
            gpu_t *table_mode = hdr->gpu;
            if (hdr->band_dirty == 0u ||
                table_mode->raster.frame_ready == 0u) {
                gpu3d_raster_deinterleave_words_blocks(page, ctx);
                break;
            }

            gpu3d_t *state_mode = hdr->gpu3d;
            uint32_t config_mode =
                table_mode->raster.disp3dcnt;
            uint32_t low = (config_mode >> 8) & 0xfu;
            uint32_t height = (0x400u >> (low & 31u)) +
                             ((uint32_t)state_mode->fog_offset &
                              0x7fffu);
            uint32_t param = low | (height << 16);
            uint8_t *counter = (unsigned char *)&state_mode->fog_color;
            uint8_t *palette = (unsigned char *)state_mode->fog_table;

            for (uint32_t offset = 0u;
                 offset != (uint32_t)RECON_3D_BAND;
                 offset += RECON_ROW_STEP) {
                gpu3d_raster_fog_density_line_512((const unsigned int *)(ctx + RECON_3D_PLANE + offset),
                                        tmp, palette, param);
                uint32_t mix = rd32(counter);
                if (selector - 2u == 0u) {
                    gpu3d_raster_fog_blend_color_split_parity(page + offset,
                                            ctx + offset, tmp, mix);
                } else {
                    gpu3d_raster_fog_blend_alpha_split_parity(page + offset,
                                            ctx + offset, tmp, mix);
                }
            }
            break;
        }

        case 2u:
        case 3u:
            gpu3d_raster_band_compose_scaled_plain(ctx, page, idx);
            break;

        case 4u:
            gpu3d_raster_band_compose_scaled_fogged(ctx, page, idx);
            break;

        case 5u:
            gpu3d_raster_bands_prepare_blocks(ctx, page, idx);
            break;

        default:
            gpu3d_raster_deinterleave_words_blocks(page, ctx);
            break;
        }

        if (tail_n == 2 && cq == 0)
            tail_force[1] = hdr->band_dirty != 0u;
        }
        ++turn;
    } while (dynamic ? 1 : (turn < batches));
}

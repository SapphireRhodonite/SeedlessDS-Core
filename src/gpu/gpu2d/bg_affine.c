#include "hires_runtime.h"

void gpu2d_bg_affine_gen_dual_ramp_packed(uint8_t *output, int base1, int base2,
                                          int inc1, int inc2, int n);

static const uint32_t gpu2d_bg_affine_blend_masks[4] = { 0u, 7u, 56u, 63u };
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <stdint.h>
#include <string.h>
#include "core_internals.h"
#include "mem_access.h"








static int32_t asr11(uint32_t v)
{
    return (int32_t)(v >> 11) - ((v & 0x80000000u) ? 0x200000 : 0);
}

static void load_tile(const uint8_t *map, uint32_t base_map,
                        uint32_t row, uint32_t col, uint32_t jump,
                        uint32_t base_tile, const uint8_t *base_palette,
                        const uint8_t **tile, const uint8_t **palette,
                        uint32_t *mix)
{
    uint32_t idx = ((row << (jump & 31u)) + col) * 2u + base_map;
    uint16_t ent = rd16(map + (int32_t)idx);

    *mix = gpu2d_bg_affine_blend_masks[((uint32_t)ent >> 10) & 3u];
    *tile = map + (int32_t)(base_tile + (((uint32_t)ent & 0x3ffu) << 6));
    *palette = base_palette + (((uint32_t)ent >> 3) & 0x1e00u);
}

static void pack_mask(uint8_t *mask, const uint8_t *texels,
                              int32_t first, int32_t last)
{
    int32_t b;

    if (first > last)
        return;
    for (b = first >> 3; b <= (last >> 3); b++) {
        uint8_t bits = 0;
        int32_t p;

        for (p = 0; p < 8; p++) {
            int32_t i = b * 8 + p;
            if (i >= first && i <= last && texels[i] != 0)
                bits |= (uint8_t)(1u << p);
        }
        wr8(mask + b, bits);
    }
}

static void paint_span(const uint8_t *map, uint8_t *dest,
                        uint8_t *mask, uint32_t base_map,
                        uint32_t base_tile, uint32_t jump,
                        const uint8_t *base_palette, uint32_t step_x,
                        uint32_t step_y, uint32_t coord_x,
                        uint32_t coord_y, int32_t first, int32_t last,
                        uint32_t wrap, uint32_t map_mask)
{
    uint8_t texels[256];
    uint32_t row, col, mix;
    const uint8_t *tile;
    const uint8_t *palette;
    int32_t i;

    if (wrap) {
        row = (coord_y >> 11) & map_mask;
        col = (coord_x >> 11) & map_mask;
    } else {
        row = (uint32_t)asr11(coord_y);
        col = (uint32_t)asr11(coord_x);
    }
    load_tile(map, base_map, row, col, jump, base_tile, base_palette,
               &tile, &palette, &mix);
    coord_x &= 0x7ffu;
    coord_y &= 0x7ffu;

    for (i = first; i <= last; i++) {
        uint32_t texel_index;
        uint8_t texel;

        if ((coord_x | coord_y) >= 0x800u) {
            if (wrap) {
                row = (row + (coord_y >> 11)) & map_mask;
                col = (col + (coord_x >> 11)) & map_mask;
            } else {
                row += (uint32_t)asr11(coord_y);
                col += (uint32_t)asr11(coord_x);
            }
            load_tile(map, base_map, row, col, jump, base_tile,
                       base_palette, &tile, &palette, &mix);
            coord_x &= 0x7ffu;
            coord_y &= 0x7ffu;
        }

        texel_index = ((coord_x >> 8) + ((coord_y >> 8) << 3)) ^ mix;
        texel = rd8(tile + texel_index);
        wr16(dest + (uint32_t)i * 2u, rd16(palette + (uint32_t)texel * 2u));
        texels[i] = rd8(tile + texel_index);
        coord_x += step_x;
        coord_y += step_y;
    }
    pack_mask(mask, texels, first, last);
}

void gpu2d_bg_affine_decode_affine_8bpp_line(void *param_1, void *param_2, void *param_3)
{
    gpu2d_bg_t *ctx = (gpu2d_bg_t *)param_1;
    uint8_t *dest = (uint8_t *)param_2;
    uint8_t *mask = (uint8_t *)param_3;
    uint32_t step_x, step_y, base_map, base_tile, jump;
    uint16_t control;
    uint8_t uses_palette_24;
    const uint8_t *map;
    const uint8_t *palette;

    if (ctx->affine_dirty != 0) {
        uint32_t limit = 0x7ffu | ((uint32_t)ctx->tiles_per_row_minus_one << 11);
        int32_t a, b;

        gpu2d_bg_text_compute_span_range(ctx->current_x, (int16_t)ctx->pa, limit,
                              (int16_t)ctx->pb, &ctx->span_x[0], &ctx->span_x[2], &ctx->span_x[1]);
        gpu2d_bg_text_compute_span_range(ctx->current_y, (int16_t)ctx->pc, limit,
                              (int16_t)ctx->pd, &ctx->span_y[0], &ctx->span_y[2], &ctx->span_y[1]);
        a = (int16_t)ctx->pa;
        b = (int16_t)ctx->pc;
        if (a != 0) {
            uint32_t v = (uint32_t)(a < 0 ? -(uint32_t)a : (uint32_t)a);
            uint32_t r = (v + GPU2D_RECIPROCAL_NUMERATOR) / v;
            ctx->pa_reciprocal = r;
        }
        if (b != 0) {
            uint32_t v = (uint32_t)(b < 0 ? -(uint32_t)b : (uint32_t)b);
            uint32_t r = (v + GPU2D_RECIPROCAL_NUMERATOR) / v;
            ctx->pc_reciprocal = r;
        }
        ctx->affine_dirty = 0;
    }

    step_x = (uint32_t)(int32_t)(int16_t)ctx->pa;
    step_y = (uint32_t)(int32_t)(int16_t)ctx->pc;
    base_map = ctx->screen_base;
    base_tile = ctx->char_base;
    jump = ctx->size_code;
    control = ctx->bgcnt;
    uses_palette_24 = ctx->ext_palette_enabled;
    map = ctx->vram_window;

    if ((control & 0x2000u) != 0) {
        if (uses_palette_24 != 0) {
            palette = ctx->ext_palette;
            if (palette == NULL)
                return;
        } else {
            palette = ctx->palette;
        }
        paint_span(map, dest, mask, base_map, base_tile, jump, palette,
                    step_x, step_y, ctx->current_x, ctx->current_y, 0, 255,
                    1, ctx->tiles_per_row_minus_one);
        return;
    }

    if (uses_palette_24 != 0) {
        palette = ctx->ext_palette;
        if (palette == NULL)
            return;
    } else {
        palette = ctx->palette;
    }

    {
        uint64_t a = ctx->span_x[2];
        uint64_t b = ctx->span_y[0];
        uint64_t c = ctx->span_x[0];
        uint64_t d = ctx->span_x[1] + c;
        uint64_t e = ctx->span_y[1] + b;
        uint64_t updated_c = a + c;
        uint64_t updated_b = ctx->span_y[2] + b;
        int32_t start = (int32_t)(b >> 32);
        int32_t other_start = (int32_t)(c >> 32);
        int32_t end = (int32_t)(e >> 32);
        int32_t other_end = (int32_t)(d >> 32);
        int32_t first, last;
        uint32_t coord_x, coord_y;
        uint32_t z = 0;

        ctx->span_y[0] = updated_b;
        ctx->span_x[0] = updated_c;
        first = start > other_start ? start : other_start;
        if (first < 0)
            first = 0;
        last = end < other_end ? end : other_end;
        memcpy(mask, &z, sizeof(z));
        memcpy(mask + 4, &z, sizeof(z));
        memcpy(mask + 8, &z, sizeof(z));
        memcpy(mask + 12, &z, sizeof(z));
        memcpy(mask + 16, &z, sizeof(z));
        memcpy(mask + 20, &z, sizeof(z));
        memcpy(mask + 24, &z, sizeof(z));
        memcpy(mask + 28, &z, sizeof(z));
        if (first > 255 || last < 0)
            return;
        if (last > 255)
            last = 255;
        if (last < first)
            return;
        coord_y = ctx->current_y + (uint32_t)first * step_y;
        coord_x = ctx->current_x + (uint32_t)first * step_x;
        paint_span(map, dest, mask, base_map, base_tile, jump, palette,
                    step_x, step_y, coord_x, coord_y, first, last, 0, 0);
    }
}








static uint16_t sample(const uint8_t *table, int32_t rowbase, uint32_t idx) {
    uint32_t off = (uint32_t)rowbase + (idx << 1);
    return rd16(table + (int64_t)(int32_t)off);
}

static uint8_t pack8(const uint16_t src8[8]) {
    uint8_t b = 0;
    for (int k = 0; k < 8; k++)
        if (src8[k] & 0x8000u) b |= (uint8_t)(1u << k);
    return b;
}

void gpu2d_bg_affine_sample_bitmap_affine_line(gpu2d_bg_t *p1, uint8_t *dst, uint8_t *info, uint32_t line)
{


    int16_t  PA      = p1->pa;
    uint16_t PB_u     = p1->pc;
    uint32_t X0       = p1->current_x;
    uint32_t Y0       = p1->current_y;
    uint16_t ctrl     = p1->bgcnt;
    uint16_t maskX    = p1->mask_x;
    uint16_t maskY    = p1->mask_y;
    uint8_t  turn    = p1->width_shift;
    int32_t  rowbase  = (int32_t)p1->affine_screen_offset;
    const uint8_t *table = p1->vram_window;
    int16_t  dy       = (int16_t)PB_u;

    if (PA == 0x100 && PB_u == 0) {
        if (ctrl & 0x2000) {

            uint16_t tmp[256];
            uint32_t idxbase = ((uint32_t)maskY & (Y0 >> 8)) << turn;
            uint32_t idx = X0 >> 8;
            for (int t = 0; t < 256; t++) {
                idx &= maskX;
                uint16_t v = sample(table, rowbase, idxbase + idx);
                tmp[t] = v;
                wr16(dst + t * 2, v);
                idx += 1;
            }
            for (int j = 0; j < 32; j++) info[j] = pack8(&tmp[8 * j]);
            return;
        }

        memset(info, 0, 32);
        if ((int32_t)Y0 < 0) return;
        int32_t row = (int32_t)Y0 >> 8;
        if ((uint32_t)row > maskY) return;
        int32_t Xs = (int32_t)X0 >> 8;

        int32_t start = ((int32_t)X0 > 0xff) ? 0 : -Xs;
        int32_t end = (int32_t)maskX - Xs;
        end = (end < 0xff) ? end : 0xff;
        if (start > end) return;
        uint32_t row_idx = (uint32_t)row << turn;
        for (int32_t c = start; c <= end; c++) {
            uint16_t v = sample(table, rowbase, row_idx + (uint32_t)(Xs + c));
            wr16(dst + c * 2, v);
            if (v & 0x8000u) info[c >> 3] |= (uint8_t)(1u << (c & 7));
        }
        return;
    }

    if (ctrl & 0x2000) {

        uint16_t tmp[256];
        uint32_t X = X0, Y = Y0;
        for (int t = 0; t < 256; t++) {
            uint32_t idxY = ((uint32_t)maskY & (Y >> 8)) << turn;
            uint32_t idxX = (X >> 8) & maskX;
            uint16_t v = sample(table, rowbase, idxX + idxY);
            tmp[t] = v;
            wr16(dst + t * 2, v);
            X += (uint32_t)PA;
            Y += (uint32_t)dy;
        }
        for (int j = 0; j < 32; j++) info[j] = pack8(&tmp[8 * j]);
        return;
    }

    if (p1->affine_dirty != 0) {
        gpu2d_bg_text_compute_span_range(
            (int32_t)X0, (int32_t)PA,
            (int32_t)(0xffu | ((uint32_t)maskX << 8)),
            (int32_t)p1->pb,
            &p1->span_x[0], &p1->span_x[2], &p1->span_x[1]);

        uint32_t Y0b = p1->current_y;
        gpu2d_bg_text_compute_span_range(
            (int32_t)Y0b, (int32_t)p1->pc,
            (int32_t)(0xffu | ((uint32_t)maskY << 8)),
            (int32_t)p1->pd,
            &p1->span_y[0], &p1->span_y[2], &p1->span_y[1]);

        p1->affine_dirty = 0;
    }

    int64_t c1_a = p1->span_x[0], c1_c = p1->span_x[1], c1_b = p1->span_x[2];
    int64_t c2_a = p1->span_y[0], c2_c = p1->span_y[1], c2_b = p1->span_y[2];

    uint64_t c1_bsum = (uint64_t)c1_b + (uint64_t)c1_a;
    uint64_t c1_csum = (uint64_t)c1_c + (uint64_t)c1_a;
    uint64_t c2_csum = (uint64_t)c2_c + (uint64_t)c2_a;
    uint64_t c2_bsum = (uint64_t)c2_b + (uint64_t)c2_a;

    p1->span_x[0] = (int64_t)c1_bsum;
    p1->span_y[0] = (int64_t)c2_bsum;

    int32_t hi_c1a = (int32_t)((uint64_t)c1_a >> 32);
    int32_t hi_c2a = (int32_t)((uint64_t)c2_a >> 32);
    int32_t hi_c1c = (int32_t)(c1_csum >> 32);
    int32_t hi_c2c = (int32_t)(c2_csum >> 32);

    int32_t start_hi = (hi_c2a > hi_c1a) ? hi_c2a : hi_c1a;
    int32_t end_hi    = (hi_c2c < hi_c1c) ? hi_c2c : hi_c1c;

    memset(info, 0, 32);
    if (start_hi > end_hi) return;

    int32_t start = (start_hi < 0) ? 0 : start_hi;
    if (start > 0xff) return;
    if (end_hi < 0) return;
    int32_t end = (end_hi < 0xff) ? end_hi : 0xff;
    if (start > end) return;

    uint32_t X = X0 + (uint32_t)(start * (int32_t)PA);
    uint32_t Y = Y0 + (uint32_t)(start * (int32_t)dy);
    for (int32_t c = start; c <= end; c++) {
        int32_t row = (int32_t)Y >> 8;
        uint32_t row_idx = (uint32_t)row << turn;
        int32_t col = (int32_t)X >> 8;
        uint16_t v = sample(table, rowbase, row_idx + (uint32_t)col);
        wr16(dst + c * 2, v);
        if (v & 0x8000u) info[c >> 3] |= (uint8_t)(1u << (c & 7));
        X += (uint32_t)PA;
        Y += (uint32_t)dy;
    }
}

typedef void *(*fn_memcpy)(void *, const void *, uint64_t);

uint32_t gpu2d_bg_affine_merge_texcoord_runs(uint8_t *param_1, uint8_t *param_2, uint8_t *param_3,
                             uint32_t param_4, uint32_t param_5)
{

    uint32_t w20 = param_5;

    if (param_4 == 0) {
        ((fn_memcpy)sym_libc_memcpy)(param_3, param_2, (uint64_t)param_5);
        return w20;
    }

    uint32_t w21 = param_4;
    uint8_t *x8 = param_1;

    if (w20 == 0) {
        ((fn_memcpy)sym_libc_memcpy)(param_3, x8, (uint64_t)w21);
        w20 = w21;
        return w20;
    }

    uint8_t *x22  = param_3;
    uint8_t *ptrA = param_2;
    uint32_t bA = *ptrA;
    uint32_t bB = *x8;

    if (bA > bB) goto hi_branch;

eq_check:
    if (bA == bB) goto dec_a;
    *x22++ = (uint8_t)bA;
dec_a:
    w20 -= 1;
    if (w20 == 0) goto a_exhausted;
    ptrA += 1;
    bA = *ptrA;
    if (bA <= bB) goto eq_check;
    goto hi_branch;

hi_branch:
    w21 -= 1;
    *x22++ = (uint8_t)bB;
    if (w21 == 0) goto b_exhausted;
    x8 += 1;
    bB = *x8;
    if (bA <= bB) goto eq_check;
    goto hi_branch;

a_exhausted:
    w20 = w21;
    ((fn_memcpy)sym_libc_memcpy)(x22, x8, (uint64_t)w20);
    goto done;

b_exhausted:
    ((fn_memcpy)sym_libc_memcpy)(x22, ptrA, (uint64_t)w20);

done:

    return (uint32_t)((x22 + w20) - param_3);
}

#define P_INCX     76
#define P_INCY     80
#define P_PA       158
#define P_PC       160
#define P_MASK     171
#define P_SHIFT     172
#define TABLE      0x120
#define IMPOSSIBLE  0x100
extern uint32_t gpu2d_bg_affine_merge_texcoord_runs_3(uint8_t *param_1, uint8_t *param_2,
                                   uint8_t *param_3, uint32_t param_4,
                                   uint32_t param_5) __asm__("gpu2d_bg_affine_merge_texcoord_runs");
extern void gpu2d_block_delta_encode_inplace(unsigned char *buf, uint32_t off, uint32_t mark);
extern void gpu2d_compose_pack_flag_mask_odd_bytes(unsigned char *dst, const unsigned char *src,
                               int32_t n);


static uint32_t axis_table(uint8_t *dst, int32_t step, uint32_t inc,
                          uint32_t org, uint32_t n) {
    int32_t end = (int32_t)((uint32_t)step * n + org) >> 11;
    int32_t start = (int32_t)org >> 11;
    uint32_t rest;
    uint32_t count;

    if (step < 0) {
        rest = (org & 0x7ffu) - (uint32_t)step;
        count = (uint32_t)(start - end);
    } else {
        rest = ((org & 0x7ffu) ^ 0x7ffu) + (uint32_t)step;
        count = (uint32_t)(end - start);
    }
    if (count == 0) return 0;

    uint32_t acc = (uint32_t)(((uint64_t)rest * (uint64_t)inc) >> 11);
    for (uint32_t i = 0; i < count; i++) {
        dst[i] = (uint8_t)(acc >> 20);
        acc += inc;
    }
    return count;
}

int32_t gpu2d_bg_affine_render_span_batched(uint8_t *ctx, uint8_t *spans, void *p3, void *p4,
                           void *p5, void *p6, uint32_t ox, uint32_t oy,
                           int32_t n)
{

    uint8_t raw[TABLE * 2 + 16];
    uint8_t *tx = raw;
    tx += (16 - ((uintptr_t)tx & 15)) & 15;
    uint8_t *ty = tx + TABLE;

    int32_t pa = rd16s(ctx + P_PA);
    int32_t pc = rd16s(ctx + P_PC);
    uint32_t incy = rd32(ctx + P_INCY);

    uint32_t n1 = 0, n2 = 0;
    if (pa != 0) n1 = axis_table(tx, pa, rd32(ctx + P_INCX), ox, (uint32_t)n);
    if (pc != 0) n2 = axis_table(ty, pc, incy, oy, (uint32_t)n);

    int32_t count = (int32_t)gpu2d_bg_affine_merge_texcoord_runs(tx, ty, spans, n1, n2);

    uint32_t spans_n = 0;
    if (count != 0) {

        const uint8_t *cursor = spans;
        uint8_t *write = spans;
        uint32_t prev = IMPOSSIBLE;
        do {
            uint32_t v = *cursor++;
            *write = (uint8_t)v;
            if (prev != v) { write++; spans_n++; }
            prev = v;
        } while (--count);
    }

    uint8_t mask = ctx[P_MASK];
    uint8_t shift = ctx[P_SHIFT];

    gpu2d_bg_affine_texcoord_pack_batched(spans, (unsigned char *)p3, (int32_t)spans_n,
                            (int32_t)ox, (int32_t)oy, pa, pc,
                            (int32_t)mask, (int32_t)shift);

    gpu2d_palette_lookup_line_idx16(p3, p4, spans_n);

    gpu2d_compose_pack_flag_mask_odd_bytes((unsigned char *)p6, (const unsigned char *)p3,
                       (int32_t)spans_n);

    gpu2d_block_delta_encode_inplace(spans, spans_n, (uint32_t)n);

    gpu2d_bg_affine_gen_dual_ramp_packed((uint8_t *)p5, (int)ox, (int)oy, pa, pc, (int)n);

    return (int32_t)(spans_n + 1u);
}
#undef P_INCX
#undef P_INCY
#undef P_PA
#undef P_PC
#undef P_MASK
#undef P_SHIFT
#undef TABLE
#undef IMPOSSIBLE

extern uint32_t gpu2d_bg_affine_merge_texcoord_runs_4(uint8_t *param_1, uint8_t *param_2,
                                    uint8_t *param_3, uint32_t param_4,
                                    uint32_t param_5) __asm__("gpu2d_bg_affine_merge_texcoord_runs");
extern void gpu2d_palette_translate_indices8_pack(uint16_t *buf, const unsigned char *table, int32_t n);
#define P_INCX     76
#define P_INCY     80
#define P_PA       158
#define P_PC       160
#define P_MASK     171
#define P_SHIFT     172
#define TABLE      0x120
#define IMPOSSIBLE  0x100

static uint32_t axis_table_4(uint8_t *dst, int32_t step, uint32_t inc,
                          uint32_t org, uint32_t n) {
    int32_t end = (int32_t)((uint32_t)step * n + org) >> 11;
    int32_t start = (int32_t)org >> 11;
    uint32_t rest;
    uint32_t count;

    if (step < 0) {
        rest = (org & 0x7ffu) - (uint32_t)step;
        count = (uint32_t)(start - end);
    } else {
        rest = ((org & 0x7ffu) ^ 0x7ffu) + (uint32_t)step;
        count = (uint32_t)(end - start);
    }
    if (count == 0) return 0;

    uint32_t acc = (uint32_t)(((uint64_t)rest * (uint64_t)inc) >> 11);
    for (uint32_t i = 0; i < count; i++) {
        dst[i] = (uint8_t)(acc >> 20);
        acc += inc;
    }
    return count;
}

int32_t gpu2d_bg_affine_render_span_scalar(uint8_t *ctx, uint8_t *spans, void *p3, void *p4,
                           void *p5, uint64_t dead, uint32_t ox, uint32_t oy,
                           uint32_t n)
{
    (void)dead;

    uint8_t raw[TABLE * 2 + 16];
    uint8_t *tx = raw;
    tx += (16 - ((uintptr_t)tx & 15)) & 15;
    uint8_t *ty = tx + TABLE;

    int32_t pa = (int16_t)rd16(ctx + P_PA);
    int32_t pc = (int16_t)rd16(ctx + P_PC);
    uint32_t incy = rd32(ctx + P_INCY);

    uint32_t n1 = 0, n2 = 0;
    if (pa != 0) n1 = axis_table_4(tx, pa, rd32(ctx + P_INCX), ox, n);
    if (pc != 0) n2 = axis_table_4(ty, pc, incy, oy, n);

    int32_t count = (int32_t)gpu2d_bg_affine_merge_texcoord_runs(tx, ty, spans, n1, n2);

    uint32_t spans_n = 0;
    if (count != 0) {

        const uint8_t *cursor = spans;
        uint8_t *write = spans;
        uint32_t prev = IMPOSSIBLE;
        do {
            uint32_t v = *cursor++;
            *write = (uint8_t)v;
            if (prev != v) { write++; spans_n++; }
            prev = v;
        } while (--count);
    }

    gpu2d_bg_affine_texcoord_pack_scalar(spans, p3, spans_n, ox, oy,
                           (uint32_t)pa, (uint32_t)pc,
                           ctx[P_MASK], ctx[P_SHIFT]);

    gpu2d_palette_translate_indices8_pack((uint16_t *)p3, (const unsigned char *)p4, (int32_t)spans_n);

    gpu2d_block_delta_encode_inplace(spans, spans_n, n);

    gpu2d_bg_affine_gen_dual_ramp_packed((uint8_t *)p5, (int)ox, (int)oy, (int)pa, (int)pc, (int)n);

    return (int32_t)(spans_n + 1u);
}
#undef P_INCX
#undef P_INCY
#undef P_PA
#undef P_PC
#undef P_MASK
#undef P_SHIFT
#undef TABLE
#undef IMPOSSIBLE


static uint32_t ushl32(uint32_t v, uint8_t offset)
{
    return offset >= 32 ? 0u : v << offset;
}

uint32_t gpu2d_bg_affine_gen_texcoord_flags(const gpu2d_bg_t *state, uint8_t *mask,
                             uint8_t *coords, const uint8_t *table,
                             uint8_t *ramp, uint8_t *output,
                             uint32_t origin_x, uint32_t origin_y)
{

    for (int32_t block = 7; block >= 0; block--) {
        uint8_t *p = mask + (uint32_t)block * 32u;
        wr64(p,      UINT64_C(0x0101010101010101));
        wr64(p + 8,  UINT64_C(0x0101010101010101));
        wr64(p + 16, UINT64_C(0x0101010101010101));
        wr64(p + 24, UINT64_C(0x0101010101010101));
    }

    int32_t pa = (int16_t)state->pa;
    int32_t pc = (int16_t)state->pc;
    uint8_t offset = state->size_code;
    uint8_t limit = state->tiles_per_row_minus_one;

    for (uint32_t block = 0; block < 32; block++) {
        for (uint32_t lane = 0; lane < 8; lane++) {
            uint32_t idx = block * 8u + lane;
            uint32_t x = origin_x + (uint32_t)pa * idx;
            uint32_t y = origin_y + (uint32_t)pc * idx;
            uint32_t column = (x >> 11) & limit;
            uint32_t row = (y >> 11) & limit;
            uint16_t coord =
                (uint16_t)(ushl32(row, offset) + column);

            wr16(coords + idx * 2u, (uint16_t)(coord << 1));
        }
    }

    pa = (int16_t)state->pa;
    pc = (int16_t)state->pc;
    gpu2d_bg_affine_gen_dual_ramp_packed(
        ramp, (int)origin_x, (int)origin_y, pa, pc, 256);
    gpu2d_palette_lookup_line_idx16(
        (uint16_t *)coords, table, 255);
    gpu2d_compose_pack_flag_mask_odd_bytes(
        output, coords, 255);

    return 256;
}


static uint32_t ushl32_6(uint32_t v, uint8_t offset)
{
    int8_t amt = (int8_t)offset;
    if (amt >= 0) {
        return amt >= 32 ? 0u : v << (unsigned)amt;
    }
    unsigned neg = (unsigned)(-(int)amt);
    return neg >= 32 ? 0u : v >> neg;
}

uint32_t gpu2d_bg_affine_gen_texindex_mask(const gpu2d_bg_t *state, uint8_t *mask,
                             uint8_t *coords, const uint8_t *table,
                             uint8_t *ramp, uint64_t dead,
                             uint32_t origin_x, uint32_t origin_y)
{

    for (uint32_t block = 0; block != 8; block++) {
        uint8_t *p = mask + block * 32u;
        wr64(p, UINT64_C(0x0101010101010101));
        wr64(p + 8, UINT64_C(0x0101010101010101));
        wr64(p + 16, UINT64_C(0x0101010101010101));
        wr64(p + 24, UINT64_C(0x0101010101010101));
    }

    int32_t pa = (int16_t)state->pa;
    int32_t pc = (int16_t)state->pc;
    uint8_t limit = state->tiles_per_row_minus_one;
    uint8_t offset = state->size_code;

    (void)dead;

    for (uint32_t vector = 0; vector != 32; vector++) {
        uint32_t base = vector * 8u;

        for (uint32_t lane = 0; lane != 4; lane++) {
            uint32_t idx = base + lane;
            uint32_t x = origin_x + (uint32_t)pa * idx;
            uint32_t y = origin_y + (uint32_t)pc * idx;
            uint32_t column = (x >> 11) & limit;
            uint32_t row = (y >> 11) & limit;
            wr16(coords + idx * 2u,
                  (uint16_t)(ushl32_6(row, offset) + column));
        }

        for (uint32_t lane = 4; lane != 8; lane++) {
            uint32_t idx = base + lane;
            uint32_t x = origin_x + (uint32_t)pa * idx;
            uint32_t y = origin_y + (uint32_t)pc * idx;
            uint32_t column = (x >> 11) & limit;
            uint32_t row = (y >> 11) & limit;
            wr16(coords + idx * 2u,
                  (uint16_t)(ushl32_6(row, offset) + column));
        }

    }

    pa = (int16_t)state->pa;
    pc = (int16_t)state->pc;
    gpu2d_bg_affine_gen_dual_ramp_packed(
        ramp, (int)origin_x, (int)origin_y, pa, pc, 256);
    gpu2d_palette_translate_indices8_pack(
        (uint16_t *)coords, table, 255);

    return 256;
}

#define P_MEM      8
#define P_EXTRA    16
#define P_OFF_A    56
#define P_OFF_B    60
#define ACC_X      88
#define ACC_Y      112
#define ORIG_X     144
#define ORIG_Y     148
#define CTL        152
#define P_PA       158
#define P_PC       160
#define MASK    171
#define SHIFT       172
#define PREV     174
#define WIDTH      256
#define TABLE      0x100
#define ONES       0x340
extern void    video_out_prepare_scale_axes(uint8_t *state);
extern void    gpu2d_bg_text_mask_bit_range256(uint32_t *map, unsigned from, unsigned until);
extern int32_t gpu2d_bg_affine_render_span_scalar_7(uint8_t *ctx, uint8_t *spans, void *p3, void *p4,
                                  void *p5, uint64_t dead, uint32_t ox, uint32_t oy,
                                  uint32_t n) __asm__("gpu2d_bg_affine_render_span_scalar");
extern void    gpu2d_planes_mask_from_bytes(uint8_t *mask, const uint8_t *pixels);
extern void    gpu2d_palette_lookup_line_idx8(uint32_t *dst, const unsigned char *src,
                                  const unsigned char *table, int32_t n);
extern void    gpu2d_palette_apply_lut64_lines(uint8_t *dest, const uint8_t *origin,
                                  const uint8_t *counts, const uint8_t *indices,
                                  int lines, const uint8_t *palettes);

static uint32_t ushl(uint32_t v, uint32_t amt) {
    int32_t k = (int8_t)(uint8_t)amt;
    if (k >= 0) return (k >= 32) ? 0u : (v << k);
    k = -k;
    return (k >= 32) ? 0u : (v >> k);
}

void gpu2d_bg_affine_draw_line(gpu2d_bg_t *bg, uint8_t *p2, uint8_t *p3, uint32_t line) {
    uint8_t *p1 = (uint8_t *)bg;

    uint8_t stack[0x600 + 16];
    uint8_t *b = stack;
    b += (16 - ((uintptr_t)b & 15)) & 15;
    uint8_t *tab  = b + TABLE;
    uint8_t *some = b + ONES;

    uint8_t *mem = rd_ptr_u8(p1 + P_MEM);
    const unsigned char *extra = rd_ptr_u8(p1 + P_EXTRA);
    uint8_t *m_a = mem + rd32(p1 + P_OFF_A);
    uint8_t *m_b = mem + rd32(p1 + P_OFF_B);
    uint32_t ctl = rd16(p1 + CTL);
    int32_t pa = (int16_t)rd16(p1 + P_PA);
    int32_t pc = (int16_t)rd16(p1 + P_PC);
    uint32_t ox = rd32(p1 + ORIG_X);
    uint32_t oy = rd32(p1 + ORIG_Y);

    if (p1[PREV]) video_out_prepare_scale_axes(p1);

    if (!(ctl & (1u << 13))) {

        uint64_t ax = rd64(p1 + ACC_X);
        uint64_t ay = rd64(p1 + ACC_Y);

        int32_t px = (int32_t)(uint32_t)(ax >> 32);
        int32_t py = (int32_t)(uint32_t)(ay >> 32);
        int32_t height = (py > px) ? py : px;

        int32_t fx = (int32_t)(uint32_t)((rd64(p1 + ACC_X + 8) + ax) >> 32);
        int32_t fy = (int32_t)(uint32_t)((rd64(p1 + ACC_Y + 8) + ay) >> 32);

        wr64(p1 + ACC_X, rd64(p1 + ACC_X + 16) + ax);
        int32_t low = (fy < fx) ? fy : fx;
        wr64(p1 + ACC_Y, rd64(p1 + ACC_Y + 16) + ay);

        if (low >= 0) {
            uint32_t start = (uint32_t)(height & ~(height >> 31));
            if ((int32_t)start < 0x100) {
                if (low > 0xfe) low = 0xff;
                int32_t n = low - (int32_t)start;
                if (n >= 0) {

                    uint32_t r = (uint32_t)gpu2d_bg_affine_render_span_scalar(
                        p1, some, tab, m_a, b, 0,
                        (uint32_t)((int32_t)start * pa) + ox,
                        (uint32_t)((int32_t)start * pc) + oy,
                        (uint32_t)n);
                    uint8_t *d = p2 + 0x100 + start;
                    gpu2d_palette_apply_lut64_lines(d, b, some, tab, (int)r, m_b);
                    gpu2d_planes_mask_from_bytes(p3, p2 + 0x100);
                    gpu2d_palette_lookup_line_idx8((uint32_t *)(p2 + start * 2), d,
                                       extra,
                                       (int32_t)((uint32_t)n + 1u));
                    gpu2d_bg_text_mask_bit_range256((uint32_t *)p3, start, (uint32_t)low);
                    return;
                }
            }
        }
        memset(p3, 0, 32);
        return;
    }

    uint32_t r;
    if ((uint32_t)(pa + 0x7ff) < 0xfffu
        && ((uint32_t)(pc + 0x7ff) & 0xffffu) < 0xfffu) {

        r = (uint32_t)gpu2d_bg_affine_render_span_scalar(p1, some, tab, m_a, b, 0, ox, oy, 0xff);
    } else {
        memset(some, 1, WIDTH);
        uint32_t mask = p1[MASK];
        uint32_t shift = p1[SHIFT];
        for (uint32_t i = 0; i < WIDTH; i++) {
            uint32_t xv = ox + (uint32_t)pa * i;
            uint32_t yv = oy + (uint32_t)pc * i;
            uint32_t col  = (xv >> 11) & mask;
            uint32_t row = (yv >> 11) & mask;
            wr16(tab + i * 2, (uint16_t)(ushl(row, shift) + col));
        }

        gpu2d_bg_affine_gen_dual_ramp_packed(b, (int)ox, (int)oy, pa, pc, 0x100);
        gpu2d_palette_translate_indices8_pack((uint16_t *)tab, m_a, 0xff);
        r = 0x100;
    }

    uint8_t *d = p2 + 0x100;

    gpu2d_palette_apply_lut64_lines(d, b, some, tab, (int)r, m_b);
    gpu2d_planes_mask_from_bytes(p3, d);
    gpu2d_palette_lookup_line_idx8((uint32_t *)p2, d, extra, 0x100);
}
#undef P_MEM
#undef P_EXTRA
#undef P_OFF_A
#undef P_OFF_B
#undef ACC_X
#undef ACC_Y
#undef ORIG_X
#undef ORIG_Y
#undef CTL
#undef P_PA
#undef P_PC
#undef MASK
#undef SHIFT
#undef PREV
#undef WIDTH
#undef TABLE
#undef ONES

#define P_MEM     8
#define P_EXTRA   16
#define P_PTR18   24
#define P_OFF_A   56
#define P_OFF_B   60
#define ACC_X     88
#define ACC_Y     112
#define ORIG_X    144
#define ORIG_Y    148
#define CTL       152
#define P_PA      158
#define P_PC      160
#define MASK   171
#define SHIFT      172
#define STYLE    173
#define PREV    174
#define TABLE     0x100
#define ONES      0x340
#define BUF6      0x460
#define WIDTH     256
extern int32_t gpu2d_bg_affine_render_span_batched_8(uint8_t *ctx, uint8_t *spans, void *p3, void *p4,
                                  void *p5, void *p6, uint32_t ox, uint32_t oy,
                                  int32_t n) __asm__("gpu2d_bg_affine_render_span_batched");
extern void    gpu2d_compose_decode_bg_row_8bpp_paletted(uint8_t *x0, uint8_t *x1, uint8_t *x2, uint8_t *x3,
                                  uint8_t *x4, int32_t x5, uint8_t *x6);
extern void    gpu2d_planes_mask_from_pixels_lowbyte(uint8_t *mask, const uint16_t *pixels);
extern void    gpu2d_palette_lookup_line_idx12(uint32_t *dst, const uint16_t *src,
                                  const unsigned char *table, int32_t n);

static uint32_t ushl_8(uint32_t v, uint32_t amt) {
    int32_t k = (int8_t)(uint8_t)amt;
    if (k >= 0) return (k >= 32) ? 0u : (v << k);
    k = -k;
    return (k >= 32) ? 0u : (v >> k);
}

static int clip(uint8_t *p1, int32_t *out_start, int32_t *out_low) {
    uint64_t ax = rd64(p1 + ACC_X);
    uint64_t ay = rd64(p1 + ACC_Y);

    int32_t px = (int32_t)(uint32_t)(ax >> 32);
    int32_t py = (int32_t)(uint32_t)(ay >> 32);
    int32_t height = (py > px) ? py : px;

    int32_t fx = (int32_t)(uint32_t)((rd64(p1 + ACC_X + 8) + ax) >> 32);
    int32_t fy = (int32_t)(uint32_t)((rd64(p1 + ACC_Y + 8) + ay) >> 32);

    wr64(p1 + ACC_X, rd64(p1 + ACC_X + 16) + ax);
    int32_t low = (fy < fx) ? fy : fx;
    wr64(p1 + ACC_Y, rd64(p1 + ACC_Y + 16) + ay);

    if (low < 0) return 0;
    uint32_t start = (uint32_t)(height & ~(height >> 31));
    if ((int32_t)start >= 0x100) return 0;
    if (low > 0xfe) low = 0xff;
    int32_t n = low - (int32_t)start;
    if (n < 0) return 0;

    *out_start = (int32_t)start;
    *out_low = low;
    return 1;
}

static void table_manual(uint8_t *tab, uint32_t ox, uint32_t oy, int32_t pa, int32_t pc,
                          uint32_t mask, uint32_t shift) {
    for (uint32_t i = 0; i < WIDTH; i++) {
        uint32_t xv = ox + (uint32_t)pa * i;
        uint32_t yv = oy + (uint32_t)pc * i;
        uint32_t col  = (xv >> 11) & mask;
        uint32_t row = (yv >> 11) & mask;
        uint16_t v = (uint16_t)(ushl_8(row, shift) + col);
        memcpy(tab + i * 2, &v, 2);
    }
}

void gpu2d_bg_affine_draw_line_dual_style(gpu2d_bg_t *bg, uint8_t *p2, uint8_t *p3, uint32_t line) {
    uint8_t *p1 = (uint8_t *)bg;

    uint8_t stack[0x600 + 16];
    uint8_t *b = stack;
    b += (16 - ((uintptr_t)b & 15)) & 15;
    uint8_t *tab  = b + TABLE;
    uint8_t *some = b + ONES;
    uint8_t *buf6 = b + BUF6;

    uint8_t *mem   = rd_ptr_u8(p1 + P_MEM);
    const unsigned char *extra = rd_ptr_u8(p1 + P_EXTRA);
    uint8_t *m_a = mem + rd32(p1 + P_OFF_A);
    uint8_t *m_b = mem + rd32(p1 + P_OFF_B);
    uint32_t ctl = rd16(p1 + CTL);
    int32_t  pa0 = rd16s(p1 + P_PA);
    int32_t  pc0 = rd16s(p1 + P_PC);
    uint32_t ox  = rd32(p1 + ORIG_X);
    uint32_t oy  = rd32(p1 + ORIG_Y);
    uint32_t style = p1[STYLE];

    if (p1[PREV]) video_out_prepare_scale_axes(p1);

    if (!(ctl & (1u << 13))) {

        if (style != 0) {

            uint8_t *ptr18 = rd_ptr_u8(p1 + P_PTR18);
            if (!ptr18) return;

            int32_t start, low;
            if (!clip(p1, &start, &low)) { memset(p3, 0, 32); return; }

            int32_t count = gpu2d_bg_affine_render_span_batched(
                p1, some, tab, m_a, b, buf6,
                (uint32_t)(ox + (uint32_t)start * pa0),
                (uint32_t)(oy + (uint32_t)start * pc0),
                low - start);

            uint8_t *dst = p2 + (uint32_t)start * 2;
            gpu2d_compose_decode_bg_row_8bpp_paletted(dst, b, some, tab, buf6, count, m_b);
            gpu2d_planes_mask_from_pixels_lowbyte(p3, (const uint16_t *)(void *)p2);
            gpu2d_palette_lookup_line_idx12((uint32_t *)(void *)dst, (const uint16_t *)(void *)dst,
                                    ptr18, (low - start) + 1);
            gpu2d_bg_text_mask_bit_range256((uint32_t *)(void *)p3, (unsigned)start, (unsigned)low);
        } else {

            int32_t start, low;
            if (!clip(p1, &start, &low)) { memset(p3, 0, 32); return; }

            int32_t count = gpu2d_bg_affine_render_span_batched(
                p1, some, tab, m_a, b, buf6,
                (uint32_t)(ox + (uint32_t)start * pa0),
                (uint32_t)(oy + (uint32_t)start * pc0),
                low - start);

            uint8_t *dst = p2 + 0x100 + start;
            gpu2d_compose_decode_bg_row_8bpp_indexed(dst, b, some, tab, buf6, (uint32_t)count, m_b);
            gpu2d_planes_mask_from_bytes(p3, p2 + 0x100);
            gpu2d_palette_lookup_line_idx8((uint32_t *)(void *)(p2 + (uint32_t)start * 2), dst,
                                    extra, (low - start) + 1);
            gpu2d_bg_text_mask_bit_range256((uint32_t *)(void *)p3, (unsigned)start, (unsigned)low);
        }
        return;
    }

    if (style != 0) {

        uint8_t *ptr18 = rd_ptr_u8(p1 + P_PTR18);
        if (!ptr18) return;

        int32_t pa = rd16s(p1 + P_PA);
        int32_t pc = rd16s(p1 + P_PC);
        uint32_t count;

        if ((uint32_t)(pa + 0x7ff) < 0xfffu && ((uint32_t)(pc + 0x7ff) & 0xffffu) < 0xfffu) {

            count = (uint32_t)gpu2d_bg_affine_render_span_batched(p1, some, tab, m_a, b, buf6, ox, oy, 0xff);
        } else {
            memset(some, 1, WIDTH);
            uint32_t mask = p1[MASK], shift = p1[SHIFT];
            table_manual(tab, ox, oy, pa, pc, mask, shift);
            gpu2d_bg_affine_gen_dual_ramp_packed(b, (int)ox, (int)oy, (int)pa, (int)pc, 0x100);
            gpu2d_palette_lookup_line_idx16((uint16_t *)(tab), ptr18, 0xff);
            gpu2d_compose_pack_flag_mask_odd_bytes(buf6, tab, 0xff);
            count = 0x100;
        }

        gpu2d_compose_decode_bg_row_8bpp_paletted(p2, b, some, tab, buf6, (int32_t)count, m_b);
        gpu2d_planes_mask_from_pixels_lowbyte(p3, (const uint16_t *)(void *)p2);
        gpu2d_palette_lookup_line_idx12((uint32_t *)(void *)p2, (const uint16_t *)(void *)p2, ptr18, 0x100);
    } else {

        int32_t pa = rd16s(p1 + P_PA);
        int32_t pc = rd16s(p1 + P_PC);
        uint32_t count;

        if ((uint32_t)(pa + 0x7ff) < 0xfffu && ((uint32_t)(pc + 0x7ff) & 0xffffu) < 0xfffu) {
            count = (uint32_t)gpu2d_bg_affine_render_span_batched(p1, some, tab, m_a, b, buf6, ox, oy, 0xff);
        } else {
            memset(some, 1, WIDTH);
            uint32_t mask = p1[MASK], shift = p1[SHIFT];
            table_manual(tab, ox, oy, pa, pc, mask, shift);
            gpu2d_bg_affine_gen_dual_ramp_packed(b, (int)ox, (int)oy, (int)pa, (int)pc, 0x100);
            gpu2d_palette_lookup_line_idx16((uint16_t *)(tab), buf6, 0xff);
            gpu2d_compose_pack_flag_mask_odd_bytes(buf6, tab, 0xff);
            count = 0x100;
        }

        uint8_t *dst = p2 + 0x100;
        gpu2d_compose_decode_bg_row_8bpp_indexed(dst, b, some, tab, buf6, count, m_b);
        gpu2d_planes_mask_from_bytes(p3, dst);
        gpu2d_palette_lookup_line_idx8((uint32_t *)(void *)p2, dst,
                                extra, 0x100);
    }

}
#undef P_MEM
#undef P_EXTRA
#undef P_PTR18
#undef P_OFF_A
#undef P_OFF_B
#undef ACC_X
#undef ACC_Y
#undef ORIG_X
#undef ORIG_Y
#undef CTL
#undef P_PA
#undef P_PC
#undef MASK
#undef SHIFT
#undef STYLE
#undef PREV
#undef TABLE
#undef ONES
#undef BUF6
#undef WIDTH

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"

void gpu2d_bg_affine_texcoord_pack_scalar(const uint8_t *texture, uint8_t *output, int count,
                        int base_u, int base_v, int step_u, int step_v,
                        int mask, int shift) {

    uint32_t hdr = (uint32_t)(((base_u >> 11) & mask)
                            + (((base_v >> 11) & mask) << shift));
    memcpy(output, &hdr, 4);
    output += 2;

    int16_t pu = (int16_t)step_u;
    int16_t pv = (int16_t)step_v;
    uint8_t mult = (uint8_t)(shift >= 8 ? 0 : (1u << shift));

#ifdef __ARM_NEON

    {
        const int16x8_t vpu = vdupq_n_s16(pu), vpv = vdupq_n_s16(pv);
        const int32x4_t vbu = vdupq_n_s32(base_u), vbv = vdupq_n_s32(base_v);
        const uint16x8_t vmu = vdupq_n_u16((uint16_t)mask);
        const uint8x16_t vmv = vdupq_n_u8((uint8_t)mask);
        const uint8x16_t vml = vdupq_n_u8(mult);
        const unsigned char *tex = texture;
        unsigned char *out = output;
        int32_t left = count;
        do {
            uint8x16_t t0 = vld1q_u8(tex), t1 = vld1q_u8(tex + 16);
            uint16x8_t g[4];
            g[0] = vmovl_u8(vget_low_u8(t0));  g[1] = vmovl_high_u8(t0);
            g[2] = vmovl_u8(vget_low_u8(t1));  g[3] = vmovl_high_u8(t1);
            uint16x8_t U[4], V[4];
            for (int k = 0; k < 4; k++) {
                int16x8_t gs = vreinterpretq_s16_u16(g[k]);
                int32x4_t ua = vmlal_s16(vbu, vget_low_s16(gs), vget_low_s16(vpu));
                int32x4_t ub = vmlal_high_s16(vbu, gs, vpu);
                uint16x8_t u = vshrn_high_n_u32(
                        vshrn_n_u32(vreinterpretq_u32_s32(ua), 8),
                        vreinterpretq_u32_s32(ub), 8);
                U[k] = vandq_u16(vshrq_n_u16(u, 3), vmu);
                int32x4_t va = vmlal_s16(vbv, vget_low_s16(gs), vget_low_s16(vpv));
                int32x4_t vb = vmlal_high_s16(vbv, gs, vpv);
                V[k] = vshrn_high_n_u32(
                        vshrn_n_u32(vreinterpretq_u32_s32(va), 8),
                        vreinterpretq_u32_s32(vb), 8);
            }
            uint8x16_t v01 = vandq_u8(vshrn_high_n_u16(vshrn_n_u16(V[0], 3), V[1], 3), vmv);
            uint8x16_t v23 = vandq_u8(vshrn_high_n_u16(vshrn_n_u16(V[2], 3), V[3], 3), vmv);
            U[0] = vmlal_u8(U[0], vget_low_u8(v01), vget_low_u8(vml));
            U[1] = vmlal_high_u8(U[1], v01, vml);
            U[2] = vmlal_u8(U[2], vget_low_u8(v23), vget_low_u8(vml));
            U[3] = vmlal_high_u8(U[3], v23, vml);
            for (int k = 0; k < 4; k++)
                vst1q_u16((uint16_t *)(void *)(out + k * 16), U[k]);
            tex += 32; out += 64; left -= 32;
        } while (left > 0);
        return;
    }
#else

    int turns = (count + 31) / 32;
    if (turns < 1) turns = 1;
    int n = turns * 32;

    for (int p = 0; p < n; p++) {
        uint32_t tex = texture[p];

        uint32_t au = (uint32_t)(base_u + (int32_t)tex * pu);
        uint32_t av = (uint32_t)(base_v + (int32_t)tex * pv);

        uint16_t u = (uint16_t)((uint16_t)(au >> 8) >> 3);
        uint8_t  v = (uint8_t)((uint16_t)(av >> 8) >> 3);

        uint16_t r = (uint16_t)((u & (uint16_t)mask)
                              + (uint16_t)((v & (uint8_t)mask) * mult));
        memcpy(output + p * 2, &r, 2);
    }
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void gpu2d_bg_affine_texcoord_pack_batched(const unsigned char *texture, unsigned char *output,
                         int count, int base_u, int base_v,
                         int step_u, int step_v, int mask, int shift)
{

    int16_t pu = (int16_t)step_u;
    int16_t pv = (int16_t)step_v;

    uint32_t mask2 = (uint32_t)mask + (uint32_t)mask;
    uint16_t maskU = (uint16_t)mask2;
    uint8_t  maskV = (uint8_t)mask2;

    uint32_t shiftHdr = (uint32_t)shift & 0x1fu;
    uint32_t hdr = (((uint32_t)base_u >> 10) & mask2)
                 + ((((uint32_t)base_v >> 10) & mask2) << shiftHdr);
    memcpy(output, &hdr, 4);
    output += 2;

    uint8_t p9 = (uint8_t)shift;
    uint8_t mult = (uint8_t)(p9 >= 8 ? 0u : (1u << p9));

#ifdef __ARM_NEON

    {
        const int16x8_t vpu = vdupq_n_s16(pu), vpv = vdupq_n_s16(pv);
        const int32x4_t vbu = vdupq_n_s32(base_u), vbv = vdupq_n_s32(base_v);
        const uint16x8_t vmu = vdupq_n_u16(maskU);
        const uint8x16_t vmv = vdupq_n_u8(maskV);
        const uint8x16_t vml = vdupq_n_u8(mult);
        const unsigned char *tex = texture;
        unsigned char *out = output;
        int32_t left = count;
        do {
            uint8x16_t t0 = vld1q_u8(tex), t1 = vld1q_u8(tex + 16);
            uint16x8_t g[4];
            g[0] = vmovl_u8(vget_low_u8(t0));  g[1] = vmovl_high_u8(t0);
            g[2] = vmovl_u8(vget_low_u8(t1));  g[3] = vmovl_high_u8(t1);
            uint16x8_t U[4], V[4];
            for (int k = 0; k < 4; k++) {
                int16x8_t gs = vreinterpretq_s16_u16(g[k]);
                int32x4_t ua = vmlal_s16(vbu, vget_low_s16(gs), vget_low_s16(vpu));
                int32x4_t ub = vmlal_high_s16(vbu, gs, vpu);
                uint16x8_t u = vshrn_high_n_u32(
                        vshrn_n_u32(vreinterpretq_u32_s32(ua), 8),
                        vreinterpretq_u32_s32(ub), 8);
                U[k] = vandq_u16(vshrq_n_u16(u, 2), vmu);
                int32x4_t va = vmlal_s16(vbv, vget_low_s16(gs), vget_low_s16(vpv));
                int32x4_t vb = vmlal_high_s16(vbv, gs, vpv);
                V[k] = vshrn_high_n_u32(
                        vshrn_n_u32(vreinterpretq_u32_s32(va), 8),
                        vreinterpretq_u32_s32(vb), 8);
            }
            uint8x16_t v01 = vandq_u8(vshrn_high_n_u16(vshrn_n_u16(V[0], 2), V[1], 2), vmv);
            uint8x16_t v23 = vandq_u8(vshrn_high_n_u16(vshrn_n_u16(V[2], 2), V[3], 2), vmv);
            U[0] = vmlal_u8(U[0], vget_low_u8(v01), vget_low_u8(vml));
            U[1] = vmlal_high_u8(U[1], v01, vml);
            U[2] = vmlal_u8(U[2], vget_low_u8(v23), vget_low_u8(vml));
            U[3] = vmlal_high_u8(U[3], v23, vml);
            for (int k = 0; k < 4; k++)
                vst1q_u16((uint16_t *)(void *)(out + k * 16), U[k]);
            tex += 32; out += 64; left -= 32;
        } while (left > 0);
        return;
    }
#else

    int32_t remaining = count;
    do {

        for (int i = 0; i < 32; i++) {
            uint32_t t = texture[i];

            uint32_t u_raw = (uint32_t)(base_u + (int)t * (int)pu);
            uint16_t u_mid = (uint16_t)(u_raw >> 8);
            uint16_t u16   = (uint16_t)((u_mid >> 2) & maskU);

            uint32_t v_raw = (uint32_t)(base_v + (int)t * (int)pv);
            uint16_t v_mid = (uint16_t)(v_raw >> 8);
            uint8_t  v8    = (uint8_t)(((uint8_t)(v_mid >> 2)) & maskV);

            uint16_t r = (uint16_t)(u16 + (uint16_t)((uint16_t)v8 * (uint16_t)mult));
            memcpy(output + i * 2, &r, 2);
        }

        texture  += 32;
        output   += 64;
        remaining -= 32;
    } while (remaining > 0);
#endif
}

#define BLOCK 32

void gpu2d_bg_affine_gen_dual_ramp_packed(uint8_t *output, int base1, int base2,
                        int inc1, int inc2, int n) {

    int turns = (n / BLOCK) + 1;

    for (int v = 0; v < turns; v++) {
        for (int k = 0; k < BLOCK; k++) {
            int i = v * BLOCK + k;
            uint16_t r1 = (uint16_t)(base1 + inc1 * i);
            uint16_t r2 = (uint16_t)(base2 + inc2 * i);
            uint8_t c1 = (uint8_t)(r1 >> 8);
            uint8_t c2 = (uint8_t)(r2 >> 8);
            output[i] = (uint8_t)(((c2 & 7) << 3) | (c1 & 7));
        }
    }
}
#undef BLOCK

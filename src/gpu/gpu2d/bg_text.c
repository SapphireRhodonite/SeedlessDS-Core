#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <string.h>
#include "core_internals.h"
#include "mem_access.h"





void gpu2d_bg_text_identity(gpu2d_bg_t *bg, uint8_t *dst, uint8_t *mask, uint32_t line)
{
}






static uint8_t paint_tile_row_4bpp(uint8_t *dst, const uint8_t *pal, uint32_t pix,
                             int inverse)
{
    uint32_t m;

    if (!inverse) {
        wr16(dst - 8, rd16(pal + ((pix & 15u) << 1)));
        wr16(dst - 6, rd16(pal + (((pix >> 4) & 15u) << 1)));
        wr16(dst - 4, rd16(pal + (((pix >> 8) & 15u) << 1)));
        wr16(dst - 2, rd16(pal + (((pix >> 12) & 15u) << 1)));
        wr16(dst, rd16(pal + (((pix >> 16) & 15u) << 1)));
        m = pix | (pix >> 2);
        m = (m | (m >> 1)) & 0x11111111u;
        wr16(dst + 2, rd16(pal + (((pix >> 20) & 15u) << 1)));
        m |= m >> 3;
        m = (m | (m >> 6)) & 0x000f000fu;
        m |= m >> 12;
        wr16(dst + 4, rd16(pal + (((pix >> 24) & 15u) << 1)));
        wr16(dst + 6, rd16(pal + (((pix >> 28) & 15u) << 1)));
        return (uint8_t)m;
    }

    wr16(dst - 8, rd16(pal + ((pix >> 27) & 0x1eu)));
    wr16(dst - 6, rd16(pal + (((pix >> 24) & 15u) << 1)));
    wr16(dst - 4, rd16(pal + (((pix >> 20) & 15u) << 1)));
    wr16(dst - 2, rd16(pal + (((pix >> 16) & 15u) << 1)));
    wr16(dst, rd16(pal + (((pix >> 12) & 15u) << 1)));
    m = pix | (pix << 2);
    m = (m | (m << 1)) & 0x88888888u;
    wr16(dst + 2, rd16(pal + (((pix >> 8) & 15u) << 1)));
    m |= m >> 5;
    m = (m | (m >> 10)) & 0x000f000fu;
    m = (m & 0x000fffffu) | ((m & 0xfffu) << 20);
    wr16(dst + 4, rd16(pal + (((pix >> 4) & 15u) << 1)));
    wr16(dst + 6, rd16(pal + ((pix & 15u) << 1)));
    return (uint8_t)(m >> 16);
}

static uint8_t paint_tile_row_8bpp(uint8_t *dst, const uint8_t *pal, uint32_t low,
                             uint32_t height, int inverse)
{
    uint32_t m, n;

    if (!inverse) {
        wr16(dst - 8, rd16(pal + ((low & 255u) << 1)));
        wr16(dst - 6, rd16(pal + (((low >> 8) & 255u) << 1)));
        wr16(dst - 4, rd16(pal + (((low >> 16) & 255u) << 1)));
        wr16(dst - 2, rd16(pal + (((low >> 24) & 255u) << 1)));
        m = (low | (low >> 4)) & 0x0f0f0f0fu;
        n = (height | (height << 4)) & 0xf0f0f0f0u;
        m |= n;
        wr16(dst, rd16(pal + ((height & 255u) << 1)));
        m |= m >> 2;
        m |= m >> 1;
        m &= 0x11111111u;
        wr16(dst + 2, rd16(pal + (((height >> 8) & 255u) << 1)));
        m |= m >> 7;
        wr16(dst + 4, rd16(pal + (((height >> 16) & 255u) << 1)));
        wr16(dst + 6, rd16(pal + (((height >> 24) & 255u) << 1)));
        return (uint8_t)m;
    }

    wr16(dst - 8, rd16(pal + ((height >> 23) & 0x1feu)));
    wr16(dst - 6, rd16(pal + (((height >> 16) & 255u) << 1)));
    wr16(dst - 4, rd16(pal + (((height >> 8) & 255u) << 1)));
    wr16(dst - 2, rd16(pal + ((height & 255u) << 1)));
    m = (height | (height >> 4)) & 0x0f0f0f0fu;
    n = (low | (low << 4)) & 0xf0f0f0f0u;
    m |= n;
    wr16(dst, rd16(pal + ((low >> 23) & 0x1feu)));
    m |= m << 2;
    m |= m << 1;
    m &= 0x88888888u;
    wr16(dst + 2, rd16(pal + (((low >> 16) & 255u) << 1)));
    m |= m >> 9;
    wr16(dst + 4, rd16(pal + (((low >> 8) & 255u) << 1)));
    m |= m >> 18;
    wr16(dst + 6, rd16(pal + ((low & 255u) << 1)));
    return (uint8_t)m;
}

void gpu2d_bg_text_decode_tiles_line(void *param_1, void *param_2, void *param_3,
                         uint32_t param_4)
{
    gpu2d_bg_t *ctx = (gpu2d_bg_t *)param_1;
    uint8_t *dst = (uint8_t *)param_2;
    uint8_t *out = (uint8_t *)param_3;
    uint8_t scratch[33];
    uint16_t pos = ctx->hofs;
    uint16_t advance = ctx->vofs;
    uint16_t mode = ctx->bgcnt;
    uint8_t *base = ctx->vram_window;
    uint8_t *pal0 = ctx->palette;
    uint32_t map = ctx->screen_base;
    uint32_t data = ctx->char_base;

    uint32_t line = (uint32_t)advance + param_4;
    uint32_t turn = (uint32_t)pos & 7u;
    uint32_t block, block_b, idx;
    uint8_t *table, *table_b;
    uint32_t count = 32u + (turn != 0);

    if ((line & 0x100u) != 0 && (mode & 0x8000u) != 0)
        map += (mode & 0x4000u) != 0 ? 0x1000u : 0x800u;
    block = map + ((line << 3) & 0x7c0u);
    block_b = block;
    if ((mode & 0x4000u) != 0) {
        uint32_t other = block + 0x800u;
        if (pos > 255u) {
            block_b = other;

        } else {
            block_b = block;
            block = other;
        }
    }
    table = base + block;
    table_b = base + block_b;
    idx = (pos >> 3) & 31u;

    if ((mode & 0x80u) == 0) {
        uint32_t q = (line & 7u) << 2;
        int32_t bias = 7 - (int32_t)(2u * (line & 7u));
        uint32_t origin = data + q;
        dst += (int64_t)(4 - (int32_t)turn) * 2;
        for (uint32_t i = 0; i < count; ++i) {
            uint16_t desc = rd16(table + (idx << 1));
            uint32_t next = idx + 1;
            uint8_t *src;
            uint32_t pix;
            const uint8_t *pal;
            idx = next & 31u;
            if (idx == 0)
                table = table_b;
            src = base + origin + ((uint32_t)(desc & 0x3ffu) << 5);
            if ((desc & 0x800u) != 0)
                src += (int64_t)bias * 4;
            pix = rd32(src);
            pal = pal0 + ((desc >> 7) & 0x1e0u);
            scratch[i] = paint_tile_row_4bpp(dst, pal, pix, (desc & 0x400u) != 0);
            dst += 16;
        }
    } else {
        uint32_t q = (line & 7u) << 3;
        int32_t bias = 14 - (int32_t)(4u * (line & 7u));
        uint32_t origin = data + q;
        int uses_palette_ext = ctx->ext_palette_enabled != 0;
        uint8_t *pal = NULL;
        if (uses_palette_ext)
            pal = ctx->ext_palette;
        if (uses_palette_ext && pal == NULL)
            return;
        dst += (int64_t)(4 - (int32_t)turn) * 2;
        for (uint32_t i = 0; i < count; ++i) {
            uint16_t desc = rd16(table + (idx << 1));
            uint32_t next = idx + 1;
            uint8_t *src;
            uint32_t low, height;
            const uint8_t *colors;
            idx = next & 31u;
            if (idx == 0)
                table = table_b;
            src = base + origin + ((uint32_t)(desc & 0x3ffu) << 6);
            if ((desc & 0x800u) != 0)
                src += (int64_t)bias * 4;
            low = rd32(src);
            height = rd32(src + 4);
            colors = uses_palette_ext ? pal + ((desc >> 3) & 0x1e00u) : pal0;
            scratch[i] = paint_tile_row_8bpp(dst, colors, low, height,
                                      (desc & 0x400u) != 0);
            dst += 16;
        }
    }

    if (turn == 0) {
        memcpy(out, scratch, 32);
    } else {
        uint32_t r = turn & 31u;
        uint32_t l = (0u - r) & 31u;
        for (uint32_t i = 0; i != 8; ++i) {
            uint32_t a = rd32(scratch + (i << 2));
            uint32_t b = i == 7 ? (uint32_t)scratch[32]
                                : rd32(scratch + ((i + 1) << 2));
            wr32(out + (i << 2), (a >> r) | (b << l));
        }
    }
}

static int64_t sdiv64(int64_t num, int64_t den) {
    if (den == 0) return 0;
    if (den == -1) return (int64_t)(0u - (uint64_t)num);
    return num / den;
}

static int32_t sdiv32(int32_t num, int32_t den) {
    if (den == 0) return 0;
    if (den == -1) return (int32_t)(0u - (uint32_t)num);
    return num / den;
}

static int64_t ceiling(int32_t height, int32_t den) {

    uint64_t num = (uint64_t)(int64_t)height << 32;
    int64_t  d   = den;
    if (height < 0) {
        if (den < 0) num = (uint64_t)d + num + 1u;
    } else {
        if (den >= 0) num = (uint64_t)d + num - 1u;
    }
    return sdiv64((int64_t)num, d);
}

static int32_t minus(int32_t v)             { return (int32_t)(0u - (uint32_t)v); }

static int32_t subtract(int32_t a, int32_t b)  { return (int32_t)((uint32_t)a - (uint32_t)b); }

static int32_t sum3(int32_t a, int32_t b, int32_t c) {
    return (int32_t)((uint32_t)a + (uint32_t)b + (uint32_t)c);
}

void gpu2d_bg_text_compute_span_range(int32_t x, int32_t a, int32_t b, int32_t c,
                        int64_t *out_a, int64_t *out_b, int64_t *out_c) {

    if (a != 0) {

        int32_t first, bb;
        if (a > 0) {
            first = subtract(a, 1);
            bb = b;
        } else {
            first = sum3(a, b, 1);
            bb = 0;
        }

        int64_t base  = ceiling(subtract(first, x), a);
        int64_t cap  = ceiling(subtract(bb, x), a);
        int64_t step  = ceiling(minus(c), a);

        *out_a = base;
        *out_c = cap - base;
        *out_b = step;
        return;
    }

    if (c == 0) {

        int outside = (x < 0) || (x > b);
        *out_a = outside ? -1 : 0;
        *out_c = outside ? 0 : 0x10000000000LL;
        *out_b = 0;
        return;
    }

    int32_t p, q;
    int32_t nx  = minus(x);
    int32_t nxb = (int32_t)(~(uint32_t)x + (uint32_t)b);
    if (c >= 1) {
        p = nxb;
        q = nx;
    } else {
        p = nx;
        q = nxb;
    }
    int64_t u = (int64_t)sdiv32(p, c) << 40;
    int64_t v = (int64_t)sdiv32(q, c) << 40;

    *out_a = -u;
    *out_c = u - v;
    *out_b = 0x10000000000LL;
}

void gpu2d_bg_text_compute_step_reciprocals(gpu2d_bg_t *obj) {

    int16_t sVar1 = obj->pa;
    int16_t sVar2 = obj->pc;

    uint32_t uVar4 = (sVar1 < 0) ? (uint32_t)(-(int32_t)sVar1) : (uint32_t)sVar1;
    uint32_t uVar5 = (sVar2 < 0) ? (uint32_t)(-(int32_t)sVar2) : (uint32_t)sVar2;

    if (uVar4 != 0) {
        uint32_t uVar3 = (uVar4 + GPU2D_RECIPROCAL_NUMERATOR) / uVar4;
        obj->pa_reciprocal = uVar3;
    }
    if (uVar5 != 0) {
        uint32_t uVar3b = (uVar5 + GPU2D_RECIPROCAL_NUMERATOR) / uVar5;
        obj->pc_reciprocal = uVar3b;
    }
}







static int32_t sar11(uint32_t v) {
    uint32_t r = v >> 11;
    if (v & 0x80000000u) r |= 0xffe00000u;
    return (int32_t)r;
}

static int32_t sar8(uint32_t v) {
    uint32_t r = v >> 8;
    if (v & 0x80000000u) r |= 0xff000000u;
    return (int32_t)r;
}

static uint8_t mask8(const uint8_t *p) {
    uint32_t a = rd32(p);
    uint32_t b = rd32(p + 4);
    a |= a >> 4;
    b |= b << 4;
    a &= 0x0f0f0f0fu;
    b &= 0xf0f0f0f0u;
    a = b | a;
    a |= a >> 2;
    a |= a >> 1;
    a &= 0x11111111u;
    a |= a >> 7;
    a |= a >> 14;
    return (uint8_t)a;
}

void gpu2d_bg_text_sample_texture_line(gpu2d_bg_t *p, uint8_t *dest, uint8_t *mask) {

    uint8_t local[256] = {0};
    for (unsigned i = 0; i != 32; ++i) mask[i] = 0;

    uint32_t step_x;
    uint32_t step_y;
    if (p->affine_dirty != 0) {
        uint32_t composite = 0x7ffu | ((uint32_t)p->tiles_per_row_minus_one << 11);
        gpu2d_bg_text_compute_span_range(
            (int32_t)p->current_x, (int16_t)p->pa,
            (int32_t)composite, (int16_t)p->pb,
            &p->span_x[0], &p->span_x[2], &p->span_x[1]);
        gpu2d_bg_text_compute_span_range(
            (int32_t)p->current_y, (int16_t)p->pc,
            (int32_t)composite, (int16_t)p->pd,
            &p->span_y[0], &p->span_y[2], &p->span_y[1]);

        step_x = (uint32_t)(int32_t)(int16_t)p->pa;
        step_y = (uint32_t)(int32_t)(int16_t)p->pc;
        uint32_t ax = (step_x & 0x80000000u) ? 0u - step_x : step_x;
        uint32_t ay = (step_y & 0x80000000u) ? 0u - step_y : step_y;
        if (ax != 0) p->pa_reciprocal = (ax + GPU2D_RECIPROCAL_NUMERATOR) / ax;
        if (ay != 0) p->pc_reciprocal = (ay + GPU2D_RECIPROCAL_NUMERATOR) / ay;
        p->affine_dirty = 0;
    } else {
        step_x = (uint32_t)(int32_t)(int16_t)p->pa;
        step_y = (uint32_t)(int32_t)(int16_t)p->pc;
    }

    uint32_t base = p->screen_base;
    uint32_t offset = p->char_base;
    const uint8_t *table = p->vram_window;
    const uint8_t *palette = p->palette;
    const uint8_t *texture = table + offset;
    uint32_t coord_x = p->current_x;
    uint32_t coord_y = p->current_y;
    uint32_t jump_row = p->size_code;
    uint16_t mode = p->bgcnt;

    if ((mode & 0x2000u) != 0) {
        uint32_t limit = p->tiles_per_row_minus_one;
        uint32_t page_x = limit & (coord_x >> 11);
        uint32_t page_y = limit & (coord_y >> 11);
        uint32_t idx = base + page_x + (page_y << (jump_row & 31));
        const uint8_t *block = texture + ((uint64_t)table[(int32_t)idx] << 6);

        for (uint32_t i = 0; i != 256; ++i) {
            uint32_t cell = (uint32_t)sar8(coord_x) +
                             ((uint32_t)sar8(coord_y) << 3);
            uint8_t entry = block[(uint64_t)cell];
            coord_x += step_x;
            coord_y += step_y;
            wr16(dest + ((uint64_t)i << 1),
                  rd16(palette + ((uint32_t)entry << 1)));
            local[i] = block[(uint64_t)cell];

            if ((coord_x | coord_y) >= 0x800u) {
                page_y = (page_y + (coord_y >> 11)) & limit;
                page_x = (page_x + (coord_x >> 11)) & limit;
                idx = base + page_x + (page_y << (jump_row & 31));
                block = texture + ((uint64_t)table[(int32_t)idx] << 6);
                coord_x &= 0x7ffu;
                coord_y &= 0x7ffu;
            }
        }
        for (unsigned i = 0; i != 32; ++i) mask[i] = mask8(local + i * 8);
        return;
    }

    uint64_t range_88 = p->span_x[0];
    uint64_t range_96 = p->span_x[1];
    uint64_t range_104 = p->span_x[2];
    uint64_t range_112 = p->span_y[0];
    uint64_t range_120 = p->span_y[1];
    uint64_t range_128 = p->span_y[2];
    uint64_t updated_88 = range_104 + range_88;
    uint64_t updated_112 = range_128 + range_112;
    p->span_y[0] = updated_112;
    p->span_x[0] = updated_88;

    int32_t start_a = (int32_t)(range_112 >> 32);
    int32_t start_b = (int32_t)(range_88 >> 32);
    int32_t start = start_a > start_b ? start_a : start_b;
    if (start < 0) start = 0;
    int32_t final_a = (int32_t)((range_96 + range_88) >> 32);
    int32_t final_b = (int32_t)((range_120 + range_112) >> 32);
    int32_t final = final_b < final_a ? final_b : final_a;

    if (start > 255 || final < 0) return;
    if (final >= 255) final = 255;

    if (final >= start) {
        uint32_t page_x = (uint32_t)sar11(coord_x) + base;
        uint32_t page_y = (uint32_t)sar11(coord_y);
        uint32_t idx = page_x + (page_y << (jump_row & 31));
        const uint8_t *block = texture + ((uint64_t)table[(int32_t)idx] << 6);
        uint8_t *output = dest + ((uint64_t)(uint32_t)start << 1);

        for (int32_t i = start; ; ++i) {
            uint32_t cell = (uint32_t)sar8(coord_x) +
                             ((uint32_t)sar8(coord_y) << 3);
            uint8_t entry = block[(uint64_t)cell];
            coord_x += step_x;
            wr16(output, rd16(palette + ((uint32_t)entry << 1)));
            coord_y += step_y;
            local[(uint32_t)i] = block[(uint64_t)cell];
            output += 2;
            if (i == final) break;
            if ((coord_x | coord_y) >= 0x800u) {
                page_y += (uint32_t)sar11(coord_y);
                page_x += (uint32_t)sar11(coord_x);
                idx = page_x + (page_y << (jump_row & 31));
                block = texture + ((uint64_t)table[(int32_t)idx] << 6);
                coord_x &= 0x7ffu;
                coord_y &= 0x7ffu;
            }
        }
    }

    unsigned first_byte = (unsigned)start >> 3;
    unsigned last_byte = (unsigned)final >> 3;
    if (first_byte <= last_byte) {
        for (unsigned i = first_byte; i <= last_byte; ++i)
            mask[i] = mask8(local + i * 8);
    }
    mask[first_byte] &= (uint8_t)(0xffu << ((unsigned)start & 7));
    mask[last_byte] &= (uint8_t)~(0xfeu << ((unsigned)final & 7));
}

void gpu2d_bg_text_compute_span_range_5(int32_t x, int32_t a, int32_t b, int32_t c,
                         int64_t *out_a, int64_t *out_b, int64_t *out_c) __asm__("gpu2d_bg_text_compute_span_range");



void gpu2d_bg_text_compute_span_range_pair(gpu2d_bg_t *p) {

    uint16_t field_a6 = p->mask_x;
    uint16_t field_a8 = p->mask_y;

    int32_t x1 = (int32_t)p->current_x;
    int32_t a1 = (int16_t)p->pa;
    int32_t b1 = (int32_t)(((uint32_t)field_a6 << 8) | 0xffu);
    int32_t c1 = (int16_t)p->pb;
    gpu2d_bg_text_compute_span_range(x1, a1, b1, c1,
                        &p->span_x[0],
                        &p->span_x[2],
                        &p->span_x[1]);

    int32_t x2 = (int32_t)p->current_y;
    int32_t a2 = (int16_t)p->pc;
    int32_t b2 = (int32_t)(((uint32_t)field_a8 << 8) | 0xffu);
    int32_t c2 = (int16_t)p->pd;
    gpu2d_bg_text_compute_span_range(x2, a2, b2, c2,
                        &p->span_y[0],
                        &p->span_y[2],
                        &p->span_y[1]);

    p->affine_dirty = 0;
}








static uint8_t pack_eight(const uint8_t *p)
{
    uint32_t a = rd32(p);
    uint32_t b = rd32(p + 4);

    a |= a >> 4;
    b |= b << 4;
    a &= 0x0f0f0f0fU;
    b &= 0xf0f0f0f0U;
    a |= b;
    a |= a >> 2;
    a |= a >> 1;
    a &= 0x11111111U;
    a |= a >> 7;
    a |= a >> 14;
    return (uint8_t)a;
}

static void convert_and_clip(uint8_t *mask, const uint8_t *tmp,
                                 uint32_t first, uint32_t last)
{
    uint32_t bfirst = first >> 3;
    uint32_t blast = last >> 3;
    uint32_t b;

    for (b = bfirst; b <= blast; ++b)
        wr8(mask + b, pack_eight(tmp + (b - bfirst) * 8));

    {
        uint8_t v = rd8(mask + bfirst);
        uint8_t m = (uint8_t)(0xffU << (first & 7));
        wr8(mask + bfirst, (uint8_t)(v & m));
    }
    {
        uint8_t v = rd8(mask + blast);
        uint8_t m = (uint8_t)~(0xfeU << (last & 7));
        wr8(mask + blast, (uint8_t)(v & m));
    }
}

static void take_pixel(const uint8_t *indices, const uint8_t *palette,
                       uint32_t address, uint8_t *tmp,
                       uint32_t it_tmp, uint8_t *dest16,
                       uint32_t it_dest)
{
    uint8_t idx = rd8(indices + (int64_t)(int32_t)address);
    wr8(tmp + it_tmp, idx);
    wr16(dest16 + ((uint64_t)it_dest << 1),
          rd16(palette + ((uint64_t)idx << 1)));
}

void gpu2d_bg_text_decode_indexed_line(gpu2d_bg_t *ctx, uint8_t *dest16, uint8_t *mask, uint32_t line)
{

    int32_t step_y = (int16_t)ctx->pa;
    uint32_t step_x_u = ctx->pc;
    uint32_t coord_y = ctx->current_x;
    uint32_t coord_x = ctx->current_y;
    uint32_t offset = ctx->width_shift;
    uint32_t base = ctx->affine_screen_offset;
    uint32_t limit_y = ctx->mask_x;
    uint32_t limit_x = ctx->mask_y;
    const uint8_t *indices = ctx->vram_window;
    const uint8_t *palette = ctx->palette;
    uint32_t flags = ctx->bgcnt;
    uint8_t tmp[256] = {0};
    uint32_t i;

    if (step_y == 256 && step_x_u == 0) {
        if (flags & 0x2000U) {
            uint32_t y = (coord_y >> 8) & limit_y;
            uint32_t x = (coord_x >> 8) & limit_x;
            uint32_t address = base + (x << (offset & 31)) + y;

            for (i = 0; i != 256; ++i) {
                take_pixel(indices, palette, address, tmp, i, dest16, i);
                ++address;
            }
        } else {
            uint32_t xbase = (uint32_t)((int32_t)coord_x >> 8);
            uint32_t ybase = (uint32_t)((int32_t)coord_y >> 8);
            uint32_t first;
            uint32_t last;
            uint32_t w11;
            uint32_t aligned;
            uint32_t address;
            uint32_t count;

            wr64(mask, 0);
            wr64(mask + 8, 0);
            wr64(mask + 16, 0);
            wr64(mask + 24, 0);
            if (coord_x & 0x80000000U)
                return;
            if (xbase > limit_x)
                return;

            last = limit_y - ybase;
            first = ((int32_t)coord_y > 255) ? 0U : (0U - ybase);
            if ((int32_t)last >= 255)
                last = 255;
            w11 = first + 7;
            if ((int32_t)first >= 0)
                w11 = first;
            if ((int32_t)(last - first) < 0)
                return;

            aligned = w11 & 0xfffffff8U;
            address = first + base + ybase +
                        (xbase << (offset & 31));
            count = last - first + 1;
            for (i = 0; i != count; ++i) {
                take_pixel(indices, palette, address, tmp,
                           first - aligned + i, dest16, first + i);
                ++address;
            }
            convert_and_clip(mask, tmp, first, last);
            return;
        }

        for (i = 0; i != 32; ++i)
            wr8(mask + i, pack_eight(tmp + i * 8));
        return;
    }

    if (flags & 0x2000U) {
        int32_t step_x = (int16_t)step_x_u;
        uint32_t y = coord_y;
        uint32_t x = coord_x;

        for (i = 0; i != 256; ++i) {
            uint32_t address = (((y >> 8) & limit_y) + base) +
                                 (((x >> 8) & limit_x) << (offset & 31));
            take_pixel(indices, palette, address, tmp, i, dest16, i);
            y += (uint32_t)step_y;
            x += (uint32_t)step_x;
        }

        for (i = 0; i != 32; ++i)
            wr8(mask + i, pack_eight(tmp + i * 8));
        return;
    }

    if (ctx->affine_dirty != 0) {
        uint32_t mix_y = 0xffU | ((limit_y & 0xffffU) << 8);
        uint32_t mix_x = 0xffU | ((limit_x & 0xffffU) << 8);

        gpu2d_bg_text_compute_span_range(coord_y, step_y, mix_y,
                                (int16_t)ctx->pb,
                                &ctx->span_x[0], &ctx->span_x[2], &ctx->span_x[1]);
        gpu2d_bg_text_compute_span_range(ctx->current_y,
                                (int16_t)ctx->pc, mix_x,
                                (int16_t)ctx->pd,
                                &ctx->span_y[0], &ctx->span_y[2], &ctx->span_y[1]);
        ctx->affine_dirty = 0;
    }

    {
        uint64_t a104 = ctx->span_x[2];
        uint64_t a112 = ctx->span_y[0];
        uint64_t a88 = ctx->span_x[0];
        uint64_t a96 = ctx->span_x[1];
        uint64_t a120 = ctx->span_y[1];
        uint64_t a128 = ctx->span_y[2];
        uint32_t height_a = (uint32_t)(a88 >> 32);
        uint32_t height_b = (uint32_t)(a112 >> 32);
        uint32_t height_c;
        uint32_t height_d;
        uint32_t first;
        uint32_t last;
        int32_t step_x = (int16_t)step_x_u;
        uint32_t count;

        a96 += a88;
        a104 += a88;
        a120 += a112;
        a112 = a128 + a112;
        height_c = (uint32_t)(a96 >> 32);
        height_d = (uint32_t)(a120 >> 32);
        ctx->span_y[0] = a112;
        first = ((int32_t)height_b > (int32_t)height_a) ? height_b : height_a;
        ctx->span_x[0] = a104;
        last = ((int32_t)height_d < (int32_t)height_c) ? height_d : height_c;

        wr64(mask, 0);
        wr64(mask + 8, 0);
        wr64(mask + 16, 0);
        wr64(mask + 24, 0);

        if ((int32_t)first > (int32_t)last)
            return;
        first &= ~(first >> 31);
        if ((int32_t)first > 255)
            return;
        if (last & 0x80000000U)
            return;
        if ((int32_t)last >= 255)
            last = 255;

        count = last - first + 1;
        for (i = 0; i != count; ++i) {
            uint32_t sample = first + i;
            uint32_t x = (uint32_t)((uint32_t)sample * (uint32_t)step_x + coord_x);
            uint32_t y = (uint32_t)((uint32_t)sample * (uint32_t)step_y + coord_y);
            uint32_t address = base +
                ((uint32_t)((int32_t)x >> 8) << (offset & 31)) +
                (uint32_t)((int32_t)y >> 8);
            take_pixel(indices, palette, address, tmp,
                       (first & 7) + i, dest16, first + i);
        }
        convert_and_clip(mask, tmp, first, last);
        return;
    }
}



static uint32_t rest_arm(uint32_t dividend, uint32_t divisor)
{
    return divisor == 0 ? dividend : dividend % divisor;
}

static uint32_t lsl_arm(uint32_t v, uint32_t offset)
{
    return v << (offset & 31u);
}

static const uint32_t gpu2d_mosaic_step_masks[16] = {
    0xffffffffu, 0x55555555u, 0x49249249u, 0x11111111u,
    0x42108421u, 0x41041041u, 0x10204081u, 0x01010101u,
    0x08040201u, 0x40100401u, 0x00400801u, 0x01001001u,
    0x04002001u, 0x10004001u, 0x40008001u, 0x00010001u,
};

static uint32_t propagate_word(uint32_t v, uint32_t scale)
{
    while (scale != 0) {
        v |= v << 1;
        scale--;
    }
    return v;
}

static uint32_t carried_bits(uint32_t carry, uint32_t offset)
{
    if (carry == 0 || offset == 0)
        return 0;
    if (offset >= 32)
        return UINT32_MAX;
    return (UINT32_C(1) << offset) - 1u;
}

void gpu2d_bg_text_expand_mask_words(void *param_1, uint32_t param_2)
{

    uint8_t *p = (uint8_t *)param_1;
    uint32_t scale = param_2 & 0xffu;
    uint32_t step = (scale + 1u) & 0xffu;
    uint32_t mask;
    uint32_t jump;
    uint32_t offset;
    uint32_t prev;
    uint32_t first;
    uint32_t i;

    first = rd32(p);
    mask = gpu2d_mosaic_step_masks[scale];
    jump = (step == 0 ? 0u : 32u / step) * step + step - 32u;
    offset = rest_arm(jump, step);
    prev = propagate_word(first & mask, scale);

    for (i = 1; i != 8; i++) {
        uint32_t word;
        uint32_t carry;

        word = rd32(p + i * 4u) & lsl_arm(mask, offset);
        carry = prev >> 31;
        wr32(p + (i - 1u) * 4u, prev);

        prev = propagate_word(word, scale) |
                   carried_bits(carry, offset);
        offset = rest_arm(jump + offset, step);
    }
    wr32(p + 28, prev);
}

void gpu2d_bg_text_expand_table_blocks(uint16_t *buf, int32_t n) {

    if (n == 0) {
        for (;;) { }
    }

    uint32_t idx  = 0;
    uint32_t step = (uint32_t)n + 1;

    for (;;) {
        uint16_t val = buf[idx];
        uint64_t j = (uint64_t)(idx + 1);
        idx = step + idx;
        int32_t count = n;

        for (;;) {
            if (j > 0xff) return;
            buf[j] = val;
            count -= 1;
            j += 1;
            if (count == 0) break;
        }
    }
}

#define LAYER_DST    544
#define LAYER_AUX    32
#define MAX_VAL        0xff






void gpu2d_bg_text_apply_mosaic_layers(gpu2d_engine_t *ctx, uint8_t *dsts, uint8_t *auxs, uint32_t line)
{

    uint32_t reg = ctx->mosaic;
    uint32_t my  = (reg >> 4) & 0xfu;

    uint32_t rest = 0;
    if (my != 0) {
        uint32_t d = my + 1u;
        rest = line - (line / d) * d;
    }

    if (ctx->bg_count == 0) return;

    uint32_t mx = reg & 0xfu;

    for (uint64_t i = 0; ; ) {
        uint32_t layer = ctx->bg_order[i];
        gpu2d_bg_t *cb = &ctx->bg[layer];

        if (cb->direct_ptr == 0) {
            uint8_t *dst = dsts + (size_t)layer * LAYER_DST + 0x10;
            uint8_t *aux = auxs + (size_t)layer * LAYER_AUX;

            if ((uint8_t)cb->bgcnt & 0x40) {

                int32_t  px  = cb->pb;
                int32_t  py  = cb->pd;
                uint32_t ox  = (uint32_t)cb->current_x;
                uint32_t oy  = (uint32_t)cb->current_y;
                uint32_t pos = cb->vofs;
                gpu2d_bg_line_fn raster = cb->line_handler;

                cb->vofs = (uint16_t)(pos - rest);
                cb->current_x = (int32_t)(ox - rest * (uint32_t)px);
                cb->current_y = (int32_t)(oy - rest * (uint32_t)py);

                raster(cb, dst, aux, line);

                cb->vofs = (uint16_t)pos;
                cb->current_x = (int32_t)ox;
                cb->current_y = (int32_t)oy;

                if (mx != 0) {
                    gpu2d_bg_text_expand_mask_words(aux, mx);

                    uint32_t s = 0;
                    for (;;) {
                        uint16_t v = rd16(dst + (size_t)s * 2);
                        uint64_t d = (uint32_t)(s + 1u);
                        s += mx + 1u;
                        uint32_t n = mx;
                        for (;;) {
                            if (d > MAX_VAL) goto next;
                            wr16(dst + d * 2, v);
                            n--;
                            d++;
                            if (n == 0) break;
                        }
                    }
                }
            } else {
                cb->line_handler(cb, dst, aux, line);
            }
        }

next:
        i++;
        if (i >= ctx->bg_count) return;
    }
}
#undef LAYER_DST
#undef LAYER_AUX
#undef MAX_VAL

#define BANKS    4
#define KIND_OK   6
#define LIMIT    0x4000





void gpu2d_bg_text_cache_direct_ptr(gpu2d_engine_t *ctx, uint32_t layer) {

    gpu2d_bg_t *c = &ctx->bg[layer];

    if ((c->bgcnt & 0xc0fcu) != 0x4084u) return;
    if (c->ref_x != 0) return;
    if (c->ref_y != 0) return;
    if ((uint16_t)c->pa != 0x100) return;
    if (c->pc != 0) return;
    if (c->pb != 0) return;
    if ((uint16_t)c->pd != 0x100) return;

    const vram_map_t *bank = (const vram_map_t *)ctx->gpu;
    uint32_t v = c->affine_screen_offset;
    uint32_t key = v & 0xfffe0000u;

    uint32_t chosen = 0xff;
    for (int i = 0; i < BANKS; i++) {
        if (bank->bank[i].map_kind != KIND_OK) continue;
        if (key == bank->bank[i].first_slot * 0x4000u)
            chosen = (uint32_t)i;
    }
    if (chosen == 0xff) return;

    uint32_t coord = (v >> 1) & 0xffffu;
    if (coord <= LIMIT) {
        uint8_t bits = GPU_OUTPUT_OF(bank)->bank_texture_bits[chosen];
        uint32_t mask = 0x3fu << (coord >> 13);
        void *p = 0;
        if ((mask & ~(uint32_t)bits) == 0) {
            uint8_t *table = GPU_OUTPUT_OF(bank)->capture_shadow[chosen];

            p = table + (size_t)coord * (size_t)recon_shadow_stride() * 2u;
        }
        c->direct_ptr_alt = p;
    }

    c->direct_ptr = ctx->vram_window + v;
}
#undef BANKS
#undef KIND_OK
#undef LIMIT

void gpu2d_bg_text_unpack33_alt_buffer_narrow(unsigned char *out16, unsigned char *out8,
                        const unsigned char *a, const unsigned char *b,
                        uint32_t idx, uint32_t base) {
    uint32_t comp = 0x1cu - base;
    const unsigned char *source = a;
    for (uint64_t i = 0; i < 0x21; i++) {
        uint16_t h;
        memcpy(&h, source + (uint64_t)idx * 2, 2);
        uint32_t v = h;
        uint32_t sig = idx + 1;
        if ((sig & 0x1fu) == 0) source = b;
        uint32_t field = (v & 0x3ffu) << 5;
        uint32_t chosen = ((v & 0x800u) == 0) ? base : comp;
        uint32_t height = v >> 8;
        uint16_t r = (uint16_t)(chosen + field);
        memcpy(out16 + i * 2, &r, 2);
        out8[i] = (unsigned char)height;
        idx = sig & 0x1fu;
    }
}

void gpu2d_bg_text_unpack33_alt_buffer_wide(unsigned char *out16, unsigned char *out8,
                        const unsigned char *a, const unsigned char *b,
                        uint32_t idx, uint32_t base) {
    uint32_t comp = 0x38u - base;
    const unsigned char *source = a;
    for (uint64_t i = 0; i < 0x21; i++) {
        uint16_t h;
        memcpy(&h, source + (uint64_t)idx * 2, 2);
        uint32_t v = h;
        uint32_t sig = idx + 1;
        if ((sig & 0x1fu) == 0) source = b;
        uint32_t chosen = ((v & 0x800u) == 0) ? base : comp;
        uint32_t height = v >> 8;
        uint16_t r = (uint16_t)(chosen + (v << 6));
        memcpy(out16 + i * 2, &r, 2);
        out8[i] = (unsigned char)height;
        idx = sig & 0x1fu;
    }
}

static uint32_t recon_rev32_87374(uint32_t v)
{
    return ((v & 0x000000ffu) << 24)
         | ((v & 0x0000ff00u) <<  8)
         | ((v & 0x00ff0000u) >>  8)
         | ((v & 0xff000000u) >> 24);
}

void gpu2d_bg_text_decode_4bpp_groups33(uint8_t *dest, uint8_t *words,
                        const uint8_t *palette, const uint8_t *table,
                        const uint8_t *indices, const uint8_t *attrs)
{

    uint64_t i = 0;

    for (;;) {

        uint16_t h;
        uint32_t w9, w11;
        const uint8_t *page;

        memcpy(&h, indices + i * 2u, 2);

        w11 = attrs[i];

        memcpy(&w9, table + (uint64_t)h, 4);

        page = palette + (uint64_t)(w11 & 0xf0u) * 2u;

        if ((w11 & 4u) != 0u) {
            uint32_t height, low;

            height = w9 >> 4;
            low = w9 << 4;
            height = height & 0x0f0f0f0fu;
            low = low & 0xf0f0f0f0u;
            w9   = height | low;
            w9   = recon_rev32_87374(w9);
        }

        {
            uint16_t p;
            uint32_t n;

            n = w9 & 0xfu;
            memcpy(&p, page + (uint64_t)n * 2u, 2);
            memcpy(dest + 0, &p, 2);

            n = (w9 >> 4) & 0xfu;
            memcpy(&p, page + (uint64_t)n * 2u, 2);
            memcpy(dest + 2, &p, 2);

            n = (w9 >> 8) & 0xfu;
            memcpy(&p, page + (uint64_t)n * 2u, 2);
            memcpy(dest + 4, &p, 2);

            n = (w9 >> 12) & 0xfu;
            memcpy(&p, page + (uint64_t)n * 2u, 2);
            memcpy(dest + 6, &p, 2);

            n = (w9 >> 16) & 0xfu;
            memcpy(&p, page + (uint64_t)n * 2u, 2);
            memcpy(dest + 8, &p, 2);

            n = (w9 >> 20) & 0xfu;
            memcpy(&p, page + (uint64_t)n * 2u, 2);
            memcpy(dest + 10, &p, 2);

            n = (w9 >> 24) & 0xfu;
            memcpy(&p, page + (uint64_t)n * 2u, 2);
            memcpy(dest + 12, &p, 2);

            n = w9 >> 28;
            memcpy(&p, page + (uint64_t)n * 2u, 2);
            memcpy(dest + 14, &p, 2);
        }

        memcpy(words + i * 4u, &w9, 4);

        i += 1;
        dest += 16;

        if (i == 0x21u) break;
    }
}

void gpu2d_bg_text_gather_words_by_offset_table(uint64_t *out, uint8_t *base, const uint16_t *offs,
                   const uint8_t *flags)
{

    for (int i = 0; i < 33; i++) {
        uint16_t off = offs[i];
        uint8_t  flg = flags[i];
        uint64_t raw;

        memcpy(&raw, base + off, sizeof(raw));

        if (flg & 4) {
            raw = __builtin_bswap64(raw);
        }

        memcpy((uint8_t *)out + 8 * i, &raw, sizeof(raw));
    }
}

static uint64_t byte_reverse64(uint64_t v)
{
    return ((v & 0x00000000000000ffull) << 56)
         | ((v & 0x000000000000ff00ull) << 40)
         | ((v & 0x0000000000ff0000ull) << 24)
         | ((v & 0x00000000ff000000ull) <<  8)
         | ((v & 0x000000ff00000000ull) >>  8)
         | ((v & 0x0000ff0000000000ull) >> 24)
         | ((v & 0x00ff000000000000ull) >> 40)
         | ((v & 0xff00000000000000ull) >> 56);
}

void gpu2d_bg_text_unpack_tile8_bank_words33(uint8_t *dest, const uint8_t *table,
                        const uint8_t *indices, const uint8_t *attrs)
{

    uint64_t i = 0;

    do {
        uint16_t h;
        uint32_t w9, w10, w11, w12, w13, w14;
        uint64_t v, rv;

        memcpy(&h, indices + i * 2u, 2);

        w10 = attrs[i];
        i += 1;

        memcpy(&v, table + (uint64_t)h, 8);

        w12 = (w10 << 4) & 0xf00u;

        {
            int eq = ((w10 & 4u) == 0u);

            rv  = byte_reverse64(v);
            w11 = w10 >> 4;
            w10 = w12;
            w13 = w12;

            v = eq ? v : rv;
        }

        w10 = (w10 & ~0xffu) | (uint32_t)( v        & 0xffu);
        w13 = (w13 & ~0xffu) | (uint32_t)((v >>  8) & 0xffu);
        w12 = (w12 & ~0xffu) | (uint32_t)((v >> 16) & 0xffu);
        w14 =                  (uint32_t)((v >> 24) & 0xffu);

        h = (uint16_t)w10;
        memcpy(dest + 0, &h, 2);

        w10 = (uint32_t)((v >> 32) & 0xffu);

        h = (uint16_t)w13;
        memcpy(dest + 2, &h, 2);

        w13 = (uint32_t)((v >> 40) & 0xffu);

        h = (uint16_t)w12;
        memcpy(dest + 4, &h, 2);

        w12 = (uint32_t)((v >> 48) & 0xffu);
        w9  = (uint32_t)( v >> 56);

        w14 = (w14 & ~0xf00u) | ((w11 & 0xfu) << 8);
        w10 = (w10 & ~0xf00u) | ((w11 & 0xfu) << 8);
        w13 = (w13 & ~0xf00u) | ((w11 & 0xfu) << 8);
        w12 = (w12 & ~0xf00u) | ((w11 & 0xfu) << 8);
        w9  = (w9  & ~0xf00u) | ((w11 & 0xfu) << 8);

        h = (uint16_t)w14; memcpy(dest +  6, &h, 2);
        h = (uint16_t)w10; memcpy(dest +  8, &h, 2);
        h = (uint16_t)w13; memcpy(dest + 10, &h, 2);
        h = (uint16_t)w12; memcpy(dest + 12, &h, 2);
        h = (uint16_t)w9;  memcpy(dest + 14, &h, 2);

        dest += 16;
    } while (i != 0x21u);

}



static uint8_t compress_87860(uint32_t v)
{
    v |= v >> 2;
    v |= v >> 1;
    v &= UINT32_C(0x11111111);
    v |= v >> 3;
    v |= v >> 6;
    v &= UINT32_C(0x000f000f);
    v |= v >> 12;
    return (uint8_t)v;
}

static uint32_t group_87860(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void gpu2d_bg_text_pack_nibbles_33(void *param_1, const void *param_2, uint32_t param_3)
{
    const uint8_t *entry = (const uint8_t *)param_2;
    uint8_t compact[33];
    uint32_t groups[9];
    uint32_t right = param_3 & 31U;
    uint32_t left = (0U - param_3) & 31U;
    unsigned int i;

    for (i = 0; i != 33; ++i)
        compact[i] = compress_87860(rd32(entry + i * 4));

    for (i = 0; i != 8; ++i)
        groups[i] = group_87860(compact + i * 4);
    groups[8] = compact[32];

    for (i = 0; i != 8; ++i) {
        uint32_t v = (groups[i] >> right) | (groups[i + 1] << left);
        wr32((uint8_t *)param_1 + i * 4, v);
    }
}


static uint8_t convert_word(uint64_t word) {
    uint32_t low = (uint32_t)word;
    uint32_t high = (uint32_t)(word >> 32);
    uint32_t left = (low | (low >> 4)) & 0x0f0f0f0fU;
    uint32_t right = ((uint32_t)(word >> 28) | high) & 0xf0f0f0f0U;
    uint32_t mask = left | right;

    mask |= mask >> 2;
    mask |= mask >> 1;
    mask &= 0x11111111U;
    mask |= mask >> 7;
    mask |= mask >> 14;
    return (uint8_t)mask;
}

void gpu2d_bg_text_pack_mask_words32(uint8_t *dest, const uint8_t *origin) {

    uintptr_t end_origin = (uintptr_t)origin + 0x100U;
    uintptr_t end_dest = (uintptr_t)dest + 0x20U;

    if (end_origin <= (uintptr_t)dest ||
        end_dest <= (uintptr_t)origin) {
        uint32_t idx;

        for (idx = 0; idx != 0x20U; idx += 2) {
            uint64_t first_row = rd64(origin);
            uint64_t second = rd64(origin + 8);

            origin += 16;
            dest[idx + 1] = convert_word(second);
            dest[idx] = convert_word(first_row);
        }
    } else {
        uint32_t idx;

        for (idx = 0; idx != 0x20U; ++idx) {
            uint64_t word = rd64(origin + ((size_t)idx << 3));
            dest[idx] = convert_word(word);
        }
    }
}



static uint8_t convert_group(uint64_t first, uint64_t second)
{
    uint32_t low;
    uint32_t height;
    uint32_t blend;

    low = ((uint32_t)first & 0x000000ffU) |
            ((uint32_t)(first >> 8) & 0x0000ff00U) |
            ((uint32_t)(first >> 16) & 0x00ff0000U) |
            ((uint32_t)(first >> 24) & 0xff000000U);
    height = ((uint32_t)second & 0x000000ffU) |
            ((uint32_t)(second >> 8) & 0x0000ff00U) |
            ((uint32_t)(second >> 16) & 0x00ff0000U) |
            ((uint32_t)(second >> 24) & 0xff000000U);

    low = (low | (low >> 4)) & 0x0f0f0f0fU;
    height = (height | (height << 4)) & 0xf0f0f0f0U;
    blend = low | height;
    blend |= blend >> 2;
    blend |= blend >> 1;
    blend &= 0x11111111U;
    blend |= blend >> 7;
    blend |= blend >> 14;
    return (uint8_t)blend;
}

uint8_t *gpu2d_bg_text_pack_4bit_pixels_from_planes(uint8_t *dest, const uint8_t *origin)
{

    uint32_t i;
    uintptr_t dir_dest = (uintptr_t)dest;
    uintptr_t dir_origin = (uintptr_t)origin;

    if (dir_origin + 0x200U <= dir_dest ||
        dir_dest + 0x20U <= dir_origin) {
        for (i = 0; i != 32; i++) {
            uint64_t first = rd64(origin + i * 16);
            uint64_t second = rd64(origin + i * 16 + 8);
            wr8(dest + i, convert_group(first, second));
        }
    } else {
        for (i = 0; i != 32; i++) {
            uint64_t first = rd64(origin + i * 16);
            uint64_t second = rd64(origin + i * 16 + 8);
            wr8(dest + i, convert_group(first, second));
        }
    }

    return dest;
}

void gpu2d_bg_text_translate_bytes_to_halfwords256(unsigned char *output, const unsigned char *entry,
                        const unsigned char *table) {
    for (uint64_t i = 0; i < 0x100; i++) {
        uint32_t idx = entry[i];
        uint16_t v;
        memcpy(&v, table + idx * 2, 2);
        memcpy(output + i * 2, &v, 2);
    }
}

void gpu2d_bg_text_translate_halfwords256(unsigned char *output, const unsigned char *entry,
                        const unsigned char *table) {
    for (uint64_t i = 0; i < 0x200; i += 2) {
        uint16_t idx;
        memcpy(&idx, entry + i, 2);
        uint16_t v;
        memcpy(&v, table + (uint64_t)idx * 2, 2);
        memcpy(output + i, &v, 2);
    }
}

extern void gpu2d_bg_text_pack_blend_window_21(uint16_t *out_px, uint8_t *out_height,
                               const uint16_t *block_a, const uint16_t *block_b,
                               int k, int blend) __asm__("gpu2d_bg_text_pack_blend_window");
extern void gpu2d_bg_text_pack_blend_window_wide_21(uint16_t *out_px, uint8_t *out_height,
                               const uint16_t *block_a, const uint16_t *block_b,
                               int k, int blend) __asm__("gpu2d_bg_text_pack_blend_window_wide");
extern void gpu2d_bg_text_decode_4bpp_line_21(uint8_t *out_color, uint8_t *out_idx,
                               const uint8_t *palettes, const uint8_t *tiledata,
                               const uint16_t *map, const uint32_t *attrs) __asm__("gpu2d_bg_text_decode_4bpp_line");
extern void gpu2d_bg_text_decode_8bpp_indices_line_21(uint8_t *output, const uint8_t *tiledata,
                               const uint16_t *map, const uint32_t *attrs) __asm__("gpu2d_bg_text_decode_8bpp_indices_line");
extern void gpu2d_bg_text_decode_8bpp_line_21(uint8_t *output, const uint8_t *tiledata,
                               const uint16_t *map, const uint32_t *attrs) __asm__("gpu2d_bg_text_decode_8bpp_line");
extern void gpu2d_bg_text_mask_4bpp_line_shifted_21(uint8_t *output, const uint8_t *indices, int shift) __asm__("gpu2d_bg_text_mask_4bpp_line_shifted");
extern void gpu2d_planes_mask_from_bytes(uint8_t *mask, const uint8_t *pixels);
extern void gpu2d_planes_mask_from_pixels_lowbyte(uint8_t *mask, const uint16_t *pixels);
extern void gpu2d_palette_lookup_line_idx8(uint32_t *dst, const unsigned char *src,
                               const unsigned char *table, int32_t n);
extern void gpu2d_palette_lookup_line_idx12(uint32_t *dst, const uint16_t *src,
                               const unsigned char *table, int32_t n);
#define BUF_B      0x50
#define BUF_C      0x80
#define DST_ALT    0x110




void gpu2d_bg_text_draw_tile_line(gpu2d_bg_t *ctx, uint8_t *dst, uint8_t *out, uint32_t shift) {

    uint8_t stack[320] __attribute__((aligned(16)));
    uint8_t *tmp   = stack;
    uint8_t *tmp_b = tmp + BUF_B;

    uint32_t row  = (uint32_t)ctx->vofs + shift;
    uint8_t *bank = (uint8_t *)ctx->vram_window;
    uint32_t ctrl2 = ctx->hofs;
    uint32_t a     = ctx->screen_base;
    uint32_t map_o = ctx->char_base;
    uint32_t ctrl  = ctx->bgcnt;

    if ((row & (1u << 8)) && (ctrl & (1u << 15)))
        a += (ctrl & 0x4000u) ? 0x1000u : 0x800u;

    uint8_t *map = bank + map_o;
    a += (row << 3) & 0x7c0u;

    uint32_t sub = ctrl2 & 7u;

    uint32_t p1 = a, p2 = a;
    if (ctrl & (1u << 14)) {
        uint32_t alt = a + 0x800u;
        if (ctrl2 > 0xffu) { p1 = alt; p2 = a;   }
        else               { p1 = a;   p2 = alt; }
    }
    uint8_t *src1 = bank + p1;
    uint8_t *src2 = bank + p2;

    uint32_t pal = (ctrl2 >> 3) & 0x1fu;
    uint32_t fx  = row & 7u;

    if (!(ctrl & (1u << 7))) {
        uint8_t *tmp_c = tmp + BUF_C;

        gpu2d_bg_text_pack_blend_window((uint16_t *)tmp, tmp_b, (const uint16_t *)src1,
                           (const uint16_t *)src2, (int)pal, (int)(fx << 2));

        gpu2d_bg_text_decode_4bpp_line(dst - (uint64_t)sub * 2, tmp_c,
                           (const uint8_t *)ctx->palette,
                           (const uint8_t *)map, (const uint16_t *)tmp,
                           (const uint32_t *)tmp_b);

        gpu2d_bg_text_mask_4bpp_line_shifted(out, (const uint8_t *)tmp_c, (int)sub);
        return;
    }

    gpu2d_bg_text_pack_blend_window_wide((uint16_t *)tmp, tmp_b, (const uint16_t *)src1,
                       (const uint16_t *)src2, (int)pal, (int)(fx << 3));

    if (ctx->ext_palette_enabled != 0) {
        void *alt = ctx->ext_palette;
        if (!alt) return;

        gpu2d_bg_text_decode_8bpp_line(dst - (uint64_t)sub * 2, (const uint8_t *)map,
                           (const uint16_t *)tmp, (const uint32_t *)tmp_b);

        gpu2d_planes_mask_from_pixels_lowbyte(out, (const uint16_t *)dst);

        gpu2d_palette_lookup_line_idx12((uint32_t *)dst, (const uint16_t *)dst,
                           (const unsigned char *)alt, 0x100);
    } else {
        uint8_t *dst2 = dst + DST_ALT;

        gpu2d_bg_text_decode_8bpp_indices_line(dst2 - sub, (const uint8_t *)map,
                           (const uint16_t *)tmp, (const uint32_t *)tmp_b);

        gpu2d_planes_mask_from_bytes(out, (const uint8_t *)dst2);

        gpu2d_palette_lookup_line_idx8((uint32_t *)dst, (const unsigned char *)dst2,
                           (const unsigned char *)ctx->palette, 0x100);
    }
}
#undef BUF_B
#undef BUF_C
#undef DST_ALT

#define BITS 256
#define WORDS (BITS / 32)

void gpu2d_bg_text_mask_bit_range256(uint32_t *map, unsigned from, unsigned until) {

    unsigned pal_from = from >> 5, pal_until = until >> 5;

    for (unsigned k = 0; k < WORDS; k++) {
        uint32_t m = 0xFFFFFFFFu;

        if (k < pal_from)       m = 0;
        else if (k == pal_from) m &= (0xFFFFFFFFu << (from & 31));

        if (k > pal_until)       m = 0;
        else if (k == pal_until) m &= ~(0xFFFFFFFEu << (until & 31));

        map[k] &= m;
    }
}
#undef BITS
#undef WORDS

void gpu2d_bg_text_copy_batches_row_lut(unsigned char *param_1, unsigned char *param_2,
                         unsigned char *param_3, unsigned char *param_4,
                         int32_t param_5, unsigned char *param_6) {

    unsigned char *x0 = param_1;
    unsigned char *x1 = param_2;
    unsigned char *x2 = param_3;
    unsigned char *x3 = param_4;
    int32_t        x4 = param_5;
    unsigned char *x5 = param_6;

    uint32_t w8, w9, w10;
    unsigned char *x9;

    w8 = *x2;
    w9 = 0x100;
    if (w8 == 0) w8 = w9;

    w9 = *x3; x3++;
    x2++;

    if (w8 != 0) goto copy;

loop:

    w8 = *x2;
    x4 = x4 - 1;
    if (x4 == 0) return;

    w9 = *x3; x3++;
    x2++;

    if (w8 == 0) goto loop;

copy:

    x9 = x5 + ((uint64_t)w9 << 6);

    do {

        w10 = *x1; x1++;
        w8 = w8 - 1;
        w10 = x9[w10];
        *x0 = (unsigned char)w10; x0++;
    } while (w8 != 0);

    goto loop;
}

void gpu2d_bg_text_paint_lines_palette_xor_byte(uint8_t *dest, const uint8_t *origin,
                        const uint8_t *counts, const uint8_t *indices,
                        const uint8_t *keys, uint32_t lines,
                        const uint8_t *palettes)
{

    uint32_t count;
    uint32_t key;
    uint32_t idx;
    uint16_t h;

    count = counts[0];
    if (count == 0) count = 0x100u;

    key = *keys++;
    memcpy(&h, indices, 2); indices += 2;
    idx = h;
    counts += 1;

    if (count != 0) goto paint;

head:
    count = counts[0];
    lines -= 1;
    if (lines == 0) return;

    key = *keys++;
    memcpy(&h, indices, 2); indices += 2;
    idx = h;
    counts += 1;

    if (count == 0) goto head;

paint:
    {

        const uint8_t *table = palettes + (uint64_t)((idx << 6) & 0xffc0u);

        do {
            uint32_t b = *origin++;
            count -= 1;
            b ^= key;
            b &= 0xffu;
            *dest++ = table[b];
        } while (count != 0);
    }
    goto head;
}

void gpu2d_bg_text_paint_lines_palette_xor_halfword(uint8_t *dest, const uint8_t *origin,
                        const uint8_t *counts, const uint8_t *indices,
                        const uint8_t *keys, uint32_t lines,
                        const uint8_t *palettes)
{

    uint32_t count;
    uint32_t key;
    uint32_t idx;
    uint16_t h;

    count = counts[0];
    if (count == 0) count = 0x100u;

    key = *keys++;
    memcpy(&h, indices, 2); indices += 2;
    idx = h;
    counts += 1;

    if (count != 0) goto paint;

head:
    count = counts[0];
    lines -= 1;
    if (lines == 0) return;

    key = *keys++;
    memcpy(&h, indices, 2); indices += 2;
    idx = h;
    counts += 1;

    if (count == 0) goto head;

paint:
    {

        const uint8_t *table = palettes + (uint64_t)((idx << 6) & 0xffc0u);

        uint32_t height = (idx >> 4) & 0xf00u;

        do {
            uint32_t b = *origin++;
            count -= 1;
            b ^= key;
            b &= 0xffu;
            b = table[b];
            b |= height;

            h = (uint16_t)b;
            memcpy(dest, &h, 2);
            dest += 2;
        } while (count != 0);
    }
    goto head;
}


void gpu2d_bg_text_gen_ramp_bytes(uint8_t *dest, uint32_t count,
                         uint32_t initial, uint32_t step)
{

    for (uint32_t idx = 0; idx < count; ++idx) {
        uint32_t value = initial + idx * step;
        wr8(dest + idx, (uint8_t)(value >> 20));
    }
}

static uint32_t asr11(uint32_t value)
{
    if ((value & UINT32_C(0x80000000)) != 0)
        return (value >> 11) | UINT32_C(0xffe00000);
    return value >> 11;
}


void gpu2d_bg_text_gen_ramp_span_bytes(int32_t span, uint32_t phase, int32_t advance,
                         uint8_t *dest, uint32_t step)
{

    if (span == 0)
        return;

    uint32_t end_phase = phase + (uint32_t)advance * (uint32_t)span;
    uint32_t int_initial = asr11(phase);
    uint32_t int_final = asr11(end_phase);
    uint32_t distance;
    uint32_t count;

    if (((uint32_t)span & UINT32_C(0x80000000)) != 0) {
        distance = (phase & UINT32_C(0x7ff)) - (uint32_t)span;
        count = int_initial - int_final;
    } else {
        distance = (uint32_t)span - (phase & UINT32_C(0x7ff)) + UINT32_C(0x7ff);
        count = int_final - int_initial;
    }

    if (count == 0)
        return;

    uint32_t value = (uint32_t)(((uint64_t)distance * step) >> 11);
    uint32_t blocks = count & UINT32_C(0xfffffff0);
    uint32_t idx = 0;

    if (count >= 16) {
        while (idx < blocks) {
            wr8(dest + idx, (uint8_t)(value >> 20));
            value += step;
            ++idx;
        }
    }

    while (idx < count) {
        wr8(dest + idx, (uint8_t)(value >> 20));
        value += step;
        ++idx;
    }
}

int gpu2d_bg_text_dedup_consecutive_bytes(unsigned char *param_1, int param_2) {

    if (param_2 == 0) {
        return 0;
    }

    unsigned char *pbVar3 = param_1;
    unsigned char *write_fn = param_1;
    unsigned uVar4 = 0x100;
    int iVar2 = 0;

    do {
        unsigned char bVar1 = *pbVar3;
        pbVar3 = pbVar3 + 1;

        unsigned char *candidate = write_fn;
        *candidate = bVar1;
        candidate = candidate + 1;

        if (uVar4 != (unsigned)bVar1) {
            iVar2 = iVar2 + 1;
            write_fn = candidate;
        }

        param_2 = param_2 - 1;
        uVar4 = (unsigned)bVar1;
    } while (param_2 != 0);

    return iVar2;
}



static uint16_t blend(uint32_t first, uint32_t second, uint32_t first_multiplier,
                       uint32_t multiplier_second, uint32_t mask, uint32_t shift,
                       uint32_t sample)
{
    uint32_t low = mask & ((sample * first_multiplier + first) >> 11);
    uint32_t height = mask & ((sample * multiplier_second + second) >> 11);
    return (uint16_t)((height << (shift & 31u)) + low);
}

void gpu2d_bg_text_pack_dual_linear_mask(const uint8_t *origin, void *dest, uint32_t count,
                        uint32_t bias_first, uint32_t bias_second,
                        uint32_t first_multiplier, uint32_t multiplier_second,
                        uint32_t mask, uint32_t shift)
{

    uint8_t *output = (uint8_t *)dest;
    wr16(output, blend(bias_first, bias_second, first_multiplier,
                         multiplier_second, mask, shift, 0u));

    for (uint32_t i = 0; i < count; i++) {
        uint8_t sample = rd8(origin + i);
        wr16(output + 2u + (uint64_t)i * 2u,
              blend(bias_first, bias_second, first_multiplier,
                     multiplier_second, mask, shift, sample));
    }
}




static void write128(void *p, const uint16_t v[8])
{
    memcpy(p, v, 16);
}

static uint16_t blend_30(uint32_t bias_low, uint32_t bias_height,
                       uint32_t step_low, uint32_t step_height,
                       uint32_t mask, uint32_t shift, uint8_t sample)
{
    uint32_t low;
    uint32_t height;

    low = (uint32_t)sample * step_low + bias_low;
    height = (uint32_t)sample * step_height + bias_height;
    height = mask & (height >> 10);
    low = mask & (low >> 10);
    height <<= shift & 31u;
    return (uint16_t)(height + low);
}

void gpu2d_bg_text_pack_dual_linear_mask_doubled(const uint8_t *origin, void *dest, uint32_t count,
                        uint32_t bias_low, uint32_t bias_height,
                        uint32_t step_low, uint32_t step_height,
                        uint32_t mask, uint32_t shift)
{

    uint8_t *output = (uint8_t *)dest;
    uint32_t double_mask = mask << 1;
    uint32_t height_initial = double_mask & (bias_height >> 10);
    uint32_t low_initial = double_mask & (bias_low >> 10);
    uint32_t idx = 0;
    uintptr_t start_origin = (uintptr_t)origin;
    uintptr_t start_output = (uintptr_t)output;

    wr16(output, (uint16_t)((height_initial << (shift & 31u)) + low_initial));

    if (count >= 8u &&
        start_origin + (uintptr_t)count > start_output + 2u &&
        start_output + (uintptr_t)count * 2u + 2u > start_origin) {
        uint32_t limit_width = count & ~7u;

        while (idx != limit_width) {
            uint64_t eight_samples = rd64(origin + idx);
            uint16_t converted[8];
            uint32_t lane;

            for (lane = 0; lane != 8u; lane++) {
                uint8_t sample = (uint8_t)(eight_samples >> (lane * 8u));
                converted[lane] = blend_30(bias_low, bias_height, step_low,
                                              step_height, double_mask, shift,
                                              sample);
            }
            write128(output + 2u + (uint64_t)idx * 2u, converted);
            idx += 8u;
        }
    }

    while (idx != count) {
        uint8_t sample = rd8(origin + idx);

        wr16(output + 2u + (uint64_t)idx * 2u,
              blend_30(bias_low, bias_height, step_low, step_height,
                     double_mask, shift, sample));
        idx++;
    }
}


static uint32_t ushl32(uint32_t val, uint32_t shift_reg)
{
    int8_t amt = (int8_t)(shift_reg & 0xffu);
    if (amt >= 0) {
        if (amt >= 32) return 0u;
        return val << (unsigned)amt;
    } else {
        unsigned neg = (unsigned)(-(int)amt);
        if (neg >= 32) return 0u;
        return val >> neg;
    }
}

void gpu2d_bg_text_gen_texindex_table256(void *out, uint32_t u0, uint32_t v0,
                         uint32_t du, uint32_t dv, uint32_t mask,
                         uint32_t shift_reg)
{

    unsigned char *o = (unsigned char *)out;
    uint32_t U = u0, V = v0;

    for (int i = 0; i < 256; i++) {
        uint32_t iu  = (U >> 11) & mask;
        uint32_t iv  = (V >> 11) & mask;
        uint32_t ivs = ushl32(iv, shift_reg);
        uint32_t idx = iu + ivs;

        wr16(o + (size_t)i * 2u, (uint16_t)idx);

        U += du;

        V += dv;
    }
}


static uint32_t ushl32_32(uint32_t value, uint32_t reg)
{
    int8_t count = (int8_t)(reg & 0xffu);

    if (count >= 0) {
        if (count >= 32)
            return 0u;
        return value << (unsigned)count;
    }

    {
        unsigned right = (unsigned)(-(int)count);
        if (right >= 32u)
            return 0u;
        return value >> right;
    }
}

void gpu2d_bg_text_gen_texindex256(void *out, uint32_t u0, uint32_t v0,
                         uint32_t du, uint32_t dv, uint32_t mask,
                         uint32_t reg_offset)
{

    unsigned char *output = (unsigned char *)out;
    uint32_t u = u0;
    uint32_t v = v0;

    for (unsigned i = 0; i < 256u; ++i) {
        uint32_t index_u = (u >> 11) & mask;
        uint32_t index_v = (v >> 11) & mask;
        uint32_t idx = index_u + ushl32_32(index_v, reg_offset);
        uint16_t short_form = (uint16_t)idx;

        wr16(output + (size_t)i * 2u, (uint16_t)(short_form << 1));
        u += du;
        v += dv;
    }
}

void gpu2d_bg_text_delta_encode_inplace(unsigned char *param_1, unsigned int param_2, char param_3)
{

    param_1[param_2] = (unsigned char)(param_3 + 1);

    unsigned int n = param_2 + 2u;
    if (n == 0) {
        return;
    }

    unsigned char prev = 0;
    for (unsigned int i = 0; i < n; i++) {
        unsigned char cur = param_1[i];
        param_1[i] = (unsigned char)(cur - prev);
        prev = cur;
    }
}

void gpu2d_bg_text_gen_dual_accum_bytes(unsigned char *output, uint32_t a, uint32_t b,
                        uint32_t pa, uint32_t pb, uint32_t n) {
    uint32_t i = 0;
    do {
        uint32_t v = ((b >> 5) & 0x38u) | ((a >> 8) & 7u);
        output[i] = (unsigned char)v;
        i += 1;
        a += pa;
        b += pb;
    } while (i <= n);
}

void gpu2d_bg_text_translate_inplace_halfword_to_byte(unsigned char *buf, const unsigned char *table,
                        uint32_t n) {
    uint32_t i = 0;
    do {
        uint16_t idx;
        memcpy(&idx, buf + (uint64_t)i * 2, 2);
        uint32_t j = i;
        i += 1;
        buf[j] = table[idx];
    } while (i <= n);
}

void gpu2d_bg_text_translate_inplace_halfwords(unsigned char *buf, const unsigned char *table,
                        uint32_t n) {
    uint32_t i = 0;
    do {
        uint64_t off = (uint64_t)i * 2;
        uint16_t idx;
        memcpy(&idx, buf + off, 2);
        i += 1;
        uint16_t v;
        memcpy(&v, table + idx, 2);
        memcpy(buf + off, &v, 2);
    } while (i <= n);
}

void gpu2d_bg_text_translate_halfword_to_byte_flagged(unsigned char *output, const unsigned char *entry,
                        uint32_t n) {
    uint32_t i = 0;
    do {
        uint16_t h;
        memcpy(&h, entry + (uint64_t)i * 2, 2);
        uint32_t v = h;
        uint32_t sign = (uint32_t)(-(int32_t)((v >> 10) & 1u));
        uint32_t a = sign & 7u;
        uint32_t b = (0x38u & ~7u) | (sign & 7u);
        uint32_t j = i;
        i += 1;
        output[j] = (unsigned char)(((v & 0x800u) == 0) ? a : b);
    } while (i <= n);
}

#define OUTPUTS 40
#define HALF   32

void gpu2d_bg_text_pack_blend_window(uint16_t *out_px, uint8_t *out_height,
                        const uint16_t *block_a, const uint16_t *block_b,
                        int k, int blend) {

    uint16_t F[HALF * 2];
    for (int i = 0; i < HALF; i++) F[i]         = block_a[i];
    for (int i = 0; i < HALF; i++) F[HALF + i] = block_b[i];

    int base = (k & ~7);
    int r    = (k & 7);

    uint16_t px[OUTPUTS];
    for (int i = 0; i < HALF; i++) px[i] = F[k + i];

    for (int j = 0; j < 8; j++)   px[HALF + j] = F[base + HALF + ((r + j) & 7)];

    uint16_t a = (uint16_t)blend;
    uint16_t b = (uint16_t)(28 - blend);

    for (int i = 0; i < OUTPUTS; i++) {
        uint16_t sel = (px[i] & 0x0800) ? b : a;
        uint16_t v = (uint16_t)((px[i] << 5) | (sel & 0x1f));
        out_px[i]   = (uint16_t)(v & 0x7fff);
        out_height[i] = (uint8_t)(px[i] >> 8);
    }

    for (int j = 0; j < 8; j++) out_height[OUTPUTS + j] = 0;
}
#undef OUTPUTS
#undef HALF

#define OUTPUTS 40
#define HALF   32

void gpu2d_bg_text_pack_blend_window_wide(uint16_t *out_px, uint8_t *out_height,
                        const uint16_t *block_a, const uint16_t *block_b,
                        int k, int blend) {

    uint16_t F[HALF * 2];
    for (int i = 0; i < HALF; i++) F[i]         = block_a[i];
    for (int i = 0; i < HALF; i++) F[HALF + i] = block_b[i];

    int base = (k & ~7);
    int r    = (k & 7);

    uint16_t px[OUTPUTS];
    for (int i = 0; i < HALF; i++) px[i] = F[k + i];
    for (int j = 0; j < 8; j++)   px[HALF + j] = F[base + HALF + ((r + j) & 7)];

    uint16_t a = (uint16_t)blend;
    uint16_t b = (uint16_t)(56 - blend);

    for (int i = 0; i < OUTPUTS; i++) {
        uint16_t sel = (px[i] & 0x0800) ? b : a;
        out_px[i]   = (uint16_t)((px[i] << 6) | (sel & 0x3f));
        out_height[i] = (uint8_t)(px[i] >> 8);
    }
    for (int j = 0; j < 8; j++) out_height[OUTPUTS + j] = 0;
}
#undef OUTPUTS
#undef HALF

#define TILES   33
#define GROUPS  8

static uint32_t flip(uint32_t d) {
    uint32_t r = 0;
    for (int p = 0; p < 8; p++) r |= ((d >> (4 * (7 - p))) & 0xfu) << (4 * p);
    return r;
}


void gpu2d_bg_text_decode_4bpp_line(uint8_t *out_color, uint8_t *out_idx,
                        const uint8_t *palettes, const uint8_t *tiledata,
                        const uint16_t *map, const uint32_t *attrs) {

    for (int v = 0; v < GROUPS; v++) {
        uint32_t attr = attrs[v];
        uint32_t data[4];

        for (int t = 0; t < 4; t++) {
            uint32_t d = rd32_at(tiledata, map[v * 4 + t]);
            if (attr & (4u << (8 * t))) d = flip(d);
            data[t] = d;
        }

        memcpy(out_idx, data, 16);
        out_idx += 16;

        for (int t = 0; t < 4; t++) {
            const uint8_t *pal = palettes + (((attr >> (8 * t + 4)) & 0xfu) << 5);
            for (int p = 0; p < 8; p++) {
                unsigned idx = (data[t] >> (4 * p)) & 0xfu;
                memcpy(out_color, pal + idx * 2, 2);
                out_color += 2;
            }
        }
    }

    {
        uint32_t attr = attrs[GROUPS];
        uint32_t d = rd32_at(tiledata, map[GROUPS * 4]);

        uint32_t w0 = (attr & 0x00000004u) ? flip(d) : d;
        uint32_t w1 = (attr & 0x00000400u) ? flip(d) : d;

        memcpy(out_idx,     &w0, 4);
        memcpy(out_idx + 4, &w1, 4);

        const uint8_t *pal = palettes + (((attr >> 4) & 0xfu) << 5);
        for (int p = 0; p < 8; p++) {
            unsigned idx = (w0 >> (4 * p)) & 0xfu;
            memcpy(out_color, pal + idx * 2, 2);
            out_color += 2;
        }
    }
}
#undef TILES
#undef GROUPS

#define GROUPS 8

static void un_tile(uint8_t *output, const uint8_t *tiledata, uint16_t entry,
                    int flip) {
    uint8_t d[8];
    memcpy(d, tiledata + entry, 8);
    for (int p = 0; p < 8; p++)
        output[p] = flip ? d[7 - p] : d[p];
}

void gpu2d_bg_text_decode_8bpp_indices_line(uint8_t *output, const uint8_t *tiledata,
                        const uint16_t *map, const uint32_t *attrs) {

    for (int v = 0; v < GROUPS; v++) {
        uint32_t attr = attrs[v];
        for (int t = 0; t < 4; t++) {
            un_tile(output, tiledata, map[v * 4 + t],
                    (attr & (4u << (8 * t))) != 0);
            output += 8;
        }
    }

    {
        const uint8_t *ab = (const uint8_t *)&attrs[GROUPS];
        un_tile(output, tiledata, map[GROUPS * 4], (*ab & 4u) != 0);
    }
}
#undef GROUPS

#define TILES  33
#define GROUPS 8

static void un_tile_42(uint8_t *output, const uint8_t *tiledata, uint16_t entry,
                    int flip, unsigned palette) {
    uint8_t d[8];
    memcpy(d, tiledata + entry, 8);

    for (int p = 0; p < 8; p++) {
        uint8_t v = flip ? d[7 - p] : d[p];
        output[2 * p]     = v;
        output[2 * p + 1] = (uint8_t)palette;
    }
}

void gpu2d_bg_text_decode_8bpp_line(uint8_t *output, const uint8_t *tiledata,
                        const uint16_t *map, const uint32_t *attrs) {

    for (int v = 0; v < GROUPS; v++) {
        uint32_t attr = attrs[v];
        for (int t = 0; t < 4; t++) {
            un_tile_42(output, tiledata, map[v * 4 + t],
                    (attr & (4u << (8 * t))) != 0,
                    (attr >> (8 * t + 4)) & 0xfu);
            output += 16;
        }
    }

    {
        uint32_t attr = attrs[GROUPS];
        un_tile_42(output, tiledata, map[GROUPS * 4],
                (attr & 4u) != 0, (attr >> 4) & 0xfu);
    }
}
#undef TILES
#undef GROUPS

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#define INPUT 136
#define PIXELS (INPUT * 2)
#define OUTPUT  32
#ifndef __ARM_NEON

static uint8_t ushl(uint8_t x, int count) {
    if (count >= 0) return count >= 8 ? 0 : (uint8_t)(x << count);
    count = -count;
    return count >= 8 ? 0 : (uint8_t)(x >> count);
}
#endif

void gpu2d_bg_text_mask_4bpp_line_shifted(uint8_t *output, const uint8_t *indices, int shift) {

#ifdef __ARM_NEON

    {
        static const uint8_t SEL[16] = { 0x02, 0x08, 0x20, 0x80, 0x02, 0x08, 0x20, 0x80,
                                         0x02, 0x08, 0x20, 0x80, 0x02, 0x08, 0x20, 0x80 };
        static const uint8_t WEIGHTS[16] = { 0x03, 0x0c, 0x30, 0xc0, 0x03, 0x0c, 0x30, 0xc0,
                                         0x03, 0x0c, 0x30, 0xc0, 0x03, 0x0c, 0x30, 0xc0 };
        const uint8x16_t vsel = vld1q_u8(SEL), vweight = vld1q_u8(WEIGHTS);
        const uint8x16_t nlow = vdupq_n_u8(0x0f), nhigh = vdupq_n_u8(0xf0);
        const int8x16_t d30 = vdupq_n_s8((int8_t)(-shift));
        const int8x16_t d31 = vdupq_n_s8((int8_t)(8 - shift));
        uint8x16_t v[9];
        for (int k = 0; k < 8; k++) v[k] = vld1q_u8(indices + k * 16);
        v[8] = vcombine_u8(vld1_u8(indices + 128), vdup_n_u8(0));
        for (int k = 0; k < 9; k++) {
            uint8x16_t b = vtstq_u8(v[k], nlow);
            uint8x16_t a = vtstq_u8(v[k], nhigh);
            v[k] = vandq_u8(vbslq_u8(vsel, a, b), vweight);
        }
        uint8x16_t p0 = vpaddq_u8(v[0], v[1]);
        uint8x16_t p1 = vpaddq_u8(v[2], v[3]);
        uint8x16_t p2 = vpaddq_u8(v[4], v[5]);
        uint8x16_t p3 = vpaddq_u8(v[6], v[7]);
        uint8x16_t p4 = vpaddq_u8(v[8], v[8]);
        uint8x16_t q0 = vpaddq_u8(p0, p1);
        uint8x16_t q1 = vpaddq_u8(p2, p3);
        uint8x16_t q2 = vpaddq_u8(p4, p4);
        uint8x16_t e0 = vextq_u8(q0, q1, 1);
        uint8x16_t e1 = vextq_u8(q1, q2, 1);
        q0 = vorrq_u8(vshlq_u8(q0, d30), vshlq_u8(e0, d31));
        q1 = vorrq_u8(vshlq_u8(q1, d30), vshlq_u8(e1, d31));
        vst1q_u8(output, q0);
        vst1q_u8(output + 16, q1);
        return;
    }
#else

    uint8_t m[PIXELS / 8];
    for (int b = 0; b < PIXELS / 8; b++) m[b] = 0;
    for (int p = 0; p < PIXELS; p++) {
        unsigned nib = (indices[p >> 1] >> (4 * (p & 1))) & 0xfu;
        if (nib) m[p >> 3] |= (uint8_t)(1u << (p & 7));
    }

    for (int k = 0; k < OUTPUT; k++)
        output[k] = (uint8_t)(ushl(m[k], -shift) | ushl(m[k + 1], 8 - shift));
#endif
}
#undef INPUT
#undef PIXELS
#undef OUTPUT

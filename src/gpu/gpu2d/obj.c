#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "mem_access.h"







static uint32_t rev32(uint32_t v)
{
    return (v >> 24) | ((v >> 8) & 0xff00u) | ((v << 8) & 0xff0000u) | (v << 24);
}

static uint32_t ext16(uint32_t v)
{
    uint32_t t = v & 0xffffu;
    return (t & 0x8000u) ? (t | 0xffff0000u) : t;
}

static uint8_t *pix(uint8_t *px, int32_t n) { return px + 2 * (ptrdiff_t)n; }

uint32_t gpu2d_obj_draw_line(gpu2d_engine_t *ctx, uint8_t *px, uint8_t *atr,
                            uint8_t *layers, uint8_t *m_bit7, uint8_t *m_bits05,
                            uint32_t line)
{

    uint8_t  pad[8 + 256 + 264];
    uint8_t *padded = pad + 8;

    uint16_t off[512];
    uint8_t  shift[512];

    uint32_t ret = 0;
    long layer;
    unsigned i, b;

    for (layer = 4; ; layer--) {
        uint8_t *msk = layers + layer * 32;

        unsigned count = ctx->sprite_count[layer][line];
        long k;

        if (count == 0) {
            for (i = 0; i < 32; i++) msk[i] = 0;
            if (layer == 0) break;
            continue;
        }

        for (i = 0; i < 256; i++) padded[i] = 0;

        for (k = (long)count - 1; k >= 0; k--) {

            unsigned idx = ctx->sprite_list[layer][line][k];
            const gpu2d_sprite_t *s = &ctx->sprite[idx];

            unsigned type = s->kind;
            int32_t  sx;
            uint32_t step;
            unsigned width;
            uint8_t  attr;
            int32_t  dy;
            uint32_t row;
            const uint8_t *dat, *pal, *p;
            uint32_t j, w0, w1, base;

            if (type > 10) continue;
            if (type == 3 || type == 7) continue;

            sx       = s->x;
            step     = s->row_stride;
            width     = s->width;
            attr = s->attribute;

            if (type <= 6) {

                dy   = (int32_t)line - (int32_t)s->y;
                row = (s->flip_v != 0) ? (uint32_t)(-dy) : (uint32_t)dy;
                if (width == 0) continue;
            }

            switch (type) {

            case 0:
                dat  = s->data;
                pal  = s->palette;
                base = (row >> 3) * step + ((row << 2) & 0x1cu);
                for (j = 0; j < width; j += 8) {
                    w0 = rd32(dat + base + (size_t)j * 4);
                    for (b = 0; b < 8; b++) {
                        unsigned ix = (w0 >> (4 * b)) & 0xfu;
                        if (ix) {
                            wr16(pix(px, sx + (int32_t)(j + b)),
                                  rd16(pal + 2u * ix));
                            padded[sx + (int32_t)(j + b)] = (uint8_t)ix;
                            atr[sx + (int32_t)(j + b)] = attr;
                        }
                    }
                }
                break;

            case 4:
                dat  = s->data;
                pal  = s->palette;
                base = (row >> 3) * step + ((row & 7u) << 2);
                p    = dat + base;
                for (j = 0; j < width; j += 8) {
                    uint32_t t = rd32(p);
                    p -= 32;
                    t = rev32(((t << 4) & 0xf0f0f0f0u) | ((t >> 4) & 0x0f0f0f0fu));
                    for (b = 0; b < 8; b++) {
                        unsigned ix = (t >> (4 * b)) & 0xfu;
                        if (ix) {
                            wr16(pix(px, sx + (int32_t)(j + b)),
                                  rd16(pal + 2u * ix));
                            padded[sx + (int32_t)(j + b)] = (uint8_t)ix;
                            atr[sx + (int32_t)(j + b)] = attr;
                        }
                    }
                }
                break;

            case 1:
                dat  = s->data;
                pal  = s->palette;
                base = (row >> 3) * step + ((row & 7u) << 3);
                p    = dat + base;
                for (j = 0; j < width; j += 8) {
                    w0 = rd32(p);
                    w1 = rd32(p + 4);
                    for (b = 0; b < 8; b++) {
                        unsigned ix = (b < 4 ? (w0 >> (8 * b))
                                             : (w1 >> (8 * (b - 4)))) & 0xffu;
                        if (ix) {
                            wr16(pix(px, sx + (int32_t)(j + b)),
                                  rd16(pal + 2u * ix));
                            padded[sx + (int32_t)(j + b)] = (uint8_t)ix;
                            atr[sx + (int32_t)(j + b)] = attr;
                        }
                    }
                    p += 0x40;
                }
                break;

            case 5:
                dat  = s->data;
                pal  = s->palette;
                base = (row >> 3) * step + ((row & 7u) << 3);
                p    = dat + base;
                for (j = 0; j < width; j += 8) {
                    w0 = rev32(rd32(p));
                    w1 = rev32(rd32(p + 4));
                    for (b = 0; b < 8; b++) {
                        unsigned ix = (b < 4 ? (w1 >> (8 * b))
                                             : (w0 >> (8 * (b - 4)))) & 0xffu;
                        if (ix) {
                            wr16(pix(px, sx + (int32_t)(j + b)),
                                  rd16(pal + 2u * ix));
                            padded[sx + (int32_t)(j + b)] = (uint8_t)ix;
                            atr[sx + (int32_t)(j + b)] = attr;
                        }
                    }
                    p -= 0x40;
                }
                break;

            case 2:
            case 6:
                dat = s->data;
                p   = dat + (size_t)(row * step);
                for (j = 0; j < width; j++) {
                    int16_t c = rd16s(p);
                    if (c < 0) {
                        wr16(pix(px, sx + (int32_t)j), (uint16_t)c);
                        padded[sx + (int32_t)j] = (uint8_t)(((uint16_t)c) >> 8);
                        atr[sx + (int32_t)j] = attr;
                    }
                    p = (type == 2) ? p + 2 : p - 2;
                }
                break;

            case 8:
            case 9:
            case 10: {
                uint64_t A, B;
                uint32_t a0, a1, b0, b1;
                int32_t  row_idx, m, n, start, end, cnt;
                uint32_t u, v, du, dv;

                dy  = (int32_t)line - (int32_t)s->y;
                row_idx = (dy < -192) ? dy + 256 : dy;

                A  = (uint64_t)s->span_x[0] + (uint64_t)s->span_x[2] * (uint64_t)(int64_t)row_idx;
                B  = (uint64_t)s->span_y[0] + (uint64_t)s->span_y[2] * (uint64_t)(int64_t)row_idx;
                a0 = (uint32_t)(A >> 32);
                a1 = (uint32_t)((A + (uint64_t)s->span_x[1]) >> 32);
                b0 = (uint32_t)(B >> 32);
                b1 = (uint32_t)((B + (uint64_t)s->span_y[1]) >> 32);

                m = ((int32_t)b0 > (int32_t)a0) ? (int32_t)b0 : (int32_t)a0;
                n = ((int32_t)b1 < (int32_t)a1) ? (int32_t)b1 : (int32_t)a1;
                start = (m < 0) ? 0 : m;
                end = (n < (int32_t)width) ? n : (int32_t)width - 1;
                cnt = end - start + 1;
                if (cnt <= 0) continue;

                u  = (uint32_t)(uint16_t)s->origin_x + (uint32_t)(uint16_t)s->pa * (uint32_t)start
                                             + (uint32_t)(uint16_t)s->pc * (uint32_t)row_idx;
                v  = (uint32_t)(uint16_t)s->origin_y + (uint32_t)(uint16_t)s->pb * (uint32_t)start
                                             + (uint32_t)(uint16_t)s->pd * (uint32_t)row_idx;
                du = (uint16_t)s->pa;
                dv = (uint16_t)s->pb;

                for (j = 0; j < (uint32_t)cnt; j++) {
                    uint32_t uu = ext16(u), vv = ext16(v);
                    if (type == 8) {
                        off[j]  = (uint16_t)((vv >> 11) * step
                                             + ((vv >> 6) & 0x1cu)
                                             + ((uu >> 6) & 0xffe0u)
                                             + ((uu >> 9) & 3u));
                        shift[j] = (uint8_t)((uu >> 6) & 4u);
                    } else if (type == 9) {
                        off[j]  = (uint16_t)((vv >> 11) * step
                                             + ((vv >> 5) & 0x38u)
                                             + ((uu >> 5) & 0xffc0u)
                                             + ((uu >> 8) & 7u));
                    } else {
                        off[j]  = (uint16_t)((vv >> 8) * step
                                             + ((uu >> 7) & 0xfffeu));
                    }
                    u += du;
                    v += dv;
                }

                dat = s->data;
                pal = s->palette;
                for (j = 0; j < (uint32_t)cnt; j++) {
                    int32_t d = sx + start + (int32_t)j;
                    if (type == 8) {
                        unsigned ix = ((unsigned)dat[off[j]] >> shift[j]) & 0xfu;
                        if (!ix) continue;
                        wr16(pix(px, d), rd16(pal + 2u * ix));
                        padded[d] = (uint8_t)ix;
                        atr[d] = attr;
                    } else if (type == 9) {
                        unsigned ix = dat[off[j]];
                        if (!ix) continue;
                        wr16(pix(px, d), rd16(pal + 2u * ix));
                        padded[d] = (uint8_t)ix;
                        atr[d] = attr;
                    } else {
                        int16_t c = rd16s(dat + off[j]);
                        if (c >= 0) continue;
                        wr16(pix(px, d), (uint16_t)c);
                        padded[d] = (uint8_t)(((uint16_t)c) >> 8);
                        atr[d] = attr;
                    }
                }
                break;
            }

            default:
                break;
            }
        }

        for (i = 0; i < 32; i++) {
            unsigned bt = 0;
            for (b = 0; b < 8; b++)
                if (padded[i * 8 + b]) bt |= 1u << b;
            msk[i] = (uint8_t)bt;
        }
        ret = 16;

        if (layer == 0) break;
    }

    if (ctx->line_mode[line] == 0) {
        for (i = 0; i < 32; i++) { m_bits05[i] = 0; m_bit7[i] = 0; }
    } else {
        for (i = 0; i < 32; i++) {
            unsigned a05 = 0, a7 = 0;
            for (b = 0; b < 8; b++) {
                uint8_t x = atr[i * 8 + b];
                if (x & 0x3fu) a05 |= 1u << b;
                if (x & 0x80u) a7  |= 1u << b;
                atr[i * 8 + b] = (uint8_t)(x & 0x3fu);
            }
            m_bits05[i] = (uint8_t)a05;
            m_bit7[i]   = (uint8_t)a7;
        }
    }

    return ret;
}

#define N_OAM      128
#define T_SIZES     0x10e528
#define T_MASK  0x458800
#define T_PTRS    0x4587e0
extern void gpu2d_line_compute_span_range3(int32_t x, int32_t a, int32_t b, int32_t c,
                               int64_t *out_a, int64_t *out_b, int64_t *out_c);






void gpu2d_obj_scan_oam(gpu2d_engine_t *R) {

    const uint8_t *oam = R->oam;
    uint32_t disp = R->dispcnt;

    uint32_t shift = (disp & (1u << 4)) ? (((disp >> 20) & 3u) + 5u) : 5u;
    uint64_t base_obj = (R->index == 1) ? 0x600000ull : 0x400000ull;
    uint64_t palext   = (disp & 0x80000000u) ? (uint64_t)(uintptr_t)R->obj_ext_palette : 0ull;
    uint64_t pal_obj  = (uint64_t)(uintptr_t)R->palette + 0x200ull;

    memset(R->sprite_count, 0, sizeof R->sprite_count);
    memset(R->line_mode, 0, sizeof R->line_mode);
    R->sprite_screen = 0;

    uint8_t skip[16];
    uint32_t n_salt = 0;

    if ((disp & (1u << 5)) && !(disp & (1u << 6))) {
        uint32_t mask = 0, prio = 0, tbase = 0;
        for (uint32_t i = 0; i < N_OAM; i++) {
            const uint8_t *e = oam + i * 8;
            uint32_t a1 = rd16(e + 2);
            if (a1 & 0x13fu) continue;
            uint32_t a0 = rd16(e);
            if (a0 & 0x3fu) continue;
            uint32_t y = a0 & 0xffu;
            if (y > 0xbfu) continue;
            if ((a0 & 0xff00u) != 0xc00u) continue;
            if ((a1 & 0xfe00u) != 0xc000u) continue;
            uint32_t a2 = rd16(e + 4);
            if ((a2 & 0xf000u) != 0xf000u) continue;

            uint32_t row = y >> 6;
            uint32_t col  = (a1 >> 6) & 7u;
            uint32_t t = (a2 & 0x3ffu) - ((row << 8) | (col << 3));
            uint32_t p = (a2 >> 10) & 3u;

            if (i == 0) { prio = p; tbase = t; }
            else if (p != prio || t != tbase) continue;

            uint32_t bit = (1u << col) << (row << 2);
            if (!(bit & mask)) {
                mask |= bit;
                skip[n_salt++] = (uint8_t)i;
            }
            if (mask == 0xfff) break;
        }

        if (mask != 0xfff) {
            n_salt = 0;
        } else {
            const vram_map_t *tb = &R->gpu->vram;
            uint32_t bank = 0xff;

            for (uint32_t k = 0; k < 4; k++) {
                if (tb->bank[k].map_kind != 6) continue;
                if (base_obj != ((uint64_t)tb->bank[k].first_slot << 14)) continue;
                bank = k;
                if (k == 3) break;
            }
            if (bank == 0xff) {
                n_salt = 0;
            } else {
                uint32_t d = ((tbase << 6) & 0xfffff800u) | ((tbase & 0x1fu) << 3);
                R->sprite_screen_alt = 0;
                if (d <= 0x4000u) {
                    uint32_t m = ((const uint8_t *)tb)[bank + T_MASK];
                    if (!((0x3fu << (tbase >> 7)) & ~m))
                        R->sprite_screen_alt = rd_ptr_u8((const uint8_t *)tb + bank * 8 + T_PTRS) + (d * 3u) * 2u;
                }
                R->sprite_screen_priority = (uint8_t)prio;
                R->sprite_screen = R->vram_window + base_obj + d * 2u;
            }
        }
    }
    skip[n_salt] = 0xff;

    static const uint8_t obj_size_table[24] = {
        0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x40, 0x40,
        0x10, 0x08, 0x20, 0x08, 0x20, 0x10, 0x40, 0x20,
        0x08, 0x10, 0x08, 0x20, 0x10, 0x20, 0x20, 0x40,
    };
    const uint8_t *sizes = obj_size_table;
    uint32_t sig = 0;
    uint32_t v_a0 = 0xffff, v_a1 = 0xffff, v_a2 = 0xffff;

    for (uint32_t i = 0; i < N_OAM; i++) {
        if (i == skip[sig]) { sig++; continue; }
        const uint8_t *e = oam + i * 8;

        uint32_t a0 = rd16(e);
        uint32_t shape = a0 >> 14;
        if (shape == 3) continue;
        if ((a0 & 0x300u) == 0x200u) continue;

        uint32_t a1 = rd16(e + 2);
        uint32_t y  = a0 & 0xffu;
        uint32_t size = ((a1 >> 14) & 3u) | (shape << 2);
        uint32_t height  = sizes[size * 2 + 1];
        uint32_t doubles = (a0 >> 9) & 1u;
        uint32_t heightE = height << doubles;
        if (y >= 0xc0u && (heightE + y) < 0x100u) continue;

        uint32_t a2 = rd16(e + 4);
        uint32_t x9 = a1 & 0x1ffu;

        if ((x9 | y) == 0) {
            if (v_a0 == a0 && v_a1 == a1 && v_a2 == a2) continue;
            v_a0 = a0; v_a1 = a1; v_a2 = a2;
        }

        uint32_t width  = sizes[size * 2];
        uint32_t widthE = width << doubles;
        if (x9 >= 0x100u && (widthE + x9) < 0x200u) continue;

        gpu2d_sprite_t *rec = &R->sprite[i];
        int32_t x = (int32_t)(a1 << 23) >> 23;

        rec->y = (int16_t)(uint16_t)y;
        rec->attribute = 0;
        rec->flip_v = 0;

        uint32_t w10 = 0;
        uint32_t widthF = widthE, heightF = heightE;
        uint32_t yF = y;

        if (!(a0 & 0x100u)) {
            w10 = (a1 >> 10) & 4u;
            if (a1 & (1u << 13)) {
                rec->y = (int16_t)(uint16_t)(heightE + y - 1u);
                rec->flip_v = 1;
            }
        } else {
            const uint8_t *m = oam + ((a1 >> 4) & 0x3e0u);
            int32_t pa = (int16_t)rd16(m + 6);
            int32_t pb = (int16_t)rd16(m + 22);
            uint32_t pc = rd16(m + 14);
            uint32_t pd = rd16(m + 30);
            uint32_t hw = widthE >> 1, hh = heightE >> 1;

            rec->pa = (int16_t)(uint16_t)pa;
            rec->pb = (int16_t)(uint16_t)pb;
            rec->pc = (int16_t)(uint16_t)pc;
            rec->pd = (int16_t)(uint16_t)pd;

            if (pd == 0x100u && ((uint32_t)pa & 0xffffu) == 0x100u
                && ((pc | ((uint32_t)pb & 0xffffu)) & 0xffffu) == 0) {

                if (doubles) {
                    yF = y + (heightE >> 2);
                    rec->y = (int16_t)(uint16_t)yF;
                    if (yF > 0xbfu && (yF + hh) < 0x100u) continue;
                    int32_t xn = x + (int32_t)(widthE >> 2);
                    if (xn > 0xff) continue;
                    if ((int32_t)hw + xn < 1) continue;
                    x = xn;
                    widthF = hw;
                    heightF  = hh;
                } else {
                    widthF = width;
                    heightF  = height;
                }
            } else {
                int32_t x128 = (int16_t)(uint16_t)(a1 << 7);
                int32_t ox = (int32_t)(doubles ? (hw << 7) : (hw << 8));
                int32_t oy = (int32_t)(doubles ? (hh << 7) : (hh << 8));
                ox -= (int32_t)hw * pa;
                oy -= (int32_t)hw * pb;

                uint32_t an = widthE;
                if (!(x128 > -0x381)) {
                    int32_t k = (-x) & ~7;
                    an -= (uint32_t)k;
                    x  += k;
                    ox += pa * k;
                    oy += pb * k;
                }
                if ((an + (uint32_t)x) > 0x100u)
                    an = (uint32_t)(0x107 - x) & ~7u;

                rec->origin_x = (int16_t)(uint16_t)ox;
                rec->origin_y = (int16_t)(uint16_t)oy;
                rec->y = (int16_t)(uint16_t)(y + (heightE >> 1));

                gpu2d_line_compute_span_range3(ox, pa, (int32_t)(width << 8) - 1,
                                   (int32_t)(int16_t)rd16(m + 14),
                                   &rec->span_x[0],
                                   &rec->span_x[2],
                                   &rec->span_x[1]);
                gpu2d_line_compute_span_range3(oy, pb, (int32_t)(height << 8) - 1,
                                   (int32_t)(int16_t)(uint16_t)rec->pd,
                                   &rec->span_y[0],
                                   &rec->span_y[2],
                                   &rec->span_y[1]);
                w10 = 8;
                widthF = an;
            }
        }

        uint32_t mode = (a0 >> 10) & 3u;
        uint32_t prio = (a2 >> 10) & 3u;
        uint32_t pal  = a2 >> 12;
        uint32_t tile = a2 & 0x3ffu;
        uint32_t bands, mark, off;

        if (mode == 3) {
            if (pal == 0) continue;
            rec->attribute = (uint8_t)(1u | ((pal & 0xfu) << 1));
            if (disp & (1u << 6)) {
                rec->row_stride = (uint16_t)(width * 2u);
                off = tile << (((disp >> 22) & 1u) + 7u);
            } else if (disp & (1u << 5)) {
                rec->row_stride = 0x200;
                off = ((tile << 7) & 0x1f000u) | ((a2 & 0x1fu) << 4);
            } else {
                rec->row_stride = 0x100;
                off = ((tile << 7) & 0x1f800u) | ((a2 & 0xfu) << 4);
            }
            if (!(w10 & 8)) {
                off += (w10 & 4u) ? (width * 2u - 2u) : 0u;
                if (!(x > -8)) {
                    int32_t k = (-x) & ~7;
                    widthF -= (uint32_t)k;
                    x += k;
                    uint32_t s = (uint32_t)k << 1;
                    off += (w10 & 4u) ? (uint32_t)(-(int32_t)s) : s;
                }
                if ((widthF + (uint32_t)x) > 0x100u)
                    widthF = (uint32_t)(0x107 - x) & ~7u;
            }
            bands = w10 | 2u;
            mark = 2;
        } else {
            uint32_t col256 = (a0 >> 13) & 1u;
            off = tile << shift;
            if (mode == 1) { rec->attribute = 0x80; mark = 1; } else mark = 0;
            if (mode == 2) prio = 4;
            bands = w10 | col256;

            if (disp & (1u << 4)) {
                uint32_t step = col256 ? ((width >> 3) << 6) : ((width >> 3) << 5);
                rec->row_stride = (uint16_t)step;
                if (w10 & 4u) off += step - (col256 ? 0x40u : 0x20u);
            } else {
                rec->row_stride = 0x400;
                if (col256) {
                    off &= ~(1u << shift);
                    if (w10 & 4u) off += ((width << 3) & 0x7c0u) - 0x40u;
                } else {
                    if (w10 & 4u) off += ((width << 2) & 0x3e0u) - 0x20u;
                }
            }
            if (!(w10 & 8)) {
                if (!(x > -8)) {
                    int32_t k = (-x) & ~7;
                    widthF -= (uint32_t)k;
                    x += k;
                    uint32_t s = (uint32_t)k << (col256 | 2u);
                    off += (w10 & 4u) ? (uint32_t)(-(int32_t)s) : s;
                }
                if ((widthF + (uint32_t)x) > 0x100u)
                    widthF = (uint32_t)(0x107 - x) & ~7u;
            }
        }
        if (widthF == 0) continue;

        rec->kind = (uint8_t)bands;
        rec->width = (uint8_t)widthF;
        rec->x = (int16_t)(uint16_t)x;
        rec->data = R->vram_window + base_obj + off;

        if (a0 & (1u << 13)) {
            rec->palette = (const uint8_t *)(uintptr_t)(palext ? (palext + ((uint64_t)pal << 9)) : pal_obj);
        } else {
            rec->palette = (const uint8_t *)(uintptr_t)(pal_obj + (uint64_t)((pal << 4) * 2u));
        }

        if ((int32_t)yF >= 0xc0) {
            yF -= 0x100u;
            rec->y = (int16_t)(uint16_t)((uint16_t)rec->y - 0x100u);
        }

        uint32_t left = heightF;
        for (;;) {
            uint32_t line = yF & 0xffu;
            if (line <= 0xbfu) {
                uint8_t *cp = &R->sprite_count[prio][line];
                uint32_t n = *cp;
                R->sprite_list[prio][line][n] = (uint8_t)i;
                *cp = (uint8_t)(n + 1);
                R->line_mode[line] |= (uint8_t)mark;
            }
            left--;
            yF++;
            if (left == 0) break;
        }
    }
}
#undef N_OAM
#undef T_SIZES
#undef T_MASK
#undef T_PTRS

#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "gpu/gpu3d/raster/texture_palette_bank_fallback.h"
#include "core_internals.h"
#include "mem_access.h"

#define TEXTURE_HASH_BYTES 0x40000u
#define TEXTURE_SCRATCH_BYTES 0x202u
static uint8_t texture_hash_table[TEXTURE_HASH_BYTES];
static uint8_t texture_scratch[TEXTURE_SCRATCH_BYTES];

static const unsigned char texture_format_shift[8] = { 0x00, 0x01, 0x03, 0x02, 0x01, 0x03, 0x01, 0x00 };
static const unsigned char texture_format_bpp[8] = { 0x00, 0x01, 0x01, 0x01, 0x01, 0x04, 0x01, 0x04 };


extern void gpu3d_raster_wrap_texel_clamp_clamp_0(uint32_t *dst, const int16_t *xy, int32_t n,
                                uint32_t width, uint32_t height, const uint8_t *val) __asm__("gpu3d_raster_wrap_texel_clamp_clamp");
extern void gpu3d_raster_wrap_texel_mirror_clamp_0(uint32_t *dst, const int16_t *xy, int32_t n,
                                uint32_t width, uint32_t height, const uint8_t *val) __asm__("gpu3d_raster_wrap_texel_mirror_clamp");
extern void gpu3d_raster_wrap_texel_clamp_repeat_0(uint32_t *dst, const int16_t *xy, int32_t n,
                                uint32_t width, uint32_t height, const uint8_t *val) __asm__("gpu3d_raster_wrap_texel_clamp_repeat");
extern void gpu3d_raster_wrap_texel_repeat_repeat_0(uint32_t *dst, const int16_t *xy, int32_t n,
                                uint32_t width, uint32_t height, const uint8_t *val) __asm__("gpu3d_raster_wrap_texel_repeat_repeat");
extern void gpu3d_raster_wrap_texel_clamp_mirror_0(uint32_t *dst, const int16_t *xy, int32_t n,
                                uint32_t width, uint32_t height, const uint8_t *val) __asm__("gpu3d_raster_wrap_texel_clamp_mirror");
extern void gpu3d_raster_wrap_texel_repeat_mirror_0(uint32_t *dst, const int16_t *xy, int32_t n,
                                uint32_t width, uint32_t height, const uint8_t *val) __asm__("gpu3d_raster_wrap_texel_repeat_mirror");
extern void gpu3d_raster_wrap_texel_mirror_mirror_0(uint32_t *dst, const int16_t *xy, int32_t n,
                                uint32_t width, uint32_t height, const uint8_t *val) __asm__("gpu3d_raster_wrap_texel_mirror_mirror");
#define CTX_MODE   0x02
#define CTX_TEX    0x10
#define TEX_WIDTH  0x40
#define TEX_HEIGHT   0x42


void gpu3d_raster_texture_wrap_dispatch(uint8_t *param_1, uint32_t *dst, const int16_t *xy,
                         int32_t n, const uint8_t *val)
{

    uint8_t *tex  = (uint8_t *)rd64(param_1 + CTX_TEX);
    uint32_t mode = rd16(param_1 + CTX_MODE) & 0xf;

    uint32_t width = rd16(tex + TEX_WIDTH);

    uint32_t height  = rd16(tex + TEX_HEIGHT);

    switch (mode) {
    case 1: case 9:
        gpu3d_raster_wrap_texel_repeat_clamp(dst, xy, n, width, height, val);
        return;
    case 2: case 6:
        gpu3d_raster_wrap_texel_clamp_repeat(dst, xy, n, width, height, val);
        return;
    case 3:
        gpu3d_raster_wrap_texel_repeat_repeat(dst, xy, n, width, height, val);
        return;
    case 5: case 13:
        gpu3d_raster_wrap_texel_mirror_clamp(dst, xy, n, width, height, val);
        return;
    case 7:
        gpu3d_raster_wrap_texel_mirror_repeat(dst, xy, n, width, height, val);
        return;
    case 10: case 14:
        gpu3d_raster_wrap_texel_clamp_mirror(dst, xy, n, width, height, val);
        return;
    case 11:
        gpu3d_raster_wrap_texel_repeat_mirror(dst, xy, n, width, height, val);
        return;
    case 15:
        gpu3d_raster_wrap_texel_mirror_mirror(dst, xy, n, width, height, val);
        return;
    case 0: case 4: case 8: case 12:
    default:
        gpu3d_raster_wrap_texel_clamp_clamp(dst, xy, n, width, height, val);
        return;
    }
}
#undef CTX_MODE
#undef CTX_TEX
#undef TEX_WIDTH
#undef TEX_HEIGHT

void gpu3d_raster_texture_blend_modulate(unsigned char *dst,
                        const unsigned char *src,
                        const unsigned char *tab,
                        uint32_t stride,
                        uint32_t alpha,
                        uint32_t n)
{

    if (n == 0) return;

    uint64_t z1 = (uint64_t)stride;
    uint64_t z2 = z1 << 1;

    do {
        uint32_t v;
        memcpy(&v, src, 4);
        src += 4;

        uint32_t b0 = tab[0];
        uint32_t b1 = tab[z1];
        uint32_t b2 = tab[z2];

        uint32_t c0 = v & 0xffu;
        uint32_t c1 = (v >> 8) & 0xffu;
        uint32_t s0 = c0 + b0;
        uint32_t c2 = (v >> 16) & 0xffu;
        uint32_t c3 = v >> 24;

        uint32_t r0 = c0 * b0 + s0;
        uint32_t s1 = c1 + b1;
        uint32_t r1 = c1 * b1 + s1;
        uint32_t s3 = c3 + alpha;
        uint32_t s2 = c2 + b2;
        uint32_t r3 = c3 * alpha + s3;
        uint32_t r2 = c2 * b2 + s2;

        r0 = r0 >> 6;
        r1 = r1 << 2;
        r3 = r3 >> 5;
        r2 = r2 << 10;

        r1 = r1 & 0x0007ff00u;

        r0 = (r0 & ~0xff000000u) | ((r3 & 0xffu) << 24);

        r2 = r2 & 0x07ff0000u;

        uint32_t res = r0 | r1;
        res = res | r2;

        n -= 1;

        memcpy(dst, &res, 4);
        dst += 4;

        tab += 1;
    } while (n != 0);
}

void gpu3d_raster_texture_blend_decal(unsigned char *dst,
                        const unsigned char *src,
                        const unsigned char *tab,
                        uint32_t stride,
                        uint32_t alpha,
                        uint32_t n)
{

    if (n == 0) return;

    uint64_t z1   = (uint64_t)stride;
    uint32_t height = alpha << 24;
    uint64_t z2   = z1 << 1;

    do {
        uint32_t v;
        memcpy(&v, src, 4);
        src += 4;

        uint32_t b1 = tab[z1];
        uint32_t b0 = tab[0];
        uint32_t b2 = tab[z2];

        uint32_t c0 = v & 0xffu;
        uint32_t c1 = (v >> 8) & 0xffu;
        uint32_t c2 = (v >> 16) & 0xffu;
        uint32_t f0 = v >> 24;

        uint32_t comp = 31u - f0;
        uint32_t f    = (f0 == 31u) ? 32u : f0;

        uint32_t p0 = f * c0;
        uint32_t p1 = f * c1;
        uint32_t p2 = f * c2;

        uint32_t g = (f == 0u) ? 32u : comp;

        uint32_t r1 = g * b1 + p1;
        uint32_t r0 = g * b0 + p0;
        uint32_t r2 = g * b2 + p2;

        r1 = r1 << 3;
        r2 = r2 << 11;

        uint32_t res = height | (r0 >> 5);

        r1 = r1 & 0xffffff00u;
        r2 = r2 & 0xffff0000u;

        res = res | r1;
        res = res | r2;

        n -= 1;

        memcpy(dst, &res, 4);
        dst += 4;

        tab += 1;
    } while (n != 0);
}

void gpu3d_raster_toon_lookup_planes(unsigned long param_1, unsigned char *param_2,
                         unsigned int param_3, int param_4) {
    if (param_4 == 0) return;

    const unsigned char *tab0 = (const unsigned char *)param_1;
    const unsigned char *tab1 = (const unsigned char *)param_1 + 0x20;
    const unsigned char *tab2 = (const unsigned char *)param_1 + 0x40;

    do {
        unsigned long idx = (unsigned long)(*param_2 >> 1);
        param_4 = param_4 - 1;
        param_2[0]                          = tab0[idx];
        param_2[param_3]                    = tab1[idx];
        param_2[(unsigned long)param_3 * 2] = tab2[idx];
        param_2 = param_2 + 1;
    } while (param_4 != 0);
}

void gpu3d_raster_toon_highlight_add(const unsigned char *table,
                        unsigned char *pix,
                        const unsigned char *level,
                        uint32_t n)
{

    if (n == 0) return;

    const unsigned char *table1 = table + 0x20;
    const unsigned char *table2 = table + 0x40;
    const uint32_t ceil = 0x3f;

    do {
        uint32_t v;
        memcpy(&v, pix, 4);

        uint64_t idx = (uint64_t)*level;
        level += 1;
        idx >>= 1;

        uint32_t c0 = v & 0x3f;
        uint32_t t0 = table[idx];
        uint32_t t1 = table1[idx];
        uint32_t t2 = table2[idx];
        uint32_t c1 = (v >> 8)  & 0x3f;
        c0 = c0 + t0;
        uint32_t c2 = (v >> 16) & 0x3f;
        c1 = c1 + t1;

        uint32_t res = v & 0x1f000000u;

        c2 = c2 + t2;

        c0 = (c0 < 0x3fu) ? c0 : ceil;
        c1 = (c1 < 0x3fu) ? c1 : ceil;

        res = c0 | res;

        c2 = (c2 < 0x3fu) ? c2 : ceil;

        res = res | (c1 << 8);
        res = res | (c2 << 16);

        n -= 1;
        memcpy(pix, &res, 4);
        pix += 4;
    } while (n != 0);
}

void gpu3d_raster_toon_lookup_words(const unsigned char *tables, uint32_t *output,
                         const unsigned char *entry, uint32_t alpha,
                         uint32_t count)
{

    if (count == 0)
        return;

    const unsigned char *table0 = tables;
    const unsigned char *table1 = tables + 0x20;
    const unsigned char *table2 = tables + 0x40;
    uint32_t alpha_high = alpha << 24;

    do {
        count--;
        unsigned idx = (unsigned)(*entry++) >> 1;
        uint32_t px = alpha_high
                    | (uint32_t)table0[idx]
                    | (uint32_t)table1[idx] << 8
                    | (uint32_t)table2[idx] << 16;
        *output++ = px;
    } while (count != 0);
}

void gpu3d_raster_toon_highlight_pack(const unsigned char *table,
                        unsigned char *dst,
                        const unsigned char *src,
                        uint32_t step,
                        uint32_t height,
                        uint32_t n)
{

    if (n == 0) return;

    uint64_t step1 = (uint64_t)step;

    const unsigned char *table1 = table + 0x20;
    const unsigned char *table2 = table + 0x40;

    uint32_t high = height << 24;

    uint64_t step2 = step1 << 1;
    const uint32_t ceil = 0x3f;

    do {
        uint32_t b0 = src[0];
        uint32_t b1 = src[step1];
        uint32_t b2 = src[step2];
        src += 1;

        uint64_t idx = (uint64_t)b0 >> 1;

        uint32_t t0 = table[idx];
        uint32_t t1 = table1[idx];
        uint32_t t2 = table2[idx];

        uint32_t c0 = t0 + b0;
        uint32_t c1 = b1 + t1;
        uint32_t c2 = b2 + t2;

        c0 = (c0 < 0x3fu) ? c0 : ceil;
        c1 = (c1 < 0x3fu) ? c1 : ceil;

        uint32_t res = c0 | high;

        c2 = (c2 < 0x3fu) ? c2 : ceil;

        res = res | (c1 << 8);
        res = res | (c2 << 16);

        n -= 1;
        memcpy(dst, &res, 4);
        dst += 4;
    } while (n != 0);
}

void *gpu3d_raster_texel_blend_dispatch(const unsigned char *a0, const gpu3d_t *a1,
                          const gpu3d_polygon_t *a2, unsigned char *a3,
                          const unsigned char *a4, unsigned char *a5,
                          unsigned int a6, unsigned int a7, unsigned int a8)
{

    uint32_t mode_word = a2->polygon_attr;
    unsigned int mode = (mode_word >> 4) & 3u;

    if (mode == 1) {
        if (a8 != 0) {
            const unsigned char *p4 = a4;
            unsigned char *p5 = a5;
            unsigned char *p3 = a3;
            unsigned int n = a8;
            do {
                uint32_t word;
                memcpy(&word, p4, 4);
                p4 += 4;

                uint32_t byte0 = word & 0xffu;
                uint32_t byte1 = (word >> 8) & 0xffu;
                uint32_t byte2 = (word >> 16) & 0xffu;
                uint32_t byte3 = word >> 24;

                uint32_t wA = (byte3 == 0x1fu) ? 32u : byte3;

                uint32_t wB = (wA == 0u) ? 32u : (31u - byte3);

                uint32_t tap0 = p5[0];
                uint32_t tap1 = p5[a6];
                uint32_t tap2 = p5[(uint64_t)a6 * 2u];

                uint32_t term0 = wB * tap0 + wA * byte0;
                uint32_t term1 = wB * tap1 + wA * byte1;
                uint32_t term2 = wB * tap2 + wA * byte2;

                uint32_t out = (a7 << 24)
                             | (term0 >> 5)
                             | ((term1 << 3) & 0xffffff00u)
                             | ((term2 << 11) & 0xffff0000u);

                memcpy(p3, &out, 4);
                p3 += 4;
                p5 += 1;
                n -= 1;
            } while (n != 0);
        }

        return NULL;
    }

    if (mode == 2) {
        unsigned char flag;

        memcpy(&flag, recon_cfg3d(a0), 1);

        if (flag & 2u) {
            gpu3d_raster_texel_blend_mono_alpha(a3, a4, a5, a7, a8);

            if (a8 != 0) {
                const unsigned char *tR = a1->toon_table_expanded[0];
                const unsigned char *tG = a1->toon_table_expanded[1];
                const unsigned char *tB = a1->toon_table_expanded[2];
                unsigned char *p3 = a3;
                unsigned char *p5 = a5;
                unsigned int n = a8;
                do {
                    uint32_t dest;
                    memcpy(&dest, p3, 4);

                    unsigned char idxbyte = *p5;
                    p5 += 1;
                    uint32_t idx = (uint32_t)idxbyte >> 1;

                    uint32_t r = (dest & 0x3fu) + tR[idx];
                    uint32_t g = ((dest >> 8) & 0x3fu) + tG[idx];
                    uint32_t b = ((dest >> 16) & 0x3fu) + tB[idx];
                    if (r > 0x3fu) r = 0x3fu;
                    if (g > 0x3fu) g = 0x3fu;
                    if (b > 0x3fu) b = 0x3fu;

                    uint32_t out = r | (dest & 0x1f000000u) | (g << 8) | (b << 16);
                    memcpy(p3, &out, 4);
                    p3 += 4;
                    n -= 1;
                } while (n != 0);
            }
            return NULL;
        }

        gpu3d_raster_texel_index_expand_planes(a1->toon_table_expanded[0], a5, a6, (int)a8);
    }

    return gpu3d_raster_texel_blend_planar_alpha(a3, a4, a5, a6, a7, (int)a8);
}



static uint32_t pixel_add_channel_offsets(uint32_t pixel, uint32_t adjust_red,
                                    uint32_t adjust_green, uint32_t adjust_blue)
{
    uint32_t red = pixel & UINT32_C(0xff);
    uint32_t green = (pixel >> 8) & UINT32_C(0xff);
    uint32_t blue = (pixel >> 16) & UINT32_C(0xff);
    uint32_t mix_red = red + adjust_red + red * adjust_red;
    uint32_t mix_green = green + adjust_green + green * adjust_green;
    uint32_t mix_blue = blue + adjust_blue + blue * adjust_blue;

    return (pixel & UINT32_C(0xff000000)) |
           (mix_red >> 6) |
           ((mix_green << 2) & UINT32_C(0x1ff00)) |
           ((mix_blue << 10) & UINT32_C(0x1ff0000));
}

void gpu3d_raster_pixels_add_packed_offset(uint8_t *dest, const uint8_t *origin,
                        uint32_t settings, int32_t count_in)
{

    uint32_t count = (uint32_t)count_in;
    uint64_t count_minus_one;
    uint64_t count64;
    uint32_t adjust_red;
    uint32_t adjust_green;
    uint32_t adjust_blue;

    if (count == 0)
        return;

    count_minus_one = (uint64_t)(count - 1u);
    count64 = count_minus_one + 1u;
    adjust_red = settings & UINT32_C(0x3f);
    adjust_green = (settings >> 8) & UINT32_C(0x3f);
    adjust_blue = (settings >> 16) & UINT32_C(0x3f);

    if (count64 >= 4u) {
        uint64_t bytes = (count_minus_one << 2) + 4u;
        uintptr_t dir_origin = (uintptr_t)origin;
        uintptr_t dir_dest = (uintptr_t)dest;

        if (dir_origin + bytes <= dir_dest ||
            dir_dest + bytes <= dir_origin) {
            uint64_t in_blocks = count64 & UINT64_C(0x1fffffffc);

            do {
                uint32_t pixel0 = rd32(origin);
                uint32_t pixel1 = rd32(origin + 4);
                uint32_t pixel2 = rd32(origin + 8);
                uint32_t pixel3 = rd32(origin + 12);

                wr32(dest,
                              pixel_add_channel_offsets(pixel0, adjust_red,
                                                   adjust_green, adjust_blue));
                wr32(dest + 4,
                              pixel_add_channel_offsets(pixel1, adjust_red,
                                                   adjust_green, adjust_blue));
                wr32(dest + 8,
                              pixel_add_channel_offsets(pixel2, adjust_red,
                                                   adjust_green, adjust_blue));
                wr32(dest + 12,
                              pixel_add_channel_offsets(pixel3, adjust_red,
                                                   adjust_green, adjust_blue));
                origin += 16;
                dest += 16;
                in_blocks -= 4;
            } while (in_blocks != 0);

            if (count64 == (count64 & UINT64_C(0x1fffffffc)))
                return;

            count = count - (uint32_t)(count64 & UINT64_C(0x1fffffffc));
        }
    }

    do {
        uint32_t pixel = rd32(origin);

        wr32(dest, pixel_add_channel_offsets(pixel, adjust_red,
                                                   adjust_green, adjust_blue));
        origin += 4;
        dest += 4;
        count -= 1;
    } while (count != 0);
}

void gpu3d_raster_layer_commit_copy_word(unsigned char *dst_a,
                        unsigned char *dst_b,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        unsigned char *mark,
                        uint32_t value,
                        const unsigned char *type,
                        const unsigned char *cond,
                        uint32_t n)
{

    if (n == 0) return;

    uint32_t height = value << 24;
    uint64_t rest = (uint64_t)n;

    unsigned int c = cond[0];

    for (;;) {
        uint32_t t;

        if (c == 0) {
            memcpy(&t, src_a, 4);
            memcpy(dst_a, &t, 4);
            memcpy(&t, src_b, 4);
            memcpy(dst_b, &t, 4);
            dst_b += 4;
        } else if (type[0] == 0x1f) {
            uint32_t a, b;
            memcpy(&a, src_b, 4);
            memcpy(&b, dst_b, 4);
            t = (a & 0x80000000u) | height;
            t |= b;
            memcpy(dst_b, &t, 4);
            dst_b += 4;
        } else {
            mark[0] = (unsigned char)value;
            memcpy(&t, src_b, 4);
            memcpy(dst_b, &t, 4);
            dst_b += 4;
        }

        cond  += 1;
        type  += 1;
        mark += 1;
        src_b += 4;
        src_a += 4;
        rest -= 1;
        dst_a += 4;

        if (rest == 0) return;
        c = cond[0];
    }
}

void gpu3d_raster_layer_commit_copy_alpha(unsigned char *dst_a,
                        unsigned char *dst_b,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        unsigned char *mark,
                        uint32_t value,
                        const unsigned char *type,
                        const unsigned char *cond,
                        uint32_t n)
{

    if (n == 0) return;

    uint32_t height = value << 24;
    uint64_t rest = (uint64_t)n;

    unsigned int c = cond[0];

    for (;;) {
        uint32_t t;

        if (c == 0) {
            memcpy(&t, src_a, 4);
            memcpy(dst_a, &t, 4);
            memcpy(&t, src_b, 4);
            memcpy(dst_b, &t, 4);
        } else if (type[0] == 0x1f) {
            uint32_t a, b;
            memcpy(&a, src_b, 4);
            memcpy(&b, dst_b, 4);
            t = (a & 0x80000000u) | height;
            t |= b;
            memcpy(dst_b, &t, 4);
        } else {
            mark[0] = (unsigned char)value;
            dst_b[3] = src_b[3];
        }

        cond  += 1;
        type  += 1;
        mark += 1;
        dst_b += 4;
        src_b += 4;
        src_a += 4;
        rest -= 1;
        dst_a += 4;

        if (rest == 0) return;
        c = cond[0];
    }
}

void gpu3d_raster_layer_commit_copy_word_marked(unsigned char *dst_a,
                        unsigned char *dst_b,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        unsigned char *mark,
                        uint32_t value,
                        const unsigned char *type,
                        const unsigned char *cond,
                        uint32_t n)
{

    if (n == 0) return;

    uint32_t height = value << 24;
    uint64_t rest = (uint64_t)n;

    unsigned int c = cond[0];

    for (;;) {
        uint32_t t;

        if (c == 0) {
            memcpy(&t, src_a, 4);
            memcpy(dst_a, &t, 4);
            memcpy(&t, src_b, 4);
            memcpy(dst_b, &t, 4);
        } else if (type[0] == 0x1f) {
            uint32_t a, b;
            memcpy(&a, src_b, 4);
            memcpy(&b, dst_b, 4);
            t = (a & 0x80000000u) | height;
            t |= b;
            memcpy(dst_b, &t, 4);

            memcpy(&t, dst_a, 4);
            t |= 0x80000000u;
            memcpy(dst_a, &t, 4);
        } else {
            uint32_t a, d;
            mark[0] = (unsigned char)value;

            memcpy(&t, src_b, 4);
            memcpy(dst_b, &t, 4);

            memcpy(&a, src_a, 4);
            memcpy(&d, dst_a, 4);
            t = d | (a & 0x80000000u);
            memcpy(dst_a, &t, 4);
        }

        cond  += 1;
        type  += 1;
        mark += 1;
        src_b += 4;
        dst_b += 4;
        src_a += 4;
        rest -= 1;
        dst_a += 4;

        if (rest == 0) return;
        c = cond[0];
    }
}

void gpu3d_raster_layer_commit_copy_alpha_marked(unsigned char *dst_a,
                        unsigned char *dst_b,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        unsigned char *mark,
                        uint32_t value,
                        const unsigned char *type,
                        const unsigned char *cond,
                        uint32_t n)
{

    if (n == 0) return;

    uint32_t height = value << 24;
    uint64_t rest = (uint64_t)n;

    unsigned int c = cond[0];

    for (;;) {
        uint32_t t;

        if (c == 0) {
            memcpy(&t, src_a, 4);
            memcpy(dst_a, &t, 4);
            memcpy(&t, src_b, 4);
            memcpy(dst_b, &t, 4);
        } else if (type[0] == 0x1f) {
            uint32_t a, b;
            memcpy(&a, src_b, 4);
            memcpy(&b, dst_b, 4);
            t = (a & 0x80000000u) | height;
            t |= b;
            memcpy(dst_b, &t, 4);

            memcpy(&t, dst_a, 4);
            t |= 0x80000000u;
            memcpy(dst_a, &t, 4);
        } else {
            uint32_t a, d;
            mark[0] = (unsigned char)value;

            dst_b[3] = src_b[3];

            memcpy(&a, src_a, 4);
            memcpy(&d, dst_a, 4);
            t = d | (a & 0x80000000u);
            memcpy(dst_a, &t, 4);
        }

        cond  += 1;
        type  += 1;
        mark += 1;
        dst_b += 4;
        src_b += 4;
        src_a += 4;
        rest -= 1;
        dst_a += 4;

        if (rest == 0) return;
        c = cond[0];
    }
}

void gpu3d_raster_layer_commit_fill(unsigned char *dst_a,
                        uint32_t base,
                        const unsigned char *src_a,
                        unsigned char *dst_b,
                        unsigned char *mark,
                        uint32_t value,
                        const unsigned char *type,
                        const unsigned char *cond,
                        uint32_t n)
{

    if (n == 0) return;

    uint32_t mix = base | (value << 24);

    uint64_t rest = (uint64_t)n;

    unsigned int c = cond[0];

    for (;;) {
        if (c == 0) {
            uint32_t t;
            memcpy(&t, src_a, 4);
            memcpy(dst_a, &t, 4);
        } else if (type[0] == 0x1f) {
            memcpy(dst_b, &mix, 4);
        } else {
            mark[0] = (unsigned char)value;
        }

        cond  += 1;
        type  += 1;
        mark += 1;
        dst_b += 4;
        src_a += 4;
        rest -= 1;
        dst_a += 4;

        if (rest == 0) return;
        c = cond[0];
    }
}

void gpu3d_raster_layer_commit_fill_keep_alpha(unsigned char *dst_a,
                        uint32_t base,
                        const unsigned char *src_a,
                        unsigned char *field,

                        unsigned char *mark,
                        uint32_t value,
                        const unsigned char *type,
                        const unsigned char *cond,
                        uint32_t n)
{

    if (n == 0) return;

    uint32_t base_low = base & 0x00ffffffu;

    uint32_t mix = base | (value << 24);

    uint64_t rest = (uint64_t)n;

    unsigned int c = cond[0];

    for (;;) {
        if (c == 0) {
            uint32_t t;
            memcpy(&t, src_a, 4);
            memcpy(dst_a, &t, 4);
        } else if (type[0] == 0x1f) {
            memcpy(field, &mix, 4);
        } else {
            mark[0] = (unsigned char)value;

            unsigned int height = field[3];

            uint32_t w12 = base_low;

            w12 = (w12 & 0x00ffffffu) | ((height & 0xffu) << 24);

            memcpy(field, &w12, 4);
        }

        cond  += 1;
        type  += 1;
        mark += 1;
        field += 4;
        src_a += 4;
        rest -= 1;
        dst_a += 4;

        if (rest == 0) return;
        c = cond[0];
    }
}

void gpu3d_raster_layer_commit_fill_marked(unsigned char *dst_a,
                        uint32_t base,
                        const unsigned char *src_a,
                        unsigned char *dst_b,
                        unsigned char *mark,
                        uint32_t value,
                        const unsigned char *type,
                        const unsigned char *cond,
                        uint32_t n)
{

    if (n == 0) return;

    uint32_t mix = base | (value << 24);

    uint64_t rest = (uint64_t)n;

    unsigned int c = cond[0];

    for (;;) {
        int has_store = 1;
        uint32_t t = 0;

        if (c == 0) {

            memcpy(&t, src_a, 4);
        } else if (type[0] == 0x1f) {

            uint32_t d;
            memcpy(&d, dst_a, 4);
            d |= 0x80000000u;
            memcpy(dst_a, &d, 4);
            memcpy(dst_b, &mix, 4);
            has_store = 0;
        } else {

            uint32_t s, d;
            mark[0] = (unsigned char)value;
            memcpy(&s, src_a, 4);
            memcpy(&d, dst_a, 4);
            s &= 0x80000000u;
            t = d | s;
        }

        if (has_store)
            memcpy(dst_a, &t, 4);

        cond  += 1;
        type  += 1;
        mark += 1;
        src_a += 4;
        dst_a += 4;
        rest -= 1;
        dst_b += 4;

        if (rest == 0) return;
        c = cond[0];
    }
}

void gpu3d_raster_layer_commit_fill_keep_alpha_marked(unsigned char *dst_a,

                        uint32_t base,
                        const unsigned char *src_a,
                        unsigned char *field,

                        unsigned char *mark,
                        uint32_t value,
                        const unsigned char *type,
                        const unsigned char *cond,
                        uint32_t n)
{

    if (n == 0) return;

    uint32_t base_low = base & 0x00ffffffu;

    uint32_t mix = base | (value << 24);

    uint64_t rest = (uint64_t)n;

    unsigned int c = cond[0];

    for (;;) {
        int has_store = 1;
        uint32_t t = 0;

        if (c == 0) {

            memcpy(&t, src_a, 4);
        } else if (type[0] == 0x1f) {

            uint32_t d;
            memcpy(&d, dst_a, 4);
            d |= 0x80000000u;
            memcpy(dst_a, &d, 4);
            memcpy(field, &mix, 4);
            has_store = 0;
        } else {

            uint32_t s, d;

            mark[0] = (unsigned char)value;

            unsigned int height = field[3];

            uint32_t w12 = base_low;

            w12 = (w12 & 0x00ffffffu) | ((height & 0xffu) << 24);

            memcpy(field, &w12, 4);

            memcpy(&s, src_a, 4);
            memcpy(&d, dst_a, 4);
            s &= 0x80000000u;
            t = d | s;
        }

        if (has_store)
            memcpy(dst_a, &t, 4);

        cond  += 1;
        type  += 1;
        mark += 1;
        field += 4;
        src_a += 4;
        rest -= 1;
        dst_a += 4;

        if (rest == 0) return;
        c = cond[0];
    }
}

typedef void *(*fn_malloc)(unsigned long);

static void *core_malloc(unsigned long n) {
    static fn_malloc f;
    if (!f) f = (fn_malloc)sym_libc_malloc;
    return f(n);
}
#define CTX_TABLE  24
#define CTX_FLAG   72




static uint32_t pack5(uint16_t h) {
    uint32_t r = (uint32_t)(h & 0x1f);
    uint32_t g = (uint32_t)((h >> 5) & 0x1f);
    uint32_t b = (uint32_t)((h >> 10) & 0x1f);
    uint32_t rr = r * 2u + (r != 0);
    uint32_t gg = g * 2u + (g != 0);
    uint32_t bb = b * 2u + (b != 0);
    return rr | (gg << 8) | (bb << 16);
}

void *gpu3d_raster_texel_palette_table_build(uint8_t *ctx, const uint16_t *colors) {

    wr16(ctx + CTX_FLAG, 0x100);

    void *table = rd_ptr(ctx + CTX_TABLE);
    if (table == 0) {
        table = core_malloc(0x400);
        wr_ptr(ctx + CTX_TABLE, table);
    }

    uint16_t h[8];
    memcpy(h, colors, sizeof(h));

    uint32_t pal[8];
    int i, j;
    for (i = 0; i < 8; i++) pal[i] = pack5(h[i]);

    uint8_t *out = (uint8_t *)table;
    for (i = 0; i < 32; i++) {
        uint32_t tag = (uint32_t)i << 24;
        for (j = 0; j < 8; j++) {
            uint32_t word = pal[j] | tag;
            memcpy(out + (size_t)i * 32 + (size_t)j * 4, &word, 4);
        }
    }

    return table;
}
#undef CTX_TABLE
#undef CTX_FLAG



static void expand_byte(unsigned char *out, uint8_t v)
{
    wr8(out, (uint8_t)(v & 3u));
    wr8(out + 1, (uint8_t)((v >> 2) & 3u));
    wr8(out + 2, (uint8_t)((v >> 4) & 3u));
    wr8(out + 3, (uint8_t)(v >> 6));
}

void gpu3d_raster_texel_unpack_2bpp(unsigned char *out, unsigned char *in, uint32_t n)
{

    if (n == 0)
        return;

    uint64_t len = (uint64_t)n;
    uint64_t done = 0;
    int overlap = ((uintptr_t)out < (uintptr_t)in + len) &&
                 ((uintptr_t)in < (uintptr_t)out + len * 4u);

    if (n >= 0x20u && !overlap) {
        uint64_t n32 = len & 0xffffffe0ULL;

        while (done != n32) {
            uint8_t block[32];
            memcpy(block, in + done, sizeof(block));

            for (uint32_t i = 0; i < 32u; i++)
                expand_byte(out + (done + i) * 4u, block[i]);

            done += 32u;
        }

        if (done == len)
            return;
    }

    while (done != len) {
        uint8_t v = rd8(in + done);
        expand_byte(out + done * 4u, v);
        done++;
    }
}

void gpu3d_raster_texel_unpack_nibbles_checked(unsigned char *out, unsigned char *in, uint32_t n)
{

    if (n == 0) {
        return;
    }

    uint32_t i;
    for (i = 0; i < n; i++) {
        unsigned char b = in[i];
        out[2u * i]      = (unsigned char)(b & 0xfu);
        out[2u * i + 1u] = (unsigned char)(b >> 4);
    }
}

static void    copy32(void *dst, const void *src) { memcpy(dst, src, 32); }

void gpu3d_raster_texel_copy_overlap_checked(unsigned char *dest, unsigned char *src, uint32_t n)
{

    if (n == 0) return;

    uint64_t len = (uint64_t)n;

    int overlap = ((uintptr_t)dest < (uintptr_t)src + len) &&
                 ((uintptr_t)src  < (uintptr_t)dest + len);

    uint64_t n32 = 0;

    if (n >= 0x20u && !overlap) {

        n32 = len & 0xffffffe0ULL;

        unsigned char *pdst = dest + 0x10;
        unsigned char *psrc = src  + 0x10;
        uint64_t remaining = n32;
        dest = dest + n32;

        do {
            unsigned char block[32];
            copy32(block, psrc - 16);
            remaining -= 0x20;
            psrc += 0x20;
            copy32(pdst - 16, block);
            pdst += 0x20;
        } while (remaining != 0);

        if (n32 == len) return;
    }

    uint64_t tail = len - n32;
    unsigned char *psrc2 = src + n32;
    do {
        uint8_t b = rd8(psrc2);
        psrc2 += 1;
        tail  -= 1;
        wr8(dest, b);
        dest  += 1;
    } while (tail != 0);
}

#define CTX_TABLE 24
#define CTX_TAG   72





static void *core_malloc_21(unsigned long n) {
    typedef void *(*fn_malloc)(unsigned long);
    static fn_malloc f;
    if (!f) f = (fn_malloc)sym_libc_malloc;
    return f(n);
}

static void *core_memset(void *p, int c, unsigned long n) {
    typedef void *(*fn_memset)(void *, int, unsigned long);
    static fn_memset f;
    if (!f) f = (fn_memset)sym_libc_memset;
    return f(p, c, n);
}

static void *ensure_table(uint8_t *node, uint16_t tag, unsigned long size) {
    wr16(node + CTX_TAG, tag);
    void *t = rd_ptr(node + CTX_TABLE);
    if (!t) { t = core_malloc_21(size); wr_ptr(node + CTX_TABLE, t); }
    return t;
}

static uint32_t pack5_21(uint16_t h) {
    uint32_t r = (uint32_t)(h & 0x1f);
    uint32_t g = (uint32_t)((h >> 5) & 0x1f);
    uint32_t b = (uint32_t)((h >> 10) & 0x1f);
    uint32_t rr = r * 2u + (r != 0);
    uint32_t gg = g * 2u + (g != 0);
    uint32_t bb = b * 2u + (b != 0);
    return rr | (gg << 8) | (bb << 16);
}

static uint32_t palette_entry(uint16_t h, int is_entry0, int color0) {
    uint32_t p = pack5_21(h);
    if (is_entry0 && color0) return p;
    return p | 0x1f000000u;
}

void gpu3d_raster_texture_format_decode(uint8_t *node, uint8_t *dest, uint8_t *origin,
                          const uint16_t *palette, uint32_t format,
                          uint32_t size, int32_t color0)
{

    uint32_t i;

    switch (format) {

    case 1: {
        if (origin) {
            gpu3d_geometry_palette_build_alpha_ramp_table(node, palette);
            if (size) for (i = 0; i < size; i++) dest[i] = origin[i];
        } else {
            void *table = ensure_table(node, 0x100, 0x400);
            uint32_t e0 = pack5_21(rd16(palette));
            memcpy(table, &e0, 4);
            core_memset(dest, 0, size);
        }
        break;
    }

    case 2: {
        void *table = ensure_table(node, 4, 0x10);
        if (origin) {
            uint32_t e[4];
            e[0] = palette_entry(rd16(palette + 0), 1, color0);
            e[1] = palette_entry(rd16(palette + 1), 0, color0);
            e[2] = palette_entry(rd16(palette + 2), 0, color0);
            e[3] = palette_entry(rd16(palette + 3), 0, color0);
            memcpy(table, e, 16);
            if (size) {
                for (i = 0; i < size; i++) {
                    uint8_t b = origin[i];
                    dest[i * 4 + 0] = (uint8_t)(b & 3);
                    dest[i * 4 + 1] = (uint8_t)((b >> 2) & 3);
                    dest[i * 4 + 2] = (uint8_t)((b >> 4) & 3);
                    dest[i * 4 + 3] = (uint8_t)((b >> 6) & 3);
                }
            }
        } else {
            uint32_t e0 = palette_entry(rd16(palette), 1, color0);
            memcpy(table, &e0, 4);
            core_memset(dest, 0, (unsigned long)size * 4);
        }
        break;
    }

    case 3: {
        void *table = ensure_table(node, 0x10, 0x40);
        if (origin) {
            uint32_t e[16];
            for (i = 0; i < 16; i++)
                e[i] = palette_entry(rd16(palette + i), i == 0, color0);
            memcpy(table, e, 64);

            typedef uint64_t (*fn_unpack)(void *, void *, uint64_t, const void *,
                                           uint32_t, uint32_t, int32_t);
            fn_unpack f = (fn_unpack)(sym_gpu3d_raster_texel_unpack_nibbles);
            f(dest, origin, (uint64_t)size, palette, 3, size, color0);
        } else {
            uint32_t e0 = palette_entry(rd16(palette), 1, color0);
            memcpy(table, &e0, 4);
            core_memset(dest, 0, (unsigned long)size * 2);
        }
        break;
    }

    case 4: {
        void *table = ensure_table(node, 0x100, 0x400);
        if (origin) {
            uint32_t e[256];
            for (i = 0; i < 256; i++)
                e[i] = palette_entry(rd16(palette + i), i == 0, color0);
            memcpy(table, e, 1024);
            if (size) for (i = 0; i < size; i++) dest[i] = origin[i];
        } else {
            uint32_t e0 = palette_entry(rd16(palette), 1, color0);
            memcpy(table, &e0, 4);
            core_memset(dest, 0, size);
        }
        break;
    }

    case 5:
        break;

    case 6: {
        if (origin) {
            gpu3d_raster_texel_palette_table_build(node, palette);
            if (size) for (i = 0; i < size; i++) dest[i] = origin[i];
        } else {

            void *table = ensure_table(node, 0x100, 0x400);
            uint32_t e0 = pack5_21(rd16(palette));
            memcpy(table, &e0, 4);
            core_memset(dest, 0, size);
        }
        break;
    }

    case 7: {
        uint32_t pix = size >> 1;
        if (origin) {
            for (i = 0; i < pix; i++) {
                uint16_t t = rd16(origin + (size_t)i * 2);
                uint32_t c = pack5_21(t);
                uint32_t a = (t & 0x8000u) ? 0x1f000000u : 0u;
                uint32_t w = c | a;
                memcpy(dest + (size_t)i * 4, &w, 4);
            }
        } else {
            core_memset(dest, 0, (unsigned long)pix * 4);
        }
        break;
    }

    default:
        break;
    }

}
#undef CTX_TABLE
#undef CTX_TAG

#define HASH_TABLE_OFF   0x3edd65eUL
#define ZERO_PAGE_OFF    0x10e9fcUL
typedef void *(*fn_malloc_22)(unsigned long);
typedef void *(*fn_realloc)(void *, unsigned long);








static uint32_t round5(uint32_t v5) { return (v5 << 1) | (v5 != 0); }

static uint32_t pack_opaque(uint32_t r5, uint32_t g5, uint32_t b5) {
    return round5(r5) | (round5(g5) << 8) | (round5(b5) << 16) | (0x1Fu << 24);
}

static void unpack555(uint16_t c, uint32_t *r, uint32_t *g, uint32_t *b) {
    *r = c & 0x1Fu;
    *g = (c >> 5) & 0x1Fu;
    *b = (c >> 10) & 0x1Fu;
}

static uint32_t hashcolor(uint32_t v) {
    uint32_t a = ((v >> 2) & 0xFC0u) | (v & 0x3Fu);
    uint32_t b = (v >> 4) & 0x3F000u;
    return a | b;
}

static void insert_if_new(uint8_t *hashtab, uint32_t *palette, uint32_t *count, uint32_t color) {
    uint32_t h = hashcolor(color);
    if (hashtab[h] == 0) {
        palette[*count] = color;

        hashtab[h] = (uint8_t)(*count);
        (*count)++;
    }
}

void gpu3d_raster_texture_decode_4x4_compressed(void *node_v, uint32_t *texel_bits, const uint16_t *block_idx,
                         uint32_t pltt_base8, void *const *pltt_banks)
{

    uint8_t *node = (uint8_t *)node_v;

    static fn_malloc_22  core_malloc;
    static fn_realloc core_realloc;
    if (!core_malloc)  core_malloc  = (fn_malloc_22)sym_libc_malloc;
    if (!core_realloc) core_realloc = (fn_realloc)sym_libc_realloc;

    uint8_t *hashtab  = texture_hash_table;
    const uint8_t *zeropage = texture_palette_bank_fallback;

    uint32_t sizeS = rd16(node + 64);
    uint32_t sizeT = rd16(node + 66);
    uint32_t blocksX = sizeS >> 2;
    uint32_t blocksY = sizeT >> 2;

    uint32_t palette[260];
    palette[0] = 0;
    uint32_t count = 1;

    uint8_t *outbuf = rd_ptr_u8(node + 16);

    if (blocksY != 0 && blocksX != 0) {
        for (uint32_t by = 0; by < blocksY; by++) {
            for (uint32_t bx = 0; bx < blocksX; bx++) {
                uint32_t blk = by * blocksX + bx;
                uint32_t tbits  = texel_bits[blk];
                uint16_t idxval = block_idx[blk];

                uint32_t off14 = (uint32_t)(idxval & 0x3FFFu);
                uint32_t comb  = (off14 << 1) + pltt_base8;
                uint32_t bank  = comb >> 13;
                uint32_t inbank = comb & 0x1FFFu;
                const uint8_t *bp = (const uint8_t *)pltt_banks[bank];
                if (!bp) bp = zeropage;
                const uint8_t *entry = bp + ((size_t)inbank << 1);

                uint16_t c0 = rd16(entry);
                uint16_t c1 = rd16(entry + 2);
                uint32_t mode = idxval >> 14;

                uint32_t r0, g0, b0, r1, g1, b1;
                unpack555(c0, &r0, &g0, &b0);
                unpack555(c1, &r1, &g1, &b1);

                uint32_t colors[4];
                colors[0] = pack_opaque(r0, g0, b0);
                colors[1] = pack_opaque(r1, g1, b1);

                switch (mode) {
                case 0: {
                    uint16_t c2 = rd16(entry + 4);
                    uint32_t r2, g2, b2;
                    unpack555(c2, &r2, &g2, &b2);
                    colors[2] = pack_opaque(r2, g2, b2);
                    colors[3] = 0;
                    break;
                }
                case 1: {
                    colors[2] = pack_opaque((r0 + r1) >> 1, (g0 + g1) >> 1, (b0 + b1) >> 1);
                    colors[3] = 0;
                    break;
                }
                case 2: {
                    uint16_t c2 = rd16(entry + 4);
                    uint16_t c3 = rd16(entry + 6);
                    uint32_t r2, g2, b2, r3, g3, b3;
                    unpack555(c2, &r2, &g2, &b2);
                    unpack555(c3, &r3, &g3, &b3);
                    colors[2] = pack_opaque(r2, g2, b2);
                    colors[3] = pack_opaque(r3, g3, b3);
                    break;
                }
                default: {
                    colors[2] = pack_opaque((5 * r0 + 3 * r1) >> 3,
                                             (5 * g0 + 3 * g1) >> 3,
                                             (5 * b0 + 3 * b1) >> 3);
                    colors[3] = pack_opaque((3 * r0 + 5 * r1) >> 3,
                                             (3 * g0 + 5 * g1) >> 3,
                                             (3 * b0 + 5 * b1) >> 3);
                    break;
                }
                }

                for (int ty = 0; ty < 4; ty++) {
                    for (int tx = 0; tx < 4; tx++) {
                        uint32_t sel = (tbits >> ((ty * 4 + tx) * 2)) & 3u;
                        uint32_t row = by * 4 + (uint32_t)ty;
                        uint32_t col = bx * 4 + (uint32_t)tx;
                        size_t off = ((size_t)row * sizeS + col) * 4u;
                        wr32(outbuf + off, colors[sel]);
                    }
                }

                if (count <= 0x100u) {

                    insert_if_new(hashtab, palette, &count, colors[0]);
                    insert_if_new(hashtab, palette, &count, colors[1]);
                    insert_if_new(hashtab, palette, &count, colors[2]);
                    if (colors[3] != 0)
                        insert_if_new(hashtab, palette, &count, colors[3]);
                }
            }
        }
    }

    uint32_t total_texels = blocksX * blocksY * 16u;

    if (count <= 0x100u) {
        if (total_texels != 0) {

            for (uint32_t i = 0; i < total_texels; i++) {
                uint32_t raw = rd32(outbuf + (size_t)i * 4);
                uint8_t idx = (raw == 0) ? 0 : hashtab[hashcolor(raw)];
                outbuf[i] = idx;
            }
        }

        void *newpal = core_malloc((unsigned long)count * 4u);
        memcpy(newpal, palette, (size_t)count * 4u);
        wr_ptr(node + 24, newpal);

        void *newbuf = core_realloc(outbuf, (unsigned long)total_texels);
        wr_ptr(node + 16, newbuf);

        wr8(node + 75, 8);
        wr16(node + 72, (uint16_t)count);

        if (count >= 2) {
            for (uint32_t i = 1; i < count; i++)
                hashtab[hashcolor(palette[i])] = 0;
        }
    } else {

        for (uint32_t i = 1; i < count; i++)
            hashtab[hashcolor(palette[i])] = 0;
    }
}
#undef HASH_TABLE_OFF
#undef ZERO_PAGE_OFF

#define LIMIT      0x20000

void gpu3d_raster_texture_data_split_bank_limit(gpu3d_texture_cache_t *cache, void *a1, uint8_t *data, unsigned size,
                            void *a4, unsigned channel, unsigned shift,
                            unsigned mode, unsigned extra) {

    uint8_t *base = cache->gpu->vram.texture[channel];
    unsigned used = size;

    if ((shift + size) >= LIMIT) {
        unsigned fits = LIMIT - shift;

        const uint8_t *t_shift = texture_format_shift;
        const uint8_t *t_mul   = texture_format_bpp;

        unsigned step = ((fits << t_shift[mode]) >> 1) * t_mul[mode];

        gpu3d_raster_texture_data_split_bank_limit(cache, a1, data + step, size - fits, a4,
                           (channel + 1) & 3,
                           0, mode, extra);

        used = fits;
    }

    void *p = base ? (void *)(base + shift) : (void *)0;

    gpu3d_raster_texture_format_decode(a1, data, p, a4, mode, used, extra);
}
#undef LIMIT

#define SHIFT_TABLE  (texture_format_shift)
#define BPP_TABLE    (texture_format_bpp)
static const uint16_t texture_palette_colors[8] = { 0u, 32u, 4u, 16u, 256u, 0u, 8u, 0u };
#define PLTLIM_TABLE ((const uint8_t *)texture_palette_colors)
#define SCRATCH_BUF  (texture_scratch)
typedef void  *(*fn_malloc_24)(unsigned long);
typedef void   (*fn_free)(void *);
typedef void  *(*fn_memcpy)(void *, const void *, unsigned long);
typedef void  *(*fn_memset)(void *, int, unsigned long);
typedef void  *(*fn_memcpy_chk)(void *, const void *, unsigned long, unsigned long);
typedef void  *(*fn_memset_chk)(void *, int, unsigned long, unsigned long);



void gpu3d_raster_texture_cache_entry_build(gpu3d_texture_entry_t *node, gpu3d_texture_cache_t *ctx, uint32_t p3, uint32_t p4)
{

    static fn_malloc_24     core_malloc;
    static fn_free        core_free;
    static fn_memcpy      core_memcpy;
    static fn_memset      core_memset;
    static fn_memcpy_chk  core_memcpy_chk;
    static fn_memset_chk  core_memset_chk;
    if (!core_malloc)    core_malloc    = (fn_malloc_24)sym_libc_malloc;
    if (!core_free)      core_free      = (fn_free)sym_libc_free;
    if (!core_memcpy)    core_memcpy    = (fn_memcpy)sym_libc_memcpy;
    if (!core_memset)    core_memset    = (fn_memset)sym_libc_memset;
    if (!core_memcpy_chk) core_memcpy_chk = (fn_memcpy_chk)fortify_memcpy;
    if (!core_memset_chk) core_memset_chk = (fn_memset_chk)fortify_memset;

    vram_map_t *vram = &ctx->gpu->vram;
    node->sub = (uint16_t)p4;
    node->key = p3 & 0xfff0ffffu;
    node->dirty = 0;

    uint32_t sizeS   = 8u << ((p3 >> 20) & 7u);
    uint32_t sizeT   = 8u << ((p3 >> 23) & 7u);
    uint32_t format = (p3 >> 26) & 7u;
    uint32_t vram_off = (p3 << 3) & 0x7fff8u;
    uint32_t narrow_off = (p3 << 3) & 0x1fff8u;
    uint32_t channel = vram_off >> 17;
    uint32_t area = sizeS * sizeT;

    uint8_t *buf0 = node->data;
    if (buf0 != 0) {
        uint8_t cache_fmt = node->format;
        if (cache_fmt != (uint8_t)format) {
            core_free(buf0);
            uint8_t *buf1 = node->palette;
            if (buf1 != 0) core_free(buf1);
            node->data = 0;
            node->palette = 0;
            buf0 = 0;
        }
    }
    if (buf0 == 0) {
        unsigned long size = (unsigned long)area * (unsigned long)rd8((void *)(BPP_TABLE + format));
        buf0 = (uint8_t *)core_malloc(size);
        node->data = buf0;
        uint32_t busy = ctx->occupied;
        ctx->occupied = busy + (uint32_t)size;
    }

    node->format = (uint8_t)format;
    node->palette_count = 0;

    uint32_t shiftamt  = (p3 >> 11) & 0x1fu;
    uint32_t shift_val = rd8((void *)(SHIFT_TABLE + format)) & 0x1fu;
    uint32_t uVar5 = (area << 1) >> shift_val;
    uint32_t iVar6 = uVar5 - 1;
    uint32_t part1 = (uint32_t)(-1) << (shiftamt & 0x1fu);
    uint32_t part2 = (uint32_t)(-2) << (((iVar6 + vram_off) >> 14) & 0x1fu);
    uint32_t mask = part1 & ~part2;

    node->width = (uint16_t)sizeS;
    node->bank_mask_a = mask;
    node->height = (uint16_t)sizeT;

    if (format == 5) {

        uint8_t *data0 = vram->texture[channel];
        uint8_t *data1 = data0 ? vram->texture[1] : NULL;
        if (data0 == NULL || data1 == NULL) {
            core_memset(buf0, 0, (unsigned long)(area << 2));
        } else {
            void *p1 = data0 + narrow_off;
            void *p2 = data1 + ((uint64_t)channel << 15) + (narrow_off >> 1);
            uint32_t size = p4 << 3;
            void *table = vram->texture_palette;
            gpu3d_raster_texture_decode_4x4_compressed(node, p1, p2, size, table);
        }

        uint32_t comb = (((channel & 3u) << 15) | 0x20000u) + (narrow_off >> 1);
        uint32_t p1b = (uint32_t)(-1) << ((comb >> 14) & 0x1fu);
        uint32_t p2b = (uint32_t)(-2) << (((iVar6 + comb) >> 14) & 0x1fu);
        uint32_t prev = node->bank_mask_a;
        node->bank_mask_a = prev | (p1b & ~p2b);
        node->bank_mask_b = 0xfu;
        return;
    }

    uint32_t color0 = (p3 >> 29) & 1u;
    uint32_t arg_size = uVar5;
    void *palette;

    if (format == 7) {
        node->bank_mask_b = 0;
        palette = 0;
    } else {
        uint16_t tbl2 = rd16((void *)(PLTLIM_TABLE + (uint64_t)format * 2));
        uint32_t bits = (format != 2) ? 3u : 2u;
        uint32_t pal_full = p4 << bits;
        uint32_t off_bank = pal_full & 0x1ffcu;
        uint32_t hi = pal_full >> 13;
        uint32_t chk = pal_full >> 14;
        uint32_t idx = (chk < 3u) ? hi : (hi - 6u);

        uint8_t *scratch = SCRATCH_BUF;

        if (off_bank + tbl2 > 0x2000u) {

            uint32_t fits = 0x2000u - off_bank;
            uint32_t idx2 = (idx + 1u < 6u) ? (idx + 1u) : (idx - 5u);
            uint8_t *ptr1 = vram->texture_palette[idx];
            uint8_t *ptr2 = vram->texture_palette[idx2];
            node->bank_mask_b = 3u << (idx & 0x1fu);

            if (ptr1 == 0)
                core_memset_chk(scratch, 0, (unsigned long)fits * 2, 512);
            else
                core_memcpy_chk(scratch, ptr1 + (size_t)off_bank * 2, (unsigned long)fits * 2, 512);

            uint32_t rest = tbl2 - fits;
            if (ptr2 == 0)
                core_memset(scratch + (size_t)fits * 2, 0, (unsigned long)rest * 2);
            else
                core_memcpy(scratch + (size_t)fits * 2, ptr2, (unsigned long)rest * 2);

            palette = scratch;
        } else {

            node->bank_mask_b = 1u << (idx & 0x1fu);
            uint8_t *ptr = vram->texture_palette[idx];
            if (ptr == 0) {
                core_memset_chk(scratch, 0, (unsigned long)tbl2 * 2, 512);
                palette = scratch;
            } else {
                palette = ptr + (size_t)off_bank * 2;
            }
        }
    }

    uint8_t *buf_current = node->data;
    gpu3d_raster_texture_data_split_bank_limit(ctx, node,
                           buf_current, arg_size, palette,
                           channel, narrow_off, format, color0);
}
#undef SHIFT_TABLE
#undef BPP_TABLE
#undef PLTLIM_TABLE
#undef SCRATCH_BUF

void *gpu3d_raster_texture_cache_lookup_or_create(gpu3d_texture_cache_t *base, uint32_t key, uint32_t sub) {

    uint32_t bucket_index = (key >> 7) & 0x1ff;
    gpu3d_texture_entry_t **head = &base->bucket[bucket_index];
    gpu3d_texture_entry_t *first = *head;
    uint32_t cut = key & 0x3fffffff;

    if (first != 0) {
        uint32_t find = key & 0x3ff0ffff;
        gpu3d_texture_entry_t *n = first;
        for (;;) {
            if (n->key == find) {
                if (n->sub == (uint16_t)sub) {
                    if (n->dirty != 0)
                        gpu3d_raster_texture_cache_entry_build(n, base, cut, sub);
                    return n;
                }
            }
            n = n->bucket_next;
            if (n == 0) break;
        }
    }

    gpu3d_texture_entry_t *n = (gpu3d_texture_entry_t *)((void *(*)(unsigned long))sym_libc_malloc)(sizeof *n);

    n->data = 0;
    n->palette = 0;
    gpu3d_raster_texture_cache_entry_build(n, base, cut, sub);

    n->bucket_next = first;
    n->bucket_prev = 0;
    n->bucket = (uint16_t)bucket_index;
    if (first != 0) first->bucket_prev = n;
    *head = n;

    gpu3d_texture_entry_t *gp = base->head;
    n->next = gp;
    n->prev = 0;
    if (gp != 0) gp->prev = n;

    uint32_t count = base->count;
    base->head = n;
    base->count = count + 1;

    return n;
}

void *gpu3d_raster_texture_cache_init(gpu3d_texture_cache_t *obj, void *origin) {
    static void *(*ms)(void *, int, size_t);
    if (!ms) ms = (void *(*)(void *, int, size_t))sym_libc_memset;
    ms(obj->bucket, 0, sizeof obj->bucket);
    obj->gpu = (struct gpu *)origin;
    *(gpu3d_texture_entry_t *volatile *)&obj->head = 0;
    *(volatile uint32_t *)&obj->dirty_pending[0] = 0;
    *(volatile uint32_t *)&obj->dirty_pending[1] = 0;
    return obj->bucket;
}

typedef void (*fnp_free)(void *);


void gpu3d_raster_texture_cache_flush_entries(gpu3d_texture_cache_t *obj)
{
    static fnp_free core_free;
    gpu3d_texture_entry_t *node = obj->head;

    if (node) {
        const unsigned char *table = texture_format_bpp;
        if (!core_free)
            core_free = (fnp_free)sym_libc_free;

        gpu3d_texture_entry_t *mark = node->bucket_prev;
        gpu3d_texture_entry_t *next = node->next;

        for (;;) {
            if (mark == 0) {
                uint16_t idx = node->bucket;
                obj->bucket[idx] = 0;
            }

            uint8_t format = node->format;
            uint16_t width = node->width;
            uint16_t height = node->height;
            uint32_t busy = obj->occupied;
            uint8_t bpp = rd8(table + format);
            uint32_t area = (uint32_t)height * (uint32_t)width;
            obj->occupied = busy - area * (uint32_t)bpp;

            core_free(node->data);
            uint8_t *second = node->palette;
            if (second != 0)
                core_free(second);
            core_free(node);

            uint32_t count = obj->count;
            node = next;
            obj->count = count - 1;
            if (!node)
                break;

            mark = node->bucket_prev;
            next = node->next;
        }
    }

    obj->head = 0;
    obj->dirty_pending[0] = 0;
    obj->dirty_pending[1] = 0;
}

uint32_t gpu3d_raster_texture_cache_purge_dirty(gpu3d_texture_cache_t *o) {
    void (*c_free)(void *) = (void (*)(void *))(sym_libc_free);

    uint32_t act1 = o->bank_bits_a;
    uint32_t prev1 = o->bank_bits_a_previous;
    uint32_t pen1 = o->dirty_pending[0];
    uint32_t act2 = o->bank_bits_b;
    uint32_t prev2 = o->bank_bits_b_previous;
    uint32_t pen2 = o->dirty_pending[1];

    uint32_t cam1 = (prev1 ^ act1) | pen1;
    uint32_t cam2 = (prev2 ^ act2) | pen2;

    o->bank_bits_a_previous = act1;
    o->bank_bits_b_previous = act2;

    if ((cam1 | cam2) == 0) return 0;

    gpu3d_texture_entry_t *n = o->head;
    if (n == 0) { o->dirty_pending[0] = 0; o->dirty_pending[1] = 0; return 0; }

    uint32_t released = 0;
    while (n != 0) {
        gpu3d_texture_entry_t *e = n;
        n = e->next;

        if (!(e->bank_mask_a & cam1) && !(e->bank_mask_b & cam2))
            continue;
        if (e->dirty == 0) {
            e->dirty = 1;
            continue;
        }

        gpu3d_texture_entry_t *sig = e->bucket_next;
        gpu3d_texture_entry_t *prev = e->bucket_prev;
        gpu3d_texture_entry_t *pre = e->prev;

        if (prev != 0) {
            prev->bucket_next = sig;
        } else {
            uint64_t bucket = e->bucket;
            o->bucket[bucket] = sig;
        }
        if (sig != 0) sig->bucket_prev = prev;

        *((pre == 0) ? &o->head : &pre->next) = n;
        if (n != 0) n->prev = pre;

        c_free(e->data);
        uint8_t *extra = e->palette;
        if (extra != 0) c_free(extra);
        c_free(e);
        released++;
    }

    o->dirty_pending[0] = 0;
    o->dirty_pending[1] = 0;
    return released;
}

static void (*core_free)(void *);

void *gpu3d_raster_texture_cache_reset(gpu3d_texture_cache_t *obj) {
    if (!core_free) core_free = (void (*)(void *))sym_libc_free;

    const unsigned char *table = texture_format_bpp;
    gpu3d_texture_entry_t *node = obj->head;

    while (node) {
        void *mark = node->bucket_prev;
        gpu3d_texture_entry_t *next = node->next;

        if (mark == 0) {
            uint32_t idx = node->bucket;
            obj->bucket[idx] = 0;
        }

        uint32_t format = node->format;
        uint32_t width   = node->width;
        uint32_t height    = node->height;
        uint32_t busy = obj->occupied;
        uint32_t bpp     = table[format];

        obj->occupied = busy - (height * width) * bpp;

        core_free(node->data);
        void *second = node->palette;
        if (second) core_free(second);
        core_free(node);

        uint32_t count = obj->count;
        obj->count = count - 1;
        node = next;
    }

    obj->head = 0;
    obj->dirty_pending[0] = 0;
    obj->dirty_pending[1] = 0;
    obj->bank_bits_a = 0;
    obj->bank_bits_a_previous = 0;
    obj->bank_bits_b = 0;
    obj->bank_bits_b_previous = 0;
    return obj;
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"

void gpu3d_raster_wrap_texel_clamp_clamp(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {
#ifdef __ARM_NEON

    {
        const int16_t *pxy = xy;
        const uint8_t *pv = val;
        uint32_t *pd = dst;
        int32_t k = n;
        int16x8_t vWidth   = vdupq_n_s16((int16_t)width);
        int16x8_t vWidthM1 = vdupq_n_s16((int16_t)(width - 1u));
        int16x8_t vHeightM1  = vdupq_n_s16((int16_t)(height - 1u));
        int16x8_t zero     = vdupq_n_s16(0);
        do {
            int16x8x2_t p = vld2q_s16(pxy); pxy += 16;
            int16x8_t m = vmovl_s8(vreinterpret_s8_u8(vld1_u8(pv))); pv += 8;
            int16x8_t x = vandq_s16(vminq_s16(vmaxq_s16(zero, p.val[0]), vWidthM1), m);
            int16x8_t y = vandq_s16(vminq_s16(vmaxq_s16(zero, p.val[1]), vHeightM1), m);
            uint16x8_t xu = vreinterpretq_u16_s16(x);
            uint16x8_t yu = vreinterpretq_u16_s16(y);
            uint16x8_t au = vreinterpretq_u16_s16(vWidth);
            uint32x4_t lo = vmlal_u16(vmovl_u16(vget_low_u16(xu)),
                                      vget_low_u16(yu), vget_low_u16(au));
            uint32x4_t hi = vmlal_high_u16(vmovl_high_u16(xu), yu, au);
            vst1q_u32(pd, lo); pd += 4;
            vst1q_u32(pd, hi); pd += 4;
            k -= 8;
        } while (k > 0);
        return;
    }
#else
    int16_t mx = (int16_t)(width - 1), my = (int16_t)(height - 1);
    do {
        for (int k = 0; k < 8; k++) {
            int16_t x = xy[k * 2], y = xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            if (x < 0) x = 0;   if (x > mx) x = mx;
            if (y < 0) y = 0;   if (y > my) y = my;
            x = (int16_t)(x & m);
            y = (int16_t)(y & m);
            dst[k] = (uint32_t)(uint16_t)x
                   + (uint32_t)(uint16_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16; val += 8; dst += 8; n -= 8;
    } while (n > 0);
#endif
}

void gpu3d_raster_wrap_texel_mirror_repeat(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {
    uint16_t mx = (uint16_t)(width - 1);
    uint16_t mirror = (uint16_t)(width & (uint32_t)~(width - 1));
    uint16_t my = (uint16_t)(height - 1);
    do {
        for (int k = 0; k < 8; k++) {
            uint16_t x = (uint16_t)xy[k * 2];
            uint16_t y = (uint16_t)xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            if (x & mirror) x = (uint16_t)~x;
            x &= mx;
            y &= my;
            x = (uint16_t)(x & m);
            y = (uint16_t)(y & m);
            dst[k] = (uint32_t)x + (uint32_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16; val += 8; dst += 8; n -= 8;
    } while (n > 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"

void gpu3d_raster_wrap_texel_mirror_clamp(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {

#ifdef __ARM_NEON

    {
        uint16x8_t vwidth = vdupq_n_u16((uint16_t)width);
        uint16x8_t vmx  = vdupq_n_u16((uint16_t)(width - 1));
        uint16x8_t vmirror = vbicq_u16(vdupq_n_u16((uint16_t)width), vmx);
        int16x8_t  vmy  = vdupq_n_s16((int16_t)(height - 1));
        const int16_t *pxy = xy; const uint8_t *pv = val;
        uint32_t *pd = dst; int32_t k = n;
        do {
            int16x8x2_t t = vld2q_s16(pxy); pxy += 16;
            int16x8_t m = vmovl_s8(vreinterpret_s8_u8(vld1_u8(pv))); pv += 8;
            uint16x8_t x = vreinterpretq_u16_s16(t.val[0]);
            uint16x8_t tst = vtstq_u16(x, vmirror);
            int16x8_t y = vmaxq_s16(vdupq_n_s16(0), t.val[1]);
            x = veorq_u16(x, tst);
            y = vminq_s16(y, vmy);
            x = vandq_u16(x, vmx);
            x = vandq_u16(x, vreinterpretq_u16_s16(m));
            uint16x8_t yu = vandq_u16(vreinterpretq_u16_s16(y),
                                      vreinterpretq_u16_s16(m));
            uint32x4_t lo = vmovl_u16(vget_low_u16(x));
            uint32x4_t hi = vmovl_high_u16(x);
            lo = vmlal_u16(lo, vget_low_u16(yu), vget_low_u16(vwidth));
            hi = vmlal_high_u16(hi, yu, vwidth);
            vst1q_u32(pd, lo); vst1q_u32(pd + 4, hi); pd += 8;
            k -= 8;
        } while (k > 0);
        return;
    }
#else

    int16_t mx = (int16_t)(width - 1);
    int16_t mirror_bit = (int16_t)(width & ~mx);
    int16_t ymax = (int16_t)(height - 1);
    do {
        for (int k = 0; k < 8; k++) {
            int16_t x = xy[k * 2];
            int16_t y = xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            if ((x & mirror_bit) != 0) x = (int16_t)~x;
            x = (int16_t)(x & mx);
            if (y < 0) y = 0;
            if (y > ymax) y = ymax;
            x = (int16_t)(x & m);
            y = (int16_t)(y & m);
            dst[k] = (uint32_t)(uint16_t)x
                   + (uint32_t)(uint16_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16;
        val += 8;
        dst += 8;
        n -= 8;
    } while (n > 0);
#endif
}

void gpu3d_raster_wrap_texel_clamp_repeat(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {
    int16_t mx = (int16_t)(width - 1);
    int16_t my = (int16_t)(height - 1);
    do {
        for (int k = 0; k < 8; k++) {
            int16_t x = xy[k * 2];
            int16_t y = xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            if (x < 0) x = 0;
            if (x > mx) x = mx;
            y = (int16_t)(y & my);
            x = (int16_t)(x & m);
            y = (int16_t)(y & m);
            dst[k] = (uint32_t)(uint16_t)x
                   + (uint32_t)(uint16_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16;
        val += 8;
        dst += 8;
        n -= 8;
    } while (n > 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_wrap_texel_repeat_repeat(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {
    uint32_t mx = width - 1, my = height - 1;
#ifdef __ARM_NEON

    {
        const int16x8_t vmx = vdupq_n_s16((int16_t)mx);
        const int16x8_t vmy = vdupq_n_s16((int16_t)my);
        const uint16x8_t vw = vdupq_n_u16((uint16_t)width);
        do {
            int16x8x2_t v = vld2q_s16(xy);
            int16x8_t m = vmovl_s8(vreinterpret_s8_u8(vld1_u8(val)));
            uint16x8_t X = vreinterpretq_u16_s16(vandq_s16(vandq_s16(v.val[0], vmx), m));
            uint16x8_t Y = vreinterpretq_u16_s16(vandq_s16(vandq_s16(v.val[1], vmy), m));
            vst1q_u32(dst,     vmlal_u16(vmovl_u16(vget_low_u16(X)),
                                         vget_low_u16(Y),  vget_low_u16(vw)));
            vst1q_u32(dst + 4, vmlal_u16(vmovl_u16(vget_high_u16(X)),
                                         vget_high_u16(Y), vget_high_u16(vw)));
            xy += 16;
            val += 8;
            dst += 8;
            n -= 8;
        } while (n > 0);
    }
#else
    do {
        for (int k = 0; k < 8; k++) {
            int32_t x = xy[k * 2];
            int32_t y = xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            x = (x & (int32_t)mx) & m;
            y = (y & (int32_t)my) & m;
            dst[k] = (uint32_t)(uint16_t)x
                   + (uint32_t)(uint16_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16;
        val += 8;
        dst += 8;
        n -= 8;
    } while (n > 0);
#endif
}

void gpu3d_raster_wrap_texel_repeat_clamp(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {
    uint32_t mx = width - 1;
    int16_t my = (int16_t)(height - 1);
#ifdef __ARM_NEON

    {
        const int16x8_t vmx = vdupq_n_s16((int16_t)mx);
        const int16x8_t vmy = vdupq_n_s16(my);
        const int16x8_t zero = vdupq_n_s16(0);
        const uint16x8_t vw = vdupq_n_u16((uint16_t)width);
        do {
            int16x8x2_t v = vld2q_s16(xy);
            int16x8_t m = vmovl_s8(vreinterpret_s8_u8(vld1_u8(val)));
            uint16x8_t X = vreinterpretq_u16_s16(vandq_s16(vandq_s16(v.val[0], vmx), m));
            int16x8_t y = vminq_s16(vmaxq_s16(zero, v.val[1]), vmy);
            uint16x8_t Y = vreinterpretq_u16_s16(vandq_s16(y, m));
            vst1q_u32(dst,     vmlal_u16(vmovl_u16(vget_low_u16(X)),
                                         vget_low_u16(Y),  vget_low_u16(vw)));
            vst1q_u32(dst + 4, vmlal_u16(vmovl_u16(vget_high_u16(X)),
                                         vget_high_u16(Y), vget_high_u16(vw)));
            xy += 16;
            val += 8;
            dst += 8;
            n -= 8;
        } while (n > 0);
    }
#else
    do {
        for (int k = 0; k < 8; k++) {
            int32_t x = xy[k * 2];
            int16_t y = xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            x = (x & (int32_t)mx) & m;
            if (y < 0) y = 0;
            if (y > my) y = my;
            y = (int16_t)(y & m);
            dst[k] = (uint32_t)(uint16_t)x
                   + (uint32_t)(uint16_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16;
        val += 8;
        dst += 8;
        n -= 8;
    } while (n > 0);
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"

void gpu3d_raster_wrap_texel_clamp_mirror(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {

#ifdef __ARM_NEON

    {
        uint16x8_t vwidth = vdupq_n_u16((uint16_t)width);
        int16x8_t  vmx  = vdupq_n_s16((int16_t)(width - 1));
        uint16x8_t vmy  = vdupq_n_u16((uint16_t)(height - 1));
        uint16x8_t vmirror = vbicq_u16(vdupq_n_u16((uint16_t)height), vmy);
        const int16_t *pxy = xy; const uint8_t *pv = val;
        uint32_t *pd = dst; int32_t k = n;
        do {
            int16x8x2_t t = vld2q_s16(pxy); pxy += 16;
            int16x8_t m = vmovl_s8(vreinterpret_s8_u8(vld1_u8(pv))); pv += 8;
            int16x8_t x = vmaxq_s16(vdupq_n_s16(0), t.val[0]);
            uint16x8_t y = vreinterpretq_u16_s16(t.val[1]);
            uint16x8_t tst = vtstq_u16(y, vmirror);
            x = vminq_s16(x, vmx);
            y = veorq_u16(y, tst);
            y = vandq_u16(y, vmy);
            uint16x8_t xu = vandq_u16(vreinterpretq_u16_s16(x),
                                      vreinterpretq_u16_s16(m));
            y = vandq_u16(y, vreinterpretq_u16_s16(m));
            uint32x4_t lo = vmovl_u16(vget_low_u16(xu));
            uint32x4_t hi = vmovl_high_u16(xu);
            lo = vmlal_u16(lo, vget_low_u16(y), vget_low_u16(vwidth));
            hi = vmlal_high_u16(hi, y, vwidth);
            vst1q_u32(pd, lo); vst1q_u32(pd + 4, hi); pd += 8;
            k -= 8;
        } while (k > 0);
        return;
    }
#else

    int16_t mx = (int16_t)(width - 1);
    uint16_t my = (uint16_t)(height - 1);
    uint16_t mirror = (uint16_t)(height & (uint32_t)~(height - 1));
    do {
        for (int k = 0; k < 8; k++) {
            int16_t x = (int16_t)xy[k * 2];
            uint16_t y = (uint16_t)xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            if (x < 0) x = 0;
            if (x > mx) x = mx;
            if (y & mirror) y = (uint16_t)~y;
            y &= my;
            x = (int16_t)((uint16_t)x & (uint16_t)m);
            y = (uint16_t)(y & m);
            dst[k] = (uint32_t)(uint16_t)x + (uint32_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16; val += 8; dst += 8; n -= 8;
    } while (n > 0);
#endif
}

void gpu3d_raster_wrap_texel_repeat_mirror(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {
    uint16_t mx = (uint16_t)(width - 1);
    uint16_t my = (uint16_t)(height - 1);
    uint16_t mirror = (uint16_t)(height & (uint32_t)~(height - 1));
    do {
        for (int k = 0; k < 8; k++) {
            uint16_t x = (uint16_t)xy[k * 2];
            uint16_t y = (uint16_t)xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            x &= mx;
            if (y & mirror) y = (uint16_t)~y;
            y &= my;
            x = (uint16_t)(x & m);
            y = (uint16_t)(y & m);
            dst[k] = (uint32_t)x + (uint32_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16; val += 8; dst += 8; n -= 8;
    } while (n > 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_wrap_texel_mirror_mirror(uint32_t *dst, const int16_t *xy, int32_t n,
                        uint32_t width, uint32_t height, const uint8_t *val) {
    uint16_t mx = (uint16_t)(width - 1);
    uint16_t my = (uint16_t)(height - 1);
    uint16_t ex = (uint16_t)(width & (uint32_t)~(width - 1));
    uint16_t ey = (uint16_t)(height & (uint32_t)~(height - 1));
#ifdef __ARM_NEON

    {
        const int16x8_t vmx = vdupq_n_s16((int16_t)mx);
        const int16x8_t vmy = vdupq_n_s16((int16_t)my);
        const int16x8_t vex = vdupq_n_s16((int16_t)ex);
        const int16x8_t vey = vdupq_n_s16((int16_t)ey);
        const uint16x8_t vw = vdupq_n_u16((uint16_t)width);
        do {
            int16x8x2_t v = vld2q_s16(xy);
            int16x8_t m = vmovl_s8(vreinterpret_s8_u8(vld1_u8(val)));
            int16x8_t x = veorq_s16(v.val[0],
                              vreinterpretq_s16_u16(vtstq_s16(v.val[0], vex)));
            int16x8_t y = veorq_s16(v.val[1],
                              vreinterpretq_s16_u16(vtstq_s16(v.val[1], vey)));
            uint16x8_t X = vreinterpretq_u16_s16(vandq_s16(vandq_s16(x, vmx), m));
            uint16x8_t Y = vreinterpretq_u16_s16(vandq_s16(vandq_s16(y, vmy), m));
            vst1q_u32(dst,     vmlal_u16(vmovl_u16(vget_low_u16(X)),
                                         vget_low_u16(Y),  vget_low_u16(vw)));
            vst1q_u32(dst + 4, vmlal_u16(vmovl_u16(vget_high_u16(X)),
                                         vget_high_u16(Y), vget_high_u16(vw)));
            xy += 16; val += 8; dst += 8; n -= 8;
        } while (n > 0);
        return;
    }
#endif
    do {
        for (int k = 0; k < 8; k++) {
            uint16_t x = (uint16_t)xy[k * 2];
            uint16_t y = (uint16_t)xy[k * 2 + 1];
            int32_t m = (int8_t)val[k];
            if (x & ex) x = (uint16_t)~x;
            if (y & ey) y = (uint16_t)~y;
            x &= mx;
            y &= my;
            x = (uint16_t)(x & m);
            y = (uint16_t)(y & m);
            dst[k] = (uint32_t)x + (uint32_t)y * (uint32_t)(uint16_t)width;
        }
        xy += 16; val += 8; dst += 8; n -= 8;
    } while (n > 0);
}

void gpu3d_raster_texel_table_lookup_chained(uint32_t *dst, const uint32_t *src,
                        const unsigned char *idx, const uint32_t *palette,
                        int64_t n) {
    do {
        uint32_t t[8];
        for (int k = 0; k < 8; k++) t[k] = src[k];
        src += 8;
        n -= 8;
        for (int k = 0; k < 8; k++) t[k] = idx[t[k]];
        for (int k = 0; k < 8; k++) t[k] = palette[t[k]];
        for (int k = 0; k < 8; k++) dst[k] = t[k];
        dst += 8;
    } while (n > 0);
}

void gpu3d_raster_texel_table_lookup(uint32_t *dst, const uint32_t *src,
                        const uint32_t *table, int32_t n) {
    do {
        uint32_t t[8];
        for (int k = 0; k < 8; k++) t[k] = src[k];
        src += 8;
        n -= 8;
        for (int k = 0; k < 8; k++) t[k] = table[t[k]];
        for (int k = 0; k < 8; k++) dst[k] = t[k];
        dst += 8;
    } while (n > 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <string.h>

void *gpu3d_raster_texel_blend_planar_alpha(uint8_t *dst, const uint8_t *src_fg,
                          const uint8_t *src_bg_planes, uint32_t stride,
                          uint32_t alpha_const, int32_t count)
{
#ifdef __ARM_NEON

    const uint8_t *pR = src_bg_planes;
    const uint8_t *pG = src_bg_planes + stride;
    const uint8_t *pB = src_bg_planes + stride * 2u;
    const uint8_t *pFg = src_fg;
    uint8_t *pDst = dst;
    uint8x16_t vA = vdupq_n_u8((uint8_t)(alpha_const & 0xffu));
    int32_t n = count;

    do {
        uint8x16_t b0 = vld1q_u8(pR); pR += 16;
        uint8x16_t b1 = vld1q_u8(pG); pG += 16;
        uint8x16_t b2 = vld1q_u8(pB); pB += 16;
        uint8x16x4_t f = vld4q_u8(pFg); pFg += 64;
        uint8x16x4_t o;

        uint16x8_t l0 = vaddl_u8(vget_low_u8(b0), vget_low_u8(f.val[0]));
        uint16x8_t l1 = vaddl_u8(vget_low_u8(b1), vget_low_u8(f.val[1]));
        uint16x8_t l2 = vaddl_u8(vget_low_u8(b2), vget_low_u8(f.val[2]));
        uint16x8_t l3 = vaddl_u8(vget_low_u8(vA), vget_low_u8(f.val[3]));
        uint16x8_t h0 = vaddl_high_u8(b0, f.val[0]);
        uint16x8_t h1 = vaddl_high_u8(b1, f.val[1]);
        uint16x8_t h2 = vaddl_high_u8(b2, f.val[2]);
        uint16x8_t h3 = vaddl_high_u8(vA, f.val[3]);

        l0 = vmlal_u8(l0, vget_low_u8(b0), vget_low_u8(f.val[0]));
        l1 = vmlal_u8(l1, vget_low_u8(b1), vget_low_u8(f.val[1]));
        l2 = vmlal_u8(l2, vget_low_u8(b2), vget_low_u8(f.val[2]));
        l3 = vmlal_u8(l3, vget_low_u8(vA), vget_low_u8(f.val[3]));
        h0 = vmlal_high_u8(h0, b0, f.val[0]);
        h1 = vmlal_high_u8(h1, b1, f.val[1]);
        h2 = vmlal_high_u8(h2, b2, f.val[2]);
        h3 = vmlal_high_u8(h3, vA, f.val[3]);

        o.val[0] = vcombine_u8(vshrn_n_u16(l0, 6), vshrn_n_u16(h0, 6));
        o.val[1] = vcombine_u8(vshrn_n_u16(l1, 6), vshrn_n_u16(h1, 6));
        o.val[2] = vcombine_u8(vshrn_n_u16(l2, 6), vshrn_n_u16(h2, 6));
        o.val[3] = vcombine_u8(vshrn_n_u16(l3, 5), vshrn_n_u16(h3, 5));
        vst4q_u8(pDst, o); pDst += 64;
        n -= 16;
    } while (n > 0);
    return pDst;
#else

    const uint8_t *pR = src_bg_planes;
    const uint8_t *pG = src_bg_planes + stride;
    const uint8_t *pB = src_bg_planes + stride * 2u;
    const uint8_t *pFg = src_fg;
    uint8_t *pDst = dst;
    uint8_t aconst = (uint8_t)(alpha_const & 0xffu);
    int32_t n = count;

    do {
        int i;
        for (i = 0; i < 16; i++) {
            uint8_t bgR = pR[i];
            uint8_t bgG = pG[i];
            uint8_t bgB = pB[i];
            uint8_t fgR = pFg[i * 4 + 0];
            uint8_t fgG = pFg[i * 4 + 1];
            uint8_t fgB = pFg[i * 4 + 2];
            uint8_t fgA = pFg[i * 4 + 3];

            uint32_t tR = (uint32_t)bgR + fgR + (uint32_t)bgR * fgR;
            uint32_t tG = (uint32_t)bgG + fgG + (uint32_t)bgG * fgG;
            uint32_t tB = (uint32_t)bgB + fgB + (uint32_t)bgB * fgB;
            uint32_t tA = (uint32_t)aconst + fgA + (uint32_t)aconst * fgA;

            pDst[i * 4 + 0] = (uint8_t)(tR >> 6);
            pDst[i * 4 + 1] = (uint8_t)(tG >> 6);
            pDst[i * 4 + 2] = (uint8_t)(tB >> 6);
            pDst[i * 4 + 3] = (uint8_t)(tA >> 5);
        }

        pR += 16;
        pG += 16;
        pB += 16;
        pFg += 64;
        pDst += 64;
        n -= 16;
    } while (n > 0);

    return pDst;
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void gpu3d_raster_texel_index_expand_planes(const unsigned char *tables, unsigned char *buf,
                        uint32_t stride, int32_t n) {

    unsigned char ta[32], tb[32], tc[32];
    for (int k = 0; k < 32; k++) ta[k] = tables[k];
    for (int k = 0; k < 32; k++) tb[k] = tables[32 + k];
    for (int k = 0; k < 32; k++) tc[k] = tables[64 + k];

    unsigned char *p1 = buf;
    unsigned char *p2 = buf + (uint64_t)stride;
    unsigned char *p3 = buf + (uint64_t)stride * 2;

#ifdef __ARM_NEON

    {
        const uint8x16x2_t T0 = { { vld1q_u8(ta), vld1q_u8(ta + 16) } };
        const uint8x16x2_t T1 = { { vld1q_u8(tb), vld1q_u8(tb + 16) } };
        const uint8x16x2_t T2 = { { vld1q_u8(tc), vld1q_u8(tc + 16) } };
        do {
            uint8x16_t i = vshrq_n_u8(vld1q_u8(p1), 1);
            vst1q_u8(p1, vqtbl2q_u8(T0, i));
            vst1q_u8(p2, vqtbl2q_u8(T1, i));
            vst1q_u8(p3, vqtbl2q_u8(T2, i));
            p1 += 16;
            p2 += 16;
            p3 += 16;
            n -= 16;
        } while (n > 0);
    }
#else
    do {
        unsigned char idx[16];
        for (int k = 0; k < 16; k++) idx[k] = (unsigned char)(p1[k] >> 1);
        for (int k = 0; k < 16; k++) {
            unsigned char i = idx[k];
            p1[k] = (i < 32) ? ta[i] : 0;
            p2[k] = (i < 32) ? tb[i] : 0;
            p3[k] = (i < 32) ? tc[i] : 0;
        }
        p1 += 16;
        p2 += 16;
        p3 += 16;
        n -= 16;
    } while (n > 0);
#endif
}

void *gpu3d_raster_texel_blend_mono_alpha(unsigned char *dst,
                          const unsigned char *fg,
                          const unsigned char *plane,
                          unsigned char alpha_const,
                          long count)
{

    unsigned char *pDst = dst;
    const unsigned char *pFg = fg;
    const unsigned char *pPlane = plane;
    uint8_t aconst = (uint8_t)(alpha_const & 0xffu);
    long n = count;

    do {
        int i;
        for (i = 0; i < 16; i++) {
            uint8_t v0 = pPlane[i];

            uint8_t fgR = pFg[i * 4 + 0];
            uint8_t fgG = pFg[i * 4 + 1];
            uint8_t fgB = pFg[i * 4 + 2];
            uint8_t fgA = pFg[i * 4 + 3];

            uint32_t tR = (uint32_t)v0 + fgR + (uint32_t)v0 * fgR;
            uint32_t tG = (uint32_t)v0 + fgG + (uint32_t)v0 * fgG;
            uint32_t tB = (uint32_t)v0 + fgB + (uint32_t)v0 * fgB;
            uint32_t tA = (uint32_t)aconst + fgA + (uint32_t)aconst * fgA;

            pDst[i * 4 + 0] = (uint8_t)(tR >> 6);
            pDst[i * 4 + 1] = (uint8_t)(tG >> 6);
            pDst[i * 4 + 2] = (uint8_t)(tB >> 6);
            pDst[i * 4 + 3] = (uint8_t)(tA >> 5);
        }

        pPlane += 16;
        pFg += 64;
        pDst += 64;
        n -= 16;
    } while (n > 0);

    return pDst;
}

void gpu3d_raster_texel_unpack_nibbles(uint8_t *output, const uint8_t *entry, int count) {

    for (int i = 0; i < count; i++) {
        uint8_t b = entry[i];
        output[2 * i]     = (uint8_t)(b & 0x0f);
        output[2 * i + 1] = (uint8_t)(b >> 4);
    }
}

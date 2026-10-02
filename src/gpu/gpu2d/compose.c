#include "core_internals.h"
#include "hires_runtime.h"
#include <stdint.h>
#include <string.h>
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <stddef.h>
#include <time.h>
#include "gpu/gpu3d/raster/raster.h"
#include "mem_access.h"




void gpu2d_line_split_bgr555_planes(unsigned char *dst, const unsigned char *src)
{

    unsigned int i;
    for (i = 0; i < 0x100u; i++) {
        uint16_t val = rd16(src + (size_t)i * 2u);

        dst[i]         = (unsigned char)((val & 0x1fu) << 1);
        dst[i + 0x100] = (unsigned char)((val >> 4) & 0x3eu);
        dst[i + 0x200] = (unsigned char)((val >> 9) & 0x3eu);
    }
}

#define SOURCE    24
#define LAYER_OFF  0x10
#define LIST_STRIDE  8
#define LAYER_STRIDE 0x20
#define BUF       0x200
#define ADJUST    0xa0
extern void gpu2d_line_apply_layer_masked_1(uint16_t *dest, const uint16_t *a, const uint16_t *b, const uint32_t *mask) __asm__("gpu2d_line_apply_layer_masked");
extern void gpu2d_line_apply_flat_color_masked(uint16_t *dest, const uint16_t *origin, int color, const uint32_t *mask);
extern void gpu2d_compose_split_rgb_planes_1(uint8_t *dest, const uint16_t *origin) __asm__("gpu2d_compose_split_rgb_planes");
extern void gpu2d_planes_split_interleaved_masked_1(uint8_t *dest, uint8_t *quarter, const uint8_t *source, const uint16_t *mask) __asm__("gpu2d_planes_split_interleaved_masked");




void gpu2d_compose_blend_marked_layers(uint8_t *ctx, void *output, uint8_t *layers,
                        uint8_t *lists, void *extra, void *extra2,
                        uint32_t mask)
{

    uint8_t stack[544] __attribute__((aligned(16)));
    uint8_t *buf = stack;

    void    *acc = 0;
    uint32_t done = 0;

    if (mask != 0) {
        uint8_t *lst = lists;
        uint8_t *cap = layers;
        uint32_t m = mask;
        for (;;) {
            if (m & 1) {
                uint8_t *new = (uint8_t *)rd_ptr(lst) + LAYER_OFF;
                if (done == 0) {
                    acc = new;
                } else {
                    gpu2d_line_apply_layer_masked((uint16_t *)buf, (const uint16_t *)acc,
                                       (const uint16_t *)new, (const uint32_t *)cap);
                    acc = buf;
                }
                done++;
            }
            m >>= 1;
            lst += LIST_STRIDE;
            cap += LAYER_STRIDE;
            if (m == 0) break;
        }
    }

    uint8_t *org = (uint8_t *)rd_ptr(ctx + SOURCE);

    if (done != 0) {
        gpu2d_line_apply_flat_color_masked((uint16_t *)buf, (const uint16_t *)acc, rd16(org),
                           (const uint32_t *)(layers + ADJUST));
    } else if ((uintptr_t)buf >= (uintptr_t)org + 1
            || (uintptr_t)buf + BUF <= (uintptr_t)org) {

        for (int k = 0; k < 16; k++) {
            uint16_t v = rd16(org);
            for (int e = 0; e < 16; e++)
                wr16(buf + k * 32 + e * 2, v);
        }
    } else {

        for (uint64_t o = 0; o < BUF; o += 2)
            wr16(buf + o, rd16(org));
    }

    gpu2d_compose_split_rgb_planes((uint8_t *)output, (const uint16_t *)buf);

    if (extra != 0 && (mask & 1))
        gpu2d_planes_split_interleaved_masked((uint8_t *)output, (uint8_t *)extra2,
                           (const uint8_t *)extra, (const uint16_t *)layers);
}
#undef SOURCE
#undef LAYER_OFF
#undef LIST_STRIDE
#undef LAYER_STRIDE
#undef BUF
#undef ADJUST

void gpu2d_window_mask_set_or_clear(unsigned char *state, const unsigned char *source,
                        uint32_t mode, uint32_t enable) {
    uint32_t mask = 0u - enable;
    uint32_t bit = mode & 1u;
    uint32_t put = 0u - bit;
    uint32_t drop = bit - 1u;
    for (int pair = 0; pair < 4; pair++) {
        uint32_t d0, d1;
        memcpy(&d0, state + pair * 8 + 0, 4);
        memcpy(&d1, state + pair * 8 + 4, 4);
        uint32_t s0, s1;
        memcpy(&s0, source + pair * 8 + 0, 4);
        s0 &= mask;
        uint32_t r0 = (d0 | (s0 & put)) & ~(s0 & drop);
        memcpy(state + pair * 8 + 0, &r0, 4);
        memcpy(&s1, source + pair * 8 + 4, 4);
        s1 &= mask;
        uint32_t r1 = (d1 | (s1 & put)) & ~(s1 & drop);
        memcpy(state + pair * 8 + 4, &r1, 4);
    }
}

static void or8(unsigned char *dst, const unsigned char *a) {
    for (int i = 0; i < 8; i++) {
        uint32_t d, av;
        memcpy(&d, dst + i * 4, 4);
        memcpy(&av, a + i * 4, 4);
        d |= av;
        memcpy(dst + i * 4, &d, 4);
    }
}

static void andnot_or8(unsigned char *dst, const unsigned char *a,
                        const unsigned char *b) {
    for (int i = 0; i < 8; i++) {
        uint32_t d, av, bv;
        memcpy(&d, dst + i * 4, 4);
        memcpy(&av, a + i * 4, 4);
        memcpy(&bv, b + i * 4, 4);
        d |= bv & ~av;
        memcpy(dst + i * 4, &d, 4);
    }
}

static void nor_or8(unsigned char *dst, const unsigned char *a,
                     const unsigned char *b) {
    for (int i = 0; i < 8; i++) {
        uint32_t d, av, bv;
        memcpy(&d, dst + i * 4, 4);
        memcpy(&av, a + i * 4, 4);
        memcpy(&bv, b + i * 4, 4);
        d |= ~(av | bv);
        memcpy(dst + i * 4, &d, 4);
    }
}

void gpu2d_window_regions_apply_three(unsigned char *table, unsigned char *param_2,
                         uint32_t param_3, const unsigned char *param_4,
                         const unsigned char *param_5, uint32_t param_6,
                         uint32_t param_7, uint32_t param_8) {

    uint32_t mask1 = param_6 & param_3;

    if ((param_6 >> 5) & 1u) {
        or8(param_2, param_4);
    }

    if (mask1 != 0) {
        uint32_t m = mask1;
        unsigned char *entry = table;
        for (;;) {
            if (m & 1u) {
                or8(entry, param_4);
            }
            if ((m >> 1) == 0) break;
            m >>= 1;
            entry += 0x20;
        }
    }

    uint32_t mask2 = param_7 & param_3;

    if ((param_7 >> 5) & 1u) {
        andnot_or8(param_2, param_4, param_5);
    }

    if (mask2 != 0) {
        uint32_t m = mask2;
        unsigned char *entry = table;
        for (;;) {
            if (m & 1u) {
                andnot_or8(entry, param_4, param_5);
            }
            if ((m >> 1) == 0) break;
            m >>= 1;
            entry += 0x20;
        }
    }

    uint32_t mask3 = param_8 & param_3;

    if ((param_8 >> 5) & 1u) {
        nor_or8(param_2, param_4, param_5);
    }

    if (mask3 != 0) {
        uint32_t m = mask3;
        unsigned char *entry = table;
        for (;;) {
            if (m & 1u) {
                nor_or8(entry, param_4, param_5);
            }
            if ((m >> 1) == 0) break;
            m >>= 1;
            entry += 0x20;
        }
    }
}

static void or8_4(unsigned char *dst, const unsigned char *src) {
    for (int i = 0; i < 8; i++) {
        uint32_t d, s;
        memcpy(&d, dst + i * 4, 4);
        memcpy(&s, src + i * 4, 4);
        d |= s;
        memcpy(dst + i * 4, &d, 4);
    }
}

static void orn8(unsigned char *dst, const unsigned char *src) {
    for (int i = 0; i < 8; i++) {
        uint32_t d, s;
        memcpy(&d, dst + i * 4, 4);
        memcpy(&s, src + i * 4, 4);
        d |= ~s;
        memcpy(dst + i * 4, &d, 4);
    }
}

void gpu2d_window_regions_apply_two(unsigned char *table, unsigned char *param_2,
                         uint32_t param_3, const unsigned char *param_4,
                         uint32_t param_5, uint32_t param_6)
{

    uint32_t mask1 = param_5 & param_3;

    if ((param_5 >> 5) & 1u) {
        or8_4(param_2, param_4);
    }

    if (mask1 != 0) {
        uint32_t m = mask1;
        unsigned char *entry = table;
        for (;;) {
            if (m & 1u) {
                or8_4(entry, param_4);
            }
            if ((m >> 1) == 0) break;
            m >>= 1;
            entry += 0x20;
        }
    }

    param_3 = param_6 & param_3;

    if ((param_6 >> 5) & 1u) {
        orn8(param_2, param_4);
    }

    if (param_3 != 0) {
        uint32_t m = param_3;
        unsigned char *entry = table;
        for (;;) {
            if (m & 1u) {
                orn8(entry, param_4);
            }
            if ((m >> 1) == 0) break;
            m >>= 1;
            entry += 0x20;
        }
    }
}

static void put_block_ff(void *p) {
    const uint64_t some = UINT64_MAX;

    memcpy((uint8_t *)p + 0, &some, sizeof(some));
    memcpy((uint8_t *)p + 8, &some, sizeof(some));
    memcpy((uint8_t *)p + 16, &some, sizeof(some));
    memcpy((uint8_t *)p + 24, &some, sizeof(some));
}

void gpu2d_window_regions_fill_all(void *param_1, void *param_2, uint32_t param_3,
                         uint32_t param_4) {

    uint32_t mask = param_4 & param_3;
    uint8_t *dest = (uint8_t *)param_1;

    if ((param_4 & 0x20u) != 0)
        put_block_ff(param_2);

    while (mask != 0) {
        if ((mask & 1u) != 0)
            put_block_ff(dest);
        mask >>= 1;
        dest += 32;
    }
}

extern void gpu2d_window_x_mask_build(uint32_t *param_1, uint32_t param_2);
extern void gpu2d_window_regions_apply_three_6(unsigned char *table, unsigned char *param_2,
                                uint32_t param_3, const unsigned char *param_4,
                                const unsigned char *param_5, uint32_t param_6,
                                uint32_t param_7, uint32_t param_8) __asm__("gpu2d_window_regions_apply_three");
extern void gpu2d_window_regions_apply_two_6(unsigned char *table, unsigned char *param_2,
                                uint32_t param_3, const unsigned char *param_4,
                                uint32_t param_5, uint32_t param_6) __asm__("gpu2d_window_regions_apply_two");
#define BLOCK     32
#define OUT_SIZE     160



void gpu2d_compose_dispatch_layer_mix(gpu2d_engine_t *layer, uint8_t *output, uint8_t *coverage,
                        void *extra, uint32_t filter, uint32_t prio)
{

    uint32_t reg = layer->dispcnt;
    memset(coverage, 0, BLOCK);

    uint32_t active = (reg >> 13) & 7u;
    if (active == 0) return;

    uint32_t pa   = layer->win0v;
    uint32_t pb   = layer->win1v;
    uint32_t base = layer->window_flags;
    uint32_t pend = layer->window_dirty;

    memset(output, 0, OUT_SIZE);

    uint32_t *v0 = layer->window_x_mask[0];
    uint32_t *v1 = layer->window_x_mask[1];

    uint32_t pal = layer->window_control;

    if (pend & 1) gpu2d_window_x_mask_build(v0, layer->win0h);

    uint32_t w = pal ^ 0x3f3f3f3fu;

    uint32_t hi_a = pa >> 8, lo_a = pa & 0xffu;
    uint32_t hi_b = pb >> 8, lo_b = pb & 0xffu;
    uint32_t sel  = base | 4u;

    if (pend & 2) gpu2d_window_x_mask_build(v1, layer->win1h);

    uint32_t m = (hi_a == prio) ? 1u : 0u;
    m = (lo_a == prio) ? (sel & 0xfeu) : (m | sel);
    if (hi_b == prio) m |= 2u;
    if (lo_b == prio) m &= 0xfffffffdu;

    layer->window_flags = (uint8_t)m;
    layer->window_dirty = 0;

    switch (m & active) {

    case 0: {
        uint32_t bits = filter & (w >> 16);
        if (w & (1u << 21)) memset(coverage, 0xff, BLOCK);
        uint8_t *p = output;
        while (bits) {
            if (bits & 1) memset(p, 0xff, BLOCK);
            p += BLOCK;
            bits >>= 1;
        }
        return;
    }

    case 1:

        gpu2d_window_regions_apply_two(output, coverage, filter, (const unsigned char *)v0, w, w >> 16);
        return;
    case 2:
        gpu2d_window_regions_apply_two(output, coverage, filter, (const unsigned char *)v1, w >> 8, w >> 16);
        return;
    case 4:
        gpu2d_window_regions_apply_two(output, coverage, filter, (const unsigned char *)extra, w >> 24, w >> 16);
        return;

    case 3:

        gpu2d_window_regions_apply_three(output, coverage, filter, (const unsigned char *)v0, (const unsigned char *)v1,
                            w, w >> 8, w >> 16);
        return;
    case 5:
        gpu2d_window_regions_apply_three(output, coverage, filter, (const unsigned char *)v0, (const unsigned char *)extra,
                            w, w >> 24, w >> 16);
        return;
    case 6:
        gpu2d_window_regions_apply_three(output, coverage, filter, (const unsigned char *)v1, (const unsigned char *)extra,
                            w >> 8, w >> 24, w >> 16);
        return;

    case 7:

        gpu2d_window_regions_apply_four(output, coverage, filter, v0, v1, extra,
                                w, w >> 8, w >> 24, w >> 16);
        return;
    }
}
#undef BLOCK
#undef OUT_SIZE

#define SIMPLE_GROUPS 4
#define PER_GROUP      8
#define QUARTER_BASE    0x20
#define ROWS          4

void gpu2d_compose_clear_masked_groups(const uint8_t *ctx, uint32_t *dest,
                        const uint32_t *source, uint32_t mask) {

    if ((((const gpu2d_engine_t *)ctx)->dispcnt & 0xe000u) == 0) return;

    for (int b = 0; b < SIMPLE_GROUPS; b++) {
        if ((mask & (1u << b)) == 0) continue;
        for (int k = 0; k < PER_GROUP; k++) {
            int i = b * PER_GROUP + k;
            dest[i] &= ~source[i];
        }
    }

    if (mask & 0x10u) {

        for (int j = QUARTER_BASE; j < QUARTER_BASE + PER_GROUP; j++) {
            uint32_t v = ~source[j];
            for (int f = 0; f < ROWS; f++)
                dest[j + f * PER_GROUP] &= v;
        }
    }
}
#undef SIMPLE_GROUPS
#undef PER_GROUP
#undef QUARTER_BASE
#undef ROWS

static uint32_t recon_48154_or8(const unsigned char *base) {
    uint32_t w[8];
    memcpy(w, base, sizeof w);
    uint32_t o = w[1] | w[0];
    o |= w[2];
    o |= w[3];
    o |= w[4];
    o |= w[5];
    o |= w[6];
    o |= w[7];
    return o;
}

void gpu2d_compose_prune_empty_groups(const unsigned char *param_1, unsigned char *param_2) {

    uint32_t w8;
    memcpy(&w8, param_2, sizeof w8);

    if (w8 & 1u) {
        uint32_t o = recon_48154_or8(param_1 + 0);

        uint32_t m = (o != 0) ? 0xfffffffeu + 1u : 0xfffffffeu;
        w8 = m & w8;
    }
    if (w8 & 2u) {
        uint32_t o = recon_48154_or8(param_1 + 32);

        uint32_t m = (o == 0) ? 0xfffffffdu : ~0u;
        w8 = m & w8;
    }
    if (w8 & 4u) {
        uint32_t o = recon_48154_or8(param_1 + 64);

        uint32_t m = (o == 0) ? 0xfffffffbu : ~0u;
        w8 = m & w8;
    }
    if (w8 & 8u) {
        uint32_t o = recon_48154_or8(param_1 + 96);

        uint32_t m = (o == 0) ? 0xfffffff7u : ~0u;
        w8 = m & w8;
    }

    memcpy(param_2, &w8, sizeof w8);
}

void gpu2d_compose_pack_rgb555_from_planes(void *param_1, uint16_t *param_2, unsigned char *param_3) {

    unsigned char *base = (unsigned char *)param_1;

    if (*(uint16_t *)(base + 76) == 0)
        return;

    uint64_t uVar2 = 0;
    do {
        unsigned char *pbVar1 = param_3 + uVar2;

        unsigned r = pbVar1[0];
        unsigned g = pbVar1[0x100];
        unsigned b = pbVar1[0x200];

        uint16_t w9 = (uint16_t)(((g << 4) & 0xfe0) | (r >> 1));
        w9 = (uint16_t)(w9 | ((b << 9) & 0x7c00));
        w9 = (uint16_t)(w9 | 0x8000);

        param_2[uVar2] = w9;

        uVar2 = uVar2 + 1;
    } while (uVar2 < *(uint16_t *)(base + 76));
}

void gpu2d_compose_pack_capture_entries(const unsigned char *param_1, unsigned char *param_2,
                         const unsigned char *param_3) {

    uint16_t n;
    memcpy(&n, param_1 + 0x4c, sizeof n);
    if (n == 0) return;

    uint64_t i = 0;
    do {
        uint32_t v;
        memcpy(&v, param_3 + i * 4, sizeof v);

        uint32_t flag        = (uint32_t)((v >> 24) != 0);
        uint32_t field_low  = (v >> 1) & 0x7fu;
        uint32_t field_mid = (v >> 4) & 0xfe0u;
        uint32_t field_height  = (v >> 7) & 0xfc00u;

        uint16_t out = (uint16_t)(field_low | field_mid | field_height |
                                   (flag << 15));
        memcpy(param_2 + i * 2, &out, sizeof out);

        i += 1;
        memcpy(&n, param_1 + 0x4c, sizeof n);
    } while (i < n);
}

#define OFF_NEON_PLANE 0x8bd90

void gpu2d_compose_convert_planes_to_rgb555_blend(void *state, void *dest, void *mix, void *planes)
{

    const gpu_output_t *e = (const gpu_output_t *)state;

    unsigned bVar4 = e->capture.eva;
    unsigned b55   = e->capture.evb;

    if (bVar4 == 0x10 && b55 == 0) {

        gpu2d_compose_pack_planes_to_rgb15(state, dest, planes);
        return;
    }

    const void *source_b = e->capture.source_b_line;
    uint32_t n0 = e->capture.width;

    if (source_b != NULL) {

        if (n0 == 0) return;

        uint32_t factor2 = b55 * 2u;
        uint32_t i = 0;
        for (;;) {
            const uint8_t *p = (const uint8_t *)planes + i;
            uint32_t mixed = rd16((const uint8_t *)mix + (size_t)i * 2);

            uint32_t r0 = (uint32_t)p[0]     * bVar4 + (mixed & 0x1fu)         * factor2;
            uint32_t g0 = (uint32_t)p[0x100] * bVar4 + ((mixed >> 5) & 0x1fu)  * factor2;
            uint32_t b0 = (uint32_t)p[0x200] * bVar4 + ((mixed >> 10) & 0x1fu) * factor2;

            uint32_t rr = (r0 < 0x400u) ? (r0 >> 5)               : 0x1fu;
            uint32_t gg = (g0 < 0x400u) ? (g0 & 0x7fe0u)          : 0x3e0u;
            uint32_t bb = (b0 < 0x400u) ? ((b0 << 5) & 0x7c00u)   : 0x7c00u;

            wr16((uint8_t *)dest + (size_t)i * 2, gg | rr | bb | 0x8000u);

            i += 1;
            if (i >= e->capture.width) break;
        }
        return;
    }

    if (n0 == 0) return;

    {
        uint32_t i = 0;
        for (;;) {
            const uint8_t *p = (const uint8_t *)planes + i;

            uint32_t r0 = (uint32_t)p[0]     * bVar4;
            uint32_t g0 = (uint32_t)p[0x100] * bVar4;
            uint32_t b0 = (uint32_t)p[0x200] * bVar4;

            uint32_t val = (g0 & 0x7fe0u) | (r0 >> 5) | ((b0 << 5) & 0x7c00u) | 0x8000u;

            wr16((uint8_t *)dest + (size_t)i * 2, val);

            i += 1;
            if (i >= e->capture.width) break;
        }
    }
}
#undef OFF_NEON_PLANE



void gpu2d_compose_convert_texels_to_rgb555_modulated(const gpu_output_t *ctx, uint8_t *dst,
                        const uint8_t *blend_src, const uint8_t *texels) {

    const uint8_t *param_1 = (const uint8_t *)ctx;
    uint8_t *param_2 = dst;
    const uint8_t *param_3 = blend_src;
    const uint8_t *param_4 = texels;

    uint8_t byte84 = *(param_1 + 0x54);
    uint8_t byte85 = *(param_1 + 0x55);

    if (byte84 == 0x10 && byte85 == 0) {

        gpu2d_compose_pack_rgba_to_rgb15(ctx, dst, texels);
        return;
    }

    uint32_t mult = byte84;
    uint16_t count = rd16(param_1 + 0x4c);

    if (param_3 == 0) {

        if (count == 0) return;

        uint64_t idx = 0;
        do {
            uint32_t texel = rd32(param_4 + idx * 4);
            uint32_t channelR = texel & 0xff;
            uint32_t channelG = (texel >> 8) & 0xff;
            uint32_t channelB = (texel >> 16) & 0xff;

            uint32_t result =
                (((channelG * mult) & 0x7fe0u)) |
                (((channelR * mult) >> 5)) |
                (((channelB * mult) << 5) & 0x7c00u) |
                0x8000u;

            wr16(param_2 + idx * 2, (uint16_t)result);

            uint16_t count_fresh = rd16(param_1 + 0x4c);
            idx = idx + 1;
            if (!(idx < count_fresh)) break;
        } while (1);
    } else {

        if (count == 0) return;

        uint32_t iVar6 = (uint32_t)byte85 * 2;
        uint64_t idx = 0;
        do {
            uint32_t texel = rd32(param_4 + idx * 4);
            uint64_t shiftB = idx * 2;
            uint16_t vcol = rd16(param_3 + shiftB);
            idx = idx + 1;

            uint32_t channelR = texel & 0xff;
            uint32_t channelG = (texel >> 8) & 0xff;
            uint32_t channelB = (texel >> 16) & 0xff;
            uint32_t vR = vcol & 0x1f;
            uint32_t vG = (vcol >> 5) & 0x1f;
            uint32_t vB = (vcol >> 10) & 0x1f;

            uint32_t sumR = channelR * mult + vR * iVar6;
            uint32_t sumG = channelG * mult + vG * iVar6;
            uint32_t sumB = channelB * mult + vB * iVar6;

            uint32_t fieldR = (sumR < 0x400) ? (sumR >> 5) : 0x1fu;
            uint32_t fieldG = (sumG < 0x400) ? (sumG & 0x7fe0u) : 0x3e0u;
            uint32_t fieldB = (sumB < 0x400) ? ((sumB << 5) & 0x7c00u) : 0x7c00u;

            uint32_t result = fieldG | fieldR | fieldB | 0x8000u;
            wr16(param_2 + shiftB, (uint16_t)result);

            uint16_t count_fresh = rd16(param_1 + 0x4c);
            if (!(idx < count_fresh)) break;
        } while (1);
    }
}

#define OFF_SCALAR_U32   0x48434
#define OFF_SCALAR_PLANE 0x48318
#define OFF_NEON_PLANE    0x8bd90
#define OFF_NEON_U32      0x8be30

void gpu2d_compose_dispatch_line_converter(void *state, void *dest, void *arg3, void *arg4,
                        void *arg5) {

    unsigned char fmt = ((const gpu_output_t *)state)->capture.source_a_mode;

    if (fmt == 2) {

        if (arg5 == 0) return;

        unsigned char mod = ((const gpu_output_t *)state)->capture.blend;
        if (mod == 0) {

            gpu2d_compose_pack_rgba_to_rgb15(state, dest, arg5);
            return;
        }

        gpu2d_compose_convert_texels_to_rgb555_modulated(state, dest, arg3, arg5);
        return;
    }

    unsigned char mod = ((const gpu_output_t *)state)->capture.blend;
    if (mod == 0) {

        gpu2d_compose_pack_planes_to_rgb15(state, dest, arg4);
        return;
    }

    gpu2d_compose_convert_planes_to_rgb555_blend(state, dest, arg3, arg4);
}
#undef OFF_SCALAR_U32
#undef OFF_SCALAR_PLANE
#undef OFF_NEON_PLANE
#undef OFF_NEON_U32



static uint32_t high_bytes_to_word(const uint8_t *entry)
{
    uint32_t first = rd32(entry);
    uint32_t second_height = entry[7];
    uint32_t third = rd32(entry + 8);
    uint32_t quarter_height = entry[15];
    uint32_t output;

    output = (first >> 24) | (second_height << 8);
    output |= (third >> 8) & UINT32_C(0x00ff0000);
    output |= quarter_height << 24;
    return output;
}

void gpu2d_compose_extract_high_byte_plane(uint8_t *dest, const uint8_t *source)
{

    uintptr_t dir_dest = (uintptr_t)dest;
    uintptr_t dir_source = (uintptr_t)source;
    uint32_t idx;

    if (dir_source + (uintptr_t)0x400 <= dir_dest ||
        dir_dest + (uintptr_t)0x100 <= dir_source) {

        for (idx = 0; idx < 0x100; idx += 16) {
            const uint8_t *block = source + ((size_t)idx << 2);
            uint8_t loaded[64];
            uint32_t out0;
            uint32_t out1;
            uint32_t out2;
            uint32_t out3;

            memcpy(loaded, block, sizeof(loaded));
            out0 = high_bytes_to_word(loaded);
            out1 = high_bytes_to_word(loaded + 16);
            out2 = high_bytes_to_word(loaded + 32);
            out3 = high_bytes_to_word(loaded + 48);

            wr32(dest + idx, out0);
            wr32(dest + idx + 4, out1);
            wr32(dest + idx + 8, out2);
            wr32(dest + idx + 12, out3);
        }
        return;
    }

    for (idx = 0; idx < 0x100; idx += 4) {
        const uint8_t *entry = source + ((size_t)idx << 2);

        wr32(dest + idx, high_bytes_to_word(entry));
    }
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#define OFF_FILL 0x8bec8
#define ROUNDS 8
#define LOCAL   320

#ifndef __ARM_NEON
typedef struct { uint32_t w[4]; } V4;

static V4 shr(V4 a, int n)      { V4 r; for (int i=0;i<4;i++) r.w[i]=a.w[i]>>n; return r; }

static V4 shl_(V4 a, int n)     { V4 r; for (int i=0;i<4;i++) r.w[i]=a.w[i]<<n; return r; }

static V4 orr(V4 a, V4 b)       { V4 r; for (int i=0;i<4;i++) r.w[i]=a.w[i]|b.w[i]; return r; }

static V4 andd(V4 a, uint32_t m){ V4 r; for (int i=0;i<4;i++) r.w[i]=a.w[i]&m; return r; }

static V4 eor(V4 a, uint32_t m) { V4 r; for (int i=0;i<4;i++) r.w[i]=a.w[i]^m; return r; }

static V4 bsl(uint32_t mask, V4 height, V4 low) {
    V4 r;
    for (int i = 0; i < 4; i++)
        r.w[i] = (mask & height.w[i]) | (~mask & low.w[i]);
    return r;
}
#endif

int gpu2d_compose_fold_alpha_summary(uint8_t *dest, const void *origin) {

    _Alignas(16) uint8_t local[LOCAL];
    gpu2d_compose_extract_alpha_plane(local, origin);

#ifdef __ARM_NEON

    uint32x4_t acc_a = vdupq_n_u32(0);
    uint32x4_t acc_b = vdupq_n_u32(0);
    const uint32x4_t k_f0 = vdupq_n_u32(0xf0f0f0f0u);
    const uint32x4_t k_1f = vdupq_n_u32(0x1f1f1f1fu);
    const uint32x4_t k_11 = vdupq_n_u32(0x11111111u);

    for (int v = 0; v < ROUNDS; v++) {
        uint32x4x2_t pair = vld2q_u32((const uint32_t *)(local + v * 32));
        uint32x4_t even = pair.val[0], odd = pair.val[1];

        uint32x4_t a = vorrq_u32(vshrq_n_u32(even, 4), even);
        uint32x4_t b = vorrq_u32(vshlq_n_u32(odd, 4), odd);
        uint32x4_t m1 = vbslq_u32(k_f0, b, a);

        uint32x4_t pe = veorq_u32(even, k_1f);
        uint32x4_t ie = veorq_u32(odd, k_1f);
        uint32x4_t c  = vorrq_u32(vshrq_n_u32(pe, 4), pe);
        uint32x4_t dd = vorrq_u32(vshlq_n_u32(ie, 4), ie);
        uint32x4_t m2 = vbslq_u32(k_f0, dd, c);

        uint32x4_t x = vorrq_u32(vshrq_n_u32(m1, 2), m1);
        uint32x4_t y = vorrq_u32(vshrq_n_u32(m2, 2), m2);
        x = vorrq_u32(vshrq_n_u32(x, 1), x);
        y = vorrq_u32(vshrq_n_u32(y, 1), y);
        x = vandq_u32(x, k_11);
        y = vandq_u32(y, k_11);
        x = vorrq_u32(vshrq_n_u32(x, 7), x);
        y = vorrq_u32(vshrq_n_u32(y, 7), y);
        x = vorrq_u32(vshrq_n_u32(x, 14), x);
        y = vorrq_u32(vshrq_n_u32(y, 14), y);

        uint16x4_t x16 = vmovn_u32(x);
        uint8x8_t  x8  = vmovn_u16(vcombine_u16(x16, x16));
        uint8_t four[8];
        vst1_u8(four, x8);
        memcpy(dest + v * 4, four, 4);

        acc_b = vorrq_u32(acc_b, vandq_u32(y, x));
        acc_a = vorrq_u32(acc_a, x);
    }
    {
        uint32x2_t ra = vorr_u32(vget_low_u32(acc_a), vget_high_u32(acc_a));
        uint32x2_t rb = vorr_u32(vget_low_u32(acc_b), vget_high_u32(acc_b));
        uint32_t ta = vget_lane_u32(ra, 0) | vget_lane_u32(ra, 1);
        uint32_t tb = vget_lane_u32(rb, 0) | vget_lane_u32(rb, 1);
        if (tb != 0) return 2;
        return ta != 0 ? 16 : 0;
    }
#else
    V4 acc_a = {{0, 0, 0, 0}};
    V4 acc_b = {{0, 0, 0, 0}};

    for (int v = 0; v < ROUNDS; v++) {

        V4 even, odd;
        for (int i = 0; i < 4; i++) {
            memcpy(&even.w[i],   local + v * 32 + i * 8,     4);
            memcpy(&odd.w[i], local + v * 32 + i * 8 + 4, 4);
        }

        V4 a = orr(shr(even, 4), even);
        V4 b = orr(shl_(odd, 4), odd);
        V4 m1 = bsl(0xf0f0f0f0u, b, a);

        V4 pe = eor(even, 0x1f1f1f1fu);
        V4 ie = eor(odd, 0x1f1f1f1fu);
        V4 c = orr(shr(pe, 4), pe);
        V4 d = orr(shl_(ie, 4), ie);
        V4 m2 = bsl(0xf0f0f0f0u, d, c);

        V4 x = orr(shr(m1, 2), m1);
        V4 y = orr(shr(m2, 2), m2);
        x = orr(shr(x, 1), x);
        y = orr(shr(y, 1), y);
        x = andd(x, 0x11111111u);
        y = andd(y, 0x11111111u);
        x = orr(shr(x, 7), x);
        y = orr(shr(y, 7), y);
        x = orr(shr(x, 14), x);
        y = orr(shr(y, 14), y);

        uint8_t four[4];
        for (int i = 0; i < 4; i++) four[i] = (uint8_t)(x.w[i] & 0xff);
        memcpy(dest + v * 4, four, 4);

        V4 common;
        for (int i = 0; i < 4; i++) common.w[i] = y.w[i] & x.w[i];

        acc_b = orr(acc_b, common);
        acc_a = orr(acc_a, x);
    }

    uint32_t all_a = acc_a.w[0] | acc_a.w[1] | acc_a.w[2] | acc_a.w[3];
    uint32_t all_b = acc_b.w[0] | acc_b.w[1] | acc_b.w[2] | acc_b.w[3];

    if (all_b != 0) return 2;
    return all_a != 0 ? 16 : 0;
#endif
}
#undef OFF_FILL
#undef ROUNDS
#undef LOCAL

typedef void *(*fn_memcpy)(void *, const void *, size_t);
typedef void *(*fn_memset)(void *, int, size_t);

void *gpu2d_compose_scroll_line_h(void *dst, const void *src, int n) {

    static fn_memcpy s_memcpy;
    static fn_memset s_memset;
    if (!s_memcpy) s_memcpy = (fn_memcpy)sym_libc_memcpy;
    if (!s_memset) s_memset = (fn_memset)sym_libc_memset;

    unsigned char *base = (unsigned char *)dst;
    int32_t        w20  = (int32_t)n;

    unsigned char *gap;
    int64_t        bytes;

    if (!(((uint32_t)w20 >> 31) & 1u)) {

        int64_t remaining = (int64_t)256 - (int64_t)w20;

        uint32_t t32 = (uint32_t)((uint32_t)remaining << 2);
        int64_t  size = (int64_t)(int32_t)t32;

        const unsigned char *source =
            (const unsigned char *)src + ((int64_t)w20 * 4);

        s_memcpy(base, source, (size_t)size);

        uint32_t h32 = (uint32_t)w20 << 2;
        bytes = (int64_t)(int32_t)h32;

        gap = base + (remaining * 4);
    } else {

        unsigned char *dest = base - ((int64_t)w20 * 4);

        uint32_t w20b = (uint32_t)w20 << 2;

        uint32_t t32 = w20b + 0x400u;
        int64_t  size = (int64_t)(int32_t)t32;

        s_memcpy(dest, src, (size_t)size);

        uint32_t h32 = (uint32_t)(0u - w20b);
        bytes = (int64_t)(int32_t)h32;

        gap = base;
    }

    return s_memset(gap, 0, (size_t)bytes);
}

extern void  gpu2d_compose_blend_marked_layers_17(uint8_t *ctx, void *output, uint8_t *layers, uint8_t *lists, void *extra, void *extra2, uint32_t mask) __asm__("gpu2d_compose_blend_marked_layers");
extern void  gpu2d_planes_render_shadow_and_zero(int32_t param_1, uint64_t param_2, uint64_t param_3, uint32_t *param_4);
extern void  gpu2d_planes_render_shadow_mirror_zero(int32_t param_1, uint64_t param_2, uint64_t param_3, uint64_t param_4, const uint32_t *param_5);
extern void  gpu2d_compose_resolve_visibility_layered_pair_17(const uint8_t *obj, const uint8_t *masks, uint8_t *above, uint8_t *below) __asm__("gpu2d_compose_resolve_visibility_layered_pair");
extern void  gpu2d_compose_resolve_visibility_17(const uint8_t *obj, const uint8_t *masks, uint8_t *output) __asm__("gpu2d_compose_resolve_visibility");
extern void  gpu2d_planes_combine_six_masks(uint8_t *output, const uint8_t *masks, int flags, int modes);
extern void  gpu2d_planes_expand_mask_to_params(int arg, uint8_t *table_a, uint8_t *table_b, const uint16_t *bits);
extern void  gpu2d_blend_weights_from_bldalpha_masked(int param_1, uint8_t *param_2, uint8_t *param_3, const uint16_t *param_4);
extern void  gpu2d_blend_apply_alpha(uint8_t *dest, const uint8_t *sources, const uint8_t *weightA, const uint8_t *weightB);
extern void  gpu2d_blend_weights_from_alpha_line(unsigned char *out_plus, unsigned char *out_inv, const unsigned char *data, const unsigned char *mask);
extern void  gpu2d_blend_weights_from_alpha_masked(unsigned char *bufA, unsigned char *bufB, const unsigned char *src, const unsigned char *mask);
#define W_COVER     0xda0
#define W_SOURCES  0x10c0
#define W_AUX      0x11c0
#define W_NO_WINDOW 0x1180
#define W_BLEND   0x12c0
#define W_SPR      0x13c0
#define W_OUTSIDE    0x1440
#define W_V0       0x1480
#define W_TMP      0x1540
#define W_V1       0x1840
#define W_ACC     0x1b40
#define W_FILTER   0x1b60
#define W_OUT   0x1b80
#define M_COVER     0xfa0
#define M_WIN0    0xec0
#define M_WIN1    0xee0
#define C_LAYER     132
#define C_V1       162
#define C_V0       164
#define WIDTH      0x100
#define CEILING      0x7ff
#define MAX_OUT       0x3f






void gpu2d_compose_blend_line_windows(uint8_t *ctx, uint8_t *dst, uint8_t *w, void *p4,
                        void *p5, void *p6, uint32_t layers, uint32_t sel,
                        uint32_t band)
{

    uint8_t *sources = w + W_SOURCES;

    if ((band & 7) == 0) {
        gpu2d_compose_resolve_visibility(ctx, w + W_COVER, sources);
        if (band & 8) {

            uint8_t *no_v = w + W_NO_WINDOW;
            gpu2d_compose_blend_marked_layers(ctx, no_v, sources, p4, p5, 0, layers);
            uint8_t *v0 = w + W_V0;
            gpu2d_planes_combine_six_masks(v0, sources, (int)layers, (int)(sel & 0x3fu));
            for (uint32_t k = 0; k < 4; k++) {
                uint64_t t = rd64(w + 4000 + (size_t)k * 8);
                wr64(w + 5248 + (size_t)k * 8,
                      rd64(w + 5248 + (size_t)k * 8) & ~t);
            }
            gpu2d_blend_apply_brightness((const gpu2d_engine_t *)(ctx), dst, no_v, (const uint16_t *)v0);
            return;
        }
        gpu2d_compose_blend_marked_layers(ctx, dst, sources, p4, p5, 0, layers);
        return;
    }

    uint8_t *spr   = w + W_SPR;
    uint8_t *window0 = w + W_V0;
    uint8_t *tmp   = w + W_TMP;
    uint8_t *acc  = w + W_ACC;
    uint8_t *aux   = w + W_AUX;
    uint8_t *output = w + W_OUT;

    uint32_t drop;
    if (layers == 0) {
        drop = 0;
    } else {
        uint32_t m = ctx[C_LAYER];

        drop = (m & 4) ? layers : (layers & ~(1u << (m & 31)));
    }
    uint32_t sel2 = drop & (sel >> 8);

    gpu2d_compose_resolve_visibility_layered_pair(ctx, w + W_COVER, spr, window0);
    gpu2d_compose_blend_marked_layers(ctx, tmp, spr, p4, p5, (band & 8) ? 0 : p6, layers);
    gpu2d_compose_blend_marked_layers(ctx, w + W_V1, window0, p4, p5, 0, sel2);
    gpu2d_planes_combine_six_masks(acc, spr, (int)layers, (int)(sel & 0x3fu));
    gpu2d_planes_combine_six_masks(w + W_FILTER, window0, (int)layers, (int)((sel >> 8) & 0x3fu));

    uint64_t v[4];
    for (uint32_t k = 0; k < 4; k++) {
        v[k] = rd64(w + 6976 + (size_t)k * 8) & ~rd64(w + 4000 + (size_t)k * 8);
        wr64(w + 6976 + (size_t)k * 8, v[k]);
    }

    uint32_t mode = band & 5u;
    int write_fn = 1;
    if (mode == 1) {
        for (uint32_t k = 0; k < 4; k++)
            v[k] = rd64(w + 3776 + (size_t)k * 8)
                 & rd64(w + 7008 + (size_t)k * 8)
                 & rd64(w + 5184 + (size_t)k * 8);
        band = (band & ~1u) | 4u;
    } else if (mode == 5) {
        for (uint32_t k = 0; k < 4; k++)
            v[k] = (( rd64(w + 5184 + (size_t)k * 8)
                    & rd64(w + 3776 + (size_t)k * 8)) | v[k])
                 & rd64(w + 7008 + (size_t)k * 8);
        band &= ~1u;
    } else if (mode == 4) {
        for (uint32_t k = 0; k < 4; k++)
            v[k] &= rd64(w + 7008 + (size_t)k * 8);
    } else {
        write_fn = 0;
    }

    if (write_fn) {
        for (uint32_t k = 0; k < 4; k++) wr64(output + (size_t)k * 8, v[k]);
        if (band & 0x10u) {
            for (uint32_t k = 0; k < 4; k++) {
                v[k] &= ~rd64(w + 5056 + (size_t)k * 8);
                wr64(output + (size_t)k * 8, v[k]);
            }
            band &= ~0x10u;
        }
        if (band & 0x20u) {
            for (uint32_t k = 0; k < 4; k++) {
                v[k] &= ~rd64(w + 5184 + (size_t)k * 8);
                wr64(output + (size_t)k * 8, v[k]);
            }
            band &= ~0x20u;
        }
    }

    void *dest_v = write_fn ? (void *)output : 0;
    uint32_t done = 0, mix = 0;

    if (band & 8u) {
        uint32_t x = rd16(ctx + C_V1);
        if (sel & 0x40u) {
            gpu2d_planes_render_shadow_and_zero((int32_t)x, (uint64_t)(uintptr_t)sources,
                               (uint64_t)(uintptr_t)aux, (uint32_t *)acc);
            mix = 0;
        } else {
            gpu2d_planes_render_shadow_mirror_zero((int32_t)x, (uint64_t)(uintptr_t)sources,
                               (uint64_t)(uintptr_t)aux,
                               (uint64_t)(uintptr_t)(w + W_BLEND),
                               (const uint32_t *)acc);
            mix = 1;
        }
        done = 1;
    }
    if (band & 4u) {
        uint32_t x = rd16(ctx + C_V0);
        if (done) gpu2d_blend_weights_from_bldalpha_masked((int)x, sources, aux, (const uint16_t *)dest_v);
        else       gpu2d_planes_expand_mask_to_params((int)x, sources, aux, (const uint16_t *)dest_v);
        done = 1;
    }

    if (p6 != 0 && (band & 2u)) {
        uint32_t res[8];
        for (uint32_t i = 0; i < 8; i++)
            res[i] = rd32(w + 3808 + (size_t)i * 4)
                   & rd32(w + 5184 + (size_t)i * 4)
                   & rd32(w + 7008 + (size_t)i * 4);
        for (uint32_t i = 0; i < 8; i++) wr32(output + (size_t)i * 4, res[i]);

        if (p5 != 0 && !(sel & 0x80u)) {
            for (uint32_t i = 0; i < 8; i++) {
                uint32_t t = rd32(w + 7008 + (size_t)i * 4)
                           & rd32(w + 5056 + (size_t)i * 4);
                wr32(output + (size_t)i * 4, res[i] | t);
            }
        }

        if (done) gpu2d_blend_weights_from_alpha_masked(sources, aux, (const unsigned char *)p6, output);
        else       gpu2d_blend_weights_from_alpha_line(sources, aux, (const unsigned char *)p6, output);
    }

    if (!mix) {
        gpu2d_blend_apply_alpha(dst, tmp, sources, aux);
        return;
    }

    for (uint32_t i = 0; i < WIDTH; i++) {
        const uint8_t *p = sources + i;
        uint32_t a  = p[512],  c0 = p[0];
        uint32_t b  = p[1152], c1 = p[256];
        uint32_t e  = p[1920], d  = p[1408];
        uint32_t f  = p[1664], g  = p[2176], h = p[2432];

        uint32_t base = (a << 6) - a + 0x10u;
        uint32_t r0 = e * c1 + (b * c0 + base);
        uint32_t r1 = g * c1 + (d * c0 + base);
        uint32_t r2 = h * c1 + (f * c0 + base);

        uint8_t *q = dst + i;
        q[0]   = (uint8_t)((r0 > CEILING) ? MAX_OUT : (r0 >> 5));
        q[256] = (uint8_t)((r1 > CEILING) ? MAX_OUT : (r1 >> 5));
        q[512] = (uint8_t)((r2 > CEILING) ? MAX_OUT : (r2 >> 5));
    }
}
#undef W_COVER
#undef W_SOURCES
#undef W_AUX
#undef W_NO_WINDOW
#undef W_BLEND
#undef W_SPR
#undef W_OUTSIDE
#undef W_V0
#undef W_TMP
#undef W_V1
#undef W_ACC
#undef W_FILTER
#undef W_OUT
#undef M_COVER
#undef M_WIN0
#undef M_WIN1
#undef C_LAYER
#undef C_V1
#undef C_V0
#undef WIDTH
#undef CEILING
#undef MAX_OUT

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
extern void gpu2d_compose_dispatch_layer_mix_18(uint8_t *layer, uint8_t *output, uint8_t *coverage,
                               void *extra, uint32_t filter, uint32_t prio) __asm__("gpu2d_compose_dispatch_layer_mix");
extern void gpu2d_compose_clear_masked_groups_18(const uint8_t *ctx, uint32_t *dest,
                               const uint32_t *source, uint32_t mask) __asm__("gpu2d_compose_clear_masked_groups");
extern void gpu2d_compose_convert_planes_to_rgb555_blend_18(void *state, void *dest, void *mix, void *planes) __asm__("gpu2d_compose_convert_planes_to_rgb555_blend");
extern int gpu2d_compose_fold_alpha_summary_18(uint8_t *dest, const void *origin) __asm__("gpu2d_compose_fold_alpha_summary");
extern void gpu2d_obj_scan_oam(gpu2d_engine_t *R);
extern void *gpu3d_raster_scanline_lookup_filtered(gpu_t *param_1, int param_2);
extern void gpu2d_compose_mark_empty_groups_18(const unsigned char *data, uint32_t *mask) __asm__("gpu2d_compose_mark_empty_groups");
extern void gpu2d_compose_pack_planes_to_rgb15_18(uint8_t *ctx, uint8_t *dst, const uint8_t *planes) __asm__("gpu2d_compose_pack_planes_to_rgb15");
extern void gpu2d_compose_pack_rgba_to_rgb15_18(uint8_t *ctx, uint8_t *dst, const uint8_t *src) __asm__("gpu2d_compose_pack_rgba_to_rgb15");


static void pack(uint8_t *d, const uint8_t *s)
{
#ifdef __ARM_NEON

    const uint32x4_t m0 = vreinterpretq_u32_u16(vdupq_n_u16(0x0001));
    const uint32x4_t m1 = vreinterpretq_u32_u16(vdupq_n_u16(0x0004));
    const uint32x4_t m2 = vreinterpretq_u32_u16(vdupq_n_u16(0x0010));
    const uint32x4_t m3 = vreinterpretq_u32_u16(vdupq_n_u16(0x0040));
    for (uint32_t i = 0; i < 8; i++) {
        uint32x4x4_t q = vld4q_u32((const uint32_t *)(s + (size_t)i * 64));
        uint32x4_t v = vorrq_u32(vorrq_u32(vandq_u32(vshrq_n_u32(q.val[0], 15), m0),
                                           vandq_u32(vshrq_n_u32(q.val[1], 13), m1)),
                                 vorrq_u32(vandq_u32(vshrq_n_u32(q.val[2], 11), m2),
                                           vandq_u32(vshrq_n_u32(q.val[3], 9), m3)));
        v = vorrq_u32(vshrq_n_u32(v, 15), v);
        uint8x8_t b = vmovn_u16(vcombine_u16(vmovn_u32(v), vdup_n_u16(0)));
        vst1_lane_u32((uint32_t *)(d + (size_t)i * 4), vreinterpret_u32_u8(b), 0);
    }
#else
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t a = rd32(s), b = rd32(s + 4);
        uint32_t cc = rd32(s + 8), e = rd32(s + 12);
        uint32_t v = ((a >> 15) & 0x10001u) | ((b >> 13) & 0x40004u)
                   | ((cc >> 11) & 0x100010u) | ((e >> 9) & 0x400040u);
        d[i] = (uint8_t)(v | (v >> 15));
        s += 16;
    }
#endif
}
#define SCALE_MAX 8u

static uint8_t *permute_to_modn(uint8_t *three)
{
    const unsigned N = recon_scale_output();
    if (!three || !recon_native || N <= 2u) return three;
    static __thread uint8_t row_modn[SCALE_MAX * SCALE_MAX * 0x400];
    uint8_t *row_modn_p = row_modn;
    __asm__ __volatile__("" : "+r"(row_modn_p));
    const unsigned half = 128u * N;
    for (unsigned r = 0; r < N; r++) {
        const uint32_t *s = (const uint32_t *)(three + (size_t)r * N * 0x400);
        uint32_t *dd = (uint32_t *)(row_modn_p + (size_t)r * N * 0x400);
#ifdef __ARM_NEON

        if (N == 4u) {
            for (unsigned pp = 0; pp < 2u; pp++) {
                const uint32_t *sp = s + (size_t)pp * half;
                uint32_t *d0 = dd + ((size_t)(0u + pp) << 8);
                uint32_t *d1 = dd + ((size_t)(2u + pp) << 8);
                for (unsigned i = 0, o = 0; i < half; i += 8u, o += 4u) {
                    uint32x4x2_t v = vld2q_u32(sp + i);
                    vst1q_u32(d0 + o, v.val[0]);
                    vst1q_u32(d1 + o, v.val[1]);
                }
            }
            continue;
        }
        if (N == 8u) {
            for (unsigned pp = 0; pp < 2u; pp++) {
                const uint32_t *sp = s + (size_t)pp * half;
                uint32_t *d0 = dd + ((size_t)(0u + pp) << 8);
                uint32_t *d1 = dd + ((size_t)(2u + pp) << 8);
                uint32_t *d2 = dd + ((size_t)(4u + pp) << 8);
                uint32_t *d3 = dd + ((size_t)(6u + pp) << 8);
                for (unsigned i = 0, o = 0; i < half; i += 16u, o += 4u) {
                    uint32x4x4_t v = vld4q_u32(sp + i);
                    vst1q_u32(d0 + o, v.val[0]);
                    vst1q_u32(d1 + o, v.val[1]);
                    vst1q_u32(d2 + o, v.val[2]);
                    vst1q_u32(d3 + o, v.val[3]);
                }
            }
            continue;
        }
#endif
        for (unsigned pp = 0; pp < 2u; pp++) {
            const uint32_t *sp = s + (size_t)pp * half;

            unsigned q = 0u, r = pp;
            for (unsigned i = 0; i < half; i++) {
                dd[(r << 8) + q] = sp[i];
                r += 2u;
                if (r >= N) { r -= N; q++; }
            }

        }
    }
    return row_modn_p;
}

static uint32_t pack_all_three(uint8_t *c, uint8_t *buf, uint8_t *lv21,
                               uint8_t *s576, uint8_t *s752,
                               uint32_t line, uint32_t idx, void **table,
                               uint32_t nsub_tot, uint32_t *use_high)
{

    const unsigned shadow_stride_line = recon_shadow_stride();
    const unsigned s0_s    = recon_s0_shadow();
    uint32_t high = (idx >= s0_s && idx < nsub_tot);
    size_t lin = ((size_t)line << 8) * 2;
    size_t alt_off = ((size_t)(line * shadow_stride_line) << 8) * 2
                   + (size_t)(idx - s0_s) * 0x200u;
    uint32_t bits = 0;
    if (lv21) {
        const uint8_t *src = lv21 + lin;
        uint8_t *alt = ((gpu2d_engine_t *)c)->sprite_screen_alt;
        if (high && alt) { src = alt + alt_off; if (use_high) *use_high = 1u; }
        pack(buf + 0xda0 + (size_t)(((gpu2d_engine_t *)c)->sprite_screen_priority + 4) * 32, src);
        bits |= 0x20;
        table[4] = (void *)(src - 0x10);
    }
    if (s576) {
        const uint8_t *src = s576 + lin;
        uint8_t *alt = (uint8_t *)((gpu2d_engine_t *)c)->bg[2].direct_ptr_alt;
        if (high && alt) { src = alt + alt_off; if (use_high) *use_high = 1u; }
        pack(buf + 0xde0, src);
        table[2] = (void *)(src - 0x10);
    }
    if (s752) {
        const uint8_t *src = s752 + lin;
        uint8_t *alt = (uint8_t *)((gpu2d_engine_t *)c)->bg[3].direct_ptr_alt;
        if (high && alt) { src = alt + alt_off; if (use_high) *use_high = 1u; }
        pack(buf + 0xe00, src);
        table[3] = (void *)(src - 0x10);
    }
    return bits;
}

static void plane(gpu_output_t *cap, void *a, void *b, void *three, void *plane2d)
{
    if (cap->capture.source_a_mode == 2) {
        if (!three) return;

        if (cap->capture.blend) gpu2d_compose_convert_texels_to_rgb555_modulated(cap, a, b, three);
        else         gpu2d_compose_pack_rgba_to_rgb15(cap, a, three);
    } else {
        if (cap->capture.blend) gpu2d_compose_convert_planes_to_rgb555_blend(cap, a, b, plane2d);
        else         gpu2d_compose_pack_planes_to_rgb15(cap, a, plane2d);
    }
}

void gpu2d_compose_render_line_prologue(void *ctx, uint32_t line, gpu_output_t *cap, uint32_t doubled,
                         uint8_t *buf, recon_b2_prep *p)
{
    gpu2d_engine_t *e = (gpu2d_engine_t *)ctx;

    uint32_t disp  = e->dispcnt;
    uint32_t flags = (disp >> 8) & 0xf;
    if (e->oam_dirty) { gpu2d_obj_scan_oam(ctx); e->oam_dirty = 0; }
    uint32_t modes  = e->line_mode[line];
    uint32_t ctl    = e->bldcnt;
    uint32_t brightness = e->bg[0].hofs;
    uint8_t *three = 0;
    uint32_t extra = 0;
    if ((disp & 8) && ((disp & 0x100) || (cap && cap->capture.source_a_mode == 2))) {

        three  = (uint8_t *)gpu3d_raster_scanline_lookup_filtered(e->gpu, (int)line);
        extra = (doubled != 0);

    }
    gpu2d_bg_text_apply_mosaic_layers(ctx, buf + 0x1e0, buf + 0xda0, line);
    if (disp & 0x1000)
        flags |= gpu2d_obj_draw_line(ctx, buf + 0xa70, buf + 0xc90, buf + 0xe20,
                                    buf + 0xec0, buf + 0xee0, line);
    uint8_t *lv21 = e->sprite_screen;
    if (lv21)             { flags |= 0x10; extra |= 0x10; }
    if (e->bg[2].direct_ptr)   extra |= 4;
    if (e->bg[3].direct_ptr)   extra |= 8;
    gpu2d_compose_dispatch_layer_mix(ctx, buf + 0xf00, buf + 0xfa0, buf + 0xea0, flags, line);

    uint32_t m = (flags | (flags << 8) | 0xf0f0f0f0u) & ctl;
    uint32_t bm = (ctl >> 6) & 3;
    if ((m & 0x3f) && bm == 1 && (m & 0x3f00)) modes |= 4;
    if (bm > 1 && (m & 0x3f) && (int16_t)e->bldy != 0) modes |= 8;
    uint32_t modes_f = (m & 0x3f00) ? modes : (modes & 0xfe);

    extra &= flags;

    p->line = line; p->doubled = doubled;
    p->flags = flags; p->m = m; p->modes_f = modes_f; p->extra = extra;
    p->bright23 = brightness << 23;
    p->sext9  = (uint32_t)((int32_t)(brightness << 23) >> 23);
    p->lv21 = lv21;
    p->s576 = (uint8_t *)e->bg[2].direct_ptr;
    p->s752 = (uint8_t *)e->bg[3].direct_ptr;
    p->three = three;
    p->buf = buf;
}

uint32_t gpu2d_compose_render_line_body(void *ctx, uint8_t *dst, gpu_output_t *cap, recon_b2_prep *p,
                            uint32_t *high)
{
    uint8_t *c = (uint8_t *)ctx;
    uint8_t *buf = p->buf;
    void *table[8];
    const uint32_t line = p->line, doubled = p->doubled;
    uint32_t flags = p->flags, extra = p->extra, modes_f = p->modes_f;
    const uint32_t m = p->m, bright23 = p->bright23, sext9 = p->sext9;
    uint8_t *lv21 = p->lv21, *s576 = p->s576, *s752 = p->s752;
    uint8_t *three = permute_to_modn(p->three);

    table[0] = buf + 0x1e0;
    table[1] = buf + 0x400; table[2] = buf + 0x620;
    table[3] = buf + 0x840; table[4] = buf + 0xa60;

    uint8_t *cap3d;

    *high = 0u;
    if (doubled && extra) {

        gpu2d_compose_clear_masked_groups(ctx, (uint32_t *)(buf + 0xda0), (const uint32_t *)(buf + 0xf00), extra ^ flags);
        flags &= ~extra;
        gpu2d_compose_mark_empty_groups(buf + 0xda0, &flags);
        flags |= extra;

        recon_native_read();
        uint32_t nsub = 4u;
        {   unsigned e = recon_scale_output();
            if (recon_native && e > 2u) nsub = e * e; }
        uint32_t use_high = 0u;
        uint32_t differ = 0u;

        const int has_alt = (lv21 && ((gpu2d_engine_t *)c)->sprite_screen_alt)
                         || (s576 && ((gpu2d_engine_t *)c)->bg[2].direct_ptr_alt)
                         || (s752 && ((gpu2d_engine_t *)c)->bg[3].direct_ptr_alt);


        for (uint32_t idx = 0; idx < nsub; idx++) {

            if (recon_shortcut_all() && idx > 0 && idx + 1u < nsub) {
                memcpy(dst + (size_t)idx * 0x300, dst, 0x300);
                continue;
            }
            if (idx > 0 && idx + 1u < nsub && three && !has_alt
                && memcmp(three + (size_t)idx * 0x400, three, 0x400) == 0) {
                memcpy(dst + (size_t)idx * 0x300, dst, 0x300);
                continue;
            }
            uint32_t w16 = modes_f;
            void *p3 = 0;
            if (three) {
                uint8_t *p = three + (size_t)idx * 0x400;
                if (bright23) { gpu2d_compose_scroll_line_h(buf, p, sext9); p = buf; }
                w16 = (uint32_t)gpu2d_compose_fold_alpha_summary(buf + 0xda0, p) | modes_f;
                p3 = p;
            }
            w16 |= pack_all_three(c, buf, lv21, s576, s752, line, idx, table,
                                     nsub, &use_high);

            uint8_t *p6;
            if (!(w16 & 2))    p6 = 0;
            else if (idx == nsub - 1u) p6 = buf + 0xc90;
            else { memcpy(buf + 0xfc0, buf + 0xc90, 256); p6 = buf + 0xfc0; }

            gpu2d_compose_clear_masked_groups(ctx, (uint32_t *)(buf + 0xda0), (const uint32_t *)(buf + 0xf00), flags & extra);

            gpu2d_compose_blend_line_windows(ctx, dst + (size_t)idx * 0x300, buf, table,
                                 p3, p6, flags, m, w16);

            if (!differ && idx > 0
                && memcmp(dst + (size_t)idx * 0x300, dst, 0x300) != 0)
                differ = 1u;
        }
        *high = differ || use_high;
        cap3d = three;
    } else {

        uint8_t *pp = three;
        if (three) {
            if (bright23) { gpu2d_compose_scroll_line_h(buf, three, sext9); pp = buf; }
            modes_f |= (uint32_t)gpu2d_compose_fold_alpha_summary(buf + 0xda0, pp);
        }
        modes_f |= pack_all_three(c, buf, lv21, s576, s752, line, 0, table,
                                      1u, 0);

        uint8_t *p6 = (modes_f & 2) ? buf + 0xc90 : 0;

        gpu2d_compose_clear_masked_groups(ctx, (uint32_t *)(buf + 0xda0), (const uint32_t *)(buf + 0xf00), flags);
        gpu2d_compose_mark_empty_groups(buf + 0xda0, &flags);

        gpu2d_compose_blend_line_windows(ctx, dst, buf, table, pp, p6, flags, m, modes_f);
        cap3d = pp;
    }

    if (!cap || cap->capture.source_a_mode == 0 || cap->capture.height <= line) return extra ? doubled : 0;

    unsigned Ncap = 2u;
    {   unsigned e = recon_scale_output();
        if (recon_native && e > 2u) Ncap = e; }
    const unsigned ns = Ncap * Ncap;

    uint32_t off = (cap->capture.width * line + cap->capture.read_offset) & 0xffff;
    uint8_t *vdst = cap->capture.source + (size_t)off * 2;
    uint8_t *vbuf = cap->capture.source_b_line;

    if (doubled) {

        const unsigned s0 = recon_s0_shadow();
        const unsigned shadow_stride = recon_shadow_stride();
        uint8_t *comp = cap->capture.shadow + (size_t)(off * shadow_stride) * 2;
        uint32_t b = cap->capture.display_mode;
        uint8_t *src2 = vbuf;
        uint32_t step = 0;
        if ((1u << (line >> 5)) & cap->bank_texture_bits[b]) {
            src2 = cap->capture_shadow[b]
                 + (size_t)((line * shadow_stride) << 8) * 2;
            step = 0x200;
        }
        for (unsigned s = s0; s < ns; s++)
            plane(cap,
                  comp + (size_t)(s - s0) * 0x200,
                  src2 + (size_t)(s - s0) * step,
                  cap3d ? cap3d + (size_t)s * 0x400 : 0,
                  cap3d ? dst + (size_t)s * 0x300 : dst);

    }

    plane(cap, vdst, vbuf, cap3d, dst);
    return extra ? doubled : 0;
}

uint32_t gpu2d_compose_render_line(void *ctx, uint8_t *dst, uint32_t line,
                            gpu_output_t *cap, uint32_t doubled)
{
    uint8_t frame[0x1c00 + 16];
    uint8_t *buf = (uint8_t *)(((uintptr_t)frame + 15) & ~(uintptr_t)15);
    recon_b2_prep p;
    uint32_t high;
    gpu2d_compose_render_line_prologue(ctx, line, cap, doubled, buf, &p);
    uint32_t r = gpu2d_compose_render_line_body(ctx, dst, cap, &p, &high);
    recon_sublines_hires = high;
    return r;
}
#undef SCALE_MAX

#define BIT_3D    3
#define BIT_BG0   8
#define BIT_OBJ   12
#define PRIORITIES      4



void gpu2d_compose_build_layer_order(gpu2d_engine_t *r) {

    uint32_t disp = r->dispcnt;

    uint8_t cnt[PRIORITIES];
    uint8_t group[PRIORITIES][PRIORITIES];
    memset(cnt, 0, sizeof cnt);

    for (uint32_t i = 0; i < PRIORITIES; i++) {
        if (!(disp & (1u << (BIT_BG0 + i)))) continue;
        uint32_t p = r->bg[i].bgcnt & 3u;
        group[p][cnt[p]] = (uint8_t)i;
        cnt[p] = (uint8_t)(cnt[p] + 1);
    }

    uint8_t *A = r->draw_order;
    uint8_t *B = r->bg_order;
    uint32_t a = 0, b = 0;
    uint32_t obj  = (disp >> BIT_OBJ) & 1u;
    uint32_t three = (disp >> BIT_3D)  & 1u;

    for (uint32_t p = 0; p < PRIORITIES; p++) {
        if (obj) A[a++] = (uint8_t)(4u + p);
        for (uint32_t i = 0; i < cnt[p]; i++) {
            uint8_t v = group[p][i];
            A[a++] = v;
            if (!three || v != 0) B[b++] = v;
        }
    }

    r->draw_count = (uint8_t)a;
    r->bg_count = (uint8_t)b;
}
#undef BIT_3D
#undef BIT_BG0
#undef BIT_OBJ
#undef PRIORITIES

void gpu2d_compose_pack_flag_mask_odd_bytes(unsigned char *dst, const unsigned char *src,
                        int32_t n) {

    do {
        unsigned char t[32];
        for (int b = 0; b < 2; b++)
            for (int k = 0; k < 16; k++) {
                unsigned char v = src[b * 32 + k * 2 + 1];
                t[b * 16 + k] = (unsigned char)(((v & 4) ? 7 : 0)
                                              | ((v & 8) ? 0x38 : 0));
            }
        src += 64;
        for (int k = 0; k < 32; k++) dst[k] = t[k];
        dst += 32;
        n -= 0x20;
    } while (n >= 0);
}

static void table_lookup_64(uint8_t out[16], const uint8_t table[64],
                              const uint8_t idx[16])
{
    int i;
    for (i = 0; i < 16; i++) {
        uint8_t j = idx[i];
        out[i] = (j < 64u) ? table[j] : 0u;
    }
}

static void xor_bytes_16(uint8_t buf[16], uint8_t key)
{
    int i;
    for (i = 0; i < 16; i++) buf[i] ^= key;
}

void gpu2d_compose_decode_bg_row_8bpp_indexed(uint8_t *x0, uint8_t *x1, uint8_t *x2, uint8_t *x3,
                        uint8_t *x4, int32_t x5, uint8_t *x6)
{

    const uint32_t w13 = 0xffc0u;

    uint8_t w8b;
    memcpy(&w8b, x2, 1);

    if (w8b == 0u) {

        uint16_t w11;
        uint32_t idx;
        uint8_t key;
        uint8_t table[64];
        int64_t cnt;

        memcpy(&w11, x3, 2);
        idx = w13 & ((uint32_t)w11 << 6);
        memcpy(&key, x4, 1);
        memcpy(table, x6 + idx, 64);

        cnt = 256;
        while (cnt != 0) {
            uint8_t iLo[16], iHi[16], cLo[16], cHi[16];

            memcpy(iLo, x1, 16);
            memcpy(iHi, x1 + 16, 16);
            x1 += 32;

            xor_bytes_16(iLo, key);
            xor_bytes_16(iHi, key);
            table_lookup_64(cLo, table, iLo);
            table_lookup_64(cHi, table, iHi);

            cnt -= 32;

            memcpy(x0, cLo, 16);
            memcpy(x0 + 16, cHi, 16);
            x0 += 32;
        }
        return;
    }

    x5 = x5 - 1;
    if (x5 == 0) goto L_tail;

L_repeat:
    {

        uint32_t w10;
        uint16_t w7;
        uint32_t idxA, idxB;
        uint64_t x8, x9;
        uint8_t keyA, keyB;
        uint8_t lookupA[64], lookupB[64];
        int irregular;

        memcpy(&w10, x3, 4); x3 += 4;
        memcpy(&w7,  x2, 2); x2 += 2;

        idxA = w13 & (w10 << 6);
        idxB = w13 & (w10 >> 10);

        x8 = (uint64_t)(w7 & 0xffu);
        x9 = (uint64_t)(w7 >> 8);

        memcpy(&keyA, x4, 1);
        memcpy(&keyB, x4 + 1, 1);
        x4 += 2;

        memcpy(lookupA, x6 + idxA, 64);
        memcpy(lookupB, x6 + idxB, 64);

        irregular = ((x8 & 0xf0u) != 0u) || ((x9 & 0xf0u) != 0u);

        if (irregular) {

            int64_t remA = (int64_t)x8;
            int64_t remB = (int64_t)x9;

            do {
                uint8_t idx[16], col[16];
                memcpy(idx, x1, 16); x1 += 16;
                xor_bytes_16(idx, keyA);
                table_lookup_64(col, lookupA, idx);
                remA -= 16;
                memcpy(x0, col, 16); x0 += 16;
            } while (remA > 0);
            {

                int32_t w8rem = (int32_t)remA;
                x1 += w8rem;
                x0 += w8rem;
            }

            do {
                uint8_t idx[16], col[16];
                memcpy(idx, x1, 16); x1 += 16;
                xor_bytes_16(idx, keyB);
                table_lookup_64(col, lookupB, idx);
                remB -= 16;
                memcpy(x0, col, 16); x0 += 16;
            } while (remB > 0);

            x5 -= 2;
            {

                int32_t w9rem = (int32_t)remB;
                x1 += w9rem;
                x0 += w9rem;
            }

            if (x5 > 0) goto L_repeat;
            if (x5 != 0) return;
            goto L_tail;
        } else {

            uint8_t iA[16], iB[16], cA[16], cB[16];

            memcpy(iA, x1, 16); x1 += x8;
            memcpy(iB, x1, 16); x1 += x9;

            xor_bytes_16(iA, keyA);
            xor_bytes_16(iB, keyB);
            table_lookup_64(cA, lookupA, iA);
            table_lookup_64(cB, lookupB, iB);

            x5 -= 2;

            memcpy(x0, cA, 16); x0 += x8;
            memcpy(x0, cB, 16); x0 += x9;

            if (x5 > 0) goto L_repeat;
            if (x5 == 0) goto L_tail;
            return;
        }
    }

L_tail:
    {

        uint16_t w11;
        uint8_t w8c;
        uint32_t idx;
        uint8_t key;
        uint8_t table[64];
        int32_t cnt;

        memcpy(&w11, x3, 2);
        memcpy(&w8c, x2, 1);

        idx = w13 & ((uint32_t)w11 << 6);
        memcpy(&key, x4, 1);
        memcpy(table, x6 + idx, 64);

        cnt = (int32_t)(uint32_t)w8c;
        do {
            uint8_t bidx[16], col[16];
            memcpy(bidx, x1, 16); x1 += 16;
            xor_bytes_16(bidx, key);
            table_lookup_64(col, table, bidx);
            cnt -= 16;

            memcpy(x0, col, 16); x0 += 16;
        } while (cnt > 0);
        return;
    }
}

static void recon_tbl64(uint8_t out[16], const uint8_t table[64], const uint8_t idx[16])
{
    int i;
    for (i = 0; i < 16; i++) {
        uint8_t j = idx[i];
        out[i] = (j < 64u) ? table[j] : 0u;
    }
}

static void recon_xor16(uint8_t buf[16], uint8_t key)
{
    int i;
    for (i = 0; i < 16; i++) buf[i] ^= key;
}

static void recon_store_pairs(uint8_t *dst, const uint8_t color[16], uint8_t pal)
{
    int i;
    for (i = 0; i < 16; i++) {
        dst[2 * i]     = color[i];
        dst[2 * i + 1] = pal;
    }
}

void gpu2d_compose_decode_bg_row_8bpp_paletted(uint8_t *x0, uint8_t *x1, uint8_t *x2, uint8_t *x3,
                         uint8_t *x4, int32_t x5, uint8_t *x6)
{

    const uint32_t w15 = 0xffc0u;
    uint8_t w8b;
    memcpy(&w8b, x2, 1);

    if (w8b == 0u) {

        uint16_t w11;
        memcpy(&w11, x3, 2);
        uint32_t pal = (uint32_t)(w11 >> 12) & 0xfu;
        uint32_t idx = w15 & ((uint32_t)w11 << 6);
        uint8_t key;
        memcpy(&key, x4, 1);

        uint8_t table[64];
        memcpy(table, x6 + idx, 64);

        int64_t cnt = 256;
        while (cnt != 0) {
            uint8_t idxLo[16], idxHi[16], colLo[16], colHi[16];
            memcpy(idxLo, x1, 16);
            memcpy(idxHi, x1 + 16, 16);
            x1 += 32;

            recon_xor16(idxLo, key);
            recon_xor16(idxHi, key);
            recon_tbl64(colLo, table, idxLo);
            recon_tbl64(colHi, table, idxHi);

            cnt -= 32;

            recon_store_pairs(x0, colLo, (uint8_t)pal);
            x0 += 32;
            recon_store_pairs(x0, colHi, (uint8_t)pal);
            x0 += 32;
        }
        return;
    }

    x5 = x5 - 1;
    if (x5 == 0) goto L_tail;

L_loop:
    {
        uint32_t w10;
        uint16_t w7;
        memcpy(&w10, x3, 4); x3 += 4;
        memcpy(&w7, x2, 2);  x2 += 2;

        uint32_t idxA = w15 & (w10 << 6);
        uint32_t idxB = w15 & (w10 >> 10);
        uint64_t cntA = (uint64_t)(w7 & 0xffu);
        uint64_t cntB = (uint64_t)(w7 >> 8);

        uint32_t palA = (w10 >> 12) & 0xfu;
        uint32_t palB = (w10 >> 28) & 0xfu;

        uint8_t keyA, keyB;
        memcpy(&keyA, x4, 1);
        memcpy(&keyB, x4 + 1, 1);
        x4 += 2;

        uint8_t tableA[64], tableB[64];
        memcpy(tableA, x6 + idxA, 64);
        memcpy(tableB, x6 + idxB, 64);

        int irregular = ((cntA & 0xf0u) != 0u) || ((cntB & 0xf0u) != 0u);

        if (irregular) {

            int64_t remA = (int64_t)cntA, remB = (int64_t)cntB;
            do {
                uint8_t idx[16], col[16];
                memcpy(idx, x1, 16); x1 += 16;
                recon_xor16(idx, keyA);
                recon_tbl64(col, tableA, idx);
                remA -= 16;
                recon_store_pairs(x0, col, (uint8_t)palA);
                x0 += 32;
            } while (remA > 0);
            {
                int32_t w8rem = (int32_t)remA;
                x1 += w8rem;
                x0 += (int64_t)w8rem * 2;
            }

            do {
                uint8_t idx[16], col[16];
                memcpy(idx, x1, 16); x1 += 16;
                recon_xor16(idx, keyB);
                recon_tbl64(col, tableB, idx);
                remB -= 16;
                recon_store_pairs(x0, col, (uint8_t)palB);
                x0 += 32;
            } while (remB > 0);
            {
                int32_t w9rem = (int32_t)remB;
                x1 += w9rem;
                x0 += (int64_t)w9rem * 2;
            }

            x5 -= 2;
            if (x5 > 0) goto L_loop;
            if (x5 != 0) return;
            goto L_tail;
        } else {

            uint8_t idxA16[16], idxB16[16], colA[16], colB[16];

            memcpy(idxA16, x1, 16); x1 += cntA;
            memcpy(idxB16, x1, 16); x1 += cntB;
            cntA += cntA;
            cntB += cntB;

            recon_xor16(idxA16, keyA);
            recon_xor16(idxB16, keyB);
            recon_tbl64(colA, tableA, idxA16);
            recon_tbl64(colB, tableB, idxB16);

            x5 -= 2;

            recon_store_pairs(x0, colA, (uint8_t)palA);
            x0 += cntA;
            recon_store_pairs(x0, colB, (uint8_t)palB);
            x0 += cntB;

            if (x5 > 0) goto L_loop;
            if (x5 == 0) goto L_tail;
            return;
        }
    }

L_tail:
    {

        uint16_t w11;
        uint8_t w8b2;
        memcpy(&w11, x3, 2);
        memcpy(&w8b2, x2, 1);

        uint32_t pal = (uint32_t)(w11 >> 12) & 0xfu;
        uint32_t idx = w15 & ((uint32_t)w11 << 6);
        uint8_t key;
        memcpy(&key, x4, 1);

        uint8_t table[64];
        memcpy(table, x6 + idx, 64);

        int32_t cnt = (int32_t)(uint32_t)w8b2;
        do {
            uint8_t bidx[16], col[16];
            memcpy(bidx, x1, 16); x1 += 16;
            recon_xor16(bidx, key);
            recon_tbl64(col, table, bidx);
            cnt -= 16;
            recon_store_pairs(x0, col, (uint8_t)pal);
            x0 += 32;
        } while (cnt > 0);
        return;
    }
}

#define PAL 4
#define OFF_COUNT 179
#define OFF_LIST  0x84
#define OFF_ACC   0x80
#define OFF_BACKDROP  0xa0

typedef struct { uint64_t p[PAL]; } m256;

static m256 load(const uint8_t *d) {
    m256 v;
    for (int i = 0; i < PAL; i++) __builtin_memcpy(&v.p[i], d + i * 8, 8);
    return v;
}

static void store(uint8_t *d, m256 v) {
    for (int i = 0; i < PAL; i++) __builtin_memcpy(d + i * 8, &v.p[i], 8);
}

void gpu2d_compose_resolve_visibility_layered_pair(const uint8_t *obj, const uint8_t *masks,
                        uint8_t *above, uint8_t *below) {

    unsigned count = obj[OFF_COUNT];
    const uint8_t *list = obj + OFF_LIST;

    m256 is_covered   = {{0, 0, 0, 0}};
    m256 covered     = {{0, 0, 0, 0}};
    m256 special = {{0, 0, 0, 0}};
    m256 acc_above   = {{0, 0, 0, 0}};
    m256 acc_below   = {{0, 0, 0, 0}};

    for (unsigned n = 0; n < count; n++) {
        uint32_t shift = (uint32_t)list[n] << 5;
        m256 own = load(masks + shift);
        m256 own_above, own_below;

        if (shift & 0x80) {
            for (int i = 0; i < PAL; i++) own.p[i] &= ~special.p[i];
        }

        for (int i = 0; i < PAL; i++) {
            own_above.p[i] = own.p[i] & ~is_covered.p[i];
            own_below.p[i] = (own.p[i] & ~covered.p[i]) & is_covered.p[i];
        }

        if (shift & 0x80) {
            for (int i = 0; i < PAL; i++) {
                acc_above.p[i] |= own_above.p[i];
                acc_below.p[i] |= own_below.p[i];
            }
        } else {
            store(above + shift, own_above);
            store(below + shift, own_below);
        }

        for (int i = 0; i < PAL; i++) {
            covered.p[i]   |= is_covered.p[i] & own.p[i];
            is_covered.p[i] |= own.p[i];
            if (shift & 0x80) special.p[i] |= own.p[i];
        }
    }

    m256 bg_above, bg_below;
    for (int i = 0; i < PAL; i++) {
        bg_above.p[i] = ~is_covered.p[i];
        bg_below.p[i] = is_covered.p[i] & ~covered.p[i];
    }
    store(above + OFF_BACKDROP, bg_above);
    store(below + OFF_BACKDROP, bg_below);
    store(above + OFF_ACC,  acc_above);
    store(below + OFF_ACC,  acc_below);
}
#undef PAL
#undef OFF_COUNT
#undef OFF_LIST
#undef OFF_ACC
#undef OFF_BACKDROP

#define PAL 4
#define OFF_COUNT 179
#define OFF_LIST  0x84
#define OFF_SPECIALS 0x80
#define OFF_BACKDROP      0xa0

typedef struct { uint64_t p[PAL]; } m256_24;

static m256_24 load_24(const uint8_t *d) {
    m256_24 v;
    for (int i = 0; i < PAL; i++) {
        uint64_t w;
        __builtin_memcpy(&w, d + i * 8, 8);
        v.p[i] = w;
    }
    return v;
}

static void store_24(uint8_t *d, m256_24 v) {
    for (int i = 0; i < PAL; i++) __builtin_memcpy(d + i * 8, &v.p[i], 8);
}

void gpu2d_compose_resolve_visibility(const uint8_t *obj, const uint8_t *masks,
                        uint8_t *output) {

    unsigned count = obj[OFF_COUNT];
    const uint8_t *list = obj + OFF_LIST;

    m256_24 is_covered   = {{0, 0, 0, 0}};
    m256_24 special = {{0, 0, 0, 0}};
    m256_24 accumulated  = {{0, 0, 0, 0}};

    for (unsigned n = 0; n < count; n++) {
        uint32_t shift = (uint32_t)list[n] << 5;
        m256_24 own = load_24(masks + shift);

        if (shift & 0x80) {
            m256_24 vis;
            for (int i = 0; i < PAL; i++) {
                own.p[i] &= ~special.p[i];
                vis.p[i]   = own.p[i] & ~is_covered.p[i];
                accumulated.p[i]  |= vis.p[i];
                is_covered.p[i]   |= own.p[i];
                special.p[i] |= own.p[i];
            }

        } else {
            m256_24 vis;
            for (int i = 0; i < PAL; i++) {
                vis.p[i] = own.p[i] & ~is_covered.p[i];
                is_covered.p[i] |= own.p[i];
            }
            store_24(output + shift, vis);
        }
    }

    m256_24 bg;
    for (int i = 0; i < PAL; i++) bg.p[i] = ~is_covered.p[i];
    store_24(output + OFF_BACKDROP, bg);
    store_24(output + OFF_SPECIALS, accumulated);
}
#undef PAL
#undef OFF_COUNT
#undef OFF_LIST
#undef OFF_SPECIALS
#undef OFF_BACKDROP

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#define WIDTH 256

void gpu2d_line_apply_layer_masked(uint16_t *dest, const uint16_t *a, const uint16_t *b,
                        const uint32_t *mask) {
#ifdef __ARM_NEON

    {
        static const uint16_t BITS[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
        const uint16x8_t v0 = vld1q_u16(BITS);
        const uint16x8_t v1 = vshlq_n_u16(v0, 8);
        const int in_site = (a == dest);
        for (int i = 0; i < WIDTH; i += 32) {
            uint32_t word = mask[i >> 5];
            if (word == 0) {
                if (!in_site) vst1q_u16_x4(dest + i, vld1q_u16_x4(a + i));
                continue;
            }
            uint16x8_t lo = vdupq_n_u16((uint16_t)word);
            uint16x8_t hi = vdupq_n_u16((uint16_t)(word >> 16));
            uint16x8x4_t d = vld1q_u16_x4(a + i);
            uint16x8x4_t sb = vld1q_u16_x4(b + i);
            d.val[0] = vbslq_u16(vtstq_u16(lo, v0), sb.val[0], d.val[0]);
            d.val[1] = vbslq_u16(vtstq_u16(lo, v1), sb.val[1], d.val[1]);
            d.val[2] = vbslq_u16(vtstq_u16(hi, v0), sb.val[2], d.val[2]);
            d.val[3] = vbslq_u16(vtstq_u16(hi, v1), sb.val[3], d.val[3]);
            vst1q_u16_x4(dest + i, d);
        }
        return;
    }
#else

    for (int i = 0; i < WIDTH; i++) {
        uint32_t word = mask[i >> 5];
        if (word & (1u << (i & 31))) dest[i] = b[i];
        else                            dest[i] = a[i];
    }
#endif
}
#undef WIDTH

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#define WIDTH 256
#define PLANE 0x100

void gpu2d_planes_split_interleaved_masked(uint8_t *dest, uint8_t *quarter,
                        const uint8_t *source, const uint16_t *mask) {

    uint8_t *p0 = dest;
    uint8_t *p1 = dest + PLANE;
    uint8_t *p2 = dest + 2 * PLANE;

#ifdef __ARM_NEON

    {
        static const uint8_t B8[16] = { 1, 2, 4, 8, 16, 32, 64, 128,
                                        1, 2, 4, 8, 16, 32, 64, 128 };
        static const uint8_t SEL[16] = { 0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1 };
        const uint8x16_t vb8 = vld1q_u8(B8), vsel = vld1q_u8(SEL);
#define BBB0_SEL(W) vtstq_u8(vqtbl1q_u8(vreinterpretq_u8_u16(vdupq_n_u16((uint16_t)(W))), vsel), vb8)
        for (int i = 0; i < WIDTH; i += 32) {
            uint32_t w = (uint32_t)mask[i >> 4] | ((uint32_t)mask[(i >> 4) + 1] << 16);
            uint8x16_t s0 = BBB0_SEL(w), s1 = BBB0_SEL(w >> 16);
            uint8x16x4_t f0 = vld4q_u8(source + (unsigned)i * 4u);
            uint8x16x4_t f1 = vld4q_u8(source + (unsigned)i * 4u + 64u);
            vst1q_u8(p0 + i,      vbslq_u8(s0, f0.val[0], vld1q_u8(p0 + i)));
            vst1q_u8(p0 + i + 16, vbslq_u8(s1, f1.val[0], vld1q_u8(p0 + i + 16)));
            vst1q_u8(p1 + i,      vbslq_u8(s0, f0.val[1], vld1q_u8(p1 + i)));
            vst1q_u8(p1 + i + 16, vbslq_u8(s1, f1.val[1], vld1q_u8(p1 + i + 16)));
            vst1q_u8(p2 + i,      vbslq_u8(s0, f0.val[2], vld1q_u8(p2 + i)));
            vst1q_u8(p2 + i + 16, vbslq_u8(s1, f1.val[2], vld1q_u8(p2 + i + 16)));
            if (quarter) {
                vst1q_u8(quarter + i,      vbslq_u8(s0, f0.val[3], vld1q_u8(quarter + i)));
                vst1q_u8(quarter + i + 16, vbslq_u8(s1, f1.val[3], vld1q_u8(quarter + i + 16)));
            }
        }
#undef BBB0_SEL
        return;
    }
#else

    for (int i = 0; i < WIDTH; i++) {
        int is_set = (mask[i >> 4] >> (i & 15)) & 1;
        if (!is_set) continue;

        p0[i] = source[i * 4 + 0];
        p1[i] = source[i * 4 + 1];
        p2[i] = source[i * 4 + 2];
        if (quarter) quarter[i] = source[i * 4 + 3];
    }
#endif
}
#undef WIDTH
#undef PLANE

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#define WIDTH 256
#define PLANE 0x100

void gpu2d_compose_split_rgb_planes(uint8_t *dest, const uint16_t *origin) {

    uint8_t *r = dest;
    uint8_t *g = dest + PLANE;
    uint8_t *b = dest + 2 * PLANE;

#ifdef __ARM_NEON

    const uint16x8_t m3e = vdupq_n_u16(0x3eu);
    for (int i = 0; i < WIDTH; i += 8) {
        uint16x8_t px = vld1q_u16(origin + i);
        vst1_u8(r + i, vmovn_u16(vandq_u16(vshlq_n_u16(px, 1), m3e)));
        vst1_u8(g + i, vmovn_u16(vandq_u16(vshrq_n_u16(px, 4), m3e)));
        vst1_u8(b + i, vmovn_u16(vandq_u16(vshrq_n_u16(px, 9), m3e)));
    }
#else
    for (int i = 0; i < WIDTH; i++) {
        uint16_t px = origin[i];
        r[i] = (uint8_t)((px << 1) & 0x3e);
        g[i] = (uint8_t)((px >> 4) & 0x3e);
        b[i] = (uint8_t)((px >> 9) & 0x3e);
    }
#endif
}
#undef WIDTH
#undef PLANE

void gpu2d_compose_mark_empty_groups(const unsigned char *data, uint32_t *mask) {

    uint32_t v = *mask;
    uint32_t bits = 0;

    for (int g = 0; g < 4; g++) {
        int some = 0;
        for (int k = 0; k < 32; k++)
            if (data[g * 32 + k]) { some = 1; break; }
        if (some) bits |= (1u << g);
    }

    *mask = v & (bits | 0xf0u | 0xff00u);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif


void gpu2d_compose_pack_planes_to_rgb15(const gpu_output_t *ctx, uint8_t *dst, const uint8_t *planes)
{

    uint32_t cnt = ctx->capture.width;

    const uint8_t *pr = planes;
    const uint8_t *pg = planes + 0x100;
    const uint8_t *pb = planes + 0x200;

    size_t off = 0;
#ifdef __ARM_NEON

    do {
        const uint8_t *R = pr + off, *G = pg + off, *B = pb + off;
        uint8_t *D = dst + off * 2;
        for (unsigned m = 0; m < 32u; m += 16u) {
            uint8x16_t r = vshrq_n_u8(vld1q_u8(R + m), 1);
            uint8x16_t g = vshrq_n_u8(vld1q_u8(G + m), 1);
            uint8x16_t b = vshrq_n_u8(vld1q_u8(B + m), 1);

            uint16x8_t lo = vaddw_u8(vshll_n_u8(vget_low_u8(g), 5), vget_low_u8(r));
            uint16x8_t hi = vaddw_u8(vshll_n_u8(vget_high_u8(g), 5), vget_high_u8(r));
            uint16x8_t bl = vshlq_n_u16(vmovl_u8(vget_low_u8(b)), 10);
            uint16x8_t bh = vshlq_n_u16(vmovl_u8(vget_high_u8(b)), 10);
            const uint16x8_t opaque = vdupq_n_u16(0x8000u);

            vst1q_u16((uint16_t *)(D + (size_t)m * 2),
                      vorrq_u16(vorrq_u16(lo, opaque), bl));
            vst1q_u16((uint16_t *)(D + (size_t)m * 2 + 16),
                      vorrq_u16(vorrq_u16(hi, opaque), bh));
        }
        off += 32;
        cnt -= 32;
    } while (cnt != 0);
#else
    do {
        for (unsigned k = 0; k < 32; k++) {
            unsigned r8 = pr[off + k] >> 1;
            unsigned g8 = pg[off + k] >> 1;
            unsigned b8 = pb[off + k] >> 1;

            uint16_t g5  = (uint16_t)(g8 << 5);
            uint16_t sum = (uint16_t)(g5 + (uint16_t)r8);
            uint16_t bsh = (uint16_t)(b8 << 10);

            uint16_t val = (uint16_t)(sum | 0x8000u | bsh);

            wr16(dst + (off + k) * 2, val);
        }
        off += 32;
        cnt -= 32;
    } while (cnt != 0);
#endif
}


void gpu2d_compose_pack_rgba_to_rgb15(const gpu_output_t *ctx, uint8_t *dst, const uint8_t *src)
{

    uint32_t cnt = ctx->capture.width;

    size_t off = 0;
    do {
        for (unsigned k = 0; k < 32; k++) {
            size_t pix = off + k;
            const uint8_t *p4 = src + pix * 4;

            unsigned r8 = (unsigned)p4[0] >> 1;
            unsigned g8 = (unsigned)p4[1] >> 1;
            unsigned b8 = (unsigned)p4[2] >> 1;
            int8_t   a8 = (int8_t)p4[3];

            unsigned amask = (a8 > 0) ? 0xffu : 0x00u;

            unsigned bmix = (b8 & 0x1fu) | ((amask << 5) & 0xe0u);

            uint16_t g16 = (uint16_t)(g8 << 5);
            uint16_t sum = (uint16_t)(g16 + (uint16_t)r8);
            uint16_t bsh = (uint16_t)(bmix << 10);

            uint16_t val = (uint16_t)(sum | bsh);

            wr16(dst + pix * 2, val);
        }
        off += 32;
        cnt -= 32;
    } while (cnt != 0);
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"
#include "core_internals.h"

void gpu2d_compose_extract_alpha_plane(unsigned char *dst, const unsigned char *src) {
#ifdef __ARM_NEON

    {
        int i;
        for (i = 0; i < 256; i += 64) {
            uint8x16x4_t a = vld4q_u8(src); src += 64;
            uint8x16x4_t b = vld4q_u8(src); src += 64;
            uint8x16x4_t c = vld4q_u8(src); src += 64;
            uint8x16x4_t d = vld4q_u8(src); src += 64;
            vst1q_u8(dst, a.val[3]); dst += 16;
            vst1q_u8(dst, b.val[3]); dst += 16;
            vst1q_u8(dst, c.val[3]); dst += 16;
            vst1q_u8(dst, d.val[3]); dst += 16;
        }
        return;
    }
#else

    uint32_t n = 256;
    do {
        unsigned char t[64];
        for (int b = 0; b < 4; b++)
            for (int k = 0; k < 16; k++)
                t[b * 16 + k] = src[b * 64 + k * 4 + 3];
        src += 256;
        for (int k = 0; k < 64; k++)
            dst[k] = t[k];
        dst += 64;
        n -= 0x40;
    } while (n != 0);
#endif
}

static const unsigned char ALPHA_4H[16] = {
    0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#define PLANE_B 0x100u

void gpu2d_compose_extract_alpha_plane_bf00(const unsigned char *pl, unsigned char *dst) {
    for (unsigned g = 0; g < 8; g++) {
        unsigned char p0[32], p1[32], p2[32];
        for (unsigned k = 0; k < 32; k++) {
            p0[k] = pl[k];
            p1[k] = pl[PLANE_B + k];
            p2[k] = pl[2 * PLANE_B + k];
        }
        for (unsigned k = 0; k < 32; k++) {
            dst[4 * k + 0] = (unsigned char)(p0[k] << 2);
            dst[4 * k + 1] = (unsigned char)(p1[k] << 2);
            dst[4 * k + 2] = (unsigned char)(p2[k] << 2);
            dst[4 * k + 3] = 0xff;
        }
        pl += 32;
        dst += 128;
    }
}

void gpu2d_compose_extract_alpha_plane_bf60(const unsigned char *a, const unsigned char *b,
                             unsigned char *dst) {
#ifdef __ARM_NEON

    {
        unsigned g;
        uint8x16_t al = vld1q_u8(ALPHA_4H);
        for (g = 0; g < 16; g++) {
            uint8x16_t a0 = vshlq_n_u8(vld1q_u8(a + 0 * PLANE_B), 2);
            uint8x16_t a1 = vshlq_n_u8(vld1q_u8(a + 1 * PLANE_B), 2);
            uint8x16_t a2 = vshlq_n_u8(vld1q_u8(a + 2 * PLANE_B), 2);
            uint8x16_t b0 = vshlq_n_u8(vld1q_u8(b + 0 * PLANE_B), 2);
            uint8x16_t b1 = vshlq_n_u8(vld1q_u8(b + 1 * PLANE_B), 2);
            uint8x16_t b2 = vshlq_n_u8(vld1q_u8(b + 2 * PLANE_B), 2);
            uint8x16x4_t lo, hi;
            lo.val[0] = vzip1q_u8(a0, b0); hi.val[0] = vzip2q_u8(a0, b0);
            lo.val[1] = vzip1q_u8(a1, b1); hi.val[1] = vzip2q_u8(a1, b1);
            lo.val[2] = vzip1q_u8(a2, b2); hi.val[2] = vzip2q_u8(a2, b2);
            lo.val[3] = al;                hi.val[3] = al;
            vst4q_u8(dst, lo);
            vst4q_u8(dst + 64, hi);
            a += 16; b += 16; dst += 128;
        }
        return;
    }
#else
    for (unsigned g = 0; g < 16; g++) {
        unsigned char A[3][16], B[3][16];
        for (unsigned c = 0; c < 3; c++)
            for (unsigned k = 0; k < 16; k++) {
                A[c][k] = a[c * PLANE_B + k];
                B[c][k] = b[c * PLANE_B + k];
            }
        for (unsigned j = 0; j < 32; j++) {
            unsigned k = j >> 1;
            const unsigned char *s0 = (j & 1) ? B[0] : A[0];
            const unsigned char *s1 = (j & 1) ? B[1] : A[1];
            const unsigned char *s2 = (j & 1) ? B[2] : A[2];
            dst[4 * j + 0] = (unsigned char)(s0[k] << 2);
            dst[4 * j + 1] = (unsigned char)(s1[k] << 2);
            dst[4 * j + 2] = (unsigned char)(s2[k] << 2);
            dst[4 * j + 3] = ALPHA_4H[j & 15];
        }
        a += 16;
        b += 16;
        dst += 128;
    }
#endif
}

static void a565(unsigned char r, unsigned char v, unsigned char z,
                 unsigned char *low, unsigned char *height) {
    *height = (unsigned char)(((unsigned char)(r << 2) & 0xe0u)
                            | (unsigned char)(v >> 3));
    *low = (unsigned char)(((unsigned char)(v << 5) & 0xe0u)
                            | ((unsigned char)(z >> 1) & 0x1fu));
}

void gpu2d_compose_extract_alpha_plane_bfdc(const unsigned char *pl, unsigned char *dst) {
    for (unsigned g = 0; g < 4; g++) {
        unsigned char p0[64], p1[64], p2[64];
        for (unsigned k = 0; k < 64; k++) {
            p0[k] = pl[k];
            p1[k] = pl[PLANE_B + k];
            p2[k] = pl[2 * PLANE_B + k];
        }
        for (unsigned k = 0; k < 64; k++)
            a565(p0[k], p1[k], p2[k], &dst[2 * k], &dst[2 * k + 1]);
        pl += 64;
        dst += 128;
    }
}

void gpu2d_compose_extract_alpha_plane_c074(const unsigned char *a, const unsigned char *b,
                             unsigned char *dst) {
    for (unsigned g = 0; g < 8; g++) {
        unsigned char A[3][32], B[3][32];
        for (unsigned c = 0; c < 3; c++)
            for (unsigned k = 0; k < 32; k++) {
                A[c][k] = a[c * PLANE_B + k];
                B[c][k] = b[c * PLANE_B + k];
            }
        for (unsigned k = 0; k < 32; k++) {
            a565(A[0][k], A[1][k], A[2][k], &dst[4 * k + 0], &dst[4 * k + 1]);
            a565(B[0][k], B[1][k], B[2][k], &dst[4 * k + 2], &dst[4 * k + 3]);
        }
        a += 32;
        b += 32;
        dst += 128;
    }
}

void gpu2d_compose_extract_alpha_plane_c110(const unsigned char *pl, unsigned char *dst,
                             uint32_t mult, uint32_t sum) {
    uint32_t m = (uint8_t)mult;
    uint32_t s = (uint16_t)sum;
    for (unsigned g = 0; g < 8; g++) {
        unsigned char p[3][32];
        for (unsigned c = 0; c < 3; c++)
            for (unsigned k = 0; k < 32; k++)
                p[c][k] = pl[c * PLANE_B + k];
        for (unsigned k = 0; k < 32; k++) {
            for (unsigned c = 0; c < 3; c++) {
                uint16_t acc = (uint16_t)(s + (uint16_t)(p[c][k] * m));
                dst[4 * k + c] = (unsigned char)((unsigned char)(acc >> 5) << 2);
            }
            dst[4 * k + 3] = 0xff;
        }
        pl += 32;
        dst += 128;
    }
}

void gpu2d_compose_extract_alpha_plane_c200(const unsigned char *a, const unsigned char *b,
                             unsigned char *dst, uint32_t mult, uint32_t sum) {
    uint32_t m = (uint8_t)mult;
    uint32_t s = (uint16_t)sum;
    for (unsigned g = 0; g < 16; g++) {
        unsigned char A[3][16], B[3][16];
        uint16_t accA0[16], accB1[16];
        for (unsigned c = 0; c < 3; c++)
            for (unsigned k = 0; k < 16; k++) {
                uint32_t qa = (unsigned char)(a[c * PLANE_B + k] << 2);
                uint32_t qb = (unsigned char)(b[c * PLANE_B + k] << 2);
                uint16_t ca = (uint16_t)(s + (uint16_t)(qa * m));
                uint16_t cb = (uint16_t)(s + (uint16_t)(qb * m));
                A[c][k] = (unsigned char)(ca >> 5);
                B[c][k] = (unsigned char)(cb >> 5);
                if (c == 0) accA0[k] = ca;
                if (c == 1) accB1[k] = cb;
            }

        unsigned char alpha[32];
        for (unsigned t = 0; t < 8; t++) {
            alpha[2 * t + 0]      = (unsigned char)(accA0[8 + t] & 0xff);
            alpha[2 * t + 1]      = (unsigned char)(accA0[8 + t] >> 8);
            alpha[16 + 2 * t + 0] = (unsigned char)(accB1[t] & 0xff);
            alpha[16 + 2 * t + 1] = (unsigned char)(accB1[t] >> 8);
        }
        for (unsigned j = 0; j < 32; j++) {
            unsigned k = j >> 1;
            const unsigned char *s0 = (j & 1) ? B[0] : A[0];
            const unsigned char *s1 = (j & 1) ? B[1] : A[1];
            const unsigned char *s2 = (j & 1) ? B[2] : A[2];
            dst[4 * j + 0] = s0[k];
            dst[4 * j + 1] = s1[k];
            dst[4 * j + 2] = s2[k];
            dst[4 * j + 3] = alpha[j];
        }
        a += 16;
        b += 16;
        dst += 128;
    }
}
#undef PLANE_B

void gpu2d_compose_gen_row_ramp_halfword(uint16_t *dst, unsigned char *tables, uint32_t rows,
                        const uint32_t *steps) {

    static const uint32_t base[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    steps = recon_steps_table(steps, 0x80000000u);

    const uint16_t *ent = (const uint16_t *)(tables + 9u * RECON_3D_ROW_ARRAY);

    int32_t n = (int32_t)*ent;  ent += 2;

    do {
        uint32_t step = steps[(uint32_t)n];
        uint32_t acc[8];
        for (int k = 0; k < 8; k++) acc[k] = base[k] * step;
        uint32_t advance = step << 3;

        uint16_t *q = dst;
        do {
            for (int k = 0; k < 8; k++) q[k] = (uint16_t)(acc[k] >> 16);
            for (int k = 0; k < 8; k++) acc[k] += advance;
            q += 8;
            n -= 8;
        } while (n > 0);

        dst = q + (int64_t)n;
        n = (int32_t)*ent;  ent += 2;
    } while (--rows != 0);
}

void gpu2d_compose_gen_row_ramp_q30(uint32_t *param_1, unsigned char *param_2,
                         int32_t param_3, const int32_t *param_4)
{

    const uint16_t *pn = (const uint16_t *)(param_2 + 9u * RECON_3D_ROW_ARRAY);
    const int32_t  *pb = (const int32_t  *)(param_2 + 3u * RECON_3D_ROW_ARRAY);
    const int32_t  *pc = (const int32_t  *)(param_2 + 2u * RECON_3D_ROW_ARRAY);

    uint32_t *dst = param_1;
    int32_t rows = param_3;

    do {
        uint32_t n = *pn;
        int32_t  vb = *pb;
        int32_t  c = *pc;
        pn += 2;
        pb += 1;
        pc += 1;

        int32_t t = param_4[n];

        uint64_t bias = (vb < 0) ? 0x3FFFFFFFu : 0u;
        uint64_t step = bias + (uint64_t)((int64_t)vb * (int64_t)t);

        uint64_t c0 = ((uint64_t)(uint32_t)c) << 30;

        uint64_t a0 = c0;
        uint64_t a1 = c0 + step;
        uint64_t a2 = a1 + step;
        uint64_t a3 = a2 + step;
        uint64_t step4 = step * 4u;

        int32_t remaining = (int32_t)n;
        uint32_t *q = dst;
        for (;;) {
            q[0] = (uint32_t)(a0 >> 30);
            q[1] = (uint32_t)(a1 >> 30);
            q[2] = (uint32_t)(a2 >> 30);
            q[3] = (uint32_t)(a3 >> 30);

            a0 += step4;
            a1 += step4;
            a2 += step4;
            a3 += step4;

            q += 4;
            remaining -= 4;
            if (!(remaining > 0)) {
                break;
            }
        }
        dst = q + remaining;

        rows -= 1;
    } while (rows != 0);
}

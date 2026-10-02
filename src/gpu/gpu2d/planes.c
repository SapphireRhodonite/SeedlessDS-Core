#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "mem_access.h"








void gpu2d_planes_accumulate_block_masks(const void *param_1, const void *param_2,
                         void *param_3)
{

    const uint8_t *ctx = (const uint8_t *)param_1;
    const uint8_t *src = (const uint8_t *)param_2;
    uint8_t *dst = (uint8_t *)param_3;
    uint32_t mask0 = 0;
    uint32_t mask1 = 0;
    uint32_t mask2 = 0;
    uint32_t mask3 = 0;
    uint32_t mask4 = 0;
    uint32_t mask5 = 0;
    uint32_t mask6 = 0;
    uint32_t mask7 = 0;
    uint32_t state0 = 0;
    uint32_t state1 = 0;
    uint32_t state2 = 0;
    uint32_t state3_saved = 0;
    uint32_t state2_saved = 0;
    uint32_t state4 = 0;
    uint32_t idx = 0;
    uint8_t count;

    wr32(dst + 128, 0);
    wr32(dst + 132, 0);
    wr32(dst + 136, 0);
    wr32(dst + 140, 0);
    wr32(dst + 144, 0);
    wr32(dst + 148, 0);
    wr32(dst + 152, 0);
    wr32(dst + 156, 0);

    count = rd8(ctx + 179);
    if (count == 0) {
        wr32(dst + 160, UINT32_MAX);
        wr32(dst + 164, UINT32_MAX);
        wr32(dst + 168, UINT32_MAX);
        wr32(dst + 172, UINT32_MAX);
        wr32(dst + 176, UINT32_MAX);
        wr32(dst + 180, UINT32_MAX);
        wr32(dst + 184, UINT32_MAX);
        wr32(dst + 188, UINT32_MAX);
        return;
    }

    for (;;) {
        uint8_t group = rd8(ctx + 132 + idx);
        uint32_t offset = (uint32_t)group << 5;
        uint32_t word0 = rd32(src + offset);

        if ((group & 4u) == 0) {
            uint32_t word1;
            uint32_t word2;
            uint32_t word3;
            uint32_t word4;
            uint32_t word5;
            uint32_t word6;
            uint32_t word7;

            wr32(dst + offset, word0 & ~mask0);
            word1 = rd32(src + offset + 4);
            wr32(dst + offset + 4, word1 & ~mask1);
            word2 = rd32(src + offset + 8);
            wr32(dst + offset + 8, word2 & ~mask2);
            word3 = rd32(src + offset + 12);
            wr32(dst + offset + 12, word3 & ~mask3);
            word4 = rd32(src + offset + 16);
            wr32(dst + offset + 16, word4 & ~mask4);
            word5 = rd32(src + offset + 20);
            wr32(dst + offset + 20, word5 & ~mask5);
            word6 = rd32(src + offset + 24);
            wr32(dst + offset + 24, word6 & ~mask6);
            word7 = rd32(src + offset + 28);
            wr32(dst + offset + 28, word7 & ~mask7);

            mask0 |= word0;
            mask1 |= word1;
            mask2 |= word2;
            mask3 |= word3;
            mask4 |= word4;
            mask5 |= word5;
            mask6 |= word6;
            mask7 |= word7;
        } else {
            uint32_t candidate0;
            uint32_t candidate1;
            uint32_t candidate2;
            uint32_t candidate3;
            uint32_t candidate4;
            uint32_t candidate5;
            uint32_t candidate6;
            uint32_t candidate7;
            uint32_t word1;
            uint32_t word2;
            uint32_t word3;
            uint32_t word4;
            uint32_t word5;
            uint32_t word6;
            uint32_t word7;
            uint32_t old_mask6 = mask6;
            uint32_t old_mask7 = mask7;
            uint32_t old_state2 = state2;
            uint32_t state3 = state3_saved;
            uint32_t mask_for5 = state2_saved;

            candidate0 = word0 & ~state1;
            wr32(dst + 128, rd32(dst + 128) |
                  (candidate0 & ~mask0));
            state1 |= word0;

            word1 = rd32(src + offset + 4);
            candidate1 = word1 & ~state0;
            wr32(dst + 132, rd32(dst + 132) | (candidate1 & ~mask1));

            word2 = rd32(src + offset + 8);
            state0 |= word1;
            candidate2 = word2 & ~old_state2;
            wr32(dst + 136, rd32(dst + 136) | (candidate2 & ~mask2));

            word3 = rd32(src + offset + 12);
            candidate3 = word3 & ~state3;
            wr32(dst + 140, rd32(dst + 140) | (candidate3 & ~mask3));

            word4 = rd32(src + offset + 16);
            state3 |= word3;
            candidate4 = word4 & ~state4;
            wr32(dst + 144, rd32(dst + 144) | (candidate4 & ~mask4));

            word5 = rd32(src + offset + 20);
            state4 |= word4;
            candidate5 = word5 & ~mask_for5;
            wr32(dst + 148, rd32(dst + 148) | (candidate5 & ~mask5));

            word6 = rd32(src + offset + 24);
            mask_for5 |= word5;
            candidate6 = word6;
            wr32(dst + 152, rd32(dst + 152) |
                  (candidate6 & ~old_mask6));

            word7 = rd32(src + offset + 28);
            candidate7 = word7 & ~old_mask7;
            wr32(dst + 156, rd32(dst + 156) |
                  (candidate7 & ~old_mask7));

            count = rd8(ctx + 179);
            state3_saved = state3;
            state2_saved = old_state2 | word2;
            state2 = old_state2 | word2;
            mask0 |= candidate0;
            mask1 |= candidate1;
            mask2 |= candidate2;
            mask3 |= candidate3;
            mask4 |= candidate4;
            mask5 |= candidate5;
            mask6 = old_mask6 | candidate6;
            mask7 = old_mask7 | candidate7;
        }

        idx++;
        if (idx >= count)
            break;
    }

    wr32(dst + 160, ~mask0);
    wr32(dst + 164, ~mask1);
    wr32(dst + 168, ~mask2);
    wr32(dst + 172, ~mask3);
    wr32(dst + 176, ~mask4);
    wr32(dst + 180, ~mask5);
    wr32(dst + 184, ~mask6);
    wr32(dst + 188, ~mask7);
}




static void distribute_word(uint32_t value, uint8_t *output_a,
                          uint8_t *output_b, uint32_t *primary,
                          uint32_t *repeated)
{
    uint32_t before_main = *primary;
    uint32_t common;
    uint32_t before_repeated;

    wr32(output_a, value & ~before_main);
    common = before_main & value;
    *primary = before_main | value;
    before_repeated = *repeated;
    wr32(output_b, common & ~before_repeated);
    *repeated = before_repeated | common;
}

static void distribute_word_masked(uint32_t value, uint8_t *output_a,
                            uint8_t *output_b, uint32_t *view,
                            uint32_t *primary, uint32_t *repeated)
{
    uint32_t updated = value & ~*view;
    uint32_t before_main = *primary;
    uint32_t common;
    uint32_t before_repeated;

    wr32(output_a, rd32(output_a) |
                (updated & ~before_main));
    common = before_main & updated;
    *primary = before_main | updated;
    before_repeated = *repeated;
    wr32(output_b, rd32(output_b) |
                (common & ~before_repeated));
    *repeated = before_repeated | common;
    *view |= value;
}

void gpu2d_planes_distribute_page_words(const uint8_t *param_1, const uint8_t *param_2,
                         uint8_t *param_3, uint8_t *param_4)
{
    uint32_t view[8] = {0};
    uint32_t primary[8] = {0};
    uint32_t repeated[8] = {0};
    uint64_t idx = 0;
    uint32_t word;
    unsigned int j;

    for (j = 0; j != 8; ++j) {
        wr32(param_3 + 128u + j * 4u, 0);
    }
    for (j = 0; j != 8; ++j) {
        wr32(param_4 + 128u + j * 4u, 0);
    }

    if (rd8(param_1 + 179u) != 0) {
        for (;;) {
            uint8_t page = rd8(param_1 + 132u + idx);
            uint64_t offset = (uint64_t)page << 5;
            const uint8_t *origin = param_2 + offset;

            word = rd32(origin);
            if ((page & 4u) != 0) {
                distribute_word_masked(word, param_3 + 128u, param_4 + 128u,
                                &view[0], &primary[0], &repeated[0]);
                for (j = 1; j != 8; ++j) {
                    word = rd32(origin + j * 4u);
                    distribute_word_masked(word, param_3 + 128u + j * 4u,
                                    param_4 + 128u + j * 4u, &view[j],
                                    &primary[j], &repeated[j]);
                }
            } else {
                distribute_word(word, param_3 + offset,
                              param_4 + offset, &primary[0],
                              &repeated[0]);
                for (j = 1; j != 8; ++j) {
                    word = rd32(origin + j * 4u);
                    distribute_word(word,
                                  param_3 + offset + j * 4u,
                                  param_4 + offset + j * 4u,
                                  &primary[j], &repeated[j]);
                }
            }

            ++idx;
            if (idx >= rd8(param_1 + 179u)) {
                break;
            }
        }
    }

    for (j = 0; j != 8; ++j) {
        wr32(param_3 + 160u + j * 4u, ~primary[j]);
        wr32(param_4 + 160u + j * 4u,
                    primary[j] & ~repeated[j]);
    }
}




void gpu2d_planes_select_copy_masked(uint8_t *dest, const uint8_t *origin_a,
                         const uint8_t *origin_b, const uint8_t *masks)
{

    if (dest != origin_a) {
        for (uint32_t block = 0; block != 8; block++) {
            uint32_t mask = rd32(masks + block * 4u);
            uint32_t base = block * 64u;

            for (uint32_t halfword = 0; halfword != 32; halfword++) {
                uint32_t offset = base + halfword * 2u;
                const uint8_t *origin = (mask & (1u << halfword)) != 0
                    ? origin_b : origin_a;
                wr16(dest + offset,
                      rd16(origin + offset));
            }
        }
    } else {
        for (uint32_t block = 0; block != 8; block++) {
            uint32_t mask = rd32(masks + block * 4u);
            uint32_t base = block * 64u;

            for (uint32_t halfword = 0; halfword != 32; halfword++) {
                uint32_t offset = base + halfword * 2u;
                if ((mask & (1u << halfword)) != 0) {
                    wr16(dest + offset,
                          rd16(origin_b + offset));
                }
            }
        }
    }
}

void gpu2d_planes_fill_or_copy_masked(uint16_t *dest, uint64_t source, uint16_t fill,
                         const uint32_t *mask)
{

    const unsigned char *base_source = (const unsigned char *)source;
    unsigned char *base_dest = (unsigned char *)dest;

    for (int word = 0; word < 8; word++) {
        uint32_t bits;
        memcpy(&bits, &mask[word], sizeof(bits));

        for (int bit = 0; bit < 32; bit++) {
            int m = word * 32 + bit;
            uint16_t value;

            if ((bits >> bit) & 1u) {
                value = fill;
            } else {
                memcpy(&value, base_source + (size_t)m * 2, sizeof(value));
            }

            memcpy(base_dest + (size_t)m * 2, &value, sizeof(value));
        }
    }
}



void gpu2d_window_layer_mask_combine(uint8_t *dst, const uint8_t *src,
                         uint32_t param_3, uint32_t param_4)
{

    uint32_t accumulated[8] = {0};
    uint32_t selector_entry = param_3;
    uint32_t selector_op = param_4;

    memset(dst, 0, 32);

    for (uint32_t group = 0; group != 5; group++) {
        uint32_t mask_entry = 0u - (selector_entry & 1u);
        uint32_t bit_op = selector_op & 1u;
        uint32_t mask_set = 0u - bit_op;
        uint32_t mask_clear = bit_op - 1u;

        for (uint32_t word = 0; word != 8; word++) {
            uint32_t value = rd32(src + group * 32u + word * 4u);
            value &= mask_entry;
            accumulated[word] = (accumulated[word] |
                                  (value & mask_set)) &
                                 ~(value & mask_clear);
            wr32(dst + word * 4u, accumulated[word]);
        }

        selector_entry >>= 1;
        selector_op >>= 1;
    }

    {
        uint32_t bit_op = selector_op & 1u;
        uint32_t mask_set = 0u - bit_op;
        uint32_t mask_clear = bit_op - 1u;

        for (uint32_t word = 0; word != 8; word++) {
            uint32_t value = rd32(src + 160u + word * 4u);
            accumulated[word] = (accumulated[word] |
                                  (value & mask_set)) &
                                 ~(value & mask_clear);
            wr32(dst + word * 4u, accumulated[word]);
        }
    }
}

void gpu2d_planes_blend_three_strips_masked(const uint8_t *param_1, uint8_t *param_2,
                         const uint8_t *param_3, const uint32_t *param_4)
{

    uint16_t width;
    memcpy(&width, param_1 + 0xa2, sizeof(width));
    uint8_t flags;
    memcpy(&flags, param_1 + 0xa0, sizeof(flags));

    uint32_t doubled = (uint32_t)width << 1;
    uint32_t clamp = (width > 0x10u) ? 0x20u : doubled;
    uint32_t weight = 0x20u - clamp;
    uint32_t end = clamp * 0x3fu + 0x10u;
    if (flags & 0x40u) {
        end = 0x10u;
    }

    for (int word = 0; word < 8; word++) {
        uint32_t mask;
        memcpy(&mask, &param_4[word], sizeof(mask));
        int base = word * 32;

        for (int bit = 0; bit < 32; bit++) {
            int idx = base + bit;

            uint8_t val0 = param_3[idx];
            uint8_t val1 = param_3[idx + 256];
            uint8_t val2 = param_3[idx + 512];

            if (mask & 1u) {
                param_2[idx]       = (uint8_t)((weight * val0 + end) >> 5);
                param_2[idx + 256] = (uint8_t)((weight * val1 + end) >> 5);
                param_2[idx + 512] = (uint8_t)((weight * val2 + end) >> 5);
            } else {
                param_2[idx]       = val0;
                param_2[idx + 256] = val1;
                param_2[idx + 512] = val2;
            }

            mask >>= 1;
        }
    }
}

void gpu2d_planes_render_shadow_and_zero(int32_t param_1, uint64_t param_2, uint64_t param_3,
                         uint32_t *param_4)
{

    uint32_t doubled = (uint32_t)param_1 << 1;
    uint32_t clamp    = (doubled < 0x20u) ? doubled : 0x20u;
    uint8_t  on_byte  = (uint8_t)(0x20u - clamp);
    const uint8_t off_byte = 0x20u;

    for (int word = 0; word < 8; word++) {
        uint32_t bits;
        memcpy(&bits, &param_4[word], sizeof(bits));

        uint8_t *dst_shadow = (uint8_t *)(param_2 + (uint64_t)word * 0x20);
        uint8_t *dst_zeros  = (uint8_t *)(param_3 + (uint64_t)word * 0x20);

        for (int bit = 0; bit < 32; bit++) {
            uint8_t v = (bits & (1u << bit)) ? on_byte : off_byte;
            dst_shadow[bit] = v;
            dst_zeros[bit]  = 0;
        }
    }
}

void gpu2d_planes_render_shadow_mirror_zero(int32_t param_1, uint64_t param_2, uint64_t param_3,
                         uint64_t param_4, const uint32_t *param_5)
{

    uint32_t doubled = (uint32_t)param_1 << 1;
    uint32_t clamp     = (doubled < 0x20u) ? doubled : 0x20u;
    uint8_t  on_p2     = (uint8_t)(0x20u - clamp);
    const uint8_t off_p2 = 0x20u;
    uint8_t  on_p4     = (uint8_t)clamp;
    const uint8_t off_p4 = 0u;

    for (int word = 0; word < 8; word++) {
        uint32_t bits;
        memcpy(&bits, &param_5[word], sizeof(bits));

        uint8_t *dst_p2   = (uint8_t *)(param_2 + (uint64_t)word * 0x20);
        uint8_t *dst_p4   = (uint8_t *)(param_4 + (uint64_t)word * 0x20);
        uint8_t *dst_zeros = (uint8_t *)(param_3 + (uint64_t)word * 0x20);

        for (int bit = 0; bit < 32; bit++) {
            int on = (bits & (1u << bit)) != 0;
            dst_p2[bit]    = on ? on_p2 : off_p2;
            dst_p4[bit]    = on ? on_p4 : off_p4;
            dst_zeros[bit] = 0;
        }
    }
}

void gpu2d_planes_stamp_dual_constant_masked(uint32_t param_1, uint8_t *param_2, uint8_t *param_3,
                         const uint32_t *param_4)
{

    uint32_t raw_a = (param_1 & 0x1fu) << 1;
    uint32_t raw_b = (param_1 >> 7) & 0x3eu;

    uint8_t val_a = (uint8_t)((raw_a < 0x20u) ? raw_a : 0x20u);
    uint8_t val_b = (uint8_t)((raw_b < 0x20u) ? raw_b : 0x20u);

    for (int word = 0; word < 8; word++) {
        uint32_t bits;
        memcpy(&bits, &param_4[word], sizeof(bits));

        uint8_t *dst_a = param_2 + (size_t)word * 0x20;
        uint8_t *dst_b = param_3 + (size_t)word * 0x20;

        for (int bit = 0; bit < 32; bit++) {
            if (bits & (1u << bit)) {
                dst_a[bit] = val_a;
                dst_b[bit] = val_b;
            }
        }
    }
}

void gpu2d_planes_expand_mirror_from_table(unsigned char *param_1, unsigned char *param_2,
                        const unsigned char *param_3,
                        const unsigned char *param_4) {

    uint32_t w9;
    memcpy(&w9, param_4 + 0, 4);
    uint64_t x8 = 0;
    const uint32_t w10 = 31u;
    do {
        uint32_t v = param_3[x8];
        uint32_t bit = w9 & 1u;
        w9 >>= 1;
        if (bit == 0) v = w10;
        uint32_t plus = v + 1u;
        uint32_t minus = w10 - v;
        param_1[x8] = (unsigned char)plus;
        param_2[x8] = (unsigned char)minus;
        x8 += 1;
    } while ((uint32_t)x8 != 0x20u);

    memcpy(&w9, param_4 + 4, 4);
    uint64_t x10 = 0;
    const uint32_t w11 = 31u;
    do {
        const unsigned char *p12 = param_3 + x10;
        uint32_t v = p12[x8];
        uint32_t bit = w9 & 1u;
        unsigned char *p13 = param_2 + x10;
        unsigned char *p14 = param_1 + x10;
        x10 += 1;
        if (bit == 0) v = w11;
        uint32_t plus = v + 1u;
        uint32_t minus = w11 - v;
        w9 >>= 1;
        p14[x8] = (unsigned char)plus;
        p13[x8] = (unsigned char)minus;
    } while ((uint32_t)x10 != 0x20u);

    unsigned char *d0 = param_1 + x8 + x10;
    unsigned char *d1 = param_2 + x8 + x10;
    const unsigned char *s = param_3 + x8 + x10;

    for (int g = 2; g < 8; g++) {
        uint32_t w11b;
        memcpy(&w11b, param_4 + 4 * g, 4);
        uint32_t w12 = 0x20u;
        const uint32_t w13 = 31u;
        do {
            uint32_t v = *s++;
            uint32_t bit = w11b & 1u;
            w11b >>= 1;
            if (bit == 0) v = w13;
            uint32_t plus = v + 1u;
            uint32_t minus = w13 - v;
            w12 -= 1u;
            *d0++ = (unsigned char)plus;
            *d1++ = (unsigned char)minus;
        } while (w12 != 0);
    }
}

void gpu2d_planes_expand_mirror_masked(uint8_t *dst1, uint8_t *dst2, const uint8_t *src,
                         const uint32_t *masks)
{

    uint32_t mask;

    memcpy(&mask, &masks[0], sizeof(mask));
    for (int i = 0; i < 32; i++) {
        if (mask & 1u) {
            uint8_t val = src[i];
            dst1[i] = (uint8_t)(val + 1);
            dst2[i] = (uint8_t)(0x1f - val);
        }
        mask >>= 1;
    }

    memcpy(&mask, &masks[1], sizeof(mask));
    for (int i = 0; i < 32; i++) {
        int idx = 32 + i;
        if (mask & 1u) {
            uint8_t val = src[idx];
            dst1[idx] = (uint8_t)(val + 1);
            dst2[idx] = (uint8_t)(0x1f - val);
        }
        mask >>= 1;
    }

    for (int block = 0; block < 6; block++) {
        memcpy(&mask, &masks[2 + block], sizeof(mask));
        int base = 64 + block * 32;
        for (int i = 0; i < 32; i++) {
            int idx = base + i;
            if (mask & 1u) {
                uint8_t val = src[idx];
                dst1[idx] = (uint8_t)(val + 1);
                dst2[idx] = (uint8_t)(0x1f - val);
            }
            mask >>= 1;
        }
    }
}



static uint8_t reduce_sample(uint32_t value)
{
    return value > 0x7ffu ? 0x3fu : (uint8_t)(value >> 5);
}

void gpu2d_planes_blend_weighted_rgb(void *param_1, const void *param_2,
                         const void *param_3, const void *param_4)
{

    uint8_t *output = (uint8_t *)param_1;
    const uint8_t *coefs = (const uint8_t *)param_2;
    const uint8_t *first_row = (const uint8_t *)param_3;
    const uint8_t *second = (const uint8_t *)param_4;
    uintptr_t start_output = (uintptr_t)param_1;
    int overlap =
        (((uintptr_t)param_3 + 0x100u > start_output) &&
         (start_output + 0x300u > (uintptr_t)param_3)) ||
        (((uintptr_t)param_4 + 0x100u > start_output) &&
         (start_output + 0x300u > (uintptr_t)param_4)) ||
        (((uintptr_t)param_2 + 0x600u > start_output) &&
         (start_output + 0x300u > (uintptr_t)param_2));

    if (overlap) {
        for (uint32_t idx = 0; idx != 0x100u; ++idx) {
            uint32_t b = rd8(second + idx);
            uint32_t c3 = rd8(coefs + 0x300u + idx);
            uint32_t a = rd8(first_row + idx);
            uint32_t c0 = rd8(coefs + idx);
            uint32_t c4 = rd8(coefs + 0x400u + idx);
            uint32_t c5 = rd8(coefs + 0x500u + idx);
            uint32_t c1 = rd8(coefs + 0x100u + idx);
            uint32_t c2 = rd8(coefs + 0x200u + idx);
            uint8_t result0 = reduce_sample(c0 * a + c3 * b + 0x10u);
            uint8_t result1 = reduce_sample(c1 * a + c4 * b + 0x10u);
            uint8_t result2 = reduce_sample(c2 * a + c5 * b + 0x10u);

            wr8(output + idx, result0);
            wr8(output + 0x100u + idx, result1);
            wr8(output + 0x200u + idx, result2);
        }
    } else {
        for (uint32_t idx = 0; idx != 0x100u; ++idx) {
            uint32_t a = rd8(first_row + idx);
            uint32_t b = rd8(second + idx);
            uint8_t result0 = reduce_sample(
                rd8(coefs + idx) * a +
                rd8(coefs + 0x300u + idx) * b + 0x10u);
            uint8_t result1 = reduce_sample(
                rd8(coefs + 0x100u + idx) * a +
                rd8(coefs + 0x400u + idx) * b + 0x10u);
            uint8_t result2 = reduce_sample(
                rd8(coefs + 0x200u + idx) * a +
                rd8(coefs + 0x500u + idx) * b + 0x10u);

            wr8(output + idx, result0);
            wr8(output + 0x100u + idx, result1);
            wr8(output + 0x200u + idx, result2);
        }
    }
}



static void blend_scalar(uint8_t *output, const uint8_t *entry,
                           const uint8_t *factor_1,
                           const uint8_t *factor_2,
                           const uint8_t *factor_3)
{
    uint32_t i;

    for (i = 0; i != 0x100u; ++i) {
        uint8_t f3 = rd8(factor_3 + i);
        uint8_t f1 = rd8(factor_1 + i);
        uint8_t e0 = rd8(entry + i);
        uint8_t f2 = rd8(factor_2 + i);
        uint8_t e3 = rd8(entry + i + 0x300u);
        uint8_t e1 = rd8(entry + i + 0x100u);
        uint32_t base = (uint32_t)f3 * 63u + 16u;
        uint8_t e2 = rd8(entry + i + 0x200u);
        uint8_t e4 = rd8(entry + i + 0x400u);
        uint8_t e5 = rd8(entry + i + 0x500u);
        uint32_t r0 = base + (uint32_t)e0 * f1 + (uint32_t)e3 * f2;
        uint32_t r1 = base + (uint32_t)e1 * f1 + (uint32_t)e4 * f2;
        uint32_t r2 = base + (uint32_t)e2 * f1 + (uint32_t)e5 * f2;

        wr8(output + i, (uint8_t)(r0 > 0x7ffu ? 63u : r0 >> 5));
        wr8(output + i + 0x100u, (uint8_t)(r1 > 0x7ffu ? 63u : r1 >> 5));
        wr8(output + i + 0x200u, (uint8_t)(r2 > 0x7ffu ? 63u : r2 >> 5));
    }
}

void gpu2d_planes_blend_six_to_three(void *param_1, const void *param_2,
                        const void *param_3, const void *param_4,
                        const void *param_5)
{
    uint8_t *output = (uint8_t *)param_1;
    const uint8_t *entry = (const uint8_t *)param_2;
    const uint8_t *factor_1 = (const uint8_t *)param_3;
    const uint8_t *factor_2 = (const uint8_t *)param_4;
    const uint8_t *factor_3 = (const uint8_t *)param_5;
    uintptr_t dest = (uintptr_t)output;
    uintptr_t end_dest = dest + 0x300u;
    int overlap;

    overlap = (((uintptr_t)factor_1 + 0x100u > dest &&
               end_dest > (uintptr_t)factor_1) ||
              ((uintptr_t)factor_2 + 0x100u > dest &&
               end_dest > (uintptr_t)factor_2) ||
              ((uintptr_t)factor_3 + 0x100u > dest &&
               end_dest > (uintptr_t)factor_3) ||
              ((uintptr_t)entry + 0x600u > dest &&
               end_dest > (uintptr_t)entry));

    if (overlap) {
        blend_scalar(output, entry, factor_1, factor_2, factor_3);
    } else {
        blend_scalar(output, entry, factor_1, factor_2, factor_3);
    }
}

void gpu2d_window_x_mask_build(uint32_t *param_1, uint32_t param_2)
{

    uint32_t hi    = param_2 >> 8;
    uint32_t loRaw = param_2 & 0xffu;
    uint32_t loAdj = (loRaw != 0) ? loRaw : 0x100u;

    if ((hi | loRaw) == 0 || hi == loAdj) {
        uint32_t z = 0;
        for (int w = 0; w < 8; w++) {
            memcpy(&param_1[w], &z, sizeof(z));
        }
        return;
    }

    uint32_t lo  = (loAdj < hi) ? loAdj : hi;
    uint32_t top = (loAdj < hi) ? hi : loAdj;

    uint32_t wStart = lo >> 5;
    uint32_t wEnd   = (top - 1u) >> 5;
    uint32_t maskLo = 0xffffffffu << (lo & 31u);
    uint32_t maskHi = 0xfffffffeu << ((top - 1u) & 31u);

    if (hi > loAdj) {

        uint32_t notMaskLo = ~maskLo;
        uint32_t f = 0xffffffffu;
        for (int w = 0; w < 8; w++) {
            memcpy(&param_1[w], &f, sizeof(f));
        }

        uint32_t v;
        if (wStart == wEnd) {
            memcpy(&v, &param_1[wStart], sizeof(v));
            v &= (maskHi | notMaskLo);
            memcpy(&param_1[wStart], &v, sizeof(v));
            return;
        }

        memcpy(&v, &param_1[wStart], sizeof(v));
        v &= notMaskLo;
        memcpy(&param_1[wStart], &v, sizeof(v));

        for (uint32_t w = wStart + 1u; w < wEnd; w++) {
            uint32_t z = 0;
            memcpy(&param_1[w], &z, sizeof(z));
        }

        memcpy(&v, &param_1[wEnd], sizeof(v));
        v &= maskHi;
        memcpy(&param_1[wEnd], &v, sizeof(v));
        return;
    } else {

        uint32_t notMaskHi = ~maskHi;
        uint32_t z0 = 0;
        for (int w = 0; w < 8; w++) {
            memcpy(&param_1[w], &z0, sizeof(z0));
        }

        uint32_t v;
        if (wStart == wEnd) {
            memcpy(&v, &param_1[wStart], sizeof(v));
            v |= (maskLo & notMaskHi);
            memcpy(&param_1[wStart], &v, sizeof(v));
            return;
        }

        memcpy(&v, &param_1[wStart], sizeof(v));
        v |= maskLo;
        memcpy(&param_1[wStart], &v, sizeof(v));

        for (uint32_t w = wStart + 1u; w < wEnd; w++) {
            uint32_t f = 0xffffffffu;
            memcpy(&param_1[w], &f, sizeof(f));
        }

        memcpy(&v, &param_1[wEnd], sizeof(v));
        v |= notMaskHi;
        memcpy(&param_1[wEnd], &v, sizeof(v));
        return;
    }
}



static void body_or(unsigned char *q, const unsigned char *A)
{
    uint32_t t, u, v;
    t = rd32(A +  0); u = rd32(q - 16); v = rd32(q - 12);
    wr32(q - 16, u | t);
    t = rd32(A +  4); wr32(q - 12, v | t);
    t = rd32(A +  8); u = rd32(q -  8); v = rd32(q -  4);
    wr32(q -  8, u | t);
    t = rd32(A + 12); wr32(q -  4, v | t);
    t = rd32(A + 16); u = rd32(q +  0); v = rd32(q +  4);
    wr32(q +  0, u | t);
    t = rd32(A + 20); wr32(q +  4, v | t);
    t = rd32(A + 24); u = rd32(q +  8); v = rd32(q + 12);
    wr32(q +  8, u | t);
    t = rd32(A + 28); wr32(q + 12, v | t);
}

static void body_bic(unsigned char *q, const unsigned char *A,
                       const unsigned char *B)
{
    uint32_t t, u, v;
    t = rd32(B +  0) & ~rd32(A +  0);
    u = rd32(q - 16); v = rd32(q - 12);
    wr32(q - 16, u | t);
    t = rd32(B +  4) & ~rd32(A +  4); wr32(q - 12, v | t);
    t = rd32(B +  8) & ~rd32(A +  8);
    u = rd32(q -  8); v = rd32(q -  4);
    wr32(q -  8, u | t);
    t = rd32(B + 12) & ~rd32(A + 12); wr32(q -  4, v | t);
    t = rd32(B + 16) & ~rd32(A + 16);
    u = rd32(q +  0); v = rd32(q +  4);
    wr32(q +  0, u | t);
    t = rd32(B + 20) & ~rd32(A + 20); wr32(q +  4, v | t);
    t = rd32(B + 24) & ~rd32(A + 24);
    u = rd32(q +  8); v = rd32(q + 12);
    wr32(q +  8, u | t);
    t = rd32(B + 28) & ~rd32(A + 28); wr32(q + 12, v | t);
}

static void body_and(unsigned char *q, const unsigned char *C,
                       const uint32_t *nab)
{
    uint32_t t, u, v;
    t = rd32(C +  0); u = rd32(q - 16); v = rd32(q - 12);
    wr32(q - 16, u | (t & nab[0]));
    t = rd32(C +  4); wr32(q - 12, v | (t & nab[1]));
    t = rd32(C +  8); u = rd32(q -  8); v = rd32(q -  4);
    wr32(q -  8, u | (t & nab[2]));
    t = rd32(C + 12); wr32(q -  4, v | (t & nab[3]));
    t = rd32(C + 16); u = rd32(q +  0); v = rd32(q +  4);
    wr32(q +  0, u | (t & nab[4]));
    t = rd32(C + 20); wr32(q +  4, v | (t & nab[5]));
    t = rd32(C + 24); u = rd32(q +  8); v = rd32(q + 12);
    wr32(q +  8, u | (t & nab[6]));
    t = rd32(C + 28); wr32(q + 12, v | (t & nab[7]));
}

static void body_orn(unsigned char *q, const unsigned char *C,
                       const uint32_t *ab)
{
    uint32_t t, u, v;
    t = rd32(C +  0); u = rd32(q - 16); v = rd32(q - 12);
    wr32(q - 16, u | ~(ab[0] | t));
    t = rd32(C +  4); wr32(q - 12, v | ~(ab[1] | t));
    t = rd32(C +  8); u = rd32(q -  8); v = rd32(q -  4);
    wr32(q -  8, u | ~(ab[2] | t));
    t = rd32(C + 12); wr32(q -  4, v | ~(ab[3] | t));
    t = rd32(C + 16); u = rd32(q +  0); v = rd32(q +  4);
    wr32(q +  0, u | ~(ab[4] | t));
    t = rd32(C + 20); wr32(q +  4, v | ~(ab[5] | t));
    t = rd32(C + 24); u = rd32(q +  8); v = rd32(q + 12);
    wr32(q +  8, u | ~(ab[6] | t));
    t = rd32(C + 28); wr32(q + 12, v | ~(ab[7] | t));
}

void gpu2d_window_regions_apply_four(void *param_1, void *param_2, uint32_t param_3,
                        void *param_4, void *param_5, void *param_6,
                        uint32_t param_7, uint32_t param_8,
                        uint32_t param_9, uint32_t param_10)
{

    unsigned char       *layers = (unsigned char *)param_1;
    unsigned char       *dst   = (unsigned char *)param_2;
    const unsigned char *A     = (const unsigned char *)param_4;
    const unsigned char *B     = (const unsigned char *)param_5;
    const unsigned char *C     = (const unsigned char *)param_6;

    uint32_t a0 = rd32(A +  0), a1 = rd32(A +  4);
    uint32_t b0 = rd32(B +  0), b1 = rd32(B +  4);
    uint32_t a2 = rd32(A +  8), a3 = rd32(A + 12);
    uint32_t b2 = rd32(B +  8), b3 = rd32(B + 12);
    uint32_t a4 = rd32(A + 16), a5 = rd32(A + 20);
    uint32_t b4 = rd32(B + 16), b5 = rd32(B + 20);
    uint32_t a6 = rd32(A + 24), a7 = rd32(A + 28);
    uint32_t b6 = rd32(B + 24), b7 = rd32(B + 28);

    uint32_t t, u, v;
    uint32_t m = param_7 & param_3;

    if (param_7 & 0x20u) {
        u = rd32(dst +  0); v = rd32(dst +  4);
        wr32(dst +  0, u | a0);
        t = rd32(A +  4); wr32(dst +  4, v | t);
        t = rd32(A +  8);
        u = rd32(dst +  8); v = rd32(dst + 12);
        wr32(dst +  8, u | t);
        t = rd32(A + 12); wr32(dst + 12, v | t);
        t = rd32(A + 16);
        u = rd32(dst + 16); v = rd32(dst + 20);
        wr32(dst + 16, u | t);
        t = rd32(A + 20); wr32(dst + 20, v | t);
        t = rd32(A + 24);
        u = rd32(dst + 24); v = rd32(dst + 28);
        wr32(dst + 24, u | t);
        t = rd32(A + 28); wr32(dst + 28, v | t);
    }

    if (m != 0) {
        unsigned char *q = layers + 0x10;
        while (m != 0) {
            if (m & 1u) body_or(q, A);
            m >>= 1;
            q += 0x20;
        }
    }

    uint32_t inobj = param_9;
    m = param_8 & param_3;

    if (param_8 & 0x20u) {
        t = rd32(B +  0) & ~rd32(A +  0);
        u = rd32(dst +  0); v = rd32(dst +  4);
        wr32(dst +  0, u | t);
        t = rd32(B +  4) & ~rd32(A +  4);
        wr32(dst +  4, v | t);
        t = rd32(B +  8) & ~rd32(A +  8);
        u = rd32(dst +  8); v = rd32(dst + 12);
        wr32(dst +  8, u | t);
        t = rd32(B + 12) & ~rd32(A + 12);
        wr32(dst + 12, v | t);
        t = rd32(B + 16) & ~rd32(A + 16);
        u = rd32(dst + 16); v = rd32(dst + 20);
        wr32(dst + 16, u | t);
        t = rd32(B + 20) & ~rd32(A + 20);
        wr32(dst + 20, v | t);
        t = rd32(B + 24) & ~rd32(A + 24);
        u = rd32(dst + 24); v = rd32(dst + 28);
        wr32(dst + 24, u | t);
        t = rd32(B + 28) & ~rd32(A + 28);
        wr32(dst + 28, v | t);
    }

    if (m != 0) {
        unsigned char *q = layers + 0x10;
        while (m != 0) {
            if (m & 1u) body_bic(q, A, B);
            m >>= 1;
            q += 0x20;
        }
    }

    uint32_t outside = param_10;
    uint32_t ab[8];
    ab[0] = b0 | a0;  ab[1] = b1 | a1;
    ab[2] = b2 | a2;  ab[3] = b3 | a3;
    ab[4] = b4 | a4;  ab[5] = b5 | a5;
    ab[6] = b6 | a6;  ab[7] = b7 | a7;

    m = inobj & param_3;

    if (inobj & 0x20u) {
        t = rd32(C +  0);
        u = rd32(dst +  0); v = rd32(dst +  4);
        wr32(dst +  0, u | (t & ~ab[0]));
        t = rd32(C +  4); wr32(dst +  4, v | (t & ~ab[1]));
        t = rd32(C +  8);
        u = rd32(dst +  8); v = rd32(dst + 12);
        wr32(dst +  8, u | (t & ~ab[2]));
        t = rd32(C + 12); wr32(dst + 12, v | (t & ~ab[3]));
        t = rd32(C + 16);
        u = rd32(dst + 16); v = rd32(dst + 20);
        wr32(dst + 16, u | (t & ~ab[4]));
        t = rd32(C + 20); wr32(dst + 20, v | (t & ~ab[5]));
        t = rd32(C + 24);
        u = rd32(dst + 24); v = rd32(dst + 28);
        wr32(dst + 24, u | (t & ~ab[6]));
        t = rd32(C + 28); wr32(dst + 28, v | (t & ~ab[7]));
    }

    if (m != 0) {
        uint32_t nab[8];
        for (int i = 0; i < 8; i++) nab[i] = ~ab[i];
        unsigned char *q = layers + 0x10;
        while (m != 0) {
            if (m & 1u) body_and(q, C, nab);
            m >>= 1;
            q += 0x20;
        }
    }

    m = outside & param_3;

    if (outside & 0x20u) {
        t = rd32(C +  0);
        u = rd32(dst +  0); v = rd32(dst +  4);
        wr32(dst +  0, u | ~(ab[0] | t));
        t = rd32(C +  4); wr32(dst +  4, v | ~(ab[1] | t));
        t = rd32(C +  8);
        u = rd32(dst +  8); v = rd32(dst + 12);
        wr32(dst +  8, u | ~(ab[2] | t));
        t = rd32(C + 12); wr32(dst + 12, v | ~(ab[3] | t));
        t = rd32(C + 16);
        u = rd32(dst + 16); v = rd32(dst + 20);
        wr32(dst + 16, u | ~(ab[4] | t));
        t = rd32(C + 20); wr32(dst + 20, v | ~(ab[5] | t));
        t = rd32(C + 24);
        u = rd32(dst + 24); v = rd32(dst + 28);
        wr32(dst + 24, u | ~(ab[6] | t));
        t = rd32(C + 28); wr32(dst + 28, v | ~(ab[7] | t));
    }

    if (m != 0) {
        unsigned char *q = layers + 0x10;
        while (m != 0) {
            if (m & 1u) body_orn(q, C, ab);
            m >>= 1;
            q += 0x20;
        }
    }
}

void gpu2d_planes_pack_three_to_word(const unsigned char *planes, unsigned char *output) {
    for (uint64_t i = 0; i < 0x100; i++) {
        uint32_t a = planes[i];
        uint32_t b = planes[i + 256];
        uint32_t c = planes[i + 512];
        uint32_t v = a << 2;
        v = (v & ~(0xffu << 10)) | ((b & 0xffu) << 10);
        v = (v & ~(0xffu << 18)) | ((c & 0xffu) << 18);
        memcpy(output + i * 4, &v, 4);
    }
}

void gpu2d_planes_pack_dual_triple_to_words(const unsigned char *a, const unsigned char *b,
                        unsigned char *output) {
    const unsigned char *bias = b + 0x100;
    for (uint64_t i = 0; i < 0x100; i++) {
        const unsigned char *pa = a + i;
        uint32_t a0 = pa[0], a1 = pa[256], a2 = pa[512];
        uint32_t v = a0 << 2;
        v = (v & ~0x0003fc00u) | ((a1 & 0xffu) << 10);
        v = (v & ~0x03fc0000u) | ((a2 & 0xffu) << 18);
        const unsigned char *pb = bias + i;
        memcpy(output, &v, 4);
        uint32_t b0 = pb[-256], b1 = pb[0], b2 = pb[256];
        uint32_t w = b0 << 2;
        w = (w & ~0x0003fc00u) | ((b1 & 0xffu) << 10);
        w = (w & ~0x03fc0000u) | ((b2 & 0xffu) << 18);
        memcpy(output + 4, &w, 4);
        output += 8;
    }
}







static uint16_t pack_rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    uint16_t v;

    v = (uint16_t)(((uint32_t)red << 10) & 0xf800u);
    v = (uint16_t)(v | ((uint32_t)green << 5));
    v = (uint16_t)(v | ((uint32_t)blue >> 1));
    return v;
}

void gpu2d_planes_pack_dual_rgb565(const uint8_t *param_1, const uint8_t *param_2,
                        uint8_t *param_3)
{
    uintptr_t a0 = (uintptr_t)param_1;
    uintptr_t a1 = (uintptr_t)param_2;
    uintptr_t d = (uintptr_t)param_3;
    unsigned int i;

    if ((((a0 + 0x300u) > d) && ((d + 0x400u) > a0)) ||
        (((a1 + 0x300u) > d) && ((d + 0x400u) > a1))) {
        for (i = 0; i != 0x100u; ++i) {
            uint8_t r = rd8(param_1 + i);
            uint8_t g = rd8(param_1 + 0x100u + i);
            uint8_t b = rd8(param_1 + 0x200u + i);

            wr16(param_3, pack_rgb565(r, g, b));

            r = rd8(param_2 + i);
            g = rd8(param_2 + 0x100u + i);
            b = rd8(param_2 + 0x200u + i);
            wr16(param_3 + 2, pack_rgb565(r, g, b));
            param_3 += 4;
        }
    } else {
        for (i = 0; i != 0x100u; ++i) {
            uint8_t r = rd8(param_1 + i);
            uint8_t g = rd8(param_1 + 0x100u + i);
            uint8_t b = rd8(param_1 + 0x200u + i);
            uint16_t pixel0 = pack_rgb565(r, g, b);

            r = rd8(param_2 + i);
            g = rd8(param_2 + 0x100u + i);
            b = rd8(param_2 + 0x200u + i);
            wr16(param_3, pixel0);
            wr16(param_3 + 2, pack_rgb565(r, g, b));
            param_3 += 4;
        }
    }
}



static uint16_t convert_pixel(uint8_t red, uint8_t green, uint8_t blue,
                                uint32_t multiplier, uint32_t addend)
{
    uint32_t r = (uint32_t)red * multiplier + addend;
    uint32_t g = (uint32_t)green * multiplier + addend;
    uint32_t b = (uint32_t)blue * multiplier + addend;

    return (uint16_t)(((r << 5) & 0xf800u) |
                      (g & 0xffe0u) |
                      (b >> 6));
}

void gpu2d_planes_pack_rgb_planes_to_rgb565(uint8_t *source, uint8_t *dest,
                         uint32_t multiplier, uint32_t addend)
{

    uintptr_t start_source = (uintptr_t)source;
    uintptr_t start_dest = (uintptr_t)dest;

    if (start_source + 0x300u <= start_dest ||
        start_dest + 0x200u <= start_source) {

        for (uint32_t output = 0; output != 0x200u; output += 16u) {
            uint32_t i = output >> 1;
            uint8_t reds[8], greens[8], blues[8];
            uint16_t pixels[8];

            memcpy(reds, source + i, 8);
            memcpy(greens, source + i + 0x100u, 8);
            memcpy(blues, source + i + 0x200u, 8);
            for (uint32_t j = 0; j != 8u; ++j)
                pixels[j] = convert_pixel(reds[j], greens[j], blues[j],
                                             multiplier, addend);
            memcpy(dest + output, pixels, 16);
        }
    } else {

        for (uint32_t i = 0; i != 0x100u; ++i) {
            uint8_t red = rd8(source + i);
            uint8_t green = rd8(source + i + 0x100u);
            uint8_t blue = rd8(source + i + 0x200u);

            wr16(dest + i * 2u,
                  convert_pixel(red, green, blue, multiplier, addend));
        }
    }
}



static uint32_t madd32(uint8_t sample, uint32_t multiplier, uint32_t sum)
{
    return (uint32_t)sample * multiplier + sum;
}

static uint16_t convert_pixel_22(const uint8_t *origin, uint32_t idx,
                                uint32_t multiplier, uint32_t sum)
{
    uint32_t red = madd32(rd8(origin + idx), multiplier, sum);
    uint32_t green = madd32(rd8(origin + idx + 0x100u), multiplier, sum);
    uint32_t blue = madd32(rd8(origin + idx + 0x200u), multiplier, sum);

    return (uint16_t)(((red << 5) & UINT32_C(0xf800)) |
                      (green & UINT32_C(0xffe0)) | (blue >> 6));
}

void gpu2d_planes_pack_dual_rgb_to_rgb565(uint8_t *param_1, uint8_t *param_2,
                         uint8_t *param_3, uint32_t param_4, uint32_t param_5)
{

    uintptr_t origin_1 = (uintptr_t)param_1;
    uintptr_t origin_2 = (uintptr_t)param_2;
    uintptr_t dest = (uintptr_t)param_3;
    uintptr_t end_dest = dest + 0x400u;
    uint32_t idx;

    if (((origin_1 + 0x300u > dest) && (end_dest > origin_1)) ||
        ((origin_2 + 0x300u > dest) && (end_dest > origin_2))) {
        for (idx = 0; idx != 0x100u; ++idx) {
            uint16_t first = convert_pixel_22(param_1, idx, param_4, param_5);
            wr16(param_3, first);

            uint16_t second = convert_pixel_22(param_2, idx, param_4, param_5);
            wr16(param_3 + 2, second);
            param_3 += 4;
        }
    } else {
        for (idx = 0; idx != 0x100u; idx += 8) {
            uint32_t lane;

            for (lane = 0; lane != 8; ++lane) {
                uint16_t first = convert_pixel_22(param_1, idx + lane,
                                                   param_4, param_5);
                uint16_t second = convert_pixel_22(param_2, idx + lane,
                                                   param_4, param_5);
                wr16(param_3 + lane * 4, first);
                wr16(param_3 + lane * 4 + 2, second);
            }
            param_3 += 32;
        }
    }
}

#define COUNT 0x100
#define PLANE  0x100

void gpu2d_planes_pack_scaled_triple(const uint8_t *entry, uint32_t *output,
                         uint32_t mult, uint32_t sum) {

    for (uint64_t i = 0; i < COUNT; i++) {
        const uint8_t *p = entry + i;

        uint32_t p0 = (uint32_t)p[0]            * mult + sum;
        uint32_t p1 = (uint32_t)p[PLANE]         * mult + sum;
        uint32_t p2 = (uint32_t)p[2 * PLANE]     * mult + sum;

        p0 = (p0 >> 3)  & 0x1ffffffcu;
        p1 = (p1 << 5)  & 0xfffffc00u;
        p2 = (p2 << 13) & 0xfffc0000u;

        output[i] = p1 | p0 | p2;
    }
}
#undef COUNT
#undef PLANE

#define COUNT 0x100
#define PLANE  0x100

void gpu2d_planes_pack_scaled_triple_dual(const uint8_t *entry_a, const uint8_t *entry_b,
                         uint32_t *output, uint32_t mult, uint32_t sum) {

    const uint8_t *base_b = entry_b + PLANE;

    for (uint64_t i = 0; i < COUNT; i++) {

        const uint8_t *pa = entry_a + i;

        uint32_t a0 = (uint32_t)pa[0]         * mult + sum;
        uint32_t a1 = (uint32_t)pa[PLANE]     * mult + sum;
        uint32_t a2 = (uint32_t)pa[2 * PLANE] * mult + sum;

        a0 = (a0 >> 3)  & 0x1ffffffcu;
        a1 = (a1 << 5)  & 0xfffffc00u;
        a2 = (a2 << 13) & 0xfffc0000u;

        output[0] = (a1 | a0) | a2;

        const uint8_t *pb = base_b + i;

        uint32_t b0 = (uint32_t)pb[-PLANE]    * mult + sum;
        uint32_t b1 = (uint32_t)pb[0]         * mult + sum;
        uint32_t b2 = (uint32_t)pb[PLANE]     * mult + sum;

        b0 = (b0 >> 3)  & 0x1ffffffcu;
        b1 = (b1 << 5)  & 0xfffffc00u;
        b2 = (b2 << 13) & 0xfffc0000u;

        output[1] = (b1 | b0) | b2;

        output += 2;
    }
}
#undef COUNT
#undef PLANE



static void read_eight(const void *p, uint16_t values[8])
{
    memcpy(values, p, 16u);
}

static void write_eight(void *p, const uint16_t values[8])
{
    memcpy(p, values, 16u);
}

static uint16_t average_rgb555(uint16_t dest, uint16_t source)
{
    uint32_t red = ((uint32_t)(dest & UINT16_C(0x001f)) +
                     (uint32_t)(source & UINT16_C(0x001f))) >> 1;
    uint32_t green = ((((uint32_t)(dest >> 5) & UINT32_C(0x003f)) +
                       ((uint32_t)(source >> 5) & UINT32_C(0x003f))) >> 1) << 5;
    uint32_t blue = (((uint32_t)(dest >> 11) +
                      (uint32_t)(source >> 11)) >> 1) << 11;

    return (uint16_t)(red | green | blue);
}

void gpu2d_planes_average_pixels16(unsigned char *dest, const unsigned char *source,
                        uint32_t count)
{

    uint32_t i;

    if (count == 0u)
        return;

    if (count >= 8u) {
        size_t bytes = (size_t)count << 1;
        uintptr_t end_source = (uintptr_t)source + bytes;
        uintptr_t end_dest = (uintptr_t)dest + bytes;

        if (end_source <= (uintptr_t)dest ||
            end_dest <= (uintptr_t)source) {
            uint32_t block = count & ~UINT32_C(7);

            for (i = 0u; i < block; i += 8u) {
                uint16_t values_dest[8];
                uint16_t values_source[8];
                uint16_t result[8];
                uint32_t j;
                size_t base = (size_t)i * 2u;

                read_eight(dest + base, values_dest);
                read_eight(source + base, values_source);
                for (j = 0u; j != 8u; ++j)
                    result[j] = average_rgb555(values_dest[j],
                                                   values_source[j]);
                write_eight(dest + base, result);
            }

            if (block == count)
                return;

            i = block;
        } else {
            i = 0u;
        }
    } else {
        i = 0u;
    }

    for (; i < count; ++i) {
        size_t base = (size_t)i * 2u;
        uint16_t value_dest = rd16(dest + base);
        uint16_t value_source = rd16(source + base);

        wr16(dest + base, average_rgb555(value_dest, value_source));
    }
}



static uint32_t average_word(uint32_t dest, uint32_t source)
{
    return ((dest >> 1) + (source >> 1)) & UINT32_C(0xfefefeff);
}

static void average_eight(uint8_t *dest, const uint8_t *source)
{
    uint32_t words_source[8];
    uint32_t words_dest[8];
    uint32_t result[8];
    uint32_t i;

    memcpy(words_source, source, sizeof(words_source));
    memcpy(words_dest, dest, sizeof(words_dest));
    for (i = 0; i != 8; ++i)
        result[i] = average_word(words_dest[i], words_source[i]);
    memcpy(dest, result, sizeof(result));
}

void gpu2d_planes_average_words32(uint8_t *dest, const uint8_t *source,
                        uint32_t count)
{

    uint32_t idx = 0;

    if (count == 0)
        return;

    if (count >= 8) {
        size_t bytes = (size_t)count << 2;
        uintptr_t end_source = (uintptr_t)source + bytes;

        if (end_source <= (uintptr_t)dest ||
            (uintptr_t)dest + bytes <= (uintptr_t)source) {
            uint32_t block = count & ~UINT32_C(7);

            while (idx != block) {
                average_eight(dest + (size_t)idx * 4,
                              source + (size_t)idx * 4);
                idx += 8;
            }
            if (block == count)
                return;
        }
    }

    while (idx != count) {
        uint32_t value_dest = rd32(dest + (size_t)idx * 4);
        uint32_t value_source = rd32(source + (size_t)idx * 4);

        wr32(dest + (size_t)idx * 4,
              average_word(value_dest, value_source));
        ++idx;
    }
}

#define WIDTH 256

void gpu2d_planes_mask_from_bytes(uint8_t *mask, const uint8_t *pixels) {

    for (int b = 0; b < WIDTH / 8; b++) {
        uint8_t byte = 0;
        for (int k = 0; k < 8; k++)
            if (pixels[b * 8 + k]) byte |= (uint8_t)(1u << k);
        mask[b] = byte;
    }
}
#undef WIDTH

#define WIDTH 256

void gpu2d_planes_mask_from_pixels_lowbyte(uint8_t *mask, const uint16_t *pixels) {

    for (int b = 0; b < WIDTH / 8; b++) {
        uint8_t byte = 0;
        for (int k = 0; k < 8; k++) {

            if (pixels[b * 8 + k] & 0xff) byte |= (uint8_t)(1u << k);
        }
        mask[b] = byte;
    }
}
#undef WIDTH

#define PAL 4
#define LAYERS 5

void gpu2d_planes_combine_six_masks(uint8_t *output, const uint8_t *masks,
                        int flags, int modes) {

    unsigned b = (unsigned)flags & 0xff;
    unsigned m = (unsigned)modes    & 0xff;

    uint64_t total[PAL] = {0, 0, 0, 0};

    for (int k = 0; k < LAYERS; k++) {
        uint64_t msk[PAL];
        for (int i = 0; i < PAL; i++)
            __builtin_memcpy(&msk[i], masks + k * 32 + i * 8, 8);

        int participates = (b >> k) & 1;
        int sum      = (m >> k) & 1;
        if (!participates) continue;

        for (int i = 0; i < PAL; i++) {
            if (sum) total[i] |=  msk[i];
            else      total[i] &= ~msk[i];
        }
    }

    {
        uint64_t msk[PAL];
        for (int i = 0; i < PAL; i++)
            __builtin_memcpy(&msk[i], masks + LAYERS * 32 + i * 8, 8);
        int sum = (m >> LAYERS) & 1;
        for (int i = 0; i < PAL; i++) {
            if (sum) total[i] |=  msk[i];
            else      total[i] &= ~msk[i];
        }
    }

    for (int i = 0; i < PAL; i++)
        __builtin_memcpy(output + i * 8, &total[i], 8);
}

const unsigned char gpu2d_lane_bit_masks[16] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};
#undef PAL
#undef LAYERS

#define WIDTH 256
#define LIMIT  32

static unsigned umin(unsigned x, unsigned y) { return x < y ? x : y; }

void gpu2d_planes_expand_mask_to_params(int arg, uint8_t *table_a, uint8_t *table_b,
                        const uint16_t *bits) {

    unsigned p = (unsigned)arg + (unsigned)arg;
    unsigned a = umin(p & 0x3f, LIMIT);
    unsigned b = umin((p >> 8) & 0x3f, LIMIT);

    for (int i = 0; i < WIDTH; i++) {
        int is_set = (bits[i >> 4] >> (i & 15)) & 1;
        table_b[i] = (uint8_t)(is_set ? b : 0);
        table_a[i] = (uint8_t)(is_set ? a : LIMIT);
    }
}
#undef WIDTH
#undef LIMIT

#define WIDTH 256
#define LIMIT  32

static unsigned recon_umin_u(unsigned x, unsigned y) { return x < y ? x : y; }

void gpu2d_blend_weights_from_bldalpha_masked(int param_1, uint8_t *param_2, uint8_t *param_3,
                        const uint16_t *param_4) {

    unsigned p = (unsigned)param_1 + (unsigned)param_1;
    unsigned a = recon_umin_u(p & 0x3f, LIMIT);
    unsigned b = recon_umin_u((p >> 8) & 0x3f, LIMIT);

    for (int i = 0; i < WIDTH; i++) {
        int is_set = (param_4[i >> 4] >> (i & 15)) & 1;
        if (is_set) {
            param_2[i] = (uint8_t)a;
            param_3[i] = (uint8_t)b;
        }

    }
}
#undef WIDTH
#undef LIMIT

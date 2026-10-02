#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <stddef.h>
#include "core_internals.h"
#include "mem_access.h"










static uint32_t scale_s15(int16_t sample, int32_t gain)
{
    uint64_t product = (uint64_t)((int64_t)sample * (int64_t)gain);
    return (uint32_t)(product >> 15);
}

void gpu2d_palette_mix_gain_bias_samples(uint8_t *dest, uint8_t *state,
                        const uint8_t *origin, uint32_t channels)
{

    if (channels == 0)
        return;

    uint8_t *counts = state + 0x630;
    uint8_t *biases = state;
    uint8_t *gains = state + 0xb0;

    for (uint32_t channel = 0; channel != channels; channel++) {
        uint16_t count = rd16(counts);
        uint32_t bias = rd32(biases);
        uint8_t *next_gain = gains + 4;

        counts += 4;
        biases += 4;

        if (count != 0) {
            int32_t gain = rd32s(gains);
            uint32_t vectorized = (uint32_t)count & ~UINT32_C(7);

            if (vectorized != 0) {
                for (uint32_t done = 0; done != vectorized; done += 8) {
                    int16_t m0 = rd16s(origin + 0);
                    int16_t m1 = rd16s(origin + 2);
                    int16_t m2 = rd16s(origin + 4);
                    int16_t m3 = rd16s(origin + 6);
                    int16_t m4 = rd16s(origin + 8);
                    int16_t m5 = rd16s(origin + 10);
                    int16_t m6 = rd16s(origin + 12);
                    int16_t m7 = rd16s(origin + 14);

                    origin += 16;
                    wr32(dest + 0, bias + scale_s15(m0, gain));
                    wr32(dest + 4, bias + scale_s15(m1, gain));
                    wr32(dest + 8, bias + scale_s15(m2, gain));
                    wr32(dest + 12, bias + scale_s15(m3, gain));
                    wr32(dest + 16, bias + scale_s15(m4, gain));
                    wr32(dest + 20, bias + scale_s15(m5, gain));
                    wr32(dest + 24, bias + scale_s15(m6, gain));
                    wr32(dest + 28, bias + scale_s15(m7, gain));
                    dest += 32;
                }
            }

            for (uint32_t done = vectorized; done != count; done++) {
                int16_t sample = rd16s(origin);
                origin += 2;
                wr32(dest, bias + scale_s15(sample, gain));
                dest += 4;
            }
        }

        gains = next_gain;
    }
}




void gpu2d_palette_gen_phase_samples(void *param_1, const void *param_2,
                         uint32_t param_3, const void *param_4)
{

    uint8_t *output = (uint8_t *)param_1;
    const uint8_t *ctx = (const uint8_t *)param_2;
    const uint8_t *table = (const uint8_t *)param_4;

    if (param_3 == 0)
        return;

    for (uint32_t entry = 0; entry != param_3; entry++) {
        uint16_t len = rd16(ctx + 0x630u + (size_t)entry * 4u);
        int32_t factor = (int32_t)rd32(ctx + 0x210u + (size_t)entry * 4u);
        uint32_t idx = len;
        uint64_t increment = (uint64_t)(int64_t)factor *
                              (uint64_t)rd32(table + (size_t)idx * 4u);

        if (factor < 0)
            increment += UINT64_C(0x3fffffff);

        if (len != 0) {
            uint64_t phase = (uint64_t)rd32(ctx + 0x160u +
                                             (size_t)entry * 4u) << 30;
            uint32_t group = (uint32_t)len & ~UINT32_C(7);

            for (uint32_t i = 0; i != group; i++) {
                wr32(output, (uint32_t)(phase >> 30));
                output += 4;
                phase += increment;
            }

            for (uint32_t i = group; i != (uint32_t)len; i++) {
                wr32(output, (uint32_t)(phase >> 30));
                output += 4;
                phase += increment;
            }
        }
    }
}





void gpu2d_palette_compare_mark_range(uint8_t *dest, const uint8_t *entry_a,
                         const uint8_t *entry_b, uint32_t count32,
                         uint32_t *count)
{

    uint64_t amount = (uint64_t)count32;
    uint64_t done = 0;
    uint32_t total = 0;
    uintptr_t p_dest = (uintptr_t)dest;
    uintptr_t p_a = (uintptr_t)entry_a;
    uintptr_t p_b = (uintptr_t)entry_b;

    if (count32 == 0) {
        wr32(count, 0);
        return;
    }

    if (amount >= 8) {
        uint64_t bytes_entry = amount << 2;
        uintptr_t end_dest = p_dest + amount;
        uintptr_t end_a = p_a + bytes_entry;
        uintptr_t end_b = p_b + bytes_entry;
        uint32_t overlap_a = (end_a > p_dest) && (end_dest > p_a);
        uint32_t overlap_b = (end_b > p_dest) && (end_dest > p_b);

        if ((overlap_a | overlap_b) == 0) {
            done = amount & UINT64_C(0x1fffffff8);

            for (uint64_t pos = 0; pos < done; pos += 8) {

                uint32_t b0 = rd32((const void *)(p_b + (pos + 0) * 4));
                uint32_t b1 = rd32((const void *)(p_b + (pos + 1) * 4));
                uint32_t b2 = rd32((const void *)(p_b + (pos + 2) * 4));
                uint32_t b3 = rd32((const void *)(p_b + (pos + 3) * 4));
                uint32_t b4 = rd32((const void *)(p_b + (pos + 4) * 4));
                uint32_t b5 = rd32((const void *)(p_b + (pos + 5) * 4));
                uint32_t b6 = rd32((const void *)(p_b + (pos + 6) * 4));
                uint32_t b7 = rd32((const void *)(p_b + (pos + 7) * 4));
                uint32_t a0 = rd32((const void *)(p_a + (pos + 0) * 4));
                uint32_t a1 = rd32((const void *)(p_a + (pos + 1) * 4));
                uint32_t a2 = rd32((const void *)(p_a + (pos + 2) * 4));
                uint32_t a3 = rd32((const void *)(p_a + (pos + 3) * 4));
                uint32_t a4 = rd32((const void *)(p_a + (pos + 4) * 4));
                uint32_t a5 = rd32((const void *)(p_a + (pos + 5) * 4));
                uint32_t a6 = rd32((const void *)(p_a + (pos + 6) * 4));
                uint32_t a7 = rd32((const void *)(p_a + (pos + 7) * 4));
                uint32_t ok0 = (a0 - (b0 & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);
                uint32_t ok1 = (a1 - (b1 & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);
                uint32_t ok2 = (a2 - (b2 & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);
                uint32_t ok3 = (a3 - (b3 & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);
                uint32_t ok4 = (a4 - (b4 & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);
                uint32_t ok5 = (a5 - (b5 & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);
                uint32_t ok6 = (a6 - (b6 & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);
                uint32_t ok7 = (a7 - (b7 & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);
                uint64_t marks =
                    ((uint64_t)(uint8_t)(0u - ok0) <<  0) |
                    ((uint64_t)(uint8_t)(0u - ok1) <<  8) |
                    ((uint64_t)(uint8_t)(0u - ok2) << 16) |
                    ((uint64_t)(uint8_t)(0u - ok3) << 24) |
                    ((uint64_t)(uint8_t)(0u - ok4) << 32) |
                    ((uint64_t)(uint8_t)(0u - ok5) << 40) |
                    ((uint64_t)(uint8_t)(0u - ok6) << 48) |
                    ((uint64_t)(uint8_t)(0u - ok7) << 56);

                total += ok0 + ok1 + ok2 + ok3 + ok4 + ok5 + ok6 + ok7;
                wr64((void *)(p_dest + pos), marks);
            }
        }
    }

    while (done < amount) {
        uint32_t a = rd32((const void *)(p_a + done * 4));
        uint32_t b = rd32((const void *)(p_b + done * 4));
        uint32_t ok = (a - (b & UINT32_C(0x00ffffff)) + UINT32_C(0xff)) < UINT32_C(0x1ff);

        total += ok;
        wr8((void *)(p_dest + done), (uint8_t)(0u - ok));
        ++done;
    }

    wr32(count, total);
}




void gpu2d_palette_mark_window24(unsigned char *dest, uint32_t param_2,
                        const unsigned char *source, uint32_t amount,
                        uint32_t *result)
{

    if (amount == 0) {
        wr32(result, 0);
        return;
    }

    uint64_t n = amount;
    uint32_t limit = param_2 + 0xffu;
    uint32_t count = 0;
    uint64_t primary = 0;
    int scalar = 1;

    if (n >= 8) {
        uintptr_t end_source = (uintptr_t)source + n * 4;
        if (end_source <= (uintptr_t)dest) {
            scalar = 0;
        } else {
            uintptr_t end_dest = (uintptr_t)dest + n;
            if (end_dest <= (uintptr_t)source)
                scalar = 0;
        }
    }

    if (!scalar) {
        primary = n & UINT64_C(0x1fffffff8);
        for (uint64_t i = 0; i != primary; ++i) {
            uint32_t value = rd32(source + i * 4) & 0x00ffffffu;
            uint32_t hit = (limit - value) < 0x1ffu;
            wr8(dest + i, (uint8_t)-(uint32_t)hit);
            count += hit;
        }
    }

    for (uint64_t i = primary; i != n; ++i) {
        uint32_t value = rd32(source + i * 4) & 0x00ffffffu;
        uint32_t hit = (limit - value) < 0x1ffu;
        wr8(dest + i, (uint8_t)-(uint32_t)hit);
        count += hit;
    }

    wr32(result, count);
}




void gpu2d_palette_compare_words24_mask(uint8_t *dest, const uint8_t *origin_a,
                         const uint8_t *origin_b, uint32_t elements,
                         void *result)
{

    if (elements == 0) {
        wr32(result, 0);
        return;
    }

    uint64_t block = 0;
    uint32_t count = 0;

    if (elements >= 8) {
        uintptr_t end_dest = (uintptr_t)dest + elements;
        uintptr_t end_a = (uintptr_t)origin_a + (uint64_t)elements * 4u;
        uintptr_t end_b = (uintptr_t)origin_b + (uint64_t)elements * 4u;
        int overlap_a = end_a > (uintptr_t)dest &&
                       end_dest > (uintptr_t)origin_a;
        int overlap_b = end_b > (uintptr_t)dest &&
                       end_dest > (uintptr_t)origin_b;

        if (!overlap_a && !overlap_b) {
            block = (uint64_t)(elements & UINT32_C(0xfffffff8));

            for (uint64_t base = 0; base < block; base += 8) {
                uint32_t values_a[8];
                uint32_t values_b[8];
                uint8_t marks[8];

                for (uint32_t j = 0; j < 8; ++j) {
                    uint64_t pos = base + j;
                    values_b[j] = rd32(origin_b + pos * 4u);
                }
                for (uint32_t j = 0; j < 8; ++j) {
                    uint64_t pos = base + j;
                    values_a[j] = rd32(origin_a + pos * 4u);
                }
                for (uint32_t j = 0; j < 8; ++j) {
                    uint8_t mark = (values_a[j] <
                                     (values_b[j] & UINT32_C(0x00ffffff))) ?
                                    0xffu : 0u;
                    marks[j] = mark;
                    count += (mark != 0);
                }

                for (uint32_t j = 0; j < 8; ++j)
                    wr8(dest + base + j, marks[j]);
            }
        }
    }

    for (uint64_t pos = block; pos < elements; ++pos) {
        uint32_t a = rd32(origin_a + pos * 4u);
        uint32_t b = rd32(origin_b + pos * 4u);
        uint8_t mark = (a < (b & UINT32_C(0x00ffffff))) ? 0xffu : 0u;

        if (mark != 0)
            ++count;
        wr8(dest + pos, mark);
    }

    wr32(result, count);
}



void gpu2d_palette_threshold_words24(uint8_t *output, uint32_t limit,
                         const uint8_t *entry, uint32_t number,
                         uint32_t *counter_p)
{

    uint32_t counter = 0;
    uint32_t done = 0;

    if (number == 0) {
        wr32(counter_p, 0);
        return;
    }

    if (number >= 8 &&
        ((uintptr_t)entry + ((size_t)number << 2) <= (uintptr_t)output ||
         (uintptr_t)output + number <= (uintptr_t)entry)) {
        uint32_t width = number & 0xfffffff8U;

        while (done != width) {
            uint32_t values[8];
            uint32_t i;

            for (i = 0; i != 8; ++i)
                values[i] = rd32(entry + ((size_t)(done + i) << 2));

            for (i = 0; i != 8; ++i) {
                uint32_t larger = ((values[i] & 0x00ffffffU) > limit);
                counter += larger;
                output[done + i] = (uint8_t)(0U - larger);
            }
            done += 8;
        }
    }

    while (done != number) {
        uint32_t larger = ((rd32(entry + ((size_t)done << 2)) &
                           0x00ffffffU) > limit);
        counter += larger;
        output[done] = (uint8_t)(0U - larger);
        ++done;
    }

    wr32(counter_p, counter);
}

void gpu2d_palette_filter_accumulate(unsigned char *output, const unsigned char *entry,
                        uint32_t threshold, uint32_t n, unsigned char *sum) {
    int32_t acc = 0;
    if (n != 0) {
        do {
            uint32_t sel = entry[3];
            uint32_t old = *output;
            entry += 4;
            uint32_t v = (sel > threshold) ? old : 0u;
            n -= 1;
            acc -= (int32_t)(int8_t)(uint8_t)v;
            *output++ = (unsigned char)v;
        } while (n != 0);
    }
    memcpy(sum, &acc, 4);
}

void gpu2d_palette_filter_field_accumulate(unsigned char *param_1, unsigned char *param_2,
                         int32_t param_3, uint32_t param_4, int32_t *param_5) {

    int32_t acc = 0;
    for (int32_t i = 0; i < param_3; i++) {
        uint32_t entry;
        memcpy(&entry, param_2 + (size_t)i * 4, 4);

        uint32_t field = (entry >> 24) & 0x3f;

        int8_t updated = 0;
        if (field != param_4 && entry > 0x7fffffffu) {
            updated = (int8_t)param_1[i];
        }
        param_1[i] = (unsigned char)updated;
        acc -= updated;
    }

    *param_5 = acc;
}

void gpu2d_palette_clear_matching(unsigned char *dst, unsigned char *src_val,
                         unsigned char *src_flag, uint32_t count, uint32_t val)
{

    if (count == 0) return;

    for (;;) {
        uint32_t b = *src_val;
        if (b == val) {
            if (*src_flag != 0x1f) {
                *dst = 0;
            }
        }
        src_val++;
        dst++;
        count--;
        src_flag++;
        if (count == 0) return;
    }
}





void gpu2d_palette_expand_entries_dual(unsigned char *state, unsigned char *output,
                        uint32_t amount, uint32_t idx)
{

    if (amount == 0) {
        return;
    }

    unsigned char *table_count = state + 9u * RECON_3D_ROW_ARRAY;
    unsigned char *values = state + 4u * RECON_3D_ROW_ARRAY;
    unsigned char *controls = state + 5u * RECON_3D_ROW_ARRAY;
    unsigned char *fields = output + (uint32_t)(idx << 2);

    do {
        uint32_t repeats = rd16(table_count);
        int16_t value0 = rd16s(values);
        int16_t control0 = rd16s(controls);
        int16_t value1 = rd16s(values + 2);
        int16_t control1 = rd16s(controls + 2);

        uint32_t word0 = (uint32_t)(int32_t)value0 << 15;
        uint32_t word1 = (uint32_t)(int32_t)value1 << 15;
        if (control0 > 0) {
            word0 |= 0x400;
        }
        if (control1 > 0) {
            word1 |= 0x400;
        }

        table_count += 4;
        values += 4;
        controls += 4;

        {
            unsigned char *out = output;
            uint32_t done = 0;

            if (repeats >= 8) {
                uint32_t block = repeats & 0xfffffff8u;
                unsigned char *out_end = output + ((size_t)block << 3);
                unsigned char *fields_block = fields;

                do {
                    uint32_t i;
                    for (i = 0; i != 8; i++) {
                        wr32(out + ((size_t)i << 3), word0);
                        wr32(out + ((size_t)i << 3) + 4, word1);
                    }
                    for (i = 0; i != 8; i++) {
                        wr16(fields_block + ((size_t)i << 2),
                              (uint16_t)control0);
                        wr16(fields_block + ((size_t)i << 2) + 2,
                              (uint16_t)control1);
                    }
                    out += 64;
                    fields_block += 32;
                    done += 8;
                } while (done != block);

                out = out_end;
                fields += (size_t)block << 2;
            }

            while (done != repeats) {
                wr32(out, word0);
                wr32(out + 4, word1);
                out += 8;
                wr16(fields, (uint16_t)control0);
                wr16(fields + 2, (uint16_t)control1);
                fields += 4;
                done++;
            }

            output = out;
        }

        amount--;
    } while (amount != 0);
}




static uint16_t blend(uint32_t accumulator, int16_t coef, int16_t sample)
{
    int32_t product = (int32_t)coef * (int32_t)sample;
    uint32_t sum = accumulator + (uint32_t)product;
    uint32_t shifted = sum >> 19;

    if ((sum & UINT32_C(0x80000000)) != 0)
        shifted |= UINT32_C(0xffffe000);
    return (uint16_t)shifted;
}

void gpu2d_palette_mix_dual_coeff(void *param_1, const void *param_2,
                         const void *param_3, uint32_t param_4,
                         uint32_t param_5)
{

    uint8_t *dest = (uint8_t *)param_1;
    const uint8_t *accumulators = (const uint8_t *)param_2;
    const uint8_t *samples = (const uint8_t *)param_3;
    uint32_t index_bytes = param_5 << 2;
    const uint8_t *coefs = accumulators + index_bytes;
    uint32_t done = 0;

    if (param_4 == 0)
        return;

    if (param_4 >= 4) {
        uintptr_t start_dest = (uintptr_t)dest;
        uintptr_t end_dest = start_dest + (size_t)param_4 * 4u;
        uintptr_t start_samples = (uintptr_t)samples;
        uintptr_t end_samples = start_samples + (size_t)param_4 * 2u;
        uintptr_t start_coefs = (uintptr_t)coefs;
        uintptr_t end_coefs = start_coefs + (size_t)param_4 * 4u;
        int overwrites_samples = (end_samples > start_dest) &&
                            (end_dest > start_samples);
        int overwrites_coefs = (end_coefs > start_dest) &&
                                (end_dest > start_coefs);

        if (!overwrites_samples && !overwrites_coefs) {
            uint32_t limit = param_4 & UINT32_C(0xfffffffc);

            while (done != limit) {
                int16_t sample[4];
                uint32_t accumulator[8];
                int16_t coef[8];
                uint16_t result[8];
                uint32_t i;

                for (i = 0; i != 4; ++i)
                    sample[i] = (int16_t)rd16(samples + ((size_t)done + i) * 2u);
                for (i = 0; i != 8; ++i)
                    accumulator[i] = rd32(accumulators + ((size_t)done * 2u + i) * 4u);
                for (i = 0; i != 8; ++i)
                    coef[i] = (int16_t)rd16(coefs + ((size_t)done * 2u + i) * 2u);

                for (i = 0; i != 4; ++i) {
                    result[i * 2u] = blend(accumulator[i * 2u],
                                                 coef[i * 2u], sample[i]);
                    result[i * 2u + 1u] = blend(accumulator[i * 2u + 1u],
                                                      coef[i * 2u + 1u], sample[i]);
                }
                for (i = 0; i != 8; ++i)
                    wr16(dest + ((size_t)done * 2u + i) * 2u, result[i]);

                done += 4;
            }
        }
    }

    while (done != param_4) {
        int16_t sample = (int16_t)rd16(samples + (size_t)done * 2u);
        uint32_t accumulator_0 = rd32(accumulators + (size_t)done * 8u);
        int16_t coef_0 = (int16_t)rd16(coefs + (size_t)done * 4u);

        wr16(dest + (size_t)done * 4u,
              blend(accumulator_0, coef_0, sample));

        uint32_t accumulator_1 = rd32(accumulators + (size_t)done * 8u + 4u);
        int16_t coef_1 = (int16_t)rd16(coefs + (size_t)done * 4u + 2u);

        wr16(dest + (size_t)done * 4u + 2u,
              blend(accumulator_1, coef_1, sample));
        ++done;
    }
}




void gpu2d_palette_fill_six_planes(void *param_1, void *param_2,
                         uint32_t param_3, uint32_t param_4)
{

    const unsigned char *table420 = (const unsigned char *)param_1 + 0x420u;
    const unsigned char *table4d0 = (const unsigned char *)param_1 + 0x4d0u;
    const unsigned char *table580 = (const unsigned char *)param_1 + 0x580u;
    const unsigned char *table630 = (const unsigned char *)param_1 + 0x630u;
    uint64_t jump = param_4;
    uint64_t jump_double = (uint32_t)(param_4 << 1);
    unsigned char *plane0 = param_2;
    unsigned char *plane1 = plane0 + jump;
    unsigned char *plane2 = plane1 + jump;
    unsigned char *plane3 = plane2 + jump;
    unsigned char *plane5 = plane3 + jump_double;
    unsigned char *plane7 = plane5 + jump_double;

    for (uint32_t group = 0; group < param_3; ++group) {
        int32_t count_signed = (int16_t)rd16(table630);

        if (count_signed != 0) {
            uint32_t amount = (uint32_t)count_signed;
            uint32_t elements = (amount > 1u) ? amount : 1u;
            uint32_t value420_0 = (uint32_t)rd16(table420) << 15;
            uint32_t value420_1 = (uint32_t)rd16(table420 + 2u) << 15;
            uint32_t value580_1 = (uint32_t)rd16(table580 + 2u) << 15;
            uint16_t value4d0_0 = rd16(table4d0);
            uint16_t value4d0_1 = rd16(table4d0 + 2u);
            uint16_t value630_1 = rd16(table630 + 2u);
            uint64_t vectorized = 0;
            int use_scalar = elements < 8u;

            if (!use_scalar) {
                uint64_t final4 = (uint64_t)(elements - 1u) << 2;
                uint64_t final2 = (uint64_t)(elements - 1u) << 1;
                unsigned char *p5_final = plane5 + final4;
                unsigned char *p3_final = plane3 + final4;
                unsigned char *p7_final = plane7 + final4;
                unsigned char *p1_final = plane1 + final2;
                unsigned char *p0_final = plane0 + final2;
                unsigned char *p2_final = plane2 + final2;

                use_scalar =
                    ((plane3 < p5_final + 4u) && (plane5 < p3_final + 4u)) ||
                    ((plane3 < p7_final + 4u) && (plane7 < p3_final + 4u)) ||
                    ((plane5 < p7_final + 4u) && (plane7 < p5_final + 4u)) ||
                    ((plane0 < p1_final + 2u) && (plane1 < p0_final + 2u)) ||
                    ((plane0 < p2_final + 2u) && (plane2 < p0_final + 2u)) ||
                    ((plane1 < p2_final + 2u) && (plane2 < p1_final + 2u));

                if (!use_scalar) {
                    vectorized = (uint64_t)elements & UINT64_C(0x1fffffff8);

                    for (uint64_t block = 0; block < vectorized; block += 8u) {
                        uint64_t index4 = block << 2;
                        uint64_t index2 = block << 1;

                        for (uint64_t j = 0; j < 8u; ++j)
                            wr32(plane3 + index4 + (j << 2), value420_0);
                        for (uint64_t j = 0; j < 8u; ++j)
                            wr32(plane5 + index4 + (j << 2), value420_1);
                        for (uint64_t j = 0; j < 8u; ++j)
                            wr32(plane7 + index4 + (j << 2), value580_1);
                        for (uint64_t j = 0; j < 8u; ++j)
                            wr16(plane0 + index2 + (j << 1), value4d0_0);
                        for (uint64_t j = 0; j < 8u; ++j)
                            wr16(plane1 + index2 + (j << 1), value4d0_1);
                        for (uint64_t j = 0; j < 8u; ++j)
                            wr16(plane2 + index2 + (j << 1), value630_1);
                    }

                    plane0 += vectorized << 1;
                    plane1 += vectorized << 1;
                    plane2 += vectorized << 1;
                    plane3 += vectorized << 2;
                    plane5 += vectorized << 2;
                    plane7 += vectorized << 2;
                }
            }

            if (use_scalar || vectorized != elements) {
                uint64_t start = vectorized;
                uint64_t remaining = (uint64_t)elements - start;

                for (uint64_t i = 0; i < remaining; ++i) {
                    wr32(plane3 + (i << 2), value420_0);
                    wr32(plane5 + (i << 2), value420_1);
                    wr32(plane7 + (i << 2), value580_1);
                    wr16(plane0 + (i << 1), value4d0_0);
                    wr16(plane1 + (i << 1), value4d0_1);
                    wr16(plane2 + (i << 1), value630_1);
                }

                plane0 += remaining << 1;
                plane1 += remaining << 1;
                plane2 += remaining << 1;
                plane3 += remaining << 2;
                plane5 += remaining << 2;
                plane7 += remaining << 2;
            }
        }

        table420 += 4u;
        table4d0 += 4u;
        table580 += 4u;
        table630 += 4u;
    }
}

void gpu2d_palette_mix_three_channel(unsigned char *dst,
                        const unsigned char *src,
                        const unsigned char *coef,
                        uint32_t n,
                        uint32_t stride)
{

    if (n == 0) return;

    uint64_t z  = (uint64_t)stride;
    uint64_t z2 = (uint64_t)(uint32_t)(stride << 1);

    const unsigned char *src0 = src;
    const unsigned char *src1 = src + z;
    unsigned char       *dst0 = dst;
    unsigned char       *dst1 = dst + z;
    const unsigned char *src2 = src1 + z;
    unsigned char       *dst2 = dst1 + z;
    const unsigned char *acc0 = src2 + z;
    const unsigned char *acc1 = acc0 + z2;
    const unsigned char *acc2 = acc1 + z2;

    uint64_t i  = 0;
    uint32_t nn = n;

    do {
        uint64_t o2 = i << 1;
        uint64_t o4 = i << 2;

        int16_t  t16;
        uint32_t acc, r, k;

        memcpy(&t16, coef + o2, 2);
        k = (uint32_t)(int32_t)t16;

        memcpy(&acc, acc0 + o4, 4);
        memcpy(&t16, src0 + o2, 2);
        r = (uint32_t)(int32_t)t16 * k + acc;
        r >>= 18;
        dst0[i] = (unsigned char)r;

        memcpy(&acc, acc1 + o4, 4);
        memcpy(&t16, src1 + o2, 2);
        r = (uint32_t)(int32_t)t16 * k + acc;
        r >>= 18;
        dst1[i] = (unsigned char)r;

        memcpy(&acc, acc2 + o4, 4);
        memcpy(&t16, src2 + o2, 2);
        r = (uint32_t)(int32_t)t16 * k + acc;
        r >>= 18;
        dst2[i] = (unsigned char)r;

        i += 1;
    } while ((uint32_t)i != nn);
}



static uint32_t convert(uint16_t first, uint16_t second,
                          uint32_t factor, uint32_t mask_first,
                          uint32_t second_mask)
{
    uint32_t a = (uint32_t)first & mask_first;
    uint32_t b = (uint32_t)second & second_mask;

    if ((a & 0x8000u) != 0)
        a |= 0xffff0000u;
    if ((b & 0x8000u) != 0)
        b |= 0xffff0000u;

    return b * factor + a;
}

void gpu2d_palette_pack_masked_pairs(void *dest, const void *source, uint32_t amount,
                         uint32_t factor, uint32_t second_mask)
{

    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)source;
    uint32_t mask_first = factor - 1u;
    uint32_t mask_b = second_mask - 1u;
    uint32_t done = 0;

    if (amount >= 8u) {
        uint32_t limit = amount & ~7u;

        do {
            uint16_t a[8];
            uint16_t b[8];
            uint32_t outs[8];
            uint32_t i;

            for (i = 4; i != 8u; ++i) {
                a[i] = rd16(s + i * 4u);
                b[i] = rd16(s + i * 4u + 2u);
            }
            for (i = 0; i != 4u; ++i) {
                a[i] = rd16(s + i * 4u);
                b[i] = rd16(s + i * 4u + 2u);
            }

            for (i = 0; i != 8u; ++i)
                outs[i] = convert(a[i], b[i], factor,
                                       mask_first, mask_b);

            memcpy(d, outs, sizeof(outs));
            s += 32u;
            d += 32u;
            done += 8u;
        } while (done != limit);
    }

    while (done != amount) {
        uint16_t second = rd16(s + 2u);
        uint16_t first = rd16(s);

        wr32(d, convert(first, second, factor,
                           mask_first, mask_b));
        s += 4u;
        d += 4u;
        ++done;
    }
}



static int32_t sign_extend16_coords(uint32_t value)
{
    value &= UINT32_C(0xffff);
    return (value & UINT32_C(0x8000)) != 0 ?
        (int32_t)value - 0x10000 : (int32_t)value;
}

static uint32_t clamp_coord(uint16_t entry, uint32_t limit)
{
    int32_t with_sign = sign_extend16_coords(entry);
    uint32_t no_sign = with_sign < 0 ? 0 : (uint32_t)with_sign;

    return limit > no_sign ? no_sign : limit - 1u;
}

static uint32_t index_coords_clamped(uint16_t x_entry, uint16_t y_entry,
                              uint32_t width, uint32_t height)
{
    uint32_t x = clamp_coord(x_entry, width);
    uint32_t y = clamp_coord(y_entry, height);

    return width * (uint32_t)sign_extend16_coords(y) +
           (uint32_t)sign_extend16_coords(x);
}

void gpu2d_palette_pack_coords_clamped(void *dest, const void *origin, uint32_t amount,
                         uint32_t width, uint32_t height)
{

    uint8_t *output = (uint8_t *)dest;
    const uint8_t *entry = (const uint8_t *)origin;
    uint32_t block;

    if (amount == 0)
        return;

    block = amount & UINT32_C(0xfffffffc);
    if (block != 0) {
        uint32_t i;

        for (i = 0; i < block; i += 4) {
            uint16_t x0 = rd16(entry + 0);
            uint16_t y0 = rd16(entry + 2);
            uint16_t x1 = rd16(entry + 4);
            uint16_t y1 = rd16(entry + 6);
            uint16_t x2 = rd16(entry + 8);
            uint16_t y2 = rd16(entry + 10);
            uint16_t x3 = rd16(entry + 12);
            uint16_t y3 = rd16(entry + 14);

            wr32(output + 0, index_coords_clamped(x0, y0, width, height));
            wr32(output + 4, index_coords_clamped(x1, y1, width, height));
            wr32(output + 8, index_coords_clamped(x2, y2, width, height));
            wr32(output + 12, index_coords_clamped(x3, y3, width, height));
            entry += 16;
            output += 16;
        }
    }

    for (block = amount - block; block != 0; --block) {
        uint16_t x = rd16(entry + 0);
        uint16_t y = rd16(entry + 2);

        entry += 4;
        wr32(output, index_coords_clamped(x, y, width, height));
        output += 4;
    }
}



static uint32_t sign_extend16_halfword_pairs(uint16_t v)
{
    return (v & UINT32_C(0x8000)) ? v | UINT32_C(0xffff0000) : v;
}

static uint32_t index_halfword_pairs(uint16_t raw_a, uint16_t raw_b,
                                    uint32_t mask_a, uint32_t mask_b)
{
    uint32_t a = sign_extend16_halfword_pairs(raw_a);
    uint32_t b = sign_extend16_halfword_pairs(raw_b);
    uint32_t limit_a = mask_a - 1u;
    uint32_t limit_b = mask_b - 1u;
    uint32_t adjust_a;
    uint32_t adjust_b;
    uint32_t low_a;
    uint32_t low_b;

    adjust_a = ((a & mask_a) == 0) ? 0 : limit_a;

    adjust_b = ((b & mask_b) == 0) ? 0 : limit_b;
    low_a = (a ^ adjust_a) & limit_a;
    low_b = (b ^ adjust_b) & limit_b;

    return sign_extend16_halfword_pairs((uint16_t)low_b) * mask_a +
           sign_extend16_halfword_pairs((uint16_t)low_a);
}

void gpu2d_palette_pack_halfword_pairs(uint8_t *dest, const uint8_t *source,
                        uint32_t count, uint32_t mask_a,
                        uint32_t mask_b)
{

    if (count == 0)
        return;

    uint64_t groups = count & UINT32_C(0x1ffffffc);
    uint64_t i = 0;

    while (i != groups) {
        uint16_t a0 = rd16(source + i * 4 + 0);
        uint16_t b0 = rd16(source + i * 4 + 2);
        uint16_t a1 = rd16(source + i * 4 + 4);
        uint16_t b1 = rd16(source + i * 4 + 6);
        uint16_t a2 = rd16(source + i * 4 + 8);
        uint16_t b2 = rd16(source + i * 4 + 10);
        uint16_t a3 = rd16(source + i * 4 + 12);
        uint16_t b3 = rd16(source + i * 4 + 14);
        uint32_t results[4];

        results[0] = index_halfword_pairs(a0, b0, mask_a, mask_b);
        results[1] = index_halfword_pairs(a1, b1, mask_a, mask_b);
        results[2] = index_halfword_pairs(a2, b2, mask_a, mask_b);
        results[3] = index_halfword_pairs(a3, b3, mask_a, mask_b);
        memcpy(dest + i * 4, results, sizeof(results));
        i += 4;
    }

    while (i != count) {
        uint16_t a = rd16(source + i * 4);
        uint16_t b = rd16(source + i * 4 + 2);

        wr32(dest + i * 4,
                    index_halfword_pairs(a, b, mask_a, mask_b));
        ++i;
    }
}



static int32_t sign_extend16_clamped(uint16_t v)
{
    return (int32_t)(v ^ UINT16_C(0x8000)) - INT32_C(0x8000);
}

static uint32_t index_clamped(uint16_t first, uint16_t second,
                                uint32_t multiplier, uint32_t limit)
{
    int32_t with_sign = sign_extend16_clamped(second);
    uint32_t positive = with_sign < 0 ? 0u : (uint32_t)with_sign;
    uint16_t chosen;
    uint16_t sum16;

    if (limit > positive)
        chosen = (uint16_t)positive;
    else
        chosen = (uint16_t)(limit - 1u);

    sum16 = (uint16_t)(first & (multiplier - 1u));
    return multiplier * (uint32_t)sign_extend16_clamped(chosen) +
           (uint32_t)sign_extend16_clamped(sum16);
}

void gpu2d_palette_pack_index_clamped(void *dest, const void *origin, uint32_t amount,
                         uint32_t multiplier, uint32_t limit)
{

    uint8_t *output = (uint8_t *)dest;
    const uint8_t *entry = (const uint8_t *)origin;
    uint32_t four;
    uint32_t i;

    if (amount == 0u)
        return;

    four = amount & ~UINT32_C(3);
    for (i = 0u; i < four; i += 4u) {
        uint16_t first0 = rd16(entry + 0u);
        uint16_t second0 = rd16(entry + 2u);
        uint16_t first1 = rd16(entry + 4u);
        uint16_t second1 = rd16(entry + 6u);
        uint16_t first2 = rd16(entry + 8u);
        uint16_t second2 = rd16(entry + 10u);
        uint16_t first3 = rd16(entry + 12u);
        uint16_t second3 = rd16(entry + 14u);

        wr32(output + 0u,
                    index_clamped(first0, second0, multiplier, limit));
        wr32(output + 4u,
                    index_clamped(first1, second1, multiplier, limit));
        wr32(output + 8u,
                    index_clamped(first2, second2, multiplier, limit));
        wr32(output + 12u,
                    index_clamped(first3, second3, multiplier, limit));
        entry += 16u;
        output += 16u;
    }

    for (four = amount - four; four != 0u; --four) {
        uint16_t second = rd16(entry + 2u);
        uint16_t first = rd16(entry);

        wr32(output,
                    index_clamped(first, second, multiplier, limit));
        entry += 4u;
        output += 4u;
    }
}



static int32_t sign_extend16(uint16_t v)
{
    return (int32_t)(v ^ UINT16_C(0x8000)) - INT32_C(0x8000);
}

static uint32_t blend_one(uint16_t first, uint16_t second,
                           uint32_t mask, uint32_t limit)
{
    uint32_t minus_mask = mask - 1u;
    uint32_t minus_limit = limit - 1u;
    int32_t first_with_sign = sign_extend16(first);
    uint16_t adjust;
    int32_t max;
    uint16_t factor16;
    int32_t factor;
    uint16_t sum16;

    if ((mask & (uint32_t)first_with_sign) == 0u)
        adjust = 0;
    else
        adjust = (uint16_t)minus_mask;

    max = sign_extend16(second);
    if (max < 0)
        max = 0;

    if (limit > (uint32_t)max)
        factor16 = (uint16_t)max;
    else
        factor16 = (uint16_t)minus_limit;

    factor = sign_extend16(factor16);
    sum16 = (uint16_t)((uint16_t)(first ^ adjust) &
                         (uint16_t)minus_mask);

    return mask * (uint32_t)factor + (uint32_t)sign_extend16(sum16);
}

void gpu2d_palette_mix_interleaved_pairs(uint32_t *dest, const uint8_t *origin,
                        uint32_t amount, uint32_t mask, uint32_t limit)
{

    uint64_t block = (uint64_t)(amount & ~UINT32_C(3));
    uint64_t i;

    for (i = 0; i < block; i += 4u) {
        uint16_t first[4];
        uint16_t second[4];
        uint32_t result[4];
        uint32_t j;

        for (j = 0; j != 4u; ++j) {
            const uint8_t *p = origin + (i + j) * 4u;
            first[j] = rd16(p);
            second[j] = rd16(p + 2u);
        }
        for (j = 0; j != 4u; ++j)
            result[j] = blend_one(first[j], second[j], mask, limit);
        for (j = 0; j != 4u; ++j)
            wr32((uint8_t *)dest + (i + j) * 4u, result[j]);
    }

    for (; i < amount; ++i) {
        const uint8_t *p = origin + i * 4u;
        uint16_t first = rd16(p);
        uint16_t second = rd16(p + 2u);
        uint32_t result = blend_one(first, second, mask, limit);

        wr32((uint8_t *)dest + i * 4u, result);
    }
}



static uint32_t sign_extend16_scaled_clamped(uint16_t v) {
    return (v & 0x8000u) ? (0xffff0000u | (uint32_t)v) : (uint32_t)v;
}

static uint32_t index_pairs_scaled_clamped(uint16_t first, uint16_t second,
                                       uint32_t scale, uint32_t mask) {
    uint16_t clamped;
    uint16_t chosen;
    uint16_t multiplicand;
    uint32_t product;

    clamped = (first & 0x8000u) ? 0u : first;
    chosen = scale > (uint32_t)clamped ? clamped
                                           : (uint16_t)(scale - 1u);
    multiplicand = (uint16_t)(second & (uint16_t)(mask - 1u));
    product = sign_extend16_scaled_clamped(multiplicand) * scale;
    return product + sign_extend16_scaled_clamped(chosen);
}

void gpu2d_palette_pack_pairs_scaled_clamped(void *output, const void *entry, uint32_t amount,
                        uint32_t scale, uint32_t mask) {

    unsigned char *dst = (unsigned char *)output;
    const unsigned char *src = (const unsigned char *)entry;
    uint32_t blocks;
    uint32_t rest;

    if (amount == 0)
        return;

    if (amount >= 4) {
        blocks = amount & 0xfffffffcu;
        rest = blocks;
        do {
            uint16_t p0 = rd16(src);
            uint16_t s0 = rd16(src + 2);
            uint16_t p1 = rd16(src + 4);
            uint16_t s1 = rd16(src + 6);
            uint16_t p2 = rd16(src + 8);
            uint16_t s2 = rd16(src + 10);
            uint16_t p3 = rd16(src + 12);
            uint16_t s3 = rd16(src + 14);

            src += 16;
            rest -= 4;
            wr32(dst, index_pairs_scaled_clamped(p0, s0, scale, mask));
            wr32(dst + 4, index_pairs_scaled_clamped(p1, s1, scale, mask));
            wr32(dst + 8, index_pairs_scaled_clamped(p2, s2, scale, mask));
            wr32(dst + 12, index_pairs_scaled_clamped(p3, s3, scale, mask));
            dst += 16;
        } while (rest != 0);

        if (amount == blocks)
            return;
    } else {
        blocks = 0;
    }

    rest = amount - blocks;
    do {
        uint16_t first = rd16(src);
        uint16_t second = rd16(src + 2);

        src += 4;
        rest--;
        wr32(dst, index_pairs_scaled_clamped(first, second,
                                                    scale, mask));
        dst += 4;
    } while (rest != 0);
}



static int32_t sext16_xor(uint16_t v)
{
    return (int32_t)(v ^ UINT16_C(0x8000)) - 0x8000;
}

static uint32_t combine(uint16_t first, uint16_t second,
                         uint32_t mask, uint32_t clip)
{
    int32_t a = sext16_xor(first);
    uint32_t mask_minus_one = mask - 1u;
    uint32_t clip_minus_one = clip - 1u;
    uint32_t chosen = (((uint32_t)a & mask) == 0u) ? 0u : mask_minus_one;
    uint16_t low = (uint16_t)(((uint32_t)a ^ chosen) & mask_minus_one);
    uint16_t height = (uint16_t)(second & clip_minus_one);

    return (uint32_t)sext16_xor(height) * mask + (uint32_t)sext16_xor(low);
}

void gpu2d_palette_pack_masked_entry(uint8_t *dest, const uint8_t *origin,
                         uint32_t count, uint32_t mask, uint32_t clip)
{

    if (count == 0u)
        return;

    if (count >= 4u) {
        uint32_t grouped = count & ~UINT32_C(3);

        while (grouped != 0u) {

            uint16_t first0 = rd16(origin + 0);
            uint16_t second0 = rd16(origin + 2);
            uint16_t first1 = rd16(origin + 4);
            uint16_t second1 = rd16(origin + 6);
            uint16_t first2 = rd16(origin + 8);
            uint16_t second2 = rd16(origin + 10);
            uint16_t first3 = rd16(origin + 12);
            uint16_t second3 = rd16(origin + 14);

            wr32(dest + 0, combine(first0, second0, mask, clip));
            wr32(dest + 4, combine(first1, second1, mask, clip));
            wr32(dest + 8, combine(first2, second2, mask, clip));
            wr32(dest + 12, combine(first3, second3, mask, clip));

            origin += 16;
            dest += 16;
            grouped -= 4u;
        }

        count &= UINT32_C(3);
    }

    while (count != 0u) {
        int32_t first = sext16_xor(rd16(origin));
        uint16_t second = rd16(origin + 2);
        uint32_t mask_minus_one = mask - 1u;
        uint32_t chosen = (((uint32_t)first & mask) == 0u)
                             ? 0u : mask_minus_one;
        uint16_t low = (uint16_t)(((uint32_t)first ^ chosen) & mask_minus_one);
        uint16_t height = (uint16_t)(second & (clip - 1u));
        uint32_t result = (uint32_t)sext16_xor(height) * mask +
                             (uint32_t)sext16_xor(low);

        wr32(dest, result);
        origin += 4;
        dest += 4;
        count -= 1u;
    }
}



static int32_t sext16(uint16_t v)
{
    return (v & 0x8000u) ? (int32_t)v - 0x10000 : (int32_t)v;
}

static uint32_t combine_samples(uint16_t first_row, uint16_t second,
                                  uint32_t limit, uint32_t mask)
{
    uint32_t positive = (first_row & 0x8000u) ? 0u : first_row;
    uint32_t minus_one = mask - 1u;
    uint16_t chosen = limit > positive ? (uint16_t)positive
                                          : (uint16_t)(limit - 1u);
    uint16_t selector = (((uint32_t)sext16(second) & mask) == 0u)
                            ? 0u : (uint16_t)minus_one;
    uint16_t difference = (uint16_t)((second ^ selector) & minus_one);

    return (uint32_t)sext16(difference) * limit +
           (uint32_t)sext16(chosen);
}

void gpu2d_palette_pack_samples_clamped(uint32_t *dest, const int16_t *origin,
                         uint32_t amount, uint32_t limit,
                         uint32_t mask)
{

    const uint8_t *entry = (const uint8_t *)origin;
    uint8_t *output = (uint8_t *)dest;
    uint32_t four = amount & ~3u;
    uint32_t i;

    for (i = 0; i < four; i += 4u) {
        size_t base = (size_t)i * 4u;
        uint16_t first0 = rd16(entry + base);
        uint16_t second0 = rd16(entry + base + 2u);
        uint16_t first1 = rd16(entry + base + 4u);
        uint16_t second1 = rd16(entry + base + 6u);
        uint16_t first2 = rd16(entry + base + 8u);
        uint16_t second2 = rd16(entry + base + 10u);
        uint16_t first3 = rd16(entry + base + 12u);
        uint16_t second3 = rd16(entry + base + 14u);

        wr32(output + base, combine_samples(first0, second0, limit, mask));
        wr32(output + base + 4u, combine_samples(first1, second1, limit, mask));
        wr32(output + base + 8u, combine_samples(first2, second2, limit, mask));
        wr32(output + base + 12u, combine_samples(first3, second3, limit, mask));
    }

    for (; i < amount; ++i) {
        size_t base = (size_t)i * 4u;
        uint16_t first_row = rd16(entry + base);
        uint16_t second = rd16(entry + base + 2u);
        wr32(output + base, combine_samples(first_row, second, limit, mask));
    }
}



static int32_t sign_extend16_pairs_scaled(uint16_t v)
{
    return (v & 0x8000u) ? (int32_t)v - 65536 : (int32_t)v;
}

static uint32_t index_pairs_scaled(uint16_t low, uint16_t height,
                                uint32_t multiplier, uint32_t mask)
{
    uint32_t limit_height = mask - 1u;
    uint16_t chosen;
    uint16_t folded;
    uint16_t clamped;
    uint32_t product;

    if (((uint32_t)sign_extend16_pairs_scaled(height) & mask) == 0)
        chosen = 0;
    else
        chosen = (uint16_t)limit_height;

    folded = (uint16_t)(((uint32_t)chosen ^ height) & limit_height);
    clamped = (uint16_t)((uint32_t)low & (multiplier - 1u));
    product = (uint32_t)sign_extend16_pairs_scaled(folded) * multiplier;
    return product + (uint32_t)sign_extend16_pairs_scaled(clamped);
}

void gpu2d_palette_pack_pairs_scaled(uint32_t *dest, const uint8_t *origin,
                        uint32_t amount, uint32_t multiplier,
                        uint32_t mask)
{

    if (amount != 0) {
        uint8_t *output = (uint8_t *)dest;
        uint32_t remaining = amount;

        if (remaining >= 4) {
            uint32_t vectorized = remaining & ~3u;

            do {
                uint16_t low0 = rd16(origin + 0);
                uint16_t high0 = rd16(origin + 2);
                uint16_t low1 = rd16(origin + 4);
                uint16_t high1 = rd16(origin + 6);
                uint16_t low2 = rd16(origin + 8);
                uint16_t high2 = rd16(origin + 10);
                uint16_t low3 = rd16(origin + 12);
                uint16_t high3 = rd16(origin + 14);
                uint32_t value0 = index_pairs_scaled(low0, high0, multiplier, mask);
                uint32_t value1 = index_pairs_scaled(low1, high1, multiplier, mask);
                uint32_t value2 = index_pairs_scaled(low2, high2, multiplier, mask);
                uint32_t value3 = index_pairs_scaled(low3, high3, multiplier, mask);

                wr32(output + 0, value0);
                wr32(output + 4, value1);
                wr32(output + 8, value2);
                wr32(output + 12, value3);
                origin += 16;
                output += 16;
                remaining -= 4;
                vectorized -= 4;
            } while (vectorized != 0);
        }

        while (remaining != 0) {
            uint16_t height = rd16(origin + 2);
            uint16_t low = rd16(origin + 0);
            uint32_t value = index_pairs_scaled(low, height, multiplier, mask);

            wr32(output, value);
            origin += 4;
            output += 4;
            remaining--;
        }
    }
}

void gpu2d_palette_lookup_chain2(uint32_t *dest, const uint32_t *indices,
                         const uint8_t *table_byte, const uint32_t *table_word,
                         int count)
{

    const uint8_t *pidx = (const uint8_t *)indices;
    uint8_t *pdst = (uint8_t *)dest;

    for (; count != 0; count--) {
        uint32_t idx;
        memcpy(&idx, pidx, sizeof(idx));
        pidx += sizeof(idx);

        uint8_t b = table_byte[idx];

        uint32_t v;
        memcpy(&v, (const uint8_t *)table_word + (uint32_t)b * 4, sizeof(v));

        memcpy(pdst, &v, sizeof(v));
        pdst += sizeof(v);
    }
}

void gpu2d_palette_translate_words(unsigned char *output, const unsigned char *entry,
                        const unsigned char *table, uint32_t n) {
    if (n == 0) return;
    do {
        uint32_t i;
        memcpy(&i, entry, 4);
        entry += 4;
        n -= 1;
        uint32_t v;
        memcpy(&v, table + (uint64_t)i * 4, 4);
        memcpy(output, &v, 4);
        output += 4;
    } while (n != 0);
}

void gpu2d_palette_blit_indexed8_rows(unsigned char *dst,
                        const unsigned char *src,
                        const unsigned char *palette,
                        uint32_t width,
                        uint32_t height,
                        uint32_t stride)
{

    if ((width & 3u) != 0u) {

        if (height == 0u) return;
        if (width == 0u) return;

        uint32_t row = 0;
        uint64_t z = (uint64_t)stride;
        uint64_t n = (uint64_t)width;

        do {
            uint64_t i = 0;

            do {
                uint32_t b = src[i];
                i += 1;

                uint32_t v;
                memcpy(&v, palette + (uint64_t)b * 4u, 4);

                memcpy(dst, &v, 4);
                dst += 4;
            } while (n != i);

            row += 1;

            src += z;
        } while (row != height);

        return;
    }

    if (height == 0u) return;
    if (width == 0u) return;

    {
        uint32_t row = 0;
        uint64_t z = (uint64_t)stride;

        do {
            uint32_t i = 0;

            do {
                uint32_t b, v;

                b = src[(uint64_t)i];
                memcpy(&v, palette + (uint64_t)b * 4u, 4);
                memcpy(dst + 0, &v, 4);

                b = src[(uint64_t)(i + 1u)];
                memcpy(&v, palette + (uint64_t)b * 4u, 4);
                memcpy(dst + 4, &v, 4);

                b = src[(uint64_t)(i + 2u)];
                memcpy(&v, palette + (uint64_t)b * 4u, 4);
                memcpy(dst + 8, &v, 4);

                b = src[(uint64_t)(i + 3u)];
                i += 4u;

                memcpy(&v, palette + (uint64_t)b * 4u, 4);
                memcpy(dst + 12, &v, 4);
                dst += 16;
            } while (i < width);

            row += 1;

            src += z;
        } while (row != height);
    }

}

void gpu2d_palette_blit_indexed8_rows_clone(unsigned char *dst,
                        const unsigned char *src,
                        const unsigned char *palette,
                        uint32_t width,
                        uint32_t height,
                        uint32_t stride)
{

    if ((width & 3u) != 0u) {

        if (height == 0u) return;
        if (width == 0u) return;

        uint32_t row = 0;
        uint64_t z = (uint64_t)stride;
        uint64_t n = (uint64_t)width;

        do {
            uint64_t i = 0;

            do {
                uint32_t b = src[i];
                i += 1;

                uint32_t v;
                memcpy(&v, palette + (uint64_t)b * 4u, 4);

                memcpy(dst, &v, 4);
                dst += 4;
            } while (n != i);

            row += 1;

            src += z;
        } while (row != height);

        return;
    }

    if (height == 0u) return;
    if (width == 0u) return;

    {
        uint32_t row = 0;
        uint64_t z = (uint64_t)stride;

        do {
            uint32_t i = 0;

            do {
                uint32_t b, v;

                b = src[(uint64_t)i];
                memcpy(&v, palette + (uint64_t)b * 4u, 4);
                memcpy(dst + 0, &v, 4);

                b = src[(uint64_t)(i + 1u)];
                memcpy(&v, palette + (uint64_t)b * 4u, 4);
                memcpy(dst + 4, &v, 4);

                b = src[(uint64_t)(i + 2u)];
                memcpy(&v, palette + (uint64_t)b * 4u, 4);
                memcpy(dst + 8, &v, 4);

                b = src[(uint64_t)(i + 3u)];
                i += 4u;

                memcpy(&v, palette + (uint64_t)b * 4u, 4);
                memcpy(dst + 12, &v, 4);
                dst += 16;
            } while (i < width);

            row += 1;

            src += z;
        } while (row != height);
    }

}



static void copy32(unsigned char *dest, const unsigned char *origin)
{
    memmove(dest, origin, 32);
}

void gpu2d_rows_copy_strided(void *param_1, const void *param_2,
                         uint32_t param_3, uint32_t param_4,
                         uint32_t param_5)
{

    if (param_4 == 0)
        return;
    if (param_3 == 0)
        return;

    unsigned char *dest = param_1;
    const unsigned char *origin = param_2;
    const unsigned char *const origin_base = origin;
    const uint64_t step_bytes = (uint64_t)param_5 << 2;
    const uint64_t width_blocks = (uint64_t)(param_3 & 0xfffffff8u);
    uint64_t row = 0;

    for (;;) {
        int blocks = 0;

        if (param_3 >= 8u) {
            uint64_t start_row = row * (uint64_t)param_5;
            const unsigned char *end_origin = origin_base +
                ((start_row + (uint64_t)param_3) << 2);
            const unsigned char *start_origin = origin_base + (start_row << 2);
            const unsigned char *end_dest = dest + ((uint64_t)param_3 << 2);

            blocks = dest >= end_origin || start_origin >= end_dest;
        }

        if (blocks) {
            uint64_t missing = width_blocks;
            uint64_t offset = 0;

            do {
                copy32(dest + offset, origin + offset);
                missing -= 8u;
                offset += 32u;
            } while (missing != 0);

            dest += width_blocks << 2;
            if (width_blocks != (uint64_t)param_3) {
                uint64_t column = width_blocks;

                do {
                    uint32_t v = rd32(origin + (column << 2));
                    column += 1u;
                    wr32(dest, v);
                    dest += 4u;
                } while (column != (uint64_t)param_3);
            }
        } else {
            uint64_t column = 0;

            do {
                uint32_t v = rd32(origin + (column << 2));
                column += 1u;
                wr32(dest, v);
                dest += 4u;
            } while (column != (uint64_t)param_3);
        }

        row += 1u;
        origin += step_bytes;
        if ((uint32_t)row == param_4)
            return;
    }
}



static uint32_t blend_block(uint32_t pixel, uint32_t adjust, uint32_t sum_alpha)
{
    uint32_t red = pixel & 0xffu;
    uint32_t green = (pixel >> 8) & 0xffu;
    uint32_t blue = (pixel >> 16) & 0xffu;
    uint32_t alpha = pixel >> 24;
    uint32_t vr = red + adjust;
    uint32_t vv = green + adjust;
    uint32_t va = blue + adjust;
    uint32_t aa = alpha + sum_alpha;

    vr += red * adjust;
    vv += green * adjust;
    va += blue * adjust;
    aa += alpha * sum_alpha;

    return ((aa << 19) & 0xff000000u) |
           (vr >> 6) |
           ((vv << 2) & 0xffffff00u) |
           ((va << 10) & 0xffff0000u);
}

static uint32_t blend_tail(uint32_t pixel, uint32_t adjust, uint32_t sum_alpha)
{
    uint32_t red = pixel & 0xffu;
    uint32_t green = (pixel >> 8) & 0xffu;
    uint32_t blue = (pixel >> 16) & 0xffu;
    uint32_t alpha = pixel >> 24;
    uint32_t vr = red + adjust;
    uint32_t vv = green + adjust;
    uint32_t va = blue + adjust;
    uint32_t aa = alpha + sum_alpha;
    uint32_t result;

    vr += red * adjust;
    vv += green * adjust;
    va += blue * adjust;
    aa += alpha * sum_alpha;

    result = (vr >> 6) & 0x00ffffffu;
    result |= ((aa >> 5) & 0xffu) << 24;
    result |= (vv << 2) & 0x0007ff00u;
    result |= (va << 10) & 0x07ff0000u;
    return result;
}

void gpu2d_pixels_apply_alpha_factor_line(uint8_t *dest, const uint8_t *origin,
                        const uint8_t *settings, uint32_t sum_alpha,
                        uint32_t amount)
{

    if (amount == 0)
        return;

    uint32_t count_block = 0;
    if (amount >= 4) {
        uintptr_t d = (uintptr_t)dest;
        uintptr_t o = (uintptr_t)origin;
        uintptr_t a = (uintptr_t)settings;
        uintptr_t end_dest = d + ((size_t)amount << 2);
        uintptr_t end_origin = o + ((size_t)amount << 2);
        uintptr_t end_settings = a + amount;

        if (!((end_origin > d && end_dest > o) ||
              (end_settings > d && end_dest > a))) {
            uint64_t remaining = (uint64_t)amount & UINT64_C(0x1fffffffc);
            count_block = (uint32_t)remaining;

            do {
                uint32_t entry[4];
                uint32_t output[4];
                uint8_t adjust0;
                uint8_t adjust1;
                uint8_t adjust2;
                uint8_t adjust3;

                memcpy(entry, origin, sizeof(entry));
                origin += 16;
                adjust0 = settings[0];
                adjust1 = settings[1];
                adjust2 = settings[2];
                adjust3 = settings[3];
                output[0] = blend_block(entry[0], adjust0, sum_alpha);
                output[1] = blend_block(entry[1], adjust1, sum_alpha);
                output[2] = blend_block(entry[2], adjust2, sum_alpha);
                output[3] = blend_block(entry[3], adjust3, sum_alpha);
                memcpy(dest, output, sizeof(output));
                dest += 16;
                settings += 4;
                remaining -= 4;
            } while (remaining != 0);

            if (amount == count_block)
                return;
        }
    }

    {
        uint32_t remaining = amount - count_block;
        do {
            uint32_t pixel = rd32(origin);
            uint32_t result = blend_tail(pixel, *settings, sum_alpha);

            wr32(dest, result);
            origin += 4;
            settings += 1;
            dest += 4;
            remaining -= 1;
        } while (remaining != 0);
    }
}

void gpu2d_planes_pack_three_with_tag(uint32_t *out, const uint8_t *in, uint32_t stride,
                         int32_t n, uint32_t tag)
{

    if (n == 0) {
        return;
    }

    uint64_t stride1 = (uint32_t)stride;
    uint64_t stride2 = stride1 << 1;
    uint32_t tag_hi = tag << 24;

    uint8_t *pout = (uint8_t *)out;
    const uint8_t *pin = in;

    do {
        n = n - 1;

        uint8_t b0 = pin[0];
        uint8_t b1 = pin[stride1];
        uint8_t b2 = pin[stride2];

        uint32_t word = tag_hi | (uint32_t)b0;
        word |= (uint32_t)b1 << 8;
        word |= (uint32_t)b2 << 16;

        memcpy(pout, &word, sizeof(word));
        pout += 4;
        pin += 1;
    } while (n != 0);
}




void gpu2d_blend_row_keep_prev_alpha(void *param_1, const void *param_2,
                         uint32_t param_3, void *param_4)
{

    uint8_t *dest = (uint8_t *)param_1;
    const uint8_t *source = (const uint8_t *)param_2;
    uint8_t *alphas = (uint8_t *)param_4;

    while (param_3 != 0) {

        uint32_t source_pixel = rd32(source);
        uint32_t dest_pixel = rd32(dest);
        uint32_t alpha_dest = dest_pixel >> 24;
        uint32_t alpha_source = (source_pixel >> 24) & 31u;
        uint32_t blue_dest = dest_pixel & 0xffu;
        uint32_t green_dest = (dest_pixel >> 8) & 0xffu;
        uint32_t red_dest = (dest_pixel >> 16) & 0xffu;
        uint32_t blue_source = source_pixel & 0xffu;
        uint32_t green_source = (source_pixel >> 8) & 0xffu;
        uint32_t red_source = (source_pixel >> 16) & 0xffu;
        uint32_t factor_dest;
        uint32_t factor_source;
        uint32_t alpha_final;
        uint32_t blue;
        uint32_t green;
        uint32_t red;
        uint32_t result;

        if (alpha_source == 0) {
            factor_dest = 31u;
            factor_source = 0;
        } else {
            factor_dest = alpha_dest;
            factor_source = (31u - alpha_dest) & 0xffu;
        }

        blue = blue_dest + factor_dest * blue_dest;
        green = green_dest + factor_dest * green_dest;
        red = red_dest + factor_dest * red_dest;
        blue += factor_source * blue_source;
        green = factor_source * green_source + green;
        red = factor_source * red_source + red;
        alpha_final = alpha_dest < alpha_source ? alpha_source : alpha_dest;

        result = ((blue >> 5) & 0xffu) |
                    ((green << 3) & 0xff00u) |
                    (((red >> 5) & 0xffu) << 16) |
                    ((alpha_final & 0xffu) << 24);

        wr8(alphas, (uint8_t)alpha_dest);
        wr32(dest, result);
        source += 4;
        dest += 4;
        alphas += 1;
        param_3 -= 1;
    }
}



void gpu2d_pixel_alpha_raise_and_save(uint8_t *pixels, const uint8_t *limits,
                        uint32_t amount, uint8_t *output)
{

    if (amount == 0u)
        return;

    uint32_t done = 0u;

    if (amount >= 4u) {
        uintptr_t end_output = (uintptr_t)output + (size_t)amount;
        uintptr_t end_pixels = (uintptr_t)pixels + (size_t)amount * 4u;
        uintptr_t end_limits = (uintptr_t)limits + (size_t)amount * 4u;
        int overlap_pixels_output = end_pixels > (uintptr_t)output &&
                                     end_output > (uintptr_t)pixels;
        int overlap_limits_output = end_limits > (uintptr_t)output &&
                                     end_output > (uintptr_t)limits;
        int overlap_pixels_limits = end_limits > (uintptr_t)pixels &&
                                      end_pixels > (uintptr_t)limits;

        if (!overlap_pixels_output && !overlap_limits_output &&
            !overlap_pixels_limits) {
            uint32_t width = amount & ~UINT32_C(3);

            for (; done != width; done += 4u) {
                size_t byte = (size_t)done * 4u;
                uint32_t limit0 = rd32(limits + byte);
                uint32_t limit1 = rd32(limits + byte + 4u);
                uint32_t limit2 = rd32(limits + byte + 8u);
                uint32_t limit3 = rd32(limits + byte + 12u);
                uint32_t pixel0 = rd32(pixels + byte);
                uint32_t pixel1 = rd32(pixels + byte + 4u);
                uint32_t pixel2 = rd32(pixels + byte + 8u);
                uint32_t pixel3 = rd32(pixels + byte + 12u);
                uint8_t high0 = (uint8_t)(pixel0 >> 24);
                uint8_t high1 = (uint8_t)(pixel1 >> 24);
                uint8_t high2 = (uint8_t)(pixel2 >> 24);
                uint8_t high3 = (uint8_t)(pixel3 >> 24);
                uint8_t new0 = (uint8_t)(limit0 >> 24) & UINT8_C(0x1f);
                uint8_t new1 = (uint8_t)(limit1 >> 24) & UINT8_C(0x1f);
                uint8_t new2 = (uint8_t)(limit2 >> 24) & UINT8_C(0x1f);
                uint8_t new3 = (uint8_t)(limit3 >> 24) & UINT8_C(0x1f);

                if (new0 < high0) new0 = high0;
                if (new1 < high1) new1 = high1;
                if (new2 < high2) new2 = high2;
                if (new3 < high3) new3 = high3;

                wr32(output + done,
                      (uint32_t)high0 | ((uint32_t)high1 << 8) |
                      ((uint32_t)high2 << 16) | ((uint32_t)high3 << 24));
                wr32(pixels + byte, (pixel0 & UINT32_C(0x00ffffff)) |
                      ((uint32_t)new0 << 24));
                wr32(pixels + byte + 4u, (pixel1 & UINT32_C(0x00ffffff)) |
                      ((uint32_t)new1 << 24));
                wr32(pixels + byte + 8u, (pixel2 & UINT32_C(0x00ffffff)) |
                      ((uint32_t)new2 << 24));
                wr32(pixels + byte + 12u, (pixel3 & UINT32_C(0x00ffffff)) |
                      ((uint32_t)new3 << 24));
            }

            if (done == amount)
                return;
        }
    }

    for (; done != amount; ++done) {
        size_t byte = (size_t)done * 4u;
        uint32_t pixel = rd32(pixels + byte);
        uint8_t height = (uint8_t)(pixel >> 24);
        uint8_t limit = limits[byte + 3u] & UINT8_C(0x1f);

        if (limit < height)
            limit = height;
        output[done] = height;
        wr32(pixels + byte, (pixel & UINT32_C(0x00ffffff)) |
              ((uint32_t)limit << 24));
    }
}



void gpu2d_pixel_words_set_top_flag(uint32_t *param_1, int param_2)
{

    if (param_2 == 0) {
        return;
    }

    uint32_t n = (uint32_t)param_2;

    unsigned char *dst = (unsigned char *)param_1;
    for (uint32_t i = 0; i < n; i++) {

        size_t off = (size_t)i * 4u;
        uint32_t w = rd32(dst + off);
        w |= 0x80000000u;
        wr32(dst + off, w);
    }
}

void gpu2d_fill_bytes_guarded(void *s, uint32_t c, uint32_t n) {
    if (n == 0) return;
    ((void *(*)(void *, int, uint64_t))sym_libc_memset)(s, (int)c, (uint64_t)n);
}


void gpu2d_pixel_words_fill(uint32_t *param_1, uint32_t param_2, int param_3)
{

    uint32_t n = (uint32_t)param_3;

    if (n == 0) {
        return;
    }

    unsigned char *dst = (unsigned char *)param_1;
    for (uint32_t i = 0; i < n; i++) {

        wr32(dst + (size_t)i * 4u, param_2);
    }
}

void gpu2d_line_margins_write_tag(const unsigned char *state,
                        unsigned char *dst,
                        uint32_t nlin)
{

    uint32_t left0 = rd16(state + 1760);
    unsigned char *p = dst + 3;

    if (left0 != 0) {

        uint64_t cnt = (uint64_t)(uint32_t)(left0 - 1u) + 1u;
        uint64_t pair;

        if (cnt > 1) {
            pair = cnt & 0x1fffffffeULL;
            unsigned char *q = dst + 7;
            p = p + pair * 4;
            uint64_t k = pair;
            do {
                q[-4] = 0x40;
                q[0]  = 0x40;
                q += 8;
                k -= 2;
            } while (k != 0);
            if (cnt == pair) goto L_1;
        } else {
            pair = 0;
        }

        {
            uint32_t rem = left0 - (uint32_t)pair;
            do {
                rem -= 1;
                *p = 0x40;
                p += 4;
            } while (rem != 0);
        }
    }

L_1:
    ;

    uint32_t right = rd16(state + 1762);
    uint32_t width = rd16(state + 1584);
    uint32_t nm1 = nlin - 1u;

    {
        uint32_t d = right + left0;
        d = width - d;
        d = d << 2;
        p = p + (int64_t)(int32_t)d;
    }

    if (nm1 != 0) {

        const unsigned char *e14 = state + 0x6e0;
        uint32_t i = 0;
        const unsigned char *e10 = state + 0x630;
        const unsigned char *e13 = e14;
        unsigned char *cur;
        uint32_t sig;

        e13 += 4;
        sig = rd16(e13);

        for (;;) {

            if ((uint32_t)(right + sig) == 0) {
                cur = p;
            } else {

                uint32_t tot = right + sig;

                uint64_t cnt = (uint64_t)(uint32_t)(tot - 1u) + 1u;
                uint64_t pair;

                if (cnt >= 2) {
                    pair = cnt & 0x1fffffffeULL;
                    cur = p + pair * 4;
                    unsigned char *q = p + 4;
                    uint64_t k = pair;
                    do {
                        q[-4] = 0x40;
                        q[0]  = 0x40;
                        q += 8;
                        k -= 2;
                    } while (k != 0);
                    if (cnt == pair) goto L_2;
                } else {
                    pair = 0;
                    cur = p;
                }

                {
                    uint32_t rem = tot - (uint32_t)pair;
                    do {
                        rem -= 1;
                        *cur = 0x40;
                        cur += 4;
                    } while (rem != 0);
                }
            }

        L_2:
            ;

            e10 += 4;
            uint32_t width_s = rd16(e10);

            right = rd16(e14 + 6);

            i += 1;

            {
                uint32_t d = right + sig;
                d = width_s - d;
                d = d << 2;
                p = cur + (int64_t)(int32_t)d;
            }

            e14 = e13;

            if (i == nm1) break;

            e13 += 4;
            sig = rd16(e13);
        }
    }

    if (right == 0) return;

    {

        uint64_t cnt = (uint64_t)(uint32_t)(right - 1u) + 1u;
        uint64_t pair;

        if (cnt > 1) {
            pair = cnt & 0x1fffffffeULL;
            unsigned char *q = p + 4;
            p = p + pair * 4;
            uint64_t k = pair;
            do {
                q[-4] = 0x40;
                q[0]  = 0x40;
                q += 8;
                k -= 2;
            } while (k != 0);
            if (cnt == pair) return;
        } else {
            pair = 0;
        }

        {
            uint32_t rem = right - (uint32_t)pair;
            do {
                rem -= 1;
                *p = 0x40;
                p += 4;
            } while (rem != 0);
        }
    }
}

typedef void *(*fn_memset)(void *, int, size_t);


void gpu2d_palette_ring_fill_gaps(const uint8_t *param_1, uint8_t *param_2,
                         uint32_t param_3)
{

    if (param_3 == 0)
        return;

    uint32_t pass = 0;
    uint8_t *cursor = param_2;

    do {
        uint32_t limit = rd16(param_1 + 0x630);
        uint32_t start = rd16(param_1 + 0x6e0);
        uint32_t jump = rd16(param_1 + 0x6e2);
        uint8_t *dest = cursor + start;

        if ((uint32_t)(limit - start) != jump) {
            uint32_t tmp = (uint32_t)(~start + limit);
            tmp = (uint32_t)(tmp - jump);
            uint64_t size = (uint64_t)tmp + 1u;

            ((fn_memset)sym_libc_memset)(dest, 0xff,
                                                (size_t)size);

            if (size < 2) {
                tmp = 0;
            } else {
                uint64_t even = size & UINT64_C(0x1fffffffe);
                uint64_t count = even;

                dest += even;
                do {
                    count -= 2;
                } while (count != 0);

                if (size == even) {
                    cursor = dest + jump;
                    pass++;
                    continue;
                }
                tmp = (uint32_t)even;
            }

            tmp = (uint32_t)(tmp + start);
            tmp = (uint32_t)(tmp + jump);
            tmp = (uint32_t)(tmp - limit);
            do {
                uint32_t before = tmp;
                tmp = (uint32_t)(tmp + 1);
                dest++;
                if (before == UINT32_MAX)
                    break;
            } while (1);
            cursor = dest + jump;
        } else {
            cursor = dest + jump;
        }

        pass++;
    } while (pass != param_3);
}

void gpu2d_palette_indices_to_rgba_line(unsigned char *machine, const gpu3d_t *tables,
                        const gpu3d_polygon_t *obj, uint32_t *dst,
                        const unsigned char *src, uint32_t step,
                        uint32_t height, uint32_t n) {

    if ((obj->polygon_attr & 0x30) != 0x20) {
        gpu3d_raster_planes_interleave_rgba((unsigned char *)dst, src, step, (int32_t)n, height);
        return;
    }

    const unsigned char *t0 = tables->toon_table_expanded[0];
    const unsigned char *t1 = tables->toon_table_expanded[1];
    const unsigned char *t2 = tables->toon_table_expanded[2];
    uint32_t hdr = height << 24;

    if (*recon_cfg3d(machine) & 2) {
        if (n == 0) return;
        uint64_t p2 = (uint64_t)step * 2;
        do {
            uint32_t m0 = src[0];
            uint32_t m1 = src[step];
            uint32_t m2 = src[p2];
            src += 1;

            uint64_t i = m0 >> 1;
            uint32_t c0 = t0[i] + m0;
            uint32_t c1 = m1 + t1[i];
            uint32_t c2 = m2 + t2[i];

            if (c0 >= 0x3f) c0 = 0x3f;
            if (c1 >= 0x3f) c1 = 0x3f;
            if (c2 >= 0x3f) c2 = 0x3f;

            *dst++ = c0 | hdr | (c1 << 8) | (c2 << 16);
        } while (--n != 0);
        return;
    }

    if (n == 0) return;
    do {
        uint64_t i = (uint32_t)(*src++) >> 1;
        *dst++ = hdr | t0[i] | ((uint32_t)t1[i] << 8) | ((uint32_t)t2[i] << 16);
    } while (--n != 0);
}

void gpu2d_palette_lookup_line_idx8(uint32_t *dst, const unsigned char *src,
                        const unsigned char *table, int32_t n) {
    do {
        uint16_t v[8];
        for (int k = 0; k < 8; k++) {
            uint64_t i = (uint64_t)src[k] * 2;
            v[k] = *(const uint16_t *)(table + i);
        }
        src += 8;
        n -= 8;
        for (int k = 0; k < 4; k++)
            dst[k] = (uint32_t)v[k * 2] | ((uint32_t)v[k * 2 + 1] << 16);
        dst += 4;
    } while (n > 0);
}

void gpu2d_palette_lookup_line_idx16(uint16_t *buf, const unsigned char *table, int32_t n) {
    const uint16_t *in = buf;
    uint32_t *out = (uint32_t *)buf;
    do {
        uint16_t v[8];
        for (int k = 0; k < 8; k++)
            v[k] = *(const uint16_t *)(table + (uint64_t)in[k]);
        in += 8;
        n -= 8;
        for (int k = 0; k < 4; k++)
            out[k] = (uint32_t)v[k * 2] | ((uint32_t)v[k * 2 + 1] << 16);
        out += 4;
    } while (n >= 0);
}

void gpu2d_palette_lookup_line_idx12(uint32_t *dst, const uint16_t *src,
                        const unsigned char *table, int32_t n) {
    do {
        uint16_t v[8];
        for (int k = 0; k < 8; k++) {
            uint64_t i = (uint64_t)(src[k] & 0xfff) * 2;
            v[k] = *(const uint16_t *)(table + i);
        }
        src += 8;
        n -= 8;
        for (int k = 0; k < 4; k++)
            dst[k] = (uint32_t)v[k * 2] | ((uint32_t)v[k * 2 + 1] << 16);
        dst += 4;
    } while (n > 0);
}

void gpu2d_block_delta_encode_inplace(unsigned char *buf, uint32_t off, uint32_t mark) {

    uint32_t m = (mark + 1) & 0xff;
    *(uint16_t *)(buf + off) = (uint16_t)(m | (m << 8));

    int32_t n = (int32_t)off + 2;

    const unsigned char *in = buf;
    unsigned char *out = buf;
    unsigned char prev = 0;
    unsigned char cur[16], res[16];

    for (int k = 0; k < 16; k++) cur[k] = in[k];
    in += 16;
    for (int k = 0; k < 16; k++) res[k] = (unsigned char)(cur[k] - (k ? cur[k-1] : prev));
    prev = cur[15];
    n -= 16;

    while (n > 0) {
        unsigned char sig[16];
        for (int k = 0; k < 16; k++) sig[k] = in[k];
        in += 16;
        for (int k = 0; k < 16; k++) out[k] = res[k];
        out += 16;
        for (int k = 0; k < 16; k++) res[k] = (unsigned char)(sig[k] - (k ? sig[k-1] : prev));
        prev = sig[15];
        n -= 16;
    }
    for (int k = 0; k < 16; k++) out[k] = res[k];
}

#define PALETTE_SIZE 64
#define WIDTH  16

static void table16(uint8_t *dst, const uint8_t *tab, const uint8_t *idx) {
    for (int i = 0; i < WIDTH; i++)
        dst[i] = idx[i] < PALETTE_SIZE ? tab[idx[i]] : 0;
}

void gpu2d_palette_apply_lut64_lines(uint8_t *dest, const uint8_t *origin,
                        const uint8_t *counts, const uint8_t *indices,
                        int lines, const uint8_t *palettes)
{

    uint32_t first_row = counts[0];

    if (first_row == 0) {

        const uint8_t *tab = palettes + ((uint32_t)indices[0] << 6 & 0x3fc0u);
        for (int n = 0; n < 256; n += 32) {
            table16(dest, tab, origin);
            table16(dest + 16, tab, origin + 16);
            dest += 32; origin += 32;
        }
        return;
    }

    int left = lines - 1;
    if (left == 0) goto tail;

    for (;;) {
        uint16_t c, k;
        memcpy(&c, counts, 2); counts += 2;
        memcpy(&k, indices, 2); indices += 2;

        const uint8_t *tab_a = palettes + (((uint32_t)k << 6) & 0x3fc0u);
        const uint8_t *tab_b = palettes + (((uint32_t)k >> 2) & 0x3fc0u);
        int64_t len_a = c & 0xff;
        int64_t len_b = c >> 8;

        if ((c & 0xf0f0f0f0u) == 0) {

            table16(dest, tab_a, origin);
            origin += len_a; dest += len_a;
            table16(dest, tab_b, origin);
            origin += len_b; dest += len_b;
            left -= 2;
            if (left > 0) continue;
            if (left == 0) goto tail;
            return;
        }

        int64_t r = len_a;
        while (r > 0) {
            table16(dest, tab_a, origin);
            r -= 16; origin += 16; dest += 16;
        }
        origin += (int32_t)r;
        dest += (int32_t)r;

        r = len_b;
        while (r > 0) {
            table16(dest, tab_b, origin);
            r -= 16; origin += 16; dest += 16;
        }
        left -= 2;
        origin += (int32_t)r;
        dest += (int32_t)r;

        if (left > 0) continue;
        if (left != 0) return;
        goto tail;
    }

tail:
    {

        const uint8_t *tab = palettes + (((uint32_t)indices[0] << 6) & 0x3fc0u);
        int32_t r = (int32_t)counts[0];
        while (r > 0) {
            table16(dest, tab, origin);
            r -= 16; origin += 16; dest += 16;
        }
    }
}
#undef PALETTE_SIZE
#undef WIDTH

void gpu2d_palette_translate_indices8_pack(uint16_t *buf, const unsigned char *table, int32_t n) {
    const uint16_t *in = buf;
    uint32_t *out = (uint32_t *)buf;
    do {
        unsigned char v[8];
        for (int k = 0; k < 8; k++)
            v[k] = table[in[k]];
        in += 8;
        n -= 8;
        for (int g = 0; g < 2; g++)
            out[g] = (uint32_t)v[g * 4]
                   | ((uint32_t)v[g * 4 + 1] << 8)
                   | ((uint32_t)v[g * 4 + 2] << 16)
                   | ((uint32_t)v[g * 4 + 3] << 24);
        out += 2;
    } while (n >= 0);
}

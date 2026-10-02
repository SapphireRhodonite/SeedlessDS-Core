#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "core/nds_state.h"
#include "blob_symbols.h"
#include "core_internals.h"
#include "mem_access.h"





void spu_resample_build_float_pairs(float *output, const void *table,
                        const uint8_t *counts, uint32_t count,
                        int32_t factor)
{

    uint8_t counter = counts[0];
    const uint8_t *entries = (const uint8_t *)table;
    uint8_t *dest = (uint8_t *)output;

    if (counter != 0) {
        const uint8_t *left = rd_ptr_u8(entries);
        const uint8_t *right = rd_ptr_u8(entries + 8);
        uint32_t first = rd32(left);
        uint32_t second = rd32(right);
        uint16_t half_right = rd16(right + 6);
        uint16_t half_left = rd16(left + 6);
        uint32_t delta_16 = (uint32_t)half_right -
                            (uint32_t)half_left;
        uint32_t delta_32 = first - second;
        uint32_t product = delta_32 * (uint32_t)factor;
        float value_a = (float)factor * (float)(int32_t)first;
        float step_a = (float)(int32_t)first;
        float step_b = (float)(int32_t)delta_32;

        volatile float product_rounded =
            (float)second * (float)(int32_t)delta_16;
        float value_b = product_rounded + (float)(int32_t)product;

        do {
            counter--;
            wr_f32(dest, value_a);
            wr_f32(dest + 4, value_b);
            dest += 8;
            value_a = value_a + step_a;
            value_b = value_b + step_b;
        } while ((int32_t)counter > 0);
    }

    if (count >= 2) {
        uint64_t idx = 1;
        const uint8_t *cursor = entries + 16;
        const uint8_t *left = rd_ptr_u8(cursor);

        counter = counts[idx];
        for (;;) {
            if (counter != 0) {
                const uint8_t *right = rd_ptr_u8(cursor + 8);
                uint32_t first = rd32(left);
                uint16_t half_left = rd16(left + 6);
                uint32_t second = rd32(right);
                uint16_t half_right = rd16(right + 6);
                uint32_t delta_16 = (uint32_t)half_right -
                                    (uint32_t)half_left;
                uint32_t delta_32 = first - second;
                float value_a = 0.0f;
                float value_b = (float)second * (float)(int32_t)delta_16;
                float step_a = (float)(int32_t)first;
                float step_b = (float)(int32_t)delta_32;

                do {
                    counter--;
                    wr_f32(dest, value_a);
                    wr_f32(dest + 4, value_b);
                    dest += 8;
                    value_a = value_a + step_a;
                    value_b = value_b + step_b;
                } while ((int32_t)counter > 0);
            }

            idx++;
            if (idx == (uint64_t)count)
                break;
            cursor += 16;
            left = rd_ptr_u8(cursor);
            counter = counts[idx];
        }
    }
}



static uint16_t q15_ratio_via_i64(float numerator, float denominator)
{
    double q = ((double)numerator * 32768.0) / (double)denominator;

    if (q != q || q < -0x1p63 || q >= 0x1p63)
        return 0;
    return (uint16_t)(int64_t)q;
}

static uint16_t q15_ratio_via_i32(float numerator, float denominator)
{
    double q = ((double)numerator * 32768.0) / (double)denominator;

    if (q != q || q < -0x1p31 || q >= 0x1p31)
        return 0;
    return (uint16_t)(int32_t)q;
}

void spu_resample_float_pairs_to_i16(void *param_1, const void *param_2, uint32_t param_3)
{

    uint8_t *output = (uint8_t *)param_1;
    const uint8_t *entry = (const uint8_t *)param_2;
    uint64_t groups = (uint64_t)(param_3 & ~UINT32_C(3));
    uint64_t i;

    if (param_3 == 0)
        return;

    for (i = 0; i < groups; i += 4) {
        float n0 = rd_f32(entry + (uint64_t)i * 8 + 0);
        float d0 = rd_f32(entry + (uint64_t)i * 8 + 4);
        float n1 = rd_f32(entry + (uint64_t)i * 8 + 8);
        float d1 = rd_f32(entry + (uint64_t)i * 8 + 12);
        float n2 = rd_f32(entry + (uint64_t)i * 8 + 16);
        float d2 = rd_f32(entry + (uint64_t)i * 8 + 20);
        float n3 = rd_f32(entry + (uint64_t)i * 8 + 24);
        float d3 = rd_f32(entry + (uint64_t)i * 8 + 28);

        wr16(output + (uint64_t)i * 2 + 0, q15_ratio_via_i64(n0, d0));
        wr16(output + (uint64_t)i * 2 + 2, q15_ratio_via_i64(n1, d1));
        wr16(output + (uint64_t)i * 2 + 4, q15_ratio_via_i64(n2, d2));
        wr16(output + (uint64_t)i * 2 + 6, q15_ratio_via_i64(n3, d3));
    }

    for (; i < (uint64_t)param_3; ++i) {
        float numerator = rd_f32(entry + (uint64_t)i * 8);
        float denominator = rd_f32(entry + (uint64_t)i * 8 + 4);
        wr16(output + (uint64_t)i * 2,
                    q15_ratio_via_i32(numerator, denominator));
    }
}




static uint32_t scale_sample(int16_t sample, int64_t delta, uint32_t initial)
{
    uint64_t product = (uint64_t)(int64_t)sample * (uint64_t)delta;
    return initial + (uint32_t)(product >> 15);
}

void spu_resample_interpolate_blocks_i16_to_i32(const void *table, void *output,
                         const void *samples, const void *weights,
                         uint32_t count)
{

    const uint8_t *table_bytes = (const uint8_t *)table;
    uint8_t *dest = (uint8_t *)output;
    const uint8_t *start_samples = (const uint8_t *)samples;
    const uint8_t *cursor_samples = start_samples;
    const uint8_t *list_weights = (const uint8_t *)weights;
    uint32_t idx;

    if (count == 0)
        return;

    for (idx = 0; idx < count; ++idx) {
        uint8_t weight = list_weights[idx];
        uint32_t entry_table;
        const uint8_t *pointer_initial;
        const uint8_t *pointer_final;
        uint32_t initial;
        uint32_t final;
        int64_t delta;
        uint32_t groups;
        uint32_t j;

        if (weight == 0) {
            cursor_samples = start_samples;
            continue;
        }

        entry_table = idx << 1;
        pointer_initial = rd_ptr_u8(table_bytes + (size_t)entry_table * 8u);
        pointer_final = rd_ptr_u8(table_bytes + (size_t)(entry_table | 1u) * 8u);
        initial = rd32(pointer_initial);
        final = rd32(pointer_final);
        delta = (int64_t)(int32_t)(final - initial);

        groups = (uint32_t)weight & UINT32_C(0xfffffff8);
        for (j = 0; j < groups; j += 8) {
            int16_t m0 = rd16s(cursor_samples + 0);
            int16_t m1 = rd16s(cursor_samples + 2);
            int16_t m2 = rd16s(cursor_samples + 4);
            int16_t m3 = rd16s(cursor_samples + 6);
            int16_t m4 = rd16s(cursor_samples + 8);
            int16_t m5 = rd16s(cursor_samples + 10);
            int16_t m6 = rd16s(cursor_samples + 12);
            int16_t m7 = rd16s(cursor_samples + 14);

            wr32(dest + 0, scale_sample(m0, delta, initial));
            wr32(dest + 4, scale_sample(m1, delta, initial));
            wr32(dest + 8, scale_sample(m2, delta, initial));
            wr32(dest + 12, scale_sample(m3, delta, initial));
            wr32(dest + 16, scale_sample(m4, delta, initial));
            wr32(dest + 20, scale_sample(m5, delta, initial));
            wr32(dest + 24, scale_sample(m6, delta, initial));
            wr32(dest + 28, scale_sample(m7, delta, initial));
            cursor_samples += 16;
            dest += 32;
        }

        for (j = groups; j < (uint32_t)weight; ++j) {
            int16_t sample = rd16s(cursor_samples);
            cursor_samples += 2;
            wr32(dest, scale_sample(sample, delta, initial));
            dest += 4;
        }
    }
}


static int32_t sext16(uint16_t v)
{
    return (v & 0x8000u) ? (int32_t)v - 0x10000 : (int32_t)v;
}

static uint32_t madd32(uint32_t a, uint32_t b, uint32_t c)
{
    return a * b + c;
}

void spu_resample_interpolate_series_i16(uint8_t *table, uint8_t *output,
                        uint8_t *samples, uint8_t *lens,
                        uint32_t count)
{

    if (count != 0) {

        uint8_t *zone_a = output + 4u * RECON_3D_ROW_ARRAY;
        uint8_t *zone_b = output + 6u * RECON_3D_ROW_ARRAY;
        uint8_t *zone_c = output + 8u * RECON_3D_ROW_ARRAY;

        for (uint64_t i = 0; i != (uint64_t)count; i++) {
            uint32_t index_pair = (uint32_t)i << 1;
            uint8_t *first = rd_ptr(table + (uint64_t)index_pair * 8u);
            uint8_t *second = rd_ptr(table + (uint64_t)(index_pair | 1u) * 8u);
            uint32_t left;
            uint32_t color_first;
            uint32_t color_second;
            int32_t base_x;
            int32_t base_y;
            int32_t delta_x;
            int32_t delta_y;
            uint32_t fixed_x;
            uint32_t fixed_y;
            uint32_t base_red;
            uint32_t base_green;
            uint32_t base_blue;
            uint32_t delta_red;
            uint32_t delta_green;
            uint32_t delta_blue;

            left = rd8(lens + i);
            color_first = gpu3d_geometry_color_expand_bgr555_to_rgb8(rd16(first + 10));
            color_second = gpu3d_geometry_color_expand_bgr555_to_rgb8(rd16(second + 10));

            base_x = sext16(rd16(first + 12));
            base_y = sext16(rd16(first + 14));
            delta_x = sext16((uint16_t)(rd16(second + 12) - rd16(first + 12)));
            delta_y = sext16((uint16_t)(rd16(second + 14) - rd16(first + 14)));
            fixed_x = ((uint32_t)base_x << 15) | (delta_x > 0 ? 0x800u : 0u);
            fixed_y = ((uint32_t)base_y << 15) | (delta_y > 0 ? 0x800u : 0u);

            if (left != 0) {
                uint32_t first_red = (color_first >> 16) & 0x3fu;
                uint32_t first_green = (color_first >> 8) & 0x3fu;
                uint32_t first_blue = color_first & 0x3fu;
                uint32_t second_red = (color_second >> 16) & 0x3fu;
                uint32_t second_green = (color_second >> 8) & 0x3fu;
                uint32_t second_blue = color_second & 0x3fu;

                base_red = (first_red << 18) | 0x38000u;
                base_green = (first_green << 18) | 0x38000u;
                base_blue = (first_blue << 18) | 0x38000u;
                delta_red = (second_red - first_red) << 3;
                delta_green = (second_green - first_green) << 3;
                delta_blue = (second_blue - first_blue) << 3;

                do {
                    uint32_t sample = (uint32_t)sext16(rd16(samples));
                    uint32_t value_x;
                    uint32_t value_y;
                    uint32_t value_blue;
                    uint32_t value_green;
                    uint32_t value_red;

                    samples += 2;
                    left--;
                    value_x = madd32(sample, (uint32_t)delta_x, fixed_x) >> 15;
                    wr16(zone_a, (uint16_t)value_x);
                    value_y = madd32(sample, (uint32_t)delta_y, fixed_y) >> 15;
                    wr16(zone_a + 2, (uint16_t)value_y);
                    value_blue = madd32(sample, delta_blue, base_blue) >> 15;
                    wr16(zone_b, (uint16_t)value_blue);
                    value_green = madd32(sample, delta_green, base_green) >> 15;
                    value_red = madd32(sample, delta_red, base_red) >> 15;
                    wr16(zone_b + 2, (uint16_t)value_green);
                    wr16(zone_c + 2, (uint16_t)value_red);
                    zone_a += 4;
                    zone_b += 4;
                    zone_c += 4;
                } while ((int32_t)left > 0);
            }
        }
    }
}

void spu_resample_clear_buffer(void *p, uint32_t n) {
    uint32_t d = n << 1;
    if (d == 0) return;
    d -= 2;
    d >>= 1;
    uint64_t size = ((uint64_t)d << 3) + 8;
    ((void *(*)(void *, int, uint64_t))sym_libc_memset)(p, 0, size);
}


static int saturate_i16(int v)
{
    if (v > 32767) {
        v = 32767;
    }
    if (v < -32768) {
        v = -32768;
    }
    return v;
}

void spu_resample_s32_to_s16(void *param_1, const void *param_2, unsigned int param_3)
{

    unsigned char *dst = (unsigned char *)param_1;
    const unsigned char *src = (const unsigned char *)param_2;
    unsigned long processed;

    if (param_3 == 0) {
        return;
    }

    if (param_3 < 4) {
        processed = 0;
    } else {
        unsigned long total = (unsigned long)param_3;
        unsigned long groups = total & 0xfffffffcUL;
        unsigned long remaining_simd = groups;
        const unsigned char *s = src;
        unsigned char *d = dst;

        do {
            int v0 = rd32s(s + 0);
            int v1 = rd32s(s + 4);
            int v2 = rd32s(s + 8);
            int v3 = rd32s(s + 12);
            s += 16;
            remaining_simd -= 4;

            int r0 = saturate_i16(v0 >> 12);
            int r1 = saturate_i16(v1 >> 12);
            int r2 = saturate_i16(v2 >> 12);
            int r3 = saturate_i16(v3 >> 12);

            wr16(d + 0, (short)r0);
            wr16(d + 2, (short)r1);
            wr16(d + 4, (short)r2);
            wr16(d + 6, (short)r3);
            d += 8;
        } while (remaining_simd != 0);

        processed = groups;
        if (groups == total) {
            return;
        }
    }

    {
        unsigned long remaining = (unsigned long)param_3 - processed;
        const unsigned char *s = src + processed * 4;
        unsigned char *d = dst + processed * 2;

        do {
            int v = rd32s(s);
            s += 4;
            int r = saturate_i16(v >> 12);
            wr16(d, (short)r);
            d += 2;
            remaining -= 1;
        } while (remaining != 0);
    }
}

#define MAGIC      0xff90ecc69f727e51ULL

static uint32_t freq_of(uint32_t v) {
    uint64_t x = (uint64_t)v << 22;
    uint64_t height = (uint64_t)(((unsigned __int128)x * MAGIC) >> 64);
    return (uint32_t)(height >> 26);
}

unsigned spu_resample_sample_by_time(spu_t *spu, uint64_t elapsed) {
    spu_recording_t *a = &spu->recording;
    if (a->active != 0) {
        uint8_t *data = a->data;
        if (data == 0)
            return 0;
        uint64_t base = a->start_cycles;
        uint32_t freq = a->frequency;
        int64_t t = (int64_t)(elapsed - base) << 10;
        uint64_t norm = (uint64_t)(t & ~(t >> 63));
        if (freq == 0) {
            freq = freq_of(a->rate);
            a->frequency = freq;
        }
        uint32_t cap = a->sample_count;
        uint64_t idx = (norm * (uint64_t)freq) >> 32;
        if (cap <= (uint32_t)idx) return 0;
        uint16_t m;
        __builtin_memcpy(&m, data + idx * 2, 2);
        return m;
    }
    if (spu->mixer.mic_loaded == 0) return 0;
    uint32_t freq = a->mic_frequency;
    if (freq == 0) {
        freq = freq_of(spu->mixer.mic_rate);
        a->mic_frequency = freq;
    }
    uint32_t base32 = (uint32_t)a->mic_time_base;
    uint32_t step = spu->mixer.mic_sample_bytes;
    int32_t t = (int32_t)(((uint32_t)elapsed - base32) << 10);
    uint32_t norm = (uint32_t)(t & ~(t >> 31));
    uint64_t idx = ((uint64_t)norm * freq) >> 32;
    uint32_t off = (uint32_t)idx * step;
    uint16_t m;
    __builtin_memcpy(&m, spu->mic_buffer + (uint64_t)off * 2, 2);
    return m;
}
#undef MAGIC

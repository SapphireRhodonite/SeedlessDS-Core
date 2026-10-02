#include "core_internals.h"
#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "gpu3d.h"
#include <stddef.h>
#include "mem_access.h"



static uint64_t recon_162978_dot3(int64_t c0, int64_t c1, int64_t c2,
                                  int64_t a, int64_t b, int64_t c)
{
    return (uint64_t)c0 * (uint64_t)a +
           (uint64_t)c1 * (uint64_t)b +
           (uint64_t)c2 * (uint64_t)c;
}

void gpu3d_geometry_matrix_mul_q12(unsigned char *out, const unsigned char *in,
                        const unsigned char *coef)
{

    int64_t v[4][4];
    int64_t k[12];
    uint32_t r[4][4];
    unsigned int lane;
    unsigned int row;

    for (row = 0; row != 4; ++row) {
        for (lane = 0; lane != 4; ++lane)
            v[row][lane] = rd32s_at(in, (uint64_t)(row * 16u + lane * 4u));
    }

    for (row = 0; row != 12; ++row)
        k[row] = rd32s_at(coef, (uint64_t)row * 4u);

    for (row = 0; row != 3; ++row) {
        for (lane = 0; lane != 4; ++lane) {
            uint64_t sum = recon_162978_dot3(k[row * 3u],
                                               k[row * 3u + 1u],
                                               k[row * 3u + 2u],
                                               v[0][lane], v[1][lane], v[2][lane]);

            r[row][lane] = (uint32_t)(sum >> 12);
        }
    }

    for (lane = 0; lane != 4; ++lane) {
        uint64_t sum = recon_162978_dot3(k[9], k[10], k[11],
                                           v[0][lane], v[1][lane], v[2][lane]);
        uint32_t q12 = (uint32_t)(sum >> 12);

        r[3][lane] = (uint32_t)((uint64_t)(uint32_t)v[3][lane] + q12);
    }

    for (lane = 0; lane != 4; ++lane) {
        wr32_at(out, (uint64_t)lane * 4u, r[0][lane]);
        wr32_at(out, 16u + (uint64_t)lane * 4u, r[1][lane]);
        wr32_at(out, 32u + (uint64_t)lane * 4u, r[2][lane]);
        wr32_at(out, 48u + (uint64_t)lane * 4u, r[3][lane]);
    }
}


void gpu3d_geometry_project_point_xy(gpu3d_t *ctx, int32_t param_2, int32_t param_3) {

    int32_t a00, a01, a10, a11, a20, a21, a30, a31;
    memcpy(&a00, &ctx->texture_matrix[0], 4);
    memcpy(&a01, &ctx->texture_matrix[1], 4);
    memcpy(&a10, &ctx->texture_matrix[4], 4);
    memcpy(&a11, &ctx->texture_matrix[5], 4);

    uint32_t s0 = (uint32_t)(a00 * param_2);
    uint32_t s1 = (uint32_t)(a01 * param_2);
    s0 += (uint32_t)(a10 * param_3);
    s1 += (uint32_t)(a11 * param_3);

    memcpy(&a20, &ctx->texture_matrix[8], 4);
    memcpy(&a21, &ctx->texture_matrix[9], 4);
    s0 += (uint32_t)a20;
    s1 += (uint32_t)a21;

    memcpy(&a30, &ctx->texture_matrix[12], 4);
    memcpy(&a31, &ctx->texture_matrix[13], 4);
    s0 += (uint32_t)a30;
    s1 += (uint32_t)a31;

    s0 >>= 12;
    s1 >>= 12;

    int16_t out0 = (int16_t)s0;
    int16_t out1 = (int16_t)s1;
    memcpy(&ctx->texcoord[0], &out0, 2);
    memcpy(&ctx->texcoord[1], &out1, 2);
}

void gpu3d_geometry_poly_winding_sign(const unsigned char *p, uint32_t idx, unsigned char *out)
{

    int64_t a = rd32s_at(p, 0);
    int64_t b = rd32s_at(p, 4);
    int64_t d = rd32s_at(p, 12);
    int64_t e = rd32s_at(p, 16);
    int64_t f = rd32s_at(p, 20);

    uint32_t idx1 = idx + 1u;
    uint32_t idx3 = idx + 3u;
    int64_t u = rd32s_at(p, (uint64_t)idx  * 4u);
    int64_t v = rd32s_at(p, (uint64_t)idx1 * 4u);
    int64_t w = rd32s_at(p, (uint64_t)idx3 * 4u);

    int64_t h = rd32s_at(p, 28);

    uint64_t A = (uint64_t)f * (uint64_t)a - (uint64_t)e * (uint64_t)b;
    uint64_t B = (uint64_t)v * (uint64_t)e - (uint64_t)u * (uint64_t)f;
    uint64_t C = (uint64_t)v * (uint64_t)a - (uint64_t)u * (uint64_t)b;

    uint64_t A_lo = A & 0xffffffffu;
    int64_t  A_hi = (int64_t)A >> 32;
    uint64_t B_lo = B & 0xffffffffu;
    int64_t  B_hi = (int64_t)B >> 32;
    uint64_t C_lo = C & 0xffffffffu;
    int64_t  C_hi = (int64_t)C >> 32;

    uint64_t lo = A_lo * (uint64_t)w;
    uint64_t hi = (uint64_t)A_hi * (uint64_t)w;
    lo += B_lo * (uint64_t)d;
    hi += (uint64_t)B_hi * (uint64_t)d;
    lo -= C_lo * (uint64_t)h;
    hi -= (uint64_t)C_hi * (uint64_t)h;

    uint64_t res = hi + (uint64_t)((int64_t)lo >> 32);

    unsigned char t = 2;
    if (res == 0 && (uint32_t)lo == 0)
        t = 3;
    if ((int64_t)res < 0)
        t = 1;

    *out = t;
}


void gpu3d_geometry_poly_winding_sign_batch(const unsigned char *p, uint32_t cnt, uint32_t step,
                        int32_t base, unsigned char *out)
{

    if (cnt == 0)
        return;

    uint32_t idx = ((uint32_t)base + 0x10u) >> 2;

    uint64_t advance = (uint64_t)(step & 0xfffffffcu);

    uint64_t shift = (uint64_t)idx * 4u;

    const uint32_t two = 2;

    do {

        int64_t a = rd32s_at(p, 0);
        int64_t b = rd32s_at(p, 4);
        int64_t d = rd32s_at(p, 12);
        int64_t e = rd32s_at(p, 16);
        int64_t f = rd32s_at(p, 20);

        int64_t u = rd32s_at(p, shift);
        int64_t v = rd32s_at(p, shift + 4);
        int64_t w = rd32s_at(p, shift + 12);

        int64_t h = rd32s_at(p, 28);

        uint64_t A = (uint64_t)f * (uint64_t)a - (uint64_t)e * (uint64_t)b;
        uint64_t B = (uint64_t)v * (uint64_t)e - (uint64_t)u * (uint64_t)f;
        uint64_t C = (uint64_t)v * (uint64_t)a - (uint64_t)u * (uint64_t)b;

        uint64_t A_lo = A & 0xffffffffu;
        int64_t  A_hi = (int64_t)A >> 32;
        uint64_t B_lo = B & 0xffffffffu;
        int64_t  B_hi = (int64_t)B >> 32;
        uint64_t C_lo = C & 0xffffffffu;
        int64_t  C_hi = (int64_t)C >> 32;

        uint64_t lo = A_lo * (uint64_t)w;
        uint64_t hi = (uint64_t)A_hi * (uint64_t)w;
        lo += B_lo * (uint64_t)d;
        hi += (uint64_t)B_hi * (uint64_t)d;
        lo -= C_lo * (uint64_t)h;
        hi -= (uint64_t)C_hi * (uint64_t)h;

        uint64_t res = hi + (uint64_t)((int64_t)lo >> 32);

        uint32_t t = two;
        if (res == 0 && (uint32_t)lo == 0)
            t = two + 1;
        if ((int64_t)res < 0)
            t = 1;

        cnt = cnt - 1u;

        *out = (unsigned char)t;
        out = out + 1;

        p = p + advance;

    } while (cnt != 0);
}


static void read4x32(uint32_t v[4], const unsigned char *p)
{
    memcpy(v, p, 16);
}

static void write4x32(unsigned char *p, const uint32_t v[4])
{
    memcpy(p, v, 16);
}


static uint64_t sign_extend32(uint32_t v)
{
    if ((v & UINT32_C(0x80000000)) != 0)
        return UINT64_C(0xffffffff00000000) | v;
    return v;
}

static int64_t counter_ushl64(int64_t offset)
{
    uint8_t low = (uint8_t)(uint64_t)offset;

    if ((low & UINT8_C(0x80)) != 0)
        return (int64_t)low - 0x100;
    return (int64_t)low;
}

static uint64_t ushl64(uint64_t v, int64_t offset)
{
    int64_t count = counter_ushl64(offset);

    if (count >= 64 || count <= -64)
        return 0;
    if (count >= 0)
        return v << (unsigned int)count;
    return v >> (unsigned int)(-count);
}

static uint32_t sum32(uint32_t a, uint32_t b)
{
    return a + b;
}

void gpu3d_geometry_scale_screen_params_lores(void *param_1, const void *param_2, const void *param_3)
{

    gpu3d_t *ctx = (gpu3d_t *)param_1;
    const unsigned char *entry = (const unsigned char *)param_2;
    const unsigned char *shift = (const unsigned char *)param_3;
    uint32_t count = ctx->emitted_count;
    uint32_t factor_a;
    uint32_t factor_b;
    uint32_t base_a;
    uint32_t base_b;
    uint32_t done = 0;

    if (count == 0)
        return;

    factor_b = ctx->viewport_size[1];
    base_a = ctx->viewport_origin[0];
    factor_a = ctx->viewport_size[0];
    base_b = UINT32_C(0xc0) - (factor_b + ctx->viewport_origin[1]);

    if (count >= 4) {
        uintptr_t start = (uintptr_t)(unsigned char *)ctx->screen_x;
        uintptr_t end = (uintptr_t)(unsigned char *)(ctx->screen_w + count);
        uintptr_t end_entry = (uintptr_t)entry + (uint64_t)count * 4;
        uintptr_t end_shift = (uintptr_t)shift + (uint64_t)count * 4;
        uint32_t vectorized = count & UINT32_C(0xfffffffc);

        if (!((start < end_shift && end > (uintptr_t)shift) ||
              (start < end_entry && end > (uintptr_t)entry))) {
            uint32_t i;

            for (i = 0; i < vectorized; i += 4) {
                uint32_t a[4], b[4], c[4], d[4], in[4], sh[4];
                uint32_t outside_a[4], outside_b[4], outside_c[4];
                unsigned int lane;

                read4x32(a, (unsigned char *)ctx->screen_x + i);
                read4x32(d, (unsigned char *)ctx->screen_w + i);
                read4x32(b, (unsigned char *)ctx->screen_y + i);
                read4x32(c, (unsigned char *)ctx->screen_z + i);
                read4x32(sh, shift + i);
                read4x32(in, entry + i);

                for (lane = 0; lane != 4; ++lane) {
                    uint64_t product_a;
                    uint64_t product_b;
                    uint64_t product_c;
                    uint64_t rounded;
                    int64_t shift_a;
                    int64_t shift_c;

                    product_a = sign_extend32(sum32(d[lane], a[lane]));
                    product_a *= factor_a;
                    product_a *= in[lane];

                    product_b = sign_extend32(d[lane] - b[lane]);
                    product_b *= factor_b;
                    product_b *= in[lane];

                    product_c = sign_extend32(sum32(d[lane], c[lane]));
                    product_c *= in[lane];
                    rounded = product_c + UINT64_C(0x7fff);
                    product_c -= rounded >> 15;

                    shift_a = -(int64_t)(uint64_t)(UINT32_C(0x3f) - sh[lane]);
                    shift_c = -(int64_t)(uint64_t)(UINT32_C(0x30) - sh[lane]);
                    outside_a[lane] = (uint32_t)ushl64(product_a, shift_a) + base_a;
                    outside_b[lane] = (uint32_t)ushl64(product_b, shift_a) + base_b;
                    outside_c[lane] = (uint32_t)ushl64(product_c, shift_c);
                }

                write4x32((unsigned char *)ctx->screen_x + i, outside_a);
                write4x32((unsigned char *)ctx->screen_y + i, outside_b);
                write4x32((unsigned char *)ctx->screen_z + i, outside_c);
            }
            done = vectorized;
        }
    }

    while (done != count) {
        uint64_t a = sign_extend32(rd32((unsigned char *)ctx->screen_x + done * 4));
        uint64_t b = sign_extend32(rd32((unsigned char *)ctx->screen_y + done * 4));
        uint64_t c = sign_extend32(rd32((unsigned char *)ctx->screen_z + done * 4));
        uint64_t d = sign_extend32(rd32((unsigned char *)ctx->screen_w + done * 4));
        uint32_t sh = rd32(shift + done * 4);
        uint32_t in = rd32(entry + done * 4);
        uint64_t product_a;
        uint64_t product_b;
        uint64_t product_c;
        uint64_t rounded;

        product_a = (d + a) * factor_a;
        product_b = (d - b) * factor_b;
        product_c = (d + c) * in;
        product_a *= in;
        product_b *= in;
        rounded = product_c + UINT64_C(0x7fff);
        product_c -= rounded >> 15;

        product_a >>= ((UINT32_C(0x3f) - sh) & 63);
        product_b >>= ((UINT32_C(0x3f) - sh) & 63);
        product_c >>= ((UINT32_C(0x30) - sh) & 63);

        wr32((unsigned char *)ctx->screen_x + done * 4, (uint32_t)product_a + base_a);
        wr32((unsigned char *)ctx->screen_y + done * 4, (uint32_t)product_b + base_b);
        wr32((unsigned char *)ctx->screen_z + done * 4, (uint32_t)product_c);
        ++done;
    }
}


static void read4x32_5(uint32_t v[4], const unsigned char *p)
{
    memcpy(v, p, 16);
}

static void write4x32_5(unsigned char *p, const uint32_t v[4])
{
    memcpy(p, v, 16);
}


static uint64_t sign_extend32_5(uint32_t v)
{
    if ((v & UINT32_C(0x80000000)) != 0)
        return UINT64_C(0xffffffff00000000) | v;
    return v;
}

static uint64_t ushl64_5(uint64_t v, int64_t offset)
{
    int8_t byte_low = (int8_t)(offset & 0xff);
    if (byte_low >= 64 || byte_low <= -64)
        return 0;
    if (byte_low >= 0)
        return v << (unsigned int)byte_low;
    return v >> (unsigned int)(-byte_low);
}

static uint32_t sum32_5(uint32_t a, uint32_t b)
{
    return a + b;
}

void gpu3d_geometry_scale_screen_params_hires(void *param_1, const void *param_2, const void *param_3)
{

    gpu3d_t *ctx = (gpu3d_t *)param_1;
    const unsigned char *entry = (const unsigned char *)param_2;
    const unsigned char *shift = (const unsigned char *)param_3;
    uint32_t count = ctx->emitted_count;
    uint32_t factor_a;
    uint32_t factor_b;
    uint32_t base_a;
    uint32_t base_b;
    uint32_t done = 0;

    if (count == 0)
        return;

    factor_b = ctx->viewport_size[1];
    base_a = ctx->viewport_origin[0] * recon_scale_geometry;
    factor_a = ctx->viewport_size[0];
    base_b = (UINT32_C(0xc0) - (factor_b + ctx->viewport_origin[1])) * recon_scale_geometry;

    if (count >= 4) {
        uintptr_t start = (uintptr_t)(unsigned char *)ctx->screen_x;
        uintptr_t end = (uintptr_t)(unsigned char *)(ctx->screen_w + count);
        uintptr_t end_entry = (uintptr_t)entry + (uint64_t)count * 4;
        uintptr_t end_shift = (uintptr_t)shift + (uint64_t)count * 4;
        uint32_t vectorized = count & UINT32_C(0xfffffffc);

        if (!((start < end_shift && end > (uintptr_t)shift) ||
              (start < end_entry && end > (uintptr_t)entry))) {
            uint32_t i;

            for (i = 0; i < vectorized; i += 4) {
                uint32_t a[4], b[4], c[4], d[4], in[4], sh[4];
                uint32_t outside_a[4], outside_b[4], outside_c[4];
                unsigned int lane;

                read4x32_5(a, (unsigned char *)ctx->screen_x + i);
                read4x32_5(d, (unsigned char *)ctx->screen_w + i);
                read4x32_5(b, (unsigned char *)ctx->screen_y + i);
                read4x32_5(c, (unsigned char *)ctx->screen_z + i);
                read4x32_5(sh, shift + i);
                read4x32_5(in, entry + i);

                for (lane = 0; lane != 4; ++lane) {
                    uint64_t product_a;
                    uint64_t product_b;
                    uint64_t product_c;
                    uint64_t rounded;
                    int64_t shift_a;
                    int64_t shift_c;

                    product_a = sign_extend32_5(sum32_5(d[lane], a[lane]));
                    product_a *= factor_a;
                    product_a *= in[lane];

                    product_b = sign_extend32_5(d[lane] - b[lane]);
                    product_b *= factor_b;
                    product_b *= in[lane];

                    product_c = sign_extend32_5(sum32_5(d[lane], c[lane]));
                    product_c *= in[lane];
                    rounded = product_c + UINT64_C(0x7fff);
                    product_c -= rounded >> 15;

                    shift_a = -(int64_t)(uint64_t)(RECON_3D_SHIFT_GEOMETRY - sh[lane]);
                    shift_c = -(int64_t)(uint64_t)(UINT32_C(0x30) - sh[lane]);
                    outside_a[lane] = (uint32_t)ushl64_5(product_a, shift_a) + base_a;
                    outside_b[lane] = (uint32_t)ushl64_5(product_b, shift_a) + base_b;
                    outside_c[lane] = (uint32_t)ushl64_5(product_c, shift_c);
                }

                write4x32_5((unsigned char *)ctx->screen_x + i, outside_a);
                write4x32_5((unsigned char *)ctx->screen_y + i, outside_b);
                write4x32_5((unsigned char *)ctx->screen_z + i, outside_c);
            }
            done = vectorized;
        }
    }

    while (done != count) {
        uint64_t a = sign_extend32_5(rd32((unsigned char *)ctx->screen_x + done * 4));
        uint64_t b = sign_extend32_5(rd32((unsigned char *)ctx->screen_y + done * 4));
        uint64_t c = sign_extend32_5(rd32((unsigned char *)ctx->screen_z + done * 4));
        uint64_t d = sign_extend32_5(rd32((unsigned char *)ctx->screen_w + done * 4));
        uint32_t sh = rd32(shift + done * 4);
        uint32_t in = rd32(entry + done * 4);
        uint64_t product_a;
        uint64_t product_b;
        uint64_t product_c;
        uint64_t rounded;

        product_a = (d + a) * factor_a;
        product_b = (d - b) * factor_b;
        product_c = (d + c) * in;
        product_a *= in;
        product_b *= in;
        rounded = product_c + UINT64_C(0x7fff);
        product_c -= rounded >> 15;

        product_a >>= ((RECON_3D_SHIFT_GEOMETRY - sh) & 63);
        product_b >>= ((RECON_3D_SHIFT_GEOMETRY - sh) & 63);

        product_c >>= ((UINT32_C(0x30) - sh) & 63);

        wr32((unsigned char *)ctx->screen_x + done * 4, (uint32_t)product_a + base_a);
        wr32((unsigned char *)ctx->screen_y + done * 4, (uint32_t)product_b + base_b);
        wr32((unsigned char *)ctx->screen_z + done * 4, (uint32_t)product_c);
        ++done;
    }
}


static void write128(void *p, const uint32_t v[4])
{
    memcpy(p, v, 16);
}

static int min_s32(uint32_t a, uint32_t b)
{
    return (a ^ UINT32_C(0x80000000)) < (b ^ UINT32_C(0x80000000));
}

static uint64_t extend_s32(uint32_t v)
{
    return (v & UINT32_C(0x80000000)) != 0
        ? (UINT64_C(0xffffffff00000000) | v)
        : v;
}

void gpu3d_geometry_filter_pixel_row_samples(gpu3d_t *ctx)
{

    uint32_t count = ctx->batch_count;
    if (count == 0)
        return;

    uint32_t idx = ctx->vertex_count;

    uint32_t coef_b[4];
    uint32_t coef_c[4];
    uint32_t coef_a[4];
    uint32_t base[4];
    memcpy(coef_b, &ctx->clip_matrix[4], 16);
    memcpy(coef_c, &ctx->clip_matrix[8], 16);
    memcpy(coef_a, &ctx->clip_matrix[0], 16);
    memcpy(base, &ctx->clip_matrix[12], 16);

    uint8_t *sample = (uint8_t *)ctx->batch_y;
    uint8_t *masks = &ctx->clip_code[idx];
    uint8_t *output = (uint8_t *)&ctx->vertex[idx];

    do {
        uint64_t top = extend_s32(rd32(sample - 0x100));
        uint64_t center = extend_s32(rd32(sample));
        uint64_t bottom = extend_s32(rd32(sample + 0x100));
        uint32_t result[4];

        for (unsigned int i = 0; i != 4; i++) {
            uint64_t sum = top * extend_s32(coef_a[i]);
            sum += center * extend_s32(coef_b[i]);
            sum += bottom * extend_s32(coef_c[i]);
            result[i] = base[i] + (uint32_t)(sum >> 12);
        }

        uint32_t limit = result[3];
        uint32_t opposite = 0u - limit;
        uint32_t mask = min_s32(limit, result[0]) ? 1u : 0u;

        if (min_s32(result[0], opposite))
            mask |= 2u;
        if (min_s32(limit, result[1]))
            mask |= 4u;
        if (min_s32(result[1], opposite))
            mask |= 8u;
        if (min_s32(limit, result[2]))
            mask |= 0x10u;
        if (min_s32(result[2], opposite))
            mask |= 0x20u;

        write128(output, result);
        count--;
        *masks++ = (uint8_t)mask;
        sample += 4;
        output += 16;
    } while (count != 0);
}


static void write64(void *p, const uint16_t v[4])
{
    memcpy(p, v, 8);
}


static int32_t extend_10(uint32_t v)
{
    return (int32_t)((v & 0x3ffu) ^ 0x200u) - 0x200;
}

static uint16_t scale64(int32_t a, int32_t b, int32_t c,
                         int32_t x, int32_t y, int32_t z)
{
    int64_t sum = (int64_t)a * x;
    sum += (int64_t)b * y;
    sum += (int64_t)c * z;
    return (uint16_t)((uint64_t)sum >> 12);
}

static uint16_t scale32(int32_t a, int32_t b, int32_t c,
                         int32_t x, int32_t y, int32_t z)
{
    uint32_t sum = (uint32_t)a * (uint32_t)x;
    sum += (uint32_t)b * (uint32_t)y;
    sum += (uint32_t)c * (uint32_t)z;
    return (uint16_t)(sum >> 12);
}

void gpu3d_matrix_transform_vtx10(void *param_1, const void *param_2,
                         const void *param_3, uint32_t param_4)
{

    if (param_4 == 0)
        return;

    const uint8_t *matrix = (const uint8_t *)param_3;

    int32_t m0  = (int32_t)rd32(matrix);
    int32_t m1  = (int32_t)rd32(matrix + 4);
    int32_t m4  = (int32_t)rd32(matrix + 16);
    int32_t m5  = (int32_t)rd32(matrix + 20);
    int32_t m8  = (int32_t)rd32(matrix + 32);
    int32_t m9  = (int32_t)rd32(matrix + 36);
    int32_t m2  = (int32_t)rd32(matrix + 8);
    int32_t m6  = (int32_t)rd32(matrix + 24);
    int32_t m10 = (int32_t)rd32(matrix + 40);

    uint8_t *dst = (uint8_t *)param_1;
    const uint8_t *src = (const uint8_t *)param_2;
    uint32_t done = 0;

    if (param_4 >= 4) {
        uint32_t round = param_4 & ~3u;

        do {
            uint32_t words[4];
            uint16_t row0[4];
            uint16_t row1[4];
            uint16_t row2[4];

            memcpy(words, src + (uint64_t)done * 4, sizeof(words));
            for (uint32_t i = 0; i != 4; i++) {
                int32_t x = extend_10(words[i]);
                int32_t y = extend_10(words[i] >> 10);
                int32_t z = extend_10(words[i] >> 20);

                row0[i] = scale64(m0, m4, m8, x, y, z);
                row1[i] = scale64(m1, m5, m9, x, y, z);
                row2[i] = scale64(m2, m6, m10, x, y, z);
            }

            write64(dst + (uint64_t)done * 2, row0);
            write64(dst + (uint64_t)done * 2 + 144, row1);
            write64(dst + (uint64_t)done * 2 + 288, row2);
            done += 4;
        } while (done != round);
    }

    while (done != param_4) {
        uint32_t word = rd32(src + (uint64_t)done * 4);
        int32_t x = extend_10(word);
        int32_t y = extend_10(word >> 10);
        int32_t z = extend_10(word >> 20);
        uint16_t row0 = scale32(m0, m4, m8, x, y, z);
        uint16_t row1 = scale32(m1, m5, m9, x, y, z);
        uint16_t row2 = scale32(m2, m6, m10, x, y, z);

        wr16(dst + (uint64_t)done * 2, row0);
        wr16(dst + (uint64_t)done * 2 + 144, row1);
        wr16(dst + (uint64_t)done * 2 + 288, row2);
        done++;
    }
}

#define LIMIT       0x80
extern void gpu3d_matrix_transform_and_clip_code_8(gpu3d_t *obj) __asm__("gpu3d_matrix_transform_and_clip_code");
extern void gpu3d_matrix_mult_4x4_neon(int32_t *output, const int32_t *a, const int32_t *b);




static int32_t prod(uint32_t a, int32_t b) {
    return (int32_t)((uint32_t)(int32_t)a * (uint32_t)b);
}

static uint32_t comp(uint32_t v, uint32_t shift) {
    if ((v >> 19) != 0) return 0x1fu << shift;
    return ((v >> 14) & 0x1fu) << shift;
}

void gpu3d_geometry_apply_lighting(gpu3d_t *ctx) {

    if (ctx->batch_count == 0) return;

    uint32_t idx = ctx->vertex_count;

    if (ctx->clip_matrix_dirty) {
        gpu3d_matrix_mult_4x4_neon(ctx->clip_matrix, ctx->projection_matrix,
                           ctx->position_matrix_ptr);
        ctx->clip_matrix_dirty = 0;
    }
    gpu3d_matrix_transform_and_clip_code(ctx);

    uint32_t count = ctx->batch_count;
    for (uint32_t t = 0; t < count; t++)
        ctx->vertex_texcoord[idx + t] = ctx->batch_texcoord[t];
    idx += count;

    uint32_t n_a = (uint32_t)((ctx->normal_cursor - (uint8_t *)ctx->batch_normal) >> 2);

    uint16_t calc[68];

    if (n_a != 0) {
        const uint8_t *mz = (const uint8_t *)ctx->vector_matrix_ptr;
        uint32_t emis0 = ctx->ambient_accum[0];
        uint32_t emis1 = ctx->ambient_accum[1];
        uint32_t emis2 = ctx->ambient_accum[2];
        uint32_t mask = ctx->light_mask;
        uint32_t with_table = ctx->shininess_enabled;

        int64_t m[9];
        m[0] = (int32_t)rd32(mz);      m[1] = (int32_t)rd32(mz + 4);
        m[2] = (int32_t)rd32(mz + 8);
        m[3] = (int32_t)rd32(mz + 16); m[4] = (int32_t)rd32(mz + 20);
        m[5] = (int32_t)rd32(mz + 24);
        m[6] = (int32_t)rd32(mz + 32); m[7] = (int32_t)rd32(mz + 36);
        m[8] = (int32_t)rd32(mz + 40);

        for (uint32_t i = 0; i < n_a; i++) {
            uint32_t c0 = emis0, c1 = emis1, c2 = emis2;
            if (mask != 0) {
                uint32_t v = ctx->batch_normal[i];
                int64_t nx = (int64_t)(int32_t)(v << 22) >> 22;
                int64_t ny = (int64_t)(int32_t)(v << 12) >> 22;
                int64_t nz = (int64_t)(int32_t)(v <<  2) >> 22;

                int32_t n0 = (int32_t)(uint32_t)
                    ((uint64_t)(nx * m[0] + ny * m[3] + nz * m[6]) >> 12);
                int32_t n1 = (int32_t)(uint32_t)
                    ((uint64_t)(nx * m[1] + ny * m[4] + nz * m[7]) >> 12);
                int32_t n2 = (int32_t)(uint32_t)
                    ((uint64_t)(nx * m[2] + ny * m[5] + nz * m[8]) >> 12);

                uint32_t mm = mask;
                for (uint32_t j = 0;; j++) {
                    if (mm & 1) {
                        const uint8_t *ld = (const uint8_t *)ctx->light_direction[j];
                        const uint8_t *lh = (const uint8_t *)ctx->light_half_vector[j];

                        int32_t d = (int32_t)(uint32_t)(
                            (uint32_t)(prod(rd32(ld + 4), n1) >> 9)
                          + (uint32_t)(prod(rd32(ld),     n0) >> 9)
                          + (uint32_t)(prod(rd32(ld + 8), n2) >> 9));
                        int32_t diff = d & ~(d >> 31);

                        int32_t s = (int32_t)(uint32_t)(
                            (uint32_t)(prod(rd32(lh + 4), n1) >> 9)
                          + (uint32_t)(prod(rd32(lh),     n0) >> 9)
                          + (uint32_t)(prod(rd32(lh + 8), n2) >> 9));
                        int32_t expected = s & ~(s >> 31);

                        uint32_t br = (uint32_t)expected * (uint32_t)expected;
                        if (with_table) {
                            uint32_t k = br >> 11;
                            if (k >= 0x7fu) k = 0x7fu;
                            br = (uint32_t)ctx->shininess_table[k] << 1;
                        } else {
                            br >>= 9;
                        }

                        const uint8_t *cd = (const uint8_t *)ctx->light_diffuse[j];
                        const uint8_t *ce = (const uint8_t *)ctx->light_specular[j];
                        c0 += (uint32_t)diff * rd16(cd);
                        c1 += (uint32_t)diff * rd16(cd + 2);
                        c2 += (uint32_t)diff * rd16(cd + 4);
                        c0 += br * rd16(ce);
                        c1 += br * rd16(ce + 2);
                        c2 += br * rd16(ce + 4);
                    }
                    mm >>= 1;
                    if (mm == 0) break;
                }
            }
            calc[i] = (uint16_t)(comp(c0, 0) | comp(c1, 5) | comp(c2, 10));
        }
    }

    uint32_t idx2 = ctx->vertex_count;
    uint32_t color = ctx->last_color;
    uint8_t *dst = (uint8_t *)&ctx->vertex_color[idx2];

    uint32_t n_b = (uint32_t)((ctx->color_cursor - (uint8_t *)ctx->batch_color) >> 1);
    const uint16_t *pb = ctx->batch_color;
    const uint8_t  *pc = ctx->batch_attribute_mark;
    const uint16_t *pa = calc;
    uint32_t prev = 0;

    if (n_a != 0 && n_b != 0) {
        uint32_t n_c = (uint32_t)(ctx->attribute_mark_cursor - ctx->batch_attribute_mark);
        for (uint32_t k = 0; k < n_c; k++) {
            uint32_t b = *pc++;
            uint32_t v = (b & 0x80u) ? *pb++ : *pa++;
            uint32_t until = b & 0x7fu;
            while (prev < until) { wr16(dst, (uint16_t)color); dst += 2; prev++; }
            color = v;
            prev = until;
        }
    } else if (n_a == 0 && n_b != 0) {

        for (uint32_t k = 0; k < n_b; k++) {
            uint32_t b = *pc++;
            uint32_t v = *pb++;
            uint32_t until = b & 0x7fu;
            while (prev < until) { wr16(dst, (uint16_t)color); dst += 2; prev++; }
            color = v;
            prev = until;
        }
    } else if (n_a != 0 ) {
        for (uint32_t k = 0; k < n_a; k++) {
            uint32_t b = *pc++;
            uint32_t v = *pa++;
            while (prev < b) { wr16(dst, (uint16_t)color); dst += 2; prev++; }
            color = v;
            prev = b;
        }
    }

    uint32_t total = count;
    while (prev < total) { wr16(dst, (uint16_t)color); dst += 2; prev++; }

    ctx->last_color = color;
    ctx->vertex_count = idx;
    ctx->batch_count = 0;
    ctx->normal_cursor = (uint8_t *)ctx->batch_normal;
    ctx->color_cursor = (uint8_t *)ctx->batch_color;
    ctx->attribute_mark_cursor = ctx->batch_attribute_mark;
    ctx->attribute_mark = 0xff;

    if (idx >= LIMIT) gpu3d_poly_commit_pending_batch(ctx);
}
#undef LIMIT

static int32_t dot3_shr12(int64_t m0, int64_t m1, int64_t m2,
                                       int64_t v0, int64_t v1, int64_t v2)
{
    int64_t acc = m0 * v0 + m1 * v1 + m2 * v2;
    return (int32_t)((uint64_t)acc >> 12);
}

void *gpu3d_matrix_transform_vectors3(void *param_1, const void *param_2, const void *param_3)
{

    const unsigned char *p2 = (const unsigned char *)param_2;
    const unsigned char *p3 = (const unsigned char *)param_3;
    unsigned char *out = (unsigned char *)param_1;

    int32_t e[12];
    for (int i = 0; i < 12; i++)
        memcpy(&e[i], p2 + 4 * i, 4);

    int32_t m[9];
    for (int i = 0; i < 9; i++)
        memcpy(&m[i], p3 + 4 * i, 4);

    const int64_t A0 = e[0],  A1 = e[4],  A2 = e[8];
    const int64_t B0 = e[1],  B1 = e[5],  B2 = e[9];
    const int64_t C0 = e[2],  C1 = e[6],  C2 = e[10];
    const int64_t D0 = e[3],  D1 = e[7],  D2 = e[11];

    for (int f = 0; f < 3; f++) {
        const int64_t mf0 = m[3 * f + 0];
        const int64_t mf1 = m[3 * f + 1];
        const int64_t mf2 = m[3 * f + 2];

        int32_t rA = dot3_shr12(mf0, mf1, mf2, A0, A1, A2);
        int32_t rB = dot3_shr12(mf0, mf1, mf2, B0, B1, B2);
        int32_t rC = dot3_shr12(mf0, mf1, mf2, C0, C1, C2);
        int32_t rD = dot3_shr12(mf0, mf1, mf2, D0, D1, D2);

        memcpy(out + 16 * f + 0,  &rA, 4);
        memcpy(out + 16 * f + 4,  &rB, 4);
        memcpy(out + 16 * f + 8,  &rC, 4);
        memcpy(out + 16 * f + 12, &rD, 4);
    }

    memcpy(out + 0x30, p2 + 0x30, 16);

    return param_1;
}

#define LIMIT        0x40


void gpu3d_geometry_vertex_submit_with_texcoord(gpu3d_t *ctx, int x, int y, int z) {

    int32_t count = ctx->batch_count;

    if (ctx->clip_matrix_dirty != 0) {
        void *p = ctx->position_matrix_ptr;
        gpu3d_matrix_mult_4x4_neon((int32_t *)((unsigned char *)ctx->clip_matrix),
                                                (const int32_t *)((unsigned char *)ctx->projection_matrix), p);
        ctx->clip_matrix_dirty = 0;
    }

    if (ctx->texcoord_mode == 3) {
        const uint8_t *m = (unsigned char *)ctx->texture_matrix;
        int64_t cx = x, cy = y, cz = z;

        uint64_t u = (uint64_t)((int64_t)rd32s(m)      * cx
                              + (int64_t)rd32s(m + 16) * cy
                              + (int64_t)rd32s(m + 32) * cz);
        ctx->texcoord[0] = (uint16_t)(ctx->texcoord_raw[0] + (u >> 24));

        uint64_t v = (uint64_t)((int64_t)rd32s(m + 4)  * cx
                              + (int64_t)rd32s(m + 20) * cy
                              + (int64_t)rd32s(m + 36) * cz);
        ctx->texcoord[1] = (uint16_t)(ctx->texcoord_raw[1] + (v >> 24));
    }

    if (count == LIMIT) {
        gpu3d_geometry_apply_lighting(ctx);
        count = ctx->batch_count;
    }

    ctx->batch_x[(uint32_t)count] = x;
    ctx->batch_y[(uint32_t)count] = y;
    ctx->batch_z[(uint32_t)count] = z;
    ctx->batch_texcoord[(uint32_t)count] = (uint32_t)ctx->texcoord[0] | ((uint32_t)ctx->texcoord[1] << 16);

    ctx->batch_count = count + 1;
    ctx->vertex_total =  ctx->vertex_total + 1;
}
#undef LIMIT


unsigned char *gpu3d_geometry_poly_split_runs(gpu3d_t *ctx,
                                  unsigned char *output,
                                  const unsigned char *src,
                                  uint32_t step,
                                  const unsigned char *ma,
                                  const unsigned char *mb,
                                  uint32_t mask)
{

    uint32_t w9  = src[4];
    uint32_t w12 = src[6];
    uint32_t w13;
    memcpy(&w13, src + 0, 4);
    uint32_t w14 = src[5];
    uint32_t w11 = src[7];


    uint32_t w15 = w9 & 3u;

    uint32_t w10 = (4u & ~1u) | (w9 & 1u);

    uint64_t x7  = 0;
    uint32_t w16 = 0;
    uint32_t w17 = 0;
    uint32_t w19 = 0;
    uint32_t w20 = 0;
    uint32_t w21 = 0;
    uint32_t w8;
    unsigned char *x0 = output;
    unsigned char *x17 = 0;
    uint64_t cnt64 = (uint64_t)w12;
    int eq;

    if (w12 == 0)
        goto L_10;

    w17 = 0;
    x7  = 0;
    w16 = 0;
    w19 = 0;
    x0  = output;

    w20 = ma[x7];

    if (w19 != 0)
        goto L_4;
    goto L_5;

L_1:

    w19 = 1;

L_2:

    x7 += 1;
    eq = (cnt64 == x7);
    w17 += step;
    if (eq)
        goto L_9;

L_3:

    w20 = ma[x7];
    if (w19 == 0)
        goto L_5;

L_4:

    if (w20 != 0)
        goto L_2;

    w20 = mb[x7];

    if (w20 == 0)
        goto L_8;

    w20 = w14 + w17;
    w21 = (uint32_t)x7 & mask;
    x0[5] = (unsigned char)w20;
    w20 = w11 ^ w21;
    x0[4] = (unsigned char)w10;
    wr32(x0, w13);
    x0[7] = (unsigned char)w20;
    x0 += 8;

    x7 += 1;
    eq = (cnt64 == x7);
    w17 += step;
    if (!eq)
        goto L_3;
    goto L_9;

L_5:

    if (w20 != 0)
        goto L_6;

    w19 = mb[x7];

    if (w19 == 0)
        goto L_2;

L_6:

    if (x7 == 0)
        goto L_7;

    w19 = w16 * step + w14;
    w21 = w16 & mask;
    w20 = (uint32_t)x7 - w16;
    x0[5] = (unsigned char)w19;
    w19 = w11 ^ w21;
    x0[4] = (unsigned char)w15;
    wr32(x0, w13);
    x0[6] = (unsigned char)w20;
    x0[7] = (unsigned char)w19;

    w20 = ma[x7];
    x0 += 8;

L_7:

    if (w20 != 0)
        goto L_1;

    w19 = w14 + w17;
    w20 = (uint32_t)x7 & mask;
    x0[5] = (unsigned char)w19;
    w19 = w11 ^ w20;
    x0[4] = (unsigned char)w10;
    wr32(x0, w13);
    x0[7] = (unsigned char)w19;
    x0 += 8;
    goto L_1;

L_8:

    w19 = 0;
    w16 = (uint32_t)x7;
    x7 += 1;
    eq = (cnt64 == x7);
    w17 += step;
    if (!eq)
        goto L_3;

L_9:

    w17 = 1;
    wr32((unsigned char *)&ctx->run_split_flag, w17);

    if (w19 != 0)
        goto L_12;

    x17 = (unsigned char *)&ctx->run_split_flag;
    goto L_11;

L_10:

    w16 = 0;
    x17 = (unsigned char *)&ctx->run_split_flag;
    wr32((unsigned char *)&ctx->run_split_flag, 1u);
    x0 = output;

L_11:

    wr32(x0, w13);
    w8  = w16 * step + w14;
    w13 = w16 & mask;
    w12 = w12 - w16;
    x0[5] = (unsigned char)w8;
    w8  = w11 ^ w13;
    x0[4] = (unsigned char)w15;
    x0[6] = (unsigned char)w12;
    x0[7] = (unsigned char)w8;
    x0 += 8;

    wr32(x17, 0u);

L_12:

    if (x0 == output)
        return x0;

    w8  = output[5];
    w11 = src[5];
    if (w8 != w11)
        return x0;

    w8 = output[4];
    if (w10 == w8)
        return x0;

    output[4] = (unsigned char)w9;

    return x0;
}

#define V_SIZE     24
#define N_MAX     16
#define PLANES    6
#define STEP_A    0x18004
#define STEP_B    0x10008
#define COUNTER  0x10000
#define OUT_Y     6272
#define OUT_Z     12544
#define OUT_W     0x4980
#define LIMIT      0x1800





void gpu3d_geometry_clip_polygon(gpu3d_t *ctx, const uint8_t *desc, uint32_t n) {

    uint8_t bufA[N_MAX * V_SIZE];
    uint8_t bufB[N_MAX * V_SIZE];

    uint32_t base = desc[5];
    uint32_t idx[4] = { base, base + 1u, base + 2u, base + 3u };

    if (desc[7] != 0) {
        if (n == 3) { idx[0] = base + 1u; idx[1] = base; }
        else        { idx[2] = base + 3u; idx[3] = base + 2u; }
    }

    if (n != 0) {
        for (uint32_t i = 0; i < n; i++) {
            uint32_t j = idx[i];
            uint8_t *v = bufB + (size_t)i * V_SIZE;
            memcpy(v,      &ctx->vertex[j], 16);
            wr32(v + 16, ctx->vertex_texcoord[j]);
            wr16(v + 20, ctx->vertex_color[j]);
            v[22] = ctx->clip_code[j];
        }
    }

    uint8_t *src = bufB, *dst = bufA;
    uint32_t live = n;

    for (uint32_t plane = 0; plane < PLANES; plane++) {
        if (live == 0) return;
        uint32_t outside = 0;
        for (uint32_t i = 0; i < live; i++) {
            const uint8_t *cur = src + (size_t)i * V_SIZE;

            const uint8_t *sig = src + (size_t)((i == live - 1) ? 0 : i + 1) * V_SIZE;
            uint8_t *out = dst + (size_t)outside * V_SIZE;

            uint32_t code = ((cur[22] >> (plane & 31)) & 1u)
                         | (((sig[22] >> (plane & 31)) & 1u) << 1);

            if (code == 2) {
                memcpy(out, cur, V_SIZE);
                gpu3d_geometry_clip_edge(dst + (size_t)(outside + 1) * V_SIZE,
                                       cur, sig, plane);
                outside += 2;
            } else if (code == 1) {
                gpu3d_geometry_clip_edge(out, sig, cur, plane);
                outside += 1;
            } else if (code == 0) {
                memcpy(out, cur, V_SIZE);
                outside += 1;
            }

        }
        if (outside == 0) return;
        live = outside;
        uint8_t *t = src; src = dst; dst = t;
    }

    uint8_t *res = src;
    uint32_t buf = ctx->bank;
    gpu3d_vertex_bank_t *bank = &ctx->vertex_bank[buf];
    uint32_t y = bank->count;
    uint32_t end = y + live;
    uint32_t pos = ctx->polygon_count;

    if (end <= LIMIT && pos != 0x800) {
        uint32_t count = ctx->texture_run_index;
        uint32_t limit = ctx->texture_run[count].first_vertex;
        uint32_t field0 = rd32(desc);
        uint32_t xx = n + desc[5] - 1u;
        uint32_t type_start = (field0 >> 16) & 0x1fu;
        uint32_t g1, g2;

        if (xx > limit) {
            do {
                limit = ctx->texture_run[count + 1].first_vertex;
                count++;
            } while (xx > limit);
            g1 = ctx->texture_run[count - 1].texture_param;
            g2 = ctx->texture_run[count - 1].palette_base;
            ctx->texture_run_index = count;
            ctx->run_texture_param = g1;
            ctx->run_palette_base = g2;
        } else {
            g1 = ctx->run_texture_param;
            g2 = ctx->run_palette_base;
        }

        uint32_t t = (type_start == 0x1f || type_start == 0) ? ((g1 >> 26) & 7u) : 6u;
        gpu3d_polygon_list_t *table = (t == 6 || t == 1) ? &ctx->translucent[buf] : &ctx->opaque[buf];

        uint32_t k = table->count;
        gpu3d_polygon_t *e = &table->polygon[k];
        e->texture_param = g1;
        e->polygon_attr = field0;
        e->palette_base = (uint16_t)g2;
        e->first_vertex = (uint16_t)y;
        e->vertex_count = live;
        table->count = k + 1;
        ctx->polygon_count = (uint16_t)(pos + 1u);
    }

    if (end > LIMIT) return;

    uint32_t c2 = ctx->emitted_count;
    gpu3d_bank_vertex_t *tex = &bank->vertex[y];

    for (uint32_t i = 0; i < live; i++) {
        const uint8_t *v = res + (size_t)i * V_SIZE;
        uint32_t uv = rd32(v + 16);
        ctx->screen_x[c2 + i] = (int32_t)rd32(v);
        ctx->screen_y[c2 + i] = (int32_t)rd32(v + 4);
        ctx->screen_z[c2 + i] = (int32_t)rd32(v + 8);
        ctx->screen_w[c2 + i] = rd32(v + 12);
        tex->s = (uint16_t)uv;
        tex->t = (uint16_t)(uv >> 16);
        tex->w = (int32_t)rd32(v + 12);
        tex->color = rd16(v + 20);
        tex++;
    }

    ctx->emitted_count = c2 + live;
    bank->count = bank->count + live;
}
#undef V_SIZE
#undef N_MAX
#undef PLANES
#undef STEP_A
#undef STEP_B
#undef COUNTER
#undef OUT_Y
#undef OUT_Z
#undef OUT_W
#undef LIMIT

#define STEP_A    0x18004
#define STEP_B    0x10008
#define LIM_0     6
#define COUNTER  0x10000
#define LIMIT_POS  0x800
#define LIMIT_END  0x1800


static gpu3d_polygon_list_t *list_for_type(gpu3d_t *ctx, uint32_t buf, uint32_t type) {
    return (type == 6 || type == 1) ? &ctx->translucent[buf] : &ctx->opaque[buf];
}

void gpu3d_geometry_emit_draw_entries(gpu3d_t *ctx, const uint8_t *desc, uint32_t step,
                        uint32_t x, uint32_t diff, uint32_t alternate)
{

    uint32_t buf = ctx->bank;
    uint32_t count = ctx->texture_run_index;
    uint32_t field0 = rd32(desc);

    uint32_t y = ctx->vertex_bank[buf].count - diff;

    const gpu3d_texture_run_t *list = &ctx->texture_run[count];
    uint32_t g1 = ctx->run_texture_param;
    uint32_t g2 = ctx->run_palette_base;
    uint32_t limit = list->first_vertex;

    uint32_t type_start = (field0 >> 16) & 0x1fu;

    uint32_t t = (type_start == 0x1f || type_start == 0) ? ((g1 >> 26) & 7u) : 6u;
    gpu3d_polygon_list_t *table = list_for_type(ctx, buf, t);

    uint32_t base_pos = ctx->polygon_count;
    uint32_t requested_n = desc[6];
    if (base_pos + requested_n > LIMIT_POS)
        requested_n = LIMIT_POS - base_pos;

    uint32_t alt = (alternate != 0) ? 0x40u : 0u;
    uint32_t field1 = (desc[7] == 0) ? x : (x | 0x40u);

    uint32_t end = (requested_n - 1) * step + (y + x);
    if (end >= 0x1801u) {
        uint32_t gap = LIMIT_END - y;
        if (gap < x) return;
        requested_n = (gap - x) / step;
    }
    if ((int32_t)requested_n < 1) goto save;

    {
        uint32_t xx = x + desc[5] - 1;
        gpu3d_polygon_list_t *t1 = &ctx->opaque[buf];
        gpu3d_polygon_list_t *t2 = &ctx->translucent[buf];
        uint32_t done = 0;

        for (;;) {
            if (xx > limit) {

                do {
                    g1 = list->texture_param;
                    limit = ctx->texture_run[count + 1].first_vertex;
                    g2 = list->palette_base;
                    count++;

                    uint32_t tp = (g1 >> 26) & 7u;
                    table = (type_start != 0x1f && type_start != 0)
                          ? t2
                          : ((tp == 6 || tp == 1) ? t2 : t1);
                    list++;
                } while (xx > limit);
                continue;
            }

            uint32_t n = table->count;
            gpu3d_polygon_t *e = &table->polygon[n];

            e->first_vertex = (uint16_t)y;
            e->vertex_count = field1;
            field1 ^= alt;
            y += step;
            e->texture_param = g1;
            e->polygon_attr = field0;
            e->palette_base = (uint16_t)g2;
            table->count = n + 1;

            done++;
            xx += step;
            if (done == requested_n) break;
        }
    }

save:
    ctx->run_texture_param = g1;
    ctx->run_palette_base = g2;
    ctx->texture_run_index = count;
    ctx->polygon_count = (uint16_t)(base_pos + requested_n);
}
#undef STEP_A
#undef STEP_B
#undef LIM_0
#undef COUNTER
#undef LIMIT_POS
#undef LIMIT_END

#define VX  0
#define VY  4
#define VZ  8
#define VW  12
#define VUV 16
#define VC  20
#define VF  22





static uint32_t interp_64(uint32_t base, uint64_t diff, uint64_t f) {
    if ((uint32_t)diff & 0x80000000u) {
        uint64_t m = (uint64_t)0 - (diff * f);
        return base - (uint32_t)(m >> 18);
    }
    uint64_t m = (uint64_t)(int64_t)(int32_t)diff * f;
    return base + (uint32_t)(m >> 18);
}

static uint32_t interp_32(uint32_t base, uint32_t diff, uint64_t f) {
    if (diff & 0x80000000u) {
        uint32_t n = 0u - diff;
        uint64_t m = (uint64_t)(int64_t)(int32_t)n * f;
        return base - (uint32_t)(m >> 18);
    }
    uint64_t m = (uint64_t)(int64_t)(int32_t)diff * f;
    return base + (uint32_t)(m >> 18);
}

void gpu3d_geometry_clip_edge(uint8_t *out, const uint8_t *a, const uint8_t *b,
                        uint32_t sel)
{

    uint32_t idx = sel >> 1;
    int      neg = (sel & 1u) != 0;

    int64_t  ax = rd32s(a + VX), ay = rd32s(a + VY);
    int64_t  bx = rd32s(b + VX), by = rd32s(b + VY);
    uint32_t az = (uint32_t)rd32s(a + VZ), aw = (uint32_t)rd32s(a + VW);
    uint32_t bz = (uint32_t)rd32s(b + VZ), bw = (uint32_t)rd32s(b + VW);
    uint32_t auv = (uint32_t)rd32s(a + VUV), buv = (uint32_t)rd32s(b + VUV);
    uint32_t acol = rd16(a + VC), bcol = rd16(b + VC);

    int64_t a_sel = rd32s(a + idx * 4);
    int64_t b_sel = rd32s(b + idx * 4);

    uint32_t sa = neg ? (0u - aw) : aw;
    uint32_t sb = neg ? (0u - bw) : bw;

    uint64_t den = ((uint64_t)a_sel - (uint64_t)b_sel)
                 + ((uint64_t)(int64_t)(int32_t)sb - (uint64_t)(int64_t)(int32_t)sa);

    uint64_t f;
    if (den == 0) {
        f = 0x40000u;
    } else {
        uint32_t da = (uint32_t)a_sel - sa;
        uint64_t n = den + ((uint64_t)(int64_t)(int32_t)da << 18) - 1u;
        f = (uint64_t)(int64_t)(int32_t)((int64_t)n / (int64_t)den);
    }

    uint32_t nx = interp_64((uint32_t)ax, (uint64_t)bx - (uint64_t)ax, f);
    uint32_t ny = interp_64((uint32_t)ay, (uint64_t)by - (uint64_t)ay, f);
    uint32_t nz = interp_64(az, (uint64_t)(int64_t)(int32_t)bz
                              - (uint64_t)(int64_t)(int32_t)az, f);
    uint32_t nw = interp_32(aw, bw - aw, f);

    uint32_t au = (uint32_t)(int32_t)(int16_t)auv;
    uint32_t bu = (uint32_t)(int32_t)(int16_t)buv;
    uint32_t nu = interp_32(au, bu - au, f);

    uint32_t av = (uint32_t)((int32_t)auv >> 16);
    uint32_t bv = (uint32_t)((int32_t)buv >> 16);
    uint32_t nv = interp_32(av, bv - av, f);

    uint32_t ar = acol & 0x1fu,          br = bcol & 0x1fu;
    uint32_t nr = interp_64(ar, (uint64_t)br - (uint64_t)ar, f);

    uint32_t ag = (acol >> 5) & 0x1fu,   bg = (bcol >> 5) & 0x1fu;
    uint32_t ng = interp_32(ag, bg - ag, f);

    uint32_t ab = (acol >> 10) & 0x1fu,  bb = (bcol >> 10) & 0x1fu;
    uint32_t nb = interp_32(ab, bb - ab, f);

    wr32(out + VX, nx);
    wr32(out + VY, ny);
    wr32(out + VZ, nz);
    wr32(out + VW, nw);
    wr32(out + VUV, (nu & 0xffffu) | (nv << 16));
    wr16(out + VC, (uint16_t)(nr | (ng << 5) | (nb << 10)));

    wr32(out + idx * 4, neg ? (0u - nw) : nw);

    int32_t ox = rd32s(out + VX), oy = rd32s(out + VY), oz = rd32s(out + VZ);
    int32_t lim = (int32_t)nw, mlim = (int32_t)(0u - nw);

    uint32_t fl = (ox > lim) ? 1u : 0u;
    if (ox < mlim) fl |= 2u;
    if (oy > lim)  fl |= 4u;
    if (oy < mlim) fl |= 8u;
    if (oz > lim)  fl |= 0x10u;
    if (oz < mlim) fl |= 0x20u;
    out[VF] = (uint8_t)fl;
}
#undef VX
#undef VY
#undef VZ
#undef VW
#undef VUV
#undef VC
#undef VF

uint32_t gpu3d_geometry_color_pack_shifted_flag(uint32_t v) {
    uint32_t doubled = v << 1;
    uint32_t with_bit = doubled | 1u;
    return (doubled == 0u) ? 0u : with_bit;
}

uint32_t gpu3d_geometry_color_unpack_bgr555(uint32_t c) {
    uint32_t r = c & 0x1fu;
    r |= ((c >> 5)  & 0x1fu) << 8;
    r |= ((c >> 10) & 0x1fu) << 16;
    return r;
}

uint32_t gpu3d_geometry_color_round5_swar(uint32_t v) {
    uint32_t sum = v + GPU3D_RGB5_MAX_PER_CHANNEL;
    uint32_t carry = 0x00010101u & (sum >> 5);
    return carry + (v << 1);
}

static inline uint32_t expand(uint32_t c) {
    uint32_t r = c & 0x1f;
    r |= ((c >> 5) & 0x1f) << 8;
    r |= ((c >> 10) & 0x1f) << 16;
    uint32_t height = ((r + GPU3D_RGB5_MAX_PER_CHANNEL) >> 5) & 0x07070707;
    return height | (r << 1);
}

uint32_t gpu3d_geometry_color_expand_bgr555_to_rgb8(uint32_t color) {
    return expand(color);
}

uint32_t gpu3d_geometry_color_expand_bgr555_to_rgb8_alpha(uint32_t color, uint32_t alpha) {
    uint32_t r = color & 0x1f;
    r |= ((color >> 5) & 0x1f) << 8;
    r |= ((color >> 10) & 0x1f) << 16;
    uint32_t height = ((r + GPU3D_RGB5_MAX_PER_CHANNEL) >> 5) & 0x07070707;
    uint32_t low = r << 1;
    low = (low & 0x00ffffffu) | ((alpha & 0xff) << 24);
    return low | height;
}

uint32_t gpu3d_geometry_color_expand_bgr555_alpha_bit15(uint64_t param_1) {

    uint32_t x0 = (uint32_t)param_1;

    uint32_t w9  = x0 >> 5;
    uint32_t w10 = x0 & 0x1f;
    uint32_t w11 = x0 >> 15;

    w10 = (w10 & ~(0x1fu << 8)) | ((w9 & 0x1f) << 8);

    w9 = 0x1f000000u;
    uint32_t w8 = x0 >> 10;
    w9 = w11 * w9;

    w11 = 0x001f1f1fu;

    w10 = (w10 & ~(0x1fu << 16)) | ((w8 & 0x1f) << 16);

    w8 = w10 + w11;
    w8 = w8 >> 5;
    w8 = w8 & 0x7070707u;

    w9 = w9 | (w10 << 1);

    return w9 | w8;
}

uint32_t gpu3d_geometry_color_expand_bgr555_alpha_flag(uint64_t param_1, uint64_t param_2) {

    uint32_t x0 = (uint32_t)param_1;
    uint32_t x1 = (uint32_t)param_2;

    uint32_t w9  = x0 >> 5;
    uint32_t w11 = x0 & 0x1fu;
    uint32_t w8  = x0 >> 10;

    w11 = (w11 & ~(0x1fu << 8)) | ((w9 & 0x1fu) << 8);

    w9 = 0x001f1f1fu;

    w11 = (w11 & ~(0x1fu << 16)) | ((w8 & 0x1fu) << 16);

    uint32_t w10 = (x1 == 0u) ? 0u : 0x1f000000u;

    w8 = w11 + w9;
    w8 = w8 >> 5;
    w8 = w8 & 0x7070707u;

    w9 = w10 | (w11 << 1);

    return w9 | w8;
}

#define CTX_TABLE 24
#define CTX_COUNT 72






static void *core_malloc(unsigned long n) {
    typedef void *(*fn_malloc)(unsigned long);
    static fn_malloc f;
    if (!f) f = (fn_malloc)sym_libc_malloc;
    return f(n);
}

static uint32_t pack5(uint16_t h) {
    uint32_t r = (uint32_t)(h & 0x1f);
    uint32_t g = (uint32_t)((h >> 5) & 0x1f);
    uint32_t b = (uint32_t)((h >> 10) & 0x1f);
    uint32_t rr = r * 2u + (r != 0);
    uint32_t gg = g * 2u + (g != 0);
    uint32_t bb = b * 2u + (b != 0);
    return rr | (gg << 8) | (bb << 16);
}

void *gpu3d_geometry_palette_cache_single_entry(uint8_t *ctx, const uint16_t *color,
                          int32_t transparent, int32_t count)
{

    wr16(ctx + CTX_COUNT, (uint16_t)count);

    void *table = rd_ptr(ctx + CTX_TABLE);
    if (table == 0) {
        uint32_t bytes = (uint32_t)count << 2;
        table = core_malloc((unsigned long)bytes);
        wr_ptr(ctx + CTX_TABLE, table);
    }

    uint16_t h = rd16(color);

    uint32_t p = pack5(h);

    uint32_t entry = (transparent == 0) ? (p | 0x1f000000u) : p;

    wr32(table, entry);

    return table;
}
#undef CTX_TABLE
#undef CTX_COUNT

typedef void *(*fn_malloc)(uint64_t);







static void write4x32_22(void *p, const uint32_t v[4]) {
    memcpy(p, v, 16);
}

static uint32_t prepare_bgr555(uint16_t pixel) {
    uint32_t v = (uint32_t)pixel;
    uint32_t r = v & 0x1fu;
    r |= ((v >> 5) & 0x1fu) << 8;
    r |= ((v >> 10) & 0x1fu) << 16;
    return r;
}

static uint32_t expand_22(uint32_t bgr, uint32_t mask) {
    return (bgr << 1) | (((bgr + GPU3D_RGB5_MAX_PER_CHANNEL) >> 5) & mask);
}

void gpu3d_geometry_color_strip_bgr555_to_words(void *param_1, const void *param_2,
                         uint32_t param_3, uint32_t param_4) {

    uint8_t *ctx = (uint8_t *)param_1;
    const uint8_t *origin = (const uint8_t *)param_2;
    uint8_t *dest = (uint8_t *)rd_ptr(ctx + 24);

    wr16(ctx + 72, (uint16_t)param_4);
    if (dest == NULL) {
        uint32_t bytes32 = param_4 << 2;
        dest = (uint8_t *)((fn_malloc)sym_libc_malloc)((uint64_t)bytes32);
        wr_ptr(ctx + 24, dest);
    }

    uint32_t first = prepare_bgr555(rd16(origin));
    first = expand_22(first, 0x07070707u);
    if (param_3 == 0)
        first |= 0x1f000000u;
    wr32(dest, first);

    if (param_4 < 2u)
        return;

    uint64_t rest = (uint64_t)param_4 - 1u;
    if (rest >= 4u) {
        uint64_t vector = rest & ~UINT64_C(3);
        uint64_t left = vector;
        const uint8_t *src_vector = origin + 2;
        uint8_t *dst_vector = dest + 4;

        do {
            uint64_t loaded = rd64(src_vector);
            uint32_t values[4];
            uint32_t lane;

            for (lane = 0; lane != 4u; ++lane) {
                uint16_t pixel = (uint16_t)(loaded >> (lane * 16u));
                uint32_t bgr = prepare_bgr555(pixel);
                values[lane] = expand_22(bgr, 0x00010101u) | 0x1f000000u;
            }
            write4x32_22(dst_vector, values);
            src_vector += 8;
            dst_vector += 16;
            left -= 4u;
        } while (left != 0);

        if (rest == vector)
            return;
    }

    {
        uint64_t idx = (rest & ~UINT64_C(3)) | 1u;
        uint64_t left = (uint64_t)param_4 - idx;
        const uint8_t *src_tail = origin + (idx << 1);
        uint8_t *dst_tail = dest + (idx << 2);

        do {
            uint32_t bgr = prepare_bgr555(rd16(src_tail));
            uint32_t value = expand_22(bgr, 0x00010101u) | 0x1f000000u;
            wr32(dst_tail, value);
            src_tail += 2;
            dst_tail += 4;
            --left;
        } while (left != 0);
    }
}

typedef void *(*fn_malloc_23)(unsigned long);

static void *core_malloc_23(unsigned long n) {
    static fn_malloc_23 f;
    if (!f) f = (fn_malloc_23)sym_libc_malloc;
    return f(n);
}
#define CTX_TABLE  24
#define CTX_FLAG   72
#define N_COLORS  32
#define N_ROWS     8
#define ROW_BYTES 128




static const uint32_t ALPHA_PER_ROW[N_ROWS] = {0, 4, 9, 13, 18, 22, 27, 31};

static uint32_t pack5_23(uint16_t h) {
    uint32_t r = (uint32_t)(h & 0x1f);
    uint32_t g = (uint32_t)((h >> 5) & 0x1f);
    uint32_t b = (uint32_t)((h >> 10) & 0x1f);
    uint32_t rr = r * 2u + (r != 0);
    uint32_t gg = g * 2u + (g != 0);
    uint32_t bb = b * 2u + (b != 0);
    return rr | (gg << 8) | (bb << 16);
}

void *gpu3d_geometry_palette_build_alpha_ramp_table(uint8_t *ctx, const uint16_t *colors) {

    wr16(ctx + CTX_FLAG, 0x100);

    void *table = rd_ptr(ctx + CTX_TABLE);
    if (table == 0) {
        table = core_malloc_23(0x400);
        wr_ptr(ctx + CTX_TABLE, table);
    }

    uint8_t *out = (uint8_t *)table;
    int row, c;
    for (row = 0; row < N_ROWS; row++) {
        uint32_t tag = ALPHA_PER_ROW[row] << 24;
        for (c = 0; c < N_COLORS; c++) {
            uint16_t h;
            memcpy(&h, colors + c, 2);
            uint32_t word = pack5_23(h) | tag;
            memcpy(out + (size_t)row * ROW_BYTES + (size_t)c * 4, &word, 4);
        }
    }

    return table;
}
#undef CTX_TABLE
#undef CTX_FLAG
#undef N_COLORS
#undef N_ROWS
#undef ROW_BYTES

#ifdef __ARM_NEON
#include <arm_neon.h>
#include "core_internals.h"
#endif

void gpu3d_matrix_transform_and_clip_code(gpu3d_t *ctx) {

    const int32_t *m = ctx->clip_matrix;
    uint32_t base = ctx->vertex_count;
    int32_t count = ctx->batch_count;
    if (count == 0) return;

    const int32_t *px = ctx->batch_x;
    const int32_t *py = ctx->batch_y;
    const int32_t *pz = ctx->batch_z;

    int32_t *output = (int32_t *)&ctx->vertex[base];
    uint8_t *codes = &ctx->clip_code[base];

#ifdef __ARM_NEON

    {
        const int32x4_t m0 = vld1q_s32(m), m1 = vld1q_s32(m + 4),
                        m2 = vld1q_s32(m + 8), m3 = vld1q_s32(m + 12);
        const int64x2_t tlo = vreinterpretq_s64_u64(vshll_n_u32(vget_low_u32(vreinterpretq_u32_s32(m3)), 12));
        const int64x2_t thi = vreinterpretq_s64_u64(vshll_n_u32(vget_high_u32(vreinterpretq_u32_s32(m3)), 12));
        const int64x2_t t0 = vdupq_laneq_s64(tlo, 0), t1 = vdupq_laneq_s64(tlo, 1),
                        t2 = vdupq_laneq_s64(thi, 0), t3 = vdupq_laneq_s64(thi, 1);
        const uint32x4_t b1 = vdupq_n_u32(1), b2 = vdupq_n_u32(2), b4 = vdupq_n_u32(4),
                         b8 = vdupq_n_u32(8), b16 = vdupq_n_u32(16), b32 = vdupq_n_u32(32);
        int32_t n = count;
        do {
            int32x4_t vx = vld1q_s32(px), vy = vld1q_s32(py), vz = vld1q_s32(pz);
            int32x2_t xl = vget_low_s32(vx), yl = vget_low_s32(vy), zl = vget_low_s32(vz);
#define T380_COL(T, C)             vmlal_laneq_s32(vmlal_laneq_s32(vmlal_laneq_s32(T, xl, m0, C), yl, m1, C), zl, m2, C)
#define T380_COL2(T, C)             vmlal_high_laneq_s32(vmlal_high_laneq_s32(vmlal_high_laneq_s32(T, vx, m0, C), vy, m1, C), vz, m2, C)
            int32x4x4_t r;
            r.val[0] = vcombine_s32(vshrn_n_s64(T380_COL(t0, 0), 12), vshrn_n_s64(T380_COL2(t0, 0), 12));
            r.val[1] = vcombine_s32(vshrn_n_s64(T380_COL(t1, 1), 12), vshrn_n_s64(T380_COL2(t1, 1), 12));
            r.val[2] = vcombine_s32(vshrn_n_s64(T380_COL(t2, 2), 12), vshrn_n_s64(T380_COL2(t2, 2), 12));
            r.val[3] = vcombine_s32(vshrn_n_s64(T380_COL(t3, 3), 12), vshrn_n_s64(T380_COL2(t3, 3), 12));
#undef T380_COL
#undef T380_COL2
            int32x4_t w = r.val[3], nw = vnegq_s32(w);
            vst4q_s32(output, r);
            uint32x4_t code = vorrq_u32(vorrq_u32(vandq_u32(vcgtq_s32(r.val[0], w), b1),
                                                 vandq_u32(vcgtq_s32(nw, r.val[0]), b2)),
                                       vorrq_u32(vorrq_u32(vandq_u32(vcgtq_s32(r.val[1], w), b4),
                                                           vandq_u32(vcgtq_s32(nw, r.val[1]), b8)),
                                                 vorrq_u32(vandq_u32(vcgtq_s32(r.val[2], w), b16),
                                                           vandq_u32(vcgtq_s32(nw, r.val[2]), b32))));
            uint8x8_t c8 = vmovn_u16(vcombine_u16(vmovn_u32(code), vdup_n_u16(0)));
            vst1_lane_u32((uint32_t *)codes, vreinterpret_u32_u8(c8), 0);
            px += 4; py += 4; pz += 4;
            output += 16;
            codes += 4;
            n -= 4;
        } while (n >= 0);
        return;
    }
#endif

    uint64_t t[4];
    for (int i = 0; i < 4; i++)
        t[i] = (uint64_t)(uint32_t)m[12 + i] << 12;

    int32_t n = count;
    do {
        for (int j = 0; j < 4; j++) {
            int64_t vx = px[j], vy = py[j], vz = pz[j];
            int32_t r[4];
            for (int c = 0; c < 4; c++) {
                uint64_t acc = t[c]
                             + (uint64_t)(vx * (int64_t)m[c])
                             + (uint64_t)(vy * (int64_t)m[4 + c])
                             + (uint64_t)(vz * (int64_t)m[8 + c]);
                r[c] = (int32_t)(uint32_t)(acc >> 12);
            }
            output[j * 4 + 0] = r[0];
            output[j * 4 + 1] = r[1];
            output[j * 4 + 2] = r[2];
            output[j * 4 + 3] = r[3];

            int32_t w = r[3];
            int32_t nw = (int32_t)(0u - (uint32_t)w);
            uint8_t code = 0;
            if (r[0] >  w)  code |= 0x01;
            if (nw > r[0])  code |= 0x02;
            if (r[1] >  w)  code |= 0x04;
            if (nw > r[1])  code |= 0x08;
            if (r[2] >  w)  code |= 0x10;
            if (nw > r[2])  code |= 0x20;
            codes[j] = code;
        }

        px += 4; py += 4; pz += 4;
        output += 16;
        codes += 4;
        n -= 4;
    } while (n >= 0);
}

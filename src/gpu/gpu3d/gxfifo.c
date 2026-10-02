#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include <stddef.h>
#include "blob_symbols.h"
#include "../../core/nds_state.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "gpu/gpu3d/gxfifo_state_primitive_table.h"
#include "mem_access.h"

static const uint32_t gx_batch_reset[4] = { 0u, 0xffu, 0u, 0u };

static const uint16_t gx_command_return_words[128] = {
       0u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
       1u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
       1u,   17u,   36u,   17u,   36u,   19u,   34u,   30u,
      35u,   31u,   28u,   22u,   22u,    1u,    1u,    1u,
       1u,    9u,    1u,    9u,    8u,    8u,    8u,    8u,
       8u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
       4u,    4u,    6u,    1u,   32u,    1u,    1u,    1u,
       1u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
       1u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
       1u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
     392u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
       1u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
       1u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
       1u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
     103u,    9u,    5u,    1u,    1u,    1u,    1u,    1u,
       1u,    1u,    1u,    1u,    1u,    1u,    1u,    1u,
};

static const uint8_t gx_param_count[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x10, 0x0c, 0x10, 0x0c, 0x09, 0x03, 0x03, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


void gpu3d_gxfifo_fill_pattern_buffer(unsigned long param_1, unsigned long param_2,
                         unsigned long param_3, unsigned int param_4)
{
    (void)param_3;

    if (param_4 == 0) {
        return;
    }

    gpu3d_t *ctx = (gpu3d_t *)param_1;

    uint32_t v975c = ctx->ambient_accum[1];
    uint32_t v9760 = ctx->ambient_accum[2];
    uint32_t v9758 = ctx->ambient_accum[0];

    uint32_t w9  = (v975c >> 9) & 0x7fffe0u;
    uint32_t w10 = (v9760 >> 4) & 0xffffc00u;
    uint32_t w8  = w9 | (v9758 >> 14);
    w8 |= w10;
    uint16_t pattern = (uint16_t)w8;

    unsigned char *dst = (unsigned char *)param_2;
    unsigned long count = (unsigned long)param_4;
    unsigned long round;

    if (param_4 < 0x10) {
        round = 0;
    } else {

        round = count & ~0xfUL;

        unsigned long off = 0;
        do {
            unsigned int k;
            for (k = 0; k < 16; k++) {
                wr16(dst + off + (unsigned long)k * 2, pattern);
            }
            off += 32;
        } while (off != round * 2);

        if (round == count) {
            return;
        }
    }

    unsigned long i;
    for (i = round; i < count; i++) {
        wr16(dst + i * 2, pattern);
    }
}

uint64_t gpu3d_gxfifo_cmd_reserved_noop_1(uint64_t x0)
{
    return x0;
}

uint64_t gpu3d_gxfifo_cmd_reserved_noop_2(uint64_t x0)
{
    return x0;
}

uint64_t gpu3d_gxfifo_cmd_reserved_noop_3(uint64_t x0)
{
    return x0;
}

uint64_t gpu3d_gxfifo_cmd_reserved_noop_4(uint64_t x0)
{
    return x0;
}

void gpu3d_gxfifo_trace_buffer_rewind(gpu3d_t *obj) {
    unsigned char *bufA = obj->command_ring;
    unsigned char *bufB = (unsigned char *)obj->param_ring;

    unsigned long (*c_fwrite)(const void *, unsigned long, unsigned long, void *) =
        (unsigned long (*)(const void *, unsigned long, unsigned long, void *))sym_libc_fwrite;
    void *(*c_memcpy)(void *, const void *, unsigned long) =
        (void *(*)(void *, const void *, unsigned long))sym_libc_memcpy;

    unsigned char *far = (unsigned char *)obj->machine;
    if (!((uint8_t)((nds_t *)far)->benchmark.pass_flags & 0x10)) {
        if (obj->trace.mode == 2) {
            unsigned char *curA = obj->command_cursor;
            if (curA != bufA) {
                c_fwrite(bufA, 1, (uint32_t)(curA - bufA),
                         obj->trace.command_file);
                unsigned char *curB = obj->param_cursor;
                c_fwrite(bufB, 1,
                         (uint64_t)(curB - bufB) & 0x3fffffffcull,
                         obj->trace.param_file);
            }
        }

        uint32_t n = (uint32_t)(obj->command_cursor - obj->command_ring);
        gpu3d_gxfifo_execute(obj, n);
    }

    unsigned char *endA = obj->command_pending_cursor;
    unsigned char *endB = obj->param_pending_cursor;
    unsigned char *curA = obj->command_cursor;
    unsigned char *curB = obj->param_cursor;
    uint64_t leftA = (uint64_t)(endA - curA);
    uint64_t leftB = (uint64_t)(endB - curB);

    if ((uint32_t)leftA != 0)
        c_memcpy(bufA, curA, leftA & 0xffffffffull);

    if (leftB & 0x3fffffffcull)
        c_memcpy(bufB, curB, (uint64_t)(((int64_t)(leftB << 30)) >> 30));

    int64_t advanceB = ((int64_t)(leftB << 30)) >> 30;

    obj->command_cursor = bufA;
    obj->param_cursor = bufB;
    obj->command_pending_cursor = bufA + (uint32_t)leftA;
    obj->param_pending_cursor = bufB + advanceB;
    obj->swap_seen = 0;
}

void  gpu3d_geometry_apply_lighting(gpu3d_t *ctx);
void  gpu3d_poly_commit_pending_batch(gpu3d_t *ctx);
void  geolog_write_frame_dump(gpu3d_t *ctx);
void  gpu3d_geometry_vertex_submit_with_texcoord(gpu3d_t *ctx, int x, int y, int z);
void *gpu3d_matrix_transform_vectors3(void *p1, const void *p2, const void *p3);
void  gpu3d_matrix_mult_4x4_neon(int *output, const int *a, const int *b);
void  gpu3d_matrix_mult_4x3(int32_t *output, const int32_t *a, const int32_t *b);


static void copy(unsigned char *d, const unsigned char *s, int n)
{ int i; for (i = 0; i < n; i++) wr32(d + 4 * i, rd32(s + 4 * i)); }

static void push16(gpu3d_t *ctx, uint32_t value)
{
    uint32_t nv    = ctx->batch_count;
    uint32_t mark = ctx->attribute_mark;
    unsigned char *p16  = ctx->color_cursor;
    unsigned char *pmark = ctx->attribute_mark_cursor;

    if ((mark & 0x7f) == nv) {
        if (mark & 0x80) {
            p16 -= 2;
        } else {
            unsigned char *p32 = ctx->normal_cursor;
            p32 -= 4;
            ctx->normal_cursor = p32;
        }
        pmark -= 1;
    }
    nv |= 0x80;
    wr16(p16, (uint16_t)value); p16 += 2;
    *pmark = (unsigned char)nv;  pmark += 1;
    ctx->attribute_mark = nv;
    ctx->attribute_mark_cursor = pmark;
    ctx->color_cursor = p16;
}

static void push32(gpu3d_t *ctx, uint32_t value)
{
    uint32_t nv    = ctx->batch_count;
    uint32_t mark = ctx->attribute_mark;
    unsigned char *p32  = ctx->normal_cursor;
    unsigned char *pmark = ctx->attribute_mark_cursor;

    if ((mark & 0x7f) == nv) {
        if (mark & 0x80) {
            unsigned char *p16 = ctx->color_cursor;
            p16 -= 2;
            ctx->color_cursor = p16;
        } else {
            p32 -= 4;
        }
        pmark -= 1;
    }
    wr32(p32, value); p32 += 4;
    *pmark = (unsigned char)nv; pmark += 1;
    ctx->attribute_mark = nv;
    ctx->attribute_mark_cursor = pmark;
    ctx->normal_cursor = p32;
}

static gpu3d_texture_run_t *texture_slot(gpu3d_t *ctx, uint32_t *tag_out)
{
    uint32_t tag    = ctx->vertex_total;
    uint32_t mark  = ctx->texture_run_mark;
    uint32_t count = ctx->texture_run_count;

    if (mark == tag) {
        count = count - 1;
    } else {
        ctx->texture_run_mark = tag;
        ctx->texture_run_count = count + 1;
    }
    *tag_out = tag;
    return &ctx->texture_run[count];
}

static void recompute_ambient(gpu3d_t *ctx,
                               uint32_t emi, uint32_t amb, uint32_t mask)
{
    uint32_t a0 = (emi << 14) & 0x7c000;
    uint32_t a1 = (emi <<  9) & 0x7c000;
    uint32_t a2 = (emi <<  4) & 0x7c000;

    if (mask) {
        uint32_t f0 = (amb << 9) & 0x3e00;
        uint32_t f1 = (amb << 4) & 0x3e00;
        uint32_t f2 = (amb >> 1) & 0x3e00;
        const unsigned char *p = (unsigned char *)ctx->light_color;

        while (mask) {
            if (mask & 1) {
                uint32_t c = rd32(p);
                a0 += (c        & 0x1f) * f0;
                a1 += ((c >> 5) & 0x1f) * f1;
                a2 += ((c >> 10) & 0x1f) * f2;
            }
            p += 4;
            mask >>= 1;
        }
    }
    ctx->ambient_accum[0] =     a0;
    ctx->ambient_accum[1] = a1;
    ctx->ambient_accum[2] = a2;
}

static void light_products(gpu3d_t *ctx, unsigned char *dest, uint32_t col)
{
    int i;
    uint32_t r = col & 0x1f, g = (col >> 5) & 0x1f, b = (col >> 10) & 0x1f;
    for (i = 0; i < 4; i++) {
        uint32_t lc = ctx->light_color[i];
        wr16(dest + 6 * i + 0, (uint16_t)((lc        & 0x1f) * r));
        wr16(dest + 6 * i + 2, (uint16_t)(((lc >> 5) & 0x1f) * g));
        wr16(dest + 6 * i + 4, (uint16_t)(((lc >> 10) & 0x1f) * b));
    }
}

static void trans_row(unsigned char *m, int row,
                       int64_t px, int64_t py, int64_t pz)
{
    int64_t a = px * (int64_t)rd32s(m + 4 * row)
              + py * (int64_t)rd32s(m + 4 * (row + 4))
              + pz * (int64_t)rd32s(m + 4 * (row + 8));
    wr32(m + 4 * (row + 12),
        rd32(m + 4 * (row + 12)) + (uint32_t)((uint64_t)a >> 12));
}

static void scale_elem(unsigned char *m, int i, int64_t s)
{
    int64_t a = s * (int64_t)rd32s(m + 4 * i);
    wr32(m + 4 * i, (uint32_t)((uint64_t)a >> 12));
}

void gpu3d_gxfifo_execute(gpu3d_t *ctx, unsigned int n)
{
    {
    const unsigned char *cmd, *pair;

    uint32_t vx, vy, vz;

    if (n == 0) return;

    cmd = (unsigned char *)ctx->command_ring;
    pair = (unsigned char *)ctx->param_ring;
    vx = ctx->vertex_xyz[0];
    vy = ctx->vertex_xyz[1];
    vz = ctx->vertex_xyz[2];

    for (;;) {
        switch (*cmd) {

        case 0x10:
            ctx->matrix_mode = (unsigned char)(*pair & 3);
            pair += 4;
            break;

        case 0x11: {
            unsigned m = ctx->matrix_mode;
            if (m == 1 || m == 2) {

                uint32_t sp5 = ctx->position_stack_level;
                unsigned char *slot = (unsigned char *)&ctx->matrix_stack[sp5];
                copy(slot, (unsigned char *)ctx->position_matrix, 32);
                if (sp5 == 0x1f) ctx->position_stack_level = 0;
                else             ctx->position_stack_level = (unsigned char)(sp5 + 1);
            } else if (m == 3) {
                uint32_t spt = ctx->texture_stack_level;
                copy((unsigned char *)ctx->texture_stack + (uint64_t)spt * 64, (unsigned char *)ctx->texture_matrix, 16);
                ctx->texture_stack_level = 0;
            } else if (m == 0) {
                uint32_t spp = ctx->projection_stack_level;
                copy((unsigned char *)ctx->projection_stack + (uint64_t)spp * 64, (unsigned char *)ctx->projection_matrix, 16);
                ctx->projection_stack_level = 0;
            }
            break; }

        case 0x12: {
            unsigned m = ctx->matrix_mode;
            if (m == 1 || m == 2) {
                uint32_t sp5;
                const unsigned char *slot;
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;

                sp5 = (ctx->position_stack_level - (uint32_t)*(const unsigned char *)pair) & 0x1f;
                ctx->position_stack_level = (unsigned char)sp5;
                slot = (unsigned char *)&ctx->matrix_stack[sp5];
                copy((unsigned char *)ctx->position_matrix, slot, 32);
                pair += 4;
            } else if (m == 3) {
                copy((unsigned char *)ctx->texture_matrix, (unsigned char *)ctx->texture_stack, 16);
                ctx->texture_stack_level = 0;
                pair += 4;
            } else if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                copy((unsigned char *)ctx->projection_matrix, (unsigned char *)ctx->projection_stack, 16);
                ctx->projection_stack_level = 0;
                pair += 4;
            }
            break; }

        case 0x13: {
            unsigned m = ctx->matrix_mode;
            if (m == 1 || m == 2) {
                uint32_t idx = rd32(pair) & 0x1f;
                pair += 4;
                copy((unsigned char *)&ctx->matrix_stack[idx], (unsigned char *)ctx->position_matrix, 32);
            } else if (m == 3) {
                pair += 4;
                copy((unsigned char *)ctx->texture_stack, (unsigned char *)ctx->texture_matrix, 16);
            } else if (m == 0) {
                pair += 4;
                copy((unsigned char *)ctx->projection_stack, (unsigned char *)ctx->projection_matrix, 16);
            }
            break; }

        case 0x14: {
            unsigned m = ctx->matrix_mode;
            if (m == 1 || m == 2) {
                uint32_t idx;
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                idx = rd32(pair) & 0x1f;
                copy((unsigned char *)ctx->position_matrix, (unsigned char *)&ctx->matrix_stack[idx], 32);
                pair += 4;
            } else if (m == 3) {
                copy((unsigned char *)ctx->texture_matrix, (unsigned char *)ctx->texture_stack, 16);
                pair += 4;
            } else if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                copy((unsigned char *)ctx->projection_matrix, (unsigned char *)ctx->projection_stack, 16);
                pair += 4;
            }
            break; }

        case 0x15: {
            unsigned m = ctx->matrix_mode;
            if (m > 3) break;
            if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                memset((unsigned char *)ctx->projection_matrix, 0, 64);
                ctx->projection_matrix[0] = 0x1000;
                ctx->projection_matrix[5] = 0x1000;
                ctx->projection_matrix[10] = 0x1000;
                ctx->projection_matrix[15] = 0x1000;
            } else if (m == 1) {
                unsigned char *mp;
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                mp = (unsigned char *)ctx->position_matrix_ptr;
                memset(mp, 0, 64);
                wr32(mp + 0, 0x1000); wr32(mp + 20, 0x1000);
                wr32(mp + 40, 0x1000); wr32(mp + 60, 0x1000);
            } else if (m == 2) {
                unsigned char *mp, *mv;
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                mp = (unsigned char *)ctx->position_matrix_ptr;
                memset(mp, 0, 64);
                wr32(mp + 0, 0x1000); wr32(mp + 20, 0x1000);
                wr32(mp + 40, 0x1000); wr32(mp + 60, 0x1000);
                mv = (unsigned char *)ctx->vector_matrix_ptr;
                memset(mv, 0, 64);
                wr32(mv + 0, 0x1000); wr32(mv + 20, 0x1000);
                wr32(mv + 40, 0x1000); wr32(mv + 60, 0x1000);
            } else {
                memset((unsigned char *)ctx->texture_matrix, 0, 64);
                ctx->texture_matrix[0] = 0x1000;
                ctx->texture_matrix[5] = 0x1000;
                ctx->texture_matrix[10] = 0x1000;
                ctx->texture_matrix[15] = 0x1000;
            }
            break; }

        case 0x16: {
            unsigned m = ctx->matrix_mode;
            if (m > 3) break;
            if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                copy((unsigned char *)ctx->projection_matrix, pair, 16);
                pair += 0x40;
            } else if (m == 1) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                copy((unsigned char *)ctx->position_matrix_ptr, pair, 16);
                pair += 0x40;
            } else if (m == 2) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                copy((unsigned char *)ctx->position_matrix_ptr, pair, 16);
                copy((unsigned char *)ctx->vector_matrix_ptr, pair, 16);
                pair += 0x40;
            } else {
                copy((unsigned char *)ctx->texture_matrix, pair, 16);
                pair += 0x40;
            }
            break; }

        case 0x17: {
            unsigned m = ctx->matrix_mode;
            unsigned char *d = NULL;
            if (m > 3) break;
            if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                d = (unsigned char *)ctx->projection_matrix;
            } else if (m == 1 || m == 2) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                d = (unsigned char *)ctx->position_matrix_ptr;
            } else {
                d = (unsigned char *)ctx->texture_matrix;
            }
            {   int f;
                for (f = 0; f < 4; f++) {
                    wr32(d + 16 * f + 0, rd32(pair + 12 * f + 0));
                    wr32(d + 16 * f + 4, rd32(pair + 12 * f + 4));
                    wr32(d + 16 * f + 8, rd32(pair + 12 * f + 8));
                    wr32(d + 16 * f + 12, f == 3 ? 0x1000u : 0u);
                }
            }
            if (m == 2) {
                unsigned char *v = (unsigned char *)ctx->vector_matrix_ptr;
                int f;
                for (f = 0; f < 4; f++) {
                    wr32(v + 16 * f + 0, rd32(pair + 12 * f + 0));
                    wr32(v + 16 * f + 4, rd32(pair + 12 * f + 4));
                    wr32(v + 16 * f + 8, rd32(pair + 12 * f + 8));
                    wr32(v + 16 * f + 12, f == 3 ? 0x1000u : 0u);
                }
            }
            pair += 0x30;
            break; }

        case 0x18: {
            unsigned m = ctx->matrix_mode;
            unsigned char *d;
            if (m > 3) break;
            if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                d = (unsigned char *)ctx->projection_matrix;
            } else if (m == 1) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
            } else if (m == 2) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                gpu3d_matrix_mult_4x4_neon((int *)d, (const int *)d, (const int *)pair);
                d = (unsigned char *)ctx->vector_matrix_ptr;
            } else {
                d = (unsigned char *)ctx->texture_matrix;
            }
            gpu3d_matrix_mult_4x4_neon((int *)d, (const int *)d, (const int *)pair);
            pair += 0x40;
            break; }

        case 0x19: {
            unsigned m = ctx->matrix_mode;
            unsigned char *d;
            if (m > 3) break;
            if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->projection_matrix;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
            } else if (m == 1) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
            } else if (m == 2) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                gpu3d_matrix_mult_4x3((int32_t *)d, (const int32_t *)d,
                                   (const int32_t *)pair);
                d = (unsigned char *)ctx->vector_matrix_ptr;
            } else {
                d = (unsigned char *)ctx->texture_matrix;
            }
            gpu3d_matrix_mult_4x3((int32_t *)d, (const int32_t *)d,
                               (const int32_t *)pair);
            pair += 0x30;
            break; }

        case 0x1a: {
            unsigned m = ctx->matrix_mode;
            unsigned char *d;
            if (m > 3) break;
            if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->projection_matrix;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
            } else if (m == 1) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
            } else if (m == 2) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                gpu3d_matrix_transform_vectors3(d, d, pair);
                d = (unsigned char *)ctx->vector_matrix_ptr;
            } else {
                d = (unsigned char *)ctx->texture_matrix;
            }
            gpu3d_matrix_transform_vectors3(d, d, pair);
            pair += 0x24;
            break; }

        case 0x1b: {
            unsigned m = ctx->matrix_mode;
            int64_t sx = (int32_t)rd32(pair);
            int64_t sy = (int32_t)rd32(pair + 4);
            int64_t sz = (int32_t)rd32(pair + 8);
            unsigned char *d = NULL;
            if (m == 1 || m == 2) {

                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
            } else if (m == 3) {
                d = (unsigned char *)ctx->texture_matrix;
            } else if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                d = (unsigned char *)ctx->projection_matrix;
            } else {
                break;
            }
            {   int i;
                for (i = 0; i <  4; i++) scale_elem(d, i, sx);
                for (i = 4; i <  8; i++) scale_elem(d, i, sy);
                for (i = 8; i < 12; i++) scale_elem(d, i, sz);
            }
            pair += 0xc;
            break; }

        case 0x1c: {
            unsigned m = ctx->matrix_mode;
            int64_t px, py, pz;
            unsigned char *d;
            if (m > 3) break;
            px = (int32_t)rd32(pair);
            py = (int32_t)rd32(pair + 4);
            pz = (int32_t)rd32(pair + 8);
            if (m == 0) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                d = (unsigned char *)ctx->projection_matrix;
            } else if (m == 1) {
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
            } else if (m == 2) {
                int f;
                gpu3d_geometry_apply_lighting(ctx);
                d = (unsigned char *)ctx->position_matrix_ptr;
                ctx->clip_matrix_dirty = 1; ctx->light_dirty = 0x0f;
                for (f = 0; f < 4; f++) trans_row(d, f, px, py, pz);
                d = (unsigned char *)ctx->vector_matrix_ptr;
            } else {
                d = (unsigned char *)ctx->texture_matrix;
            }
            {   int f;
                for (f = 0; f < 4; f++) trans_row(d, f, px, py, pz);
            }
            pair += 0xc;
            break; }

        case 0x20:
            push16(ctx, rd32(pair));
            pair += 4;
            break;

        case 0x21: {
            uint32_t p = rd32(pair);
            pair += 4;
            if (ctx->texcoord_mode == 2) {

                int64_t nx = (int32_t)(p << 22) >> 22;
                int64_t ny = (int32_t)(p << 12) >> 22;
                int64_t nz = (int32_t)(p <<  2) >> 22;
                int64_t s = (int64_t)(int32_t)ctx->texture_matrix[0] * nx
                          + (int64_t)(int32_t)ctx->texture_matrix[4] * ny
                          + (int64_t)(int32_t)ctx->texture_matrix[8] * nz;
                int64_t t = (int64_t)(int32_t)ctx->texture_matrix[1] * nx
                          + (int64_t)(int32_t)ctx->texture_matrix[5] * ny
                          + (int64_t)(int32_t)ctx->texture_matrix[9] * nz;
                uint32_t bs = ctx->texcoord_raw[0];
                uint32_t bt = ctx->texcoord_raw[1];
                ctx->texcoord[0] =     (uint16_t)(bs + (uint32_t)((uint64_t)s >> 21));
                ctx->texcoord[1] = (uint16_t)(bt + (uint32_t)((uint64_t)t >> 21));
            }
            push32(ctx, p);
            break; }

        case 0x22: {
            uint32_t p = rd32(pair);
            uint32_t mode;
            pair += 4;
            mode = ctx->texcoord_mode;
            ctx->texcoord_raw[0] =     (uint16_t)p;
            ctx->texcoord_raw[1] = (uint16_t)(p >> 16);
            if (mode == 0) {
                ctx->texcoord[0] =     (uint16_t)p;
                ctx->texcoord[1] = (uint16_t)(p >> 16);
            } else if (mode == 1) {

                int32_t s16 = (int16_t)p;
                int32_t t16 = (int32_t)p >> 16;
                uint32_t s = (uint32_t)((int32_t)ctx->texture_matrix[0] * s16)
                           + (uint32_t)((int32_t)ctx->texture_matrix[4] * t16);
                int64_t  t = (int64_t)(int32_t)ctx->texture_matrix[1] * (int64_t)s16
                           + (int64_t)(int32_t)ctx->texture_matrix[5] * (int64_t)t16;

                s += ctx->texture_matrix[8];
                t += (int64_t)(uint64_t)ctx->texture_matrix[9];
                s += ctx->texture_matrix[12];
                t += (int64_t)(uint64_t)ctx->texture_matrix[13];
                ctx->texcoord[0] =     (uint16_t)(s >> 12);
                ctx->texcoord[1] = (uint16_t)(uint32_t)((uint64_t)t >> 12);
            }
            break; }

        case 0x23: {
            uint32_t p0 = rd32(pair), p1 = rd32(pair + 4);
            vx = p0;
            vz = p1;
            gpu3d_geometry_vertex_submit_with_texcoord(ctx, (int16_t)p0, (int32_t)p0 >> 16, (int16_t)p1);
            vy = p0 >> 16;
            pair += 8;
            break; }

        case 0x24: {
            uint32_t p = rd32(pair);
            int32_t x = (int32_t)(p << 22) >> 22;
            int32_t y = ((int32_t)(p << 12) >> 22);
            int32_t z = ((int32_t)(p <<  2) >> 22);
            vy = (uint32_t)y << 6;
            vz = (uint32_t)z << 6;
            vx = p << 6;
            gpu3d_geometry_vertex_submit_with_texcoord(ctx, (int32_t)((uint32_t)x << 6),
                               (int32_t)vy, (int32_t)vz);
            pair += 4;
            break; }

        case 0x25: {
            uint32_t p = rd32(pair);
            vx = p;
            gpu3d_geometry_vertex_submit_with_texcoord(ctx, (int16_t)p, (int32_t)p >> 16, (int16_t)vz);
            vy = p >> 16;
            pair += 4;
            break; }

        case 0x26: {
            uint32_t p = rd32(pair);
            vx = p;
            gpu3d_geometry_vertex_submit_with_texcoord(ctx, (int16_t)p, (int16_t)vy, (int32_t)p >> 16);
            vz = p >> 16;
            pair += 4;
            break; }

        case 0x27: {
            uint32_t p = rd32(pair);
            gpu3d_geometry_vertex_submit_with_texcoord(ctx, (int16_t)vx, (int16_t)p, (int32_t)p >> 16);
            vy = p;
            vz = p >> 16;
            pair += 4;
            break; }

        case 0x28: {
            uint32_t p = rd32(pair);
            vx = vx + (uint32_t)((int32_t)(p << 22) >> 22);
            vy = vy + (uint32_t)((int32_t)(p << 12) >> 22);
            vz = vz + (uint32_t)((int32_t)(p <<  2) >> 22);
            gpu3d_geometry_vertex_submit_with_texcoord(ctx, (int16_t)vx, (int16_t)vy, (int16_t)vz);
            pair += 4;
            break; }

        case 0x29:
            ctx->polygon_attr = rd32(pair);
            pair += 4;
            break;

        case 0x2a: {
            uint32_t p = rd32(pair);
            uint32_t v = p & 0x3fffffff;
            uint32_t mode = p >> 30;
            pair += 4;
            ctx->texcoord_mode = (unsigned char)mode;
            if (v != ctx->texture_param) {
                uint32_t tag;
                gpu3d_texture_run_t *r;
                ctx->texture_param = v;
                r = texture_slot(ctx, &tag);
                r->texture_param = v;
                r->first_vertex = (unsigned char)tag;
                r->palette_base = ctx->texture_palette_base;
            }
            break; }

        case 0x2b: {
            uint32_t v = rd32(pair) & 0x1fff;
            pair += 4;
            if (v != (uint32_t)ctx->texture_palette_base) {
                uint32_t tag;
                gpu3d_texture_run_t *r;
                ctx->texture_palette_base = (uint16_t)v;
                r = texture_slot(ctx, &tag);
                r->palette_base = (uint16_t)v;
                r->texture_param = ctx->texture_param;
                r->first_vertex = (unsigned char)tag;
            }
            break; }

        case 0x30: {
            uint32_t p = rd32(pair);
            uint32_t sinbit, diff, amb;
            pair += 4;
            if (p & 0x8000)
                push16(ctx, p & 0x7fff);
            sinbit = p & 0xffff7fff;
            if (sinbit != ctx->diffuse_ambient_raw) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->diffuse_ambient_raw = sinbit;
            }
            diff = p & 0x7fff;
            amb = p >> 16;
            ctx->diffuse_color = diff;
            light_products(ctx, (unsigned char *)ctx->light_diffuse[0], diff);
            ctx->ambient_color = amb;
            recompute_ambient(ctx, ctx->emission_color, amb, ctx->light_mask);
            break; }

        case 0x31: {
            uint32_t p = rd32(pair);
            pair += 4;
            if (p != ctx->specular_emission_raw) {
                uint32_t emi_old = ctx->emission_color;
                uint32_t spe = p & 0x7fff;
                uint32_t emi = (p >> 16) & 0x7fff;
                gpu3d_geometry_apply_lighting(ctx);
                ctx->specular_emission_raw = p;
                ctx->shininess_enabled = (unsigned char)((p >> 15) & 1);
                if (spe != ctx->specular_color) {
                    light_products(ctx, (unsigned char *)ctx->light_specular[0], spe);
                    ctx->specular_color = spe;
                }
                if (emi != emi_old) {

                    uint32_t a0 = ctx->ambient_accum[0];
                    uint32_t a1 = ctx->ambient_accum[1];
                    uint32_t a2 = ctx->ambient_accum[2];
                    a0 += ((emi & 0x1f) << 14) - ((emi_old & 0x1f) << 14);
                    a1 += ((p >> 7)  & 0x7c000) - ((emi_old << 9) & 0x7c000);
                    a2 += ((p >> 12) & 0x7c000) - ((emi_old << 4) & 0x7c000);
                    ctx->emission_color = emi;
                    ctx->ambient_accum[0] =     a0;
                    ctx->ambient_accum[1] = a1;
                    ctx->ambient_accum[2] = a2;
                }
            }
            break; }

        case 0x32: {
            uint32_t p = rd32(pair);
            uint32_t idx = p >> 30;
            uint32_t v   = p & 0x3fffffff;
            uint32_t bit = 1u << idx;
            unsigned char *cache = (unsigned char *)ctx->light_vector_raw + 4 * idx;
            int recompute;
            pair += 4;

            if (rd32(cache) != v)          recompute = 1;
            else                          recompute = (ctx->light_dirty & bit) != 0;

            if (recompute) {
                int64_t nx = -(int64_t)((int32_t)(p << 22) >> 22);
                int64_t ny = -(int64_t)((int32_t)(p << 12) >> 22);
                int64_t nz = -(int64_t)((int32_t)(p <<  2) >> 22);
                unsigned char *dir  = (unsigned char *)ctx->light_direction[0]  + 12 * idx;
                unsigned char *half = (unsigned char *)ctx->light_half_vector[0] + 12 * idx;
                const unsigned char *mv;
                int32_t d0, d1, d2, h2a, h2b;

                gpu3d_geometry_apply_lighting(ctx);
                wr32(cache, v);
                mv = (unsigned char *)ctx->vector_matrix_ptr;

                d0 = (int32_t)(uint32_t)((uint64_t)((int64_t)rd32s(mv + 0) * nx
                                                  + (int64_t)rd32s(mv + 16) * ny
                                                  + (int64_t)rd32s(mv + 32) * nz) >> 12);
                wr32(dir, (uint32_t)d0);
                d1 = (int32_t)(uint32_t)((uint64_t)((int64_t)rd32s(mv + 4) * nx
                                                  + (int64_t)rd32s(mv + 20) * ny
                                                  + (int64_t)rd32s(mv + 36) * nz) >> 12);
                wr32(dir + 4, (uint32_t)d1);
                d2 = (int32_t)(uint32_t)((uint64_t)((int64_t)rd32s(mv + 8) * nx
                                                  + (int64_t)rd32s(mv + 24) * ny
                                                  + (int64_t)rd32s(mv + 40) * nz) >> 12);
                wr32(dir + 8, (uint32_t)d2);

                if (d0 < 0) d0 += 1;
                if (d1 < 0) d1 += 1;
                h2a = d2 + 0x200;
                h2b = d2 + 0x201;
                wr32(half,     (uint32_t)(d0 >> 1));
                wr32(half + 4, (uint32_t)(d1 >> 1));
                wr32(half + 8, (uint32_t)(((h2a < 0) ? h2b : h2a) >> 1));

                ctx->light_dirty = (unsigned char)(ctx->light_dirty & ~bit);
            }
            break; }

        case 0x33: {
            uint32_t p = rd32(pair);
            uint32_t idx = p >> 30;
            unsigned char *cache = (unsigned char *)ctx->light_color + 4 * idx;
            uint32_t old = rd32(cache);
            uint32_t updated = p & 0x7fff;
            pair += 4;
            if (updated != old) {
                uint32_t spe, diff, other;
                unsigned char *pspe = (unsigned char *)ctx->light_specular[0] + 6 * idx;
                unsigned char *pdif = (unsigned char *)ctx->light_diffuse[0] + 6 * idx;
                gpu3d_geometry_apply_lighting(ctx);
                spe  = ctx->specular_color;
                other = rd32(cache);
                wr16(pspe + 0, (uint16_t)((other         & 0x1f) * (spe        & 0x1f)));
                wr16(pspe + 2, (uint16_t)(((other >> 5)  & 0x1f) * ((spe >> 5) & 0x1f)));
                wr16(pspe + 4, (uint16_t)(((other >> 10) & 0x1f) * ((spe >> 10) & 0x1f)));
                diff = ctx->diffuse_color;
                wr16(pdif + 0, (uint16_t)((diff         & 0x1f) * (other        & 0x1f)));
                wr16(pdif + 2, (uint16_t)(((diff >> 5)  & 0x1f) * ((other >> 5) & 0x1f)));
                wr16(pdif + 4, (uint16_t)(((diff >> 10) & 0x1f) * ((other >> 10) & 0x1f)));

                if (ctx->light_mask & (1u << idx)) {
                    uint32_t amb = ctx->ambient_color;
                    uint32_t a0 = ctx->ambient_accum[0];
                    uint32_t a1 = ctx->ambient_accum[1];
                    uint32_t a2 = ctx->ambient_accum[2];
                    a0 += ((p        & 0x1f) - (old        & 0x1f)) * ((amb << 9) & 0x3e00);
                    a1 += (((p >> 5) & 0x1f) - ((old >> 5) & 0x1f)) * ((amb << 4) & 0x3e00);
                    a2 += (((p >> 10) & 0x1f) - ((old >> 10) & 0x1f)) * ((amb >> 1) & 0x3e00);
                    ctx->ambient_accum[0] =     a0;
                    ctx->ambient_accum[1] = a1;
                    ctx->ambient_accum[2] = a2;
                }
                wr32(cache, updated);
            }
            break; }

        case 0x34:
            if (ctx->shininess_enabled) gpu3d_geometry_apply_lighting(ctx);
            memcpy((unsigned char *)ctx->shininess_table, pair, 128);
            pair += 0x80;
            break;

        case 0x40: {
            uint32_t poly = ctx->polygon_attr;
            uint32_t p    = rd32(pair);
            uint32_t lights = poly & 0xf;
            uint32_t nreg, tag;
            gpu3d_primitive_run_t *r;
            pair += 4;

            if (lights != (uint32_t)ctx->light_mask) {
                gpu3d_geometry_apply_lighting(ctx);
                ctx->light_mask = (unsigned char)lights;
                recompute_ambient(ctx, ctx->emission_color,
                                   ctx->ambient_color, lights);
            }

            nreg = ctx->primitive_run_count;
            if (nreg != 0) {
                uint32_t prev = nreg - 1;
                unsigned char m = ctx->primitive_run[prev].first_vertex;
                tag = ctx->vertex_total;
                if (tag == (uint32_t)m) nreg = prev;
            } else {
                tag = ctx->vertex_total;
            }
            r = &ctx->primitive_run[nreg];
            r->flipped = 0;
            r->polygon_attr = poly;
            r->first_vertex = (unsigned char)tag;
            r->type = (unsigned char)(p & 3);
            ctx->primitive_run_count = nreg + 1;
            break; }

        case 0x41:
            break;

        case 0x50:
            ctx->swap_pending  = 1;
            ctx->swap_params_requested = (unsigned char)rd32(pair);
            gpu3d_geometry_apply_lighting(ctx);
            gpu3d_poly_commit_pending_batch(ctx);
            geolog_write_frame_dump(ctx);
            goto end;

        case 0x60: {
            uint32_t p = rd32(pair);
            uint32_t x1 = p & 0xff;
            uint32_t y1raw = (p >> 8) & 0xff;

            uint32_t y1 = (y1raw > 0xbf) ? ((p >> 8) | 0xffffff00u) : y1raw;
            uint32_t x2 = (p >> 16) & 0xff;
            uint32_t y2 = p >> 24;
            gpu3d_geometry_apply_lighting(ctx);
            gpu3d_poly_commit_pending_batch(ctx);
            ctx->viewport_origin[0] =     (uint16_t)x1;
            ctx->viewport_origin[1] = (uint16_t)y1;
            ctx->viewport_size[0] =     (uint16_t)(x2 - x1 + 1);
            ctx->viewport_size[1] = (uint16_t)(y2 - y1 + 1);
            pair += 4;
            break; }

        case 0x70: {
            uint32_t p0 = rd32(pair), p1 = rd32(pair + 4), p2 = rd32(pair + 8);
            int32_t X  = (int16_t)p0;
            int32_t Y  = (int32_t)p0 >> 16;
            int32_t Z  = (int16_t)p1;
            int32_t XW = (int32_t)(p1 + (p0 << 16)) >> 16;
            int32_t ZD = (int32_t)(p2 + (p1 << 16)) >> 16;
            int32_t YH = (int16_t)(p2 + (uint32_t)Y);
            int32_t xs[8], ys[8], zs[8];
            unsigned char oc[8];
            const unsigned char *mc;
            int i;
            pair += 0xc;

            xs[0] = xs[1] = xs[2] = xs[3] = X;
            xs[4] = xs[5] = xs[6] = xs[7] = XW;
            ys[0] = ys[1] = ys[4] = ys[5] = Y;
            ys[2] = ys[3] = ys[6] = ys[7] = YH;
            zs[0] = zs[2] = zs[4] = zs[6] = Z;
            zs[1] = zs[3] = zs[5] = zs[7] = ZD;

            if (ctx->clip_matrix_dirty) {
                gpu3d_matrix_mult_4x4_neon(ctx->clip_matrix,
                                   ctx->projection_matrix,
                                   ctx->position_matrix_ptr);
                ctx->clip_matrix_dirty = 0;
            }
            mc = (unsigned char *)ctx->clip_matrix;
            for (i = 0; i < 8; i++) {
                int64_t x = xs[i], y = ys[i], z = zs[i];
                int32_t cx, cy, cz, cw, ncw;
                unsigned c;
                cx = (int32_t)(uint32_t)((uint64_t)(
                        (int64_t)rd32s(mc + 0) * x + (int64_t)rd32s(mc + 16) * y
                      + (int64_t)rd32s(mc + 32) * z
                      + (int64_t)((uint64_t)(int64_t)rd32s(mc + 48) << 12)) >> 12);
                cy = (int32_t)(uint32_t)((uint64_t)(
                        (int64_t)rd32s(mc + 4) * x + (int64_t)rd32s(mc + 20) * y
                      + (int64_t)rd32s(mc + 36) * z
                      + (int64_t)((uint64_t)(int64_t)rd32s(mc + 52) << 12)) >> 12);
                cz = (int32_t)(uint32_t)((uint64_t)(
                        (int64_t)rd32s(mc + 8) * x + (int64_t)rd32s(mc + 24) * y
                      + (int64_t)rd32s(mc + 40) * z
                      + (int64_t)((uint64_t)(int64_t)rd32s(mc + 56) << 12)) >> 12);
                cw = (int32_t)(uint32_t)((uint64_t)(
                        (int64_t)rd32s(mc + 12) * x + (int64_t)rd32s(mc + 28) * y
                      + (int64_t)rd32s(mc + 44) * z
                      + (int64_t)((uint64_t)(int64_t)rd32s(mc + 60) << 12)) >> 12);
                ncw = -cw;
                c = 0;
                if (cw  <  cx) c |= 0x01;
                if (ncw >  cx) c |= 0x02;
                if (cw  <  cy) c |= 0x04;
                if (ncw >  cy) c |= 0x08;
                if (cz  >  cw) c |= 0x10;
                if (cz  < ncw) c |= 0x20;
                oc[i] = (unsigned char)c;
            }

            {
                unsigned c0 = oc[0], c1 = oc[1], c2 = oc[2], c3 = oc[3];
                unsigned c4 = oc[4], c5 = oc[5], c6 = oc[6], c7 = oc[7];
                unsigned r = 1;
                if ((c6 & c4 & c0 & c2) != 0
                 && (c7 & c5 & c1 & c3) != 0
                 && ((c5 & c4) & (c1 & c0)) != 0
                 && ((c6 & c2 & c3) & c7) != 0
                 && ((c3 & c2) & (c1 & c0)) != 0
                 && ((c6 & c4 & c5) & c7) != 0)
                    r = 0;
                ctx->box_test_result = (unsigned char)r;
            }
            break; }

        case 0x71: {
            uint32_t p0 = rd32(pair);
            int64_t X = (int16_t)p0;
            int64_t Y = (int32_t)p0 >> 16;
            int64_t Z = (int16_t)rd16(pair + 4);
            unsigned char *res = (unsigned char *)ctx->machine->bus.io_mirror[0].pos_result;
            const unsigned char *mc;
            int f;
            if (ctx->clip_matrix_dirty) {
                gpu3d_matrix_mult_4x4_neon(ctx->clip_matrix,
                                   ctx->projection_matrix,
                                   ctx->position_matrix_ptr);
                ctx->clip_matrix_dirty = 0;
            }
            mc = (unsigned char *)ctx->clip_matrix;
            for (f = 0; f < 4; f++) {

                uint64_t a = (uint64_t)((int64_t)rd32s(mc + 4 * f)       * X
                                      + (int64_t)rd32s(mc + 4 * (f + 4)) * Y
                                      + (int64_t)rd32s(mc + 4 * (f + 8)) * Z)
                           + ((uint64_t)rd32(mc + 4 * (f + 12)) << 12);
                wr32(res + 4 * f, (uint32_t)(a >> 12));
            }
            pair += 8;
            break; }

        case 0x72: {
            uint32_t p = rd32(pair);
            const unsigned char *mv = (unsigned char *)ctx->vector_matrix_ptr;
            unsigned char *res = (unsigned char *)ctx->machine->bus.io_mirror[0].vec_result;

            int64_t vxx = (int32_t)((uint32_t)((int32_t)(p << 22) >> 22) << 3);
            int64_t vyy = (int32_t)((uint32_t)((int32_t)(p << 12) >> 22) << 3);
            int64_t vzz = (int32_t)((uint32_t)((int32_t)(p <<  2) >> 22) << 3);
            int i;
            for (i = 0; i < 3; i++) {
                int64_t a = (int64_t)rd32s(mv + 4 * i)       * vxx
                          + (int64_t)rd32s(mv + 4 * (i + 4)) * vyy
                          + (int64_t)rd32s(mv + 4 * (i + 8)) * vzz;
                uint32_t v = (uint32_t)((uint64_t)a >> 12);

                v = (v & 0x8000) ? (v | 0x7000) : (v & 0x8fff);
                wr16(res + 2 * i, (uint16_t)v);
            }
            pair += 4;
            break; }

        default:
            break;
        }

        if (--n == 0) goto end;
        cmd++;
    }

end:
    ctx->vertex_xyz[0] =     (uint16_t)vx;
    ctx->vertex_xyz[1] = (uint16_t)vy;
    ctx->vertex_xyz[2] = (uint16_t)vz;
    }
}

extern void  gpu3d_gxfifo_trace_buffer_rewind_7(gpu3d_t *obj) __asm__("gpu3d_gxfifo_trace_buffer_rewind");
extern void  gpu3d_geometry_apply_lighting_7(gpu3d_t *p1) __asm__("gpu3d_geometry_apply_lighting");
extern void  gpu3d_gxfifo_pack_command_7(gpu3d_t *ctx, uint64_t param_2, uint32_t param_3) __asm__("gpu3d_gxfifo_pack_command");




void gpu3d_gxfifo_frame_end(gpu3d_t *ctx) {

    uint32_t ctrl = ctx->clear_color;
    uint32_t clip = ctx->clear_depth & 0x7fffu;

    uint32_t a = (clip == 0x7fffu) ? 0xffffffu : (clip << 9);
    a |= ctrl & 0x3f000000u;

    uint32_t b = gpu3d_geometry_color_expand_bgr555_to_rgb8_alpha(ctrl, (ctrl >> 16) & 0x1fu);
    b |= (ctrl << 16) & 0x80000000u;

    uint8_t *regs = (uint8_t *)&ctx->machine->gpu.raster.clear_color_rgb;
    if (rd32(regs + 4) != a) {
        wr32(regs + 4, a);
        ctx->render_dirty = 1;
    }
    if (rd32(regs) != b) {
        wr32(regs, b);
        ctx->render_dirty = 1;
    }

    gpu3d_gxfifo_trace_buffer_rewind(ctx);
    gpu3d_geometry_apply_lighting(ctx);
    gpu3d_poly_commit_pending_batch(ctx);

    if (ctx->swap_pending != 0) {

        ctx->render_dirty = 1;
        uint8_t t1 = ctx->swap_params, t2 = ctx->swap_params_requested;
        ctx->swap_params_previous = t1;
        ctx->swap_params = t2;

        uint64_t updated = (uint64_t)ctx->bank ^ 1u;
        ctx->bank = (uint8_t)updated;

        ctx->vertex_bank[updated].count = 0;
        ctx->opaque[updated].count = 0;
        ctx->translucent[updated].count = 0;

        ctx->polygon_count = 0;
        ctx->swap_pending = 0;
        recon_scale_note_swap();
    }

    if (ctx->side_command_pending != 0) {
        uint8_t *bk = (uint8_t *)ctx->machine;
        arm_t *arm9 = &ctx->machine->arm9;
        io_mirror_t *sub = arm9->io_mirror;

        uint32_t p1 = ctx->side_command[0];
        uint32_t p2 = ctx->side_command[1];
        gpu3d_gxfifo_pack_command(ctx, p1, p2);

        arm9->wake_flags = 0;
        arm9->halt_flags = arm9->halt_flags & 0xfffffffbu;

        uint32_t m = sub->irq.ie & sub->irq.if_pending;
        m &= (uint32_t)(0u - sub->irq.ime);
        arm9->irq_pending = m;

        ctx->side_command_pending = 0;
        uint32_t n = ((nds_t *)bk)->arm9.pc;
        void *r = jit_cache_lookup_or_compile(arm9, n);
        uint8_t *rp = (uint8_t *)r + 8;
        ((nds_t *)bk)->arm9.jit_block = rp;
    }
}

extern void gpu3d_gxfifo_trace_buffer_rewind_8(gpu3d_t *obj) __asm__("gpu3d_gxfifo_trace_buffer_rewind");
#define RING_LIMIT    0x401
#define TBL_LEN_OFF   0x10e94c



void gpu3d_gxfifo_pack_command(gpu3d_t *ctx, uint64_t param_2, uint32_t param_3) {

    uint8_t remain = ctx->params_remaining;

    if (remain != 0) {

        uint8_t *dw = ctx->param_pending_cursor;
        uint8_t rem = (uint8_t)(remain - 1);

        wr32(dw, param_3);
        dw += 4;
        ctx->param_pending_cursor = dw;

        if (rem != 0) {
            ctx->params_remaining = rem;
            return;
        }

        uint8_t *op = ctx->command_cursor;
        op += 1;
        ctx->param_cursor = dw;
        ctx->command_pending_cursor = op;
        ctx->command_cursor = op;

        if ((long)(op - ctx->command_ring) >= RING_LIMIT)
            gpu3d_gxfifo_trace_buffer_rewind(ctx);

        ctx->params_remaining = rem;
        return;
    }

    uint32_t cmd = (uint32_t)param_2 & 0x7f;
    uint8_t *op = ctx->command_cursor;

    if (ctx->swap_seen != 0) {
        uint8_t *base = (uint8_t *)ctx->machine;
        if (((nds_t *)base)->runtime.gxfifo_swap_side_command != 0) {

            ctx->side_command[0] = cmd;
            ctx->side_command[1] = param_3;
            ctx->side_command_pending = 1;

            arm_t *arm9 = &ctx->machine->arm9;
            uint32_t a = arm9->wake_flags;
            uint32_t b = arm9->halt_flags;
            arm9->irq_pending = 0;
            arm9->wake_flags = a | 0x10u;
            arm9->halt_flags = b | 0x4u;
            return;
        }
    }

    if (cmd == 0x50)
        ctx->swap_seen = 1;

    const uint8_t *tbl_len = gx_param_count;
    uint8_t len = tbl_len[cmd];
    *op = (uint8_t)cmd;

    if (len >= 2) {
        uint8_t *dw = ctx->param_cursor;
        wr32(dw, param_3);
        dw += 4;
        ctx->param_pending_cursor = dw;
        ctx->params_remaining = (uint8_t)(len - 1);
        return;
    }

    op += 1;
    if (len != 0) {
        uint8_t *dw = ctx->param_cursor;
        wr32(dw, param_3);
        dw += 4;
        ctx->param_cursor = dw;
        ctx->param_pending_cursor = dw;
    }

    ctx->command_pending_cursor = op;
    ctx->command_cursor = op;

    if ((long)(op - ctx->command_ring) >= RING_LIMIT)
        gpu3d_gxfifo_trace_buffer_rewind(ctx);

    if (ctx->swap_seen != 0) {
        gpu3d_gxfifo_trace_buffer_rewind(ctx);
        ctx->swap_seen = 1;
        return;
    }

    ctx->params_remaining = 0;
}
#undef RING_LIMIT
#undef TBL_LEN_OFF

#define PARAM_TABLE   0x10e94c
#define MARGIN         0x401
#define OFF_FLUSH     0x63aec
typedef void (*fn_clear)(gpu3d_t *);

void gpu3d_gxfifo_push_word(gpu3d_t *state, unsigned word) {

    unsigned pending = state->params_remaining;
    fn_clear clear = gpu3d_gxfifo_trace_buffer_rewind;

    if (pending != 0) {
        uint8_t *p;
        p = state->param_pending_cursor;
        __builtin_memcpy(p, &word, 4);
        p += 4;
        state->param_pending_cursor = p;

        unsigned left = pending - 1;
        if (left == 0) {
            uint8_t *cmd;
            cmd = state->command_pending_cursor;
            int64_t used = (int64_t)(cmd - state->command_ring);
            state->command_cursor = cmd;
            state->param_cursor = p;
            if (used >= MARGIN) clear(state);
        }
        state->params_remaining = (uint8_t)left;
        return;
    }

    const uint8_t *table = gx_param_count;
    uint8_t *cmd = state->command_cursor;

    unsigned total = 0;
    for (int k = 0; k < 4; k++) {
        unsigned c = (word >> (8 * k)) & 0x7f;
        total += table[c];
        if (c != 0) *cmd++ = (uint8_t)c;
    }
    state->command_pending_cursor = cmd;

    if (total != 0) {
        state->params_remaining = (uint8_t)total;
        return;
    }

    int64_t used = (int64_t)(cmd - state->command_ring);
    state->command_cursor = cmd;
    if (used >= MARGIN) clear(state);
}
#undef PARAM_TABLE
#undef MARGIN
#undef OFF_FLUSH

#define OFF_TABLE_PARAMS 0x10e94cu
#define BATCH_CAP 0x400u
#define FLUSH_FN_OFF 0x63aecu

static void copy_words_fwd(uint8_t *dst, const uint8_t *src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t v = rd32(src);
        wr32(dst, v);
        src += 4;
        dst += 4;
    }
}

void gpu3d_gxfifo_decode_commands(gpu3d_t *ctx, uint32_t *param_2, uint32_t param_3)
{

    uint8_t *x1  = (uint8_t *)param_2;
    uint32_t w2  = param_3;

    uint8_t *x8  = ctx->command_pending_cursor;
    uint8_t  w9  = ctx->params_remaining;
    uint8_t *x11 = ctx->param_pending_cursor;

    if (w9 != 0) {
        uint32_t carry = (uint32_t)w9;

        if (carry > w2) {

            copy_words_fwd(x11, x1, w2);
            x11 += (size_t)w2 * 4u;
            ctx->params_remaining = (uint8_t)(carry - w2);
            ctx->param_pending_cursor = x11;
            return;
        }

        copy_words_fwd(x11, x1, carry);
        {
            uint8_t *x10 = x11 + (size_t)carry * 4u;
            x1 += (size_t)carry * 4u;
            w2 -= carry;

            if (w2 == 0) {

                ctx->params_remaining = 0;
                ctx->param_pending_cursor = x10;
                ctx->command_cursor = x8;
                ctx->param_cursor = x10;
                return;
            }

            x11 = x10;
        }
    }

    {
        uint8_t *cmd_base  = (unsigned char *)ctx->command_ring;
        uint8_t *data_base = (unsigned char *)ctx->param_ring;
        const uint8_t *table = gx_param_count;

        uint8_t *x10 = x11;
        uint8_t *x21 = x1;
        uint32_t w11_batch = (uint32_t)(x8 - cmd_base);

        for (;;) {
            uint32_t word = rd32(x21);
            uint8_t *x9 = x8;

            uint32_t op1 = word & 0x7fu;
            uint32_t op2 = (word >> 8) & 0x7fu;
            uint32_t op3 = (word >> 16) & 0x7fu;
            uint32_t op4 = (word >> 24) & 0x7fu;

            uint8_t p1 = table[op1];
            uint8_t p2 = table[op2];
            if (op1 != 0) { wr8(x9, (uint8_t)op1); x9 += 1; }
            uint8_t p3 = table[op3];
            uint32_t sum = (uint32_t)p1 + p2;
            if (op2 != 0) { wr8(x9, (uint8_t)op2); x9 += 1; }
            uint8_t p4 = table[op4];
            sum += p3;
            if (op3 != 0) { wr8(x9, (uint8_t)op3); x9 += 1; }
            sum += p4;
            if (op4 != 0) { wr8(x9, (uint8_t)op4); x9 += 1; }

            uint32_t total  = sum;
            uint32_t budget = w2 - 1u;
            uint8_t *x12 = x21 + 4;

            if (budget < total) {

                ctx->param_cursor = x10;
                copy_words_fwd(x10, x12, budget);
                x10 += (size_t)budget * 4u;

                ctx->command_cursor = x8;
                ctx->params_remaining = (uint8_t)(total - budget);
                ctx->command_pending_cursor = x9;
                ctx->param_pending_cursor = x10;
                return;
            }

            copy_words_fwd(x10, x12, total);
            x10 += (size_t)total * 4u;
            x21 = x12 + (size_t)total * 4u;

            {
                uint32_t remain = budget - total;
                x8 = x9;

                w11_batch++;
                if (w11_batch >= BATCH_CAP) {

                    ctx->command_cursor = x8;
                    ctx->param_cursor = x10;
                    ctx->command_pending_cursor = x8;
                    ctx->param_pending_cursor = x10;
                    gpu3d_gxfifo_trace_buffer_rewind(ctx);
                    w11_batch = 0;
                    x10 = data_base;
                    x8  = cmd_base;
                }

                w2 = remain;
                if (remain == 0) {

                    ctx->command_cursor = x8;
                    ctx->param_cursor = x10;
                    ctx->command_pending_cursor = x8;
                    ctx->param_pending_cursor = x10;
                    ctx->params_remaining = 0;
                    return;
                }

            }
        }
    }
}
#undef OFF_TABLE_PARAMS
#undef BATCH_CAP
#undef FLUSH_FN_OFF






static void copy_words_forward(uint8_t *dst, const uint8_t *src, uint32_t n)
{
    uint32_t i;

    for (i = 0; i < n; i++) {
        uint32_t v = rd32(src);
        memcpy(dst, &v, sizeof(v));
        src += 4;
        dst += 4;
    }
}

uint32_t gpu3d_gxfifo_decode_commands_with_cycles(gpu3d_t *ctx, uint32_t *param_2, uint32_t param_3)
{

    uint8_t *entry = (uint8_t *)param_2;
    uint32_t available = param_3;
    uint8_t *cmd = ctx->command_pending_cursor;
    uint8_t carry = ctx->params_remaining;
    uint8_t *data = ctx->param_pending_cursor;

    if (carry != 0) {
        uint32_t ncarry = carry;

        if (ncarry > available) {
            copy_words_forward(data, entry, available);
            data += (size_t)available * 4u;
            ctx->params_remaining = (uint8_t)(ncarry - available);
            ctx->param_pending_cursor = data;
            return 0;
        }

        copy_words_forward(data, entry, ncarry);
        data += (size_t)ncarry * 4u;
        entry += (size_t)ncarry * 4u;
        available -= ncarry;

        if (available == 0) {
            ctx->params_remaining = 0;
            ctx->param_pending_cursor = data;
            ctx->command_cursor = cmd;
            ctx->param_cursor = data;
            ctx->command_pending_cursor = cmd;
            return 0;
        }
    }

    {
        const uint8_t *table_params = gx_param_count;
        const uint8_t *table_ret = (const uint8_t *)gx_command_return_words;
        uint8_t *base_cmd = (unsigned char *)ctx->command_ring;
        uint8_t *base_data = (unsigned char *)ctx->param_ring;
        uint32_t groups = (uint32_t)(cmd - base_cmd);
        uint32_t ret = 0;

        for (;;) {
            uint32_t word = rd32(entry);
            uint32_t op1 = word & 0x7fu;
            uint32_t op2 = (word >> 8) & 0x7fu;
            uint32_t op3 = (word >> 16) & 0x7fu;
            uint32_t op4 = (word >> 24) & 0x7fu;
            uint16_t r1 = rd16(table_ret + op1 * 2u);
            uint8_t p1 = rd8(table_params + op1);
            uint8_t p2 = rd8(table_params + op2);
            uint16_t r2 = rd16(table_ret + op2 * 2u);
            uint8_t p3 = rd8(table_params + op3);
            uint16_t r3 = rd16(table_ret + op3 * 2u);
            uint8_t p4 = rd8(table_params + op4);
            uint16_t r4 = rd16(table_ret + op4 * 2u);
            uint8_t *cmd_updated = cmd;
            uint32_t total;
            uint32_t budget;
            uint8_t *params;

            ret += r1;
            if (op1 != 0) {
                wr8(cmd_updated, (uint8_t)op1);
                cmd_updated++;
            }

            total = (uint32_t)p1 + p2;
            ret += r2;
            if (op2 != 0) {
                wr8(cmd_updated, (uint8_t)op2);
                cmd_updated++;
            }

            total += p3;
            ret += r3;
            if (op3 != 0) {
                wr8(cmd_updated, (uint8_t)op3);
                cmd_updated++;
            }

            total += p4;
            ret += r4;
            if (op4 != 0) {
                wr8(cmd_updated, (uint8_t)op4);
                cmd_updated++;
            }

            budget = available - 1u;
            params = entry + 4;
            if (budget < total) {
                ctx->param_cursor = data;
                copy_words_forward(data, params, budget);
                data += (size_t)budget * 4u;
                ctx->command_cursor = cmd;
                ctx->params_remaining = (uint8_t)(total - budget);
                ctx->command_pending_cursor = cmd_updated;
                ctx->param_pending_cursor = data;
                return ret;
            }

            copy_words_forward(data, params, total);
            data += (size_t)total * 4u;
            entry = params + (size_t)total * 4u;
            groups++;

            if (groups >= 0x400u) {
                ctx->command_cursor = cmd_updated;
                ctx->param_cursor = data;
                ctx->command_pending_cursor = cmd_updated;
                ctx->param_pending_cursor = data;
                gpu3d_gxfifo_trace_buffer_rewind(ctx);
                groups = 0;
                data = base_data;
                cmd = base_cmd;
            } else {
                cmd = cmd_updated;
            }

            available = budget - total;
            if (available == 0) {
                ctx->command_cursor = cmd;
                ctx->param_cursor = data;
                ctx->command_pending_cursor = cmd;
                ctx->param_pending_cursor = data;
                ctx->params_remaining = 0;
                return ret;
            }
        }
    }
}

void gpu3d_context_subblock_init(gpu3d_t *obj, nds_t *machine, gpu3d_texture_cache_t *texture_cache) {
    obj->machine = machine;
    obj->texture_cache = texture_cache;
    obj->position_matrix_ptr = obj->position_matrix;
    obj->vector_matrix_ptr = obj->vector_matrix;
    obj->unmapped_4 = 0;
}

static void *(*core_memset)(void *, int, size_t);

static void zero(unsigned char *p, size_t n) {
    for (size_t i = 0; i < n; i++) p[i] = 0;
}

void *gpu3d_context_init(gpu3d_t *o) {
    if (!core_memset)
        core_memset = (void *(*)(void *, int, size_t))sym_libc_memset;

    o->normal_cursor = (unsigned char *)o->batch_normal;
    o->color_cursor = (unsigned char *)o->batch_color;
    o->attribute_mark_cursor = (unsigned char *)o->batch_attribute_mark;
    o->attribute_mark = 255;
    o->texture_run_count = 0;
    zero((unsigned char *)&o->run_texture_param, 8);
    o->last_color = 0;

    void *r = core_memset(o->matrix_stack, 0, sizeof o->matrix_stack);

    uint64_t constant = 0x7fff000000000000ULL;

    zero((unsigned char *)o->projection_stack, 64);
    zero((unsigned char *)o->position_matrix, 128);
    zero((unsigned char *)o->projection_matrix, 64);
    zero((unsigned char *)o->texture_matrix, 64);
    zero((unsigned char *)o->clip_matrix, 64);
    zero((unsigned char *)o->shininess_table, 128);

    zero(&o->params_remaining, 2);
    zero(&o->position_stack_level, 2);
    o->texture_stack_level = 0;
    zero((unsigned char *)&o->specular_color, 8);
    zero((unsigned char *)&o->specular_emission_raw, 16);

    o->command_cursor = o->command_ring;
    o->param_cursor = (unsigned char *)o->param_ring;
    o->command_pending_cursor = o->command_ring;
    o->param_pending_cursor = (unsigned char *)o->param_ring;

    o->vertex_bank[0].count = 0;
    o->opaque[0].count = 0;
    o->translucent[0].count = 0;

    zero((unsigned char *)o->edge_color, 16);
    zero((unsigned char *)o->toon_table, 64);
    zero((unsigned char *)o->toon_table_expanded[0], 96);
    zero((unsigned char *)o->fog_table, 32);
    zero((unsigned char *)o->unmapped_3, 32);

    zero((unsigned char *)o->texcoord, 8);
    zero((unsigned char *)&o->disp3dcnt, 16);
    memcpy(&o->polygon_count, &constant, 8);

    zero(&o->texture_stack_level, 4);
    zero((unsigned char *)&o->clear_image_offset, 32);

    o->swap_params_previous = 0;
    o->swap_params = 0;
    o->shininess_enabled = 0;
    o->render_dirty = 0;
    o->clip_matrix_dirty = 1;
    o->light_dirty = 0x0f;
    o->unmapped_6[0] = 0;

    return r;
}



void gpu3d_state_read_record32(unsigned char *param_1, unsigned char *param_2)
{

    unsigned char *field_cursor = param_1 + 0x20;
    unsigned char *x8;
    unsigned char *x9;
    unsigned char block16[16];
    unsigned int word32;
    unsigned short media16;
    unsigned char byte8;

    x8 = (unsigned char *)rd64(field_cursor);
    memcpy(block16, x8, 16);
    memcpy(param_2 + 0, block16, 16);

    x8 = (unsigned char *)rd64(field_cursor);
    x9 = x8 + 0x10;
    wr64(field_cursor, (unsigned long)x9);
    memcpy(&word32, x8 + 16, 4);
    memcpy(param_2 + 16, &word32, 4);

    x8 = (unsigned char *)rd64(field_cursor);
    x9 = x8 + 0x4;
    wr64(field_cursor, (unsigned long)x9);
    memcpy(&word32, x8 + 4, 4);
    memcpy(param_2 + 20, &word32, 4);

    x8 = (unsigned char *)rd64(field_cursor);
    x9 = x8 + 0x4;
    wr64(field_cursor, (unsigned long)x9);
    memcpy(&media16, x8 + 4, 2);
    memcpy(param_2 + 24, &media16, 2);

    x8 = (unsigned char *)rd64(field_cursor);
    x9 = x8 + 0x2;
    wr64(field_cursor, (unsigned long)x9);
    memcpy(&media16, x8 + 2, 2);
    memcpy(param_2 + 26, &media16, 2);

    x8 = (unsigned char *)rd64(field_cursor);
    x9 = x8 + 0x2;
    wr64(field_cursor, (unsigned long)x9);
    memcpy(&media16, x8 + 2, 2);
    memcpy(param_2 + 28, &media16, 2);

    x8 = (unsigned char *)rd64(field_cursor);
    x9 = x8 + 0x2;
    wr64(field_cursor, (unsigned long)x9);
    byte8 = *(x8 + 2);
    *(param_2 + 30) = byte8;

    x8 = (unsigned char *)rd64(field_cursor);
    x9 = x8 + 0x1;
    wr64(field_cursor, (unsigned long)x9);
    byte8 = *(x8 + 1);
    *(param_2 + 31) = byte8;

    x8 = (unsigned char *)rd64(field_cursor);
    x9 = x8 + 0x1;
    wr64(field_cursor, (unsigned long)x9);
}

static void header_write_vertex_word(unsigned char *dst, uint32_t v)
{
    memcpy(dst + 8, &v, 4);
}

void gpu3d_polygon_header_classify(unsigned char *dst,
                        const unsigned char *src)
{

    uint32_t w;
    uint16_t h;

    memcpy(&w, src + 0, 4);

    uint32_t cnt = (uint32_t)src[30];

    memcpy(dst + 4, &w, 4);

    memcpy(&w, src + 24, 4);
    memcpy(dst + 0, &w, 4);

    memcpy(&h, src + 28, 2);
    memcpy(dst + 24, &h, 2);

    memcpy(&h, src + 4, 2);
    memcpy(dst + 26, &h, 2);

    if (cnt == 4) {

        int32_t b = rd16s_at(src, 4);
        int32_t a = rd16s_at(src, 6);

        if ((uint32_t)b + 1u != (uint32_t)a) {
            header_write_vertex_word(dst, 4);
            return;
        }

        int32_t d = rd16s_at(src, 10);

        uint32_t class = 4;

        if ((uint32_t)a + 1u != (uint32_t)d) {
            header_write_vertex_word(dst, class);
            return;
        }

        int32_t e = rd16s_at(src, 8);

        if ((uint32_t)d + 1u == (uint32_t)e)
            class = 0x44;

        header_write_vertex_word(dst, class);
        return;
    }

    if (cnt != 3) {

        header_write_vertex_word(dst, cnt);
        return;
    }

    int32_t a = rd16s_at(src, 6);
    int32_t b = rd16s_at(src, 4);

    if ((uint32_t)a + 1u != (uint32_t)b) {
        header_write_vertex_word(dst, 3);
        return;
    }

    int32_t c = rd16s_at(src, 8);
    if ((uint32_t)b + 1u != (uint32_t)c) {
        header_write_vertex_word(dst, 3);
        return;
    }

    h = (uint16_t)((uint32_t)a & 0xffffu);
    memcpy(dst + 26, &h, 2);

    header_write_vertex_word(dst, 0x43);
}

void gpu3d_context_store_descriptor_entry(uint8_t *param_1, uint32_t param_2, uint8_t *param_3)
{

    uint8_t *t1 = param_1 + (uint64_t)param_2 * 16;
    uint32_t w0v, w1v, w2v, w3v;
    memcpy(&w0v, param_3 + 0,  4);
    memcpy(&w1v, param_3 + 4,  4);
    memcpy(&w2v, param_3 + 8,  4);
    memcpy(&w3v, param_3 + 12, 4);
    memcpy(t1 + 1616, &w0v, 4);
    memcpy(t1 + 1620, &w1v, 4);
    memcpy(t1 + 1624, &w2v, 4);
    memcpy(t1 + 1628, &w3v, 4);

    uint32_t v;
    memcpy(&v, param_3 + 20, 4);

    uint8_t *t2 = param_1 + (uint64_t)param_2 * 2;
    uint16_t v16 = (uint16_t)v;
    memcpy(t2 + 4948, &v16, 2);

    uint8_t *t3 = param_1 + (uint64_t)param_2 * 4;
    memcpy(t3 + 5340, &v, 4);

    uint8_t b = *(param_3 + 31);
    uint8_t *t4 = param_1 + (uint64_t)param_2;
    t4[0x1290] = b;
}

typedef void *(*fnp_memcpy)(void *, const void *, size_t);
static fnp_memcpy libc_memcpy;

static void copy_bytes(void *d, const void *s, size_t n)
{
    libc_memcpy(d, s, n);
}

static uint8_t state_rd8(const void *p)
{
    uint8_t v;
    copy_bytes(&v, p, 1);
    return v;
}

static uint16_t state_rd16(const void *p)
{
    uint16_t v;
    copy_bytes(&v, p, 2);
    return v;
}

static int16_t state_rd16s(const void *p)
{
    int16_t v;
    copy_bytes(&v, p, 2);
    return v;
}

static uint32_t state_rd32(const void *p)
{
    uint32_t v;
    copy_bytes(&v, p, 4);
    return v;
}

static uint64_t state_rd64(const void *p)
{
    uint64_t v;
    copy_bytes(&v, p, 8);
    return v;
}

static void state_wr8(void *p, uint32_t v)
{
    uint8_t b = (uint8_t)v;
    copy_bytes(p, &b, 1);
}

static void state_wr16(void *p, uint32_t v)
{
    uint16_t short_form = (uint16_t)v;
    copy_bytes(p, &short_form, 2);
}

static void state_wr32(void *p, uint32_t v)
{
    copy_bytes(p, &v, 4);
}

static void state_wr64(void *p, uint64_t v)
{
    copy_bytes(p, &v, 8);
}

static uint8_t *reader_cursor(uint8_t *reader)
{
    return (uint8_t *)(uintptr_t)state_rd64(reader + 32);
}

static void reader_set_cursor(uint8_t *reader, uint8_t *p)
{
    state_wr64(reader + 32, (uint64_t)(uintptr_t)p);
}

static uint8_t *reader_advance(uint8_t *reader, size_t n)
{
    uint8_t *p = reader_cursor(reader) + n;
    reader_set_cursor(reader, p);
    return p;
}

static void vertex_bank_unpack_records(uint8_t *dst, const uint8_t *src, uint32_t n)
{
    for (; n; n--, src += 32, dst += 16) {
        uint32_t a = (uint32_t)((int32_t)state_rd32(src + 16) >> 8);
        state_wr32(dst, state_rd32(src + 12));
        state_wr16(dst + 4, state_rd16(src + 28));
        state_wr16(dst + 6, state_rd8(src + 30));
        state_wr16(dst + 8, ((a << 15) - a) >> 16);
        state_wr16(dst + 10, state_rd32(src + 20));
        state_wr16(dst + 12, state_rd16(src + 24));
        state_wr16(dst + 14, state_rd16(src + 26));
    }
}

static void polygon_list_unpack_records(uint8_t *dst, const uint8_t *base, uint32_t n,
                          int back)
{
    uint32_t i;
    int64_t idx = 0x7ff;

    for (i = 0; i < n; i++, dst += 32) {
        const uint8_t *src = base +
            (size_t)(back ? (uint32_t)(uint64_t)idx : i) * 36u;
        uint32_t class = state_rd8(src + 30);
        int32_t s4 = state_rd16s(src + 4);
        uint32_t out = class;

        state_wr32(dst, state_rd32(src + 24));
        state_wr32(dst + 4, state_rd32(src));
        state_wr16(dst + 24, state_rd16(src + 28));
        state_wr16(dst + 26, (uint32_t)s4);

        if (class == 4) {
            int32_t s6 = state_rd16s(src + 6);
            out = 4;
            if (s4 + 1 == s6) {
                int32_t s10 = state_rd16s(src + 10);
                if (s6 + 1 == s10) {
                    int32_t s8 = state_rd16s(src + 8);
                    if (s10 + 1 == s8)
                        out = 0x44;
                }
            }
        } else if (class == 3) {
            int32_t s6 = state_rd16s(src + 6);
            out = 3;
            if (s6 + 1 == s4) {
                int32_t s8 = state_rd16s(src + 8);
                if (s4 + 1 == s8) {
                    state_wr16(dst + 26, (uint32_t)s6);
                    out = 0x43;
                }
            }
        }
        state_wr32(dst + 8, out);
        if (back)
            idx--;
    }
}

static uint8_t color5_to_6_r(uint32_t v)
{
    uint32_t t = (v << 1) & 0x3eu;
    return t ? (uint8_t)(t | 1u) : 0;
}

static uint8_t color5_to_6_g(uint32_t v)
{
    uint32_t t = (v >> 4) & 0x3eu;
    return t ? (uint8_t)(t | 1u) : 0;
}

static uint8_t color5_to_6_b(uint32_t v)
{
    uint32_t t = (v >> 9) & 0x3eu;
    return t ? (uint8_t)(t | 1u) : 0;
}

void gpu3d_state_read_block(void *param_1, void *param_2)
{

    gpu3d_t *ctx = (gpu3d_t *)param_1;
    uint8_t *reader = (uint8_t *)param_2;
    gpu3d_state_blocks_t frame;
    uint8_t *header = frame.header;
    const uint8_t *table;
    uint8_t *p, *end;
    uint32_t sp20, sp52, sp68, w26;
    uint32_t sel, len_a, len_b, idx;
    uint32_t h0, h1, h2, h3, h4, h5;
    uint32_t t0, t1, t2, t3, count;
    uint32_t i, va, vb;

    if (!libc_memcpy)
        libc_memcpy = (fnp_memcpy)sym_libc_memcpy;

    for (i = 0; i != 2; i++) {
        uint32_t j;
        uint8_t *regs = frame.vertices[i];
        for (j = 0; j != 0x1810u; j++)
            gpu3d_state_read_record32(reader, regs + (size_t)j * GPU3D_STATE_PACKED_VERTEX_BYTES);

        p = reader_cursor(reader);
        for (j = 0; j != 0x800u; j++) {
            uint8_t *q = frame.polygons[i] + (size_t)j * GPU3D_STATE_PACKED_POLYGON_BYTES;
            state_wr32(q, state_rd32(p));
            state_wr16(q + 4, state_rd16(p + 4));
            state_wr16(q + 6, state_rd16(p + 6));
            state_wr16(q + 8, state_rd16(p + 8));
            state_wr16(q + 10, state_rd16(p + 10));
            state_wr16(q + 12, state_rd16(p + 12));
            state_wr16(q + 14, state_rd16(p + 14));
            state_wr16(q + 16, state_rd16(p + 16));
            state_wr16(q + 18, state_rd16(p + 18));
            state_wr16(q + 20, state_rd16(p + 20));
            state_wr32(q + 24, state_rd32(p + 22));
            state_wr16(q + 28, state_rd16(p + 26));
            state_wr8(q + 30, state_rd8(p + 28));
            state_wr8(q + 31, state_rd8(p + 29));
            state_wr8(q + 32, state_rd8(p + 30));
            p += 31;
            reader_set_cursor(reader, p);
        }
    }

    end = reader_cursor(reader);
    state_wr32(frame.command_word, state_rd32(end));
    p = reader_advance(reader, 4);
    copy_bytes(header, p, 0x200);
    p = reader_advance(reader, 0x200);
    copy_bytes((unsigned char *)ctx->matrix_stack, p, 0x1000);

    p = reader_advance(reader, 0x1000);
    copy_bytes((unsigned char *)ctx->projection_stack, p, 64);
    p = reader_advance(reader, 0x40);
    copy_bytes((unsigned char *)ctx->position_matrix, p, 128);
    p = reader_advance(reader, 0x80);
    copy_bytes((unsigned char *)ctx->projection_matrix, p, 64);
    p = reader_advance(reader, 0x40);
    copy_bytes((unsigned char *)ctx->texture_matrix, p, 64);
    p = reader_advance(reader, 0x40);
    copy_bytes((unsigned char *)ctx->clip_matrix, p, 64);

    (void)reader_advance(reader, 0x40);
    gpu3d_state_read_record32(reader, frame.pending_vertices[0]);
    gpu3d_state_read_record32(reader, frame.pending_vertices[1]);
    copy_bytes((unsigned char *)ctx->shininess_table, reader_cursor(reader), 128);

    p = reader_advance(reader, 0x80);
    copy_bytes((unsigned char *)ctx->light_direction[0], p, 48);
    p = reader_advance(reader, 0x30);
    copy_bytes((unsigned char *)ctx->light_half_vector[0], p, 48);
    p = reader_advance(reader, 0x30);
    copy_bytes((unsigned char *)ctx->light_color, p, 16);

    p = reader_advance(reader, 0x10);
    ctx->diffuse_color = state_rd32(p);
    p = reader_advance(reader, 4);
    ctx->ambient_color = state_rd32(p);
    p = reader_advance(reader, 4);
    ctx->specular_color = state_rd32(p);
    p = reader_advance(reader, 4);
    ctx->emission_color = state_rd32(p);
    p = reader_advance(reader, 4);
    copy_bytes((unsigned char *)ctx->edge_color, p, 16);
    p = reader_advance(reader, 0x10);
    copy_bytes((unsigned char *)ctx->toon_table, p, 64);
    p = reader_advance(reader, 0x40);
    copy_bytes((unsigned char *)ctx->fog_table, p, 32);

    p = reader_advance(reader, 0x20);
    ctx->disp3dcnt = state_rd32(p);
    p = reader_advance(reader, 4);
    ctx->clear_color = state_rd32(p);
    p = reader_advance(reader, 4);
    ctx->fog_color = state_rd32(p);

    p = reader_advance(reader, 4);
    w26 = state_rd32(p);
    p = reader_advance(reader, 4);
    ctx->polygon_attr = state_rd32(p);
    p = reader_advance(reader, 4);
    ctx->texture_param = state_rd32(p);

    p = reader_advance(reader, 4);
    ctx->clear_depth = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->clear_image_offset = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->fog_offset = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->dot_depth = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->texcoord_raw[0] = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->texcoord_raw[1] = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->texcoord[0] = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->texcoord[1] = state_rd16(p);

    p = reader_cursor(reader);
    sp68 = state_rd16(p + 2);
    ctx->texture_palette_base = state_rd16(p + 4);
    reader_set_cursor(reader, p + 4);

    p = reader_cursor(reader);
    h0 = state_rd16(p + 2);
    h1 = state_rd16(p + 4);
    h2 = state_rd16(p + 6);
    h3 = state_rd16(p + 8);
    h4 = state_rd16(p + 10);
    h5 = state_rd16(p + 12);
    sp20 = h5;
    ctx->vertex_xyz[0] = state_rd16(p + 14);
    reader_set_cursor(reader, p + 14);

    p = reader_advance(reader, 2);
    ctx->vertex_xyz[1] = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->vertex_xyz[2] = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->viewport_size[0] = state_rd16(p);
    p = reader_advance(reader, 2);
    ctx->alpha_test_ref = state_rd8(p);

    p = reader_cursor(reader);
    idx = state_rd8(p + 1);
    sp52 = state_rd8(p + 2);
    ctx->viewport_size[1] = state_rd8(p + 3);
    ctx->viewport_origin[0] = state_rd8(p + 4);
    ctx->viewport_origin[1] = state_rd8(p + 5);
    ctx->bank = state_rd8(p + 6);
    reader_set_cursor(reader, p + 6);

    p = reader_cursor(reader);
    len_a = state_rd8(p + 1);
    len_b = state_rd8(p + 3);
    ctx->matrix_mode = state_rd8(p + 5);
    reader_set_cursor(reader, p + 5);

    p = reader_advance(reader, 1); ctx->texcoord_mode = state_rd8(p);
    p = reader_advance(reader, 1); ctx->position_stack_level = state_rd8(p);
    p = reader_advance(reader, 1); ctx->projection_stack_level = state_rd8(p);
    p = reader_advance(reader, 1); ctx->swap_pending = state_rd8(p);
    p = reader_advance(reader, 1); ctx->swap_params_previous = state_rd8(p);
    p = reader_advance(reader, 1); ctx->swap_params = state_rd8(p);
    p = reader_advance(reader, 1); ctx->shininess_enabled = state_rd8(p);
    (void)reader_advance(reader, 1);

    sel = ctx->bank;
    ctx->vertex_bank[sel].count = h5;
    ctx->vertex_bank[sel ^ 1u].count = h2;

    vertex_bank_unpack_records((unsigned char *)ctx->vertex_bank[0].vertex, frame.vertices[0], ctx->vertex_bank[0].count);
    vertex_bank_unpack_records((unsigned char *)ctx->vertex_bank[1].vertex, frame.vertices[1],
                  ctx->vertex_bank[1].count);

    {
        uint32_t other = sel ^ 1u;
        ctx->opaque[sel].count = h3;
        ctx->opaque[other].count = h0;
        ctx->translucent[sel].count = 0x7ffu - h4;
        ctx->translucent[other].count = 0x7ffu - h1;
    }

    polygon_list_unpack_records((unsigned char *)ctx->opaque[0].polygon, frame.polygons[0], ctx->opaque[0].count, 0);
    polygon_list_unpack_records((unsigned char *)ctx->translucent[0].polygon, frame.polygons[0], ctx->translucent[0].count, 1);
    polygon_list_unpack_records((unsigned char *)ctx->opaque[1].polygon, frame.polygons[1],
                  ctx->opaque[1].count, 0);
    polygon_list_unpack_records((unsigned char *)ctx->translucent[1].polygon, frame.polygons[1],
                  ctx->translucent[1].count, 1);

    copy_bytes(ctx->command_ring, frame.command_word, len_a);
    copy_bytes(ctx->param_ring, header, len_b);
    ctx->command_cursor = ctx->command_ring;
    ctx->param_cursor = (unsigned char *)ctx->param_ring;
    ctx->command_pending_cursor = ctx->command_ring + len_a;
    ctx->param_pending_cursor = (unsigned char *)ctx->param_ring + (size_t)len_b * 4u;
    ctx->attribute_mark = 0xff;
    ctx->attribute_mark_cursor = ctx->batch_attribute_mark;
    ctx->normal_cursor = (unsigned char *)ctx->batch_normal;
    ctx->color_cursor = (unsigned char *)ctx->batch_color;
    ctx->vertex_count = 0;
    ctx->last_color = sp68;

    table = gxfifo_state_primitive_table + (size_t)idx * 16u;
    t0 = state_rd32(table);
    t2 = state_rd32(table + 8);
    t3 = state_rd32(table + 12);

    ctx->run_texture_param = ctx->texture_param;
    copy_bytes((unsigned char *)&ctx->batch_count, gx_batch_reset, 16);
    ctx->primitive_run_count = 1;
    ctx->run_palette_base = ctx->texture_palette_base;
    ctx->primitive_run[0].polygon_attr = w26;
    ctx->primitive_run[0].first_vertex = 0;
    ctx->primitive_run[0].type = t3;
    ctx->primitive_run[0].flipped = t2;

    count = t0 - sp52;

    if ((UINT64_C(0x2c0) >> (idx & 63u)) & 1u) {
        const uint8_t *e = frame.pending_vertices[0];
        uint32_t v;
        copy_bytes((unsigned char *)&ctx->vertex[0], e, 16);
        v = state_rd32(e + 20);
        ctx->vertex_color[0] = v;
        ctx->vertex_texcoord[0] = v;
        ctx->clip_code[0] = state_rd8(e + 31);
        copy_bytes((unsigned char *)&ctx->vertex[1], e + 32, 16);
        v = state_rd32(e + 52);
        ctx->vertex_color[1] = v;
        ctx->vertex_texcoord[1] = v;
        ctx->clip_code[1] = state_rd8(e + 63);
    }

    t1 = state_rd32(table + 4);

    if (count) {
        uint32_t n = count;
        uint32_t reg = sp20 - 1u;
        uint32_t k = t1;
        for (; n; n--) {
            uint32_t s = ctx->bank;
            const uint8_t *r = frame.vertices[s] +
                               (size_t)reg * GPU3D_STATE_PACKED_VERTEX_BYTES;
            uint32_t v;
            reg++;
            copy_bytes((unsigned char *)&ctx->vertex[k], r, 16);
            v = state_rd32(r + 20);
            ctx->vertex_color[k] = (uint16_t)v;
            ctx->vertex_texcoord[k] = v;
            ctx->clip_code[k] = state_rd8(r + 31);
            k++;
        }
    }

    ctx->vertex_count = t1 + count;
    ctx->vertex_total = t1 + count;

    {
        uint8_t *base = (unsigned char *)ctx->light_color;
        uint32_t a = ctx->diffuse_color;
        uint32_t b = ctx->specular_color;
        uint32_t aa[3] = { a & 31u, (a >> 5) & 31u, (a >> 10) & 31u };
        uint32_t bb[3] = { b & 31u, (b >> 5) & 31u, (b >> 10) & 31u };
        uint32_t r, s;

        for (r = 0; r != 4; r++) {
            uint32_t d = state_rd32(base + r * 4u);
            uint32_t dd[3] = { d & 31u, (d >> 5) & 31u, (d >> 10) & 31u };
            for (s = 0; s != 3; s++) {
                size_t k = (size_t)r * 3u + s;
                state_wr16(base + 0x70 + k * 2u, dd[s] * aa[s]);
                state_wr16(base + 0x88 + k * 2u, dd[s] * bb[s]);
            }
        }

        va = ctx->emission_color;
        vb = ctx->ambient_color;
        {
            uint32_t c0 = (va << 14) & 0x7c000u;
            uint32_t c1 = (va << 9) & 0x7c000u;
            uint32_t c2 = (va << 4) & 0x7c000u;
            uint32_t m0 = (vb << 9) & 0x3e00u;
            uint32_t m1 = (vb << 4) & 0x3e00u;
            uint32_t m2 = (vb >> 1) & 0x3e00u;
            uint32_t bits = ctx->light_mask;
            for (i = 0; i != 8; i++) {
                if (bits & (1u << i)) {
                    uint32_t w = state_rd32(base + i * 4u);
                    c0 += (w & 31u) * m0;
                    c1 += ((w >> 5) & 31u) * m1;
                    c2 += ((w >> 10) & 31u) * m2;
                }
            }
            state_wr32(base + 0xa8, c2);
            state_wr32(base + 0xa0, c0);
            state_wr32(base + 0xa4, c1);
        }
    }

    for (i = 0; i != 32; i++) {
        uint32_t color = state_rd16((unsigned char *)ctx->toon_table + i * 2u);
        state_wr8((unsigned char *)ctx->toon_table_expanded[0] + i, color5_to_6_r(color));
        state_wr8((unsigned char *)ctx->toon_table_expanded[1] + i, color5_to_6_g(color));
        state_wr8((unsigned char *)ctx->toon_table_expanded[2] + i, color5_to_6_b(color));
    }

    ctx->render_dirty = 1;
    ctx->clip_matrix_dirty = 1;
    ctx->light_dirty = 15;
}

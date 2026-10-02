#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <string.h>
#include "gpu3d.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "mem_access.h"

const uint32_t gpu3d_vertex_pattern_table[87] = {
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000210u, 0x00000021u, 0x00000102u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00003210u, 0x00000321u, 0x00001032u, 0x00002103u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00043210u, 0x00004321u,
    0x00010432u, 0x00021043u, 0x00032104u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00543210u, 0x00054321u, 0x00105432u, 0x00210543u, 0x00321054u, 0x00432105u,
    0x00000000u, 0x00000000u, 0x06543210u, 0x00654321u, 0x01065432u, 0x02106543u,
    0x03210654u, 0x04321065u, 0x05432106u, 0x00000000u, 0x76543210u, 0x07654321u,
    0x10765432u, 0x21076543u, 0x32107654u, 0x43210765u, 0x54321076u, 0x65432107u,
    0x00002310u, 0x00000231u, 0x00003102u, 0x00001023u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x0000000eu, 0x00000009u, 0x00000009u,
    0x00000004u, 0x00000007u, 0x0000000cu,
};

#define VX(p) (*(uint16_t *)((p) + 4))
#define VY(p) (*(uint16_t *)((p) + 6))
#define VZ(p) (*(uint16_t *)((p) + 12))
#define VW(p) (*(uint16_t *)((p) + 14))
#define SZ(p) (*(int16_t *)((p) + 12))
#define SW(p) (*(int16_t *)((p) + 14))

void gpu3d_poly_detect_axis_rect_fastpath(unsigned char *o, unsigned char *ent) {

    uint32_t d = *(uint32_t *)(o + 8);
    const uint32_t *table = gpu3d_vertex_pattern_table;
    uint32_t pat = table[(d >> 16) & 0x7f];
    unsigned char *tex = *(unsigned char **)(o + 16);

    unsigned char *p0 = ent + (uint64_t)(pat & 0xf) * 16;
    unsigned char *p1 = ent + (uint64_t)((pat >> 4) & 0xf) * 16;
    unsigned char *p2 = ent + (uint64_t)((pat >> 8) & 0xf) * 16;
    unsigned char *p3 = ent + (uint64_t)(pat >> 12) * 16;

    uint32_t y0 = VY(p0);
    uint32_t y1 = VY(p1);
    uint32_t y3 = VY(p3);

    unsigned char *pa, *pb, *pc;

    if (y1 == y0 && y3 == VY(p2)) {

        uint32_t ax = VX(p0), bx = VX(p1);
        if (ax != VX(p3)) return;
        if (bx != VX(p2)) return;
        if (VW(p0) != VW(p1)) return;
        if (VW(p3) != VW(p2)) return;
        if (VZ(p0) != VZ(p3)) return;
        if (VZ(p1) != VZ(p2)) return;
        int hi = (bx > ax);
        pc = hi ? p3 : p2;
        pb = hi ? p1 : p0;
        pa = hi ? p0 : p1;
    } else {

        if (y3 != y0) return;
        if (VY(p2) != y1) return;
        uint32_t cx = VX(p3), ax = VX(p0);
        if (cx != VX(p2)) return;
        if (ax != VX(p1)) return;
        if (VW(p0) != VW(p3)) return;
        if (VW(p1) != VW(p2)) return;
        if (VZ(p0) != VZ(p1)) return;
        if (VZ(p3) != VZ(p2)) return;
        int hi = (cx > ax);
        pc = hi ? p1 : p2;
        pb = hi ? p3 : p0;
        pa = hi ? p0 : p3;
    }

    int32_t px = VX(pa), qx = VX(pb);
    int32_t pz = SZ(pa), qz = SZ(pb);
    int32_t pw = SW(pa);
    uint32_t ry = VY(pc);
    int32_t rw = SW(pc);

    int32_t dx = qx - px;
    int32_t dz = qz - pz;
    if ((dx << 4) != dz && (dx << 4) != dz + 1) return;

    int32_t dy = (int32_t)(ry - y0);
    int32_t dw = rw - pw;
    uint32_t bad = (uint32_t)((dy << 4) != dw) & (uint32_t)((dy << 4) != dw + 1);

    if (!(d & (1u << 9))) return;
    if (bad & 1) return;
    if ((pw | pz) & 0x80000000u) return;
    if ((d & 0x3000) != 0x3000) return;
    if (*(uint32_t *)(o + 4) & 0x30) return;

    if ((uint32_t)(dx + (pz >> 4)) > (uint32_t)*(uint16_t *)(tex + 64)) return;
    if ((uint32_t)(dy + (pw >> 4)) > (uint32_t)*(uint16_t *)(tex + 66)) return;

    *(uint32_t *)(o + 8) = d | 0x4000;
}
#undef VX
#undef VY
#undef VZ
#undef VW
#undef SZ
#undef SW

unsigned char *gpu3d_geometry_poly_split_runs(gpu3d_t *base, unsigned char *output,
                                  const unsigned char *src, uint32_t step,
                                  const unsigned char *ma, const unsigned char *mb,
                                  uint32_t mask);
void gpu3d_geometry_clip_polygon(gpu3d_t *ctx, const unsigned char *desc, uint32_t n);
void gpu3d_geometry_emit_draw_entries(gpu3d_t *ctx, const unsigned char *desc, uint32_t step,
                        uint32_t x, uint32_t diff, uint32_t alternate);
void gpu3d_raster_compute_reciprocal_block(unsigned int *, unsigned int *, const unsigned int *,
                        unsigned int);
void gpu3d_raster_advance_vertex_accumulators(gpu3d_t *, const unsigned int *, const unsigned int *);
void gpu3d_raster_advance_vertex_accumulators_scaled(gpu3d_t *, const unsigned int *, const unsigned int *);
#define MAX_VERTICES    6144
#define N_FLAGS  192
#define N_LIST     192
#define N_PROJ      1568


static const unsigned char FACE_TABLE[4] = { 0x00, 0x03, 0x03, 0x00 };

static unsigned char orient(const unsigned char *v0,
                             const unsigned char *v1,
                             const unsigned char *v2)
{
    int64_t x0 = rd32s(v0), y0 = rd32s(v0 + 4), w0 = rd32s(v0 + 12);
    int64_t x1 = rd32s(v1), y1 = rd32s(v1 + 4), w1 = rd32s(v1 + 12);
    int64_t x2 = rd32s(v2), y2 = rd32s(v2 + 4), w2 = rd32s(v2 + 12);
    int64_t  a, b, c;
    uint64_t low, height;
    int64_t  res;

    a = y1 * x0 - x1 * y0;
    b = y2 * x1 - x2 * y1;
    c = y2 * x0 - x2 * y0;

    low = (uint64_t)(uint32_t)a * (uint64_t)w2
         + (uint64_t)(uint32_t)b * (uint64_t)w0
         - (uint64_t)(uint32_t)c * (uint64_t)w1;
    height = (uint64_t)(a >> 32) * (uint64_t)w2
         + (uint64_t)(b >> 32) * (uint64_t)w0
         - (uint64_t)(c >> 32) * (uint64_t)w1;

    res = (int64_t)(height + (uint64_t)(((int64_t)low) >> 32));

    if (res < 0)                              return 1;
    if (res == 0 && (uint32_t)low == 0)      return 3;
    return 2;
}

static void compact(gpu3d_t *ctx, uint32_t from, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        ctx->vertex[i] = ctx->vertex[from + i];
        ctx->clip_code[i] = ctx->clip_code[from + i];
        ctx->vertex_color[i] = ctx->vertex_color[from + i];
        ctx->vertex_texcoord[i] = ctx->vertex_texcoord[from + i];
    }
}

static void emit_poly(gpu3d_t *ctx, uint32_t sel, uint32_t bcnt,
                  uint32_t emitted, uint32_t vbase, uint32_t n)
{
    gpu3d_vertex_bank_t *bank = &ctx->vertex_bank[sel];
    uint32_t i;

    for (i = 0; i < n; i++) {
        const unsigned char *v = (unsigned char *)&ctx->vertex[vbase + i];
        gpu3d_bank_vertex_t *bv = &bank->vertex[bcnt + i];
        uint32_t idx = emitted + i;
        int32_t  w   = rd32s(v + 12);
        uint32_t col = ctx->vertex_texcoord[vbase + i];
        uint16_t tex = ctx->vertex_color[vbase + i];
        uint32_t absw;

        absw = (w < 0) ? (uint32_t)(-(int64_t)w) : (uint32_t)w;

        ctx->screen_x[idx] = rd32(v);
        ctx->screen_y[idx] = rd32(v + 4);
        ctx->screen_w[idx] = absw;
        ctx->screen_z[idx] = rd32(v + 8);

        bv->w = w;
        bv->s = (uint16_t)col;
        bv->t = (uint16_t)(col >> 16);
        bv->color = tex;
    }
}

static uint32_t clip(uint32_t bcnt, uint32_t n)
{
    if ((uint32_t)(bcnt + n) > (uint32_t)MAX_VERTICES)
        return (uint32_t)MAX_VERTICES - bcnt;
    return n;
}

void gpu3d_poly_commit_pending_batch(gpu3d_t *ctx)
{
    {

    unsigned char frame[N_FLAGS * 3 + N_LIST * 8];
    unsigned char *v_y    = frame;
    unsigned char *v_cross = frame + N_FLAGS;
    unsigned char *v_o    = frame + N_FLAGS * 2;
    unsigned char *list  = frame + N_FLAGS * 3;

    unsigned int proj[N_PROJ * 2];
    unsigned int *recip = proj;
    unsigned int *zeros = proj + N_PROJ;

    unsigned char *cursor = list;
    uint32_t nreg  = ctx->primitive_run_count;
    uint32_t nvert = ctx->vertex_count;
    uint32_t last_count = 0;
    uint32_t nlist, emitted, sel, i;

    ctx->primitive_run[nreg].first_vertex = (unsigned char)nvert;

    for (i = 0; i < nreg; i++) {
        gpu3d_primitive_run_t *reg = &ctx->primitive_run[i];
        uint32_t sig  = ctx->primitive_run[i + 1].first_vertex;
        uint32_t type = reg->type;
        uint32_t start  = reg->first_vertex;
        uint32_t count = sig - start;
        uint32_t attr, face, alternate, lim, all, step, mask;
        uint64_t j;

        last_count = count;
        if (type > 7) continue;

        attr = reg->polygon_attr;
        face = (attr >> 6) & 3;
        alternate = 0;
        all = 1;
        lim = 0;

        if (type == 4 || type == 5) continue;

        if (type == 0) {

            uint32_t q = count / 3u;
            const unsigned char *p;
            reg->polygon_count = (unsigned char)q;
            if ((q & 0xff) == 0) continue;
            p = (unsigned char *)ctx->clip_code + start;
            j = 0;
            do {
                uint32_t a = p[0], b = p[1], c = p[2];
                uint32_t y, o;
                p += 3;
                y = (b & a) & c;
                o = (b | a) | c;
                v_o[j] = (unsigned char)o;
                v_y[j] = (unsigned char)y;
                lim = reg->polygon_count;
                all &= (y != 0) ? 1u : 0u;
                j++;
            } while (j < lim);
            if (all) continue;
            if (face != 3 && lim != 0) {
                uint32_t k, start2 = reg->first_vertex;
                const unsigned char *b0 = (unsigned char *)&ctx->vertex[start2];
                for (k = 0; k < lim; k++)
                    v_cross[k] = orient(b0 + 48u * k, b0 + 48u * k + 16,
                                        b0 + 48u * k + 32);
            }
            step = 3; mask = 0;
        } else if (type == 1) {

            uint32_t q = count >> 2;
            const unsigned char *p;
            reg->polygon_count = (unsigned char)q;
            if ((q & 0xff) == 0) continue;
            p = (unsigned char *)ctx->clip_code + start;
            j = 0;
            do {
                uint32_t a = p[0], b = p[1], c = p[2], d = p[3];
                uint32_t y, o;
                p += 4;
                y = ((b & a) & c) & d;
                o = ((b | a) | c) | d;
                v_o[j] = (unsigned char)o;
                v_y[j] = (unsigned char)y;
                lim = reg->polygon_count;
                all &= (y != 0) ? 1u : 0u;
                j++;
            } while (j < lim);
            if (all) continue;
            if (face != 3 && lim != 0) {
                uint32_t k, start2 = reg->first_vertex;
                const unsigned char *b0 = (unsigned char *)&ctx->vertex[start2];
                for (k = 0; k < lim; k++)
                    v_cross[k] = orient(b0 + 64u * k, b0 + 64u * k + 16,
                                        b0 + 64u * k + 32);
            }
            step = 4; mask = 0;
        } else if (type == 2 || type == 6) {

            uint32_t flip, c0, c1;
            int32_t  n2;
            alternate = FACE_TABLE[face];
            flip  = reg->flipped ? alternate : 0u;
            n2 = (int32_t)(count - 2u);
            if (n2 < 1) continue;
            reg->polygon_count = (unsigned char)n2;
            if (((uint32_t)n2 & 0xff) == 0) continue;
            face ^= flip;
            c0 = ctx->clip_code[start];
            c1 = ctx->clip_code[start + 1];
            j = 0;
            do {
                uint32_t a = c1;
                uint32_t c = ctx->clip_code[2 + start + j];
                uint32_t y2 = c0 & a;
                uint32_t o  = c0 | a;
                uint32_t y;
                c0 = a;
                y  = y2 & c;
                o  = c | o;
                v_o[j] = (unsigned char)o;
                v_y[j] = (unsigned char)y;
                lim = reg->polygon_count;
                all &= (y != 0) ? 1u : 0u;
                j++;
                c1 = c;
            } while (j < lim);
            if (all) continue;
            if (face != 3 && lim != 0) {
                uint32_t k, start2 = reg->first_vertex;
                const unsigned char *b0 = (unsigned char *)&ctx->vertex[start2];
                for (k = 0; k < lim; k++)
                    v_cross[k] = orient(b0 + 16u * k, b0 + 16u * k + 16,
                                        b0 + 16u * k + 32);
            }
            step = 1; mask = 1;
        } else {

            uint32_t q = (count >> 1) - 1u;
            uint32_t c0, c1;
            const unsigned char *p;
            reg->flipped = 1;
            if ((int32_t)q < 1) continue;
            reg->polygon_count = (unsigned char)q;
            if ((q & 0xff) == 0) continue;
            c0 = ctx->clip_code[start];
            c1 = ctx->clip_code[start + 1];
            p  = &ctx->clip_code[3] + start;
            j = 0;
            do {
                uint32_t d = p[0];
                uint32_t c = *(p - 1);
                uint32_t y2 = (c0 & c1) & d;
                uint32_t o  = ((c0 | c1) | d) | c;
                uint32_t y  = y2 & c;
                v_o[j] = (unsigned char)o;
                v_y[j] = (unsigned char)y;
                all &= (y != 0) ? 1u : 0u;
                lim = reg->polygon_count;
                j++;
                p += 2;
                c1 = d;
                c0 = c;
            } while (j < lim);
            if (all) continue;
            if (face != 3 && lim != 0) {
                uint32_t k, start2 = reg->first_vertex;
                const unsigned char *b0 = (unsigned char *)&ctx->vertex[start2];

                for (k = 0; k < lim; k++)
                    v_cross[k] = orient(b0 + 32u * k, b0 + 32u * k + 16,
                                        b0 + 32u * k + 48);
            }
            step = 2; mask = 0;
        }

        if (face != 3) {
            uint32_t lim2 = reg->polygon_count;
            if (lim2 != 0) {
                uint64_t k = 0;
                do {
                    uint32_t cr = v_cross[k];
                    uint32_t vy = v_y[k];
                    v_y[k] = (unsigned char)(vy | (((face & cr) == 0) ? 1u : 0u));
                    lim2 = reg->polygon_count;
                    k++;
                    face ^= alternate;
                } while (k < lim2);
            }
        }

        cursor = gpu3d_geometry_poly_split_runs(ctx, cursor, (const unsigned char *)reg, step, v_y, v_o, mask);
    }

    nlist = (uint32_t)(((size_t)(cursor - list)) >> 3);
    ctx->texture_run_index = 0;
    ctx->texture_run[ctx->texture_run_count].first_vertex = 0xff;
    ctx->emitted_count = 0;

    for (i = 0; i < nlist; i++) {
        const unsigned char *ent = list + 8u * i;
        uint32_t type = ent[4];
        uint32_t start, cnt, bcnt, emit, n, vbase;
        uint32_t *bc;

        if (type > 7) continue;
        cnt = ent[6];
        start = ent[5];

        if (type == 4 || type == 5) {

            gpu3d_geometry_clip_polygon(ctx, ent, type == 4 ? 3u : 4u);
            continue;
        }

        switch (type) {
        case 0: gpu3d_geometry_emit_draw_entries(ctx, ent, 3, 3, 0, 0); break;
        case 1: gpu3d_geometry_emit_draw_entries(ctx, ent, 4, 4, 0, 0); break;
        case 2: gpu3d_geometry_emit_draw_entries(ctx, ent, 1, 3, 0, 1); break;
        case 3: gpu3d_geometry_emit_draw_entries(ctx, ent, 2, 4, 0, 0); break;
        case 6: gpu3d_geometry_emit_draw_entries(ctx, ent, 1, 3, 2, 1); break;
        default:gpu3d_geometry_emit_draw_entries(ctx, ent, 2, 4, 2, 0); break;
        }

        sel  = ctx->bank;
        emit = ctx->emitted_count;
        bc   = &ctx->vertex_bank[sel].count;
        bcnt = *bc;

        switch (type) {
        case 0: n = cnt + cnt * 2u; vbase = start;     break;
        case 1: n = cnt << 2;       vbase = start;     break;
        case 2: n = cnt + 2u;       vbase = start;     break;
        case 3: n = (cnt << 1) + 2u;vbase = start;     break;
        case 6: n = cnt;            vbase = start + 2; break;
        default:n = cnt << 1;       vbase = start + 2; break;
        }
        n = clip(bcnt, n);

        if (n != 0)
            emit_poly(ctx, sel, bcnt, emit, vbase, n);

        ctx->emitted_count = ctx->emitted_count + n;
        *bc = *bc + n;
    }

    emitted = (nlist != 0) ? ctx->emitted_count : 0u;
    sel = ctx->bank;
    {
        gpu3d_vertex_bank_t *bank = &ctx->vertex_bank[sel];
        uint32_t bcnt = bank->count;
        unsigned char *paux;
        uint32_t sel2;

        gpu3d_raster_compute_reciprocal_block(recip, zeros,
                           ctx->screen_w, emitted);

        paux = (unsigned char *)ctx->machine;
        sel2 = ((nds_t *)paux)->config.hires_3d;
        if (sel2 != 0)
            gpu3d_raster_advance_vertex_accumulators_scaled(ctx, recip, zeros);
        else
            gpu3d_raster_advance_vertex_accumulators(ctx, recip, zeros);

        if (emitted != 0) {
            uint32_t k;
            uint32_t base = bcnt - emitted;
            for (k = 0; k < emitted; k++) {
                gpu3d_bank_vertex_t *bv = &bank->vertex[base + k];
                bv->x = (uint16_t)ctx->screen_x[k];
                bv->y = (uint16_t)ctx->screen_y[k];
                bv->z = (uint16_t)ctx->screen_z[k];
                bv->w = (int32_t)ctx->screen_w[k];
            }
        }
    }

    if (nreg != 0) {
        gpu3d_primitive_run_t *last  = &ctx->primitive_run[nreg - 1u];
        gpu3d_primitive_run_t *reg0 = &ctx->primitive_run[0];
        uint32_t type = last->type;

        if (type <= 7 && type != 4 && type != 5) {
            uint32_t nv = ctx->vertex_count;
            uint32_t attr = last->polygon_attr;

            if (type == 0) {

                uint32_t rest = last_count % 3u;
                uint32_t from = nv - rest;
                reg0->polygon_attr = attr;
                reg0->type = (uint8_t)(0);
                reg0->first_vertex = 0;
                reg0->flipped = 0;
                if ((int32_t)(nv - rest) >= 0) {
                    if (rest != 0) compact(ctx, from, rest);
                    ctx->vertex_count =  rest;
                    ctx->vertex_total = rest;
                    ctx->primitive_run_count =   1;
                }
            } else if (type == 1) {

                uint32_t rest = last_count & 3u;
                uint32_t from = nv - rest;
                reg0->polygon_attr = attr;
                reg0->type = (uint8_t)(1);
                reg0->first_vertex = 0;
                reg0->flipped = 0;
                if ((int32_t)(nv - rest) >= 0) {
                    if (rest != 0) compact(ctx, from, rest);
                    ctx->vertex_count =  rest;
                    ctx->vertex_total = rest;
                    ctx->primitive_run_count =   1;
                }
            } else if (type == 2 || type == 6) {
                if (type == 2 && last_count <= 2u) {

                    uint32_t from = nv - last_count;
                    reg0->polygon_attr = attr;
                    reg0->type = (uint8_t)(2);
                reg0->first_vertex = 0;
                    reg0->flipped = last->flipped;
                    if ((int32_t)(nv - last_count) >= 0) {
                        if (last_count != 0) compact(ctx, from, last_count);
                        ctx->vertex_count =  last_count;
                        ctx->vertex_total = last_count;
                        ctx->primitive_run_count =   1;
                    }
                } else {

                    uint32_t broken = ctx->run_split_flag;
                    uint32_t from = nv - 2u;
                    reg0->polygon_attr = attr;
                    reg0->type = (uint8_t)(broken ? 2u : 6u);
                reg0->first_vertex = 0;
                    reg0->flipped = (unsigned char)((last_count & 1u) ^ last->flipped);
                    if ((int32_t)from >= 0) {
                        compact(ctx, from, 2);
                        ctx->vertex_count =  2;
                        ctx->primitive_run_count =   1;
                        ctx->vertex_total = 2;
                    }
                }
            } else {

                if (type == 3 && last_count <= 3u) {

                    uint32_t from = nv - last_count;
                    reg0->polygon_attr = attr;
                    reg0->type = (uint8_t)(3);
                reg0->first_vertex = 0;
                    reg0->flipped = 1;
                    if ((int32_t)(nv - last_count) >= 0) {
                        if (last_count != 0) compact(ctx, from, last_count);
                        ctx->vertex_count =  last_count;
                        ctx->vertex_total = last_count;
                        ctx->primitive_run_count =   1;
                    }
                } else {

                    uint32_t broken = ctx->run_split_flag;
                    uint32_t n = 2u | (last_count & 1u);
                    uint32_t from = nv - n;
                    reg0->polygon_attr = attr;
                    reg0->type = (uint8_t)(broken ? 3u : 7u);
                reg0->first_vertex = 0;
                    reg0->flipped = 1;
                    if ((int32_t)from >= 0) {
                        compact(ctx, from, n);
                        ctx->vertex_count =  n;
                        ctx->vertex_total = n;
                        ctx->primitive_run_count =   1;
                    }
                }
            }
        }
    }

    {
        uint32_t n2 = ctx->texture_run_count;
        if (n2 != 0) {
            const unsigned char *p = (unsigned char *)&ctx->texture_run[0].texture_param + 8u * (n2 - 1u);
            ctx->run_texture_param = rd32(p);
            ctx->run_palette_base = (uint32_t)rd16(p + 4);
        }

        ctx->texture_run_mark =   0xffu;
        ctx->texture_run_count = 0u;
    }
    }
}
#undef BANK_STRIDE
#undef MAX_VERTICES
#undef N_FLAGS
#undef N_LIST
#undef N_PROJ

#include <stdint.h>
#include "hires_runtime.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>
#include <time.h>
#include "blob_symbols.h"
#include "gpu3d.h"
#include "../gpu.h"
#include "cpu/arm.h"

void arm_bank_select(unsigned char *machine, uint32_t updated) {
    arm_t *cpu = (arm_t *)machine;
    uint32_t old = cpu->bank;
    if (old == updated)
        return;

    int store_block = 0;

    if (updated == ARM_BANK_FIQ) {

        ((uint64_t *)cpu->fiq_r8_r14)[0] = ((const uint64_t *)&cpu->r[8])[0];
        ((uint64_t *)cpu->fiq_r8_r14)[1] = ((const uint64_t *)&cpu->r[8])[1];
        for (int i = 0; i < 16; i++)
            ((unsigned char *)&cpu->fiq_r8_r14[3])[i] = ((const unsigned char *)&cpu->r[11])[i];
        if (old == ARM_BANK_FIQ)
            store_block = 1;
    } else {
        cpu->banked_sp_lr[old][0] = cpu->r[13];
        cpu->banked_sp_lr[old][1] = cpu->r[14];
        if (old == ARM_BANK_FIQ)
            store_block = 1;
    }

    if (store_block) {

        const unsigned char *o = (const unsigned char *)cpu->fiq_r8_r14;
        ((uint64_t *)&cpu->r[8])[0] = ((const uint64_t *)o)[0];
        ((uint64_t *)&cpu->r[8])[1] = ((const uint64_t *)o)[1];
        cpu->r[14] = cpu->fiq_r8_r14[6];
        *(uint64_t *)&cpu->r[12] = *(const uint64_t *)(o + 16);
    } else {
        cpu->r[13] = cpu->banked_sp_lr[updated][0];
        cpu->r[14] = cpu->banked_sp_lr[updated][1];
    }

    cpu->bank = updated;
}

static uint8_t color5_to_6_r(uint16_t c)
{
    uint32_t x = ((uint32_t)c << 1) & 0x3eu;
    return x == 0 ? 0 : (uint8_t)(x | 1u);
}

static uint8_t color5_to_6_g(uint16_t c)
{
    uint32_t x = ((uint32_t)c >> 4) & 0x3eu;
    return x == 0 ? 0 : (uint8_t)(x | 1u);
}

static uint8_t color5_to_6_b(uint16_t c)
{
    uint32_t x = ((uint32_t)c >> 9) & 0x3eu;
    return x == 0 ? 0 : (uint8_t)(x | 1u);
}

void gpu3d_init_color_product_tables(gpu3d_t *ctx)
{
    uint32_t a, b, d[4], ah, bh;
    uint32_t av[3], bv[3], dv[4][3];
    uint32_t lane0, lane1, lane2, factor0, factor1, factor2;
    uint32_t bus0, bus1, i;

    ctx->param_cursor = (unsigned char *)ctx->param_ring;
    ctx->param_pending_cursor = (unsigned char *)ctx->param_ring +
        (size_t)ctx->params_remaining * 4u;
    ctx->normal_cursor = (unsigned char *)ctx->batch_normal;
    ctx->color_cursor = (unsigned char *)ctx->batch_color;
    ctx->attribute_mark_cursor = (unsigned char *)ctx->batch_attribute_mark;
    ctx->attribute_mark = 0xff;
    ctx->run_texture_param = ctx->texture_param;
    ctx->batch_count = 0;
    ctx->texture_run_mark = 0xff;
    ctx->texture_run_count = 0;
    ctx->texture_run_index = 0;
    ctx->primitive_run_count = 1;
    ctx->run_palette_base = ctx->texture_palette_base;
    ctx->primitive_run[0].first_vertex = 0;
    ctx->vertex_total = ctx->vertex_count;

    a = ctx->specular_emission_raw;
    b = ctx->diffuse_ambient_raw;
    ctx->specular_color = a & 0x7fff;
    ctx->emission_color = (a >> 16) & 0x7fff;
    ctx->shininess_enabled = (uint8_t)((a >> 15) & 1u);
    ctx->diffuse_color = b & 0x7fff;
    ctx->ambient_color = (b >> 16) & 0x7fff;
    for (i = 0; i != 4; ++i)
        d[i] = ctx->light_color[i];
    for (i = 0; i != 3; ++i) {
        av[i] = (a >> ((i * 5u) & 31u)) & 0x1fu;
        bv[i] = (b >> ((i * 5u) & 31u)) & 0x1fu;
    }
    for (i = 0; i != 4; ++i) {
        dv[i][0] = d[i] & 0x1fu;
        dv[i][1] = (d[i] >> 5) & 0x1fu;
        dv[i][2] = (d[i] >> 10) & 0x1fu;
    }
    bus0 = ctx->opaque[ctx->bank].count;
    bus1 = ctx->translucent[ctx->bank].count;

    ctx->light_diffuse[3][0] = dv[3][0] * bv[0];
    ctx->light_diffuse[0][0] = dv[0][0] * bv[0];
    ctx->light_diffuse[0][1] = dv[0][1] * bv[1];
    ctx->light_diffuse[0][2] = dv[0][2] * bv[2];
    ctx->light_specular[0][0] = dv[0][0] * av[0];
    ctx->light_specular[0][1] = dv[0][1] * av[1];
    ctx->light_specular[0][2] = dv[0][2] * av[2];
    ctx->light_diffuse[1][0] = dv[1][0] * bv[0];
    ctx->light_diffuse[1][1] = dv[1][1] * bv[1];
    ctx->light_diffuse[1][2] = dv[1][2] * bv[2];
    ctx->light_specular[1][0] = dv[1][0] * av[0];
    ctx->light_specular[1][1] = dv[1][1] * av[1];
    ctx->light_specular[1][2] = dv[1][2] * av[2];
    ctx->light_diffuse[2][0] = dv[2][0] * bv[0];
    ctx->light_diffuse[2][1] = dv[2][1] * bv[1];
    ctx->light_diffuse[2][2] = dv[2][2] * bv[2];
    ctx->light_specular[2][0] = dv[2][0] * av[0];
    ctx->light_specular[2][1] = dv[2][1] * av[1];
    ctx->light_specular[2][2] = dv[2][2] * av[2];
    ctx->light_diffuse[3][1] = dv[3][1] * bv[1];
    ctx->light_diffuse[3][2] = dv[3][2] * bv[2];
    ctx->light_specular[3][0] = dv[3][0] * av[0];
    ctx->light_specular[3][1] = dv[3][1] * av[1];
    ctx->light_specular[3][2] = dv[3][2] * av[2];
    ctx->polygon_count = bus0 + bus1;

    ah = a >> 16;
    bh = b >> 16;

    lane0 = (ah << 14) & 0x7c000u;
    lane1 = (ah << 9) & 0x7c000u;
    lane2 = (a >> 12) & 0x7c000u;

    factor0 = (bh << 9) & 0x3e00u;
    factor1 = (bh << 4) & 0x3e00u;
    factor2 = (b >> 17) & 0x3e00u;
    {
        uint8_t mask = ctx->light_mask;
        for (i = 0; i != 8; ++i) {
            if ((mask & (uint8_t)(1u << i)) != 0) {
                uint32_t word = ctx->light_color[i];
                lane0 += (word & 0x1fu) * factor0;
                lane1 += ((word >> 5) & 0x1fu) * factor1;
                lane2 += ((word >> 10) & 0x1fu) * factor2;
            }
        }
    }
    ctx->ambient_accum[2] = lane2;
    ctx->ambient_accum[0] = lane0;
    ctx->ambient_accum[1] = lane1;

    for (i = 0; i != 32; ++i) {
        uint16_t color;
        ctx->fog_table[i] = ctx->fog_table[i] & 0x7f;
        color = ctx->toon_table[i];
        ctx->toon_table_expanded[0][i] = color5_to_6_r(color);
        ctx->toon_table_expanded[1][i] = color5_to_6_g(color);
        ctx->toon_table_expanded[2][i] = color5_to_6_b(color);
    }
    for (i = 0; i != 8; ++i) {
        uint16_t color = ctx->edge_color[i];
        ctx->edge_color_expanded[0][i] = color5_to_6_r(color);
        ctx->edge_color_expanded[1][i] = color5_to_6_g(color);
        ctx->edge_color_expanded[2][i] = color5_to_6_b(color);
    }

    bus0 = ctx->opaque[ctx->bank].count;
    bus1 = ctx->translucent[ctx->bank].count;
    ctx->polygon_count = bus0 + bus1;
    ctx->render_dirty = 1;
    ctx->clip_matrix_dirty = 1;
}

unsigned recon_threads_created = 4u;
static unsigned band_ticket;

static unsigned char band_state[32];
__thread unsigned recon_band_current = 0xffffu;

void recon_band_reset(void) { __atomic_store_n(&band_ticket, 0u, __ATOMIC_RELEASE); memset(band_state, 0, sizeof band_state); }

void recon_band_publish(unsigned band, unsigned dirty)
{ if (band < 32u) __atomic_fetch_or(&band_state[band], (unsigned char)(1u | (dirty ? 2u : 0u)), __ATOMIC_RELEASE); }

void recon_band_publish_dirty(void)
{ unsigned b = recon_band_current; if (b < 32u && !(__atomic_load_n(&band_state[b], __ATOMIC_RELAXED) & 2u)) __atomic_fetch_or(&band_state[b], (unsigned char)2u, __ATOMIC_RELEASE); }

unsigned recon_band_sibling_dirty(unsigned band)
{
    unsigned h = band ^ 1u;
    if (h >= 32u) return 0u;
    unsigned char e;
    unsigned long turns = 0;
    while (!((e = __atomic_load_n(&band_state[h], __ATOMIC_ACQUIRE)) & 3u)) {
        if (++turns > 200000000ul) return 0u;
        if ((turns & 63u) == 0u) sched_yield();
    }
    return (e >> 1) & 1u;
}

unsigned recon_band_take(void)
{ return __atomic_fetch_add(&band_ticket, 1u, __ATOMIC_ACQ_REL); }

unsigned recon_band_steal(unsigned h)
{ unsigned expected = h; unsigned r = __atomic_compare_exchange_n(&band_ticket, &expected, h + 1u, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED) ? 1u : 0u; return r; }

unsigned recon_threads_3d(unsigned requested)
{
    if (requested < 1u) return 1u;
    if (requested > recon_threads_created) requested = recon_threads_created;
    if (recon_scale_3d <= 2u && requested > 4u) requested = 4u;

    if (!recon_dynamic_share())
        while (requested > 1u && (RECON_BANDS % requested) != 0u) requested--;
    return requested;
}
__attribute__((weak)) unsigned long nds_output_reads;
__attribute__((weak)) unsigned long nds_output_frames;
unsigned recon_bands_n;
unsigned recon_bands_map;
unsigned recon_which_3d;
unsigned recon_height_hits, recon_height_misses, recon_height_evictions;
unsigned recon_scale_3d = 2u;

unsigned recon_bands_3d = 12u;
unsigned recon_band_rows = 32u;



gpu3d_band_bucket_t *recon_buckets(unsigned char *arena, unsigned side)
{
    if (recon_bands_3d == 12u) return ((gpu_t *)arena)->bucket[side ? 1 : 0];
    static gpu3d_band_bucket_t buckets[2][32] __attribute__((aligned(64)));
    return buckets[side ? 1 : 0];
}
unsigned recon_native;
unsigned recon_scale_pages = 2u;
__thread unsigned recon_sublines_hires;



int recon_native_loaded, recon_scale_3d_loaded;


unsigned recon_shadow_stride(void)
{
    unsigned e = recon_scale_output();
    return (e > 2u) ? (e * e) : 3u;
}

unsigned recon_s0_shadow(void)
{
    return (recon_scale_output() > 2u) ? 0u : 1u;
}

unsigned recon_scale_output(void)
{
    recon_native_read();
    if (!recon_native) return 2u;
    recon_scale_3d_read();
    unsigned e = recon_scale_out;
    if (recon_scale_pages && recon_scale_pages < e) {
        static int logged;
        if (!logged) { logged = 1;
            typedef int (*fn_log)(int, const char *, const char *, ...);
            void *l = sym_libc___android_log_print;
            if (l) ((fn_log)l)(6, "reconDS",
                "SCALE MISMATCH: pages were laid out at x%u and x%u was requested; "
                "delivering x%u to stay in bounds",
                recon_scale_pages, e, recon_scale_pages); }
        e = recon_scale_pages;
    }
    return e ? e : 2u;
}

static unsigned char *buf3d_base;
static unsigned      buf3d_scale;

static unsigned long buf3d_size(unsigned e) { return 0x30000ul * e * e; }

static void buf3d_ensure(void)
{
    recon_scale_3d_read();
    if (recon_scale_3d <= 2u) return;
    if (buf3d_base) return;
    recon_scale_ceiling_read();
    unsigned long t = buf3d_size(recon_scale_ceiling);

    recon_native_read();
    void *p = mmap(0, 2ul * t, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return;
    buf3d_base = (unsigned char *)p;
    buf3d_scale = recon_scale_ceiling;
}
unsigned char *recon_arena_3d;

const unsigned char *recon_cfg3d(const unsigned char *buffer)
{
    (void)buffer;
    recon_scale_3d_read();
    return (const unsigned char *)&((gpu_t *)recon_arena_3d)->raster;
}

unsigned char *recon_buf3d_front(unsigned char *arena)
{
    recon_arena_3d = arena;
    buf3d_ensure();
    if (!buf3d_base || recon_scale_3d <= 2u) return ((gpu_t *)arena)->frame[0];
    return buf3d_base;
}

unsigned char *recon_buf3d_back(unsigned char *arena)
{
    buf3d_ensure();
    if (!buf3d_base || recon_scale_3d <= 2u) return ((gpu_t *)arena)->frame[1];
    return buf3d_base + buf3d_size(buf3d_scale);
}

unsigned long recon_buf3d_bytes(void) { return buf3d_size(recon_scale_3d); }
static unsigned char *bucket_lst_by_scale[9];
static int16_t       *bucket_cnt_by_scale[9];
static unsigned char *bucket_lst;
static int16_t       *bucket_cnt;
static unsigned       bucket_scale;

static void bucket_ensure(void)
{
    recon_scale_3d_read();
    if (recon_scale_3d <= 2u) return;
    if (bucket_lst && bucket_scale == recon_scale_3d) return;
    if (bucket_lst_by_scale[recon_scale_3d]) {
        bucket_lst = bucket_lst_by_scale[recon_scale_3d];
        bucket_cnt = bucket_cnt_by_scale[recon_scale_3d];
        bucket_scale = recon_scale_3d;
        return;
    }
    unsigned height = 192u * recon_scale_3d;
    unsigned long tl = (unsigned long)(height + 1u) * 4096ul;
    unsigned long tc = (unsigned long)(height + 16u) * 2ul;
    void *l = mmap(0, tl, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void *c = mmap(0, tc, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (l == MAP_FAILED || c == MAP_FAILED) return;
    bucket_lst_by_scale[recon_scale_3d] = (unsigned char *)l;
    bucket_cnt_by_scale[recon_scale_3d] = (int16_t *)c;
    bucket_lst = (unsigned char *)l;
    bucket_cnt = (int16_t *)c;
    bucket_scale = recon_scale_3d;
}
static uint32_t *steps_prop[2][9];

const uint32_t *recon_steps_table(const uint32_t *fallback, uint32_t base)
{
    recon_scale_3d_read();
    unsigned e = recon_scale_3d;
    if (e <= 2u || e > 8u) return fallback;
    int i = (base == 0x40000000u) ? 0 : 1;
    if (steps_prop[i][e]) return steps_prop[i][e];

    unsigned n_max = 16u * 192u * e;
    unsigned long t = (unsigned long)(n_max + 2u) * sizeof(uint32_t);
    void *p = mmap(0, t, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return fallback;
    uint32_t *tb = (uint32_t *)p;
    tb[0] = fallback ? fallback[0] : 0u;
    for (unsigned k = 1u; k <= n_max + 1u; k++)
        tb[k] = (uint32_t)(((unsigned long)base + k - 1ul) / k);
    steps_prop[i][e] = tb;
    return tb;
}

unsigned char *recon_buckets_lst(unsigned char *fallback)
{
    bucket_ensure();
    if (!bucket_lst || recon_scale_3d <= 2u) return fallback;
    return bucket_lst;
}

int16_t *recon_buckets_cnt(int16_t *fallback)
{
    bucket_ensure();
    if (!bucket_cnt || recon_scale_3d <= 2u) return fallback;
    return bucket_cnt;
}
#define OBJ_MAX 2048u
#define OBJ_SLOTS 8u
static unsigned char *obj_base[OBJ_SLOTS];
static uint16_t       obj_value[OBJ_SLOTS][OBJ_MAX];

static int obj_slot(unsigned char *base)
{
    for (unsigned r = 0; r < OBJ_SLOTS; r++)
        if (obj_base[r] == base) return (int)r;
    for (unsigned r = 0; r < OBJ_SLOTS; r++)
        if (!obj_base[r]) { obj_base[r] = base; return (int)r; }
    for (unsigned r = 1; r < OBJ_SLOTS; r++) obj_base[r] = 0;
    obj_base[0] = base;
    return 0;
}

void recon_height_reset(void)
{
    for (unsigned r = 0; r < OBJ_SLOTS; r++) obj_base[r] = 0;
}

void recon_height_set(unsigned char *objs, unsigned idx, unsigned value)
{
    if (recon_scale_3d <= 2u || idx >= OBJ_MAX) return;
    int r = obj_slot(objs);

    obj_value[r][idx] = (uint16_t)value;
}

unsigned recon_height_get(const unsigned char *o, unsigned fallback)
{
    if (recon_scale_3d <= 2u) return fallback;

    int best = -1; unsigned long best_d = 0;
    for (unsigned r = 0; r < OBJ_SLOTS; r++) {
        unsigned char *b = obj_base[r];
        if (!b || o < b) continue;
        unsigned long d = (unsigned long)(o - b);
        if (d >= OBJ_MAX * 32ul || (d & 31ul)) continue;
        if (best < 0 || d < best_d) { best = (int)r; best_d = d; }
    }
    if (best >= 0) {
        unsigned v = obj_value[best][best_d / 32ul];
        if (v) recon_height_hits++; else recon_height_misses++;
        return v ? v : fallback;
    }
    recon_height_evictions++;
    return fallback;
}
unsigned recon_pass;
unsigned long recon_poly_band;
static unsigned char *ctx3d_base_prop;
static unsigned       ctx3d_scale;

unsigned char *recon_ctx3d_base(unsigned char *arena)
{
    recon_scale_3d_read();
    recon_scale_ceiling_read();

    if (recon_scale_ceiling <= 2u) return ((gpu_t *)arena)->band_context[0];
    if (ctx3d_base_prop) return ctx3d_base_prop;
    unsigned long b = recon_block_stride;

    void *p = mmap(0, (unsigned long)(RECON_HIRES_THREADS + 1u) * b + GPU3D_BAND_MMAP_GUARD_BYTES,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return ((gpu_t *)arena)->band_context[0];
    ctx3d_base_prop = (unsigned char *)p;
    ctx3d_scale = recon_scale_ceiling;

    {
        unsigned long b = recon_block_stride;

        for (unsigned k = 0; k < RECON_HIRES_THREADS + 1u; k++) {
            gpu3d_band_header_t *h = GPU3D_BAND_HEADER(ctx3d_base_prop + (unsigned long)k * b);
            h->gpu = (gpu_t *)arena;
            h->gpu3d = GPU3D_OF(arena);
        }

        ((gpu_t *)arena)->raster.frame_front = recon_buf3d_front(arena);
    }
    return ctx3d_base_prop;
}
static unsigned char *tab_base_by_scale[9];

unsigned char *recon_tables_base(unsigned char *arena)
{
    recon_scale_3d_read();
    unsigned e = recon_scale_3d;
    if (e <= 2u || e > 8u) return ((gpu_t *)arena)->frame[0];
    if (tab_base_by_scale[e]) return tab_base_by_scale[e];

    unsigned long t = 0x300000ul * e;
    void *p = mmap(0, t, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return ((gpu_t *)arena)->frame[0];
    tab_base_by_scale[e] = (unsigned char *)p;
    return tab_base_by_scale[e];
}

int recon_shortcut_all(void)
{
    return 0;
}
#undef OBJ_MAX
#undef OBJ_SLOTS

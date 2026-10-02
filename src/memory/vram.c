#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "memory/vram.h"
#include "core/nds_state.h"
#include <string.h>
#include <stddef.h>
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"

static const uint32_t vram_bank_size_kb[VRAM_BANKS] = {
    128u, 128u, 128u, 128u, 64u, 16u, 16u, 32u, 16u
};
static const uint32_t vram_bank_first_slot[VRAM_BANKS] = {
    0x200u, 0x208u, 0x210u, 0x218u, 0x220u, 0x224u, 0x225u, 0x226u, 0x228u
};

static void *const vram_cache_write_handlers[3] = {
    (void *)vram_reg_cache_write8,
    (void *)vram_reg_cache_write16,
    (void *)vram_reg_cache_write32,
};




#define OFF_PALETTE_CACHE    0x16870
#define OFF_POINTERS 0xfba68
typedef void (*fn_writer)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void vram_reg_cache_write8(uint8_t *state, uint32_t dir, uint32_t value) {

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + OFF_PALETTE_CACHE + idx;

    if (*cache == (uint8_t)value) return;

    uint8_t original = (uint8_t)value;

    void **pointers = (void **)(state + OFF_POINTERS);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra = *(uint16_t *)(first + 20);

    void *dest = (dir & 0x400) ? (void *)&((gpu_t *)second)->engine[1]
                                  : (void *)&((gpu_t *)second)->engine[0];

    fn_writer write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | 0x100000u, value & 0xffu, 1u, extra);

    *cache = original;
}
#undef OFF_PALETTE_CACHE
#undef OFF_POINTERS

#define OFF_MARK     0xfbe78
#define OFF_PALETTE    0x16070
#define OFF_PALETTE_CACHE     0x16870
#define PALETTE_RAM_SIZE     0x800
#define OFF_AUX       0xfbe60
#define OFF_TEMPLATE 0x133a48
#define OFF_POINTERS  0xfba68

void vram_reg_cache_write8_init(uint8_t *state, unsigned dir, unsigned value) {

    void *mark;
    memcpy(&mark, state + OFF_MARK, 8);

    if (mark != (void *)vram_reg_cache_write8) {
        uint8_t *table = state + OFF_PALETTE_CACHE;
        memcpy(table, state + OFF_PALETTE, PALETTE_RAM_SIZE);

        const uint8_t *template = (const uint8_t *)vram_cache_write_handlers;
        uint8_t *aux = state + OFF_AUX;

        aux[80] = 0;
        memcpy(aux, &table, 8);
        memcpy(state + OFF_MARK + 16, template + 16, 8);
        memcpy(state + OFF_MARK, template, 16);

    }

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + OFF_PALETTE_CACHE + idx;

    uint8_t prev = *cache;
    if (prev == (uint8_t)value) return;

    uint8_t original = (uint8_t)value;

    void **pointers = (void **)(state + OFF_POINTERS);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    memcpy(&extra, first + 20, 2);

    uint8_t *dest = ((dir & 0x400) == 0) ? (uint8_t *)&((gpu_t *)second)->engine[0]
                                            : (uint8_t *)&((gpu_t *)second)->engine[1];

    gpu2d_deferred_capture_queue_push((gpu2d_engine_t *)((gpu2d_engine_t *)(dest)), (uint32_t)idx | 0x100000u,
                       (uint32_t)(value & 0xffu), 1u, (uint32_t)extra);

    *cache = original;
}
#undef OFF_MARK
#undef OFF_PALETTE
#undef OFF_PALETTE_CACHE
#undef PALETTE_RAM_SIZE
#undef OFF_AUX
#undef OFF_TEMPLATE
#undef OFF_POINTERS

#define OFF_MARK     0xfbe78UL
#define OFF_FN_B      0xfbe80UL
#define OFF_FN_C      0xfbe88UL
#define OFF_BUFPTR    0xfbe60UL
#define OFF_FLAGB     0xfbeb0UL
#define OFF_PALETTE_CACHE       0x16870UL
#define OFF_PALETTE       0x16070UL
#define PALETTE_RAM_SIZE     0x800UL

uint8_t vram_reg_cache_read8_init(uint8_t *param_1, uint64_t param_2)
{

    static void *(*s_memcpy)(void *, const void *, size_t);
    if (!s_memcpy)
        s_memcpy = (void *(*)(void *, const void *, size_t))sym_libc_memcpy;

    void *mark_list = (void *)vram_reg_cache_write8;

    if (rd_ptr(param_1 + OFF_MARK) != mark_list) {

        const uint8_t *table = (const uint8_t *)vram_cache_write_handlers;
        uint64_t v_mark = rd64(table + 0);
        uint64_t v_fn_b  = rd64(table + 8);
        uint64_t v_fn_c  = rd64(table + 16);

        s_memcpy(param_1 + OFF_PALETTE_CACHE, param_1 + OFF_PALETTE, PALETTE_RAM_SIZE);

        wr8(param_1 + OFF_FLAGB, 0);
        wr_ptr(param_1 + OFF_BUFPTR, param_1 + OFF_PALETTE_CACHE);

        wr64(param_1 + OFF_FN_C, v_fn_c);
        wr64(param_1 + OFF_FN_B, v_fn_b);
        wr64(param_1 + OFF_MARK, v_mark);
    }

    return rd8(param_1 + OFF_PALETTE_CACHE + (param_2 & 0x7ffUL));
}
#undef OFF_MARK
#undef OFF_FN_B
#undef OFF_FN_C
#undef OFF_BUFPTR
#undef OFF_FLAGB
#undef OFF_PALETTE_CACHE
#undef OFF_PALETTE
#undef PALETTE_RAM_SIZE

#define OFF_PALETTE_CACHE    0x16870
#define OFF_POINTERS 0xfba68
typedef void (*fn_writer_5)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void vram_reg_cache_write16(uint8_t *state, unsigned dir, unsigned value) {

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + OFF_PALETTE_CACHE + idx;

    uint16_t prev;
    __builtin_memcpy(&prev, cache, 2);
    if (prev == (uint16_t)value) return;

    void **pointers = (void **)(state + OFF_POINTERS);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    __builtin_memcpy(&extra, first + 20, 2);

    void *dest = (dir & 0x400) ? (void *)&((gpu_t *)second)->engine[1]
                                  : (void *)&((gpu_t *)second)->engine[0];

    fn_writer_5 write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | 0x100000u, value & 0xffffu, 2u, extra);

    uint16_t updated = (uint16_t)value;
    __builtin_memcpy(cache, &updated, 2);
}
#undef OFF_PALETTE_CACHE
#undef OFF_POINTERS

#define OFF_MARK     0xfbe78
#define OFF_PALETTE    0x16070
#define OFF_PALETTE_CACHE     0x16870
#define PALETTE_RAM_SIZE     0x800
#define OFF_AUX       0xfbe60
#define OFF_TEMPLATE 0x133a48
#define OFF_POINTERS  0xfba68
typedef void (*fn_writer_6)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void vram_reg_cache_write16_init(uint8_t *state, unsigned dir, unsigned value) {

    void *mark;
    memcpy(&mark, state + OFF_MARK, 8);
    if (mark != (void *)vram_reg_cache_write8) {
        uint8_t *table = state + OFF_PALETTE_CACHE;
        memcpy(table, state + OFF_PALETTE, PALETTE_RAM_SIZE);

        const uint8_t *template = (const uint8_t *)vram_cache_write_handlers;
        uint8_t *aux = state + OFF_AUX;

        aux[80] = 0;
        memcpy(aux, &table, 8);
        memcpy(state + OFF_MARK + 16, template + 16, 8);
        memcpy(state + OFF_MARK, template, 16);
    }

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + OFF_PALETTE_CACHE + idx;

    uint16_t prev;
    memcpy(&prev, cache, 2);
    if (prev == (uint16_t)value) return;

    void **pointers = (void **)(state + OFF_POINTERS);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    memcpy(&extra, first + 20, 2);

    void *dest = (dir & 0x400) ? (void *)&((gpu_t *)second)->engine[1]
                                  : (void *)&((gpu_t *)second)->engine[0];

    fn_writer_6 write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | 0x100000u, value & 0xffffu, 2u, extra);

    uint16_t updated = (uint16_t)value;
    memcpy(cache, &updated, 2);
}
#undef OFF_MARK
#undef OFF_PALETTE
#undef OFF_PALETTE_CACHE
#undef PALETTE_RAM_SIZE
#undef OFF_AUX
#undef OFF_TEMPLATE
#undef OFF_POINTERS

#define OFF_INIT_FLAG    0xfbe78
#define OFF_SELF_REF     0xfbe60
#define OFF_ZERO_BYTE    0xfbeb0
#define OFF_PALETTE_CACHE    0x16870
#define OFF_PALETTE    0x16070
#define PALETTE_RAM_SIZE        0x800
#define FN_TABLE_SIZE     24





uint32_t vram_write_cache_read16_lazy(uint8_t *state, uint32_t param_2)
{

    void *field = rd_ptr(state + OFF_INIT_FLAG);
    void *target = (void *)vram_reg_cache_write8;

    if (field != target) {
        memcpy(state + OFF_PALETTE_CACHE, state + OFF_PALETTE, PALETTE_RAM_SIZE);

        uint8_t table[FN_TABLE_SIZE];
        memcpy(table, vram_cache_write_handlers, FN_TABLE_SIZE);

        state[OFF_ZERO_BYTE] = 0;
        wr_ptr(state + OFF_SELF_REF, state + OFF_PALETTE_CACHE);
        memcpy(state + OFF_INIT_FLAG, table, FN_TABLE_SIZE);
    }

    uint32_t idx = param_2 & 0x7ff;
    return (uint32_t)rd16(state + OFF_PALETTE_CACHE + idx);
}
#undef OFF_INIT_FLAG
#undef OFF_SELF_REF
#undef OFF_ZERO_BYTE
#undef OFF_PALETTE_CACHE
#undef OFF_PALETTE
#undef PALETTE_RAM_SIZE
#undef FN_TABLE_SIZE


void vram_reg_cache_write32(unsigned char *machine, uint32_t dir, uint32_t value) {

    uint64_t key = dir & 0x7ff;
    bus_t *bus = (bus_t *)machine;
    unsigned char *cache = bus->palette_cache;

    if (*(uint32_t *)(cache + key) == value) return;

    unsigned char *a = (unsigned char *)bus->machine;
    uint32_t extra = *(uint16_t *)(a + 20);

    gpu2d_engine_t *list = &bus->gpu->engine[(dir & 0x400) == 0 ? 0 : 1];

    gpu2d_deferred_capture_queue_push(list, (uint32_t)key | 0x100000, value, 4, extra);

    *(uint32_t *)(cache + key) = value;
}

#define OFF_MARK     0xfbe78
#define OFF_PALETTE    0x16070
#define OFF_PALETTE_CACHE     0x16870
#define PALETTE_RAM_SIZE     0x800
#define OFF_AUX       0xfbe60
#define OFF_TEMPLATE 0x133a48
#define OFF_POINTERS  0xfba68
typedef void (*fn_writer_9)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void vram_reg_cache_write32_init(uint8_t *state, unsigned dir, unsigned value) {

    void *mark;
    memcpy(&mark, state + OFF_MARK, 8);
    if (mark != (void *)vram_reg_cache_write8) {
        uint8_t *table = state + OFF_PALETTE_CACHE;
        memcpy(table, state + OFF_PALETTE, PALETTE_RAM_SIZE);

        const uint8_t *template = (const uint8_t *)vram_cache_write_handlers;
        uint8_t *aux = state + OFF_AUX;

        aux[80] = 0;
        memcpy(aux, &table, 8);
        memcpy(state + OFF_MARK + 16, template + 16, 8);
        memcpy(state + OFF_MARK, template, 16);
    }

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + OFF_PALETTE_CACHE + idx;

    uint32_t prev;
    memcpy(&prev, cache, 4);
    if (prev == value) return;

    void **pointers = (void **)(state + OFF_POINTERS);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    memcpy(&extra, first + 20, 2);

    void *dest = (dir & 0x400) ? (void *)&((gpu_t *)second)->engine[1]
                                  : (void *)&((gpu_t *)second)->engine[0];

    fn_writer_9 write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | 0x100000u, value, 4u, extra);

    memcpy(cache, &value, 4);
}
#undef OFF_MARK
#undef OFF_PALETTE
#undef OFF_PALETTE_CACHE
#undef PALETTE_RAM_SIZE
#undef OFF_AUX
#undef OFF_TEMPLATE
#undef OFF_POINTERS

#define OFF_MARK     0xfbe78UL
#define OFF_FN_B      0xfbe80UL
#define OFF_FN_C      0xfbe88UL
#define OFF_BUFPTR    0xfbe60UL
#define OFF_FLAGB     0xfbeb0UL
#define OFF_PALETTE_CACHE       0x16870UL
#define OFF_PALETTE       0x16070UL
#define PALETTE_RAM_SIZE     0x800UL

uint32_t vram_reg_cache_read32_init(uint8_t *param_1, uint64_t param_2)
{

    static void *(*s_memcpy)(void *, const void *, size_t);
    if (!s_memcpy)
        s_memcpy = (void *(*)(void *, const void *, size_t))sym_libc_memcpy;

    void *mark_list = (void *)vram_reg_cache_write8;

    if (rd_ptr(param_1 + OFF_MARK) != mark_list) {

        const uint8_t *table = (const uint8_t *)vram_cache_write_handlers;
        uint64_t v_mark = rd64(table + 0);
        uint64_t v_fn_b  = rd64(table + 8);
        uint64_t v_fn_c  = rd64(table + 16);

        s_memcpy(param_1 + OFF_PALETTE_CACHE, param_1 + OFF_PALETTE, PALETTE_RAM_SIZE);

        wr8(param_1 + OFF_FLAGB, 0);
        wr_ptr(param_1 + OFF_BUFPTR, param_1 + OFF_PALETTE_CACHE);

        wr64(param_1 + OFF_FN_C, v_fn_c);
        wr64(param_1 + OFF_FN_B, v_fn_b);
        wr64(param_1 + OFF_MARK, v_mark);
    }

    return rd32(param_1 + OFF_PALETTE_CACHE + (param_2 & 0x7ffUL));
}
#undef OFF_MARK
#undef OFF_FN_B
#undef OFF_FN_C
#undef OFF_BUFPTR
#undef OFF_FLAGB
#undef OFF_PALETTE_CACHE
#undef OFF_PALETTE
#undef PALETTE_RAM_SIZE

void *vram_resolve_ptr_region10(bus_t *bus, uint32_t dir) {
    uint32_t i = (dir >> 14) & 0x3ff;
    return bus->gpu->vram.slot_base[i] + (dir & 0xffffff);
}

unsigned char *vram_resolve_ptr_bus_tagged(bus_t *bus, uint32_t dir)
{
    vram_map_t *vram = &bus->gpu->vram;
    unsigned char *ctx = (unsigned char *)vram;
    uint32_t idx = (dir >> 14) & 0x3ffu;
    uint8_t *page_v = vram->slot_base[idx];
    uint8_t *forbidden = bus->unmapped_page;
    uint32_t tag = vram->slot_tag[idx];
    uint8_t *p = page_v + (uint64_t)(dir & 0xffc000u);
    uint8_t *discard = bus->discard_page;

    if (p == forbidden)
        p = discard;

    unsigned char *map = (unsigned char *)&GPU_OUTPUT_OF(ctx)->capture.bank_bits;

    if (tag <= 0x1fu) {
        uint32_t word;
        memcpy(&word, map, 4);
        word |= 1u << (tag & 31u);
        unsigned char *vec = &GPU_OUTPUT_OF(ctx)->bank_texture_bits[tag >> 3];
        memcpy(map, &word, 4);
        uint32_t byte_map = *vec;
        byte_map = byte_map & ~(1u << (tag & 7u));
        *vec = (unsigned char)byte_map;
    } else {
        uint32_t word;
        memcpy(&word, map + 4, 4);
        word |= 1u << (tag & 31u);
        memcpy(map + 4, &word, 4);
    }

    return p + (uint64_t)(dir & 0x3fffu);
}

void *vram_resolve_ptr_bank17(bus_t *bus, uint32_t dir) {
    uint32_t which = (dir >> 17) & 1;
    return bus->gpu->vram.arm7_vram[which] + (dir & 0x1ffff);
}

void *vram_resolve_ptr_bank17_diverted(bus_t *bus, uint32_t dir) {
    uint32_t bank = (dir >> 17) & 1;
    unsigned char *p = bus->gpu->vram.arm7_vram[bank];

    if (p == bus->blank_page)
        p = bus->scratch_page;

    return p + (uint64_t)(dir & 0x1ffff);
}

void *vram_resolve_ptr_bank14_second_pair(bus_t *bus, uint32_t dir) {
    uint32_t which = (dir >> 14) & 1;
    return bus->wram_window[0][which] + (dir & 0x3fff);
}

void *vram_resolve_ptr_bank14_first_pair(bus_t *bus, uint32_t dir) {
    uint32_t which = (dir >> 14) & 1;
    return bus->wram_window[1][which] + (dir & 0x3fff);
}

long vram_resolve_ptr_returns_null(long a0, long a1, long a2, long a3,
                         long a4, long a5, long a6, long a7)
{

    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;

    return 0;
}

void *vram_resolve_cell_ptr_coarse(void *unused, unsigned char *entry, uint32_t dir) {
    (void)unused;
    unsigned char *buf = *(unsigned char **)(entry + 56);
    if (buf == 0)
        return 0;
    uint32_t v = (*(const uint32_t *)entry & dir) >> 14;
    return buf + ((uint64_t)v & 0x3fffc);
}

void *vram_resolve_cell_ptr_fine(void *unused, unsigned char *entry, uint32_t dir) {
    (void)unused;
    unsigned char *buf = *(unsigned char **)(entry + 64);
    if (buf == 0)
        return 0;
    uint32_t v = (*(const uint32_t *)entry & dir) >> 4;
    return buf + ((uint64_t)v & 0xffffffc);
}

void *vram_resolve_slot_page64k(bus_t *bus, unsigned char *obj, uint32_t dir) {

    void *(*translate)(void *, uint32_t) = *(void *(**)(void *, uint32_t))(obj + 8);
    unsigned char *p = (unsigned char *)translate(bus, dir);

    if (p == bus->unmapped_page) return 0;

    unsigned char *base = bus->vram_bank[0];
    unsigned char *table = *(unsigned char **)(obj + 0x38);

    int64_t d = (int64_t)(p - base);
    if (d < 0) d += 0xffff;
    uint64_t idx = (uint64_t)d >> 16;

    return table + (uint64_t)(uint32_t)idx * 4;
}

void *vram_resolve_slot64(bus_t *bus, unsigned char *obj, uint32_t dir) {

    void *(*translate)(void *, uint32_t) = *(void *(**)(void *, uint32_t))(obj + 8);
    unsigned char *p = (unsigned char *)translate(bus, dir);

    if (p == bus->unmapped_page) return 0;

    unsigned char *base = bus->vram_bank[0];
    unsigned char *table = *(unsigned char **)(obj + 64);

    int64_t d = (int64_t)(p - base);
    if (d < 0) d += 0x3f;
    uint64_t idx = (uint64_t)d >> 6;

    return table + (uint64_t)(uint32_t)idx * 4;
}



















#undef F

typedef void *(*fn_malloc)(uint64_t size);
typedef void *(*fn_realloc)(void *p, uint64_t size);
typedef void (*fn_free)(void *p);
typedef void (*fn_qsort)(void *base, uint64_t count, uint64_t size,
                         int (*comparator)(const void *, const void *));
typedef int (*fn_fprintf)(void *stream, const char *format, ...);
typedef int (*fn_fflush)(void *stream);
typedef int (*fn_fputc)(int character, void *stream);
typedef uint64_t (*fn_fwrite)(const void *p, uint64_t size,
                              uint64_t count, void *stream);
typedef int (*fn_fclose)(void *stream);







int32_t vram_entry_compare_by_field8(const void *param_1, const void *param_2) {

    int32_t w8, w9;
    memcpy(&w8, (const unsigned char *)param_2 + 8, sizeof(w8));
    memcpy(&w9, (const unsigned char *)param_1 + 8, sizeof(w9));

    return w8 - w9;
}

typedef void (*fn_apply)(vram_map_t *, uint8_t *, unsigned, unsigned, unsigned);

void vram_vramcnt_write_cascade(vram_map_t *vram, uint8_t *bank_data, unsigned idx,
                        unsigned updated) {

    if (vram->bank[idx].control == updated) return;

    fn_apply f = vram_vramcnt_bank_remap;

    vram->changed_banks = 0;
    f(vram, bank_data, idx, updated, 1);

    uint16_t bits = vram->changed_banks;
    if (bits == 0) return;

    unsigned i = 0;
    while (bits) {
        if (bits & 1) {
            vram->bank[i].control = 0xffffffffu;
            f(vram, vram->bank_data[i], i, *vram->bank_control_reg[i], 0);
        }
        bits >>= 1;
        i++;
    }
}

void pagetable_unmap_range(unsigned char *t, unsigned int dir, unsigned int len);
void pagetable_clip_write(unsigned char *machine, unsigned int dir, unsigned int len);



#define SL32(v, s) ((uint32_t)((uint32_t)(v) << ((uint32_t)(s) & 31u)))
#define SR32(v, s) ((uint32_t)((uint32_t)(v) >> ((uint32_t)(s) & 31u)))

static void mark_range(vram_map_t *vram, uint32_t base, uint32_t n)
{
    uint32_t q;

    if (base > 0x1ff)
        return;
    q = base >> 5;
    vram->slot_touched[q] |= SL32(~SL32(0xffffffffu, n), base);
    vram->slot_touched_groups |= SL32(1u, q);
}

static void refresh_substate(vram_map_t *vram, uint32_t k)
{
    gpu2d_engine_t *engine = &((gpu_t *)vram)->engine[k];
    uint8_t **tab = engine->bg_ext_palette;

    engine->bg[0].ext_palette = tab[(engine->bg[0].bgcnt >> 12) & 2];
    engine->bg[1].ext_palette = tab[((engine->bg[1].bgcnt >> 12) & 2) | 1];
    engine->bg[2].ext_palette = tab[2];
    engine->bg[3].ext_palette = tab[3];
}
static unsigned char *pagetable_arm9(vram_map_t *vram)
{
    return (unsigned char *)vram->bus->arm9_pagetable;
}

static void unmap_round(vram_map_t *vram, vram_bank_t *obj,
                           uint32_t bank, uint32_t j0)
{
    uint8_t *pbase = vram->bus->unmapped_page;
    uint32_t n = obj->mapped_kb >> 4;
    uint32_t sh = (uint32_t)(j0 << 14);
    uint8_t *ptr;
    uint32_t mask, j, cnt;

    pagetable_unmap_range(pagetable_arm9(vram), sh + NDS_VRAM_BASE, (uint32_t)(n << 14));

    ptr = pbase - (uint64_t)sh;
    mark_range(vram, j0, n);

    mask = ~(1u << (bank & 31));
    j = j0;
    cnt = n;
    do {
        uint16_t v = (uint16_t)(vram->slot_banks[j] & mask);

        vram->slot_banks[j] = v;
        vram->slot_base[j] = ptr;
        vram->slot_tag[j] = 0;
        vram->changed_banks |= v;
        ptr -= 0x4000;
        j++;
    } while (--cnt != 0);
}

static void map_range(vram_map_t *vram, uint32_t slot, uint32_t tag,
                        uint32_t cnt, uint64_t ptr, uint32_t bit)
{
    do {
        vram->slot_base[slot] = (uint8_t *)(uintptr_t)ptr;
        vram->slot_banks[slot] |= (uint16_t)bit;
        vram->slot_tag[slot] = (unsigned char)tag;
        slot++;
        tag++;
    } while (--cnt != 0);
}

void vram_vramcnt_bank_remap(vram_map_t *vram, uint8_t *bank_data,
                        unsigned int idx, unsigned int attrs,
                        unsigned int mode)
{

    {
    const uint64_t x21 = (uint64_t)(uintptr_t)bank_data;
    unsigned char *engine = (unsigned char *)vram;
    vram_bank_t *obj = &vram->bank[idx];
    unsigned char *x5  = (unsigned char *)((gpu_t *)engine)->texture_cache.dirty_pending;
    unsigned char *x9m = (unsigned char *)&GPU_OUTPUT_OF(engine)->capture.bank_bits;
    uint32_t bank = idx;
    uint32_t w6 = attrs;
    uint32_t w7 = vram_bank_size_kb[idx];
    uint32_t w16 = (attrs & 0x80u) ? (attrs & 7u) : 6u;
    uint32_t w26 = idx << 3;
    uint32_t mask = ~(1u << (bank & 31));
    uint32_t w24 = 0, w25 = 0, w8 = 0, w10v;
    uint32_t w22n = 0, w27bit;

    if (mode != 0) {
        uint32_t type = obj->map_kind;
        if (type <= 10) {
            uint32_t a = obj->first_slot;
            uint32_t b = obj->mapped_kb;
            uint32_t n, j, cnt;
            uint8_t *empty;

            switch (type) {
            case 0: case 1: {
                empty = vram->bus->unmapped_page;
                n = b >> 3;
                j = a; cnt = n;
                do {
                    uint16_t v = (uint16_t)(vram->bg_ext_palette_banks[type][j] & mask);
                    vram->bg_ext_palette_banks[type][j] = v;
                    if (v == 0)
                        vram->bg_ext_palette[type][j] = empty;
                    j++;
                } while (--cnt != 0);
                refresh_substate(vram, type);
                break;
            }
            case 2: case 3: {
                uint32_t k = type - 2;
                empty = vram->bus->unmapped_page;
                n = b >> 3;
                j = a; cnt = n;
                do {
                    uint16_t v = (uint16_t)(vram->obj_ext_palette_banks[k + j] & mask);
                    vram->obj_ext_palette_banks[k + j] = v;
                    if (v == 0)
                        vram->obj_ext_palette[k + j] = empty;
                    j++;
                } while (--cnt != 0);

                ((gpu_t *)vram)->engine[k].obj_ext_palette = 0;
                break;
            }
            case 4: {
                n = b >> 7;
                j = a; cnt = n;
                do {
                    uint16_t v = (uint16_t)(vram->texture_banks[j] & mask);
                    vram->texture_banks[j] = v;
                    if (v == 0)
                        vram->texture[j] = 0;
                    j++;
                } while (--cnt != 0);

                wr32(x5 + 8, rd32(x5 + 8) & ~SL32(0xffu, w26));
                break;
            }
            case 5: {
                n = b >> 4;
                j = a; cnt = n;
                do {
                    uint16_t v = (uint16_t)(vram->texture_palette_banks[j] & mask);
                    vram->texture_palette_banks[j] = v;
                    if (v == 0)
                        vram->texture_palette[j] = 0;
                    j++;
                } while (--cnt != 0);

                if (bank == 4)
                    wr32(x5 + 16, 0);
                else
                    wr32(x5 + 16, rd32(x5 + 16) & ~(1u << (bank & 31)));
                break;
            }
            case 6:
                unmap_round(vram, obj, bank, a);
                break;
            case 7: {
                uint32_t a2, b2;
                pagetable_clip_write((unsigned char *)vram->bus->arm7_pagetable,
                                   0x6000000u + (uint32_t)(a << 14),
                                   (uint32_t)(b << 10));
                a2 = obj->first_slot;
                b2 = obj->mapped_kb;
                empty = vram->bus->unmapped_page;
                n = b2 >> 7;
                j = a2; cnt = n;
                do {
                    uint16_t v = (uint16_t)(vram->arm7_vram_banks[j] & mask);
                    vram->arm7_vram_banks[j] = v;
                    if (v == 0)
                        vram->arm7_vram[j] = empty;
                    j++;
                } while (--cnt != 0);
                break;
            }
            case 8:
                unmap_round(vram, obj, bank, a);
                unmap_round(vram, obj, bank, a + 3);
                break;
            case 9:
                unmap_round(vram, obj, bank, a);
                unmap_round(vram, obj, bank, a + 1);
                unmap_round(vram, obj, bank, a + 4);
                unmap_round(vram, obj, bank, a + 5);
                break;
            default:
                unmap_round(vram, obj, bank, a);
                unmap_round(vram, obj, bank, a + 4);
                break;
            }
        }
    }

    w24 = (w6 >> 3) & 3u;
    w10v = w6 >> 3;
    w27bit = 1u << (bank & 31);

    if (w16 > 6)
        goto L_generic;

    switch (w16) {
    case 0:
        if (bank >= 9)
            goto L_generic;
        w25 = vram_bank_first_slot[bank];
        goto L_generic;

    case 1:
        w25 = 0;
        if (bank > 8)
            goto L_generic;
        switch (bank) {
        case 0: case 1: case 2: case 3:
            w25 = w24 << 3;
            goto L_generic;
        case 4:
            goto L_generic;
        case 5: case 6:
            w24 = ((w10v << 1) & 4u) | ((w6 >> 3) & 1u);
            goto L_two_blocks;
        case 7:
            goto L_bank_h;
        default:
            goto L_bank_i;
        }

    case 2:
        w24 = 0;
        w8 = 1;
        if (bank > 8) { w25 = w24; goto L_generic; }
        switch (bank) {
        case 0: case 1:
            w25 = (w6 & 8u) | 0x100u;
            goto L_generic;
        case 2: case 3:
            goto L_block_tail;
        case 4:
            w25 = 0x100;
            goto L_generic;
        case 5: case 6:
            w24 = (((w10v << 1) & 4u) | ((w6 >> 3) & 1u)) | 0x100u;
            goto L_two_blocks;
        case 7:
            goto L_texture_slots;
        default:
            w25 = 0x180;
            goto L_generic;
        }

    case 3:
        w25 = 0;
        if (bank > 8)
            goto L_generic;
        w8 = 3;
        switch (bank) {
        case 0: case 1: case 2: case 3:
            goto L_mst3_abcd;
        case 4:
            goto L_mst3_e;
        case 5: case 6:
            w25 = ((w10v << 1) & 4u) | ((w6 >> 3) & 1u);
            goto L_mst3_e;
        case 7:
            goto L_unmapped;
        default:
            goto L_pair_2170;
        }

    case 4:
        if (bank > 8) { w25 = 0; goto L_generic; }
        switch (bank) {
        case 2:
            w25 = 0x80;
            goto L_generic;
        case 3:
            w25 = 0x180;
            goto L_generic;
        case 4:
            w24 = 0; w8 = 0; w7 >>= 1;
            goto L_texture_slots;
        case 5: case 6:
            w8 = 0;
            w24 = (w10v & 1u) << 1;
            goto L_texture_slots;
        default:
            goto L_unmapped;
        }

    case 5:
        w24 = 0;
        if (bank > 8) { w25 = w24; goto L_generic; }
        if (((1u << (bank & 31)) & 0x19fu) == 0) {
            w8 = 2;
            goto L_pair_2170;
        }
        w8 = 12;
        goto L_epilogue;

    default:
        w24 = 0;
        w8 = 11;
        goto L_epilogue;
    }

L_unmapped:
    w24 = 0;
    w8 = 12;
    goto L_epilogue;

L_generic:
    {
        uint32_t nn = w7 >> 4;
        uint32_t w19, w28 = w7;
        uint64_t p;

        w24 = nn;
        w19 = (uint32_t)(w25 << 14);
        w22n = (uint32_t)(nn << 14);

        pagetable_unmap_range(pagetable_arm9(vram), w19 + NDS_VRAM_BASE, w22n);
        mark_range(vram, w25, nn);

        p = x21 - (uint64_t)w19;
        map_range(vram, w25, w26, w24, p, w27bit);

        if ((w25 & 0xffffff80u) == 0x80u) {
            uint32_t w23 = w25 + 8;

            w19 = (uint32_t)(w23 << 14);
            pagetable_unmap_range(pagetable_arm9(vram), w19 + NDS_VRAM_BASE, w22n);
            mark_range(vram, w23, w24);
            w7 = w28;
            p = x21 - (uint64_t)w19;
            map_range(vram, w23, w26, w24, p, w27bit);
        } else {
            w7 = w28;
        }

        w8 = 6;
        w24 = w25;
        goto L_epilogue;
    }

L_two_blocks:
    {
        uint32_t w19, w23, w28 = w7;
        uint64_t p;

        w25 = w7 >> 4;
        w22n = (uint32_t)(w25 << 14);
        w19 = (uint32_t)(w24 << 14);

        pagetable_unmap_range(pagetable_arm9(vram), 0x6000000u | ((w24 & 0x1ffu) << 14), w22n);
        mark_range(vram, w24, w25);

        p = x21 - (uint64_t)w19;
        map_range(vram, w24, w26, w25, p, w27bit);

        w23 = w24 + 3;
        w19 = (uint32_t)(w23 << 14);
        pagetable_unmap_range(pagetable_arm9(vram), 0x6000000u | ((w23 & 0x3ffu) << 14), w22n);
        mark_range(vram, w23, w25);
        w7 = w28;
        p = x21 - (uint64_t)w19;
        map_range(vram, w23, w26, w25, p, w27bit);

        w8 = 8;
        goto L_epilogue;
    }

L_block_tail:
    {
        uint32_t n, slot;
        uint64_t p;

        w24 = w6 & 8u;
        pagetable_clip_write((unsigned char *)vram->bus->arm7_pagetable, 0x6000000u | (((w24 >> 3) & 1u) << 17),
                           (uint32_t)(w7 << 10));
        n = w7 >> 7;
        slot = w24;
        p = x21;
        do {
            vram->arm7_vram[slot] = (uint8_t *)(uintptr_t)p;
            vram->arm7_vram_banks[slot] |= (uint16_t)w27bit;
            slot++;
            p += 0x20000;
        } while (--n != 0);
        w8 = 7;
        goto L_epilogue;
    }

L_mst3_e:
    {
        uint32_t n = w7 >> 4;
        uint32_t slot = w25;
        uint64_t p = x21;

        do {
            vram->texture_palette[slot] = (uint8_t *)(uintptr_t)p;
            vram->texture_palette_banks[slot] |= (uint16_t)w27bit;
            p += 0x4000;
            slot++;
        } while (--n != 0);

        if (bank == 4) {
            uint32_t v8 = rd32(x9m + 4);
            uint32_t v10 = rd32(x5 + 4);
            wr32(x5 + 4, v10 | (v8 & 0xfu));
            wr32(x9m + 4, v8 & 0xffffff00u);
            wr32(x5 + 16, 15);
        } else {
            uint32_t v10 = rd32(x9m + 4);
            uint32_t v11 = rd32(x5 + 4);
            uint32_t m   = SL32(0xffu, w26);
            uint32_t bit = SR32(v10, w26) & 1u;
            wr32(x5 + 4, SL32(bit, w25 << 3) | v11);
            wr32(x9m + 4, v10 & ~m);
            wr32(x5 + 16, rd32(x5 + 16) | w27bit);
        }
        w8 = 5;
        w24 = w25;
        goto L_epilogue;
    }

L_mst3_abcd:
    {
        uint32_t v8 = rd32(x9m);
        uint32_t n = w7 >> 7;
        uint32_t w11v = SR32(v8, w26);
        uint32_t slot = w24;
        uint64_t p = x21;
        uint32_t v12, v14, contrib, mbyte;

        do {
            vram->texture[slot] = (uint8_t *)(uintptr_t)p;
            vram->texture_banks[slot] |= (uint16_t)w27bit;
            p += 0x20000;
            slot++;
        } while (--n != 0);

        v12 = rd32(x5);
        v14 = rd32(x5 + 8);
        contrib = SL32(w11v & 0xffu, w24 << 3);
        mbyte  = SL32(0xffu, w26);
        v8 &= ~mbyte;
        wr32(x5, v12 | contrib);
        wr32(x5 + 8, v14 | mbyte);
        wr32(x9m, v8);
        w8 = 4;
        goto L_epilogue;
    }

L_pair_2170:
    {
        uint32_t n = w7 >> 4;
        uint32_t w9s = w8 - 2;
        uint32_t slot = w9s;
        uint64_t p = x21;

        do {
            vram->obj_ext_palette[slot] = (uint8_t *)(uintptr_t)p;
            vram->obj_ext_palette_banks[slot] |= (uint16_t)w27bit;
            slot++;
            p += 0x2000;
        } while (--n != 0);

        ((gpu_t *)vram)->engine[w9s].obj_ext_palette = vram->obj_ext_palette[w9s];
        w24 = 0;
        w7 >>= 1;
        goto L_epilogue;
    }

L_texture_slots:
    {
        uint32_t n = w7 >> 3;
        uint32_t slot = w24;
        uint64_t p = x21;

        do {
            vram->bg_ext_palette[w8][slot] = (uint8_t *)(uintptr_t)p;
            vram->bg_ext_palette_banks[w8][slot] |= (uint16_t)w27bit;
            p += 0x2000;
            slot++;
        } while (--n != 0);

        refresh_substate(vram, w8);
        goto L_epilogue;
    }

L_bank_h:
    {
        uint32_t v11, v12, v13, v8;
        uint64_t p;

        w25 = w7 >> 4;
        w22n = (uint32_t)(w25 << 14);
        pagetable_unmap_range(pagetable_arm9(vram), 0x6200000u, w22n);

        v12 = vram->slot_touched[4];
        v13 = vram->slot_touched_groups;
        v11 = v12 | ~SL32(0xffffffffu, w25);
        p = x21 - 0x200000u;
        vram->slot_touched[4] = v11;
        vram->slot_touched_groups = v13 | 0x10u;
        map_range(vram, 0x80, 0x38, w25, p, 0x80);

        pagetable_unmap_range(pagetable_arm9(vram), 0x6210000u, w22n);
        v11 = vram->slot_touched[4];
        v12 = vram->slot_touched_groups;
        v8 = SL32(0xfffffff0u, w25) ^ 0xfffffff0u;
        p = x21 - 0x210000u;
        vram->slot_touched_groups = v12 | 0x10u;
        vram->slot_touched[4] = v11 | v8;
        map_range(vram, 0x84, 0x38, w25, p, 0x80);

        w24 = 0x80;
        w8 = 10;
        goto L_epilogue;
    }

L_bank_i:
    {
        uint32_t v10, v11, v12, m;
        uint64_t p;

        w25 = w7 >> 4;
        w22n = (uint32_t)(w25 << 14);
        pagetable_unmap_range(pagetable_arm9(vram), 0x6208000u, w22n);

        v10 = vram->slot_touched[4];
        v11 = vram->slot_touched_groups;
        m = SL32(0xfffffffcu, w25) ^ 0xfffffffcu;
        p = x21 - 0x208000u;
        vram->slot_touched[4] = v10 | m;
        vram->slot_touched_groups = v11 | 0x10u;
        map_range(vram, 0x82, 0x40, w25, p, 0x100);

        pagetable_unmap_range(pagetable_arm9(vram), 0x620c000u, w22n);
        v11 = vram->slot_touched[4];
        v12 = vram->slot_touched_groups;
        m = SL32(0xfffffff8u, w25) ^ 0xfffffff8u;
        p = x21 - 0x20c000u;
        vram->slot_touched[4] = v11 | m;
        vram->slot_touched_groups = v12 | 0x10u;
        map_range(vram, 0x83, 0x40, w25, p, 0x100);

        pagetable_unmap_range(pagetable_arm9(vram), 0x6218000u, w22n);
        v11 = vram->slot_touched[4];
        v12 = vram->slot_touched_groups;
        m = SL32(0xffffffc0u, w25) ^ 0xffffffc0u;
        p = x21 - 0x218000u;
        vram->slot_touched[4] = v11 | m;
        vram->slot_touched_groups = v12 | 0x10u;
        map_range(vram, 0x86, 0x40, w25, p, 0x100);

        pagetable_unmap_range(pagetable_arm9(vram), 0x621c000u, w22n);
        v11 = vram->slot_touched[4];
        v12 = vram->slot_touched_groups;
        m = SL32(0xffffff80u, w25) ^ 0xffffff80u;
        p = x21 - 0x21c000u;
        vram->slot_touched_groups = v12 | 0x10u;
        vram->slot_touched[4] = v11 | m;
        map_range(vram, 0x87, 0x40, w25, p, 0x100);

        w24 = 0x82;
        w8 = 9;
        goto L_epilogue;
    }

L_epilogue:
    obj->control = w6;
    obj->first_slot = w24;
    obj->map_kind = w8;
    obj->mapped_kb = w7;
    return;
    }
}
#undef SL32
#undef SR32

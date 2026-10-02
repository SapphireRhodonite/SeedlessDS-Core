#include "hires_runtime.h"
#include "blob_symbols.h"
#include "memory/io_common.h"
#include "memory/io9.h"
#include "memory/io7.h"
#include "cart/cart.h"
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "core/nds_state.h"
#include "core_internals.h"
#include "mem_access.h"

#define PT(p) ((pagetable_t *)(p))



static uint64_t asr64_2(uint64_t v)
{
    return (v >> 2) | ((v & UINT64_C(0x8000000000000000))
                       ? UINT64_C(0xc000000000000000) : 0);
}
typedef uint64_t (*fn_translate)(unsigned char *, uint32_t);

uint64_t bus_page_fill(unsigned char *base, uint32_t address)
{

    uint32_t page = address >> 23;
    bus_region_t *entry = &PT(base)->region[page];
    uint8_t mode = entry->read_kind;

    if (mode == 0) {
        uint32_t index_map_a = address >> 21;
        uint32_t *map_a = &PT(base)->group_bits[index_map_a];
        uint32_t map_a_old = rd32(map_a);
        uint32_t index_map_b = address >> 16;
        uint32_t mask = entry->mask;
        uint64_t pointer_base = (uint64_t)(uintptr_t)entry->read_memory;

        wr32(map_a, map_a_old | (UINT32_C(1) << (index_map_b & 31)));

        uint32_t *map_b = &PT(base)->page_bits[index_map_b];
        uint32_t map_b_old = rd32(map_b);
        uint32_t index_cache = address >> 11;
        wr32(map_b, map_b_old | (UINT32_C(1) << (index_cache & 31)));

        uint64_t aligned = (uint64_t)address & UINT64_C(0xfffff800);
        uint64_t dest = pointer_base + (aligned & mask);
        uint64_t cache = asr64_2(dest - aligned) | UINT64_C(0x4000000000000000);
        PT(base)->page[index_cache] = cache;

        uint32_t mask_reread = entry->mask;
        return pointer_base + (mask_reread & address);
    }

    if (mode == 1) {
        fn_translate callback = (fn_translate)entry->read_translate;
        unsigned char *ctx = (unsigned char *)PT(base)->bus;
        uint64_t result = callback(ctx, address);
        uint32_t inside_page = address & 0x7ff;
        uint32_t aligned = address - inside_page;
        uint32_t index_map_a = aligned >> 21;
        uint32_t *map_a = &PT(base)->group_bits[index_map_a];
        uint32_t map_a_old = rd32(map_a);
        uint32_t index_map_b = aligned >> 16;

        wr32(map_a, map_a_old | (UINT32_C(1) << (index_map_b & 31)));

        uint32_t *map_b = &PT(base)->page_bits[index_map_b];
        uint32_t map_b_old = rd32(map_b);
        uint32_t index_cache = aligned >> 11;
        wr32(map_b, map_b_old | (UINT32_C(1) << (index_cache & 31)));

        uint64_t cache = asr64_2((result - inside_page) - aligned) |
                         UINT64_C(0x4000000000000000);
        PT(base)->page[index_cache] = cache;
        return result;
    }

    return 0;
}

typedef unsigned  (*fn_value)(void *, unsigned);
typedef uint8_t  *(*fn_pointer)(void *, unsigned);

static void mark(uint8_t *state, uint32_t dir, int64_t shift, int bit62) {
    uint32_t *a = &PT(state)->group_bits[dir >> 21];
    *a |= 1u << ((dir >> 16) & 31);

    uint32_t *b = &PT(state)->page_bits[dir >> 16];
    *b |= 1u << ((dir >> 11) & 31);

    uint64_t v = (uint64_t)(shift >> 2);
    if (bit62) v |= PAGETABLE_MISSING;
    PT(state)->page[dir >> 11] = v;
}

unsigned bus_read8_slow(uint8_t *state, unsigned dir) {

    if (dir >> 28) return 0xff;

    bus_region_t *entry = &PT(state)->region[dir >> 23];
    unsigned type = entry->read_kind;

    void *arg;
    arg = PT(state)->bus;

    if (type == 2) {
        uint32_t mask;
        mask = entry->mask;
        fn_value f;
        f = (__typeof__(f))entry->read8;
        return f(arg, mask & dir);
    }

    if (type == 1) {
        fn_pointer f;
        f = (__typeof__(f))entry->read8;
        uint8_t *p = f(arg, dir);

        uint32_t low = dir & 0x7ff;
        uint32_t align = dir - low;
        int64_t shift = (int64_t)((uint64_t)p - low - align);
        mark(state, align, shift, 1);
        return *p;
    }

    if (type != 0) return 0;

    int flag = entry->write_kind;
    uint8_t *base;
    base = entry->read_memory;
    uint32_t mask;
    mask = entry->mask;

    uint64_t align = dir & 0xfffff800u;
    int64_t shift = (int64_t)((uint64_t)(base + (align & mask)) - align);
    mark(state, dir, shift, flag != 0);

    mask = entry->mask;
    return base[mask & dir];
}

typedef unsigned  (*fn_value_2)(void *, unsigned);
typedef uint8_t  *(*fn_pointer_2)(void *, unsigned);

static void mark_2(uint8_t *state, uint32_t dir, int64_t shift, int bit62) {
    uint32_t *a = &PT(state)->group_bits[dir >> 21];
    *a |= 1u << ((dir >> 16) & 31);

    uint32_t *b = &PT(state)->page_bits[dir >> 16];
    *b |= 1u << ((dir >> 11) & 31);

    uint64_t v = (uint64_t)(shift >> 2);
    if (bit62) v |= PAGETABLE_MISSING;
    PT(state)->page[dir >> 11] = v;
}

unsigned bus_read16_slow(uint8_t *state, unsigned dir) {

    if (dir >> 28) return 0xffff;

    bus_region_t *entry = &PT(state)->region[dir >> 23];
    unsigned type = entry->read_kind;

    void *arg;
    arg = PT(state)->bus;

    if (type == 2) {
        uint32_t mask;
        mask = entry->mask;
        fn_value_2 f;
        f = (__typeof__(f))entry->read16;
        return f(arg, mask & dir);
    }

    if (type == 1) {
        fn_pointer_2 f;
        f = (__typeof__(f))entry->read8;
        uint8_t *p = f(arg, dir);

        uint32_t low = dir & 0x7ff;
        uint32_t align = dir - low;
        int64_t shift = (int64_t)((uint64_t)p - low - align);
        mark_2(state, align, shift, 1);
        return rd16(p);
    }

    if (type != 0) return 0;

    int flag = entry->write_kind;
    uint8_t *base;
    base = entry->read_memory;
    uint32_t mask;
    mask = entry->mask;

    uint64_t align = dir & 0xfffff800u;
    int64_t shift = (int64_t)((uint64_t)(base + (align & mask)) - align);
    mark_2(state, dir, shift, flag != 0);

    mask = entry->mask;
    return rd16(base + (mask & dir));
}

typedef uint64_t (*fn_io)(void *, uint32_t);
typedef void    *(*fn_request)(void *, uint32_t);


static void mark_3(uint8_t *ctx, uint32_t d) {
    uint32_t *a = &PT(ctx)->group_bits[d >> 21];
    *a |= 1u << ((d >> 16) & 31);
    uint32_t *b = &PT(ctx)->page_bits[d >> 16];
    *b |= 1u << ((d >> 11) & 31);
}

uint64_t bus_read32_slow(uint8_t *ctx, uint32_t dir) {

    if ((dir >> 28) != 0) return 0xffffffffULL;

    bus_region_t *r = &PT(ctx)->region[dir >> 23];

    switch (r->read_kind) {

    case 2:
        return ((fn_io)r->read32)(PT(ctx)->bus, r->mask & dir);

    case 1: {
        uint32_t inside = dir & 0x7ffu;
        uint8_t *p = (uint8_t *)((fn_request)r->read_translate)(PT(ctx)->bus, dir);

        uint32_t al = dir - inside;
        mark_3(ctx, al);

        int64_t e = (int64_t)((uint64_t)(uintptr_t)p - inside - al);
        PT(ctx)->page[al >> 11] = (uint64_t)(e >> 2) | PAGETABLE_MISSING;

        return rd32(p);
    }

    case 0: {
        int mark = (r->write_kind != 0);
        uint32_t mask = r->mask;
        uint8_t *base = r->read_memory;

        mark_3(ctx, dir);

        uint64_t al = (uint64_t)dir & 0xfffff800ull;
        int64_t e = (int64_t)((uint64_t)(uintptr_t)base + (al & mask) - al);
        uint64_t ent = (uint64_t)(e >> 2);
        if (mark) ent |= PAGETABLE_MISSING;
        PT(ctx)->page[dir >> 11] = ent;

        return rd32(base + ((uint64_t)r->mask & dir));
    }

    default:
        return 0;
    }
}

typedef uint64_t (*fn_resolve)(uint8_t *ctx, uint32_t dir);

uint64_t bus_read64_slow(void *param_1, uint32_t param_2) {

    fn_resolve resolve = bus_read32_slow;

    uint32_t ret1 = (uint32_t)resolve(param_1, param_2);

    uint32_t ret2 = (uint32_t)resolve(param_1, param_2 + 4);

    return (uint64_t)ret1 | ((uint64_t)ret2 << 32);
}

typedef void (*fn_bus_write)(void *, unsigned, unsigned);
typedef uint8_t *(*fn_pointer_5)(void *, unsigned);

static void mark_5(uint8_t *state, uint32_t dir, int64_t shift) {
    uint32_t *a = &PT(state)->group_bits[dir >> 21];
    *a |= 1u << ((dir >> 16) & 31);

    uint32_t *b = &PT(state)->page_bits[dir >> 16];
    *b |= 1u << ((dir >> 11) & 31);

    uint64_t v = (uint64_t)(shift >> 2);
    PT(state)->page[dir >> 11] = v;
}

void bus_write8_slow(uint8_t *state, unsigned dir, unsigned value) {

    if (dir >> 28) return;

    bus_region_t *entry = &PT(state)->region[dir >> 23];
    unsigned type = entry->write_kind;

    if (type == 2) {
        uint32_t mask;
        mask = entry->mask;
        void *arg;
        arg = PT(state)->bus;
        fn_bus_write f;
        f = (__typeof__(f))entry->write8;
        f(arg, mask & dir, value);
        return;
    }

    if (type == 1) {
        void *arg;
        arg = PT(state)->bus;
        fn_pointer_5 f;
        f = (__typeof__(f))entry->write8;
        uint8_t *p = f(arg, dir);

        if (entry->read_kind == entry->write_kind) {
            uint32_t low = dir & 0x7ff;
            uint32_t align = dir - low;
            int64_t shift = (int64_t)((uint64_t)p - low - align);
            mark_5(state, align, shift);
        }
        *p = (uint8_t)value;
        return;
    }

    if (type != 0) return;

    unsigned flag = entry->read_kind;
    uint8_t *base;
    base = entry->write_memory;

    if (flag == 0) {
        uint32_t mask;
        mask = entry->mask;
        uint64_t align = dir & 0xfffff800u;
        int64_t shift = (int64_t)((uint64_t)(base + (align & mask)) - align);
        mark_5(state, dir, shift);
    }

    uint32_t mask;
    mask = entry->mask;
    base[(uint64_t)(mask & dir)] = (uint8_t)value;
}

typedef void (*fn_write_6)(void *, unsigned, unsigned);
typedef uint8_t *(*fn_pointer_6)(void *, unsigned);

static void mark_6(uint8_t *state, uint32_t dir, int64_t shift) {
    uint32_t *a = &PT(state)->group_bits[dir >> 21];
    *a |= 1u << ((dir >> 16) & 31);

    uint32_t *b = &PT(state)->page_bits[dir >> 16];
    *b |= 1u << ((dir >> 11) & 31);

    uint64_t v = (uint64_t)(shift >> 2);
    PT(state)->page[dir >> 11] = v;
}

void bus_write16_slow(uint8_t *state, unsigned dir, unsigned value) {

    if (dir >> 28) return;

    bus_region_t *entry = &PT(state)->region[dir >> 23];
    unsigned type = entry->write_kind;

    void *arg;
    arg = PT(state)->bus;

    if (type == 2) {
        uint32_t mask;
        mask = entry->mask;
        fn_write_6 f;
        f = (__typeof__(f))entry->write16;
        f(arg, mask & dir, value);
        return;
    }

    if (type == 1) {
        fn_pointer_6 f;
        f = (__typeof__(f))entry->write8;
        uint8_t *p = f(arg, dir);

        if (entry->read_kind == entry->write_kind) {
            uint32_t low = dir & 0x7ff;
            uint32_t align = dir - low;
            int64_t shift = (int64_t)((uint64_t)p - low - align);
            mark_6(state, align, shift);
        }
        wr16(p, value);
        return;
    }

    if (type != 0) return;

    unsigned flag = entry->read_kind;
    uint8_t *base;
    base = entry->write_memory;
    uint32_t mask;
    mask = entry->mask;

    if (flag == 0) {
        uint64_t align = dir & 0xfffff800u;
        int64_t shift = (int64_t)((uint64_t)(base + (align & mask)) - align);
        mark_6(state, dir, shift);
    }

    mask = entry->mask;
    wr16(base + (mask & dir), value);
}

typedef void (*fn_write_7)(void *, unsigned, unsigned);
typedef uint8_t *(*fn_pointer_7)(void *, unsigned);

static void mark_7(uint8_t *state, uint32_t dir, int64_t shift) {
    uint32_t *a = &PT(state)->group_bits[dir >> 21];
    *a |= 1u << ((dir >> 16) & 31);

    uint32_t *b = &PT(state)->page_bits[dir >> 16];
    *b |= 1u << ((dir >> 11) & 31);

    uint64_t v = (uint64_t)(shift >> 2);
    PT(state)->page[dir >> 11] = v;
}

void bus_write32_slow(uint8_t *state, unsigned dir, unsigned value) {

    if (dir >> 28) return;

    bus_region_t *entry = &PT(state)->region[dir >> 23];
    unsigned type = entry->write_kind;

    void *arg;
    arg = PT(state)->bus;

    if (type == 2) {
        uint32_t mask;
        mask = entry->mask;
        fn_write_7 f;
        f = (__typeof__(f))entry->write32;
        f(arg, mask & dir, value);
        return;
    }

    if (type == 1) {
        fn_pointer_7 f;
        f = (__typeof__(f))entry->write8;
        uint8_t *p = f(arg, dir);

        if (entry->read_kind == entry->write_kind) {
            uint32_t low = dir & 0x7ff;
            uint32_t align = dir - low;
            int64_t shift = (int64_t)((uint64_t)p - low - align);
            mark_7(state, align, shift);
        }
        wr32(p, value);
        return;
    }

    if (type != 0) return;

    unsigned flag = entry->read_kind;
    uint8_t *base;
    base = entry->write_memory;
    uint32_t mask;
    mask = entry->mask;

    if (flag == 0) {
        uint64_t align = dir & 0xfffff800u;
        int64_t shift = (int64_t)((uint64_t)(base + (align & mask)) - align);
        mark_7(state, dir, shift);
    }

    mask = entry->mask;
    wr32(base + (mask & dir), value);
}

typedef void (*fn_write32)(uint8_t *state, unsigned dir, unsigned value);

void bus_write64_slow(uint8_t *state, unsigned dir, uint64_t value64) {

    fn_write32 f = bus_write32_slow;

    f(state, dir, (unsigned)value64);
    f(state, dir + 4, (unsigned)(value64 >> 32));
}

extern unsigned bus_read8_slow_9(uint8_t *state, unsigned dir) __asm__("bus_read8_slow");

uint32_t bus_read8(uint8_t *param_1, uint32_t param_2)
{

    uint32_t idx = param_2 >> 11;

    uint64_t entry;
    memcpy(&entry, param_1 + (uint64_t)idx * 8, sizeof(entry));

    uint64_t dest = entry << 2;

    if (dest == 0) {

        return bus_read8_slow(param_1, param_2);
    }

    uint8_t v;
    memcpy(&v, (const uint8_t *)(uintptr_t)(dest + param_2), 1);
    return v;
}

uint32_t bus_read16(unsigned char *table, uint32_t dir) {

    uint64_t base = *(uint64_t *)(table + (uint64_t)(dir >> 11) * 8) << 2;
    if (base == 0)
        return bus_read16_slow(table, dir);
    return *(uint16_t *)(base + (uint64_t)dir);
}

uint32_t bus_read32(unsigned char *table, uint32_t dir) {

    uint64_t base = *(uint64_t *)(table + (uint64_t)(dir >> 11) * 8) << 2;
    if (base == 0)
        return bus_read32_slow(table, dir);
    return *(uint32_t *)(base + (uint64_t)dir);
}

typedef uint64_t (*fn_resolve_11)(uint8_t *ctx, uint32_t dir);


uint64_t bus_read64(uint8_t *ctx, uint32_t dir) {

    uint32_t idx = dir >> 11;
    uint64_t entry = rd64(ctx + (size_t)idx * 8);
    uint64_t x8 = entry << 2;

    if (x8 != 0) {

        uintptr_t addr = (uintptr_t)x8 + (uint32_t)dir;
        return rd64((const void *)addr);
    }

    fn_resolve_11 resolve = bus_read32_slow;

    uint32_t ret1 = (uint32_t)resolve(ctx, dir);
    uint32_t ret2 = (uint32_t)resolve(ctx, dir + 4);

    return (uint64_t)ret1 | ((uint64_t)ret2 << 32);
}


void bus_write8(uint8_t *param_1, uint64_t param_2, uint64_t param_3)
{

    uint32_t idx = (uint32_t)param_2 >> 11;

    uint64_t entry;
    memcpy(&entry, param_1 + (uint64_t)idx * 8, sizeof(entry));

    if (((entry >> 62) & 1u) != 0) {

        bus_write8_slow(param_1, param_2, param_3);
        return;
    }

    uint64_t dest = entry << 2;
    uint8_t value = (uint8_t)param_3;
    memcpy((void *)(uintptr_t)(dest + (uint64_t)(uint32_t)param_2),
           &value, 1);
}

extern void bus_write16_slow_13(uint8_t *state, unsigned dir, unsigned value) __asm__("bus_write16_slow");

void bus_write16(uint8_t *param_1, uint32_t param_2, uint16_t param_3)
{

    uint32_t idx = param_2 >> 11;

    uint64_t entry;
    memcpy(&entry, param_1 + (uint64_t)idx * 8, sizeof(entry));

    if (((entry >> 62) & 1u) != 0) {

        bus_write16_slow(param_1, param_2, param_3);
        return;
    }

    uint64_t dest = entry << 2;
    memcpy((void *)(uintptr_t)(dest + param_2), &param_3, 2);
}


void bus_write32(uint8_t *param_1, uint32_t param_2, uint32_t param_3)
{

    uint32_t idx = param_2 >> 11;

    uint64_t entry;
    memcpy(&entry, param_1 + (uint64_t)idx * 8, sizeof(entry));

    if (((entry >> 62) & 1u) != 0) {

        bus_write32_slow(param_1, param_2, param_3);
        return;
    }

    uint64_t dest = entry << 2;
    memcpy((void *)(uintptr_t)(dest + param_2), &param_3, 4);
}

typedef void (*fn_write32_15)(uint8_t *state, unsigned dir, unsigned value);

void bus_write64(uint8_t *param_1, uint32_t param_2, uint64_t param_3)
{

    uint32_t idx = param_2 >> 11;

    uint64_t entry;
    memcpy(&entry, param_1 + (size_t)idx * 8, sizeof(entry));

    if (((entry >> 62) & 1u) == 0) {
        uint64_t dest = (entry << 2) + (uint64_t)param_2;
        memcpy((void *)(uintptr_t)dest, &param_3, sizeof(param_3));
        return;
    }

    fn_write32_15 f = bus_write32_slow;

    f(param_1, param_2, (unsigned)param_3);
    f(param_1, param_2 + 4, (unsigned)(param_3 >> 32));
}

typedef uint8_t *(*fn_resolve_16)(bus_t *, unsigned int);

void *bus_region_page_lookup(bus_t *a, bus_region_t *b, unsigned int idx) {

    bus_t *bus = a;
    fn_resolve_16 resolve = (fn_resolve_16)b->read_translate;
    uint8_t *r = resolve(a, idx);

    uint8_t *mark = r - (idx & 0x3fffu);
    if (mark == bus->blank_page)
        return NULL;

    int64_t diff = r - bus->shared_wram;
    uint64_t adjusted = (uint64_t)((diff < 0) ? (diff + (BUS_SHARED_WRAM_SLOT_BYTES - 1)) : diff);

    uint32_t slot = (uint32_t)(adjusted >> 16);

    uint8_t *table = (uint8_t *)b->page_bits;
    return table + (size_t)slot * 4;
}

typedef uint8_t *(*fn_getter)(bus_t *, unsigned long);

void *bus_region_slot_lookup_by_time(bus_t *param_1, bus_region_t *param_2, unsigned int param_3)
{

    bus_t *bus = param_1;
    fn_getter getter = (fn_getter)param_2->read_translate;
    uint8_t *lVar2 = getter(param_1, (unsigned long)param_3);

    unsigned long mask = (unsigned long)param_3 & 0x3fffu;
    uint8_t *key = lVar2 - mask;
    uint8_t *target = bus->blank_page;
    if (key == target) {
        return NULL;
    }

    long uVar3 = lVar2 - bus->shared_wram;
    long uVar1;
    if (uVar3 < 0) {
        uVar1 = uVar3 + 0x3f;
    } else {
        uVar1 = uVar3;
    }

    unsigned long idx64 = ((unsigned long)uVar1) >> 6;
    uint32_t idx32 = (uint32_t)idx64;

    uint8_t *table = param_2->code_bits;
    return table + (long)((unsigned long)idx32 * 4u);
}

void bus_region_attach_backed_descriptors(uint8_t *machine, bus_region_t *region)
{
    bus_t *bus = (bus_t *)machine;

    region[16].read_kind = 0;
    region[16].mask = 0x7fffffu;
    region[16].read_memory = bus->slot2_rom;
    region[16].write_kind = 0;
    region[16].write_memory = bus->slot2_rom;
    region[16].page_bits = bus->slot2_page_bits;
    region[16].code_bits = bus->slot2_code_bits;
    region[16].bitmap_lookup[0] = (void *)vram_resolve_cell_ptr_coarse;
    region[16].bitmap_lookup[1] = (void *)vram_resolve_cell_ptr_fine;

    region[17].mask = 0x7fffffu;
    region[17].read_kind = 0;
    region[17].read_memory = bus->slot2_rom + 1 * BUS_REGION_SIZE;
    region[17].write_kind = 0;
    region[17].write_memory = bus->slot2_rom + 1 * BUS_REGION_SIZE;
    region[17].page_bits = bus->slot2_page_bits + 1 * BUS_REGION_PAGE_BITS;
    region[17].code_bits = bus->slot2_code_bits + 1 * BUS_REGION_CODE_BITS;
    region[17].bitmap_lookup[0] = (void *)vram_resolve_cell_ptr_coarse;
    region[17].bitmap_lookup[1] = (void *)vram_resolve_cell_ptr_fine;

    region[18].mask = 0x7fffffu;
    region[18].read_kind = 0;
    region[18].read_memory = bus->slot2_rom + 2 * BUS_REGION_SIZE;
    region[18].write_kind = 0;
    region[18].write_memory = bus->slot2_rom + 2 * BUS_REGION_SIZE;
    region[18].page_bits = bus->slot2_page_bits + 2 * BUS_REGION_PAGE_BITS;
    region[18].code_bits = bus->slot2_code_bits + 2 * BUS_REGION_CODE_BITS;
    region[18].bitmap_lookup[0] = (void *)vram_resolve_cell_ptr_coarse;
    region[18].bitmap_lookup[1] = (void *)vram_resolve_cell_ptr_fine;

    region[19].mask = 0x7fffffu;
    region[19].read_kind = 0;
    region[19].read_memory = bus->slot2_rom + 3 * BUS_REGION_SIZE;
    region[19].write_kind = 0;
    region[19].write_memory = bus->slot2_rom + 3 * BUS_REGION_SIZE;
    region[19].page_bits = bus->slot2_page_bits + 3 * BUS_REGION_PAGE_BITS;
    region[19].code_bits = bus->slot2_code_bits + 3 * BUS_REGION_CODE_BITS;
    region[19].bitmap_lookup[0] = (void *)vram_resolve_cell_ptr_coarse;
    region[19].bitmap_lookup[1] = (void *)vram_resolve_cell_ptr_fine;
}

#define FP_FIND_COARSE 0x28228UL
#define FP_FIND_FINE   0x28250UL
#define DATA_OFF       0xFBA38UL
#define SZ_DATA        0x2000000UL
#define SZ_COARSE_TABLE   0x800UL
#define SZ_FINE_TABLE   0x200000UL
#define STRIDE_DATA       0x800000UL
#define STRIDE_COARSE_TABLE  0x200UL
#define STRIDE_FINE_TABLE  0x80000UL
#define REGION_MASK  0x7fffffUL

static const unsigned long DESC_BASE[8] = {
    0xFC098UL, 0xFC0F8UL, 0xFC158UL, 0xFC1B8UL,
    0xFCC98UL, 0xFCCF8UL, 0xFCD58UL, 0xFCDB8UL
};

static void *(*s_malloc)(uint64_t);
static void  (*s_free)(void *);

static void *malloc_core(uint64_t n) {
    if (!s_malloc) s_malloc = (void *(*)(uint64_t))sym_libc_malloc;
    return s_malloc(n);
}

static void free_core(void *p) {
    if (!s_free) s_free = (void (*)(void *))sym_libc_free;
    s_free(p);
}

int bus_regions_alloc(unsigned char *ctx) {

    void *data = malloc_core(SZ_DATA);
    wr_ptr_at(ctx, DATA_OFF, data);
    wr8_at(ctx, DATA_OFF + 44, 0);
    wr32_at(ctx, DATA_OFF + 40, (uint32_t)SZ_DATA);
    if (data == 0)
        return -1;

    void *table_coarse = malloc_core(SZ_COARSE_TABLE);
    wr_ptr_at(ctx, BUS_SLOT2_PAGE_BITS_OFF, table_coarse);
    if (table_coarse == 0) {
        free_core(data);
        wr64_at(ctx, DATA_OFF, 0);

        return -1;
    }

    void *table_fine = malloc_core(SZ_FINE_TABLE);
    wr_ptr_at(ctx, BUS_SLOT2_CODE_BITS_OFF, table_fine);
    if (table_fine == 0) {
        free_core(data);

        free_core(rd_ptr_at(ctx, BUS_SLOT2_PAGE_BITS_OFF));
        wr64_at(ctx, DATA_OFF, 0);
        wr64_at(ctx, BUS_SLOT2_PAGE_BITS_OFF, 0);

        return -1;
    }

    void *fp_coarse = (void *)vram_resolve_cell_ptr_coarse;
    void *fp_fine   = (void *)vram_resolve_cell_ptr_fine;

    uint8_t *b_data = data;
    uint8_t *b_coarse  = table_coarse;
    uint8_t *b_fine  = table_fine;

    int i;
    for (i = 0; i < 8; i++) {
        unsigned long R = DESC_BASE[i];
        unsigned long s = (unsigned long)(i & 3);

        wr32_at(ctx, R + 0x00, (uint32_t)REGION_MASK);
        wr_ptr_at(ctx, R + 0x08, b_data + s * STRIDE_DATA);
        wr_ptr_at(ctx, R + 0x20, b_data + s * STRIDE_DATA);
        wr_ptr_at(ctx, R + 0x38, b_coarse  + s * STRIDE_COARSE_TABLE);
        wr_ptr_at(ctx, R + 0x40, b_fine  + s * STRIDE_FINE_TABLE);
        wr_ptr_at(ctx, R + 0x48, fp_coarse);
        wr_ptr_at(ctx, R + 0x50, fp_fine);
        wr16_at(ctx, R + 0x58, 0);
    }

    return 0;
}
#undef FP_FIND_COARSE
#undef FP_FIND_FINE
#undef DATA_OFF
#undef SZ_DATA
#undef SZ_COARSE_TABLE
#undef SZ_FINE_TABLE
#undef STRIDE_DATA
#undef STRIDE_COARSE_TABLE
#undef STRIDE_FINE_TABLE
#undef REGION_MASK



uint32_t bus_regions_init(uint8_t *machine)
{
    bus_t *bus = (bus_t *)machine;

    void *table_1[2];
    void *table_2[2];
    void *common[2];
    void *pointer_1;
    void *pointer_2;
    void *dest_common;

    {
        void *p0 = (void *)io_slot2_open_bus_read8;
        void *p1 = (void *)io_slot2_open_bus_read16;
        memcpy(&table_1[0], &p0, 8);
        memcpy(&table_1[1], &p1, 8);
    }
    pointer_1 = (void *)io_slot2_open_bus_read32;
    dest_common = (void *)vram_resolve_ptr_returns_null;
    wr_ptr(&common[0], dest_common);
    wr_ptr(&common[1], dest_common);

    bus->region[16].mask = 0x7fffffu;
    bus->region[16].read_kind = 2;
    bus->region[16].read32 = pointer_1;
    memcpy(&bus->region[16].read8, &table_1[0], 8);
    memcpy(&bus->region[16].read16, &table_1[1], 8);
    bus->region[16].write_kind = 3;
    bus->region[16].code_bits = 0;
    bus->region[16].page_bits = 0;
    memcpy(&bus->region[16].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[16].bitmap_lookup[1], &common[1], 8);

    bus->region[17].mask = 0x7fffffu;
    bus->region[17].read_kind = 2;
    bus->region[17].read32 = pointer_1;
    memcpy(&bus->region[17].read8, &table_1[0], 8);
    memcpy(&bus->region[17].read16, &table_1[1], 8);
    bus->region[17].write_kind = 3;
    bus->region[17].code_bits = 0;
    bus->region[17].page_bits = 0;
    memcpy(&bus->region[17].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[17].bitmap_lookup[1], &common[1], 8);

    bus->region[18].mask = 0x7fffffu;
    bus->region[18].read_kind = 2;
    memcpy(&bus->region[18].read8, &table_1[0], 8);
    memcpy(&bus->region[18].read16, &table_1[1], 8);
    bus->region[18].read32 = pointer_1;
    bus->region[18].write_kind = 3;
    bus->region[18].page_bits = 0;
    bus->region[18].code_bits = 0;
    memcpy(&bus->region[18].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[18].bitmap_lookup[1], &common[1], 8);

    bus->region[19].mask = 0x7fffffu;
    bus->region[19].read_kind = 2;
    bus->region[19].read32 = pointer_1;
    memcpy(&bus->region[19].read8, &table_1[0], 8);
    memcpy(&bus->region[19].read16, &table_1[1], 8);
    bus->region[19].write_kind = 3;
    bus->region[19].code_bits = 0;

    {
        void *p0 = (void *)io_slot2_serial_read8_field;
        void *p1 = (void *)cart_queue_pop_pair;
        memcpy(&table_2[0], &p0, 8);
        memcpy(&table_2[1], &p1, 8);
    }
    pointer_2 = (void *)io_slot2_serial_read32_pop_pair;

    bus->region[19].page_bits = 0;
    memcpy(&bus->region[19].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[19].bitmap_lookup[1], &common[1], 8);

    bus->region[20].mask = 0x7fffffu;
    bus->region[20].read_kind = 2;
    bus->region[20].read32 = pointer_2;
    memcpy(&bus->region[20].read8, &table_2[0], 8);
    memcpy(&bus->region[20].read16, &table_2[1], 8);
    bus->region[20].write_kind = 3;
    bus->region[20].code_bits = 0;
    bus->region[20].page_bits = 0;
    memcpy(&bus->region[20].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[20].bitmap_lookup[1], &common[1], 8);

    bus->region[21].mask = 0x7fffffu;
    bus->region[21].read_kind = 2;
    bus->region[21].read32 = pointer_2;
    memcpy(&bus->region[21].read8, &table_2[0], 8);
    memcpy(&bus->region[21].read16, &table_2[1], 8);
    bus->region[21].write_kind = 3;
    bus->region[21].code_bits = 0;
    bus->region[21].page_bits = 0;
    memcpy(&bus->region[21].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[21].bitmap_lookup[1], &common[1], 8);

    bus->region[48].mask = 0x7fffffu;
    bus->region[48].read_kind = 2;
    bus->region[48].read32 = pointer_1;
    memcpy(&bus->region[48].read8, &table_1[0], 8);
    memcpy(&bus->region[48].read16, &table_1[1], 8);
    bus->region[48].write_kind = 3;
    bus->region[48].code_bits = 0;
    bus->region[48].page_bits = 0;
    memcpy(&bus->region[48].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[48].bitmap_lookup[1], &common[1], 8);

    bus->region[49].mask = 0x7fffffu;
    bus->region[49].read_kind = 2;
    bus->region[49].read32 = pointer_1;
    memcpy(&bus->region[49].read8, &table_1[0], 8);
    memcpy(&bus->region[49].read16, &table_1[1], 8);
    bus->region[49].write_kind = 3;
    bus->region[49].code_bits = 0;
    bus->region[49].page_bits = 0;
    memcpy(&bus->region[49].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[49].bitmap_lookup[1], &common[1], 8);

    bus->region[50].mask = 0x7fffffu;
    bus->region[50].read_kind = 2;
    bus->region[50].read32 = pointer_1;
    memcpy(&bus->region[50].read8, &table_1[0], 8);
    memcpy(&bus->region[50].read16, &table_1[1], 8);
    bus->region[50].write_kind = 3;
    bus->region[50].code_bits = 0;
    bus->region[50].page_bits = 0;
    memcpy(&bus->region[50].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[50].bitmap_lookup[1], &common[1], 8);

    bus->region[51].mask = 0x7fffffu;
    bus->region[51].read_kind = 2;
    bus->region[51].read32 = pointer_1;
    memcpy(&bus->region[51].read8, &table_1[0], 8);
    memcpy(&bus->region[51].read16, &table_1[1], 8);
    bus->region[51].write_kind = 3;
    bus->region[51].code_bits = 0;
    bus->region[51].page_bits = 0;
    memcpy(&bus->region[51].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[51].bitmap_lookup[1], &common[1], 8);

    bus->region[52].mask = 0x7fffffu;
    bus->region[52].read_kind = 2;
    memcpy(&bus->region[52].read8, &table_2[0], 8);
    memcpy(&bus->region[52].read16, &table_2[1], 8);
    bus->region[52].read32 = pointer_2;
    bus->region[52].write_kind = 3;
    bus->region[52].page_bits = 0;
    bus->region[52].code_bits = 0;
    memcpy(&bus->region[52].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[52].bitmap_lookup[1], &common[1], 8);

    bus->region[53].mask = 0x7fffffu;
    bus->region[53].read_kind = 2;
    bus->region[53].read32 = pointer_2;
    memcpy(&bus->region[53].read8, &table_2[0], 8);
    memcpy(&bus->region[53].read16, &table_2[1], 8);
    bus->region[53].write_kind = 3;
    bus->region[53].code_bits = 0;
    bus->region[53].page_bits = 0;
    memcpy(&bus->region[53].bitmap_lookup[0], &common[0], 8);
    memcpy(&bus->region[53].bitmap_lookup[1], &common[1], 8);

    return 0;
}

#define N_REGISTERS   6
static void (*core_free)(void *);

static void free_core_21(void *p) {
    if (!core_free) core_free = (void (*)(void *))sym_libc_free;
    core_free(p);
}

static void write_register(bus_region_t *r,
                              void *read8, void *read16, void *read32) {
    r->mask = BUS_REGION_MASK;
    r->read8 = read8;
    r->read16 = read16;
    r->read32 = read32;
    r->page_bits = 0;
    r->code_bits = 0;
    r->bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    r->bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    r->read_kind = BUS_REGION_HANDLED;
    r->write_kind = BUS_REGION_IGNORED;
}

void bus_region_reset_and_free(bus_t *bus) {

    void *p;
    int i;

    p = bus->slot2_rom;
    if (p != NULL) free_core_21(p);
    p = bus->slot2_page_bits;
    if (p != NULL) free_core_21(p);
    p = bus->slot2_code_bits;
    if (p != NULL) free_core_21(p);

    {
        uint32_t z32 = 0;
        memcpy(bus->unmapped_2, &z32, 4);
        bus->slot2_rom = 0;
        bus->slot2_page_bits = 0;
        bus->slot2_code_bits = 0;
    }

    for (i = 0; i < N_REGISTERS; i++) {
        write_register(&bus->region[BUS_SLOT2_REGION + i], (void *)io_exmemcnt_gba_slot_arm9_mask_byte,
                       (void *)io_exmemcnt_gba_slot_arm9_mask, (void *)io_exmemcnt_gba_slot_arm9_mask_word);
        write_register(&bus->region[BUS_ARM9_REGIONS + BUS_SLOT2_REGION + i], (void *)io_exmemcnt_gba_slot_arm7_mask,
                       (void *)io_exmemcnt_gba_slot_arm7_mask_clone, (void *)io_exmemcnt_gba_slot_arm7_mask_clone2);
    }
}
#undef N_REGISTERS







void bus_region_init_queue_descriptors(void *param_1) {
    bus_t *bus = (bus_t *)param_1;

    uint8_t *main_ram = bus->main_ram;

    void *t0_lo = (void *)io9_read8;
    void *t0_hi = (void *)io9_read16;
    void *t0_x = (void *)io9_read32;
    void *t1_lo = (void *)io9_write8;
    void *t1_hi = (void *)io9_write16;
    void *t1_x = (void *)io9_write32;

    bus->region[0].mask = 0x1ffffu;
    bus->region[0].page_bits = 0;
    bus->region[0].code_bits = 0;
    bus->region[0].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[0].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[0].read_kind = 3;
    bus->region[0].write_kind = 3;
    bus->region[1].mask = 0x1ffffu;
    bus->region[1].page_bits = 0;
    bus->region[1].code_bits = 0;
    bus->region[1].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[1].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[1].read_kind = 3;
    bus->region[1].write_kind = 3;
    bus->region[2].mask = 0x1ffffu;
    bus->region[2].page_bits = 0;
    bus->region[2].code_bits = 0;
    bus->region[2].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[2].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[2].read_kind = 3;
    bus->region[2].write_kind = 3;
    bus->region[3].mask = 0x1ffffu;
    bus->region[3].page_bits = 0;
    bus->region[3].code_bits = 0;
    bus->region[3].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[3].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[3].read_kind = 3;
    bus->region[3].write_kind = 3;

    bus->region[4].mask = 0x3fffffu;

    bus->region[4].read_memory = main_ram;
    bus->region[4].write_memory = main_ram;
    bus->region[4].page_bits = bus->main_ram_page_bits;
    bus->region[4].code_bits = bus->main_ram_code_bits;
    bus->region[4].bitmap_lookup[0] = (void *)vram_resolve_cell_ptr_coarse;
    bus->region[4].bitmap_lookup[1] = (void *)vram_resolve_cell_ptr_fine;
    bus->region[4].read_kind = 0;
    bus->region[4].write_kind = 0;
    bus->region[5].mask = 0x3fffffu;
    bus->region[5].read_memory = main_ram;
    bus->region[5].write_memory = main_ram;
    bus->region[5].page_bits = bus->main_ram_page_bits;
    bus->region[5].code_bits = bus->main_ram_code_bits;
    bus->region[5].bitmap_lookup[0] = (void *)vram_resolve_cell_ptr_coarse;
    bus->region[5].bitmap_lookup[1] = (void *)vram_resolve_cell_ptr_fine;
    bus->region[5].read_kind = 0;
    bus->region[5].write_kind = 0;
    bus->region[6].mask = 0x3fffu;
    bus->region[6].read8 = (void *)vram_resolve_ptr_bank14_second_pair;
    bus->region[6].write8 = (void *)vram_resolve_ptr_bank14_second_pair;
    bus->region[6].page_bits = &bus->shared_wram_page_bits;
    bus->region[6].code_bits = bus->shared_wram_code_bits;
    bus->region[6].bitmap_lookup[0] = (void *)bus_region_page_lookup;
    bus->region[6].bitmap_lookup[1] = (void *)bus_region_slot_lookup_by_time;

    bus->region[6].read_kind = 0x1;
    bus->region[6].write_kind = 0x1;
    bus->region[7].mask = 0x3fffu;

    bus->region[7].read8 = (void *)vram_resolve_ptr_bank14_second_pair;
    bus->region[7].write8 = (void *)vram_resolve_ptr_bank14_second_pair;
    bus->region[7].page_bits = &bus->shared_wram_page_bits;
    bus->region[7].code_bits = bus->shared_wram_code_bits;
    bus->region[7].bitmap_lookup[0] = (void *)bus_region_page_lookup;
    bus->region[7].bitmap_lookup[1] = (void *)bus_region_slot_lookup_by_time;
    bus->region[7].read_kind = 1;
    bus->region[7].write_kind = 1;
    bus->region[8].mask = 0x7fffffu;

    bus->region[8].read8 = t0_lo;
    bus->region[8].read16 = t0_hi;
    bus->region[8].read32 = t0_x;

    bus->region[8].write8 = t1_lo;
    bus->region[8].write16 = t1_hi;
    bus->region[8].write32 = t1_x;
    bus->region[8].page_bits = 0;
    bus->region[8].code_bits = 0;
    bus->region[8].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[8].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[8].read_kind = 0x2;
    bus->region[8].write_kind = 0x2;
    bus->region[9].mask = 0x7fffffu;
    bus->region[9].read8 = t0_lo;
    bus->region[9].read16 = t0_hi;
    bus->region[9].read32 = t0_x;
    bus->region[9].write8 = t1_lo;
    bus->region[9].write16 = t1_hi;
    bus->region[9].write32 = t1_x;
    bus->region[9].page_bits = 0;
    bus->region[9].code_bits = 0;
    bus->region[9].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[9].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[9].read_kind = 0x2;
    bus->region[9].write_kind = 0x2;

    bus->region[12].mask = 0x3fffu;
    bus->region[12].read8 = (void *)vram_resolve_ptr_region10;
    bus->region[12].write8 = (void *)vram_resolve_ptr_bus_tagged;
    bus->region[12].page_bits = bus->vram_page_bits;
    bus->region[12].code_bits = bus->vram_code_bits;
    bus->region[12].bitmap_lookup[0] = (void *)vram_resolve_slot_page64k;
    bus->region[12].bitmap_lookup[1] = (void *)vram_resolve_slot64;
    bus->region[12].read_kind = 1;
    bus->region[12].write_kind = 1;
    bus->region[13].mask = 0x3fffu;
    bus->region[13].read8 = (void *)vram_resolve_ptr_region10;
    bus->region[13].write8 = (void *)vram_resolve_ptr_bus_tagged;
    bus->region[13].page_bits = bus->vram_page_bits;
    bus->region[13].code_bits = bus->vram_code_bits;
    bus->region[13].bitmap_lookup[0] = (void *)vram_resolve_slot_page64k;
    bus->region[13].bitmap_lookup[1] = (void *)vram_resolve_slot64;
    bus->region[13].read_kind = 1;
    bus->region[13].write_kind = 1;

    bus->region[22].mask = 0x1ffffu;
    bus->region[22].page_bits = 0;
    bus->region[22].code_bits = 0;
    bus->region[22].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[22].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[22].read_kind = 3;
    bus->region[22].write_kind = 3;
    bus->region[23].mask = 0x1ffffu;
    bus->region[23].page_bits = 0;
    bus->region[23].code_bits = 0;
    bus->region[23].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[23].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[23].read_kind = 3;
    bus->region[23].write_kind = 3;
    bus->region[24].mask = 0x1ffffu;
    bus->region[24].page_bits = 0;
    bus->region[24].code_bits = 0;
    bus->region[24].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[24].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[24].read_kind = 3;
    bus->region[24].write_kind = 3;
    bus->region[25].mask = 0x1ffffu;
    bus->region[25].page_bits = 0;
    bus->region[25].code_bits = 0;
    bus->region[25].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[25].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[25].read_kind = 3;
    bus->region[25].write_kind = 3;
    bus->region[26].mask = 0x1ffffu;
    bus->region[26].page_bits = 0;
    bus->region[26].code_bits = 0;
    bus->region[26].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[26].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[26].read_kind = 3;
    bus->region[26].write_kind = 3;
    bus->region[27].mask = 0x1ffffu;
    bus->region[27].page_bits = 0;
    bus->region[27].code_bits = 0;
    bus->region[27].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[27].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[27].read_kind = 3;
    bus->region[27].write_kind = 3;
    bus->region[28].mask = 0x1ffffu;
    bus->region[28].page_bits = 0;
    bus->region[28].code_bits = 0;
    bus->region[28].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[28].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[28].read_kind = 3;
    bus->region[28].write_kind = 3;
    bus->region[29].mask = 0x1ffffu;
    bus->region[29].page_bits = 0;
    bus->region[29].code_bits = 0;
    bus->region[29].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[29].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[29].read_kind = 3;
    bus->region[29].write_kind = 3;
    bus->region[30].mask = 0x1ffffu;
    bus->region[30].page_bits = 0;
    bus->region[30].code_bits = 0;

    bus->region[30].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[30].bitmap_lookup[1] = vram_resolve_ptr_returns_null;
    bus->region[30].read_kind = 3;
    bus->region[30].write_kind = 3;
    bus->region[31].mask = 0x1ffffu;
    bus->region[31].page_bits = 0;
    bus->region[31].code_bits = 0;
    bus->region[31].bitmap_lookup[0] = vram_resolve_ptr_returns_null;
    bus->region[31].bitmap_lookup[1] = vram_resolve_ptr_returns_null;

    bus->region[31].read_kind = 3;
    bus->region[31].write_kind = 3;
}







void *bus_region_init_channel_descriptors(bus_t *bus)
{

    void *fp_a = (void *)vram_resolve_cell_ptr_coarse;
    void *fp_b = (void *)vram_resolve_cell_ptr_fine;
    void *fp_c = (void *)bus_region_page_lookup;
    void *fp_d = (void *)vram_resolve_ptr_returns_null;
    void *fp_e = (void *)bus_region_slot_lookup_by_time;
    void *fp_f = (void *)vram_resolve_ptr_bank14_first_pair;
    void *fp_g = (void *)vram_resolve_slot_page64k;
    void *fp_h = (void *)vram_resolve_slot64;
    void *fp_i = (void *)vram_resolve_ptr_bank17;
    void *fp_j = (void *)vram_resolve_ptr_bank17_diverted;

    uint8_t *hdr = bus->main_ram;
    uint8_t *self_20 = bus->arm7_wram;

    bus->region[32].mask = 0x3fffu;
    bus->region[32].read_memory = bus->bios7;
    bus->region[32].read_kind = 0;
    bus->region[32].write_kind = 3;
    bus->region[32].page_bits = 0;
    bus->region[32].code_bits = 0;
    bus->region[32].bitmap_lookup[0] = fp_d;
    bus->region[32].bitmap_lookup[1] = fp_d;
    bus->region[33].mask = 0x1ffffu;

    bus->region[33].read_kind = 3;
    bus->region[33].write_kind = 3;
    bus->region[33].page_bits = 0;
    bus->region[33].code_bits = 0;
    bus->region[33].bitmap_lookup[0] = fp_d;
    bus->region[33].bitmap_lookup[1] = fp_d;
    bus->region[34].mask = 0x1ffffu;

    bus->region[34].read_kind = 3;
    bus->region[34].write_kind = 3;
    bus->region[34].page_bits = 0;
    bus->region[34].code_bits = 0;
    bus->region[34].bitmap_lookup[0] = fp_d;
    bus->region[34].bitmap_lookup[1] = fp_d;
    bus->region[35].mask = 0x1ffffu;

    bus->region[35].read_kind = 3;
    bus->region[35].write_kind = 3;
    bus->region[35].page_bits = 0;
    bus->region[35].code_bits = 0;
    bus->region[35].bitmap_lookup[0] = fp_d;
    bus->region[35].bitmap_lookup[1] = fp_d;
    bus->region[36].mask = 0x3fffffu;

    bus->region[40].read8 = (void *)io7_read8;
    bus->region[40].read16 = (void *)io7_read16;
    bus->region[40].read32 = (void *)io7_read32;
    bus->region[40].write8 = (void *)io7_write8;
    bus->region[40].write16 = (void *)io7_write16;
    bus->region[40].write32 = (void *)io7_write32;
    bus->region[40].page_bits = 0;
    bus->region[40].code_bits = 0;
    bus->region[41].read8 = (void *)io_reg_read_returns_zero;
    bus->region[41].read16 = (void *)io_read_port16;
    bus->region[41].read32 = (void *)io_read32_unmapped_zero;
    bus->region[41].write8 = (void *)io9_write32_noop_tail;
    bus->region[41].write16 = (void *)io_write_port16;
    bus->region[41].write32 = (void *)io7_write8_noop_tail;
    bus->region[41].page_bits = 0;
    bus->region[41].code_bits = 0;

    bus->region[42].page_bits = 0;
    bus->region[42].code_bits = 0;
    bus->region[43].page_bits = 0;
    bus->region[43].code_bits = 0;
    bus->region[46].page_bits = 0;
    bus->region[46].code_bits = 0;
    bus->region[47].page_bits = 0;
    bus->region[47].code_bits = 0;
    bus->region[54].page_bits = 0;
    bus->region[54].code_bits = 0;
    bus->region[55].page_bits = 0;
    bus->region[55].code_bits = 0;
    bus->region[56].page_bits = 0;
    bus->region[56].code_bits = 0;
    bus->region[57].page_bits = 0;
    bus->region[57].code_bits = 0;
    bus->region[58].page_bits = 0;
    bus->region[58].code_bits = 0;
    bus->region[59].page_bits = 0;
    bus->region[59].code_bits = 0;
    bus->region[60].page_bits = 0;
    bus->region[60].code_bits = 0;
    bus->region[61].page_bits = 0;
    bus->region[61].code_bits = 0;
    bus->region[62].page_bits = 0;
    bus->region[62].code_bits = 0;
    bus->region[63].page_bits = 0;
    bus->region[63].code_bits = 0;

    bus->region[36].read_kind = 0;
    bus->region[36].write_kind = 0;
    bus->region[37].mask = 0x3fffffu;
    bus->region[36].page_bits = bus->main_ram_code_bits;
    bus->region[36].code_bits = bus->main_ram_page_bits;
    bus->region[37].page_bits = bus->main_ram_code_bits;
    bus->region[37].code_bits = bus->main_ram_page_bits;
    bus->region[36].read8 = hdr;
    bus->region[36].write8 = hdr;
    bus->region[37].read8 = hdr;
    bus->region[37].write8 = hdr;
    bus->region[36].bitmap_lookup[0] = fp_a;
    bus->region[36].bitmap_lookup[1] = fp_b;
    bus->region[37].read_kind = 0;
    bus->region[37].write_kind = 0;
    bus->region[37].bitmap_lookup[0] = fp_a;
    bus->region[37].bitmap_lookup[1] = fp_b;
    bus->region[38].mask = 0x3fffu;
    bus->region[38].read_kind = 1;
    bus->region[38].read8 = fp_f;
    bus->region[38].write_kind = 1;
    bus->region[38].page_bits = &bus->shared_wram_page_bits;
    bus->region[38].code_bits = bus->shared_wram_code_bits;
    bus->region[39].mask = 0xffffu;
    bus->region[38].write8 = fp_f;
    bus->region[38].bitmap_lookup[0] = fp_c;
    bus->region[38].bitmap_lookup[1] = fp_e;

    bus->region[39].read8 = self_20;
    bus->region[39].write8 = self_20;
    bus->region[39].page_bits = &bus->arm7_wram_page_bits;
    bus->region[39].code_bits = bus->arm7_wram_code_bits;
    bus->region[39].bitmap_lookup[0] = fp_a;
    bus->region[39].bitmap_lookup[1] = fp_b;
    bus->region[39].read_kind = 0;
    bus->region[39].write_kind = 0;
    bus->region[40].mask = 0x7fffffu;
    bus->region[40].read_kind = 2;
    bus->region[40].write_kind = 2;
    bus->region[41].mask = 0x7fffffu;
    bus->region[40].bitmap_lookup[0] = fp_d;
    bus->region[40].bitmap_lookup[1] = fp_d;
    bus->region[41].read_kind = 2;
    bus->region[41].write_kind = 2;
    bus->region[41].bitmap_lookup[0] = fp_d;
    bus->region[41].bitmap_lookup[1] = fp_d;
    bus->region[42].mask = 0x1ffffu;
    bus->region[42].read_kind = 3;
    bus->region[42].write_kind = 3;
    bus->region[42].bitmap_lookup[0] = fp_d;
    bus->region[42].bitmap_lookup[1] = fp_d;
    bus->region[43].mask = 0x1ffffu;
    bus->region[43].read_kind = 3;
    bus->region[43].write_kind = 3;
    bus->region[43].bitmap_lookup[0] = fp_d;
    bus->region[43].bitmap_lookup[1] = fp_d;
    bus->region[44].mask = 0x3fffu;

    bus->region[44].read_kind = 1;
    bus->region[44].read8 = fp_i;
    bus->region[44].write_kind = 1;
    bus->region[44].write8 = fp_j;
    bus->region[44].page_bits = bus->vram_page_bits;
    bus->region[44].code_bits = bus->vram_code_bits;
    bus->region[44].bitmap_lookup[0] = fp_g;
    bus->region[44].bitmap_lookup[1] = fp_h;
    bus->region[45].mask = 0x3fffu;
    bus->region[45].read_kind = 1;
    bus->region[45].write_kind = 1;
    bus->region[45].read8 = fp_i;
    bus->region[45].write8 = fp_j;
    bus->region[45].page_bits = bus->vram_page_bits;
    bus->region[45].code_bits = bus->vram_code_bits;
    bus->region[45].bitmap_lookup[0] = fp_g;
    bus->region[45].bitmap_lookup[1] = fp_h;
    bus->region[46].mask = 0x1ffffu;
    bus->region[46].read_kind = 3;
    bus->region[46].write_kind = 3;
    bus->region[46].bitmap_lookup[0] = fp_d;
    bus->region[46].bitmap_lookup[1] = fp_d;
    bus->region[47].mask = 0x1ffffu;
    bus->region[47].read_kind = 3;
    bus->region[47].write_kind = 3;
    bus->region[47].bitmap_lookup[0] = fp_d;
    bus->region[47].bitmap_lookup[1] = fp_d;
    bus->region[54].mask = 0x1ffffu;
    bus->region[54].read_kind = 3;
    bus->region[54].write_kind = 3;
    bus->region[54].bitmap_lookup[0] = fp_d;
    bus->region[54].bitmap_lookup[1] = fp_d;
    bus->region[55].mask = 0x1ffffu;
    bus->region[55].read_kind = 3;
    bus->region[55].write_kind = 3;
    bus->region[55].bitmap_lookup[0] = fp_d;
    bus->region[55].bitmap_lookup[1] = fp_d;
    bus->region[56].mask = 0x1ffffu;
    bus->region[56].read_kind = 3;
    bus->region[56].write_kind = 3;
    bus->region[56].bitmap_lookup[0] = fp_d;
    bus->region[56].bitmap_lookup[1] = fp_d;
    bus->region[57].mask = 0x1ffffu;
    bus->region[57].read_kind = 3;
    bus->region[57].write_kind = 3;
    bus->region[57].bitmap_lookup[0] = fp_d;
    bus->region[57].bitmap_lookup[1] = fp_d;
    bus->region[58].mask = 0x1ffffu;
    bus->region[58].read_kind = 3;
    bus->region[58].write_kind = 3;
    bus->region[58].bitmap_lookup[0] = fp_d;
    bus->region[58].bitmap_lookup[1] = fp_d;
    bus->region[59].mask = 0x1ffffu;
    bus->region[59].read_kind = 3;
    bus->region[59].write_kind = 3;
    bus->region[59].bitmap_lookup[0] = fp_d;
    bus->region[59].bitmap_lookup[1] = fp_d;
    bus->region[60].mask = 0x1ffffu;
    bus->region[60].read_kind = 3;
    bus->region[60].write_kind = 3;
    bus->region[60].bitmap_lookup[0] = fp_d;
    bus->region[60].bitmap_lookup[1] = fp_d;
    bus->region[61].mask = 0x1ffffu;
    bus->region[61].read_kind = 3;
    bus->region[61].write_kind = 3;
    bus->region[61].bitmap_lookup[0] = fp_d;
    bus->region[61].bitmap_lookup[1] = fp_d;
    bus->region[62].mask = 0x1ffffu;
    bus->region[62].read_kind = 3;
    bus->region[62].write_kind = 3;
    bus->region[62].bitmap_lookup[0] = fp_d;
    bus->region[62].bitmap_lookup[1] = fp_d;
    bus->region[63].mask = 0x1ffffu;
    bus->region[63].read_kind = 3;
    bus->region[63].write_kind = 3;
    bus->region[63].bitmap_lookup[0] = fp_d;
    bus->region[63].bitmap_lookup[1] = fp_d;

    return bus->vram_code_bits;
}

extern void pagetable_unmap_range(unsigned char *t, uint32_t dir, uint32_t len);

static void block96(bus_region_t *dst,
                      void *t1_0, void *t1_1, void *t1_2,
                      void *t2_0, void *t2_1, void *t2_2,
                      void *dup_ptr)
{
    dst->mask = 0x7ff;
    dst->read8 = t1_0;
    dst->read16 = t1_1;
    dst->read32 = t1_2;
    dst->write8 = t2_0;
    dst->write16 = t2_1;
    dst->write32 = t2_2;
    dst->page_bits = 0;
    dst->code_bits = 0;
    dst->bitmap_lookup[0] = dup_ptr;
    dst->bitmap_lookup[1] = dup_ptr;
    dst->read_kind = BUS_REGION_HANDLED;
    dst->write_kind = BUS_REGION_HANDLED;
}

void bus_access_tables_init(bus_t *bus)
{

    void *a0 = (void *)vram_reg_cache_read8_init, *a1 = (void *)vram_write_cache_read16_lazy, *a2 = (void *)vram_reg_cache_read32_init;
    void *b0 = (void *)vram_reg_cache_write8_init, *b1 = (void *)vram_reg_cache_write16_init, *b2 = (void *)vram_reg_cache_write32_init;
    void *c0 = (void *)io_reg_cache_read8_init, *c1 = (void *)io_reg_cache_read16_init, *c2 = (void *)io_bank15870_read32_lazy_init;
    void *d0 = (void *)io_reg_cache_write8_init, *d1 = (void *)io_reg_cache_write16_init, *d2 = (void *)io_reg_cache_write32_init;
    void *dup_ptr = (void *)vram_resolve_ptr_returns_null;

    block96(&bus->region[10], a0, a1, a2, b0, b1, b2, dup_ptr);
    block96(&bus->region[11], a0, a1, a2, b0, b1, b2, dup_ptr);
    block96(&bus->region[14], c0, c1, c2, d0, d1, d2, dup_ptr);
    block96(&bus->region[15], c0, c1, c2, d0, d1, d2, dup_ptr);

    {
        pagetable_t *x19 = bus->arm9_pagetable;
        bus_t *base = x19->bus;

        uint32_t w21 = base->dtcm_start;
        uint32_t arg1 = 0x5000000u;
        uint32_t arg2 = 0x1000000u;

        if (w21 <= 0x5000000u) {
            uint32_t w22 = base->dtcm_end;
            if (w22 > 0x6000000u) {

                pagetable_unmap_range((unsigned char *)x19, 0x5000000u, w21 - 0x5000000u);
                arg1 = w21;
                arg2 = 0x6000000u - w22;
            }
        }

        pagetable_unmap_range((unsigned char *)x19, arg1, arg2);
    }

    {
        pagetable_t *x19 = bus->arm9_pagetable;
        bus_t *base = x19->bus;

        uint32_t w20 = base->dtcm_start;
        uint32_t arg1 = 0x7000000u;
        uint32_t arg2 = 0x1000000u;

        if (w20 <= 0x7000000u) {
            uint32_t w21 = base->dtcm_end;
            if (w21 > 0x8000000u) {

                pagetable_unmap_range((unsigned char *)x19, 0x7000000u, w20 - 0x7000000u);
                arg1 = w20;
                arg2 = 0x8000000u - w21;
            }
        }

        pagetable_unmap_range((unsigned char *)x19, arg1, arg2);
    }
}

#define TEMPLATE   0x60
#define D1      0xfbe58
#define D3      0xfbfd8

static void descriptor(uint8_t *d, uint8_t *base) {
    uint32_t z32 = 0x7ff;
    uint64_t z64 = 0;
    uint16_t z16 = 0;
    void *pb = base;
    void *f1 = (void *)vram_resolve_cell_ptr_coarse;
    void *f2 = (void *)vram_resolve_cell_ptr_fine;
    memcpy(d + 0x00, &z32, 4);
    memcpy(d + 0x08, &pb, 8);
    memcpy(d + 0x20, &pb, 8);
    memcpy(d + 0x38, &z64, 8);
    memcpy(d + 0x40, &z64, 8);
    memcpy(d + 0x48, &f1, 8);
    memcpy(d + 0x50, &f2, 8);
    memcpy(d + 0x58, &z16, 2);
}

static void adjust(uint8_t *ctx, uint32_t limit) {
    pagetable_t *obj = ((bus_t *)ctx)->arm9_pagetable;
    bus_t *p = obj->bus;

    uint32_t cap = limit + BUS_UNMAP_SPAN_BYTES;
    uint32_t arg1 = limit, arg2 = BUS_UNMAP_SPAN_BYTES;

    uint32_t a = p->dtcm_start;
    if (a <= limit) {
        uint32_t b = p->dtcm_end;
        if (b > cap) {

            pagetable_unmap_range((unsigned char *)obj, limit, a - limit);
            arg1 = a;
            arg2 = cap - b;
        }
    }
    pagetable_unmap_range((unsigned char *)obj, arg1, arg2);
}

void bus_region_init_mirror_pair_and_flush(uint8_t *ctx) {

    descriptor(ctx + D1,             ((bus_t *)ctx)->palette);
    descriptor(ctx + D1 + TEMPLATE,     ((bus_t *)ctx)->palette);
    descriptor(ctx + D3,             ((bus_t *)ctx)->oam);
    descriptor(ctx + D3 + TEMPLATE,     ((bus_t *)ctx)->oam);
    adjust(ctx, 0x5000000u);
    adjust(ctx, 0x7000000u);
}
#undef TEMPLATE
#undef D1
#undef D3




#define REC_SIZE       96
#define REC_OFF_MASK  0
#define REC_OFF_H1    72
#define REC_OFF_H2    80
typedef void *(*fn_handler_t)(void *core, void *rec, uint32_t address);
typedef void  *(*fn_memset_t)(void *, int, size_t);






static void mark_range_bits(unsigned char *bitmap,
                              uint32_t n_words,
                              uint32_t start_bit, uint32_t end_bit)
{
    if (n_words == 0) {
        uint32_t mask = (0xFFFFFFFFu << start_bit) & ~(0xFFFFFFFEu << end_bit);
        wr32(bitmap, rd32(bitmap) | mask);
        return;
    }

    unsigned char *head = bitmap;
    wr32(head, rd32(head) | (0xFFFFFFFFu << start_bit));

    uint32_t nmid = n_words - 1;
    if (nmid > 0) {
        ((fn_memset_t)sym_libc_memset)(head + 4, 0xff, (size_t)nmid * 4);
    }

    unsigned char *tail = bitmap + (size_t)n_words * 4;
    wr32(tail, rd32(tail) | ~(0xFFFFFFFEu << end_bit));
}

void bus_range_mark_written(unsigned char *ctx, uint32_t address, uint32_t size)
{

    unsigned char *core_ptr = (unsigned char *)((arm_t *)ctx)->pagetable.bus;

    uint32_t end_addr   = address + size;
    uint32_t start_page = address >> 11;
    uint32_t end_page   = (end_addr - 1) >> 11;

    if (start_page <= end_page) {
        uint64_t count = (uint64_t)(end_page - start_page) + 1;
        unsigned char *entry = (unsigned char *)&((arm_t *)ctx)->pagetable.page[start_page];
        while (count != 0) {
            uint64_t v = rd64(entry);
            if ((v & PAGETABLE_MISSING) == 0) {
                wr64(entry, v | 0xC000000000000000ULL);
            }
            entry += 8;
            count--;
        }
    }

    if ((address >> 28) != 0) {
        return;
    }

    unsigned char *core = core_ptr;

    unsigned char *regions = (unsigned char *)((arm_t *)ctx)->pagetable.region;
    uint32_t region_idx = address >> 23;
    unsigned char *rec = regions + (size_t)region_idx * REC_SIZE;

    fn_handler_t h1 = (fn_handler_t)rd_ptr(rec + REC_OFF_H1);
    void *bitmap1 = h1(core, rec, address);
    fn_handler_t h2 = (fn_handler_t)rd_ptr(rec + REC_OFF_H2);
    void *bitmap2 = h2(core, rec, address);

    uint32_t mode = ((arm_t *)ctx)->is_arm9;
    uint32_t mask = rd32(rec + REC_OFF_MASK);

    if (mode == 1) {
        uint32_t limit = ((bus_t *)core)->itcm_size;
        if (limit > address) {
            mask = 0x7fff;
            bitmap1 = (unsigned char *)&((bus_t *)core)->itcm_page_bits;
            uint32_t idx9 = (address >> 6) & 0x1ff;
            bitmap2 = (unsigned char *)&((bus_t *)core)->itcm_code_bits[idx9];
        }
    }

    uint32_t local   = mask & address;
    uint32_t bank_sz = mask + 1;
    uint32_t sz      = size;

    if (local + sz > bank_sz) {
        uint32_t next_addr = end_addr & ~mask;
        uint32_t overflow  = (local + sz) - bank_sz;
        bus_range_mark_written(ctx, next_addr, overflow);
        sz = bank_sz - local;
    }

    if (bitmap1 == NULL) {
        return;
    }

    uint32_t local_end = local + sz - 1;

    mark_range_bits((unsigned char *)bitmap1,
                      (local_end >> 16) - (local >> 16),
                      (local >> 11) & 31, (local_end >> 11) & 31);

    mark_range_bits((unsigned char *)bitmap2,
                      (local_end >> 6) - (local >> 6),
                      (address >> 1) & 31, (local_end >> 1) & 31);
}
#undef REC_SIZE
#undef REC_OFF_MASK
#undef REC_OFF_H1
#undef REC_OFF_H2

void bus_map_reset(unsigned char *machine) {

    void (*map)(vram_map_t *, uint8_t *, uint32_t, uint32_t, uint32_t) = vram_vramcnt_bank_remap;
    void (*c_free)(void *) = (void (*)(void *))sym_libc_free;

    vram_map_t *vram = (vram_map_t *)machine;
    bus_t *sub = vram->bus;
    gpu_output_t *res = GPU_OUTPUT_OF(machine);

    for (uint64_t i = 0; i < 9; i++) {
        vram->bank[i].map_kind = 11;
        uint32_t type = *vram->bank_control_reg[i];
        if (vram->bank[i].control == type) continue;

        vram->changed_banks = 0;
        map(vram, vram->bank_data[i], (uint32_t)i, type, 1);

        uint32_t m = vram->changed_banks;
        if (m == 0) continue;

        uint32_t bit = 0;
        for (;;) {
            if (m & 1) {
                vram->bank[bit].control = 0xffffffffu;
                map(vram, vram->bank_data[bit], bit, *vram->bank_control_reg[bit], 0);
            }
            m >>= 1;
            bit++;
            if (m == 0) break;
        }
    }

    {
        unsigned char *base = sub->unmapped_page;
        for (uint64_t i = 0; i < 0x400; i++) {
            vram->slot_base[i] = base - i * 0x4000;
            vram->slot_banks[i] = 0;
        }
    }

    unsigned char *cap = sub->blank_page;

    vram->slot_touched_groups = 0;
    memset(vram->slot_touched, 0, sizeof vram->slot_touched);
    memset(vram->bg_ext_palette, 0, sizeof vram->bg_ext_palette);
    memset(vram->obj_ext_palette, 0, sizeof vram->obj_ext_palette);
    memset(vram->texture, 0, sizeof vram->texture);
    memset(vram->texture_palette, 0, sizeof vram->texture_palette);
    memset(vram->bg_ext_palette_banks, 0, sizeof vram->bg_ext_palette_banks);
    memset(vram->obj_ext_palette_banks, 0, sizeof vram->obj_ext_palette_banks);
    memset(vram->texture_banks, 0, sizeof vram->texture_banks);
    memset(vram->texture_palette_banks, 0, sizeof vram->texture_palette_banks);
    memset(vram->arm7_vram_banks, 0, sizeof vram->arm7_vram_banks);
    vram->arm7_vram[0] = cap;
    vram->arm7_vram[1] = cap;
    void *p0 = res->capture_shadow[0];
    res->capture.lines_done = 0;
    if (p0) { c_free(p0); res->capture_shadow[0] = 0; }
    for (int k = 1; k < 4; k++) {
        void *p = res->capture_shadow[k];
        res->bank_texture_bits[k - 1] = 0;
        if (p) { c_free(p); res->capture_shadow[k] = 0; }
    }
    res->bank_texture_bits[3] = 0;

    gpu2d_engine_init_substructures(&((gpu_t *)machine)->engine[0]);
    gpu2d_engine_init_substructures(&((gpu_t *)machine)->engine[1]);
    gpu3d_raster_texture_cache_reset(&((gpu_t *)machine)->texture_cache);
    gpu3d_context_init(GPU3D_OF(machine));

    gpu3d_raster_double_buffer_init(machine);
}



#undef F

#undef F

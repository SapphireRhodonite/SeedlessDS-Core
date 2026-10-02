#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "memory/io_common.h"
#include "cart/cart.h"
#include "core/nds_state.h"
#include "cart.h"
#include <stddef.h>
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"

static void *const slot2_serial_read_idle[3] = {
    (void *)io_slot2_serial_read8_idle, (void *)io_slot2_serial_read16_idle,
    (void *)io_slot2_serial_read32_idle,
};
static void *const slot2_serial_read_pop[3] = {
    (void *)io_slot2_serial_read8_pop, (void *)io_slot2_serial_read16_pop,
    (void *)io_slot2_serial_read32_pop,
};


#define IDX_MASK     0x1ffu
#define PAGE_STRIDE     0x800000u
#define OFF_READ_TABLE   0x133a78u
#define OFF_WRITE_TABLE   0x133a90u
#define OFF_NULL_FN    0x28220u


static void entry_rom(bus_region_t *e, uint8_t *page, void *null)
{
    e->read_memory = page;
    e->mask = BUS_REGION_MASK;
    e->read_kind = BUS_REGION_DIRECT;
    e->write_kind = BUS_REGION_IGNORED;
    e->page_bits = 0;
    e->code_bits = 0;
    e->bitmap_lookup[0] = null;
    e->bitmap_lookup[1] = null;
}

static void set_fixed_entry(bus_region_t *e, void *const *read_fns,
                         void *const *wr, void *null)
{
    e->mask = BUS_REGION_MASK;
    e->read_kind = BUS_REGION_HANDLED;
    e->read32 = read_fns[2];
    e->read8 = read_fns[0];
    e->read16 = read_fns[1];
    e->write_kind = BUS_REGION_HANDLED;
    e->write32 = wr[2];
    e->write8 = wr[0];
    e->write16 = wr[1];
    e->code_bits = 0;
    e->page_bits = 0;
    e->bitmap_lookup[0] = null;
    e->bitmap_lookup[1] = null;
}

uint32_t cart_slot2_map(unsigned char *ctx, uint8_t *rom, uint32_t size)
{
    bus_t *bus = (bus_t *)ctx;

    uint64_t n = (uint64_t)(size >> 23);

    void *const null = (void *)vram_resolve_ptr_returns_null;

    if (n != 0) {
        uint8_t *page = rom;
        for (uint64_t i = 0; i < n; i++) {
            uint64_t idx = ((uint32_t)i + BUS_SLOT2_REGION) & IDX_MASK;
            bus_region_t *e = &bus->region[idx];
            entry_rom(e, page, null);
            page += PAGE_STRIDE;
        }
    }

    void *const read_fns[3] = { (void *)io_slot2_backup_read8,
                              (void *)io_slot2_backup_read16_zero,
                              (void *)io_slot2_backup_read32_zero };
    void *const wr[3] = { (void *)io_slot2_backup_write8,
                              (void *)io_slot2_backup_write16_ignore,
                              (void *)io_slot2_backup_write32_ignore };

    set_fixed_entry(&bus->region[20], read_fns, wr, null);
    set_fixed_entry(&bus->region[21], read_fns, wr, null);

    if (n != 0) {
        uint8_t *page = rom;
        for (uint64_t i = 0; i < n; i++) {
            uint64_t idx = ((uint32_t)i + BUS_SLOT2_REGION) & IDX_MASK;
            bus_region_t *e = &bus->region[BUS_ARM9_REGIONS + idx];
            entry_rom(e, page, null);
            page += PAGE_STRIDE;
        }
    }

    set_fixed_entry(&bus->region[52], read_fns, wr, null);
    set_fixed_entry(&bus->region[53], read_fns, wr, null);

    return 0;
}
#undef IDX_MASK
#undef PAGE_STRIDE
#undef OFF_READ_TABLE
#undef OFF_WRITE_TABLE
#undef OFF_NULL_FN

#define FP_340 0x26340UL
#define FP_350 0x26350UL
#define FP_358 0x26358UL
#define FP_360 0x26360UL
#define FP_378 0x26378UL
#define FP_390 0x26390UL
#define FP_220 0x28220UL
#define REC_STRIDE 0x60UL

static const unsigned long REC_BASE[8] = {
    0xFC098UL, 0xFC0F8UL, 0xFC158UL, 0xFC1B8UL,
    0xFCC98UL, 0xFCCF8UL, 0xFCD58UL, 0xFCDB8UL
};

int cart_slot2_map_gpio_region_arm9(unsigned char *ctx)
{

    void *fp340 = (void *)io_slot2_gpio_read8_idle;
    void *fp350 = (void *)io_slot2_gpio_read16_idle;
    void *fp358 = (void *)io_slot2_gpio_read32_idle;
    void *fp360 = (void *)io_slot2_gpio_write8;
    void *fp378 = (void *)io_slot2_gpio_write16;
    void *fp390 = (void *)io_slot2_gpio_write32;
    void *fp220 = (void *)vram_resolve_ptr_returns_null;

    int i;
    for (i = 0; i < 8; i++) {
        unsigned char *R = ctx + REC_BASE[i];

        wr32_at(R, 0x00, 0x7fffff);
        wr_ptr_at(R, 0x08, fp340);
        wr_ptr_at(R, 0x10, fp350);
        wr_ptr_at(R, 0x18, fp358);
        wr_ptr_at(R, 0x20, fp360);
        wr_ptr_at(R, 0x28, fp378);
        wr_ptr_at(R, 0x30, fp390);
        wr64_at(R, 0x38, 0);
        wr64_at(R, 0x40, 0);
        wr_ptr_at(R, 0x48, fp220);
        wr_ptr_at(R, 0x50, fp220);
        wr8_at(R, 0x58, 2);
        wr8_at(R, 0x59, 2);
    }

    return 0;
}
#undef FP_340
#undef FP_350
#undef FP_358
#undef FP_360
#undef FP_378
#undef FP_390
#undef FP_220
#undef REC_STRIDE


static void write_descriptor(bus_region_t *r,
                               void *pointer_1, void *pointer_2,
                               void *pointer_3, void *fallback)
{
    r->mask = BUS_REGION_MASK;
    r->read_kind = BUS_REGION_HANDLED;
    r->read32 = pointer_3;
    r->read8 = pointer_1;
    r->read16 = pointer_2;
    r->write_kind = BUS_REGION_IGNORED;
    r->code_bits = 0;
    r->page_bits = 0;
    r->bitmap_lookup[0] = fallback;
    r->bitmap_lookup[1] = fallback;
}

uint32_t cart_slot2_map_region_both_cpus(void *param_1)
{

    uint8_t *ctx = (uint8_t *)param_1;
    void *first_1;
    void *first_2;
    void *first_3;
    void *second_1;
    void *second_2;
    void *second_3;
    void *fallback = (void *)vram_resolve_ptr_returns_null;

    first_1 = slot2_serial_read_idle[0];
    first_2 = slot2_serial_read_idle[1];
    first_3 = slot2_serial_read_idle[2];
    write_descriptor(&((bus_t *)ctx)->region[16], first_1, first_2, first_3, fallback);
    write_descriptor(&((bus_t *)ctx)->region[17], first_1, first_2, first_3, fallback);
    write_descriptor(&((bus_t *)ctx)->region[18], first_1, first_2, first_3, fallback);
    write_descriptor(&((bus_t *)ctx)->region[19], first_1, first_2, first_3, fallback);

    second_1 = slot2_serial_read_pop[0];
    second_2 = slot2_serial_read_pop[1];
    second_3 = slot2_serial_read_pop[2];
    write_descriptor(&((bus_t *)ctx)->region[20], second_1, second_2, second_3, fallback);
    write_descriptor(&((bus_t *)ctx)->region[21], second_1, second_2, second_3, fallback);

    write_descriptor(&((bus_t *)ctx)->region[48], first_1, first_2, first_3, fallback);
    write_descriptor(&((bus_t *)ctx)->region[49], first_1, first_2, first_3, fallback);
    write_descriptor(&((bus_t *)ctx)->region[50], first_1, first_2, first_3, fallback);
    write_descriptor(&((bus_t *)ctx)->region[51], first_1, first_2, first_3, fallback);

    write_descriptor(&((bus_t *)ctx)->region[52], second_1, second_2, second_3, fallback);
    write_descriptor(&((bus_t *)ctx)->region[53], second_1, second_2, second_3, fallback);

    return 0;
}


uint32_t cart_slot2_backup_read_byte(const slot2_t *sub, uint32_t dir)
{

    unsigned char type;
    type = sub->backup_type;

    if (type == 3u) {
        unsigned char subtype;
        subtype = sub->backup_command;

        if (subtype != 2u) {
            uint32_t bank;
            const unsigned char *datum;
            unsigned char b;

            bank = sub->flash_bank;
            datum = sub->save;

            bank = bank + (uint32_t)(uint16_t)dir;

            memcpy(&b, datum + (uint64_t)bank, 1);
            return (uint32_t)b;
        }

        if (dir == 1u) {
            uint32_t size;
            size = sub->save_size;

            return (size == 0x10000u) ? 0x1bu : 9u;
        }

        if (dir == 0u) {
            uint32_t size;
            size = sub->save_size;

            return (size == 0x10000u) ? 0x32u : 0xffffffc2u;
        }

        return 0xffu;
    }

    if (type == 1u) {
        uint32_t size;
        const unsigned char *datum;
        unsigned char b;

        size = sub->save_size;
        datum = sub->save;

        {
            uint64_t mask = (uint64_t)(uint32_t)(size - 1u);
            uint64_t idx     = mask & (uint64_t)dir;
            memcpy(&b, datum + idx, 1);
        }
        return (uint32_t)b;
    }

    return 0xffu;
}

typedef void *(*fn_memset)(void *, int, size_t);


void cart_slot2_backup_write_byte(slot2_t *sub, uint32_t dir, uint32_t value)
{

    static fn_memset s_memset;

    uint8_t type = sub->backup_type;

    if (type == 3u) {

        uint8_t state = sub->backup_command;

        if ((value & 0xffu) == 0xf0u && state == 2u) {
            sub->backup_command = (0);
            return;
        }

        if (dir == 0x2aaau) {
            if ((value & 0xffu) == 0x55u) {
                uint8_t step_b = sub->backup_phase;
                if (step_b == 1u) {
                    sub->backup_phase = (2);
                    return;
                }
            }
            goto common;
        }

        if (dir == 0x5555u && state != 3u) {

            uint8_t step_a = sub->backup_phase;

            if ((value & 0xffu) == 0xaau && step_a == 0u) {
                sub->backup_phase = (1);
                return;
            }

            if (step_a != 2u)
                return;

            {

                uint32_t x   = (value & 0xffu) - 0x10u;
                uint32_t idx = (x >> 4) | (x << 28);

                if (idx > 10u)
                    goto reset_step;

                switch (idx) {
                case 0:

                    if (state == 1u) {
                        void *datum = sub->save;
                        uint32_t size = sub->save_size;
                        if (!s_memset) s_memset = (fn_memset)sym_libc_memset;
                        s_memset(datum, 0, (size_t)size);
                        sub->backup_command = 0;
                        sub->save_countdown = (0x3cu);

                        sub->backup_phase = (0);
                        return;
                    }
                    goto reset_step;

                case 7:
                    if (state != 0u) goto reset_step;
                    sub->backup_command = (1);
                    sub->backup_phase = (0);
                    return;

                case 8:
                    if (state != 0u) goto reset_step;
                    sub->backup_command = (2);
                    sub->backup_phase = (0);
                    return;

                case 9:
                    if (state != 0u) goto reset_step;
                    sub->backup_command = (3);
                    sub->backup_phase = (0);
                    return;

                case 10: {
                    uint32_t size;
                    if (state != 0u) goto reset_step;
                    size = sub->save_size;

                    if (size != 0x20000u) goto reset_step;
                    sub->backup_command = (4);
                    sub->backup_phase = (0);
                    return;
                }

                default:
                    goto reset_step;
                }
            }
        }

    common:
        {

            uint8_t step_c = sub->backup_phase;

            if (step_c == 0u) {
                if (dir == 0u && state == 4u) {

                    sub->flash_bank = ((value & 1u) << 16);
                    sub->backup_command = 0;
                    return;
                }

                if (state != 3u)
                    return;
                {
                    uint32_t bank = sub->flash_bank;
                    unsigned char *datum = (unsigned char *)sub->save;

                    uint32_t idx = bank + dir;
                    uint8_t  b   = (uint8_t)value;
                    memcpy(datum + (uint64_t)idx, &b, 1);
                    sub->backup_command = 0;
                    sub->save_countdown = (0x3cu);
                    return;
                }
            }

            if (step_c != 2u)
                return;
            if ((value & 0xffu) != 0x30u)
                return;
            if (state != 1u)
                return;

            {

                unsigned char *datum = (unsigned char *)sub->save;
                uint32_t bank = sub->flash_bank;
                unsigned char *dest;

                dest = datum + (uint64_t)bank;
                dest = dest + (uint64_t)(dir & 0xf000u);
                if (!s_memset) s_memset = (fn_memset)sym_libc_memset;
                s_memset(dest, 0xff, 0x1000u);
                {

                    sub->backup_command = 0;
                    sub->backup_phase = 0;
                }
                sub->save_countdown = (0x3cu);
                return;
            }
        }

    reset_step:
        sub->backup_phase = (0);
        return;
    }

    if (type == 1u) {
        uint32_t size = sub->save_size;
        unsigned char *datum = (unsigned char *)sub->save;

        uint64_t mask = (uint64_t)(uint32_t)(size - 1u);
        uint64_t idx     = mask & (uint64_t)dir;
        uint8_t  b       = (uint8_t)value;
        memcpy(datum + idx, &b, 1);
        sub->save_countdown = (0x3cu);
        return;
    }
}

static int    (*core_fseek_11)(void *, long, int);
static size_t (*core_fwrite_11)(const void *, size_t, size_t, void *);
static int    (*core_fflush_11)(void *);

long cart_slot2_backup_autosave_tick(slot2_t *obj) {
    uint32_t left = obj->save_countdown;
    if (left == 0)
        return (long)(intptr_t)obj;

    left -= 1;
    obj->save_countdown = left;
    if (left != 0)
        return (long)(intptr_t)obj;

    if (obj->loaded == 0)
        return (long)(intptr_t)obj;
    void *data = obj->save;
    if (data == 0)
        return (long)(intptr_t)obj;
    void *file = obj->file;
    if (file == 0)
        return (long)(intptr_t)obj;

    if (!core_fseek_11) {
        core_fseek_11  = (int (*)(void *, long, int))sym_libc_fseek;
        core_fwrite_11 = (size_t (*)(const void *, size_t, size_t, void *))sym_libc_fwrite;
        core_fflush_11 = (int (*)(void *))sym_libc_fflush;
    }
    core_fseek_11(file, 0, 0);
    uint32_t size = obj->save_size;
    core_fwrite_11(obj->save, (size_t)size, 1, file);
    return core_fflush_11(file);
}

#define P_BASE     0x59a0
#define P_FILE     0x8e8
#define P_ROM      0x8f0
#define P_SAVE     0x8f8
#define P_ROM_SIZE  0x900
#define P_SAVE_SIZE 0x904
#define P_ZERO     0x90c
#define P_TYPE     0x910
#define P_READY    0x913
#define P_PATH     0x4c8
#define PATH_N     0x420
#define MIN_ROM    0x800000
#define FMT_GBA    0x10edbc
#define FMT_SAV    0x10edce
#define MODE_R     0x10fbe2
#define MODE_RW    0x10ede0
#define MODE_W     0x106d1f
#define M_FLAS     0x53414c46u
#define M_H_V      0x00565f48u
#define M_H512     0x32313548u
#define M_H1M      0x5f4d3148u
#define M_EEPR     0x52504545u
#define M_OM_V     0x565f4d4fu
#define M_SRAM     0x4d415253u
#define M_V_       0x00005f56u
typedef void    *(*fn_malloc_12)(uint64_t);
typedef void     (*fn_free)(void *);
typedef int      (*fn_fclose)(void *);
typedef long     (*fn_ftell)(void *);
typedef int      (*fn_fseek)(void *, long, int);
typedef uint64_t (*fn_fread)(void *, uint64_t, uint64_t, void *);
typedef void    *(*fn_memset_12)(void *, int, uint64_t);


int32_t cart_slot2_load_gba(cart_t *cart, const char *name) {
    slot2_t *p = &cart->slot2;

    uint8_t *base = (uint8_t *)cart->machine;
    const char *dir = ((nds_t *)base)->save_dir;

    char path[PATH_N];
    str_vsnprintf_limited_swapped_args(path, PATH_N, PATH_N, "%s%cslot2%c%s.gba",
                         dir, '/', '/', name);
    void *fp = nds_platform_default()->files.open(nds_platform_default()->user, path, "rb");

    if (p->loaded) {
        ((fn_free)sym_libc_free)(p->rom);
        void *s = p->save;
        if (s) ((fn_free)sym_libc_free)(s);
        void *f = p->file;
        if (f) ((fn_fclose)sym_libc_fclose)(f);
        p->loaded = 0;
        p->file = NULL;
        p->rom = NULL;
        p->save = NULL;
    }
    p->save_countdown = (0);

    if (!fp) return -1;

    long pos = ((fn_ftell)sym_libc_ftell)(fp);
    ((fn_fseek)sym_libc_fseek)(fp, 0, 2);
    long len = ((fn_ftell)sym_libc_ftell)(fp);
    p->rom_size = ((uint32_t)len);
    ((fn_fseek)sym_libc_fseek)(fp, pos, 0);

    uint32_t real = p->rom_size;

    uint32_t block = ((uint64_t)real > (uint64_t)MIN_ROM) ? real : MIN_ROM;
    uint8_t *rom = (uint8_t *)((fn_malloc_12)sym_libc_malloc)(block);
    p->rom = rom;
    ((fn_fread)sym_libc_fread)(rom, real, 1, fp);
    ((fn_fclose)sym_libc_fclose)(fp);

    uint32_t r2 = p->rom_size;
    if (block != r2)
        ((fn_memset_12)sym_libc_memset)(
            p->rom + r2, 0xff, block - r2);

    const uint8_t *q = p->rom;
    uint32_t left = (block >> 2) - 1u;
    p->rom_size = (block);
    p->save_size = (0);
    p->save_countdown &= 0x00ffffffu;
    p->backup_type = 0;
    p->backup_command = 0;
    p->backup_phase = 0;
    p->save_countdown = (0);

    if (left != 0) {
        uint32_t a = rd32(q); q += 4;
        uint32_t type = 0, size = 0;
        for (;;) {
            uint32_t b = rd32(q);
            if (a == M_FLAS) {
                if ((b & 0xffffffu) == M_H_V) { type = 3; size = 0x10000; break; }
                if (b == M_H512)              { type = 3; size = 0x10000; break; }
                if (b == M_H1M)               { type = 3; size = 0x20000; break; }
            } else if (a == M_EEPR) {
                if (b == M_OM_V) { p->backup_type = 2; goto save; }
            } else if (a == M_SRAM) {
                if ((b & 0xffffu) == M_V_) { type = 1; size = 0x8000; break; }
            }
            left--;
            q += 4;
            a = b;
            if (left == 0) goto save;
        }
        p->save_size = (size);
        p->backup_type = (uint8_t)type;
        p->save = ((fn_malloc_12)sym_libc_malloc)(size);
    }

save:;

    char *rsav = p->path;
    str_vsnprintf_limited_swapped_args(rsav, PATH_N, PATH_N, "%s%cslot2%c%s.sav",
                         dir, '/', '/', name);
    void *fs = nds_platform_default()->files.open(nds_platform_default()->user, rsav, "rb");
    const char *mode;

    if (fs && p->save != NULL) {
        long p0 = ((fn_ftell)sym_libc_ftell)(fs);
        ((fn_fseek)sym_libc_fseek)(fs, 0, 2);
        long end = ((fn_ftell)sym_libc_ftell)(fs);
        ((fn_fseek)sym_libc_fseek)(fs, p0, 0);

        uint32_t fits = p->save_size;
        uint32_t read_fn = ((uint64_t)fits < (uint64_t)end) ? fits : (uint32_t)end;
        ((fn_fread)sym_libc_fread)(p->save, read_fn, 1, fs);
        ((fn_fclose)sym_libc_fclose)(fs);

        uint32_t c2 = p->save_size;
        if (c2 > read_fn)
            ((fn_memset_12)sym_libc_memset)(
                p->save + read_fn, 0xff, c2 - read_fn);
        mode = "rb+";
    } else {
        if (fs) ((fn_fclose)sym_libc_fclose)(fs);
        ((fn_memset_12)sym_libc_memset)(p->save, 0xff,
                                          p->save_size);
        mode = "wb";
    }

    p->file = nds_platform_default()->files.open(nds_platform_default()->user, rsav, mode);
    p->loaded = 1;
    cart_slot2_map((unsigned char *)(&((nds_t *)base)->bus), p->rom,
                         p->rom_size);
    return 0;
}
#undef P_BASE
#undef P_FILE
#undef P_ROM
#undef P_SAVE
#undef P_ROM_SIZE
#undef P_SAVE_SIZE
#undef P_ZERO
#undef P_TYPE
#undef P_READY
#undef P_PATH
#undef PATH_N
#undef MIN_ROM
#undef FMT_GBA
#undef FMT_SAV
#undef MODE_R
#undef MODE_RW
#undef MODE_W
#undef M_FLAS
#undef M_H_V
#undef M_H512
#undef M_H1M
#undef M_EEPR
#undef M_OM_V
#undef M_SRAM
#undef M_V_

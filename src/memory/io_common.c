#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "memory/io_common.h"
#include "mem_access.h"

static void *const io_cache_write_handlers[3] = {
    (void *)io_reg_cache_write8,
    (void *)io_reg_cache_write16,
    (void *)io_reg_cache_write32,
};

void io_write_port16(unsigned char *state, uint32_t dir, uint32_t value)
{

    uint32_t sel = dir & 0xc000u;
    uint32_t off = dir & 0x3fffu;

    uint16_t mid = (uint16_t)value;

    if (sel == 0x4000u) {

        memcpy(((bus_t *)state)->arm7_io_scratch + off, &mid, 2);
        return;
    }

    if (off == 0x158u) {

        if ((value & 0x3000u) == 0x1000u) {

            unsigned char b = ((bus_t *)state)->wifi_regs[0x15a];

            ((bus_t *)state)->wifi_regs[0x400 + (value & 0x7fu)] = b;
        }

    }

    memcpy(((bus_t *)state)->wifi_regs + off, &mid, 2);

}

int io_reg_read_returns_zero(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return 0;
}

#define PORTS_TABLE  0xfb5b8
#define REG_15C    0xfb710
#define TABLE_15C      0xfb9b8

unsigned int io_read_port16(const uint8_t *state, unsigned int dir) {

    unsigned off = dir & 0x3fff;

    if ((dir & 0xc000) == 0x4000) {
        uint16_t v;
        __builtin_memcpy(&v, ((const bus_t *)state)->arm7_io_scratch + off, 2);
        return v;
    }

    if ((int)off <= 0x15b) {
        if (off == 0x004) return 0;
        if (off == 0x03c) return 0x200;
    } else {
        if (off == 0x180) return 0;
        if (off == 0x15e) return 0;
        if (off == 0x15c) {
            uint16_t v;
            __builtin_memcpy(&v, state + REG_15C, 2);
            if ((v & 0x7000) != 0x6000) return 0;
            return state[TABLE_15C + (v & 0x7f)];
        }
    }

    {
        uint16_t v;
        __builtin_memcpy(&v, state + PORTS_TABLE + off, 2);
        return v;
    }
}
#undef PORTS_TABLE
#undef REG_15C
#undef TABLE_15C

int io_read32_unmapped_zero(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return 0;
}

int32_t io_exmemcnt_gba_slot_arm9_mask_byte(const unsigned char *obj, uint32_t dir) {
    (void)dir;
    uint32_t b = obj[0x1b274];
    return (int32_t)((b >> 7) - 1u);
}

int io_exmemcnt_gba_slot_arm9_mask_word(unsigned char *cpu, uint32_t dir) {
    (void)dir;
    uint32_t v = *(uint32_t *)&((bus_t *)cpu)->io_mirror[0].exmemcnt;
    return (int32_t)((v >> 7) & 1) - 1;
}

int32_t io_exmemcnt_gba_slot_arm7_mask(const unsigned char *obj, uint32_t dir) {
    (void)dir;
    uint32_t v;
    memcpy(&v, &((const bus_t *)obj)->io_mirror[0].exmemcnt, 4);
    return -(int32_t)((v >> 7) & 1u);
}

int io_exmemcnt_gba_slot_arm7_mask_clone(unsigned char *cpu, uint32_t dir) {
    (void)dir;
    uint32_t v = *(uint32_t *)&((bus_t *)cpu)->io_mirror[0].exmemcnt;
    return (int32_t)(v << 24) >> 31;
}

int io_exmemcnt_gba_slot_arm9_mask(unsigned char *cpu, uint32_t dir) {
    (void)dir;
    uint32_t v = ((bus_t *)cpu)->io_mirror[0].exmemcnt;
    return (int32_t)((v >> 7) & 1) - 1;
}

int32_t io_exmemcnt_gba_slot_arm7_mask_clone2(const unsigned char *obj, uint32_t dir) {
    (void)dir;
    uint32_t v;
    memcpy(&v, &((const bus_t *)obj)->io_mirror[0].exmemcnt, 4);
    return -(int32_t)((v >> 7) & 1u);
}

uint64_t io_slot2_backup_read8(const unsigned char *ctx, uint32_t dir) {
    unsigned char *sub;
    sub = (uint8_t *)((bus_t *)ctx)->cart;
    return cart_slot2_backup_read_byte(&((cart_t *)sub)->slot2, dir);
}

int io_slot2_backup_read16_zero(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return 0;
}

int io_slot2_backup_read32_zero(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return 0;
}

void io_slot2_backup_write8(uint8_t *ctx, uint32_t dir, uint32_t value) {

    uint8_t *sub;
    sub = (uint8_t *)((bus_t *)ctx)->cart;

    cart_slot2_backup_write_byte(&((cart_t *)sub)->slot2, dir, value);
}

uint64_t io_slot2_backup_write16_ignore(uint64_t x0)
{
    return x0;
}

uint64_t io_slot2_backup_write32_ignore(uint64_t x0)
{
    return x0;
}

int32_t io_slot2_gpio_read8_idle(uint64_t param_1, uint32_t param_2) {
    (void)param_1;

    if ((param_2 & 1) != 0) {
        return -1;
    }
    return -3;
}

int io_slot2_gpio_read16_idle(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return 0xfffd;
}

int io_slot2_gpio_read32_idle(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return (int)0xfffdfffd;
}


void io_slot2_gpio_write8(uint8_t *ctx, uint64_t param_2, uint32_t param_3) {
    (void)param_2;

    uint8_t *sub;
    sub = (uint8_t *)((bus_t *)ctx)->cart;

    cart_slot2_gpio_set_bit1(&((cart_t *)sub)->slot2.gpio, param_3);
}


void io_slot2_gpio_write16(uint8_t *ctx, uint64_t param_2, uint32_t param_3) {
    (void)param_2;

    uint8_t *sub;
    sub = (uint8_t *)((bus_t *)ctx)->cart;

    cart_slot2_gpio_set_bit1(&((cart_t *)sub)->slot2.gpio, param_3);
}


void io_slot2_gpio_write32(uint8_t *ctx, uint64_t param_2, uint32_t param_3) {
    (void)param_2;

    uint8_t *sub1;
    sub1 = (uint8_t *)((bus_t *)ctx)->cart;

    cart_slot2_gpio_set_bit1(&((cart_t *)sub1)->slot2.gpio, param_3);

    uint8_t *sub2;
    sub2 = (uint8_t *)((bus_t *)ctx)->cart;

    cart_slot2_gpio_set_bit1(&((cart_t *)sub2)->slot2.gpio, param_3 >> 16);
}

int32_t io_slot2_serial_read8_idle(uint64_t param_1, uint32_t param_2) {

    (void)param_1;

    if ((param_2 & 1u) != 0u)
        return (int32_t)0xfffffffc;

    return (int32_t)0xffffffff;
}

uint32_t io_slot2_serial_read16_idle(void)
{
    return 0xfcffu;
}

unsigned int io_slot2_serial_read32_idle(void) {
    return 0xfcfffcffu;
}

uint64_t io_slot2_serial_read8_pop(const unsigned char *ctx) {
    unsigned char *sub;
    sub = (uint8_t *)((bus_t *)ctx)->cart;
    return cart_slot2_queue_pop_2bit(&((cart_t *)sub)->slot2.sensor);
}


uint32_t io_slot2_serial_read16_pop(uint8_t *ctx) {

    uint8_t *sub;
    sub = (uint8_t *)((bus_t *)ctx)->cart;

    uint32_t r = cart_slot2_queue_pop_2bit(&((cart_t *)sub)->slot2.sensor);
    return r & 0xffu;
}


uint32_t io_slot2_serial_read32_pop(uint8_t *ctx) {

    uint8_t *sub;
    sub = (uint8_t *)((bus_t *)ctx)->cart;

    uint32_t r = cart_slot2_queue_pop_2bit(&((cart_t *)sub)->slot2.sensor);

    return r & 0xffu;
}

uint32_t io_slot2_open_bus_read8(void)
{
    return 0xffu;
}

unsigned int io_slot2_open_bus_read16(void)
{
    return 0xffffu;
}

int io_slot2_open_bus_read32(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return -1;
}


uint32_t io_slot2_serial_read8_field(uint8_t *ctx, uint32_t param_2) {

    uint8_t *sub;
    sub = (uint8_t *)((bus_t *)ctx)->cart;

    uint32_t v = param_2 & 0xfu;
    return cart_slot2_descriptor_next_field(&((cart_t *)sub)->slot2.sensor, v);
}

typedef uint32_t (*fn_queue)(slot2_sensor_t *);


uint32_t io_slot2_serial_read32_pop_pair(uint8_t *ctx) {

    fn_queue queue_pop = cart_slot2_queue_pop_2bit;

    uint32_t r1 = queue_pop(&((bus_t *)ctx)->cart->slot2.sensor) & 0xffu;

    uint32_t r2 = queue_pop(&((bus_t *)ctx)->cart->slot2.sensor) & 0xffu;

    return r1 | (r2 << 16);
}

#define CACHE_OFF    0x15870
#define POINTERS_OFF 0xfba68
#define WRITER_OFF 0x50014
typedef void (*fn_writer)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void io_reg_cache_write8(unsigned char *state, uint32_t dir, uint32_t value) {

    uint32_t idx = dir & 0x7ff;
    unsigned char *cache = state + CACHE_OFF + idx;

    if (*cache == (uint8_t)value) return;

    unsigned char **pointers = (unsigned char **)(state + POINTERS_OFF);
    unsigned char *first = pointers[0];
    unsigned char *second = pointers[1];

    uint16_t extra;
    __builtin_memcpy(&extra, first + 20, 2);

    gpu2d_engine_t *dest = (dir & 0x400) ? &((gpu_t *)second)->engine[1]
                                            : &((gpu_t *)second)->engine[0];

    fn_writer write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | 0x200000u, (uint8_t)value, 1u, extra);

    *cache = (uint8_t)value;
}
#undef CACHE_OFF
#undef POINTERS_OFF
#undef WRITER_OFF

#define MARK_OFF     0xfbff8
#define SENTINEL_OFF 0x27874
#define ORIGIN_OFF    0x15070
#define CACHE_OFF     0x15870
#define TABLE_SIZE     0x800
#define AUX_OFF       0xfbfe0
#define TEMPLATE_OFF 0x133a60
#define POINTERS_OFF  0xfba68
#define MARK_ADDR     0x200000u

void io_reg_cache_write8_init(uint8_t *state, unsigned dir, unsigned value) {

    void *mark;
    memcpy(&mark, state + MARK_OFF, 8);

    if (mark != (void *)io_reg_cache_write8) {
        uint8_t *table = state + CACHE_OFF;
        memcpy(table, state + ORIGIN_OFF, TABLE_SIZE);

        const uint8_t *template =
            (const uint8_t *)io_cache_write_handlers;
        uint8_t *aux = state + AUX_OFF;

        aux[80] = 0;
        memcpy(aux, &table, 8);
        memcpy(state + MARK_OFF + 16, template + 16, 8);
        memcpy(state + MARK_OFF, template, 16);

    }

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + CACHE_OFF + idx;

    uint8_t prev = *cache;
    if (prev == (uint8_t)value) return;

    uint8_t original = (uint8_t)value;

    void **pointers = (void **)(state + POINTERS_OFF);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    memcpy(&extra, first + 20, 2);

    uint8_t *dest = ((dir & 0x400) == 0) ? (uint8_t *)&((gpu_t *)second)->engine[0]
                                            : (uint8_t *)&((gpu_t *)second)->engine[1];

    gpu2d_deferred_capture_queue_push((gpu2d_engine_t *)((gpu2d_engine_t *)(dest)), (uint32_t)idx | MARK_ADDR,
                       (uint32_t)(value & 0xffu), 1u, (uint32_t)extra);

    *cache = original;
}
#undef MARK_OFF
#undef SENTINEL_OFF
#undef ORIGIN_OFF
#undef CACHE_OFF
#undef TABLE_SIZE
#undef AUX_OFF
#undef TEMPLATE_OFF
#undef POINTERS_OFF
#undef MARK_ADDR

#define MARK_OFF   0xfbff8u
#define OFF_VT16    0xfc000u
#define OFF_VT32    0xfc008u
#define SELF_OFF    0xfbfe0u
#define FLAG_OFF    0xfc030u
#define CACHE_OFF   0x15870u
#define DEFAULT_OFF 0x15070u
#define CACHE_SIZE   0x800u
#define FIXED_TABLE  0x133a60u
#define MARK_874   0x27874u

uint32_t io_reg_cache_read8_init(unsigned char *state, uint32_t dir)
{

    void *mark = rd_ptr(state + MARK_OFF);

    if (mark != (void *)io_reg_cache_write8) {

        memcpy(state + CACHE_OFF, state + DEFAULT_OFF, CACHE_SIZE);

        state[FLAG_OFF] = 0;

        wr_ptr(state + SELF_OFF, state + CACHE_OFF);

        wr64(state + OFF_VT32,
            rd64((const void *)((const uint8_t *)io_cache_write_handlers + 16)));

        wr64(state + MARK_OFF,
            rd64((const void *)io_cache_write_handlers));
        wr64(state + OFF_VT16,
            rd64((const void *)((const uint8_t *)io_cache_write_handlers + 8)));
    }

    return state[(dir & 0x7ffu) + CACHE_OFF];
}
#undef MARK_OFF
#undef OFF_VT16
#undef OFF_VT32
#undef SELF_OFF
#undef FLAG_OFF
#undef CACHE_OFF
#undef DEFAULT_OFF
#undef CACHE_SIZE
#undef FIXED_TABLE
#undef MARK_874

#define CACHE_OFF    0x15870
#define POINTERS_OFF 0xfba68
#define WRITER_OFF 0x50014
typedef void (*fn_writer_35)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void io_reg_cache_write16(uint8_t *state, unsigned dir, unsigned value) {

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + CACHE_OFF + idx;

    uint16_t prev;
    __builtin_memcpy(&prev, cache, 2);
    if (prev == (uint16_t)value) return;

    void **pointers = (void **)(state + POINTERS_OFF);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    __builtin_memcpy(&extra, first + 20, 2);

    gpu2d_engine_t *dest = (dir & 0x400) ? &((gpu_t *)second)->engine[1]
                                      : &((gpu_t *)second)->engine[0];

    fn_writer_35 write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | 0x200000u, value & 0xffffu, 2u, extra);

    uint16_t updated = (uint16_t)value;
    __builtin_memcpy(cache, &updated, 2);
}
#undef CACHE_OFF
#undef POINTERS_OFF
#undef WRITER_OFF

#define MARK_OFF     0xfbff8
#define SENTINEL_OFF 0x27874
#define ORIGIN_OFF    0x15070
#define CACHE_OFF     0x15870
#define TABLE_SIZE     0x800
#define AUX_OFF       0xfbfe0
#define TEMPLATE_OFF 0x133a60
#define POINTERS_OFF  0xfba68
#define MARK_ADDR     0x200000u
#define WRITER_OFF  0x50014
typedef void (*fn_writer_36)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void io_reg_cache_write16_init(uint8_t *state, unsigned dir, unsigned value) {

    void *mark;
    memcpy(&mark, state + MARK_OFF, 8);
    if (mark != (void *)io_reg_cache_write8) {
        uint8_t *table = state + CACHE_OFF;
        memcpy(table, state + ORIGIN_OFF, TABLE_SIZE);

        const uint8_t *template = (const uint8_t *)io_cache_write_handlers;
        uint8_t *aux = state + AUX_OFF;

        aux[80] = 0;
        memcpy(aux, &table, 8);
        memcpy(state + MARK_OFF + 16, template + 16, 8);
        memcpy(state + MARK_OFF, template, 16);
    }

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + CACHE_OFF + idx;

    uint16_t prev;
    memcpy(&prev, cache, 2);
    if (prev == (uint16_t)value) return;

    void **pointers = (void **)(state + POINTERS_OFF);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    memcpy(&extra, first + 20, 2);

    void *dest = (dir & 0x400) ? (void *)&((gpu_t *)second)->engine[1]
                                  : (void *)&((gpu_t *)second)->engine[0];

    fn_writer_36 write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | MARK_ADDR, value & 0xffffu, 2u, extra);

    uint16_t updated = (uint16_t)value;
    memcpy(cache, &updated, 2);
}
#undef MARK_OFF
#undef SENTINEL_OFF
#undef ORIGIN_OFF
#undef CACHE_OFF
#undef TABLE_SIZE
#undef AUX_OFF
#undef TEMPLATE_OFF
#undef POINTERS_OFF
#undef MARK_ADDR
#undef WRITER_OFF

#define MARK_OFF   0xfbff8u
#define OFF_VT16    0xfc000u
#define OFF_VT32    0xfc008u
#define SELF_OFF    0xfbfe0u
#define FLAG_OFF    0xfc030u
#define CACHE_OFF   0x15870u
#define DEFAULT_OFF 0x15070u
#define CACHE_SIZE   0x800u
#define FIXED_TABLE  0x133a60u
#define MARK_874   0x27874u

uint32_t io_reg_cache_read16_init(unsigned char *state, uint32_t dir)
{

    static void *(*s_memcpy)(void *, const void *, size_t);
    if (!s_memcpy)
        s_memcpy = (void *(*)(void *, const void *, size_t))sym_libc_memcpy;

    void *mark = rd_ptr(state + MARK_OFF);

    if (mark != (void *)io_reg_cache_write8) {

        s_memcpy(state + CACHE_OFF, state + DEFAULT_OFF, CACHE_SIZE);

        uint64_t t0 = rd64((const void *)io_cache_write_handlers);
        uint64_t t8 = rd64((const void *)((const uint8_t *)io_cache_write_handlers + 8));
        uint64_t t16 = rd64((const void *)((const uint8_t *)io_cache_write_handlers + 16));

        state[FLAG_OFF] = 0;

        wr_ptr(state + SELF_OFF, state + CACHE_OFF);

        wr64(state + OFF_VT32, t16);

        wr64(state + MARK_OFF, t0);
        wr64(state + OFF_VT16, t8);
    }

    uint32_t idx = dir & 0x7ffu;

    return (uint32_t)rd16(state + CACHE_OFF + idx);
}
#undef MARK_OFF
#undef OFF_VT16
#undef OFF_VT32
#undef SELF_OFF
#undef FLAG_OFF
#undef CACHE_OFF
#undef DEFAULT_OFF
#undef CACHE_SIZE
#undef FIXED_TABLE
#undef MARK_874

#define CACHE_OFF    0x15870
#define POINTERS_OFF 0xfba68
#define WRITER_OFF 0x50014
typedef void (*fn_writer_38)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void io_reg_cache_write32(uint8_t *state, unsigned dir, unsigned value) {

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + CACHE_OFF + idx;

    uint32_t prev;
    __builtin_memcpy(&prev, cache, 4);
    if (prev == value) return;

    void **pointers = (void **)(state + POINTERS_OFF);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    __builtin_memcpy(&extra, first + 20, 2);

    void *dest = (dir & 0x400) ? (void *)&((gpu_t *)second)->engine[1]
                                  : (void *)&((gpu_t *)second)->engine[0];

    fn_writer_38 write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | 0x200000u, value, 4u, extra);

    __builtin_memcpy(cache, &value, 4);
}
#undef CACHE_OFF
#undef POINTERS_OFF
#undef WRITER_OFF

#define MARK_OFF    0xfbff8
#define SENTINEL_OFF 0x27874
#define ORIGIN_OFF   0x15070
#define CACHE_OFF    0x15870
#define TABLE_SIZE    0x800
#define AUX_OFF      0xfbfe0
#define TEMPLATE_OFF 0x133a60
#define POINTERS_OFF 0xfba68
#define WRITER_OFF 0x50014
typedef void (*fn_writer_39)(gpu2d_engine_t *, uint32_t, uint32_t, uint32_t, uint32_t);

void io_reg_cache_write32_init(uint8_t *state, unsigned dir, unsigned value) {

    void *mark;
    memcpy(&mark, state + MARK_OFF, 8);
    if (mark != (void *)io_reg_cache_write8) {
        uint8_t *table = state + CACHE_OFF;
        memcpy(table, state + ORIGIN_OFF, TABLE_SIZE);

        const uint8_t *template = (const uint8_t *)io_cache_write_handlers;
        uint8_t *aux = state + AUX_OFF;

        aux[80] = 0;
        memcpy(aux, &table, 8);
        memcpy(state + MARK_OFF + 16, template + 16, 8);
        memcpy(state + MARK_OFF, template, 16);
    }

    unsigned idx = dir & 0x7ff;
    uint8_t *cache = state + CACHE_OFF + idx;

    uint32_t prev;
    memcpy(&prev, cache, 4);
    if (prev == value) return;

    void **pointers = (void **)(state + POINTERS_OFF);
    uint8_t *first = (uint8_t *)pointers[0];
    uint8_t *second = (uint8_t *)pointers[1];

    uint16_t extra;
    memcpy(&extra, first + 20, 2);

    void *dest = (dir & 0x400) ? (void *)&((gpu_t *)second)->engine[1]
                                  : (void *)&((gpu_t *)second)->engine[0];

    fn_writer_39 write_fn = gpu2d_deferred_capture_queue_push;
    write_fn(dest, idx | 0x200000u, value, 4u, extra);

    memcpy(cache, &value, 4);
}
#undef MARK_OFF
#undef SENTINEL_OFF
#undef ORIGIN_OFF
#undef CACHE_OFF
#undef TABLE_SIZE
#undef AUX_OFF
#undef TEMPLATE_OFF
#undef POINTERS_OFF
#undef WRITER_OFF

#define MARK_OFF   0xfbff8u
#define OFF_VT16    0xfc000u
#define OFF_VT32    0xfc008u
#define SELF_OFF    0xfbfe0u
#define FLAG_OFF    0xfc030u
#define CACHE_OFF   0x15870u
#define DEFAULT_OFF 0x15070u
#define CACHE_SIZE   0x800u
#define FIXED_TABLE  0x133a60u
#define MARK_874   0x27874u

uint32_t io_bank15870_read32_lazy_init(unsigned char *state, uint32_t dir)
{

    static void *(*s_memcpy)(void *, const void *, size_t);
    if (!s_memcpy)
        s_memcpy = (void *(*)(void *, const void *, size_t))sym_libc_memcpy;

    void *mark = rd_ptr(state + MARK_OFF);

    if (mark != (void *)io_reg_cache_write8) {

        s_memcpy(state + CACHE_OFF, state + DEFAULT_OFF, CACHE_SIZE);

        uint64_t t0  = rd64((const void *)io_cache_write_handlers);
        uint64_t t8  = rd64((const void *)((const uint8_t *)io_cache_write_handlers + 8));
        uint64_t t16 = rd64((const void *)((const uint8_t *)io_cache_write_handlers + 16));

        state[FLAG_OFF] = 0;

        wr_ptr(state + SELF_OFF, state + CACHE_OFF);

        wr64(state + OFF_VT32, t16);

        wr64(state + MARK_OFF, t0);
        wr64(state + OFF_VT16, t8);
    }

    uint32_t idx = dir & 0x7ffu;

    return rd32(state + CACHE_OFF + idx);
}
#undef MARK_OFF
#undef OFF_VT16
#undef OFF_VT32
#undef SELF_OFF
#undef FLAG_OFF
#undef CACHE_OFF
#undef DEFAULT_OFF
#undef CACHE_SIZE
#undef FIXED_TABLE
#undef MARK_874

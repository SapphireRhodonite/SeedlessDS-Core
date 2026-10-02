#include <stdint.h>
#include "script_lua.h"
#include "frontend/script/script_state.h"
#include "frontend/script/script_io.h"
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/data_paths.h"
#include "core/nds_state.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdio.h>
#include "frontend/script/script_io.h"
#include "core_internals.h"
#include "mem_access.h"

script_state_t script_state;

uint32_t script_io_get_axis_lx(void *obj) {

    float value = SCRIPT_IO->axis_lx;
    return script_cursor_push_float(obj, value);
}

uint32_t script_io_get_axis_ly(void *obj) {

    float value;
    value = SCRIPT_IO->axis_ly;
    return script_cursor_push_float(obj, value);
}

extern uint32_t script_cursor_push_float_2(void *obj, float value) __asm__("script_cursor_push_float");

uint32_t script_io_get_axis_rx(void *obj) {

    float value;
    value = SCRIPT_IO->axis_rx;
    return script_cursor_push_float(obj, value);
}


uint32_t script_io_get_axis_ry(void *obj) {

    float value;
    value = SCRIPT_IO->axis_ry;
    return script_cursor_push_float(obj, value);
}

extern uint32_t script_cursor_push_int_4(void *obj, uint32_t value) __asm__("script_cursor_push_int");

uint32_t script_io_get_rotation(void *obj) {

    return script_cursor_push_int(
        obj, (uint32_t)(int32_t)SCRIPT_IO->rotation);
}


uint32_t script_io_set_layout(void *param_1) {

    return script_lua_read_integer_arg(param_1, &SCRIPT_IO->layout);
}


uint32_t script_io_show_overlay(void *param_1) {

    return script_lua_read_integer_arg(param_1,
                                  &SCRIPT_IO->overlay);
}


uint32_t script_io_set_screen_swap(void *param_1) {

    return script_lua_read_integer_arg(param_1, &SCRIPT_IO->screen_swap);
}

int script_lua_init(unsigned char *machine) {

    script_state_t *g = SCRIPT_STATE;
    void (*require_library)(void *, const char *, void *, uint32_t) =
        (void (*)(void *, const char *, void *, uint32_t))((void *)luaL_requiref);
    void (*settop)(void *, int32_t) = (void (*)(void *, int32_t))((void *)lua_settop);
    void (*name_str)(void *, const char *) =
        (void (*)(void *, const char *))((void *)lua_pushstring);
    void (*int_val)(void *, uint32_t) = (void (*)(void *, uint32_t))((void *)lua_pushinteger);
    void (*settable)(void *, int32_t) = (void (*)(void *, int32_t))((void *)lua_settable);

    g->machine = (struct nds *)machine;
    g->input_record = (struct nds_input_record *)&((nds_t *)machine)->input_record;
    g->bus = (struct bus *)&((nds_t *)machine)->bus;
    g->loaded = 0;
    g->on_load_called = 0;

    void *L = ((void *(*)(void))((void *)luaL_newstate))();
    g->lua = L;
    if (L == 0) return -1;

    static lua_CFunction const opener[8] = {
        luaopen_base, luaopen_package, luaopen_table, luaopen_io,
        luaopen_string, luaopen_math, luaopen_utf8, luaopen_bit32
    };
    static const char *const label[8] = {
        "_G", "package", "table", "io",
        "string", "math", "utf8", "bit32"
    };
    for (int i = 0; i < 8; i++) {
        L = g->lua;
        require_library(L, label[i],
                  (void *)opener[i], 1);
        L = g->lua;
        settop(L, -2);
    }

    L = g->lua;
    {
        uint32_t bits = 0x43fb8000;
        float f;
        __builtin_memcpy(&f, &bits, 4);
        ((void (*)(void *, uint32_t, float))((void *)luaL_checkversion_))(L, 68, f);
    }

typedef struct script_lua_entry {
    const char *name;
    uint32_t (*func)(void *state);
} script_lua_entry_t;

static const script_lua_entry_t script_lua_registry[18] = {
    { "get_path",              (uint32_t (*)(void *))script_lua_get_path },
    { "get_buttons",           (uint32_t (*)(void *))script_lua_get_buttons },
    { "set_buttons",           script_lua_set_buttons },
    { "get_touch",             script_lua_get_touch },
    { "set_touch",             script_lua_set_touch },
    { "get_ds_memory_arm9_8",  script_lua_get_ds_memory_arm9_8 },
    { "get_ds_memory_arm9_16", script_lua_get_ds_memory_arm9_16 },
    { "get_ds_memory_arm9_32", script_lua_get_ds_memory_arm9_32 },
    { "get_ds_memory_arm7_8",  script_lua_get_ds_memory_arm7_8 },
    { "get_ds_memory_arm7_16", script_lua_get_ds_memory_arm7_16 },
    { "get_ds_memory_arm7_32", script_lua_get_ds_memory_arm7_32 },
    { "set_ds_memory_arm9_8",  script_lua_set_ds_memory_arm9_8 },
    { "set_ds_memory_arm9_16", script_lua_set_ds_memory_arm9_16 },
    { "set_ds_memory_arm9_32", script_lua_set_ds_memory_arm9_32 },
    { "set_ds_memory_arm7_8",  script_lua_set_ds_memory_arm7_8 },
    { "set_ds_memory_arm7_16", script_lua_set_ds_memory_arm7_16 },
    { "set_ds_memory_arm7_32", script_lua_set_ds_memory_arm7_32 },
    { 0, 0 },
};

    L = g->lua;
    ((void (*)(void *, int32_t, int32_t))((void *)lua_createtable))(L, 0, 0x11);
    L = g->lua;
    ((void (*)(void *, void *, int32_t))((void *)luaL_setfuncs))(
        L, (void *)script_lua_registry, 0);
    L = g->lua;
    name_str(L, "C");
    L = g->lua;
    ((void (*)(void *, int32_t, int32_t))((void *)lua_createtable))(L, 0xe, 0);

    static const char *const text[14] = {
        "BUTTON_UP", "BUTTON_DOWN", "BUTTON_LEFT", "BUTTON_RIGHT", "BUTTON_A",
        "BUTTON_B", "BUTTON_X", "BUTTON_Y", "BUTTON_L", "BUTTON_R",
        "BUTTON_START", "BUTTON_SELECT", "BUTTON_FFWD", "BUTTON_TOUCH"
    };
    static const uint32_t value[14] = {
        1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80,
        0x100, 0x200, 0x400, 0x800, 0x20000, 0x80000000u
    };
    for (int i = 0; i < 14; i++) {
        L = g->lua;
        name_str(L, text[i]);
        L = g->lua;
        int_val(L, value[i]);
        L = g->lua;
        settable(L, -3);
    }

    L = g->lua;
    settable(L, -3);
    L = g->lua;
    ((long (*)(void *, void *))((void *)lua_setglobal))(L, (void *)SCRIPT_GLOBAL_TABLE);
    return 0;
}

FILE *recon_lua_fopen(const char *path, const char *mode)
{
    extern void *files_fopen_resolved(const char *, const char *);
    return (FILE *)files_fopen_resolved(path, mode);
}
typedef int32_t (*fnp_lua_loadfilex)(void *, void *, uint64_t);


int32_t script_run_file(const char *path)
{

    SCRIPT_STATE->loaded = 0;

    if (path == NULL) return -1;

    lua_State *obj = SCRIPT_STATE->lua;
    if (obj == NULL) return -1;

    int32_t chk = ((fnp_lua_loadfilex)((void *)luaL_loadfilex))(obj, (void *)path, 0);
    if (chk != 0) return -1;

    lua_pcallk(SCRIPT_STATE->lua, 0, -1, 0, 0, 0);

    SCRIPT_STATE->loaded = 1;

    return 0;
}

typedef int32_t (*fn_lua_getglobal)(void *, const char *);
typedef int32_t (*fn_lua_pcallk)(void *, int32_t, int32_t, int32_t,
                                 uint64_t, void *);




void script_run_on_unload(void)
{

    if (SCRIPT_STATE->loaded == 0)
        return;

    (void)((fn_lua_getglobal)((void *)lua_getglobal))(SCRIPT_STATE->lua, "on_unload");

    (void)((fn_lua_pcallk)((void *)lua_pcallk))(SCRIPT_STATE->lua, 0, -1, 0, 0, NULL);

    SCRIPT_STATE->on_load_called = 0;
}

typedef int32_t (*fn_notice)(void *, const char *);
typedef int32_t (*fn_prepare_close)(void *, int32_t, int32_t, int32_t,
                                     uint64_t, void *);
typedef void (*fn_close)(void *);





void script_shutdown(void) {
    script_state_t *g = SCRIPT_STATE;

    if (g->lua == 0)
        return;

    if (g->on_load_called != 0) {
        if (g->loaded != 0) {
            (void)((fn_notice)((void *)lua_getglobal))(g->lua,
                                          "on_unload");

            (void)((fn_prepare_close)((void *)lua_pcallk))(g->lua, 0, -1, 0, 0, 0);
            g->on_load_called = 0;
        }
    }

    ((fn_close)((void *)lua_close))(g->lua);
    g->lua = 0;
    g->loaded = 0;
}

uint32_t script_is_active(void) {
    return (uint32_t)((SCRIPT_STATE->lua != 0) & (SCRIPT_STATE->loaded != 0));
}

typedef void     (*fn_f1)(void *, int, float);
typedef void     (*fn_f2)(void *, int, int);
typedef void     (*fn_f3)(void *, void *, int);
typedef uint64_t (*fn_f4)(void *, void *);

static void *read_object(void) {
    return SCRIPT_STATE->lua;
}

uint64_t script_register_library(const char *name, const void *functions, unsigned count) {

    void *obj = read_object();
    if (obj == 0) return 0;

    uint8_t flag = *(volatile uint8_t *)&SCRIPT_STATE->loaded;
    if (flag != 0) return 0;

    uint32_t pattern = 0x43fb8000u;
    float c;
    memcpy(&c, &pattern, 4);

    ((fn_f1)(((void *)luaL_checkversion_)))(obj, 68, c);

    obj = read_object();
    ((fn_f2)(((void *)lua_createtable)))(obj, 0, (int)count - 1);

    obj = read_object();
    ((fn_f3)(((void *)luaL_setfuncs)))(obj, (void *)functions, 0);

    obj = read_object();
    return ((fn_f4)(((void *)lua_setglobal)))(obj, (void *)name);
}

typedef void * (*fnp_lua_push_integer)(unsigned char *obj, unsigned int value);
extern void *lua_push_integer(unsigned char *obj, uint32_t value);

uint32_t script_cursor_push_int(void *obj, uint32_t value) {

    (void)((fnp_lua_push_integer)((void *)lua_pushinteger))((unsigned char *)obj, value);

    return 1;
}

typedef uint32_t (*fnp_lua_checkinteger)(void *, uint32_t);

uint32_t script_lua_read_integer_arg(void *param_1, uint32_t *param_2) {

    uint32_t uVar1 = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(param_1, 1);
    *param_2 = uVar1;

    return 0;
}

typedef void * (*fnp_lua_push_number)(unsigned char *obj, float value);
extern void *lua_push_number(unsigned char *obj, float value);

uint32_t script_cursor_push_float(void *obj, float value) {

    ((fnp_lua_push_number)((void *)lua_pushnumber))(obj, value);

    return 1;
}

#ifndef RECON_VECTOR16
#define RECON_VECTOR16
typedef uint8_t vector16_t __attribute__((vector_size(16)));
#endif
typedef vector16_t (*fnp_lua_checknumber)(void *, int32_t);


uint32_t script_lua_read_float_arg(void *param_1, uint32_t *param_2)
{

    vector16_t value = ((fnp_lua_checknumber)((void *)luaL_checknumber))(param_1, 1);
    uint32_t low;

    memcpy(&low, &value, sizeof(low));
    wr32(param_2, low);
    return 0;
}
#undef RECON_VECTOR16

typedef int   (*fn_getglobal)(void *, const char *);
typedef void *(*fn_push)(void *, const char *);
typedef int   (*fn_pcall)(void *, uint32_t, int32_t, int32_t, uint64_t, void *);




void script_invoke_on_load(const char *path) {

    script_state_t *control = SCRIPT_STATE;

    if (control->loaded == 0)
        return;

    if (control->on_load_called != 0) {
        ((fn_getglobal)((void *)lua_getglobal))(control->lua,
                                   "on_unload");
        ((fn_pcall)((void *)lua_pcallk))(control->lua, 0, -1, 0, 0, 0);
        control->on_load_called = 0;
    }

    ((fn_getglobal)((void *)lua_getglobal))(control->lua, "on_load");
    ((fn_push)((void *)lua_pushstring))(control->lua, path);
    ((fn_pcall)((void *)lua_pcallk))(control->lua, 1, -1, 0, 0, 0);
    control->on_load_called = 1;
}

typedef int32_t (*fn_lua_getglobal_19)(void *, const char *);
typedef int32_t (*fn_lua_pcallk_19)(void *, int32_t, int32_t, int32_t,
                                 uint64_t, void *);


void script_run_on_frame_update(void)
{

    if (SCRIPT_STATE->loaded == 0) return;

    (void)((fn_lua_getglobal_19)((void *)lua_getglobal))(SCRIPT_STATE->lua, "on_frame_update");

    (void)((fn_lua_pcallk_19)((void *)lua_pcallk))(SCRIPT_STATE->lua, 0, -1, 0, 0, NULL);
}


typedef void * (*fnp_lua_pushstring)(unsigned char *, void *);

unsigned int script_lua_get_path(void) {

    fnp_lua_pushstring pushstring = (fnp_lua_pushstring)((void *)lua_pushstring);

    unsigned char *obj = (unsigned char *)SCRIPT_STATE->lua;
    void *arg = SCRIPT_STATE->machine->cache_dir;

    pushstring(obj, arg);

    return 1;
}

#define MASK    0x20fffUL
typedef void * (*fnp_lua_pushinteger_bytes)(unsigned char *, uint32_t);



uint32_t script_lua_get_buttons(unsigned char *param_1) {

    fnp_lua_pushinteger_bytes pushinteger = (fnp_lua_pushinteger_bytes)((void *)lua_pushinteger);

    nds_input_record_t *record = SCRIPT_STATE->input_record;

    uint32_t w9 = record->flags;
    uint8_t  w8 = rd8(&record->touch_down);

    w9 &= MASK;

    uint32_t value = (w8 == 0) ? w9 : (w9 | 0x80000000UL);

    pushinteger(param_1, value);

    return 1;
}
#undef MASK

typedef int32_t (*fn_checkinteger_t)(void *L, unsigned long idx);




uint32_t script_lua_set_buttons(void *L)
{

    int32_t int_val = ((fn_checkinteger_t)((void *)luaL_checkinteger))(L, 1);
    nds_input_record_t *record = SCRIPT_STATE->input_record;

    uint32_t prev = record->flags;
    wr8(&record->touch_down, (uint8_t)((uint32_t)int_val >> 31));
    prev &= 0xfffdf000u;
    prev |= (uint32_t)int_val & 0x00020fffu;
    record->flags = prev;
    return 0;
}

typedef void * (*fnp_lua_pushinteger)(void *, uint32_t);




uint32_t script_lua_get_touch(void *param_1) {

    fnp_lua_pushinteger pushinteger = (fnp_lua_pushinteger)((void *)lua_pushinteger);

    nds_input_record_t *record = SCRIPT_STATE->input_record;

    uint32_t w9 = record->touch_x;
    uint16_t w1 = rd16(&record->touch_y);

    uint32_t value = ((uint32_t)w9 << 16) | (uint32_t)w1;

    pushinteger(param_1, value);

    return 1;
}


typedef uint32_t (*fn_selector)(void *obj, uint32_t mode);

uint32_t script_lua_set_touch(void *obj)
{

    uint32_t result = ((fn_selector)((void *)luaL_checkinteger))(obj, 1u);
    SCRIPT_STATE->input_record->touch_x = result;

    result = ((fn_selector)((void *)luaL_checkinteger))(obj, 2u);
    SCRIPT_STATE->input_record->touch_y = result;

    return 0;
}

typedef uint32_t (*fn_u32_pu32)(void *, uint32_t);

uint32_t script_lua_get_ds_memory_arm9_8(void *param_1) {

    fn_u32_pu32 checkinteger = (fn_u32_pu32)((void *)luaL_checkinteger);
    uint32_t (*read8)(uint8_t *, uint32_t) = bus_read8;
    fn_u32_pu32 pushinteger = (fn_u32_pu32)((void *)lua_pushinteger);

    uint32_t w0 = checkinteger(param_1, 1);

    nds_t *machine = SCRIPT_STATE->machine;

    void *arg0 = &machine->arm9.pagetable;

    uint32_t w0b = read8(arg0, w0);
    uint32_t w1 = w0b & 0xff;

    pushinteger(param_1, w1);

    return 1;
}

typedef uint32_t (*fnp_bus_read16)(unsigned char *, uint32_t);

uint32_t script_lua_get_ds_memory_arm9_16(void *param_1) {

    fnp_lua_checkinteger checkinteger = (fnp_lua_checkinteger)(((void *)luaL_checkinteger));
    fnp_bus_read16 read16 = bus_read16;
    fnp_lua_pushinteger pushinteger = (fnp_lua_pushinteger)(((void *)lua_pushinteger));

    uint32_t uVar2 = checkinteger(param_1, 1);

    void *table = &SCRIPT_STATE->machine->arm9.pagetable;

    uint32_t uVar0 = read16(table, uVar2);

    uint32_t uVar1 = uVar0 & 0xffffu;

    pushinteger(param_1, uVar1);

    return 1;
}


typedef uint32_t (*fn_u32_pu32_27)(void *, uint32_t);

uint32_t script_lua_get_ds_memory_arm9_32(void *param_1)
{

    uint32_t idx = ((fn_u32_pu32_27)((void *)luaL_checkinteger))(param_1, 1u);
    uint32_t value = bus_read32(
        (unsigned char *)&SCRIPT_STATE->machine->arm9.pagetable, idx);

    ((fnp_lua_pushinteger)((void *)lua_pushinteger))(param_1, value);
    return 1u;
}

typedef uint32_t (*fn_u32_pu32_28)(void *, uint32_t);

uint32_t script_lua_get_ds_memory_arm7_8(void *param_1)
{

    uint32_t idx = ((fn_u32_pu32_28)((void *)luaL_checkinteger))(param_1, 1);
    uint32_t value;

    value = bus_read8(
        (uint8_t *)&SCRIPT_STATE->machine->arm7.pagetable, idx);
    ((fnp_lua_pushinteger)((void *)lua_pushinteger))(param_1, value & 0xffu);

    return 1;
}
#undef F

typedef int32_t (*fnp_lua_checkinteger_i32)(void *, uint32_t);
typedef void (*fnp_lua_pushinteger_void)(void *, uint32_t);

uint32_t script_lua_get_ds_memory_arm7_16(void *state)
{

    uint32_t address = (uint32_t)((fnp_lua_checkinteger_i32)((void *)luaL_checkinteger))(state, 1);
    uint32_t value = bus_read16(
        (unsigned char *)&SCRIPT_STATE->machine->arm7.pagetable, address);

    ((fnp_lua_pushinteger_void)((void *)lua_pushinteger))(state, value & 0xffffu);
    return 1;
}


uint32_t script_lua_get_ds_memory_arm7_32(void *param_1)
{

    uint32_t idx = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(param_1, 1u);
    uint8_t *table = (uint8_t *)&SCRIPT_STATE->machine->arm7.pagetable;
    uint32_t value = bus_read32(table, idx);

    (void)((fnp_lua_pushinteger)((void *)lua_pushinteger))(param_1, value);
    return 1u;
}

typedef struct { uint64_t v; } __attribute__((packed, may_alias)) unaligned64;

typedef struct { uint32_t v; } __attribute__((packed, may_alias)) unaligned32;

uint32_t script_lua_set_ds_memory_arm9_8(void *L)
{

    uint32_t dir = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 1u);
    uint32_t val = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 2u);

    nds_t *machine = SCRIPT_STATE->machine;
    uint8_t *table = (uint8_t *)&machine->arm9.pagetable;

    uint64_t ent = ((const unaligned64 *)(const void *)
                    (table + ((uint64_t)(dir >> 11) << 3)))->v;
    uint8_t *base = (uint8_t *)(uintptr_t)(ent << 2);

    if (((ent >> 62) & 1u) == 0u) {
        base[(uint64_t)dir] = (uint8_t)val;
        return 0u;
    }

    if (((ent >> 63) & 1u) == 0u) {
        bus_write8_slow(table, dir, val);
        return 0u;
    }

    if (val == (uint32_t)base[(uint64_t)dir])
        return 0u;

    {
        void *cpu = &machine->arm9;

        base[(uint64_t)dir] = (uint8_t)val;

        if (jit_watch_write8(cpu, dir) != 0u) {
            uint8_t *state = (uint8_t *)&machine->arm9.jit_block;

            jit_cache_flush(cpu, 0x2000000u);

            if (((const unaligned64 *)(const void *)state)->v != 0u) {
                uint8_t *r;
                jit_cache_refresh_block_info(cpu);
                r = jit_cache_lookup_or_compile(
                        cpu, ((const unaligned32 *)(const void *)(state + 292))->v);
                wr_ptr(state, r + 8);
            }
        }
    }
    return 0u;
}

typedef struct { uint64_t v; } __attribute__((packed, may_alias)) unaligned64_32;

typedef struct { uint32_t v; } __attribute__((packed, may_alias)) unaligned32_32;

typedef struct { uint16_t v; } __attribute__((packed, may_alias)) unaligned16;

uint32_t script_lua_set_ds_memory_arm9_16(void *L)
{

    uint32_t dir = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 1u);
    uint64_t msk = (uint64_t)dir & 0xfffffffeULL;
    uint32_t val = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 2u);

    nds_t *machine = SCRIPT_STATE->machine;
    uint8_t *table = (uint8_t *)&machine->arm9.pagetable;

    uint64_t ent = ((const unaligned64_32 *)(const void *)
                    (table + ((uint64_t)(dir >> 11) << 3)))->v;
    uint8_t *base = (uint8_t *)(uintptr_t)(ent << 2);

    if (((ent >> 62) & 1u) == 0u) {
        ((unaligned16 *)(void *)(base + msk))->v = (uint16_t)val;
        return 0u;
    }

    if (((ent >> 63) & 1u) == 0u) {
        bus_write16_slow(table, (uint32_t)msk, val);
        return 0u;
    }

    if (val == (uint32_t)((const unaligned16 *)(const void *)(base + msk))->v)
        return 0u;

    {
        void *cpu = &machine->arm9;

        ((unaligned16 *)(void *)(base + msk))->v = (uint16_t)val;

        if (jit_watch_write16(cpu, (uint32_t)msk) != 0u) {
            uint8_t *state = (uint8_t *)&machine->arm9.jit_block;

            jit_cache_flush(cpu, 0x2000000u);

            if (((const unaligned64_32 *)(const void *)state)->v != 0u) {
                uint8_t *r;
                jit_cache_refresh_block_info(cpu);
                r = jit_cache_lookup_or_compile(
                        cpu, ((const unaligned32_32 *)(const void *)(state + 292))->v);
                wr_ptr(state, r + 8);
            }
        }
    }
    return 0u;
}

typedef struct { uint64_t v; } __attribute__((packed, may_alias)) unaligned64_33;

typedef struct { uint32_t v; } __attribute__((packed, may_alias)) unaligned32_33;

uint32_t script_lua_set_ds_memory_arm9_32(void *L)
{

    uint32_t dir = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 1u);
    uint64_t msk = (uint64_t)dir & 0xfffffffcULL;
    uint32_t val = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 2u);

    nds_t *machine = SCRIPT_STATE->machine;
    uint8_t *table = (uint8_t *)&machine->arm9.pagetable;

    uint64_t ent = ((const unaligned64_33 *)(const void *)
                    (table + ((uint64_t)(dir >> 11) << 3)))->v;
    uint8_t *base = (uint8_t *)(uintptr_t)(ent << 2);

    if (((ent >> 62) & 1u) == 0u) {
        ((unaligned32_33 *)(void *)(base + msk))->v = val;
        return 0u;
    }

    if (((ent >> 63) & 1u) == 0u) {
        bus_write32_slow(table, (uint32_t)msk, val);
        return 0u;
    }

    if (((const unaligned32_33 *)(const void *)(base + msk))->v == val)
        return 0u;

    {
        void *cpu = &machine->arm9;

        ((unaligned32_33 *)(void *)(base + msk))->v = val;

        if (jit_watch_write32(cpu, (uint32_t)msk) != 0u) {
            uint8_t *state = (uint8_t *)&machine->arm9.jit_block;

            jit_cache_flush(cpu, 0x2000000u);

            if (((const unaligned64_33 *)(const void *)state)->v != 0u) {
                uint8_t *r;
                jit_cache_refresh_block_info(cpu);
                r = jit_cache_lookup_or_compile(
                        cpu, ((const unaligned32_33 *)(const void *)(state + 292))->v);
                wr_ptr(state, r + 8);
            }
        }
    }
    return 0u;
}

typedef struct { uint64_t v; } __attribute__((packed, may_alias)) unaligned64_34;

typedef struct { uint32_t v; } __attribute__((packed, may_alias)) unaligned32_34;

uint32_t script_lua_set_ds_memory_arm7_8(void *L)
{

    uint32_t dir = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 1u);
    uint32_t val = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 2u);

    nds_t *machine = SCRIPT_STATE->machine;
    uint8_t *table = (uint8_t *)&machine->arm7.pagetable;

    uint64_t ent = ((const unaligned64_34 *)(const void *)
                    (table + ((uint64_t)(dir >> 11) << 3)))->v;
    uint8_t *base = (uint8_t *)(uintptr_t)(ent << 2);

    if (((ent >> 62) & 1u) == 0u) {
        base[(uint64_t)dir] = (uint8_t)val;
        return 0u;
    }

    if (((ent >> 63) & 1u) == 0u) {
        bus_write8_slow(table, dir, val);
        return 0u;
    }

    if (val == (uint32_t)base[(uint64_t)dir])
        return 0u;

    {
        void *cpu = &machine->arm7;

        base[(uint64_t)dir] = (uint8_t)val;

        if (jit_watch_write8(cpu, dir) != 0u) {
            uint8_t *state = (uint8_t *)&machine->arm7.jit_block;

            jit_cache_flush(cpu, 0x2000000u);

            if (((const unaligned64_34 *)(const void *)state)->v != 0u) {
                uint8_t *r;
                jit_cache_refresh_block_info(cpu);
                r = jit_cache_lookup_or_compile(
                        cpu, ((const unaligned32_34 *)(const void *)(state + 292))->v);
                wr_ptr(state, r + 8);
            }
        }
    }
    return 0u;
}

typedef struct { uint64_t v; } __attribute__((packed, may_alias)) unaligned64_35;

typedef struct { uint32_t v; } __attribute__((packed, may_alias)) unaligned32_35;

typedef struct { uint16_t v; } __attribute__((packed, may_alias)) unaligned16_35;

uint32_t script_lua_set_ds_memory_arm7_16(void *L)
{

    uint32_t dir = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 1u);
    uint64_t msk = (uint64_t)dir & 0xfffffffeULL;
    uint32_t val = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 2u);

    nds_t *machine = SCRIPT_STATE->machine;
    uint8_t *table = (uint8_t *)&machine->arm7.pagetable;

    uint64_t ent = ((const unaligned64_35 *)(const void *)
                    (table + ((uint64_t)(dir >> 11) << 3)))->v;
    uint8_t *base = (uint8_t *)(uintptr_t)(ent << 2);

    if (((ent >> 62) & 1u) == 0u) {
        ((unaligned16_35 *)(void *)(base + msk))->v = (uint16_t)val;
        return 0u;
    }

    if (((ent >> 63) & 1u) == 0u) {
        bus_write16_slow(table, (uint32_t)msk, val);
        return 0u;
    }

    if (val == (uint32_t)((const unaligned16_35 *)(const void *)(base + msk))->v)
        return 0u;

    {
        void *cpu = &machine->arm7;

        ((unaligned16_35 *)(void *)(base + msk))->v = (uint16_t)val;

        if (jit_watch_write16(cpu, (uint32_t)msk) != 0u) {
            uint8_t *state = (uint8_t *)&machine->arm7.jit_block;

            jit_cache_flush(cpu, 0x2000000u);

            if (((const unaligned64_35 *)(const void *)state)->v != 0u) {
                uint8_t *r;
                jit_cache_refresh_block_info(cpu);
                r = jit_cache_lookup_or_compile(
                        cpu, ((const unaligned32_35 *)(const void *)(state + 292))->v);
                wr_ptr(state, r + 8);
            }
        }
    }
    return 0u;
}

typedef struct { uint64_t v; } __attribute__((packed, may_alias)) unaligned64_36;

typedef struct { uint32_t v; } __attribute__((packed, may_alias)) unaligned32_36;

uint32_t script_lua_set_ds_memory_arm7_32(void *L)
{

    uint32_t dir = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 1u);
    uint64_t msk = (uint64_t)dir & 0xfffffffcULL;
    uint32_t val = ((fnp_lua_checkinteger)((void *)luaL_checkinteger))(L, 2u);

    nds_t *machine = SCRIPT_STATE->machine;
    uint8_t *table = (uint8_t *)&machine->arm7.pagetable;

    uint64_t ent = ((const unaligned64_36 *)(const void *)
                    (table + ((uint64_t)(dir >> 11) << 3)))->v;
    uint8_t *base = (uint8_t *)(uintptr_t)(ent << 2);

    if (((ent >> 62) & 1u) == 0u) {
        ((unaligned32_36 *)(void *)(base + msk))->v = val;
        return 0u;
    }

    if (((ent >> 63) & 1u) == 0u) {
        bus_write32_slow(table, (uint32_t)msk, val);
        return 0u;
    }

    if (((const unaligned32_36 *)(const void *)(base + msk))->v == val)
        return 0u;

    {
        void *cpu = &machine->arm7;

        ((unaligned32_36 *)(void *)(base + msk))->v = val;

        if (jit_watch_write32(cpu, (uint32_t)msk) != 0u) {
            uint8_t *state = (uint8_t *)&machine->arm7.jit_block;

            jit_cache_flush(cpu, 0x2000000u);

            if (((const unaligned64_36 *)(const void *)state)->v != 0u) {
                uint8_t *r;
                jit_cache_refresh_block_info(cpu);
                r = jit_cache_lookup_or_compile(
                        cpu, ((const unaligned32_36 *)(const void *)(state + 292))->v);
                wr_ptr(state, r + 8);
            }
        }
    }
    return 0u;
}
#define JIT_DISPATCH_ARM9_OFFSET 0x15cfd50
#define JIT_DISPATCH_ARM7_OFFSET 0x25d6340
#define JIT_DISPATCH_STR_(x) #x
#define JIT_DISPATCH_STR(x) JIT_DISPATCH_STR_(x)
_Static_assert(offsetof(nds_t, arm9) == JIT_DISPATCH_ARM9_OFFSET, "arm9 offset");
_Static_assert(offsetof(nds_t, arm7) == JIT_DISPATCH_ARM7_OFFSET, "arm7 offset");
__asm__(
"    .text\n"
"    .align 2\n"
"    .global jit_dispatch_arm\n"
"    .type   jit_dispatch_arm, %function\n"
"    .global jit_dispatch_block_cache\n"
"    .type   jit_dispatch_block_cache, %function\n"
"    .global jit_block_end_switch_processor\n"
"    .type   jit_block_end_switch_processor, %function\n"
"    .global jit_block_end_run_scheduler\n"
"    .type   jit_block_end_run_scheduler, %function\n"
"    .global jit_scheduler_return_dispatch\n"
"    .type   jit_scheduler_return_dispatch, %function\n"
"jit_dispatch_arm:\n"
"    ldr x1, [x28, #8816]                      \n"
"    ubfx w2, w0, #2, #13                      \n"
"    ldr w1, [x1, w2, uxtw #2]                 \n"
"    cbz w1, L_jit_dispatch_lookup_or_compile_1                           \n"
"    add x1, x11, w1, uxtw                     \n"
"    br x1                                     \n"
"jit_dispatch_block_cache:\n"
"    and w1, w0, #0xffc                        \n"
"    add w2, w1, #0x80                         \n"
"    add w1, w2, #0x1, lsl #12                 \n"
"    ldr w2, [x28, w2, uxtw]                   \n"
"    ldr w1, [x28, w1, uxtw]                   \n"
"    sub w2, w2, w0                            \n"
"    cbnz w2, L_jit_dispatch_lookup_or_compile_1                          \n"
"    add x1, x11, w1, uxtw                     \n"
"    br x1                                     \n"

"    .globl jit_dispatch_lookup_or_compile\n"
"    .type  jit_dispatch_lookup_or_compile, %function\n"
"jit_dispatch_lookup_or_compile:\n"
"L_jit_dispatch_lookup_or_compile_1:\n"
"    mov w1, w0                                \n"
"    mov x0, x28                               \n"
"    stp w12, w13, [x9, #-224]                 \n"
"    stp w14, w15, [x9, #-216]                 \n"
"    stp w16, w17, [x9, #-208]                 \n"
"    str w18, [x28, #8968]                     \n"
"    mrs x2, nzcv                              \n"
"    str w2, [x28, #9044]                      \n"
"    bl   jit_cache_lookup_or_compile\n"
"    add x9, x28, #0x3d0                       \n"
"    add x9, x9, #0x2, lsl #12                 \n"
"    ldp x10, x11, [x9, #-120]                 \n"
"    ldp w12, w13, [x9, #-224]                 \n"
"    ldp w14, w15, [x9, #-216]                 \n"
"    ldp w16, w17, [x9, #-208]                 \n"
"    ldr w18, [x28, #8968]                     \n"
"    ldr w2, [x28, #9044]                      \n"
"    msr nzcv, x2                              \n"
"    br x0                                     \n"
"L_jit_dispatch_lookup_or_compile_2:\n"
"    mov w12, #0xffffffff                      \n"
"jit_block_end_switch_processor:\n"
"L_jit_block_end_switch_processor_1:\n"
"    str x30, [x28, #8856]                     \n"
"    mrs x0, nzcv                              \n"
"    stp w13, w14, [x9, #-96]                  \n"
"    stp w15, w16, [x9, #-88]                  \n"
"    stp w17, w18, [x9, #-80]                  \n"
"    stp w19, w20, [x9, #-72]                  \n"
"    stp w21, w22, [x9, #-64]                  \n"
"    stp w23, w24, [x9, #-56]                  \n"
"    stp w25, w26, [x9, #-48]                  \n"
"    stur w27, [x9, #-40]                      \n"
"    str w0, [x28, #9144]                      \n"
"    str w12, [x28, #8848]                     \n"
"    ldr x28, [x28, #8864]                     \n"
"    ldr w12, [x28, #8848]                     \n"
"    ldr x0, [x28, #8792]                      \n"
"    ldr w0, [x0, #16]                         \n"
"    ldr x30, [x28, #8856]                     \n"
"    add w12, w12, w0                          \n"
"    ldr w0, [x28, #8464]                      \n"
"    add x9, x28, #0x3d0                       \n"
"    add x9, x9, #0x2, lsl #12                 \n"
"    ldp x10, x11, [x9, #-120]                 \n"
"    ldr w1, [x28, #9144]                      \n"
"    ldp w13, w14, [x9, #-96]                  \n"
"    ldp w15, w16, [x9, #-88]                  \n"
"    ldp w17, w18, [x9, #-80]                  \n"
"    ldp w19, w20, [x9, #-72]                  \n"
"    ldp w21, w22, [x9, #-64]                  \n"
"    ldp w23, w24, [x9, #-56]                  \n"
"    ldp w25, w26, [x9, #-48]                  \n"
"    ldur w27, [x9, #-40]                      \n"
"    msr nzcv, x1                              \n"
"    tbnz w12, #31, L_jit_block_end_run_scheduler_1                    \n"
"    cbnz w0, L_jit_block_end_switch_processor_2                          \n"
"    br x30                                    \n"
"L_jit_block_end_switch_processor_2:\n"
"    mov w12, #0xffffffff                      \n"
"jit_block_end_run_scheduler:\n"
"L_jit_block_end_run_scheduler_1:\n"
"    str x30, [x28, #8856]                     \n"
"    mrs x0, nzcv                              \n"
"    stp w13, w14, [x9, #-96]                  \n"
"    stp w15, w16, [x9, #-88]                  \n"
"    stp w17, w18, [x9, #-80]                  \n"
"    stp w19, w20, [x9, #-72]                  \n"
"    stp w21, w22, [x9, #-64]                  \n"
"    stp w23, w24, [x9, #-56]                  \n"
"    stp w25, w26, [x9, #-48]                  \n"
"    stur w27, [x9, #-40]                      \n"
"    str w0, [x28, #9144]                      \n"
"    str w12, [x28, #8848]                     \n"
"    ldr x19, [x28, #8792]                     \n"
"    mov x0, x19                               \n"
"jit_scheduler_return_dispatch:\n"
"    bl   sched_advance\n"
"    mov w1, #(" JIT_DISPATCH_STR(JIT_DISPATCH_ARM7_OFFSET) " & 0xffff)\n"
"    mov w2, #(" JIT_DISPATCH_STR(JIT_DISPATCH_ARM9_OFFSET) " & 0xffff)\n"
"    movk w1, #(" JIT_DISPATCH_STR(JIT_DISPATCH_ARM7_OFFSET) " >> 16), lsl #16\n"
"    movk w2, #(" JIT_DISPATCH_STR(JIT_DISPATCH_ARM9_OFFSET) " >> 16), lsl #16\n"
"    add x20, x19, w1, uxtw                    \n"
"    add x28, x19, w2, uxtw                    \n"
"    ldr w0, [x28, #8456]                      \n"
"    ldr w2, [x28, #9152]                      \n"
"    cbz w0, L_jit_scheduler_return_dispatch_4                           \n"
"    ldr w0, [x28, #8464]                      \n"
"    ldr x1, [x28, #8856]                      \n"
"    tbnz w2, #7, L_jit_scheduler_return_dispatch_2                      \n"
"    cbz x1, L_jit_scheduler_return_dispatch_1                           \n"
"    ldur w0, [x1, #-12]                       \n"
"    ldr x1, [x28, #9056]                      \n"
"    add x0, x1, w0, uxtw                      \n"
"    ldr w1, [x0, #4]                          \n"
"    str w1, [x28, #9148]                      \n"
"L_jit_scheduler_return_dispatch_1:\n"
"    ldr w1, [x28, #9144]                      \n"
"    ldr w2, [x28, #9152]                      \n"
"    bfxil w1, w2, #0, #28                     \n"
"    mov x0, x28                               \n"
"    str w1, [x28, #9152]                      \n"
"    bl   arm_irq_raise\n"
"    ldr x0, [x28, #8832]                      \n"
"    b L_jit_scheduler_return_dispatch_3                                 \n"
"L_jit_scheduler_return_dispatch_2:\n"
"    cbz w0, L_jit_scheduler_return_dispatch_4                           \n"
"    mov x0, x28                               \n"
"    ldr w1, [x28, #9148]                      \n"
"    bl   jit_cache_lookup_or_compile\n"
"L_jit_scheduler_return_dispatch_3:\n"
"    add x0, x0, #0x8                          \n"
"    str wzr, [x28, #8464]                     \n"
"    str x0, [x28, #8856]                      \n"
"L_jit_scheduler_return_dispatch_4:\n"
"    ldr w0, [x20, #8456]                      \n"
"    ldr w2, [x20, #9152]                      \n"
"    cbz w0, L_jit_scheduler_return_dispatch_10                           \n"
"    ldr w0, [x20, #8464]                      \n"
"    ldr x1, [x20, #8856]                      \n"
"    tbnz w2, #7, L_jit_scheduler_return_dispatch_7                      \n"
"    cbz x1, L_jit_scheduler_return_dispatch_5                           \n"
"    ldur w0, [x1, #-12]                       \n"
"    ldr x1, [x20, #9056]                      \n"
"    add x0, x1, w0, uxtw                      \n"
"    ldr w1, [x0, #4]                          \n"
"    str w1, [x20, #9148]                      \n"
"L_jit_scheduler_return_dispatch_5:\n"
"    ldr w1, [x20, #8464]                      \n"
"    mov x0, x19                               \n"
"    cbz w1, L_jit_scheduler_return_dispatch_6                           \n"
"    bl   sched_event_insert\n"
"    ldr w1, [x20, #8464]                      \n"
"    tbz w1, #1, L_jit_scheduler_return_dispatch_6                       \n"
"    ldr w2, [x28, #8464]                      \n"
"    sub w2, w2, #0x2                          \n"
"    str w2, [x28, #8464]                      \n"
"L_jit_scheduler_return_dispatch_6:\n"
"    ldr w1, [x20, #9144]                      \n"
"    ldr w2, [x20, #9152]                      \n"
"    bfxil w1, w2, #0, #28                     \n"
"    mov x0, x20                               \n"
"    str w1, [x20, #9152]                      \n"
"    bl   arm_irq_raise\n"
"    ldr x0, [x20, #8832]                      \n"
"    b L_jit_scheduler_return_dispatch_9                                 \n"
"L_jit_scheduler_return_dispatch_7:\n"
"    cbz w0, L_jit_scheduler_return_dispatch_10                           \n"
"    mov x0, x19                               \n"
"    bl   sched_event_insert\n"
"    ldr w1, [x20, #8464]                      \n"
"    tbz w1, #1, L_jit_scheduler_return_dispatch_8                       \n"
"    ldr w2, [x28, #8464]                      \n"
"    sub w2, w2, #0x2                          \n"
"    str w2, [x28, #8464]                      \n"
"L_jit_scheduler_return_dispatch_8:\n"
"    mov x0, x20                               \n"
"    ldr w1, [x20, #9148]                      \n"
"    bl   jit_cache_lookup_or_compile\n"
"L_jit_scheduler_return_dispatch_9:\n"
"    add x0, x0, #0x8                          \n"
"    str wzr, [x20, #8464]                     \n"
"    str x0, [x20, #8856]                      \n"
"L_jit_scheduler_return_dispatch_10:\n"
"    ldr x0, [x19, #792]                       \n"
"    ldr w0, [x0]                              \n"
"    ldr w12, [x28, #8848]                     \n"
"    str w0, [x19, #16]                        \n"
"    add w12, w12, w0                          \n"
"    ldr x30, [x28, #8856]                     \n"
"    ldr w0, [x28, #8464]                      \n"
"    add x9, x28, #0x3d0                       \n"
"    add x9, x9, #0x2, lsl #12                 \n"
"    ldp x10, x11, [x9, #-120]                 \n"
"    ldr w1, [x28, #9144]                      \n"
"    ldp w13, w14, [x9, #-96]                  \n"
"    ldp w15, w16, [x9, #-88]                  \n"
"    ldp w17, w18, [x9, #-80]                  \n"
"    ldp w19, w20, [x9, #-72]                  \n"
"    ldp w21, w22, [x9, #-64]                  \n"
"    ldp w23, w24, [x9, #-56]                  \n"
"    ldp w25, w26, [x9, #-48]                  \n"
"    ldur w27, [x9, #-40]                      \n"
"    msr nzcv, x1                              \n"
"    tbnz w12, #31, L_jit_block_end_switch_processor_1                    \n"
"    cbnz w0, L_jit_dispatch_lookup_or_compile_2                          \n"
"    br x30                                    \n"
"L_jit_scheduler_return_dispatch_11:\n"
"    orr w1, wzr, w0, lsr #11                  \n"
"    ldr x1, [x9, w1, uxtw #3]                 \n"
"    orr x1, xzr, x1, lsl #2                   \n"
"    cbz x1, L_jit_scheduler_return_dispatch_12                           \n"
"    ldrb w0, [x1, w0, uxtw]                   \n"
"    ret                                       \n"
"L_jit_scheduler_return_dispatch_12:\n"
"    mov w1, w0                                \n"
"    mov x0, x9                                \n"
"    stp w12, w13, [x9, #-224]                 \n"
"    stp w14, w15, [x9, #-216]                 \n"
"    stp w16, w17, [x9, #-208]                 \n"
"    str w18, [x28, #8968]                     \n"
"    str x30, [x28, #9064]                     \n"
"    mrs x2, nzcv                              \n"
"    str w2, [x28, #9044]                      \n"
"    str w12, [x28, #8848]                     \n"
"    bl   bus_read8_slow\n"
"    add x9, x28, #0x3d0                       \n"
"    add x9, x9, #0x2, lsl #12                 \n"
"    ldp x10, x11, [x9, #-120]                 \n"
"    ldp w12, w13, [x9, #-224]                 \n"
"    ldp w14, w15, [x9, #-216]                 \n"
"    ldp w16, w17, [x9, #-208]                 \n"
"    ldr w18, [x28, #8968]                     \n"
"    ldr x30, [x28, #9064]                     \n"
"    ldr w2, [x28, #9044]                      \n"
"    msr nzcv, x2                              \n"
"    uxtb w0, w0                               \n"
"    ret                                       \n"
"L_jit_scheduler_return_dispatch_13:\n"
"    orr w1, wzr, w0, lsr #11                  \n"
"    ldr x1, [x9, w1, uxtw #3]                 \n"
"    orr x1, xzr, x1, lsl #2                   \n"
"    cbz x1, L_jit_scheduler_return_dispatch_14                           \n"
"    ldrsb w0, [x1, w0, uxtw]                  \n"
"    ret                                       \n"
"L_jit_scheduler_return_dispatch_14:\n"
"    mov w1, w0                                \n"
"    mov x0, x9                                \n"
"    stp w12, w13, [x9, #-224]                 \n"
"    stp w14, w15, [x9, #-216]                 \n"
"    stp w16, w17, [x9, #-208]                 \n"
"    str w18, [x28, #8968]                     \n"
"    str x30, [x28, #9064]                     \n"
"    mrs x2, nzcv                              \n"
"    str w2, [x28, #9044]                      \n"
"    str w12, [x28, #8848]                     \n"
"    bl   bus_read8_slow\n"
"    add x9, x28, #0x3d0                       \n"
"    add x9, x9, #0x2, lsl #12                 \n"
"    ldp x10, x11, [x9, #-120]                 \n"
"    ldp w12, w13, [x9, #-224]                 \n"
"    ldp w14, w15, [x9, #-216]                 \n"
"    ldp w16, w17, [x9, #-208]                 \n"
"    ldr w18, [x28, #8968]                     \n"
"    ldr x30, [x28, #9064]                     \n"
"    ldr w2, [x28, #9044]                      \n"
"    msr nzcv, x2                              \n"
"    sxtb w0, w0                               \n"
"    ret                                       \n"
"L_jit_scheduler_return_dispatch_15:\n"
"    orr w1, wzr, w0, lsr #11                  \n"
"    and w0, w0, #0xfffffffe                   \n"
"    ldr x1, [x9, w1, uxtw #3]                 \n"
"    orr x1, xzr, x1, lsl #2                   \n"
"    cbz x1, L_jit_scheduler_return_dispatch_16                           \n"
"    ldrh w0, [x1, w0, uxtw]                   \n"
"    ret                                       \n"
"L_jit_scheduler_return_dispatch_16:\n"
"    mov w1, w0                                \n"
"    mov x0, x9                                \n"
"    stp w12, w13, [x9, #-224]                 \n"
"    stp w14, w15, [x9, #-216]                 \n"
"    stp w16, w17, [x9, #-208]                 \n"
"    str w18, [x28, #8968]                     \n"
"    str x30, [x28, #9064]                     \n"
"    mrs x2, nzcv                              \n"
"    str w2, [x28, #9044]                      \n"
"    str w12, [x28, #8848]                     \n"
"    bl   bus_read16_slow\n"
"    add x9, x28, #0x3d0                       \n"
"    add x9, x9, #0x2, lsl #12                 \n"
"    ldp x10, x11, [x9, #-120]                 \n"
"    ldp w12, w13, [x9, #-224]                 \n"
"    ldp w14, w15, [x9, #-216]                 \n"
"    ldp w16, w17, [x9, #-208]                 \n"
"    ldr w18, [x28, #8968]                     \n"
"    ldr x30, [x28, #9064]                     \n"
"    ldr w2, [x28, #9044]                      \n"
"    msr nzcv, x2                              \n"
"    uxth w0, w0                               \n"
"    ret                                       \n"
"L_jit_scheduler_return_dispatch_17:\n"
"    orr w1, wzr, w0, lsr #11                  \n"
"    and w0, w0, #0xfffffffe                   \n"
"    ldr x1, [x9, w1, uxtw #3]                 \n"
"    orr x1, xzr, x1, lsl #2                   \n"
"    cbz x1, L_jit_scheduler_return_dispatch_18                           \n"
"    ldrsh w0, [x1, w0, uxtw]                  \n"
"    ret                                       \n"
"L_jit_scheduler_return_dispatch_18:\n"
"    mov w1, w0                                \n"
"    mov x0, x9                                \n"
"    stp w12, w13, [x9, #-224]                 \n"
"    stp w14, w15, [x9, #-216]                 \n"
"    stp w16, w17, [x9, #-208]                 \n"
"    str w18, [x28, #8968]                     \n"
"    str x30, [x28, #9064]                     \n"
"    mrs x2, nzcv                              \n"
"    str w2, [x28, #9044]                      \n"
"    str w12, [x28, #8848]                     \n"
"    bl   bus_read16_slow\n"
"    add x9, x28, #0x3d0                       \n"
"    add x9, x9, #0x2, lsl #12                 \n"
"    ldp x10, x11, [x9, #-120]                 \n"
"    ldp w12, w13, [x9, #-224]                 \n"
"    ldp w14, w15, [x9, #-216]                 \n"
"    ldp w16, w17, [x9, #-208]                 \n"
"    ldr w18, [x28, #8968]                     \n"
"    ldr x30, [x28, #9064]                     \n"
"    ldr w2, [x28, #9044]                      \n"
"    msr nzcv, x2                              \n"
"    sxth w0, w0                               \n"
"    ret                                       \n"
"    .size jit_dispatch_arm, .-jit_dispatch_arm\n"
"    .size jit_dispatch_block_cache, .-jit_dispatch_block_cache\n"
"    .size jit_block_end_switch_processor, .-jit_block_end_switch_processor\n"
"    .size jit_block_end_run_scheduler, .-jit_block_end_run_scheduler\n"
"    .size jit_scheduler_return_dispatch, .-jit_scheduler_return_dispatch\n"
);

extern void script_lua_register_dual_slot_tail(void *a0, uint64_t a1);

void jit_hook_register_dual_slot(unsigned char *machine, void *arg) {

    nds_t *nds = (nds_t *)machine;

    nds->arm7.host_x10 = arg;
    nds->arm7.host_x11 = &nds->jit_arena;

    nds->arm9.host_x10 = arg;
    nds->arm9.host_x11 = &nds->jit_arena;

    script_lua_register_dual_slot_tail(machine, offsetof(nds_t, arm9));
}

script_io_t script_io_state;

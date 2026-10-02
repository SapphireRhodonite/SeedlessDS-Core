#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include <stdio.h>
#include "mem_access.h"

#define CPU(p) ((arm_t *)(p))

extern unsigned int bus_read32(unsigned char *table, unsigned int dir);
extern unsigned int bus_read16(unsigned char *table, unsigned int dir);
extern uint32_t     arm_thumb_to_arm(uint32_t media, void *output);
extern void         bus_range_mark_written(unsigned char *ctx, uint32_t dir, uint32_t size);
extern void        *jit_block_lookup(void *ctx, uint32_t dir);
static void *(*core_malloc)(unsigned long);
static void *(*core_realloc)(void *, unsigned long);
static void  (*core_free)(void *);
static int   (*core_fflush)(void *);

static void *malloc_core(unsigned long n)
{
    if (!core_malloc) core_malloc = (void *(*)(unsigned long))sym_libc_malloc;
    return core_malloc(n);
}

static void *realloc_core(void *p, unsigned long n)
{
    if (!core_realloc) core_realloc = (void *(*)(void *, unsigned long))sym_libc_realloc;
    return core_realloc(p, n);
}

static void free_core(void *p)
{
    if (!core_free) core_free = (void (*)(void *))sym_libc_free;
    core_free(p);
}

static void fflush_core(void *f)
{
    if (!core_fflush) core_fflush = (int (*)(void *))sym_libc_fflush;
    core_fflush(f);
}



#define EXTS1(v) ((uint32_t)(0u - ((uint32_t)(v) & 1u)))
#define SAR31(v) ((uint32_t)(0u - ((uint32_t)(v) >> 31)))

static const uint32_t TAB_COND[8] = {
    0x40000000u, 0x20000000u, 0x80000000u, 0x10000000u,
    0x60000000u, 0x90000000u, 0xd0000000u, 0x00000000u
};
typedef long (*fn_page)(void *obj, void *entry, uint32_t dir);

uint32_t *jit_block_analyze(void *ctx_v, uint32_t dir, uint32_t thumb_arg)
{

    unsigned char *ctx = (unsigned char *)ctx_v;
    jit_block_analysis_t d;

    uint32_t w0, w1, w3, w8, w9, w10, w11, w12, w13, w14, w15, w16, w17;
    uint32_t w19, w20, w22, w23, w25, w28;
    uint64_t x10u, x11u, x19u, ri, x27;
    jit_instruction_record_t *pr, *p10;
    jit_link_record_t *x13p;
    uint32_t *result;

    uint32_t v210c = CPU(ctx)->is_arm9;

    w19 = dir >> 25;
    d.arena    = CPU(ctx)->jit_arena;
    d.pc     = dir;
    d.options = (uint8_t)((v210c == 1) ? 8 : 0);

    if (w19 != 0 && (dir < 0xffff0000u || v210c != 1)) {
        bus_region_t *table = CPU(ctx)->pagetable.region;
        void         *obj   = CPU(ctx)->pagetable.bus;
        bus_region_t *ent   = &table[dir >> BUS_REGION_SHIFT];
        fn_page f = (fn_page)ent->bitmap_lookup[0];
        if (f(obj, ent, dir) == 0) {

            free_core(NULL);
            return (uint32_t *)0;
        }
    }

    jit_instruction_record_t *buf = (jit_instruction_record_t *)malloc_core(0x10 * sizeof(jit_instruction_record_t));
    uint64_t cap = 0x10;
    unsigned char *ents = d.arena->record_cursor;
    uint32_t v210c_b;
    uint32_t mode, thumb_b;
    unsigned char *table_mem = (unsigned char *)&CPU(ctx)->pagetable;
    uint32_t prev_wr;
    uint32_t thumb;

    d.link_count = 0;
    d.end_kind   = 0;
    d.records = buf;
    d.links = (jit_link_record_t *)ents;
    v210c_b = CPU(ctx)->is_arm9;
    thumb_b = (thumb_arg != 0);
    mode    = (v210c_b == 1 && w19 == 0) ? 0u : 2u;
    if ((dir >> 24) == 2) mode = 1;

    x27 = 0;
    w25 = 0;
    w28 = 0;
    ri = 0;
    w22 = 0xf;
    w23 = dir;
    prev_wr = 0;
    w20 = 0xf0000000u;
    pr  = &d.records[ri];
    thumb = thumb_arg;
    w0 = w1 = w3 = w8 = w9 = w10 = w11 = w12 = w13 = 0;
    w14 = w15 = w16 = w17 = 0;
    if (thumb != 0) goto L36738;

L_1:
    w0 = bus_read32(table_mem, w23);
    w8 = w23 + 8;
    w23 = w23 + 4;
    w28 = (w0 != 0) ? 0u : w28 + 1u;
    pr->word = (w0);
    pr->pc_value = (w8);
    if (w22 == 0xf) goto L36794;
    goto L36700;

L36738:
    {
        uint32_t media = bus_read16(table_mem, w23);
        uint32_t aux = 0;
        uint32_t media16 = media & 0xffff;
        w28 = (media16 != 0) ? 0u : w28 + 1u;
        w19 = w23 + 2;
        w0 = arm_thumb_to_arm(media16, &aux);
        pr->word = (w0);
        w8 = aux;
        w9 = w23 + 4;
        w23 = w19;
        thumb = thumb_arg;
        w10 = w9 & 0xfffffffdu;
        w8  = (w8 == 0) ? w9 : w10;
        pr->pc_value = (w8);
        if (w22 != 0xf) goto L36700;
        goto L36794;
    }

L36700:
    w9 = w0 >> 29;
    if (w9 != (w22 >> 1)) goto L36794;
    if (w22 == (w0 >> 28)) {
        pr->word = (0xe1a00000u);
        w0 = 0xe1a00000u;
        goto L36794;
    }
    w9 = (w0 & 0x0fffffffu) | 0xe0000000u;
    w0 = w9;
    pr->word = (w9);

L36794:
    pr->emit_flags = 0;
    pr->cycles = 1;
    pr->live_flags = 0;
    pr->live_registers = (0);
    pr->writes_pc  = 0;
    w9  = TAB_COND[w0 >> 29];
    w10 = (w0 >> 25) & 7;
    w14 = w0 >> 28;
    w1  = (w0 >> 16) & 0xf;
    w17 = (w0 >> 12) & 0xf;
    w12 = (w0 >> 21) & 0xf;
    w15 = EXTS1(w0 >> 19);
    w11 = (w0 >> 20) & 1;
    w13 = w0 << 10;
    switch (w10) {
    case 0:  goto L_23;
    case 1:  goto L36814;
    case 2:
    case 3:  goto L_2;
    case 4:  goto L_54;
    case 5:  goto L_61;
    case 6:  goto L37140;
    default: goto L36948;
    }

L_2:
    if (w14 != 0xf) goto L36938;
L_3:
    w11 = 0; w14 = 0; w15 = 0; w12 = 0; w10 = 0; w13 = 1;
    goto L_67;

L36938:
    if (w0 & (1u << 25)) goto L_4;
    w10 = 0; w11 = 1;
    goto L_5;

L_4:
    w11 = 1;
    if (w0 & (1u << 4)) goto L_7;
    w10 = w0 & 0xf;
    w10 = w11 << w10;
L_5:
    w12 = 0x1200000u;
    w11 = w11 << w1;
    w8  = (0x1200000u & ~w0);
    w10 = w10 | w11;
    w12 = (w8 == 0) ? w11 : 0u;
    w11 = 0;
    if (w0 & (1u << 20)) goto L_6;
    w16 = 4; w14 = 0; w15 = 0; w13 = 1;
    pr->emit_flags = 4;
    w16 = w13 << w17;
    w10 = w10 | w16;
    goto L_67;

L_6:
    w13 = 8;
    pr->emit_flags = 8;
    w13 = 1; w16 = 0; w14 = 0; w15 = 0;
    w17 = w13 << w17;
    goto L_60;

L_7:
    if (thumb == 0) goto L37140;
    w12 = 0xc000; w16 = 0x4000;
    w11 = 0; w14 = 0; w15 = 0;
    w10 = 0x8000; w13 = 1;
    w12 = ((w0 & 0x10000u) == 0) ? w16 : w12;
    goto L_67;

L36814:
    w10 = w0 & 0x1900000u;
    if (w10 != 0x1000000u) goto L_12;
    if ((~w0) & 0xf000u) goto L37140;
L36834:
    w10 = w0 & 0xf;
    w13 = 1;
    w10 = w13 << w10;
    if (w0 & (1u << 22)) goto L_9;
    w15 = w15 & 0xf0000000u;
    w11 = 0; w14 = 0;
    if (!(w0 & (1u << 16))) goto L_11;
    w12 = 0;
    w10 = w10 | 0x7f00u;
L_8:
    w13 = 4;
    pr->emit_flags = 4;
    w13 = 1;
    goto L_67;

L_9:
    w11 = 0; w14 = 0; w15 = 0; w12 = 0;
    goto L_67;

L_10:
    w11 = 0; w14 = 0; w15 = 0;
L_11:
    w12 = 0; w13 = 1;
    goto L_67;

L_12:

    w13 = w0 & 0x1800000u;
    w14 = (((w0 & 0x100000u) != 0) && ((w0 & 0xf00u) != 0))
              ? 0x20000000u : 0u;
    switch (w12) {
    case 0: case 1: case 12: case 14: goto L_13;
    case 2: case 3: case 4:           goto L_15;
    case 5: case 6: case 7:           goto L_16;
    case 8: case 9:                   goto L_18;
    case 10: case 11:                 goto L_19;
    default:                          goto L_20;
    }

L_13:
    w10 = w14 | 0xc0000000u;
    w15 = ((w0 & 0x100000u) == 0) ? w14 : w10;
    w10 = 1;
    w10 = w10 << w1;
    if (w17 != 0xf) goto L_14;
    w11 = 1;
    pr->writes_pc = 1;
    w12 = 0x8000;
    goto L_21;

L_14:
    w12 = 1;
    w11 = 0;
    w12 = w12 << w17;
    goto L_21;

L_15:
    w12 = 1;
    w11 = 0;
    w10 = w12 << w1;
    w12 = w12 << w17;
    goto L_17;

L_16:
    w12 = 1;
    w11 = 0;
    w10 = w12 << w1;
    w12 = w12 << w17;
    w9  = w9 | 0x20000000u;
L_17:
    w17 = 0xf0000000u;
    w15 = ((w0 & 0x100000u) == 0) ? w14 : w17;
    goto L_22;

L_18:
    w10 = 1;
    w11 = 0; w12 = 0;
    w10 = w10 << w1;
    w15 = w14 | 0xc0000000u;
    goto L_21;

L_19:
    w10 = 1;
    w11 = 0; w12 = 0;
    w10 = w10 << w1;
    w15 = 0xf0000000u;
    goto L_21;

L_20:
    w12 = 1;
    w15 = w14 | 0xc0000000u;
    w11 = 0; w10 = 0;
    w12 = w12 << w17;
    w15 = ((w0 & 0x100000u) == 0) ? w14 : w15;
    goto L_21;

L_21:
    w17 = 0xf0000000u;
L_22:

    w16 = 0x1000000u;
    if (w13 != w16 && (0x10f000u & ~w0) == 0) w15 = w17;
    w14 = 0;
    w13 = 1;
    goto L_67;

L_23:
    w16 = (w0 >> 8) & 0xf;
    if ((0x90u & ~w0) != 0) goto L_33;
    if ((w0 & 0x60u) == 0) goto L_29;
    w15 = 0; w13 = 1;
    if (!(w0 & (1u << 6))) goto L_25;
    if (w11 != 0) goto L_25;
    w13 = 1;
    w11 = 1u & ~(w0 >> 5);
    if (w14 != 0xe) goto L_24;
    w13 = 2;
    pr->cycles = 2;
L_24:
    w15 = 1;
L_25:
    w10 = 1;
    w12 = w10 << w1;
    w10 = w12;
    if (w0 & (1u << 22)) goto L_26;
    w10 = w0 & 0xf;
    w16 = 1;
    w10 = w16 << w10;
    w10 = w12 | w10;
L_26:
    w16 = 0x1200000u;
    w8  = (0x1200000u & ~w0);
    w16 = 2;
    w12 = (w8 == 0) ? w12 : 0u;
    w1  = w16 << w17;
    if (w11 == 0) goto L_27;
    w16 = 8;
    pr->emit_flags = 8;
    w16 = 1;
    w16 = w16 << w17;
    w12 = w12 | w16;
    w15 = (w15 == 0) ? 0u : w1;
    w11 = 0;
    w12 = w12 | w15;
    if (w14 != 0xe) goto L_28;
    if (w17 != 0xf) goto L_28;
    w13 = 3;
    w11 = 0; w16 = 0; w14 = 0; w15 = 0;
    pr->cycles = 3;
    goto L37160;

L_27:
    w11 = 4;
    w16 = 1;
    pr->emit_flags = 4;
    w11 = w16 << w17;
    w10 = w10 | w11;
    w11 = 0; w14 = 0;
    if (w15 == 0) goto L37160;
    w15 = 0;
    w10 = w10 | w1;
    goto L_67;

L_28:
    w16 = w11; w14 = w11; w15 = w11;
    goto L37160;

L_29:
    if (w12 > 7) goto L_32;
    w10 = w0 & 0xf;
    w12 = 1;
    w10 = w12 << w10;
    w11 = w12 << w16;
    w10 = w11 | w10;
    if (w0 & (1u << 23)) goto L_30;
    w11 = 1;
    w12 = w11 << w1;
    if (!(w0 & (1u << 21))) goto L_31;
    w11 = w11 << w17;
    w10 = w10 | w11;
    goto L_31;

L_30:
    w11 = w12 << w17;
    w12 = 1;
    w12 = w12 << w1;
    w12 = w11 | w12;
    w11 = w12 & SAR31(w13);
    w10 = w11 | w10;
L_31:
    w15 = EXTS1(w0 >> 20);
    w11 = 0; w14 = 0; w13 = 1;
    w15 = w15 & 0xc0000000u;
    goto L_67;

L_32:
    w10 = w12 | 2;
    if (w10 != 0xa) goto L37140;
    w16 = 1;
    w13 = 1;
    if (w14 == 0xe) {
        w13 = 2;
        pr->cycles = 2;
    }
    w10 = 4;
    pr->emit_flags = 4;
    w10 = w0 & 0xf;
    w12 = w16 << w1;
    w10 = w16 << w10;
    w11 = 0; w14 = 0; w15 = 0;
    w10 = w12 | w10;
    w12 = w16 << w17;
    goto L37160;

L_33:
    w10 = w0 & 0x1900000u;
    w11 = (w0 >> 5) & 3;
    if (w10 != 0x1000000u) goto L_44;
    if (w0 & (1u << 7)) goto L_34;
    if (w0 & (1u << 4)) goto L_39;
    if (w0 & (1u << 21)) goto L36834;
    w13 = 1;
    w11 = 0; w14 = 0; w15 = 0; w10 = 0;
    w12 = w13 << w17;
    w9  = ((w0 & 0x400000u) == 0) ? w20 : w9;
    goto L_67;

L_34:
    if (v210c_b != 1) goto L37140;
    w15 = 1;
    w11 = w15 << w16;
    w10 = w0 & 0xf;
    w12 = (w0 >> 21) & 3;
    w10 = w15 << w10;
    w10 = w11 | w10;
    w12 = w15 << w1;

    switch ((w0 >> 21) & 3) {
    case 1:  goto L37094;
    case 2:  goto L_38;
    default: goto L_35;
    }

L_35:
    w13 = 1;
    if (w0 & (1u << 22)) goto L_37;
L_36:

    w11 = 0; w14 = 0; w15 = 0;
    w16 = w13 << w17;
    w10 = w10 | w16;
    goto L_67;

L37094:
    w13 = 1;
    if (!(w0 & (1u << 5))) goto L_36;
L_37:
    w11 = 0; w14 = 0; w15 = 0;
    goto L_67;

L_38:
    w13 = 1;
    w16 = w13 << w17;
    w11 = 0; w14 = 0; w15 = 0;
    w12 = w12 | w16;
    goto L_67;

L_39:

    switch (w11 & 3) {
    case 1:  goto L_41;
    case 2:  goto L_42;
    case 3:  goto L_43;
    default: goto L_40;
    }

L_40:
    if (w0 & (1u << 22)) goto L37414;
    w10 = w0 & 0xf;
    w13 = 1;
    w11 = 0; w14 = 0; w15 = 0;
    w12 = 0x8000;
    w10 = w13 << w10;
    goto L_67;

L37414:
    w13 = 1;
    w11 = 0; w14 = 0; w15 = 0;
    if (v210c_b != 1) goto L37454;
    w10 = w0 & 0xf;
    w12 = w13 << w17;
    w10 = w13 << w10;
    goto L_67;

L37454:
    w12 = 0; w10 = 0;
    goto L_67;

L_41:
    if (v210c_b != 1) goto L37140;
    w10 = w0 & 0xf;
    w13 = 1;
    w10 = w13 << w10;
    w11 = 0; w14 = 0; w15 = 0;
    w12 = 0xc000;
    w10 = w10 | 0x8000u;
    goto L_67;

L_42:
    if (v210c_b != 1) goto L37140;
    w10 = w0 & 0xf;
    w13 = 1;
    w10 = w13 << w10;
    w16 = w13 << w1;
    w11 = 0; w14 = 0; w15 = 0;
    w12 = w13 << w17;
    w10 = w16 | w10;
    goto L_67;

L_43:
    if (v210c_b != 1) goto L37140;
    if (w14 == 0xe) goto L_3;
    goto L37140;

L_44:
    w10 = w0 & 0xf;
    w13 = 1;
    w15 = w0 & 0x1800000u;
    w10 = w13 << w10;
    if (w0 & (1u << 4)) goto L_45;
    w14 = (w0 >> 7) & 0x1f;
    w12 = w11 | w14;

    w14 = ((w12 != 0) && ((w0 & 0x100000u) != 0))
              ? 0x20000000u : 0u;

    if (((w0 >> 7) & 0x1f) == 0 && w11 == 3) w9 = w9 | 0x20000000u;
    w13 = 1;
    w3  = 1;

    switch ((w0 >> 21) & 0xf) {
    case 0: case 1: case 12: case 14: goto L_48;
    case 2: case 3: case 4:           goto L_50;
    case 5: case 6: case 7:           goto L_51;
    case 8: case 9:                   goto L37008;
    case 10: case 11:                 goto L37020;
    default:                          goto L_53;
    }

L_45:
    if (w0 & (1u << 20)) goto L_46;
    w13 = 1;
    goto L_47;

L_46:
    w13 = 2;
    w9  = w9 | 0x20000000u;
    pr->cycles = 2;
L_47:
    w3  = 1;
    w11 = w3 << w16;
    w14 = 0;
    w10 = w11 | w10;
    switch ((w0 >> 21) & 0xf) {
    case 0: case 1: case 12: case 14: goto L_48;
    case 2: case 3: case 4:           goto L_50;
    case 5: case 6: case 7:           goto L_51;
    case 8: case 9:                   goto L37008;
    case 10: case 11:                 goto L37020;
    default:                          goto L_53;
    }

L_48:
    w11 = w3 << w1;
    w12 = w14 | 0xc0000000u;
    w16 = ((w0 & 0x100000u) == 0) ? w14 : w12;
    w10 = w10 | w11;
    if (w17 != 0xf) goto L_49;
    w11 = 1;
    pr->writes_pc = 1;
    w12 = 0x8000;
    goto L37034;

L_49:
    w11 = 0;
    w12 = w3 << w17;
    goto L37034;

L_50:
    w11 = 0;
    w16 = w3 << w1;
    w12 = w3 << w17;
    goto L_52;

L_51:
    w11 = 0;
    w16 = w3 << w1;
    w12 = w3 << w17;
    w9  = w9 | 0x20000000u;
L_52:
    w1  = 0xf0000000u;
    w10 = w10 | w16;
    w16 = ((w0 & 0x100000u) == 0) ? w14 : w1;
    goto L37038;

L_53:
    w16 = w14 | 0xc0000000u;
    w11 = 0;
    w12 = w3 << w17;
    w16 = ((w0 & 0x100000u) == 0) ? w14 : w16;
    goto L37034;

L37008:
    w16 = w3 << w1;
    w11 = 0; w12 = 0;
    w10 = w10 | w16;
    w16 = w14 | 0xc0000000u;
    goto L37034;

L37020:
    w14 = w3 << w1;
    w11 = 0; w12 = 0;
    w10 = w10 | w14;
    w16 = 0xf0000000u;
L37034:
    w1  = 0xf0000000u;
L37038:

    w17 = 0x1000000u;
    w15 = (w15 != w17 && (0x10f000u & ~w0) == 0) ? w1 : w16;
    w14 = 0;
    goto L_67;

L_54:
    {
        const unsigned char *pop = arm_popcount_table;
        uint32_t n;
        w17 = w0 & 0xffff;
        x10u = (uint64_t)(w0 & 0xff);
        w10 = pop[x10u];
        x11u = (uint64_t)w17 >> 8;
        w12 = pop[x11u];
        n = w12 + w10;
        w15 = n;
        w10 = 1;
        w10 = w10 << w1;
        w12 = w10 & SAR31(w13);
        if (n == 0) goto L_57;
        if (w14 != 0xe) goto L_56;
        w13 = (w15 == 1) ? 2u : w15;
        if (!(w0 & (1u << 15))) goto L_55;
        if (w11 == 0) goto L_55;
        w13 = w13 + 1;
        w11 = 0;
        pr->cycles = (uint8_t)w13;
        goto L_59;
    }

L_55:
    w11 = 0;
    pr->cycles = (uint8_t)w13;
    if (!(w0 & (1u << 20))) goto L_58;
    goto L_59;

L_56:
    w11 = 0; w13 = 1;
    if (!(w0 & (1u << 20))) goto L_58;
    goto L_59;

L_57:
    w11 = 1;
    pr->writes_pc = 1;
    w13 = 1;
    if (w0 & (1u << 20)) goto L_59;
L_58:
    w16 = 4; w14 = 0; w15 = 0;
    pr->emit_flags = 4;
    w10 = w10 | w17;
    goto L_67;

L_59:
    w16 = 0; w14 = 0; w15 = 0;
    w1  = 8;
    w17 = ((w0 & 0x400000u) == 0) ? w17 : 0u;
    pr->emit_flags = 8;
L_60:
    w12 = w12 | w17;
    goto L37160;

L_61:
    w11 = w0 << 8;
    w10 = (uint32_t)((int32_t)w11 >> 6);
    if (w14 != 0xf) goto L_62;
    w12 = thumb_b;
    w11 = (w0 >> 23) & 2;
    w10 = w10 + 4;
    w11 = w11 | w12;
    w10 = w11 | w10;
    w13 = w10 ^ 1u;
    w12 = 0xc000;
    goto L_63;

L_62:
    w11 = (uint32_t)((int32_t)w11 >> 7);
    w10 = w10 + 4;
    w11 = w11 + 2;
    w10 = (thumb != 0) ? w11 : w10;
    w11 = thumb_b;
    w12 = (w0 >> 10) & 0x4000u;
    w12 = w12 | 0x8000u;
    w13 = w10 | w11;
L_63:
    w11 = 0; w15 = 0; w10 = 0;
    w14 = w13 + w23;
    w13 = 1;
    goto L_67;

L36948:
    if (w0 & (1u << 24)) goto L_65;
    if (v210c_b != 1) goto L37140;
    if (!(w0 & (1u << 4))) goto L37140;
    w13 = 1;
    w10 = w13 << w17;
    if (w0 & (1u << 20)) goto L_64;
    w11 = w0 & 0x00e00f00u;
    if (w11 != 0xf00u) goto L_10;
    w12 = w0 & 0xefu;
    if (w1 != 7) goto L_66;
    w13 = 1;
    if (w12 != 0x80u) {
        if (w12 != 0x48u) goto L_9;
    }

    w11 = 0; w14 = 0; w15 = 0;
    pr->emit_flags = (uint8_t)w13;
    w10 = w10 | 0x8000u;
    w12 = 0x8000;
    goto L_67;

L_64:
    w11 = 0; w14 = 0; w15 = 0;
    w16 = 1;
    w12 = w10;
    w10 = 0;
    goto L37160;

L_65:
    w11 = 0; w14 = 0; w15 = 0;
    w10 = 0x8000;
    w12 = 0xc000;
    w13 = 1;
    goto L_67;

L_66:
    if (w12 == 1 && w1 == 9) goto L37080;
    w11 = 0;
    w13 = 1;
    if (w12 != 0) goto L37440;
    if (w1 != 1) goto L37440;
L37080:
    w11 = 0; w14 = 0; w15 = 0; w12 = 0;
    goto L_8;

L37440:
    w16 = 1;
    w14 = w11; w15 = w11; w12 = w11;
    goto L37160;

L37140:
    w11 = 1;
    w14 = 0; w15 = 0; w12 = 0; w10 = 0;
    w13 = 1;
    pr->writes_pc = 1;
L_67:
    w16 = 1;
L37160:
    w15 = w15 >> 28;
    w9  = (w15 << 4) | (w9 >> 28);
    w15 = prev_wr;
    w1  = w12 & 0xffff;
    w20 = (w16 != 0) ? 0u : w1;
    pr->reads = ((uint16_t)w10);
    pr->writes = ((uint16_t)w12);
    pr->flag_masks = (uint8_t)w9;
    pr->successor = 0;
    if ((w15 & w10) != 0) {
        w10 = w13 + 1;
        pr->cycles = (uint8_t)w10;
    }
    if (thumb == 0) goto L_68;
    if ((w0 & 0x0e000010u) != 0x06000010u) goto L_68;
    if (w0 & (1u << 16)) goto L37320;
    w25 = 2;
    goto L_69;

L_68:
    if (w25 == 0) goto L_70;
L_69:
    w25 = w25 - 1;
    if (w28 == 4) goto L37644;
    goto L_71;

L_70:
    w25 = 0;
    if (w28 == 4) goto L37644;

L_71:
    if ((w23 >> 25) != 0) goto L37210;
    if (thumb != 0) goto L37210;
    if (CPU(ctx)->is_arm9 != 1) goto L37210;
    {
        uint32_t k = (w23 >> 2) & 0x1fff;
        if (CPU(ctx)->jit_arena->itcm_arm_hits[k] >= JIT_ITCM_HOT_BLOCK) goto L_74;
    }
L37210:
    if (w11 != 0) goto L37460;
    if (!(w12 & 0x8000u)) goto L37378;
    w10 = w0 >> 29;
    if (w14 == 0) goto L_73;
    {
        uint32_t v = CPU(ctx)->is_arm9;
        uint32_t t = (w14 - 0x800000u) >> 23;
        if (t <= 2 && v == 0) goto L37460;

        w12 = w14 >> 25;
        w13 = w14 >> 24;
        {
            jit_link_record_t *e = d.links;
            uint32_t is_mode2 = (mode == 2);
            uint16_t n = d.link_count;
            jit_link_record_t *ent;
            uint32_t w13b, type, mark;

            w8 = (v == 1 && w12 == 0) ? 0u : 2u;
            type = (w13 == 2) ? 1u : w8;
            w13b = w0 >> 28;

            if (w23 > w14 && w10 < 7) w22 = w13b;

            ent = &e[n];
            mark = (type == 2);
            ent->flags = 0;
            ent->target_pc = w14;
            ent->source_pc = w23;
            ent->instruction_index = (uint16_t)x27;
            ent->record_index = 0;
            d.link_count = (uint16_t)(n + 1);

            if (mode != type) {
                if ((is_mode2 | mark) != 0) ent->flags = 4;
            }
            if (type != 0) goto L37370;
            if (mode != 0) goto L_72;
            if (w14 & 1) goto L37370;
            {
                uint32_t k2 = (w14 >> 2) & 0x1fff;
                if (CPU(ctx)->jit_arena->itcm_arm_hits[k2] < JIT_ITCM_HOT_BLOCK) goto L37370;
            }
L_72:
            ent->flags = 2;
            if (w10 <= 6) goto L37378;
            goto L_75;
        }
    }

L_73:
    w11 = ~w12;
    w12 = ~w9;
    pr->live_registers = ((uint16_t)w11);
    pr->live_flags = (uint8_t)((w12 >> 4) & 0xf);
L37370:
    if (w10 > 6) goto L_75;

L37378:
    w8 = w9 & 0xff;
    if (w8 > 0xf) w22 = 0xf;
    x19u = x27 + 1;
    if (x19u == (uint64_t)(uint32_t)cap) {
        cap = (uint32_t)(cap << 1);
        d.records = (jit_instruction_record_t *)realloc_core(d.records, (unsigned long)(cap << 5));
    }
    if (x27 == 0x7ff) goto L_76;
    ri = ri + 1;
    x27 = x19u;
    prev_wr = w20;
    thumb = thumb_arg;
    w20 = 0xf0000000u;
    pr = &d.records[ri];
    if (thumb != 0) goto L36738;
    goto L_1;

L37320:
    if (w25 == 0) goto L_70;
    p10 = &d.records[ri];
    w13 = p10[-1].word;
    w13 = w13 << 6;
    w13 = (w13 & ~0x7ffu) | ((w0 >> 5) & 0x7ffu);
    w13 = w13 << 10;
    w13 = w23 + (uint32_t)((int32_t)w13 >> 9);
    if (w0 & (1u << 17)) {
        w14 = w13 | 1u;
        w13 = (uint32_t)(0u - (uint32_t)(uintptr_t)pr);
        w15 = 0xfffffeu;
        w13 = w15 + (w13 >> 2);
        w0  = 0xeb000000u;
        w0  = (w0 & ~0xffffffu) | (w13 & 0xffffffu);
    } else {
        w14 = w13 & 0xfffffffcu;
        w0  = 0xfa000000u;
    }
    w13 = 0xe1a00000u;
    pr->word = (w0);
    p10[-1].word = w13;
    w25 = w25 - 1;
    if (w28 == 4) goto L37644;
    goto L_71;

L37460:
    d.end_kind = 2;
    w8 = (ri != 0) ? (uint32_t)x27 : 1u;
    goto L_77;

L37644:
    d.end_kind = 2;
    w8 = (uint32_t)x27 - 2u;
    goto L_77;

L_74:
    d.end_kind = 4;
L_75:
    if (x27 != 0x7ff) {

        w8 = (uint32_t)(x27 + 1);
        if (d.end_kind == 0) goto L_78;
        goto L_77;
    }
L_76:
    d.end_kind = 1;
    w8 = 0x800;

L_77:
    if (w8 == 0) goto L_78;
    {
        jit_instruction_record_t *r = &d.records[w8 - 1];
        r->live_flags = 0xf;
        r->live_registers = 0x7fff;
    }
L_78:
    {
        uint32_t s = (thumb == 0);
        d.instruction_count  = (uint16_t)w8;
        d.halfword_count = (uint16_t)(w8 << s);
    }
    {
        unsigned char *cur = d.arena->record_cursor;
        unsigned char *updated = cur + (uint64_t)d.link_count * JIT_RECORD_SIZE;
        unsigned char *lim = d.arena->records + sizeof d.arena->records;
        d.arena->record_cursor = updated;
        if (updated >= lim) {
            fflush_core(stdout);
        }
    }
    w8 = d.link_count;
    if (w8 == 0) goto L_79;

    {
        jit_link_record_t *x10p = d.links;
        uint32_t dir0 = d.pc;
        uint32_t best = 0;
        uint32_t dist = 0xffffffffu;
        uint32_t n = w8;
        x13p = x10p;
        for (;;) {
            uint32_t dst = x13p->target_pc;
            uint32_t d0  = dst & 0xfffffffeu;
            uint32_t delta = d0 - dir0;
            if ((int32_t)delta >= 1) {
                uint32_t pcb = x13p->source_pc;
                if (d0 < pcb && delta < dist) {
                    best = d0; dist = delta;
                }
            }
            n = n - 1;
            x13p = x13p + 1;
            if (n == 0) break;
        }
        if (dist == 0xffffffffu) goto L_79;
        {
            uint32_t d1 = dist >> 1;
            uint32_t d2 = dist >> 2;
            uint32_t nins = (thumb == 0) ? d2 : d1;
            uint32_t i = 0;
            d.halfword_count = (uint16_t)d1;
            d.instruction_count  = (uint16_t)nins;
            d.end_kind    = 3;
            for (;;) {
                if (x10p->source_pc > best) break;
                i = i + 1;
                if (w8 == i) { i = w8; break; }
                x10p = x10p + 1;
            }
            {
                unsigned char *cur = d.arena->record_cursor;
                uint32_t nent = i + 1;
                uint32_t excess = w8 - nent;
                unsigned char *updated = cur - (uint64_t)excess * JIT_RECORD_SIZE;
                uint32_t type;
                d.arena->record_cursor = updated;
                d.link_count = (uint16_t)nent;
                type = x10p->flags;
                x10p->instruction_index = (uint16_t)(nins - 1);
                type = type | 1u;
                x10p->source_pc = best;
                x10p->flags = type;
                x10p->target_pc = best | thumb;
            }
            w11 = nins;
            goto L_80;
        }
    }

L_79:
    w11 = d.instruction_count;
L_80:
    {
        jit_instruction_record_t *nb = (jit_instruction_record_t *)realloc_core(d.records,
                                (unsigned long)((uint64_t)(w11 & 0xffff) << 5));
        uint32_t nent = d.link_count;
        jit_link_record_t *x20p = d.links;
        jit_instruction_record_t *x22p = nb;
        d.records = nb;

        if (nent == 0) goto L37730;
        if ((x20p->target_pc & 0xfffffffeu) != d.pc) goto L37730;
        {

            uint64_t x10b = x20p->instruction_index;
            uint64_t x8b  = x10b + 1;
            uint32_t acc = 0;
            uint64_t pair;

            if (x8b >= 2) {
                uint32_t acc2 = 0;
                jit_instruction_record_t *p = x22p + 1;
                uint64_t k;
                pair = x8b & 0x1fffe;
                k = pair;
                do {
                    acc  |= p[-1].writes;
                    acc2 |= p[0].writes;
                    p += 2;
                    k -= 2;
                } while (k != 0);
                acc = acc2 | acc;
                if (x8b == pair) goto Ltail;
            } else {
                pair = 0;
                acc = 0;
            }
            {
                uint64_t rest = x10b - pair + 1;
                jit_instruction_record_t *p = x22p + pair;
                do {
                    acc |= p->writes;
                    p += 1;
                    rest--;
                } while (rest != 0);
            }
Ltail:
            {
                uint32_t live = (~acc) & 0x7fffu;
                jit_instruction_record_t *p = x22p;
                uint64_t n = x8b;
                uint32_t class = p->emit_flags;
                if (class & 4) goto L37730;
                for (;;) {
                    uint32_t reads = p->reads;
                    if ((reads & ~live) != 0) {
                        if (acc & reads) goto L37730;
                        n--; p += 1;
                        if (n == 0) break;
                        class = p->emit_flags;
                        if (class & 4) goto L37730;
                        continue;
                    }
                    if ((p->word >> 29) < 7) {
                        n--; p += 1;
                        if (n == 0) break;
                        class = p->emit_flags;
                        if (class & 4) goto L37730;
                        continue;
                    }
                    live |= p->writes;
                    n--; p += 1;
                    if (n == 0) break;
                    class = p->emit_flags;
                    if (class & 4) goto L37730;
                }
                d.options = (uint8_t)(d.options | 4);
            }
        }
    }

L37730:
    {
        jit_link_record_t *x20p = d.links;
        jit_instruction_record_t *x22p = d.records;
        unsigned char *x25p;
        uint32_t w23b = (uint32_t)d.halfword_count << 1;
        uint32_t w25b = d.pc;
        uint32_t nent = d.link_count;
        uint32_t i;
        uint32_t shift;

        bus_range_mark_written(ctx, w25b, w23b);

        {
            uint32_t c = d.arena->block_pc_count;
            d.arena->block_pc[c] = thumb | w25b;
            d.arena->block_pc_count = c + 1;
        }
        if (nent == 0) goto L37974;

        x25p = (unsigned char *)CPU(ctx)->jit_arena;
        shift = (thumb == 0) ? 2u : 1u;
        i = 0;

        for (;;) {
            uint32_t class = (uint8_t)x20p->flags;
            uint32_t irec  = x20p->instruction_index;
            if (class & 6) {
                jit_instruction_record_t *r = &x22p[irec];
                r->live_registers = 0x7fff;
                r->live_flags = 0xf;
            } else {
                uint32_t dst = x20p->target_pc;
                uint32_t delta = dst - d.pc;
                int inside = 0;
                if ((int32_t)delta >= 1 && delta < w23b) {
                    if (dst >= x20p->source_pc) inside = 1;
                }
                if (inside) {

                    uint32_t ir = delta >> shift;
                    jit_instruction_record_t *r = &x22p[irec];
                    jit_instruction_record_t *rd;
                    uint32_t m;
                    x20p->record_index = (uint16_t)ir;
                    r->successor = (uint16_t)ir;
                    rd = &x22p[ir];
                    m = (rd->reads | rd->live_registers) | r->live_registers;
                    r->live_registers = (uint16_t)m;
                    rd->emit_flags = (uint8_t)(rd->emit_flags | 2);
                } else {
                    uint32_t n1 = d.arena->block_pc_count;
                    uint64_t k;
                    int found = 0;
                    if (n1 != 0) {
                        for (k = 0; k < n1; k++) {
                            if (d.arena->block_pc[k] == dst) { found = 1; break; }
                        }
                    }
                    if (found) {

                        uint32_t c = d.arena->pending_branch_count;
                        jit_pending_branch_t *e = &d.arena->pending_branch[c];
                        jit_instruction_record_t *r = &x22p[irec];
                        e->target_pc = dst;
                        d.arena->pending_branch_count = c + 1;
                        x20p->patch_slot = &e->instruction;
                        x20p->target_code = (uint8_t *)0;
                        r->live_registers = 0x7fff;
                        r->live_flags = 0xf;
                    } else {
                        void *blk = jit_block_lookup(ctx, dst);
                        jit_instruction_record_t *r = &x22p[irec];
                        if (blk != (void *)0) {
                            uint32_t off = rd32((unsigned char *)blk - 4);
                            const jit_block_header_t *hdr = (const jit_block_header_t *)(x25p + off);
                            uint32_t m;
                            x20p->patch_slot = (uint8_t **)0;
                            m = (hdr->live_registers & 0x7fffu) | r->live_registers;
                            r->live_registers = (uint16_t)m;
                            r->live_flags = (uint8_t)(r->live_flags | (uint8_t)hdr->live_flags);
                            x20p->target_code = (uint8_t *)blk;
                        } else {

                            uint32_t c = d.arena->pending_branch_count;
                            jit_pending_branch_t *e = &d.arena->pending_branch[c];
                            e->target_pc = x20p->target_pc;
                            d.arena->pending_branch_count = c + 1;
                            x20p->patch_slot = &e->instruction;
                            x20p->target_code = (uint8_t *)0;
                            r->live_registers = 0x7fff;
                            r->live_flags = 0xf;
                        }
                    }
                }
            }
            i = i + 1;
            if (i >= (uint32_t)d.link_count) break;
            x22p = d.records;
            x20p = x20p + 1;
        }
        {
            uint32_t c = d.arena->block_pc_count;
            x22p = d.records;
            thumb = thumb_arg;
            d.arena->block_pc_count = c - 1;
        }
        goto L37980;
L37974:
        {
            uint32_t c = d.arena->block_pc_count;
            d.arena->block_pc_count = c - 1;
        }
L37980: ;
    }

    {
        jit_instruction_record_t *x22p = d.records;
        uint64_t x8b = d.instruction_count;
        uint32_t nib, regs;
        {
            jit_instruction_record_t *p = x22p + x8b;
            nib  = p[-1].flag_masks & 0xf;
            regs = p[-1].reads;
        }
        if (x8b != 0) {
            uint64_t off2 = x8b;
            for (;;) {
                jit_instruction_record_t *p = x22p + off2;
                uint32_t vregs = p[-1].live_registers;
                uint64_t sig   = p[-1].successor;
                uint32_t vnib  = p[-1].live_flags;
                uint32_t cond;
                uint32_t nib24;
                if (sig != 0) {
                    jit_instruction_record_t *q = x22p + sig;
                    uint32_t q24 = q->flag_masks;
                    uint32_t q25 = q->live_flags;
                    uint32_t q18 = q->reads;
                    uint32_t q20 = q->writes;
                    uint32_t q22 = q->live_registers;
                    q24 = q24 & ~(q24 >> 4);
                    vnib = q25 | vnib;
                    vregs = q22 | vregs;
                    vnib = (q24 & 0xf) | vnib;
                    vregs = vregs | (q18 & ~q20);
                }
                nib = vnib | nib;
                cond = p[-1].word >> 29;
                regs = regs | (vregs & 0xffff);
                nib24 = p[-1].flag_masks;
                p[-1].live_flags = (uint8_t)nib;
                p[-1].live_registers = (uint16_t)regs;
                if (cond >= 7) {
                    jit_instruction_record_t *q = x22p + off2;
                    uint32_t wr = q[-1].writes;
                    nib  = nib  & ~(nib24 >> 4);
                    regs = regs & ~wr;
                }
                {
                    jit_instruction_record_t *q = x22p + off2;
                    uint32_t reads = q[-1].reads;
                    nib  = nib | (nib24 & 0xf);
                    off2 = off2 - 1;
                    regs = regs | reads;
                    if (off2 == 0) break;
                }
            }
        }
        d.live_flags  = (uint8_t)nib;
        d.live_registers = (uint16_t)regs;

        result = jit_emit_block(&d, ctx, d.pc, thumb);
    }

    free_core(d.records);

    return result;
}
#undef EXTS1
#undef SAR31

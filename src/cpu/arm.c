#include "hires_runtime.h"
#include <stdint.h>
#include "blob_symbols.h"
#include "arm.h"
#include "core/nds_state.h"

uint8_t arm_popcount_table[ARM_POPCOUNT_TABLE_BYTES];
#define CPU(p) ((arm_t *)(p))
#include <string.h>
#include "core/sched.h"
#include "core_internals.h"
#include "mem_access.h"

void arm_hook_noop(void) {
}

static const uint32_t arm_bank_by_mode[16] = {
    0, 1, 2, 3, 6, 6, 6, 4, 6, 6, 6, 5, 6, 6, 6, 0
};

const uint32_t arm_mode_by_bank[ARM_BANK_COUNT] = {
    0x10, 0x11, 0x12, 0x13, 0x17, 0x1b, 0x1f
};

uint32_t arm_mode_to_bank(unsigned char *cpu) {

    uint32_t mode = ((arm_t *)cpu)->cpsr & 0x1f;
    uint32_t i = mode - 0x10;

    if (i <= 0xf) {
        return arm_bank_by_mode[i];
    }
    return 6;
}

void arm_bank_switch(unsigned char *cpu) {

    uint64_t mode = ((arm_t *)cpu)->cpsr & 0x1f;
    uint64_t i = mode - 0x10;
    uint32_t bank = ((uint32_t)i <= 0xf)
                   ? arm_bank_by_mode[i]
                   : 6;

    uint32_t current = CPU(cpu)->bank;
    if (current == bank) return;

    int restore;
    if (bank == ARM_BANK_FIQ) {
        unsigned char *d = (unsigned char *)CPU(cpu)->fiq_r8_r14;
        __builtin_memcpy(d, &CPU(cpu)->r[8], 16);
        __builtin_memcpy(d + 12, &CPU(cpu)->r[11], 16);
        restore = (current == ARM_BANK_FIQ);
    } else {
        CPU(cpu)->banked_sp_lr[current][0] = CPU(cpu)->r[13];
        CPU(cpu)->banked_sp_lr[current][1] = CPU(cpu)->r[14];
        restore = (current == ARM_BANK_FIQ);
    }

    if (!restore) {
        CPU(cpu)->r[13] = CPU(cpu)->banked_sp_lr[bank][0];
        CPU(cpu)->r[14] = CPU(cpu)->banked_sp_lr[bank][1];
    } else {
        unsigned char *s = (unsigned char *)CPU(cpu)->fiq_r8_r14;
        unsigned char tmp[16];
        __builtin_memcpy(tmp, s, 16);
        uint32_t v = CPU(cpu)->fiq_r8_r14[6];
        __builtin_memcpy(&CPU(cpu)->r[8], tmp, 16);
        uint64_t x = *(uint64_t *)(s + 16);
        CPU(cpu)->r[14] = v;
        *(uint64_t *)&CPU(cpu)->r[12] = x;
    }

    CPU(cpu)->bank = bank;
}

static void copy_block(unsigned char *m) {
    ((uint64_t *)&CPU(m)->r[8])[0] = ((const uint64_t *)CPU(m)->fiq_r8_r14)[0];
    ((uint64_t *)&CPU(m)->r[8])[1] = ((const uint64_t *)CPU(m)->fiq_r8_r14)[1];
    *(uint64_t *)&CPU(m)->r[12] = *(const uint64_t *)&CPU(m)->fiq_r8_r14[4];
    CPU(m)->r[14] = CPU(m)->fiq_r8_r14[6];
}

static void store_pair(unsigned char *m, uint32_t mode) {
    CPU(m)->banked_sp_lr[mode][0] = CPU(m)->r[13];
    CPU(m)->banked_sp_lr[mode][1] = CPU(m)->r[14];
}

void arm_exception_raise(unsigned char *machine, uint32_t order) {
    uint32_t cycles = CPU(machine)->pc;
    uint32_t sig = 0;
    if (cycles & 1) {
        cycles &= 0xfffffffeu;
        sig = 1;
        CPU(machine)->pc = cycles;
    }
    uint32_t cost = order << 2;

    if (order <= 7) {
        uint32_t mode = CPU(machine)->bank;
        switch (order) {
        case 0:
            if (mode != 3) {
                store_pair(machine, mode);
                if (mode == 1) copy_block(machine);
                else *(uint64_t *)&CPU(machine)->r[13] = *(const uint64_t *)CPU(machine)->banked_sp_lr[ARM_BANK_SVC];
                CPU(machine)->bank = 3;
            }
            break;
        case 1:
            if (mode != 5) {
                store_pair(machine, mode);
                if (mode == 1) copy_block(machine);
                else *(uint64_t *)&CPU(machine)->r[13] = *(const uint64_t *)CPU(machine)->banked_sp_lr[ARM_BANK_UND];
                CPU(machine)->bank = 5;
            }
            CPU(machine)->r[14] = cycles;
            break;
        case 2:
            if (mode != 3) {
                store_pair(machine, mode);
                if (mode == 1) copy_block(machine);
                else *(uint64_t *)&CPU(machine)->r[13] = *(const uint64_t *)CPU(machine)->banked_sp_lr[ARM_BANK_SVC];
                CPU(machine)->bank = 3;
            }
            CPU(machine)->r[14] = cycles;
            break;
        case 3:
            if (mode != 4) {
                store_pair(machine, mode);
                if (mode == 1) copy_block(machine);
                else *(uint64_t *)&CPU(machine)->r[13] = *(const uint64_t *)CPU(machine)->banked_sp_lr[ARM_BANK_ABT];
                CPU(machine)->bank = 4;
            }
            CPU(machine)->r[14] = cycles;
            break;
        case 4:
            if (mode != 4) {
                store_pair(machine, mode);
                if (mode == 1) copy_block(machine);
                else *(uint64_t *)&CPU(machine)->r[13] = *(const uint64_t *)CPU(machine)->banked_sp_lr[ARM_BANK_ABT];
                CPU(machine)->bank = 4;
            }
            CPU(machine)->r[14] = cycles - 4;
            break;
        case 5:
            break;
        case 6:
            if (mode != 2) {
                store_pair(machine, mode);
                if (mode == 1) copy_block(machine);
                else *(uint64_t *)&CPU(machine)->r[13] = *(const uint64_t *)CPU(machine)->banked_sp_lr[ARM_BANK_IRQ];
                CPU(machine)->bank = 2;
            }
            CPU(machine)->r[14] = cycles + 4;
            break;
        case 7:
            if (mode != 1) {

                ((uint64_t *)CPU(machine)->fiq_r8_r14)[0] = ((const uint64_t *)&CPU(machine)->r[8])[0];
                ((uint64_t *)CPU(machine)->fiq_r8_r14)[1] = ((const uint64_t *)&CPU(machine)->r[8])[1];
                for (int i = 0; i < 16; i++)
                    ((unsigned char *)&CPU(machine)->fiq_r8_r14[3])[i] = ((const unsigned char *)&CPU(machine)->r[11])[i];
                *(uint64_t *)&CPU(machine)->r[13] = *(const uint64_t *)CPU(machine)->banked_sp_lr[ARM_BANK_FIQ];
                CPU(machine)->bank = 1;
            }
            CPU(machine)->r[14] = cycles + 4;
            break;
        }
    }

    {
        uint32_t flags = ((arm_t *)machine)->cpsr;
        uint32_t updated = CPU(machine)->bank;
        uint32_t marked = (sig == 0) ? flags : (flags | 0x20);
        CPU(machine)->spsr[updated] = marked;

        static const uint32_t BY_MODE[8] =
            { 0x10, 0x11, 0x12, 0x13, 0x17, 0x1b, 0x1f, 0x00 };
        updated = CPU(machine)->bank;
        flags = ((arm_t *)machine)->cpsr;
        uint32_t output = BY_MODE[updated] | (flags & 0xffffffe0u);
        ((arm_t *)machine)->cpsr = output;

        if (((arm_t *)machine)->is_arm9 == 1)
            cost += CPU(machine)->cp15->exception_vector_base;

        output = (output & 0xffffffdfu) | 0x80;
        ((arm_t *)machine)->cpsr = output;
        CPU(machine)->pc = cost;
    }
}

void arm_irq_raise(unsigned char *machine) {
    void (*state)(unsigned char *, uint32_t) = arm_exception_raise;
    state(machine, 6);
}

void arm_halt_enter(arm_t *cpu) {
    void (*resume)(sched_t *, uint32_t) = sched_deadline_remove;

    nds_t *obj = cpu->machine;
    cpu->halt_flags = 1;
    resume(&obj->sched, SCHED_DEADLINE_CYCLE_EVENT);
}

extern void arm_exception_raise_6(unsigned char *machine, uint32_t order) __asm__("arm_exception_raise");

void arm_irq_dispatch(unsigned char *param_1) {

    if (((arm_t *)param_1)->irq_pending == 0)
        return;

    uint32_t count = ((arm_t *)param_1)->halt_flags;
    ((arm_t *)param_1)->halt_flags = 0;
    ((arm_t *)param_1)->wake_flags = 0;

    if ((int8_t)((arm_t *)param_1)->cpsr >= 0) {

        arm_exception_raise(param_1, 6);
    }

    if (count != 0 && ((arm_t *)param_1)->is_arm9 == 0) {

        if (count >= 2) {

            unsigned char *other = (unsigned char *)((arm_t *)param_1)->partner;
            uint32_t v = ((arm_t *)other)->halt_flags;
            v &= 0xfffffffdu;
            ((arm_t *)other)->halt_flags = v;
        }

        unsigned char *obj = (unsigned char *)((arm_t *)param_1)->machine;
        sched_event_insert(obj, NULL);
    }
}






static void output(unsigned char *cpu, uint32_t flags)
{
    if ((flags & 0x80u) != 0)
        return;

    if (((arm_t *)cpu)->irq_pending == 0)
        return;

    ((arm_t *)cpu)->wake_flags = (((arm_t *)cpu)->wake_flags | 8u);
    arm_exception_raise(cpu, 6u);
}

void arm_cpsr_set(unsigned char *cpu, uint32_t cpsr)
{

    uint32_t idx = (cpsr & 0x1fu) - 0x10u;
    uint32_t bank;
    uint32_t current;

    ((arm_t *)cpu)->cpsr = (cpsr);
    if (idx <= 0xfu)
        bank = arm_bank_by_mode[idx];
    else
        bank = 6u;

    current = CPU(cpu)->bank;
    if (current == bank) {
        output(cpu, cpsr);
        return;
    }

    if (bank == 1u) {
        unsigned char tmp[16];

        memcpy(tmp, &CPU(cpu)->r[8], sizeof(tmp));
        memcpy(CPU(cpu)->fiq_r8_r14, tmp, sizeof(tmp));
        memcpy(tmp, &CPU(cpu)->r[11], sizeof(tmp));
        memcpy(&CPU(cpu)->fiq_r8_r14[3], tmp, sizeof(tmp));
    } else {
        CPU(cpu)->banked_sp_lr[current][0] = CPU(cpu)->r[13];
        CPU(cpu)->banked_sp_lr[current][1] = CPU(cpu)->r[14];
    }

    if (current == 1u) {
        unsigned char tmp[16];
        unsigned char *origin = (unsigned char *)CPU(cpu)->fiq_r8_r14;
        uint32_t value;
        uint64_t pair;

        memcpy(tmp, origin, sizeof(tmp));
        value = CPU(cpu)->fiq_r8_r14[6];
        memcpy(&CPU(cpu)->r[8], tmp, sizeof(tmp));
        pair = rd64(origin + 16);
        CPU(cpu)->r[14] = value;
        wr64(&CPU(cpu)->r[12], pair);
    } else {
        CPU(cpu)->r[13] = CPU(cpu)->banked_sp_lr[bank][0];
        CPU(cpu)->r[14] = CPU(cpu)->banked_sp_lr[bank][1];
    }

    CPU(cpu)->bank = bank;
    output(cpu, ((arm_t *)cpu)->cpsr);
}


uint32_t arm_cpsr_restore_from_spsr(unsigned char *state) {

    uint32_t cpsr = ((arm_t *)state)->cpsr;

    if ((cpsr & 0x1f) != 0x10) {
        uint32_t idx = CPU(state)->bank;
        uint32_t value = CPU(state)->spsr[idx];

        void (*change_mode)(unsigned char *, uint32_t) = arm_cpsr_set;
        change_mode(state, value);

        cpsr = ((arm_t *)state)->cpsr;
    }

    return cpsr;
}

typedef uint64_t (*jump_t)(void *arg);

uint64_t arm_slice_run(const void *param_1)
{

    jump_t dest;
    void *arg;

    dest = CPU(param_1)->run_entry;
    arg = ((const arm_t *)param_1)->machine;
    return dest(arg);
}

static void copy16(unsigned char *d, const unsigned char *o) {
    for (int i = 0; i < 16; i++) d[i] = o[i];
}

void arm_init(unsigned char *obj, unsigned char *ctx,
                        uint32_t idx, void *pair) {

    if (arm_popcount_table[ARM_POPCOUNT_TABLE_BYTES - 1u] == 0) {
        static const unsigned char popcount_rows[80] = {
        0x00, 0x01, 0x01, 0x02, 0x01, 0x02, 0x02, 0x03, 0x01, 0x02, 0x02, 0x03, 0x02, 0x03, 0x03, 0x04,
        0x01, 0x02, 0x02, 0x03, 0x02, 0x03, 0x03, 0x04, 0x02, 0x03, 0x03, 0x04, 0x03, 0x04, 0x04, 0x05,
        0x02, 0x03, 0x03, 0x04, 0x03, 0x04, 0x04, 0x05, 0x03, 0x04, 0x04, 0x05, 0x04, 0x05, 0x05, 0x06,
        0x03, 0x04, 0x04, 0x05, 0x04, 0x05, 0x05, 0x06, 0x04, 0x05, 0x05, 0x06, 0x05, 0x06, 0x06, 0x07,
        0x04, 0x05, 0x05, 0x06, 0x05, 0x06, 0x06, 0x07, 0x05, 0x06, 0x06, 0x07, 0x06, 0x07, 0x07, 0x08,
        };
        const unsigned char *k = popcount_rows;
        unsigned char *t = arm_popcount_table;

        for (int i = 0; i < 16; i++) {
            int n = __builtin_popcount((unsigned)i);
            copy16(t + i * 16, k + n * 16);
        }
    }

    CPU(obj)->jit_arena = &((nds_t *)ctx)->jit_arena;
    ((arm_t *)obj)->is_arm9 = idx;
    ((arm_t *)obj)->machine = (nds_t *)ctx;
    ((arm_t *)obj)->partner = pair;
    CPU(obj)->bus = &((nds_t *)ctx)->bus;
    CPU(obj)->gpu3d = &((nds_t *)ctx)->gpu3d;

    arm_debug_init(&CPU(obj)->debug, CPU(obj));

    CPU(obj)->timers[0].cpu = CPU(obj);
    CPU(obj)->timers[0].irq_bit = IRQ_TIMER0;
    CPU(obj)->timers[0].index = 0;

    void (*deadline_callback)(void *, void *) = timer_overflow_event;
    uint32_t id = SCHED_DEADLINE_TIMER_BASE | (idx << 2);
    sched_t *reg = &((nds_t *)ctx)->sched;

    sched_deadline_set(reg, id, deadline_callback, &CPU(obj)->timers[0]);

    CPU(obj)->timers[1].cpu = CPU(obj);
    CPU(obj)->timers[1].irq_bit = IRQ_TIMER0 << 1;
    CPU(obj)->timers[1].index = 1;
    sched_deadline_set(reg, id + 1, deadline_callback, &CPU(obj)->timers[1]);

    CPU(obj)->timers[2].cpu = CPU(obj);
    CPU(obj)->timers[2].irq_bit = IRQ_TIMER0 << 2;
    CPU(obj)->timers[2].index = 2;
    sched_deadline_set(reg, id + 2, deadline_callback, &CPU(obj)->timers[2]);

    CPU(obj)->timers[3].cpu = CPU(obj);
    CPU(obj)->timers[3].index = 3;
    CPU(obj)->timers[3].irq_bit = IRQ_TIMER0 << 3;
    sched_deadline_set(reg, id + 3, deadline_callback, &CPU(obj)->timers[3]);

    if (idx == 1) {
        CPU(obj)->itcm_arm_blocks = ((nds_t *)ctx)->jit_arena.itcm_arm_blocks;
        CPU(obj)->itcm_thumb_blocks = ((nds_t *)ctx)->jit_arena.itcm_thumb_blocks;
    }
}


void arm_reset_vector_follow(uint64_t param_1) {

    uint8_t *ctx = (uint8_t *)param_1;

    int32_t counter;
    counter = (int32_t)CPU(ctx)->pc;
    if (counter != 0)
        return;

    uint32_t uVar1 = bus_read32((unsigned char *)&CPU(ctx)->pagetable, 0);

    if (((uVar1 >> 24) & 0xffu) == 0xea) {
        int32_t shift = (int32_t)(uVar1 << 8);
        shift >>= 6;

        counter = (int32_t)CPU(ctx)->pc;
        counter = counter + shift + 8;
        CPU(ctx)->pc = (uint32_t)counter;
    }
}

typedef struct { uint64_t v; } __attribute__((packed, aligned(1))) u64d;

void arm_reset(unsigned char *o) {

    for (int i = 0; i < 4; i++) {
        unsigned char *p = (unsigned char *)&CPU(o)->r[12 - 4 * i];
        ((uint64_t *)p)[0] = 0;
        ((uint64_t *)p)[1] = 0;
    }

    uint64_t constant = 0x0000001f00000000ULL;

    CPU(o)->r[13] = NDS_MAIN_RAM_BASE + NDS_MAIN_RAM_SIZE;
    ((u64d *)&CPU(o)->pc)->v = constant;

    ((arm_t *)o)->cycle_mark = 0xFFFFFFFFu;
    ((arm_t *)o)->halt_flags = 0;
    ((u64d *)&CPU(o)->bank)->v = 0;

    for (int i = 0; i < 4; i++) {
        nds_timer_t *t = &((arm_t *)o)->timers[i];
        t->period_cycles = 65536;
        t->reload = 0;
        t->control = 0;
        t->prescaler_shift = 0;
        t->scheduled = 0;
    }

    ((arm_t *)o)->wake_flags = 0;
    CPU(o)->nzcv = 0;

    arm_debug_reset(&CPU(o)->debug);
}

static uint32_t bfi_u32(uint32_t dst, uint32_t src, int lsb, int width)
{
    uint32_t mask = ((uint32_t)1u << width) - 1u;
    return (dst & ~(mask << lsb)) | ((src & mask) << lsb);
}

static uint32_t bfxil_u32(uint32_t dst, uint32_t src, int lsb, int width)
{
    uint32_t mask = ((uint32_t)1u << width) - 1u;
    uint32_t val = (src >> lsb) & mask;
    return (dst & ~mask) | val;
}

static uint32_t ubfiz_u32(uint32_t src, int lsb, int width)
{
    uint32_t mask = ((uint32_t)1u << width) - 1u;
    return (src & mask) << lsb;
}

static int32_t sbfx_u32(uint32_t src, int lsb, int width)
{
    uint32_t mask = ((uint32_t)1u << width) - 1u;
    uint32_t val = (src >> lsb) & mask;
    uint32_t signbit = (uint32_t)1u << (width - 1);
    if (val & signbit) val |= ~mask;
    return (int32_t)val;
}

uint32_t arm_thumb_to_arm(uint32_t param_1, void *param_2)
{

    uint32_t w0 = param_1;
    uint32_t zero32 = 0;
    memcpy(param_2, &zero32, sizeof(zero32));

    uint32_t idx13 = (w0 >> 13) & 7u;

    switch (idx13) {

    case 0: {
        uint32_t t8 = (w0 >> 11) & 3u;
        if (t8 == 3) {

            uint32_t t12 = ubfiz_u32(w0, 12, 3);
            uint32_t t8b = w0 >> 3;
            t12 = bfxil_u32(t12, w0, 6, 3);
            uint32_t t10 = ((w0 & 0x200u) == 0) ? 0x800000u : 0x400000u;
            t12 = bfi_u32(t12, t8b, 16, 3);
            uint32_t res = t12 | t10;
            if ((w0 >> 10) & 1u) {
                return res | 0xe2100000u;
            }
            return res | 0xe0100000u;
        } else {

            uint32_t t11 = ubfiz_u32(w0, 12, 3);
            uint32_t t10 = w0 >> 6;
            t11 = bfxil_u32(t11, w0, 3, 3);
            t11 = bfi_u32(t11, t10, 7, 5);
            t11 = bfi_u32(t11, t8, 5, 2);
            uint32_t res = t11 | 0xe1a00000u;
            res |= 0x100000u;
            return res;
        }
    }

    case 1: {
        uint32_t t11 = (w0 >> 11) & 3u;
        uint32_t t10 = (w0 >> 8) & 7u;
        uint32_t t8 = w0 & 0xFFu;
        if (t11 == 0) {

            t8 = bfi_u32(t8, t10, 12, 3);
            return t8 | 0xe3b00000u;
        } else {

            static const uint32_t thumb_alu_imm_opcode[4] = { 13u, 10u, 4u, 2u };
            uint32_t tval = thumb_alu_imm_opcode[t11];
            t8 = bfi_u32(t8, t10, 16, 3);
            t8 = bfi_u32(t8, t10, 12, 3);
            t8 = bfi_u32(t8, tval, 21, 11);
            return t8 | 0xe2100000u;
        }
    }

    case 2: {
        uint32_t level2 = (w0 >> 10) & 7u;
        switch (level2) {

        case 0: {
            uint32_t uVar1 = (w0 >> 6) & 0xFu;
            uint32_t idx = uVar1 - 2u;
            uint32_t uVar2 = w0 & 7u;
            uint32_t uVar4 = (w0 >> 3) & 7u;

            switch (idx) {
            case 0: case 1: case 2: case 5: {
                static const uint32_t thumb_shift_type[6] = { 0u, 1u, 2u, 0u, 0u, 3u };
                uint32_t tval = thumb_shift_type[idx];
                uint32_t t10 = uVar2;
                t10 = bfi_u32(t10, uVar2, 12, 3);
                t10 = bfi_u32(t10, uVar4, 8, 3);
                uint32_t res = t10 | (tval << 5);
                return res | 0xe1b00010u;
            }
            case 6: case 8: case 9: {
                static const uint32_t thumb_test_opcode[4] = { 8u, 0u, 10u, 11u };
                uint32_t tval = thumb_test_opcode[uVar1 - 8u];
                uint32_t t8 = uVar4;
                t8 = bfi_u32(t8, uVar2, 16, 3);
                t8 = bfi_u32(t8, tval, 21, 11);
                return t8 | 0xe0100000u;
            }
            case 7: {
                uint32_t t8 = uVar4 << 16;
                t8 = bfi_u32(t8, uVar2, 12, 3);
                return t8 | 0xe2700000u;
            }
            case 11: {
                uint32_t t10 = uVar2;
                t10 = bfi_u32(t10, uVar2, 16, 3);
                t10 = bfi_u32(t10, uVar4, 8, 3);
                uint32_t res = t10 | 0xe0100000u;
                return res | 0x90u;
            }
            case 13: {
                uint32_t t8 = uVar4;
                t8 = bfi_u32(t8, uVar2, 12, 3);
                uint32_t res = t8 | 0xe2100000u;
                return res - 0x200000u;
            }
            default: {

                static const int32_t default_table[16] = {
                    0, 1, 0, 0, 0, 5, 6, 0, 0, 0, 0, 0, 12, 0, 14, 0
                };
                uint32_t tval = (uint32_t)default_table[uVar1 & 0xFu];
                uint32_t t8 = uVar4;
                t8 = bfi_u32(t8, uVar2, 16, 3);
                t8 = bfi_u32(t8, uVar2, 12, 3);
                t8 = bfi_u32(t8, tval, 21, 11);
                return t8 | 0xe0100000u;
            }
            }
        }

        case 1: {
            uint32_t g = (w0 >> 8) & 3u;
            uint32_t w8v = (w0 >> 3) & 0xFu;
            switch (g) {
            case 0: {
                uint32_t t10 = w0 >> 4;
                t10 = bfxil_u32(t10, w0, 0, 3);
                uint32_t t8 = w8v;
                t8 = bfi_u32(t8, t10, 16, 4);
                t8 = bfi_u32(t8, t10, 12, 4);
                return t8 | ARM_ENCODING_ADD_ALWAYS;
            }
            case 1: {
                uint32_t t10 = w0 >> 4;
                t10 = bfxil_u32(t10, w0, 0, 3);
                uint32_t t8 = w8v;
                t8 = bfi_u32(t8, t10, 16, 4);
                return t8 | 0xe1500000u;
            }
            case 2: {
                uint32_t t10 = w0 >> 4;
                t10 = bfxil_u32(t10, w0, 0, 3);
                uint32_t t8 = w8v;
                t8 = bfi_u32(t8, t10, 12, 4);
                return t8 | 0xe1a00000u;
            }
            default: {
                uint32_t t10 = (w0 >> 2) & 0x20u;
                uint32_t t8 = w8v | t10;
                return t8 | 0xe12fff10u;
            }
            }
        }

        case 2: case 3: {
            uint32_t t8 = (uint32_t)(w0 << 4) & 0x7000u;
            t8 = bfi_u32(t8, w0, 2, 8);
            uint32_t res = t8 | 0xe5800000u;
            res |= 0x1f0000u;
            uint32_t one32 = 1;
            memcpy(param_2, &one32, sizeof(one32));
            return res;
        }

        default: {
            uint32_t w11v = w0 & 7u;
            uint32_t w8v = (w0 >> 3) & 7u;
            uint32_t w10v = (w0 >> 6) & 7u;
            if ((w0 >> 9) & 1u) {

                static const uint32_t thumb_halfword_sh_bits[4] = { 1u, 2u, 1u, 3u };
                uint32_t didx = (w0 >> 10) & 3u;
                uint32_t tval = thumb_halfword_sh_bits[didx];
                uint32_t t10 = w10v;
                t10 = bfi_u32(t10, w11v, 12, 3);
                uint32_t flag = (((w0 >> 10) & 3u) != 0) ? 1u : 0u;
                t10 = bfi_u32(t10, w8v, 16, 3);
                t10 = bfi_u32(t10, flag, 20, 1);
                uint32_t res = t10 | (tval << 5);
                return res | 0xe1800090u;
            } else {

                uint32_t t13 = (uint32_t)(w0 << 9) & 0x100000u;
                t13 = bfi_u32(t13, w11v, 12, 3);
                uint32_t t10 = t13 | w10v;
                uint32_t t12 = w0 >> 10;
                t10 = bfi_u32(t10, w8v, 16, 3);
                t10 = bfi_u32(t10, t12, 22, 1);
                return t10 | 0xe7800000u;
            }
        }
        }
    }

    case 3: {
        uint32_t t13 = (uint32_t)(w0 << 13) & 0x70000u;
        uint32_t t8a = w0 >> 11;
        uint32_t t11 = (w0 >> 6) & 0x1Fu;
        uint32_t t10a = (w0 >> 12) & 1u;
        t13 = bfi_u32(t13, w0, 12, 3);
        t13 = bfi_u32(t13, t8a, 20, 1);
        uint32_t t11b = (((w0 >> 12) & 1u) == 0) ? (t11 << 2) : t11;
        t13 = bfi_u32(t13, t10a, 22, 1);
        uint32_t res = t13 | t11b;
        return res | 0xe5800000u;
    }

    case 4: {
        uint32_t t8 = (w0 >> 11) & 1u;
        if ((w0 >> 12) & 1u) {

            uint32_t t10 = (uint32_t)(w0 << 4) & 0x7000u;
            t10 = bfi_u32(t10, w0, 2, 8);
            t10 = bfi_u32(t10, t8, 20, 1);
            uint32_t res = t10 | 0xe5800000u;
            return res | 0xd0000u;
        } else {

            uint32_t t11 = (uint32_t)(w0 << 13) & 0x70000u;
            uint32_t t12 = (w0 >> 1) & 0x300u;
            t11 = bfi_u32(t11, w0, 12, 3);
            uint32_t t10 = (w0 >> 5) & 0xEu;
            t11 = t11 | t12;
            uint32_t t10b = t11 | t10;
            t10b = bfi_u32(t10b, t8, 20, 1);
            return t10b | 0xe1c000b0u;
        }
    }

    case 5: {
        if ((w0 >> 12) & 1u) {

            if ((w0 & 0xF00u) == 0) {

                uint32_t t8 = w0 & 0x7Fu;
                if ((w0 >> 7) & 1u) {

                    return t8 | 0xe24ddf00u;
                } else {
                    uint32_t res = t8 | 0xe28d0f00u;
                    return res | 0xd000u;
                }
            } else {
                uint32_t t8 = 0xe8a00000u;
                uint32_t t10 = w0 & 0xFFu;
                uint32_t t11 = w0 & 0x100u;
                if ((w0 >> 11) & 1u) {

                    t10 = t10 | (t11 << 7);
                    t8 = t8 + (0x1d0u << 12);
                    return t10 | t8;
                } else {
                    t10 = t10 | (t11 << 6);
                    t8 = t8 + (0x8d0u << 12);
                    return t10 | t8;
                }
            }
        } else {

            uint32_t t10 = 0xe28d0f00u;

            uint32_t t11 = w0 >> 8;
            uint32_t t8 = w0 & 0xFFu;
            t8 = bfi_u32(t8, t11, 12, 3);
            if ((w0 >> 11) & 1u) {
                return t8 | t10;
            }
            t10 = t10 + (0x20u << 12);
            t8 = t8 | t10;
            uint32_t one32 = 1;
            memcpy(param_2, &one32, sizeof(one32));
            return t8;
        }
    }

    case 6: {
        if ((w0 >> 12) & 1u) {

            uint32_t inv = ~w0;
            if ((inv & 0xF00u) != 0) {

                int32_t sx = (int32_t)(int8_t)(w0 & 0xFFu);
                uint32_t t10 = ((uint32_t)sx) & 0xFFFFFFu;
                uint32_t t8 = w0 >> 8;
                t10 = bfi_u32(t10, t8, 28, 4);
                return t10 | 0xa000000u;
            } else {

                uint32_t t8 = 0xef000000u;
                t8 = bfi_u32(t8, w0, 16, 8);
                return t8;
            }
        } else {

            uint32_t t10 = (uint32_t)(w0 << 9) & 0x100000u;
            uint32_t t8 = w0 >> 8;
            t10 = bfxil_u32(t10, w0, 0, 8);
            t10 = bfi_u32(t10, t8, 16, 3);
            return t10 | 0xe8a00000u;
        }
    }

    case 7: default: {

        if ((w0 & 0x1800u) != 0) {
            uint32_t t8 = 0xe6000010u;
            t8 = t8 | (w0 << 5);
            return t8;
        } else {

            int32_t t10 = sbfx_u32(w0, 0, 11);
            uint32_t t8 = 0xea000000u;
            t8 = bfxil_u32(t8, (uint32_t)t10, 0, 24);
            return t8;
        }
    }
    }
}

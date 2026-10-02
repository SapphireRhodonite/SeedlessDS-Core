#include "hires_runtime.h"
#include "cpu/arm.h"
#include "blob_symbols.h"
#include "emit_contract.h"
#include "core/nds_state.h"
#include "jit.h"
#include "templates.h"
#include "jit_hooks.h"
#include "mem_access.h"

static inline double tpl_f64(uint64_t v) { double d; memcpy(&d, &v, 8); return d; }
#define CPU(p) ((arm_t *)(p))



static uint32_t ldst_branch_rel(uint32_t mold, const char *dest, const unsigned char *pc)
{
    int64_t d = (int64_t)(uintptr_t)dest - (int64_t)(uintptr_t)pc;
    int64_t n = (d < 0) ? d + 3 : d;
    return (mold & ~UINT32_C(0x3ffffff)) |
           (((uint32_t)(n >> 2)) & UINT32_C(0x3ffffff));
}

static void emit_undefined_instruction_trap(jit_emitter_t *machine)
{
    arm_t *ctx          = machine->cpu;
    unsigned char *x8   = machine->code;
    uint32_t w2         = machine->pc;
    uint32_t mold      = UINT32_C(0x528000a1);
    const struct cp15 *x21 = ctx->cp15;
    const char *x25     = jit_bank_select_hook;
    unsigned char *x8b;

    wr32(x8, mold);
    wr32(x8 + 4, ldst_branch_rel(UINT32_C(0x94000000), x25, x8 + 4));
    machine->code = (unsigned char *)(x8 + 8);
    jit_emit_mov_imm32(machine, 0x1bu, w2);

    {
        unsigned char *x9 = machine->code;
        wr32(x9, UINT32_C(0xb963c380));
        x8b = x9 + 4;
        machine->code = (unsigned char *)x8b;
        if (machine->thumb != 0) {
            wr32(x9 + 4, UINT32_C(0x323b0000));
            x8b = x9 + 8;
            machine->code = (unsigned char *)x8b;
        }
    }

    {
        double d0 = tpl_f64(jit_tpl_store_spsr_und);
        double d1 = tpl_f64(jit_tpl_cpsr_disable_irq);
        uint32_t template = arm_mode_by_bank[5];
        uint32_t w10 = UINT32_C(0x11000000) | (template << 10);
        wr_f64(x8b, d0);
        wr_f64(x8b + 12, d1);
        wr32(x8b + 8, w10);
        machine->code = (unsigned char *)(x8b + 0x14);
    }

    {
        unsigned char *x9;
        if (x21 == NULL) {
            x9 = x8b + 0x14;
            wr32(x9, mold - 0x21u);
        } else {
            double d0b = tpl_f64(jit_tpl_load_exception_vector_base);
            uint32_t w10 = UINT32_C(0x1000) | (UINT32_C(0x1100) << 16);
            x9 = x8b + 0x1c;
            wr_f64(x8b + 20, d0b);
            machine->code = (unsigned char *)x9;
            wr32(x8b + 28, w10);
        }

        {
            const char *table = jit_dispatch_block_cache;
            wr32(x9 + 4, ldst_branch_rel(UINT32_C(0x14000000), table, x9 + 4));
            machine->code = (unsigned char *)(x9 + 8);
        }
    }
}

static void emit_pc_thumb_literal(jit_emitter_t *machine, uint32_t w20)
{
    unsigned char *x8;

    jit_emit_mov_imm32(machine, 0x1bu,
                       machine->pc | 1u);
    jit_emit_mov_imm32(machine, 0x1bu,
                       machine->pc | 1u);

    x8 = machine->code;
    {
        uint32_t w9  = (w20 << 6) & UINT32_C(0x3ff800);
        uint32_t w10 = UINT32_C(0x1000) | (UINT32_C(0x1100) << 16);
        w9 = w9 + w10;
        w9 = w9 - UINT32_C(0xca0);
        wr32(x8, w9);
        machine->code = (unsigned char *)(x8 + 4);
    }
    {

        uint32_t w9 = ((w20 & (UINT32_C(1) << 17)) != 0)
                      ? UINT32_C(0x32000000)
                      : UINT32_C(0x123e7400);

        const char *table = jit_dispatch_block_cache;
        wr32(x8 + 4, w9);
        wr32(x8 + 8, ldst_branch_rel(UINT32_C(0x14000000), table, x8 + 8));
        machine->code = (unsigned char *)(x8 + 0xc);
    }
}

static void emit_pc_relative_address(jit_emitter_t *machine, uint32_t w20)
{
    if (machine->thumb == 0) { emit_undefined_instruction_trap(machine); return; }
    if ((w20 & (UINT32_C(1) << 16)) != 0) { emit_pc_thumb_literal(machine, w20); return; }

    {

        const jit_instruction_record_t *x8 = machine->record;
        uint32_t w8 = x8->pc_value;

        uint32_t w9 = ((uint32_t)(int32_t)(int16_t)(uint16_t)w20) << 7;
        w9 &= UINT32_C(0xfffff000);
        jit_emit_mov_imm32(machine, 0x1bu, w8 + w9);
    }
}

unsigned char *jit_emit_arm_single_data_transfer_common(jit_emitter_t *machine, uint32_t w20)
{

    uint32_t w10 = (w20 >> 28) & 0xFu;
    uint32_t w11 = (w20 >> 29) & 0x7u;
    unsigned char *x22;
    uint32_t w2, w3;

    if (w11 > 6u) {

        x22 = (unsigned char *)0;
    } else {

        x22 = machine->code;

        int64_t x22i = (int64_t)(uintptr_t)x22;

        uint64_t sel = (x22i < 0) ? (uint64_t)(x22i + 3) : (uint64_t)x22i;

        uint32_t w11b = 0u - ((uint32_t)sel >> 2);

        uint32_t w10b = (w10 & ~UINT32_C(0x00ffffe0)) | ((w11b & UINT32_C(0x7ffff)) << 5);

        uint32_t w11c = w10b ^ UINT32_C(0x54000001);
        wr32(x22, w11c);
        machine->code = (unsigned char *)(x22 + 4);
    }

    w2 = (w20 >> 20) & 1u;
    w3 = (w20 >> 22) & 1u;

    if ((w20 & (UINT32_C(1) << 25)) == 0u) {

        uint32_t imm12 = w20 & UINT32_C(0xfff);
        jit_emit_template_call(machine, w20, w2, w3,
                            0u, 0u, 0u, 0u, imm12);
    } else if ((w20 & (UINT32_C(1) << 4)) == 0u) {

        jit_emit_template_call(machine, w20, w2, w3,
                            0u, 0u, 0u, 2u, 0u);
    } else {

        emit_pc_relative_address(machine, w20);
        return x22;
    }

    return x22;
}

unsigned char *jit_emit_arm_single_data_transfer_imm(jit_emitter_t *machine, uint32_t w20)
{
    return jit_emit_arm_single_data_transfer_common(machine, w20);
}

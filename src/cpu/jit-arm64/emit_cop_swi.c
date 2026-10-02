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



static inline uint32_t branch_rel(uint32_t mold, const char *dest, const unsigned char *pc)
{
    int64_t d = (int64_t)(uintptr_t)dest - (int64_t)(uintptr_t)pc;
    int64_t n = (d < 0) ? d + 3 : d;
    return (mold & ~UINT32_C(0x3ffffff)) |
           (((uint32_t)(n >> 2)) & UINT32_C(0x3ffffff));
}

static void emit_swi_trap_tail(jit_emitter_t *machine, unsigned char *x8b,
                            const struct cp15 *x21, uint32_t mold)
{
    unsigned char *x9;

    if (x21 == NULL) {

        x9 = x8b + 0x14;
        wr32(x9, mold + 0x5fu);
    } else {

        double d0 = tpl_f64(jit_tpl_load_exception_vector_base);
        uint32_t w10 = UINT32_C(0x2000) | (UINT32_C(0x1100) << 16);
        x9 = x8b + 0x1c;
        wr_f64(x8b + 20, d0);
        machine->code = (unsigned char *)x9;
        wr32(x8b + 28, w10);
    }

    {
        const char *table = jit_dispatch_block_cache;
        uint32_t w11 = branch_rel(UINT32_C(0x14000000), table, x9 + 4);
        wr32(x9 + 4, w11);
        machine->code = (unsigned char *)(x9 + 8);
    }
}

static unsigned char *emit_swi_trap(jit_emitter_t *machine, unsigned char *x22)
{
    arm_t *ctx = machine->cpu;
    unsigned char *x8  = machine->code;
    uint32_t mold     = UINT32_C(0x528000a1);
    const struct cp15 *x21 = ctx->cp15;
    uint32_t w2        = machine->pc;
    const char *x25   = jit_bank_select_hook;

    wr32(x8, mold - 0x40u);
    wr32(x8 + 4, branch_rel(UINT32_C(0x94000000), x25, x8 + 4));
    machine->code = (unsigned char *)(x8 + 8);
    jit_emit_mov_imm32(machine, 0x1bu, w2);

    {
        unsigned char *x9 = machine->code;
        unsigned char *x8b;
        wr32(x9, UINT32_C(0xb963c380));
        x8b = x9 + 4;
        machine->code = (unsigned char *)x8b;
        if (machine->thumb != 0) {
            wr32(x9 + 4, UINT32_C(0x323b0000));
            x8b = x9 + 8;
            machine->code = (unsigned char *)x8b;
        }

        {
            double d0 = tpl_f64(jit_tpl_store_spsr_svc);
            double d1 = tpl_f64(jit_tpl_cpsr_disable_irq);
            uint32_t template = arm_mode_by_bank[3];
            uint32_t w10 = UINT32_C(0x11000000) | (template << 10);
            wr_f64(x8b, d0);
            wr_f64(x8b + 12, d1);
            wr32(x8b + 8, w10);
            machine->code = (unsigned char *)(x8b + 0x14);
        }

        emit_swi_trap_tail(machine, x8b, x21, mold);
    }
    return x22;
}

static void emit_cp15_call(jit_emitter_t *machine, uint32_t w8sel, const char *table, int with_resume)
{
    unsigned char *x9 = machine->code;

    uint32_t w10 = UINT32_C(0x2a0003e0);
    w10 = (w10 & ~UINT32_C(0x001f0000)) | ((w8sel & 0x1fu) << 16);
    wr32(x9, w10 | 1u);

    wr32(x9 + 4, branch_rel(UINT32_C(0x94000000), table, x9 + 4));

    {
        uint32_t w8 = machine->pc - machine->block_pc;
        uint32_t w11 = (uint32_t)(uintptr_t)machine->block_code;
        uint32_t w10b = (uint32_t)(uintptr_t)(x9 + 8) - w11;
        machine->code = (unsigned char *)(x9 + 8);

        if (with_resume) {

            uint32_t *x9b = machine->resume_table;
            w10b = (w10b << 14) & UINT32_C(0xffff0000);
            wr32(x9b, w10b | w8);
            machine->resume_table = x9b + 1;
        }
    }
}

static void emit_cp15_register_pair(jit_emitter_t *machine, uint32_t w8sel,
                        uint32_t sum, uint32_t ora)
{
    unsigned char *x10 = machine->code;
    uint32_t w9 = w8sel | UINT32_C(0xf9512b80);
    uint32_t w8 = (w8sel & ~UINT32_C(0x3e0)) | ((w8sel & 0x1fu) << 5);
    w8 |= UINT32_C(0xb9401400);
    w8 += sum;
    w8 |= ora;
    wr32(x10, w9);
    wr32(x10 + 4, w8);
    machine->code = (unsigned char *)(x10 + 8);
}

static unsigned char *emit_undefined_instruction_trap(jit_emitter_t *machine, unsigned char *x22)
{
    unsigned char *c0 = machine->code;
    uint32_t w2 = machine->pc;

    {
        uint64_t x25 = (uint64_t)(uintptr_t)jit_bank_select_hook;
        int64_t disp1 = (int64_t)x25 - (int64_t)(uintptr_t)(c0 + 4);
        int64_t num1 = (disp1 < 0) ? (disp1 + 3) : disp1;
        uint32_t bl1 = UINT32_C(0x94000000) |
                       (((uint32_t)(num1 >> 2)) & UINT32_C(0x3ffffff));

        wr32(c0, UINT32_C(0x528000a1));
        wr32(c0 + 4, bl1);
        machine->code = (unsigned char *)(c0 + 8);

        jit_emit_mov_imm32(machine, 0x1bu, w2);
    }

    c0 = machine->code;
    wr32(c0, UINT32_C(0xb963c380));
    {
        unsigned char *x8 = c0 + 4;
        machine->code = (unsigned char *)x8;

        if (machine->thumb != 0) {
            wr32(c0 + 4, UINT32_C(0x323b0000));
            x8 = c0 + 8;
            machine->code = (unsigned char *)x8;
        }

        {
            double d0a = tpl_f64(jit_tpl_store_spsr_und);
            double d0b = tpl_f64(jit_tpl_cpsr_disable_irq);
            uint64_t table3728 = (uint64_t)(uintptr_t)jit_dispatch_block_cache;

            unsigned char *pc_bl2 = x8 + 0x18;
            uint32_t template_w9 = arm_mode_by_bank[5];
            uint32_t w12b = UINT32_C(0x528000a1) - 0x21;

            int64_t disp2 = (int64_t)table3728 - (int64_t)(uintptr_t)pc_bl2;
            int64_t num2 = (disp2 < 0) ? (disp2 + 3) : disp2;
            uint32_t w11b = UINT32_C(0x14000000) |
                            (((uint32_t)(num2 >> 2)) & UINT32_C(0x3ffffff));
            uint32_t w9b = UINT32_C(0x11000000) | (template_w9 << 10);

            wr_f64(x8, d0a);
            wr_f64(x8 + 12, d0b);
            wr32(x8 + 20, w12b);
            wr32(x8 + 24, w11b);
            wr32(x8 + 8, w9b);
            machine->code = (unsigned char *)(x8 + 0x1c);
        }
    }

    return x22;
}

static unsigned char *emit_cp15_transfer(jit_emitter_t *machine, uint32_t w20, unsigned char *x22)
{

    if ((w20 & UINT32_C(0x00e00f00)) != UINT32_C(0xf00))
        return x22;

    {
        uint32_t w8raw = (w20 >> 12) & 0xfu;
        uint32_t w11   = (w20 >> 16) & 0xfu;
        uint32_t w9    = (w20 >> 5) & 7u;
        uint32_t w10   = w20 & 0xfu;

        uint32_t w8sel = (w8raw == 0xfu) ? 0u : (w8raw + 0xdu);

        if (((w20 >> 20) & 1u) != 0) {

            if (w11 == 9u) {
                if (w10 != 1u) goto L_1;
                if (w9 == 1u) { emit_cp15_register_pair(machine, w8sel, 0u, 0x800u); return x22; }
                if (w9 != 0u) goto L_1;
                emit_cp15_register_pair(machine, w8sel, 0x400u, 0u);
                return x22;
            }
            if (w11 == 1u) {
                if ((w9 | w10) == 0u) {
                    emit_cp15_register_pair(machine, w8sel, 0u, 0u);
                    return x22;
                }
                goto L_1;
            }
            if (w11 != 0u) goto L_1;
            if (w10 != 0u) goto L_1;
            {

                double d0;
                if (w9 == 1u)      d0 = tpl_f64(jit_tpl_mov_imm32_sel1);
                else if (w9 != 2u) d0 = tpl_f64(jit_tpl_mov_imm32_seln);
                else               d0 = tpl_f64(jit_tpl_mov_imm32_sel2);
                {
                    unsigned char *x9p = machine->code;
                    uint64_t dup = ((uint64_t)w8sel << 32) | (uint64_t)w8sel;
                    uint64_t val;
                    memcpy(&val, &d0, 8);
                    val |= dup;
                    memcpy(x9p, &val, 8);
                    machine->code = (unsigned char *)(x9p + 8);
                }
                return x22;
            }
        }

        if (w11 == 9u) {
            if (w10 != 1u) return x22;
            if (w9 == 1u) {
                emit_cp15_call(machine, w8sel,
                         jit_cp15_itcm_hook, 0);
                return x22;
            }
            if (w9 != 0u) return x22;
            emit_cp15_call(machine, w8sel,
                     jit_cp15_dtcm_hook, 1);
            return x22;
        }

        if (w11 == 7u) {

            if (!((w10 == 0u && w9 == 4u) || (w10 == 8u && w9 == 2u)))
                return x22;

            {
                uint32_t w8t = machine->pc;
                uint8_t  w9t = machine->thumb;
                jit_emit_mov_imm32(machine,
                                   0u, w8t | w9t);
            }
            {

                unsigned char *x8w = machine->code;
                const char *table = jit_exit_store_pc_flags;
                double   d0w   = tpl_f64(jit_tpl_set_halt_flag);

                wr_f64(x8w, d0w);

                wr32(x8w + 8,
                          branch_rel(UINT32_C(0x14000000), table, x8w + 8));
                machine->code = (unsigned char *)(x8w + 12);
            }

            return x22;
        }

        if (w11 != 1u) return x22;
        if ((w9 | w10) != 0u) return x22;

        emit_cp15_call(machine, w8sel, jit_cp15_control_hook, 1);
        return x22;

    L_1:

        {
            unsigned char *x9p = machine->code;
            uint32_t w8 = w8sel | UINT32_C(0x2a0003e0) | UINT32_C(0x1f0000);
            wr32(x9p, w8);
            machine->code = (unsigned char *)(x9p + 4);
        }
        return x22;
    }
}

unsigned char *jit_emit_arm_coprocessor_and_swi(jit_emitter_t *machine, uint32_t w20)
{
    uint32_t w11sel = (w20 >> 29) & 0x7u;
    unsigned char *x22;

    if (w11sel > 6u) {
        x22 = NULL;
    } else {

        uint32_t w10 = (w20 >> 28) & 0xFu;
        int64_t x22i;
        uint64_t sel;
        uint32_t w8;

        x22  = machine->code;
        x22i = (int64_t)(uintptr_t)x22;
        sel  = (x22i < 0) ? (uint64_t)(x22i + 3) : (uint64_t)x22i;
        w8   = 0u - ((uint32_t)sel >> 2);
        w10  = (w10 & ~UINT32_C(0x00ffffe0)) |
               ((w8 & UINT32_C(0x7ffff)) << 5);
        w8   = w10 ^ UINT32_C(0x54000001);
        wr32(x22, w8);
        machine->code = (unsigned char *)(x22 + 4);
    }

    if ((w20 & (UINT32_C(1) << 24)) != 0)
        return emit_swi_trap(machine, x22);

    {
        arm_t *ctx = machine->cpu;
        if (ctx->cp15 == NULL)
            return emit_undefined_instruction_trap(machine, x22);
        if ((w20 & (UINT32_C(1) << 4)) == 0)
            return x22;
    }

    return emit_cp15_transfer(machine, w20, x22);
}

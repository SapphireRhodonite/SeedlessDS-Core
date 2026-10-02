#include "hires_runtime.h"
#include "cpu/arm.h"
#include "blob_symbols.h"
#include "emit_contract.h"
#include <stdint.h>
#include <string.h>
#include "core/nds_state.h"
#include "jit.h"
#include "templates.h"
#include "jit_hooks.h"
#include "mem_access.h"

static inline double tpl_f64(uint64_t v) { double d; memcpy(&d, &v, 8); return d; }
#define CPU(p) ((arm_t *)(p))



unsigned char *jit_emit_arm_coprocessor_data_transfer(jit_emitter_t *machine, uint32_t w20)
{

    uint32_t w11 = (w20 >> 29) & 7u;
    uint32_t w10 = (w20 >> 28) & 15u;

    unsigned char *x22;

    if (w11 <= 6) {

        x22 = machine->code;

        int64_t x22i = (int64_t)(uintptr_t)x22;
        uint64_t sel = (x22i < 0) ? (uint64_t)(x22i + 3)
                                  : (uint64_t)x22i;
        uint32_t w8 = 0u - ((uint32_t)sel >> 2);
        w10 = (w10 & ~UINT32_C(0x00ffffe0)) |
              ((w8 & UINT32_C(0x7ffff)) << 5);
        w8 = w10 ^ UINT32_C(0x54000001);
        wr32(x22, w8);
        machine->code = (unsigned char *)(x22 + 4);
    } else {

        x22 = NULL;
    }

    arm_t *fpu_ctx = machine->cpu;
    if (fpu_ctx->cp15 != NULL) return x22;

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

    return x22;
}

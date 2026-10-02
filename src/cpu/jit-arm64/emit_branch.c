#include "hires_runtime.h"
#include "emit_contract.h"
#include <stdint.h>
#include "jit.h"

extern void jit_emit_link_queue_entry(jit_emitter_t *ctx);

unsigned char *jit_emit_arm_branch(jit_emitter_t *machine, uint32_t w20)
{

    uint32_t w11 = (w20 >> 29) & 7u;
    uint32_t w10 = (w20 >> 28) & 15u;

    unsigned char *x22 = NULL;
    int needs_return_address = 0;

    if (w11 <= 6) {

        x22 = machine->code;

        {
            uint32_t w8 = w10;
            int64_t x22i = (int64_t)(uintptr_t)x22;
            uint64_t sel = (x22i < 0) ? (uint64_t)(x22i + 3)
                                      : (uint64_t)x22i;
            uint32_t w9 = 0u - ((uint32_t)sel >> 2);
            w8 = (w8 & ~UINT32_C(0x00ffffe0)) |
                 ((w9 & UINT32_C(0x7ffff)) << 5);
            w9 = w8 ^ UINT32_C(0x54000001);
            wr32(x22, w9);
        }
        machine->code = (unsigned char *)(x22 + 4);

        if (w10 > 0xe || (w20 & (UINT32_C(1) << 24)) != 0) {

            needs_return_address = 1;
        } else {

            jit_emit_link_queue_entry(machine);
            return x22;
        }
    } else {

        if (w10 > 0xe) {

            needs_return_address = 1;
        } else if ((w20 & (UINT32_C(1) << 24)) != 0) {

            needs_return_address = 1;
        } else {

            jit_emit_link_queue_entry(machine);
            return NULL;
        }
    }

    if (needs_return_address) {
        uint8_t flag = machine->thumb;
        uint32_t w9 = machine->pc;
        uint32_t w8 = (flag != 0) ? 1u : 0u;

        jit_emit_mov_imm32(machine, 0x1bu, w9 | w8);
        jit_emit_link_queue_entry(machine);
        return x22;
    }

    return x22;

}

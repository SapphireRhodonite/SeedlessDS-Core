#include "hires_runtime.h"
#include "emit_contract.h"
#include "jit.h"

extern unsigned char *jit_emit_arm_single_data_transfer_common(jit_emitter_t *machine, uint32_t w20);

unsigned char *jit_emit_arm_single_data_transfer_reg(jit_emitter_t *machine, uint32_t w20)
{
    return jit_emit_arm_single_data_transfer_common(machine, w20);
}

#ifndef EMIT_CONTRACT_H
#define EMIT_CONTRACT_H

#include <stdint.h>
#include <string.h>
#include "jit.h"
#include "mem_access.h"


void jit_emit_mov_imm32(jit_emitter_t *emitter, uint32_t param_2, uint32_t param_3);
void jit_emit_template_call(jit_emitter_t *param_1, uint32_t param_2, uint32_t param_3, uint32_t param_4, uint32_t param_5, uint32_t param_6, uint32_t param_7, uint32_t param_8, uint32_t param_9);

unsigned char *jit_emit_arm_data_processing_reg(jit_emitter_t *machine, uint32_t w20);
unsigned char *jit_emit_arm_data_processing_imm(jit_emitter_t *machine, uint32_t w20);
unsigned char *jit_emit_arm_single_data_transfer_imm(jit_emitter_t *machine, uint32_t w20);
unsigned char *jit_emit_arm_single_data_transfer_reg(jit_emitter_t *machine, uint32_t w20);
unsigned char *jit_emit_arm_block_data_transfer(jit_emitter_t *machine, uint32_t w20);
unsigned char *jit_emit_arm_branch(jit_emitter_t *machine, uint32_t w20);
unsigned char *jit_emit_arm_coprocessor_data_transfer(jit_emitter_t *machine, uint32_t w20);
unsigned char *jit_emit_arm_coprocessor_and_swi(jit_emitter_t *machine, uint32_t w20);

#endif

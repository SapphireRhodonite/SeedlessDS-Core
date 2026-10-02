#ifndef SEEDLESSDS_CPU_JIT_ARM64_TEMPLATES_H
#define SEEDLESSDS_CPU_JIT_ARM64_TEMPLATES_H

static const uint64_t jit_tpl_cpsr_disable_irq = UINT64_C(0xb923c38032390000);
static const uint64_t jit_tpl_load_exception_vector_base = UINT64_C(0xb9401000f9512b80);
static const uint64_t jit_tpl_store_spsr_svc = UINT64_C(0x123a6400b920f780);
static const uint64_t jit_tpl_store_spsr_und = UINT64_C(0x123a6400b920ff80);
static const uint64_t jit_tpl_set_halt_flag = UINT64_C(0xb921138152800021);
static const uint64_t jit_tpl_mov_imm32_sel1 = UINT64_C(0x72a1e1a052842240);
static const uint64_t jit_tpl_mov_imm32_sel2 = UINT64_C(0x72a0028052803000);
static const uint64_t jit_tpl_mov_imm32_seln = UINT64_C(0x72a820005292ac20);
static const uint64_t jit_tpl_read_cycle_mark_add4 = UINT64_C(0x11001000b8560120);
static const uint64_t jit_tpl_zero_w0_ones_w1 = UINT64_C(0x2a3f03e12a1f03e0);

#endif

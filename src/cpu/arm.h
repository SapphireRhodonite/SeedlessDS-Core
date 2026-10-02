#ifndef SEEDLESSDS_ARM_H
#define SEEDLESSDS_ARM_H

#include <stddef.h>
#include <stdint.h>
#include "../timers.h"
#include "bus.h"

#define ARM_SIZE 0x10065f0u
#define ARM_BANKS 7
#define ARM_BANK_USR 0
#define ARM_BANK_FIQ 1
#define ARM_BANK_IRQ 2
#define ARM_BANK_SVC 3
#define ARM_BANK_ABT 4
#define ARM_BANK_UND 5
#define ARM_BANK_SYS 6
#define ARM_ENCODING_ADD_ALWAYS 0xe0800000u
#define ARM_ITCM_ARM_BLOCKS 0x2000u
#define ARM_ITCM_THUMB_BLOCKS 0x4000u
#define ARM_BLOCK_LOOKUP 0x400u
#define ARM_DEBUG_SIZE 0x138u
#define ARM_DEBUG_VERSION 0x00010102u

struct nds;
struct io_mirror;
struct cp15;
struct gpu3d;
struct jit_arena;

typedef struct arm_debug {
    struct arm *owner;
    uint8_t unmapped_0[0x108 - 8];
    uint64_t instruction_count;
    uint64_t argument;
    uint64_t unmapped_1;
    uint32_t count_a;
    uint32_t unmapped_2;
    uint32_t count_b;
    uint32_t unmapped_3;
    uint8_t version;
    uint8_t mode;
    uint8_t mode_shadow;
    uint8_t unmapped_4[ARM_DEBUG_SIZE - 0x133];
} arm_debug_t;

typedef struct arm {
    nds_timer_t timers[4];
    uint32_t block_lookup_pc[ARM_BLOCK_LOOKUP];
    uint32_t block_lookup_offset[ARM_BLOCK_LOOKUP];
    struct io_mirror *io_mirror;
    struct jit_arena *jit_arena;
    uint32_t banked_sp_lr[ARM_BANKS][2];
    uint32_t fiq_r8_r14[7];
    uint32_t unmapped_1;
    uint32_t spsr[ARM_BANKS];
    uint32_t bank;
    uint32_t irq_pending;
    uint32_t is_arm9;
    uint32_t halt_flags;
    uint32_t unmapped_2;
    arm_debug_t debug;
    struct cp15 *cp15;
    struct nds *machine;
    struct bus *bus;
    struct gpu3d *gpu3d;
    uint32_t *itcm_arm_blocks;
    uint32_t *itcm_thumb_blocks;
    uint8_t *jit_irq_entry;
    uint8_t *jit_swi_entry;
    uint32_t cycle_mark;
    uint32_t unmapped_5;
    uint8_t *jit_block;
    struct arm *partner;
    uint32_t wake_flags;
    uint8_t unmapped_6[0x22f0 - 0x22ac];
    uint32_t host_regs[16];
    uint32_t host_scratch[5];
    uint8_t unmapped_7[0x2354 - 0x2344];
    uint32_t host_nzcv;
    void *host_x10;
    struct jit_arena *host_x11;
    uint8_t *host_lr;
    uint32_t r[16];
    uint64_t (*run_entry)(void *);
    uint32_t nzcv;
    uint32_t pc;
    uint32_t cpsr;
    uint8_t unmapped_8[0x23d0 - 0x23c4];
    pagetable_t pagetable;
} arm_t;
#define ARM_BANK_COUNT 7
extern const uint32_t arm_mode_by_bank[ARM_BANK_COUNT];

#define ARM_POPCOUNT_TABLE_BYTES 256u
extern uint8_t arm_popcount_table[ARM_POPCOUNT_TABLE_BYTES];

#endif

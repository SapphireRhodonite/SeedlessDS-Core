#ifndef SEEDLESSDS_JIT_H
#define SEEDLESSDS_JIT_H

#include <stddef.h>
#include <stdint.h>
#include "../arm.h"

#define JIT_CODE_MAIN_RAM_SIZE 0x1000000u
#define JIT_CODE_ITCM_SIZE 0x100000u
#define JIT_CODE_OTHER_SIZE 0x200000u
#define JIT_MAIN_RAM_HASH_ENTRIES 0x8000u
#define JIT_OTHER_HASH_ENTRIES 0x2000u
#define JIT_RECORDS_SIZE 0x80000u
#define JIT_RECORD_SIZE 40u
#define JIT_REGIONS 3
#define JIT_REGION_MAIN_RAM 0
#define JIT_REGION_ITCM 1
#define JIT_REGION_OTHER 2
#define JIT_PENDING_BRANCHES 0x8000u
#define JIT_BLOCK_PCS 0x8000u
#define JIT_ITCM_HOT_BLOCK 9u
#define JIT_MAIN_RAM_WRITTEN_WORDS 0x8000u
#define JIT_ITCM_VARIANTS 16
#define JIT_TABLES_RESET_BYTES 0x100000u
#define JIT_OTHER_TABLES_RESET_BYTES 0x40000u
#define JIT_ITCM_HAS_VARIANTS 0x80u

typedef struct jit_hash_entry {
    uint32_t pc;
    uint32_t offset;
    uint32_t pc_2;
    uint32_t offset_2;
} jit_hash_entry_t;

typedef struct jit_region {
    uint8_t *code;
    uint8_t *table;
} jit_region_t;

typedef struct jit_pending_branch {
    uint8_t *instruction;
    uint32_t target_pc;
    uint32_t unmapped_0;
} jit_pending_branch_t;

typedef struct jit_itcm_variant {
    uint32_t word;
    uint32_t pc;
    uint8_t *block;
} jit_itcm_variant_t;

typedef struct jit_arena {
    uint8_t code_main_ram[JIT_CODE_MAIN_RAM_SIZE];
    uint8_t code_itcm[JIT_CODE_ITCM_SIZE];
    uint8_t code_other[JIT_CODE_OTHER_SIZE];
    jit_hash_entry_t main_ram_blocks[JIT_MAIN_RAM_HASH_ENTRIES];
    jit_hash_entry_t other_blocks[JIT_OTHER_HASH_ENTRIES];
    uint8_t records[JIT_RECORDS_SIZE];
    jit_region_t region[JIT_REGIONS];
    jit_pending_branch_t pending_branch[JIT_PENDING_BRANCHES];
    uint32_t pending_branch_count;
    uint32_t block_pc[JIT_BLOCK_PCS];
    uint32_t block_pc_count;
    uint32_t itcm_arm_blocks[ARM_ITCM_ARM_BLOCKS];
    uint32_t itcm_thumb_blocks[ARM_ITCM_THUMB_BLOCKS];
    uint8_t itcm_arm_hits[ARM_ITCM_ARM_BLOCKS];
    jit_itcm_variant_t itcm_variant[JIT_ITCM_VARIANTS];
    uint32_t main_ram_written_words[JIT_MAIN_RAM_WRITTEN_WORDS];
    uint32_t itcm_variant_count;
    uint32_t unmapped_1;
    uint8_t *record_cursor;
    uint64_t unmapped_2[2];
    uint8_t unmapped_3[JIT_ARENA_SIZE - 0x14fa158u];
} jit_arena_t;

#define JIT_EMITTER_SCRATCH_WORDS 0x100
#define JIT_BLOCK_HEADER_HALFWORDS_MASK 0xfffu
#define JIT_BLOCK_HEADER_THUMB 0x1000u
#define JIT_BLOCK_HEADER_ARM9 0x8000u
#define JIT_RECORD_FLAGGED_OP 0x4u
#define JIT_ANALYSIS_ARM9 0x8u
#define JIT_ANALYSIS_LINK_MATCH 0x4u
#define JIT_LINK_LIMIT 0x7ffu

enum {
    JIT_BLOCK_END_NONE = 0,
    JIT_BLOCK_END_INDIRECT = 1,
    JIT_BLOCK_END_INDIRECT_2 = 2,
    JIT_BLOCK_END_LINK = 3,
    JIT_BLOCK_END_DISPATCH = 4
};

typedef struct jit_instruction_record {
    uint32_t word;
    uint32_t pc_value;
    uint8_t *code;
    uint16_t successor;
    uint16_t reads;
    uint16_t writes;
    uint16_t live_registers;
    uint8_t flag_masks;
    uint8_t live_flags;
    uint8_t emit_flags;
    uint8_t cycles;
    uint8_t writes_pc;
    uint8_t unmapped_0[3];
} jit_instruction_record_t;

typedef struct jit_link_record {
    uint8_t *target_code;
    uint8_t *branch_code;
    uint8_t **patch_slot;
    uint32_t target_pc;
    uint32_t source_pc;
    uint32_t flags;
    uint16_t instruction_index;
    uint16_t record_index;
} jit_link_record_t;

typedef struct jit_block_analysis {
    jit_arena_t *arena;
    jit_instruction_record_t *records;
    jit_link_record_t *links;
    uint32_t pc;
    uint32_t unmapped_0;
    uint16_t instruction_count;
    uint16_t halfword_count;
    uint16_t link_count;
    uint16_t live_registers;
    uint8_t live_flags;
    uint8_t options;
    uint8_t end_kind;
    uint8_t unmapped_1[5];
} jit_block_analysis_t;

typedef struct jit_block_header {
    uint32_t unmapped_0;
    uint32_t pc;
    uint32_t code_offset;
    uint16_t live_registers;
    uint16_t live_flags;
    uint16_t flags;
    uint16_t code_bytes;
    uint32_t table_offset;
} jit_block_header_t;

typedef struct jit_emitter {
    uint32_t scratch[JIT_EMITTER_SCRATCH_WORDS];
    jit_arena_t *arena;
    uint8_t *code;
    uint8_t *table;
    uint8_t *block_transfer_continue;
    uint32_t flagged_count;
    uint32_t unmapped_0;
    uint8_t *table_start;
    uint32_t *resume_table;
    uint8_t *block_code;
    uint32_t block_pc;
    uint32_t cycles;
    uint32_t pc;
    uint32_t unmapped_1;
    jit_block_analysis_t *block;
    jit_instruction_record_t *record;
    jit_link_record_t *link;
    struct arm *cpu;
    uint8_t unmapped_2[8];
    uint8_t is_arm9;
    uint8_t thumb;
    uint8_t unmapped_3[0x86];
} jit_emitter_t;
#endif

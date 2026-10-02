#include "hires_runtime.h"
#include "cpu/arm.h"
#include "blob_symbols.h"
#include "cpu/jit-arm64/jit_hooks.h"
#include "jit.h"
#include "templates.h"
#include <stdint.h>
#include <string.h>
#include "emit_contract.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "mem_access.h"



static inline double tpl_f64(uint64_t v) { double d; memcpy(&d, &v, 8); return d; }
#define CPU(p) ((arm_t *)(p))





static uint32_t branch_word(const void *dest, const void *origin,
                                uint32_t opcode)
{

    uint64_t difference = (uint64_t)(uintptr_t)dest - (uint64_t)(uintptr_t)origin;
    if ((difference & UINT64_C(0x8000000000000000)) != 0) {
        difference += 3;
    }

    return opcode | (((uint32_t)difference >> 2) & 0x03ffffffu);
}

void jit_emit_branch_pair(jit_emitter_t *param_1, uint32_t param_2,
                         uint32_t param_3)
{


    jit_emitter_t *em = param_1;
    arm_t *block = em->cpu;
    uint32_t value_448 = em->pc;
    struct cp15 *selector = block->cp15;
    void *field_emission = &em->code;
    const uint32_t base_imm = 0xb9000380u;
    uint8_t *pointer;

    jit_emit_mov_imm32(param_1, 1u, param_3);

    pointer = rd_ptr_u8(field_emission);
    const char *dest = jit_bank_select_hook;
    wr32(pointer,
          branch_word(dest, pointer, 0x94000000u));
    pointer += 4;
    wr_ptr(field_emission, pointer);

    jit_emit_mov_imm32(param_1, 0x1bu, value_448);

    pointer = rd_ptr_u8(field_emission);
    wr32(pointer, base_imm | 0x0063c000u);
    pointer += 4;
    wr_ptr(field_emission, pointer);
    if (em->thumb != 0) {
        wr32(pointer, 0x323b0000u);
        pointer += 4;
        wr_ptr(field_emission, pointer);
    }

    {
        uint32_t idx = 0x0020e800u + (param_3 << 10);
        uint32_t entry = arm_mode_by_bank[param_3];
        uint32_t word0 = (idx & 0x003ffc00u) | base_imm;
        uint32_t word2 = 0x11000000u | (entry << 10);
        uint64_t literal = jit_tpl_cpsr_disable_irq;

        wr32(pointer + 0, word0);
        wr32(pointer + 4, 0x123a6400u);
        wr64(pointer + 12, literal);
        wr32(pointer + 8, word2);
        pointer += 20;
        wr_ptr(field_emission, pointer);
    }

    {
        uint32_t word_final;
        if (selector != NULL) {

            uint64_t literal = jit_tpl_load_exception_vector_base;
            wr64(pointer, literal);
            pointer += 8;
            wr_ptr(field_emission, pointer);
            word_final = 0x11000000u | (param_2 << 12);
        } else {

            word_final = 0x52800000u | (param_2 << 7);
        }

        dest = jit_dispatch_block_cache;
        uint8_t *address_branch = pointer + 4;
        wr32(pointer, word_final);
        pointer += 8;
        wr32(address_branch,
              branch_word(dest, address_branch, 0x14000000u));
        wr_ptr(field_emission, pointer);
    }
}

void jit_emit_mov_imm32(jit_emitter_t *emitter, uint32_t param_2, uint32_t param_3)
{

    jit_emitter_t *em = emitter;
    void *field_addr = &em->code;
    uint32_t lo16 = param_3 & 0xffffu;
    uint32_t *bufptr;

    if (lo16 == 0) {

        uint32_t word = ((param_3 >> 11) & 0x1fffe0u) | param_2 | 0x52a00000u;
        memcpy(&bufptr, (void *)field_addr, sizeof(bufptr));
        memcpy(bufptr, &word, sizeof(word));
        bufptr += 1;
        memcpy((void *)field_addr, &bufptr, sizeof(bufptr));
        return;
    }

    if ((param_3 >> 16) == 0) {

        uint32_t word = param_2 | (lo16 << 5) | 0x52800000u;
        memcpy(&bufptr, (void *)field_addr, sizeof(bufptr));
        memcpy(bufptr, &word, sizeof(word));
        bufptr += 1;
        memcpy((void *)field_addr, &bufptr, sizeof(bufptr));
        return;
    }

    if (lo16 == 0xffffu) {

        uint32_t word = (((param_3 >> 11) & 0x1fffe0u) ^ 0x12bfffe0u) | param_2;
        memcpy(&bufptr, (void *)field_addr, sizeof(bufptr));
        memcpy(bufptr, &word, sizeof(word));
        bufptr += 1;
        memcpy((void *)field_addr, &bufptr, sizeof(bufptr));
        return;
    }

    if (param_3 < 0xffff0000u) {

        int32_t out_hi = 0;
        int32_t out_lo = 0;
        typedef uint32_t (*fnp_encode_logical_imm)(uint32_t, void *, void *);
        fnp_encode_logical_imm encode_logical_imm = jit_emit_encode_logical_imm;
        int rc = encode_logical_imm(param_3, &out_hi, &out_lo);

        if (rc != 0) {

            uint32_t word = param_2
                           | ((uint32_t)out_hi << 16)
                           | ((uint32_t)out_lo << 10)
                           | 0x320003e0u;
            memcpy(&bufptr, (void *)field_addr, sizeof(bufptr));
            memcpy(bufptr, &word, sizeof(word));
            bufptr += 1;
            memcpy((void *)field_addr, &bufptr, sizeof(bufptr));
        } else {

            uint32_t w0 = param_2 | (lo16 << 5) | 0x52800000u;
            uint32_t w1 = ((param_3 >> 11) & 0x1fffe0u) | param_2 | 0x72a00000u;
            memcpy(&bufptr, (void *)field_addr, sizeof(bufptr));
            memcpy(bufptr + 0, &w0, sizeof(w0));
            memcpy(bufptr + 1, &w1, sizeof(w1));
            bufptr += 2;
            memcpy((void *)field_addr, &bufptr, sizeof(bufptr));
        }
        return;
    }

    {
        uint32_t word = ((lo16 << 5) ^ 0x129fffe0u) | param_2;
        memcpy(&bufptr, (void *)field_addr, sizeof(bufptr));
        memcpy(bufptr, &word, sizeof(word));
        bufptr += 1;
        memcpy((void *)field_addr, &bufptr, sizeof(bufptr));
    }
}

void jit_emit_instruction(jit_emitter_t *machine)
{

    const jit_instruction_record_t *ctx = machine->record;
    uint32_t w20 = ctx->word;

    unsigned char *x22 = NULL;
    switch ((w20 >> 25) & 7u) {
    case 0: x22 = jit_emit_arm_data_processing_reg(machine, w20); break;
    case 1: x22 = jit_emit_arm_data_processing_imm(machine, w20); break;
    case 2: x22 = jit_emit_arm_single_data_transfer_imm(machine, w20); break;
    case 3: x22 = jit_emit_arm_single_data_transfer_reg(machine, w20); break;
    case 4: x22 = jit_emit_arm_block_data_transfer(machine, w20); break;
    case 5: x22 = jit_emit_arm_branch(machine, w20); break;
    case 6: x22 = jit_emit_arm_coprocessor_data_transfer(machine, w20); break;
    case 7: x22 = jit_emit_arm_coprocessor_and_swi(machine, w20); break;
    }

    if (x22 != NULL) {

        uint8_t *cursor = machine->code;
        uint32_t w8 = (uint32_t)(cursor - x22);
        w8 >>= 2;
        uint32_t w9 = rd32(x22);
        w9 = (w9 & ~UINT32_C(0x00ffffe0)) |
             ((w8 & UINT32_C(0x7ffff)) << 5);
        wr32(x22, w9);
    }
}

void jit_emit_fix_cursor_by_flagged_ops(jit_emitter_t *param_1, const jit_block_analysis_t *param_2) {

    uint16_t count16 = param_2->instruction_count;
    uint64_t count = count16;

    uint32_t sum = 0;

    if (count != 0) {
        const jit_instruction_record_t *base = param_2->records;

        uint64_t pair;

        if (count == 1) {
            pair = 0;
        } else {

            pair = count & ~(uint64_t)1;
            uint32_t acc_pair   = 0;
            uint32_t acc_odd = 0;

            const jit_instruction_record_t *p = base + 1;
            uint64_t remaining = pair;
            do {
                remaining -= 2;
                uint8_t b_pair   = p[-1].emit_flags;
                uint8_t b_odd = p[0].emit_flags;
                p += 2;
                acc_pair   += (b_pair   >> 2) & 1u;
                acc_odd += (b_odd >> 2) & 1u;
            } while (remaining != 0);

            sum = acc_odd + acc_pair;

            if (pair == count) goto end_index;
        }

        {
            const jit_instruction_record_t *p = base + pair;
            do {
                uint8_t b = p[0].emit_flags;
                p += 1;
                pair += 1;
                sum += (b >> 2) & 1u;
            } while (pair < count);
        }
    }

end_index:
    {
        uint8_t *val = param_1->table;
        param_1->flagged_count = sum;
        val = val - ((uint64_t)sum << 2);
        param_1->table = val;
        param_1->table_start = val;
        param_1->resume_table = (uint32_t *)val;
    }
}


uint32_t *jit_emit_block(void *param_1, void *param_2,
                              uint32_t param_3, uint32_t param_4)
{

    jit_block_analysis_t *ent = (jit_block_analysis_t *)param_1;
    uint8_t *ctx = (uint8_t *)param_2;

    jit_arena_t *arena = CPU(ctx)->jit_arena;
    uint64_t lVar24 = (uint64_t)(uintptr_t)arena;
    uint8_t  bVar3  = ent->options;
    uint32_t mode0  = CPU(ctx)->is_arm9;

    uint32_t *piVar9;
    uint64_t  field8_off;
    int       bVar7;
    int       mode2_ok = ((param_3 >> 24) == 2);

    if ((param_3 >> 25) == 0 && mode0 == 1) {
        piVar9      = (uint32_t *)arena->region[JIT_REGION_ITCM].code;
        field8_off  = JIT_REGION_ITCM;
        bVar7 = 1;
    } else if (mode2_ok) {
        piVar9      = (uint32_t *)arena->region[JIT_REGION_MAIN_RAM].code;
        field8_off  = JIT_REGION_MAIN_RAM;
        bVar7 = 0;
    } else {
        piVar9      = (uint32_t *)arena->region[JIT_REGION_OTHER].code;
        field8_off  = JIT_REGION_OTHER;
        bVar7 = 0;
    }
    uint64_t lVar23 = (uint64_t)(uintptr_t)arena->region[field8_off].table;

    uint32_t *piVar1 = piVar9 + 1;

    uint8_t  bVar4     = ent->end_kind;
    uint16_t cntInstr  = ent->instruction_count;
    uint32_t local_ac  = (bVar4 == 0) ? ((mode0 != 1) ? 4u : 2u) : 0u;
    uint32_t local_d0  = 0;
    jit_instruction_record_t *instrbase = ent->records;

    if (cntInstr != 0) {
        for (uint32_t i = 0; i < (uint32_t)cntInstr; i++) {
            uint8_t b1a = instrbase[i].emit_flags;
            if ((b1a >> 2) & 1) local_d0++;
        }
    }

    uint64_t local_e0 = lVar23 - 0x18 - (uint64_t)local_d0 * 4;

    const char *puVar2 = ((mode0 & 0xff) != 1) ? jit_block_end_run_scheduler : jit_block_end_switch_processor;
    {
        int64_t delta = (int64_t)(uintptr_t)puVar2 - (int64_t)(uintptr_t)(piVar9 + 2);
        int64_t rounded = (delta < 0) ? delta + 3 : delta;
        uint32_t imm26 = (uint32_t)((uint64_t)rounded >> 2) & 0x3ffffffu;
        wr32(piVar9 + 2, 0x94000000u | imm26);
    }
    wr32(piVar9 + 1, 0x36f8004cu);
    uint32_t *local_e8 = piVar9 + 3;

    jit_emitter_t st;
    memset(&st, 0, sizeof(st));

    st.arena = arena;
    st.code = (uint8_t *)local_e8;
    st.table = (uint8_t *)(uintptr_t)local_e0;
    st.flagged_count = local_d0;
    st.table_start = (uint8_t *)(uintptr_t)local_e0;
    st.resume_table = (uint32_t *)(uintptr_t)local_e0;
    st.block_code = (uint8_t *)piVar1;
    st.block_pc = param_3;
    st.cycles = local_ac;
    st.block = ent;
    st.link = ent->links;
    st.cpu = CPU(ctx);
    st.pc = param_3;
    st.is_arm9 = (uint8_t)mode0;
    st.thumb = (uint8_t)param_4;

    uint32_t runVal = param_3;

    if (cntInstr != 0) {
        int32_t step = (param_4 == 0) ? 4 : 2;

        for (uint32_t i = 0; i < (uint32_t)cntInstr; i++) {
            jit_instruction_record_t *rec = &instrbase[i];
            uint32_t mode_it = CPU(ctx)->is_arm9;
            uint32_t uVar11;

            if (mode_it == 1) {
                const nds_t *T = CPU(ctx)->machine;
                uVar11 = local_ac;
                local_ac = T->runtime.extra_cycles + (uint32_t)rec->cycles;
            } else {
                uVar11 = (uint32_t)rec->cycles << 1;
            }
            local_ac += uVar11;

            uint16_t f12 = rec->reads;
            uint16_t f14 = rec->writes;
            uint16_t f16 = rec->live_registers;
            rec->live_registers = (uint16_t)(f14 | f12 | f16);

            local_e8 = (uint32_t *)st.code;
            rec->code = (uint8_t *)local_e8;

            if ((int16_t)f14 < 0) {
                uint32_t *dst = local_e8;
                if (local_ac > 0x1000) {
                    dst = local_e8 + 1;
                    wr32(local_e8, ((local_ac >> 2) & 0x3ffffc00u) | 0x5140018cu);
                }
                local_e8 = dst + 1;
                wr32(dst, ((local_ac & 0xfffu) << 10) | 0x5100018cu);
                local_ac = 0;
            }
            st.code = (uint8_t *)local_e8;

            runVal += (uint32_t)step;
            st.pc = runVal;
            st.cycles = local_ac;
            st.record = rec;

            jit_emit_instruction(&st);

            local_e8 = (uint32_t *)st.code;
        }
        bVar4 = ent->end_kind;
    }

    uint32_t sel = bVar4;
    local_e8 = (uint32_t *)st.code;
    local_ac = st.cycles;
    uint32_t *puVar19 = local_e8;
    const char *puVar12 = NULL;
    int emit_b_final = 0;

    if (sel - 1u < 2u) {
        uint32_t u10 = (uint32_t)st.thumb;
        uint32_t u11 = (runVal & 0xffffu) | u10;
        if ((runVal & 0xffffu) == 0 && u10 == 0) {
            wr32(local_e8, ((runVal >> 11) & 0x1fffe0u) | 0x52a00000u);
            puVar19 = local_e8 + 1;
        } else if ((runVal >> 16) == 0) {
            wr32(local_e8, (u11 << 5) | 0x52800000u);
            puVar19 = local_e8 + 1;
        } else if (u11 == 0xffffu) {
            wr32(local_e8, (((runVal >> 11) & 0x1fffe0u)) ^ 0x12bfffe0u);
            puVar19 = local_e8 + 1;
        } else if ((runVal | u10) > 0xfffeffffu) {
            wr32(local_e8, (((runVal & 0xffffu) | u10) << 5) ^ 0x129fffe0u);
            puVar19 = local_e8 + 1;
        } else {
            uint32_t hi = 0, lo = 0;

            uint32_t ok = jit_emit_encode_logical_imm(runVal | u10, &hi, &lo);
            if (ok != 0) {
                wr32(local_e8, (hi << 16) | (lo << 10) | 0x320003e0u);
                puVar19 = local_e8 + 1;
            } else {
                wr32(local_e8,     (u11 << 5) | 0x52800000u);
                wr32(local_e8 + 1, ((runVal >> 11) & 0x1fffe0u) | 0x72a00000u);
                puVar19 = local_e8 + 2;
            }
        }
        st.code = (uint8_t *)puVar19;
        puVar12 = jit_dispatch_block_cache;
        emit_b_final = 1;
    } else if (sel == 3u) {
        if (local_ac > 0x1000) {
            wr32(local_e8, ((local_ac >> 2) & 0x3ffffc00u) | 0x5140018cu);
            local_e8++;
        }
        wr32(local_e8, ((local_ac & 0xfffu) << 10) | 0x5100018cu);
        local_ac = 0;
        local_e8++;
        st.code = (uint8_t *)local_e8;
        st.cycles = local_ac;

        jit_emit_link_queue_entry(&st);
        local_e8 = (uint32_t *)st.code;
        emit_b_final = 0;
    } else if (sel == 4u) {
        uint32_t u11 = runVal & 0xffffu;
        if (u11 == 0) {
            wr32(local_e8, ((runVal >> 11) & 0x1fffe0u) | 0x52a00000u);
            puVar19 = local_e8 + 1;
        } else if ((runVal >> 16) == 0) {
            wr32(local_e8, (u11 << 5) | 0x52800000u);
            puVar19 = local_e8 + 1;
        } else if (u11 == 0xffffu) {
            wr32(local_e8, ((runVal >> 11) & 0x1fffe0u) ^ 0x12bfffe0u);
            puVar19 = local_e8 + 1;
        } else if (runVal > 0xfffeffffu) {
            wr32(local_e8, (u11 << 5) ^ 0x129fffe0u);
            puVar19 = local_e8 + 1;
        } else {
            uint32_t hi = 0, lo = 0;

            uint32_t ok = jit_emit_encode_logical_imm(runVal, &hi, &lo);
            if (ok != 0) {
                wr32(local_e8, (hi << 16) | (lo << 10) | 0x320003e0u);
                puVar19 = local_e8 + 1;
            } else {
                wr32(local_e8,     (u11 << 5) | 0x52800000u);
                wr32(local_e8 + 1, ((runVal >> 11) & 0x1fffe0u) | 0x72a00000u);
                puVar19 = local_e8 + 2;
            }
        }
        st.code = (uint8_t *)puVar19;
        puVar12 = jit_dispatch_arm;
        emit_b_final = 1;
    }

    if (emit_b_final) {
        int64_t delta = (int64_t)(uintptr_t)puVar12 - (int64_t)(uintptr_t)puVar19;
        int64_t rounded = (delta < 0) ? delta + 3 : delta;
        uint32_t imm26 = (uint32_t)((uint64_t)rounded >> 2) & 0x3ffffffu;
        wr32(puVar19, 0x14000000u | imm26);
        local_e8 = puVar19 + 1;
        st.code = (uint8_t *)local_e8;
    }

    uint32_t base32 = (uint32_t)lVar24;
    jit_block_header_t *hdr = (jit_block_header_t *)(uintptr_t)(lVar23 - sizeof(jit_block_header_t));

    uint32_t *cur     = (uint32_t *)st.code;
    uint8_t  *e0_tab  = st.table;
    uint32_t  e0_hdr  = (uint32_t)(uintptr_t)st.table_start;

    wr32(piVar9, (uint32_t)(uintptr_t)hdr - base32);
    hdr->unmapped_0 = 0;
    hdr->pc = param_4 | param_3;
    hdr->code_offset = (uint32_t)(uintptr_t)piVar1 - base32;
    hdr->live_registers = ent->live_registers;
    hdr->live_flags = (uint16_t)ent->live_flags;
    {
        uint16_t f22 = ent->halfword_count;
        int16_t  delta16 = (int16_t)((uint32_t)(uintptr_t)cur - (uint32_t)(uintptr_t)piVar1);

        uint16_t field = (uint16_t)(bVar3 | ((param_4 != 0) ? 1u : 0u));
        hdr->code_bytes = (uint16_t)delta16;
        hdr->flags = (uint16_t)((f22 & JIT_BLOCK_HEADER_HALFWORDS_MASK) | ((field & 0xfu) << 12));
    }
    hdr->table_offset = e0_hdr - base32;

    if (bVar7) {
        arena->region[JIT_REGION_ITCM].code = (uint8_t *)cur;
        arena->region[JIT_REGION_ITCM].table = e0_tab;
    } else if (mode2_ok) {
        arena->region[JIT_REGION_MAIN_RAM].code = (uint8_t *)cur;
        arena->region[JIT_REGION_MAIN_RAM].table = e0_tab;
    } else {
        arena->region[JIT_REGION_OTHER].code = (uint8_t *)cur;
        arena->region[JIT_REGION_OTHER].table = e0_tab;
    }

    uint16_t cntPatch = ent->link_count;
    if (cntPatch != 0) {
        jit_link_record_t *entry = ent->links;
        for (uint32_t i = 0; i < (uint32_t)cntPatch; i++, entry++) {
            uint16_t idx = entry->record_index;
            if (idx != 0) {
                uint8_t *target = instrbase[idx].code;
                uint32_t *patchaddr = (uint32_t *)entry->branch_code;
                uint32_t old = rd32(patchaddr);
                uint32_t delta = (uint32_t)(target - (uint8_t *)patchaddr);
                wr32(patchaddr, (old & 0xfc000000u) | ((delta >> 2) & 0x3ffffffu));
            }
        }
    }

    return piVar1;
}



void jit_emit_link_queue_entry(jit_emitter_t *ctx) {

    const jit_block_analysis_t *p450 = ctx->block;
    jit_link_record_t *entry = ctx->link;

    uint64_t target = (uint64_t)(uintptr_t)entry->target_code;

    int match = 0;
    if ((p450->options >> 2) & 1) {
        const jit_link_record_t *cand = p450->links;
        match = (entry == cand);
    }

    if (target != 0) {
        if ((uint8_t)entry->flags & 1)
            target = target + 8;
    }

    if (match) {
        uint8_t *cp = ctx->code;
        wr32(cp, 0x1280000cu);
        ctx->code = cp + 4;
    }

    uint32_t flags = entry->flags;
    if ((flags >> 1) & 1) {

        jit_emit_mov_imm32(ctx, 0, entry->target_pc);

        flags = entry->flags;
        target = ((uint8_t)entry->target_pc & 1)
                     ? (uint64_t)(uintptr_t)jit_dispatch_thumb
                     : (uint64_t)(uintptr_t)jit_dispatch_arm;
    }
    if ((flags >> 2) & 1) {

        jit_emit_mov_imm32(ctx, 0, entry->target_pc);
        target = (uint64_t)(uintptr_t)jit_dispatch_block_cache;
    }

    uint8_t *cp = ctx->code;
    int64_t delta = (int64_t)target - (int64_t)(uintptr_t)cp;
    int64_t round = (delta < 0) ? delta + 3 : delta;
    uint32_t instr = 0x14000000u | (((uint32_t)round >> 2) & 0x3ffffffu);
    wr32(cp, instr);
    ctx->code = cp + 4;

    if (target == 0
        && (((uint8_t)entry->flags >> 1) & 1) == 0
        && entry->record_index == 0) {
        uint8_t **slot = entry->patch_slot;
        *slot = cp;
    }

    entry->branch_code = cp;
    ctx->link = entry + 1;
}




static uint32_t rbit32(uint32_t x)
{
    x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
    x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
    x = ((x >> 4) & 0x0f0f0f0fu) | ((x & 0x0f0f0f0fu) << 4);
    x = ((x >> 8) & 0x00ff00ffu) | ((x & 0x00ff00ffu) << 8);
    x = (x >> 16) | (x << 16);
    return x;
}

static uint32_t clz32(uint32_t x)
{
    if (x == 0)
        return 32;
    uint32_t n = 0;
    if ((x & 0xFFFF0000u) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF000000u) == 0) { n += 8;  x <<= 8;  }
    if ((x & 0xF0000000u) == 0) { n += 4;  x <<= 4;  }
    if ((x & 0xC0000000u) == 0) { n += 2;  x <<= 2;  }
    if ((x & 0x80000000u) == 0) { n += 1; }
    return n;
}

static uint32_t ctz32(uint32_t x)
{
    return clz32(rbit32(x));
}

static uint32_t asr32(uint32_t x, uint32_t s)
{
    return (uint32_t)((int32_t)x >> (s & 31u));
}

uint32_t jit_emit_encode_logical_imm(uint32_t param_1, void *param_2, void *param_3)
{

    uint32_t w0 = param_1;
    uint32_t w8, w9, w10, w11;

    w8 = w0 + 1u;
    if (!(w8 >= 2u))
        goto L96218;

    w8 = w0 & 0xffffu;
    if (w8 != (w0 >> 16))
        goto L96288;

    w9 = w0 ^ (w0 >> 8);
    if ((w9 & 0xffu) == 0)
        goto L96388;

    if (w8 == 0)
        goto L96320;

    w9 = ctz32(w8);
    w8 = w8 >> (w9 & 31u);
    w8 = w8 + 1u;
    w10 = clz32(w8);
    w8 = ctz32(w8);
    w10 = w10 + w8;
    if (w10 != 0x1fu)
        goto L96320;
    w9 = (uint32_t)(-(int32_t)w9);
    w8 = w8 - 1u;
    w10 = 0x20u;
    w9 = w9 & 0x3fu;
    w10 = (w10 & ~0xFu) | (w8 & 0xFu);
    w0 = 1u;
    memcpy(param_2, &w9, 4);
    memcpy(param_3, &w10, 4);
    return w0;

L96288:

    w8 = ctz32(w0);
    w9 = w0 >> (w8 & 31u);
    w9 = w9 + 1u;
    w10 = clz32(w9);
    w9 = ctz32(w9);
    w10 = w10 + w9;
    if (w10 != 0x1fu)
        goto L_1;
    w8 = (uint32_t)(-(int32_t)w8);
    w9 = w9 - 1u;
    w8 = w8 & 0x3fu;
    w9 = w9 & 0x1fu;
    w0 = 1u;
    memcpy(param_2, &w8, 4);
    memcpy(param_3, &w9, 4);
    return w0;

L_1:

    w8 = ~w0;
    w9 = ctz32(w8);
    w8 = w8 >> (w9 & 31u);
    w8 = w8 + 1u;
    w10 = clz32(w8);
    w8 = ctz32(w8);
    w10 = w10 + w8;
    if (w10 != 0x1fu)
        goto L96218;
    w9 = w9 + w8;
    w9 = (uint32_t)(-(int32_t)w9);
    w8 = w10 - w8;
    w9 = w9 & 0x3fu;
    w8 = w8 & 0x1fu;
    w0 = 1u;
    memcpy(param_2, &w9, 4);
    memcpy(param_3, &w8, 4);
    return w0;

L96320:

    w8 = 0xffff0000u;
    if (w8 == (w0 << 16))
        goto L96218;
    w8 = (uint32_t)(int32_t)(int16_t)(w0 & 0xffffu);
    w8 = ~w8;
    w9 = ctz32(w8);
    w8 = w8 >> (w9 & 31u);
    w8 = w8 + 1u;
    w10 = clz32(w8);
    w8 = ctz32(w8);
    w10 = w10 + w8;
    if (w10 != 0x1fu)
        goto L96218;
    w9 = w9 + w8;
    w10 = 0xfu;
    w11 = 0x20u;
    w9 = (uint32_t)(-(int32_t)w9);
    w8 = w10 - w8;
    w9 = w9 & 0x3fu;
    w11 = (w11 & ~0xFu) | (w8 & 0xFu);
    w0 = 1u;
    memcpy(param_2, &w9, 4);
    memcpy(param_3, &w11, 4);
    return w0;

L96388:

    w8 = w0 ^ (w0 >> 4);
    if ((w8 & 0xfu) == 0)
        goto L96450;

    w9 = w0 & 0xffu;
    if (w9 == 0)
        goto L_2;

    w8 = ctz32(w9);
    w9 = w9 >> (w8 & 31u);
    w9 = w9 + 1u;
    w10 = clz32(w9);
    w9 = ctz32(w9);
    w10 = w10 + w9;
    if (w10 != 0x1fu)
        goto L_2;
    w8 = (uint32_t)(-(int32_t)w8);
    w9 = w9 - 1u;
    w10 = 0x30u;
    w8 = w8 & 0x3fu;
    w10 = (w10 & ~0x7u) | (w9 & 0x7u);
    w0 = 1u;
    memcpy(param_2, &w8, 4);
    memcpy(param_3, &w10, 4);
    return w0;

L_2:

    w8 = 0xff000000u;
    if (w8 == (w0 << 24))
        goto L96218;
    w8 = (uint32_t)(int32_t)(int8_t)(w0 & 0xffu);
    w8 = ~w8;
    w9 = ctz32(w8);
    w8 = w8 >> (w9 & 31u);
    w8 = w8 + 1u;
    w10 = clz32(w8);
    w8 = ctz32(w8);
    w10 = w10 + w8;
    if (w10 != 0x1fu)
        goto L96218;
    w9 = w9 + w8;
    w10 = 0x7u;
    w11 = 0x30u;
    w9 = (uint32_t)(-(int32_t)w9);
    w8 = w10 - w8;
    w9 = w9 & 0x3fu;
    w11 = (w11 & ~0x7u) | (w8 & 0x7u);
    w0 = 1u;
    memcpy(param_2, &w9, 4);
    memcpy(param_3, &w11, 4);
    return w0;

L96450:

    w8 = w0 ^ (w0 >> 2);
    if ((w8 & 0x3u) == 0)
        goto L_4;

    w9 = w0 & 0xfu;
    if (w9 == 0)
        goto L_3;

    w8 = ctz32(w9);
    w9 = w9 >> (w8 & 31u);
    w9 = w9 + 1u;
    w10 = clz32(w9);
    w9 = ctz32(w9);
    w10 = w10 + w9;
    if (w10 != 0x1fu)
        goto L_3;
    w8 = (uint32_t)(-(int32_t)w8);
    w9 = w9 - 1u;
    w10 = 0x38u;
    w8 = w8 & 0x3fu;
    w10 = (w10 & ~0x3u) | (w9 & 0x3u);
    w0 = 1u;
    memcpy(param_2, &w8, 4);
    memcpy(param_3, &w10, 4);
    return w0;

L_3:

    w8 = w0 << 28;
    w9 = 0xf0000000u;
    if (w9 == w8)
        goto L96218;
    w9 = 0xffffffffu;
    w8 = w9 ^ asr32(w8, 28);
    w9 = ctz32(w8);
    w8 = w8 >> (w9 & 31u);
    w8 = w8 + 1u;
    w10 = clz32(w8);
    w8 = ctz32(w8);
    w10 = w10 + w8;
    if (w10 != 0x1fu)
        goto L96218;
    w9 = w9 + w8;
    w10 = 0x3u;
    w11 = 0x38u;
    w9 = (uint32_t)(-(int32_t)w9);
    w8 = w10 - w8;
    w9 = w9 & 0x3fu;
    w11 = (w11 & ~0x3u) | (w8 & 0x3u);
    w0 = 1u;
    memcpy(param_2, &w9, 4);
    memcpy(param_3, &w11, 4);
    return w0;

L_4:

    w9 = w0 & 0x3u;
    if (w9 == 0)
        goto L96570;

    w8 = ctz32(w9);
    w9 = w9 >> (w8 & 31u);
    w9 = w9 + 1u;
    w10 = clz32(w9);
    w9 = ctz32(w9);
    w10 = w10 + w9;
    if (w10 != 0x1fu)
        goto L96570;
    w8 = (uint32_t)(-(int32_t)w8);
    w9 = w9 & 0x1u;
    w10 = 0x3du;
    w8 = w8 & 0x3fu;
    w9 = w9 ^ w10;
    w0 = 1u;
    memcpy(param_2, &w8, 4);
    memcpy(param_3, &w9, 4);
    return w0;

L96570:

    w8 = w0 << 30;
    w9 = 0xc0000000u;
    if (w9 == w8)
        goto L96218;
    w9 = 0xffffffffu;
    w8 = w9 ^ asr32(w8, 30);
    w9 = ctz32(w8);
    w8 = w8 >> (w9 & 31u);
    w8 = w8 + 1u;
    w10 = clz32(w8);
    w8 = ctz32(w8);
    w10 = w10 + w8;
    if (w10 != 0x1fu)
        goto L96218;
    w9 = w9 + w8;
    w0 = 1u;
    w10 = 0x3cu;
    w9 = (uint32_t)(-(int32_t)w9);
    w8 = w0 - w8;
    w9 = w9 & 0x3fu;
    w10 = (w10 & ~0x1u) | (w8 & 0x1u);
    memcpy(param_2, &w9, 4);
    memcpy(param_3, &w10, 4);
    return w0;

L96218:

    w0 = 0u;
    return w0;
}


void jit_emit_template_call(jit_emitter_t *param_1, uint32_t param_2, uint32_t param_3,
                         uint32_t param_4, uint32_t param_5, uint32_t param_6,
                         uint32_t param_7, uint32_t param_8, uint32_t param_9)
{

    jit_emitter_t *em = param_1;
    uint64_t r0=0,r1=0,r2=0,r3=0,r8=0,r9=0,r10=0,r11=0,r12=0,r13=0,r14=0,r15=0,r16=0,r17=0;
    uint64_t r20,r21,r22,r24=0,r25,r26=0;
    double d0;
    uint32_t v0l[4], v1l[4], v2l[4], v3l[4], v4l[4], v5l[4], v6l[4];
    int i;
    unsigned carry;
    uint64_t old12;
    uint8_t *p8 = NULL, *p9 = NULL, *p10 = NULL, *p13 = NULL, *p14 = NULL, *p15 = NULL, *p16 = NULL;
    const uint8_t *p17 = NULL;
    const uint8_t *arena;
    arm_t *cpu = NULL;
    const uint32_t *s13;
    uint32_t *q12;

    r8 = (uint32_t)param_2 >> 28;
    if (r8 > 0xe) return;

    r21 = param_4;
    r3  = param_9;
    r22 = param_7;
    r20 = param_6;
    r25 = param_3;
    r8  = (param_2 >> 16) & 0xfu;
    r26 = (param_2 >> 12) & 0xfu;

    if (param_3 == 0) goto L_2;
    if (r8 != 0xf) goto L_2;
    r9 = (param_2 & 0x200000u) | r20;
    if (r9 != 0) goto L_2;
    if (((param_2 >> 24) & 1u) == 0) goto L_3;
    if (param_8 != 0) goto L_3;
    r8 = em->is_arm9;
    if (r8 != 1) goto L_3;
    r9 = ((param_2 & 0x800000u) == 0) ? (uint32_t)(-(int32_t)(uint32_t)r3) : (uint32_t)r3;
    r8 = em->record->pc_value;
    r24 = (uint32_t)(r8 + r9);
    r8 = (uint32_t)r24 >> 26;
    if (r8 != 0) goto L_3;
    r8 = (param_5 != 0) ? 1u : 0u;
    r9 = (((uint32_t)r24 & 1u) == 0) ? 1u : 0u;
    if (((uint32_t)r24 & 3u) == 0) goto L_1;
    r8 = r8 & r9;
    if (r8 & 1u) goto L_1;
    if (param_4 == 0) goto L_3;

L_1:
    cpu = em->cpu;
    r8  = (uint32_t)(r26 + 0xd);
    r20 = (r26 == 0xf) ? 0u : r8;
    r9  = (uint32_t)r24 >> 25;
    if (r9 != 0) goto L_55;
    arena = (const uint8_t *)cpu->jit_arena;
    r9 = ((uint32_t)r24 >> 2) & 0x1fffu;
    r10 = 0x014d8038u;
    r9 = rd8(arena + r9 + r10);
    if (r9 >= 8) goto L_56;
    goto L_57;

L_2:

    if (r8 != 0xd) goto L_3;
    if (r20 != 0) goto L_3;
    r8 = em->is_arm9;
    if (r8 != 1) goto L_3;
    r8 = em->cpu->cp15->dtcm_below_64mb;
    if (r8 == 0) goto L_3;
    r20 = jit_emit_operand_word(em, param_2, param_8, (uint32_t)r3);
    if (r25 == 0) goto L_45;

    r8 = (uint32_t)(r26 + 0xd);
    r9 = (r26 == 0xf) ? 0u : r8;
    if (param_5 == 0) goto L_50;
    r9 = ((uint32_t)r9 & 0x0000ffffu) | (((uint32_t)r20 & 0xffffu) << 16);
    if (r22 == 0) goto L_65;
    p8 = em->code;
    r10 = 0x78604940u;
    goto L_51;

L_3:
    r24 = jit_emit_operand_word(em, param_2, param_8, (uint32_t)r3);
    if (r25 == 0) goto L_8;
    r15 = (uint64_t)(uintptr_t)jit_mem_read8s_generic;
    r16 = (uint64_t)(uintptr_t)jit_mem_read8_generic;
    r17 = (uint64_t)(uintptr_t)jit_mem_read16s_generic;
    r0  = (uint64_t)(uintptr_t)jit_mem_read16_generic;
    r1  = (uint64_t)(uintptr_t)jit_hook_mem_read32;
    r11 = (uint32_t)(r26 + 0xd);
    r8  = 0;
    r12 = 0;
    r9  = (r26 == 0xf) ? 0u : r11;
    if (r20 != 0) goto L_14;
    {
        uint32_t is_arm9 = em->is_arm9;
        r10 = r8;
        if (is_arm9 != 1) goto L_18;
    }
    r10 = 0x34000041u;
    r14 = 0x12261401u;
    r12 = r9;
    r12 = ((uint32_t)r12 & 0x0000ffffu) | (((uint32_t)r24 & 0xffffu) << 16);
    if (param_5 == 0) goto L_5;
    p13 = em->code;
    r13 = (uint64_t)(uintptr_t)p13;
    r8  = (uint32_t)(r14 + 0x400);
    r8  = r8 | ((uint32_t)r24 << 5);
    r14 = 0x14000000u;
    wr32(p13, (uint32_t)r8);
    p8 = p13 + 8;
    r8 = r13 + 8;
    r15 = r13 + 0xb;
    r15 = ((int64_t)r8 < 0) ? r15 : r8;
    r15 = (uint32_t)(-((uint32_t)r15 >> 2));
    r14 = ((uint32_t)r14 & 0xfc000000u) | ((uint32_t)r15 & 0x03ffffffu);
    wr32(p13 + 8, (uint32_t)r14);
    r14 = 0x78604940u;
    em->code = p13 + 0xc;
    wr32(p13 + 4, (uint32_t)r10);
    if (r22 == 0) goto L_6;
L_4:
    r12 = r12 | r14;
    r12 = r12 | 0x800000u;
    p10 = p13 + 0x10;
    wr32(p13 + 12, (uint32_t)r12);
    goto L_17;

L_5:
    r8 = (uint32_t)r24 << 5;
    if (param_4 == 0) goto L_7;
    p13 = em->code;
    r13 = (uint64_t)(uintptr_t)p13;
    r8  = r8 | r14;
    r14 = 0x14000000u;
    wr32(p13, (uint32_t)r8);
    p8 = p13 + 8;
    r8 = r13 + 8;
    r15 = r13 + 0xb;
    r15 = ((int64_t)r8 < 0) ? r15 : r8;
    r15 = (uint32_t)(-((uint32_t)r15 >> 2));
    r14 = ((uint32_t)r14 & 0xfc000000u) | ((uint32_t)r15 & 0x03ffffffu);
    wr32(p13 + 8, (uint32_t)r14);
    r14 = 0x4940u;
    r14 = (r14 & 0xffffu) | (0x3860u << 16);
    em->code = p13 + 0xc;
    wr32(p13 + 4, (uint32_t)r10);
    if (r22 != 0) goto L_4;
L_6:
    r12 = r12 | r14;
    p10 = p13 + 0x10;
    wr32(p13 + 12, (uint32_t)r12);
    goto L_17;

L_7:
    r13 = (uint32_t)(r14 + 0x800);
    p14 = em->code;
    r14 = (uint64_t)(uintptr_t)p14;
    r16 = 0x4940u;
    r16 = (r16 & 0xffffu) | (0xb860u << 16);
    r13 = r8 | r13;
    p8  = p14 + 8;
    r8  = r14 + 8;
    r12 = r12 | r16;
    r16 = r14 + 0xb;

    wr32(p14, (uint32_t)r13);
    wr32(p14 + 4, (uint32_t)r10);
    r13 = ((int64_t)r8 < 0) ? r16 : r8;
    r15 = 0x14000000u;
    r13 = (uint32_t)(-((uint32_t)r13 >> 2));
    p10 = p14 + 0x10;
    r15 = ((uint32_t)r15 & 0xfc000000u) | ((uint32_t)r13 & 0x03ffffffu);
    wr32(p14 + 8, (uint32_t)r15);
    wr32(p14 + 12, (uint32_t)r12);
    goto L_17;

L_8:
    r22 = 0x3e0u;
    r22 = (r22 & 0xffffu) | (0x2a00u << 16);
    r25 = (uint32_t)r26 << 16;

    if (r26 != 0xf) goto L_9;
    r1 = 1u;
    r2 = em->record->pc_value;
    jit_emit_mov_imm32(em, (uint32_t)r1, (uint32_t)r2);
    if (r24 != 0) goto L_10;
    goto L_11;

L_9:
    p8 = em->code;
    r9 = ((uint32_t)r26 << 16) + (0xd0u << 12);
    r10 = (uint32_t)(r22 + 1u);
    r9 = r9 | r10;
    wr32(p8, (uint32_t)r9); p8 += 4;
    em->code = p8;
    if (r24 == 0) goto L_11;
L_10:
    p8 = em->code;
    r9 = (uint32_t)r22 | ((uint32_t)r24 << 16);
    wr32(p8, (uint32_t)r9); p8 += 4;
    em->code = p8;
L_11:
    if (r20 == 0) goto L_15;
    {
        uint32_t is_arm9 = em->is_arm9;
        d0  = tpl_f64(jit_tpl_read_cycle_mark_add4);
        r9  = (uint64_t)(uintptr_t)jit_mem_write32_arm7;
        p10 = em->code;
        r10 = (uint64_t)(uintptr_t)p10;
        r12 = (uint64_t)(uintptr_t)jit_mem_write32_arm9;
        r8  = r10 + 4;
        r11 = 0x120u;
        r21 = (is_arm9 == 1) ? r12 : r9;
        r8  = r21 - r8;
        r9  = r8 + 3;
        r8  = ((int64_t)r8 < 0) ? r9 : r8;
        r12 = 0x94000000u;
        r20 = 0x94000000u;
        r11 = (r11 & 0xffffu) | (0xb816u << 16);
        p9  = p10 + 0x10;
        r12 = (r12 & 0xfc000000u) | (((uint32_t)r8 >> 2) & 0x03ffffffu);
        wr_f64(p10 + 8, d0);
        wr32(p10, (uint32_t)r11); wr32(p10 + 4, (uint32_t)r12);
        em->code = p9;
        if (r26 != 0xe) goto L_12;
        r1 = 0; r2 = em->record->pc_value;
        jit_emit_mov_imm32(em, (uint32_t)r1, (uint32_t)r2);
        p9  = em->code;
        r10 = (uint32_t)(r22 + 1u);
        goto L_13;
    }

L_12:

    r8  = (uint32_t)(r25 + (0xe0u << 12));
    r10 = (uint32_t)(r22 + 1u);
    r10 = r8 | r10;
L_13:
    p8 = p9;
    r11 = (uint64_t)(uintptr_t)p9 + 4;
    wr32(p8, (uint32_t)r10); p8 += 8;
    r10 = r21 - r11;
    r11 = r10 + 3;
    r10 = ((int64_t)r10 < 0) ? r11 : r10;
    r10 = ((uint32_t)r10 >> 2) & 0x03ffffffu;
    r10 = r10 | r20;
    wr32(p9 + 4, (uint32_t)r10);
    goto L_54;

L_14:
    r10 = r8;
    r13 = 0x3e0u;
    r13 = (r13 & 0xffffu) | (0x2a00u << 16);
    if (r24 != 0) goto L_19;
    goto L_20;

L_15:
    r9 = 0x94000000u;
    if (param_5 == 0) goto L_16;
    r11 = (uint64_t)(uintptr_t)jit_mem_write16_arm7;
    r12 = (uint64_t)(uintptr_t)jit_mem_write16_arm9;
    {
        uint32_t is_arm9 = em->is_arm9;
        r10 = is_arm9;
    }
    p8 = em->code;
    goto L_53;

L_16:

    r10 = em->is_arm9;
    if (param_4 == 0) goto L_52;
    r11 = (uint64_t)(uintptr_t)jit_mem_write8_arm7;
    r12 = (uint64_t)(uintptr_t)jit_mem_write8_arm9;
    p8 = em->code;
    goto L_53;

L_17:

    r10 = (uint64_t)(uintptr_t)p10;
    em->code = p10;
    em->block_transfer_continue = p10;
    em->code = (uint8_t *)em->scratch;
    r15 = (uint64_t)(uintptr_t)jit_mem_read8s_hook;
    r16 = (uint64_t)(uintptr_t)jit_mem_read8_hook;
    r17 = (uint64_t)(uintptr_t)jit_mem_read16s_hook;
    r0  = (uint64_t)(uintptr_t)jit_mem_read16_hook;
    r1  = (uint64_t)(uintptr_t)jit_mem_read32_arm9;
    r12 = 1u;

L_18:
    r13 = 0x3e0u;
    r13 = (r13 & 0xffffu) | (0x2a00u << 16);
    if (r24 == 0) goto L_20;
L_19:
    p14 = em->code;
    r2  = r13 | ((uint32_t)r24 << 16);
    wr32(p14, (uint32_t)r2); p14 += 4;
    em->code = p14;
L_20:
    if (r20 == 0) goto L_33;
    r14 = (uint64_t)(uintptr_t)jit_mem_read32x2_hook;
    p15 = em->code;
    r15 = (uint64_t)(uintptr_t)p15;
    r16 = 0x94000000u;
    r17 = r14 - r15;
    r0  = r17 + 3;
    r17 = ((int64_t)r17 < 0) ? r0 : r17;
    p14 = p15;
    r16 = ((uint32_t)r16 & 0xfc000000u) | (((uint32_t)r17 >> 2) & 0x03ffffffu);
    wr32(p14, (uint32_t)r16); p14 += 4;
    em->code = p14;
    if (r26 != 0xf) goto L_34;
    if (r26 != 0xe) goto L_35;
L_21:
    if (r12 == 0) goto L_36;
L_22:
    if (r26 == 0xf) goto L_23;
    r9 = r9 | r13;
    wr32(p14, (uint32_t)r9); p14 += 4;
    em->code = p14;
L_23:
    r9  = r10 - (uint64_t)(uintptr_t)p14;
    r10 = r9 + 3;
    r9  = ((int64_t)r9 < 0) ? r10 : r9;
    r10 = 0x14000000u;
    r10 = ((uint32_t)r10 & 0xfc000000u) | (((uint32_t)r9 >> 2) & 0x03ffffffu);
    wr32(p14, (uint32_t)r10); p14 += 4;
    p10 = em->table;
    r10 = (uint64_t)(uintptr_t)p10;
    r13 = p14 - (uint8_t *)em->scratch;
    r11 = r13 >> 2;
    em->code = p14;
    r9 = r10 - ((uint64_t)(uint32_t)r11 << 2);
    em->table = p10 - ((uint64_t)(uint32_t)r11 << 2);
    if ((uint32_t)r11 == 0) goto L_31;
    r12 = (uint32_t)(r13 >> 2);
    r11 = (uint64_t)(uintptr_t)em->scratch - r9;
    r11 = r11 >> 2;

    if ((uint64_t)r12 < 4) goto L_24;
    r14 = (uint64_t)(uintptr_t)(em->scratch + (uint32_t)r12);
    if (r9 >= r14) goto L_30;
    if (r10 <= (uint64_t)(uintptr_t)em->scratch) goto L_30;
L_24:
    r13 = 0;
L_25:
    r12 = r13 - r12;
    s13 = em->scratch + r13;
    r14 = 0x14000000u;
L_26_loop:
    r15 = rd32(s13);
    r16 = r15 & 0x7c000000u;
    if (r16 == r14) goto L_29;
L_27:
    wr32(p10 + (r12 << 2), (uint32_t)r15);
    old12 = r12; r12 = r12 + 1; carry = (r12 < old12);
    s13 = s13 + 1;
    if (carry) goto L_31;
L_28:
    r15 = rd32(s13);
    r16 = r15 & 0x7c000000u;
    if (r16 != r14) goto L_27;
L_29:
    r16 = (uint32_t)(r15 + r11);
    r15 = ((uint32_t)r15 & 0xfc000000u) | ((uint32_t)r16 & 0x03ffffffu);
    wr32(p10 + (r12 << 2), (uint32_t)r15);
    old12 = r12; r12 = r12 + 1; carry = (r12 < old12);
    s13 = s13 + 1;
    if (!carry) goto L_28;
    goto L_31;

L_30:
    r14 = (r13 >> 2) & 0x3u;
    r13 = r12 - r14;
    for (i = 0; i < 4; i++) v0l[i] = (uint32_t)r11;
    for (i = 0; i < 4; i++) v1l[i] = 0x7c000000u;
    for (i = 0; i < 4; i++) v2l[i] = 0x14000000u;
    r15 = r13;
    p16 = em->table;
    p17 = (const uint8_t *)em->scratch;
    for (;;) {
        unsigned zflag;
        memcpy(v3l, p17, 16); p17 += 16;
        for (i = 0; i < 4; i++) v4l[i] = ~0xfc000000u;
        r15 = r15 - 4; zflag = (r15 == 0);
        for (i = 0; i < 4; i++) v5l[i] = v3l[i] & v1l[i];
        for (i = 0; i < 4; i++) v6l[i] = v3l[i] + v0l[i];
        for (i = 0; i < 4; i++) v5l[i] = (v5l[i] == v2l[i]) ? 0xffffffffu : 0u;
        for (i = 0; i < 4; i++) v4l[i] = (v4l[i] & v6l[i]) | (~v4l[i] & v3l[i]);
        for (i = 0; i < 4; i++) v5l[i] = (v5l[i] & v4l[i]) | (~v5l[i] & v3l[i]);
        memcpy(p16, v5l, 16); p16 += 16;
        if (zflag) break;
    }
    if (r14 != 0) goto L_25;
    goto L_31;

L_31:
    r10 = rd32(p8);
    r9  = (uint32_t)(r9 - (uint64_t)(uintptr_t)p8);
    r10 = ((uint32_t)r10 & 0xfc000000u) | (((uint32_t)r9 >> 2) & 0x03ffffffu);
    wr32(p8, (uint32_t)r10);
    em->code = em->block_transfer_continue;
    r8 = (uint32_t)(int32_t)(int16_t)em->record->writes;
    if (((uint32_t)r8 & 0x80000000u) == 0) return;
L_32:
    r8 = em->is_arm9;
    if (r8 == 1) goto L_38;
    r8 = em->thumb;
    if (r8 == 0) goto L_41;
    if (r8 != 1) goto L_38;
    p8 = em->code;
    r9 = 0x32000000u;
    goto L_42;

L_33:
    if (param_5 == 0) goto L_39;
    if (r22 == 0) goto L_40;
    p14 = em->code;
    r14 = (uint64_t)(uintptr_t)p14;
    r11 = r17 - r14;
    goto L_47;

L_34:
    r16 = (uint32_t)(r13 + (0x10u << 12));
    p14 = p15 + 8;
    r11 = r11 | r16;
    wr32(p15 + 4, (uint32_t)r11);
    em->code = p14;
    if (r26 == 0xe) goto L_21;
L_35:
    r11 = (uint32_t)(r26 + 0xe);
    r11 = r11 | r13;
    wr32(p14, (uint32_t)r11); p14 += 4;
    em->code = p14;
    if (r12 != 0) goto L_22;
L_36:
    if (r20 != 0) goto L_37;
    if (r9 == 0) goto L_37;
    r8 = r9 | r13;
    wr32(p14, (uint32_t)r8); p14 += 4;
    em->code = p14;
L_37:
    r8 = (uint32_t)(int32_t)(int16_t)em->record->writes;
    if (((uint32_t)r8 & 0x80000000u) == 0) return;
    goto L_32;

L_38:
    p8 = em->code;
    goto L_43;

L_39:
    if (param_4 == 0) goto L_46;
    r11 = 0x94000000u;
    if (r22 == 0) goto L_48;
    p14 = em->code;
    r14 = (uint64_t)(uintptr_t)p14;
    r15 = r15 - r14;
    goto L_49;

L_40:
    p14 = em->code;
    r14 = (uint64_t)(uintptr_t)p14;
    r11 = r0 - r14;
    goto L_47;

L_41:
    p8 = em->code;
    r9 = 0x7400u;
    r9 = (r9 & 0xffffu) | (0x123eu << 16);
L_42:
    wr32(p8, (uint32_t)r9); p8 += 4;
    em->code = p8;
L_43:
    r9 = (uint64_t)(uintptr_t)jit_dispatch_block_cache;
    r9 = r9 - (uint64_t)(uintptr_t)p8;
    r10 = r9 + 3;
    r9 = ((int64_t)r9 < 0) ? r10 : r9;
    r10 = 0x14000000u;
    r10 = ((uint32_t)r10 & 0xfc000000u) | (((uint32_t)r9 >> 2) & 0x03ffffffu);
    wr32(p8, (uint32_t)r10); p8 += 4;
L_44:
    em->code = p8;
    return;

L_45:
    if (r26 != 0xf) goto L_58;
    r1 = 2u; r22 = 2u;
    r2 = em->record->pc_value;
    jit_emit_mov_imm32(em, (uint32_t)r1, (uint32_t)r2);
    r22 = ((uint32_t)r22 & 0x0000ffffu) | (((uint32_t)r20 & 0xffffu) << 16);
    if (param_5 != 0) goto L_59;
    goto L_61;

L_46:
    p14 = em->code;
    r14 = (uint64_t)(uintptr_t)p14;
    r11 = r1 - r14;
L_47:
    r15 = r11 + 3;
    r11 = ((int64_t)r11 < 0) ? r15 : r11;
    r15 = 0x94000000u;
    r15 = ((uint32_t)r15 & 0xfc000000u) | (((uint32_t)r11 >> 2) & 0x03ffffffu);
    wr32(p14, (uint32_t)r15); p14 += 4;
    em->code = p14;
    if (r12 != 0) goto L_22;
    goto L_36;

L_48:
    p14 = em->code;
    r14 = (uint64_t)(uintptr_t)p14;
    r15 = r16 - r14;
L_49:
    r16 = r15 + 3;
    r15 = ((int64_t)r15 < 0) ? r16 : r15;
    r15 = ((uint32_t)r15 >> 2) & 0x03ffffffu;
    r11 = r15 | r11;
    wr32(p14, (uint32_t)r11); p14 += 4;
    em->code = p14;
    if (r12 != 0) goto L_22;
    goto L_36;

L_50:
    if (param_4 == 0) goto L_64;
    r9 = ((uint32_t)r9 & 0x0000ffffu) | (((uint32_t)r20 & 0xffffu) << 16);
    if (r22 == 0) goto L_66;
    p8 = em->code;
    r10 = 0x4940u;
    r10 = (r10 & 0xffffu) | (0x3860u << 16);
L_51:
    r9 = r9 | r10;
    r9 = r9 | 0x800000u;
    goto L_68;

L_52:
    r11 = (uint64_t)(uintptr_t)jit_mem_write32_arm7;
    r12 = (uint64_t)(uintptr_t)jit_mem_write32_arm9;
    p8  = em->code;
L_53:
    r10 = (r10 == 1) ? r12 : r11;
    r10 = r10 - (uint64_t)(uintptr_t)p8;
    r11 = r10 + 3;
    r10 = ((int64_t)r10 < 0) ? r11 : r10;
    r10 = ((uint32_t)r10 >> 2) & 0x03ffffffu;
    r9  = r10 | r9;
    wr32(p8, (uint32_t)r9); p8 += 4;
L_54:
    r11 = (uint32_t)(uintptr_t)em->block_code;
    r9  = em->pc;
    r10 = em->block_pc;
    em->code = p8;
    q12 = em->resume_table;
    r8  = (uint32_t)((uint32_t)(uintptr_t)p8 - (uint32_t)r11);
    r8  = (uint32_t)r8 << 14;
    r9  = (uint32_t)((uint32_t)r9 - (uint32_t)r10);
    r8  = (uint32_t)r8 & 0xffff0000u;
    r8  = r8 | r9;
    wr32(q12, (uint32_t)r8); q12 += 1;
    em->resume_table = q12;
    return;

L_55:
    r8 = (uint32_t)r24 >> 24;
    if (r8 > 2) goto L_62;
L_56:
    arena = (const uint8_t *)cpu->jit_arena;
    r10 = ((uint32_t)r24 >> 7) & 0x7fffu;
    arena = arena + ((uint64_t)r10 << 2);
    r10 = 0x014da138u;
    r8  = rd32(arena + r10);
    r9  = (uint32_t)r24 >> 2;
    r10 = 1u;
    r9  = (uint32_t)r10 << (r9 & 0x1fu);
    if ((r8 & r9) == 0) goto L_62;
L_57:
    r1 = 1u; r2 = r24;
    jit_emit_mov_imm32(em, (uint32_t)r1, (uint32_t)r2);
    if (param_5 == 0) goto L_80;
    if (r22 == 0) goto L_70;
    r8 = 0x4940u;
    p9 = em->code;
    r8 = (r8 & 0xffffu) | (0x7860u << 16);
    r8 = r20 | r8;
    r10 = 0x810000u;
    r8 = r8 | r10;
    goto L_77;

L_58:
    r22 = (uint32_t)(r26 + 0xd);
    r22 = ((uint32_t)r22 & 0x0000ffffu) | (((uint32_t)r20 & 0xffffu) << 16);
    if (param_5 == 0) goto L_61;
L_59:
    p8 = em->code;
    r9 = 0x4940u;
    r9 = (r9 & 0xffffu) | (0x7860u << 16);
L_60:
    r9 = (uint32_t)(r9 - (0x400u << 12));
    r9 = (uint32_t)r22 | r9;
    wr32(p8, (uint32_t)r9); p8 += 4;
    goto L_44;

L_61:

    if (param_4 == 0) goto L_69;
    p8 = em->code;
    r9 = 0x4940u;
    r9 = (r9 & 0xffffu) | (0x3860u << 16);
    goto L_60;

L_62:
    if (param_5 == 0) goto L_63;
    r2 = 2u; r1 = r24;
    bus_range_mark_written((unsigned char *)cpu, (uint32_t)r1, (uint32_t)r2);
    r1 = r24;
    r0 = bus_read16((unsigned char *)&cpu->pagetable, (uint32_t)r1);
    if (r22 == 0) goto L_71;
    r2 = (uint32_t)(int32_t)(int16_t)(uint16_t)r0;
    goto L_79;

L_63:
    if (param_4 == 0) goto L_74;
    r1 = (uint32_t)r24 & 0xfffffffeu;
    r2 = 2u;
    bus_range_mark_written((unsigned char *)cpu, (uint32_t)r1, (uint32_t)r2);
    r1 = r24;
    r0 = bus_read8((unsigned char *)&cpu->pagetable, (uint32_t)r1);
    if (r22 == 0) goto L_78;
    r2 = (uint32_t)(int32_t)(int8_t)(uint8_t)r0;
    goto L_79;

L_64:
    p8 = em->code;
    r10 = 0x4940u;
    r9 = ((uint32_t)r9 & 0x0000ffffu) | (((uint32_t)r20 & 0xffffu) << 16);
    r10 = (r10 & 0xffffu) | (0xb860u << 16);
    goto L_67;

L_65:
    p8 = em->code;
    r10 = 0x4940u;
    r10 = (r10 & 0xffffu) | (0x7860u << 16);
    goto L_67;

L_66:
    p8 = em->code;
    r10 = 0x4940u;
    r10 = (r10 & 0xffffu) | (0x3860u << 16);
L_67:
    r9 = r9 | r10;
L_68:
    wr32(p8, (uint32_t)r9); p8 += 4;
    em->code = p8;
    r9 = (uint32_t)(int32_t)(int16_t)em->record->writes;
    if (((uint32_t)r9 & 0x80000000u) == 0) return;
    goto L_43;

L_69:
    p8 = em->code;
    r9 = 0x4940u;
    r9 = (r9 & 0xffffu) | (0xb860u << 16);
    goto L_60;

L_70:
    p8 = em->code;
    r9 = 0x4940u;
    r9 = (r9 & 0xffffu) | (0x7860u << 16);
    goto L_73;

L_71:
    r2 = (uint32_t)r0 & 0xffffu;
    goto L_79;

L_72:
    p8 = em->code;
    r9 = 0x4940u;
    r9 = (r9 & 0xffffu) | (0xb860u << 16);
L_73:
    r9 = r20 | r9;
    r9 = r9 | 0x10000u;
    wr32(p8, (uint32_t)r9); p8 += 4;
    em->code = p8;
    r8 = (uint32_t)(int32_t)(int16_t)em->record->writes;
    if (((uint32_t)r8 & 0x80000000u) == 0) return;
    goto L_32;

L_74:
    r2 = 4u; r1 = r24;
    bus_range_mark_written((unsigned char *)cpu, (uint32_t)r1, (uint32_t)r2);
    r1 = r24;
    r0 = bus_read32((unsigned char *)&cpu->pagetable, (uint32_t)r1);
    r2 = (uint32_t)r0;
    goto L_79;

L_75:
    p9 = em->code;
    r8 = (uint32_t)(r8 + (0x10u << 12));
L_76:
    r8 = r20 | r8;
L_77:
    wr32(p9, (uint32_t)r8); p9 += 4;
    em->code = p9;
    r8 = (uint32_t)(int32_t)(int16_t)em->record->writes;
    if (((uint32_t)r8 & 0x80000000u) == 0) return;
    goto L_32;

L_78:
    r2 = (uint32_t)r0 & 0xffu;
L_79:
    r1 = r20;
    jit_emit_mov_imm32(em, (uint32_t)r1, (uint32_t)r2);
    r8 = (uint32_t)(int32_t)(int16_t)em->record->writes;
    if (((uint32_t)r8 & 0x80000000u) == 0) return;
    goto L_32;

L_80:
    if (param_4 == 0) goto L_72;
    r8 = 0x4940u;
    r8 = (r8 & 0xffffu) | (0x3860u << 16);
    if (r22 == 0) goto L_75;
    p9 = em->code;
    r8 = (uint32_t)(r8 + (0x810u << 12));
    goto L_76;
}

#define EMIT_PTR  1032
#define OTHER_PTR     1112
#define OP_MOV   0x2a0003e0u
#define OP_A     0x51000000u
#define OP_B     0x11000000u





static uint32_t bfi(uint32_t d, uint32_t s, int lsb, int n)
{
    uint32_t m = (n >= 32) ? 0xffffffffu : ((1u << n) - 1u);
    return (d & ~(m << lsb)) | ((s & m) << lsb);
}

uint32_t jit_emit_operand_word(void *ctx, uint32_t desc, uint32_t mode, uint32_t val)
{
    uint8_t *c = (uint8_t *)ctx;

    if (mode == 1) return jit_emit_operand_word_ext((jit_emitter_t *)(c), desc, 0);
    if (mode == 2) return jit_emit_operand_word_ext((jit_emitter_t *)(c), desc, 1);

    uint32_t reg = (desc >> 16) & 0xf;
    uint32_t rd  = reg + 13;

    if (val == 0 && reg != 15) return rd;

    if (!(desc & (1u << 24))) {

        uint8_t *p = rd_ptr_u8(c + EMIT_PTR);
        uint32_t a = bfi(OP_MOV, rd, 16, 5);
        uint32_t b = bfi(rd, val, 10, 22);
        wr32(p, a);
        b = bfi(b, rd, 5, 5);
        wr_ptr(c + EMIT_PTR, p + 4);
        wr32(p + 4, b | ((desc & (1u << 23)) ? OP_B : OP_A));
        wr_ptr(c + EMIT_PTR, p + 8);
        return 0;
    }

    if (reg == 15) {
        uint8_t *q = rd_ptr_u8(c + OTHER_PTR);

        uint32_t d = (desc & 0x800000u) ? val : (uint32_t)(-(int32_t)val);

        jit_emit_mov_imm32((jit_emitter_t *)c, 0, rd32(q + 4) + d);
        return 0;
    }

    uint32_t ret = rd & (uint32_t)(((int32_t)(desc << 10)) >> 31);
    uint32_t v   = bfi(val << 10, rd, 5, 5) | ret;
    uint8_t *p   = rd_ptr_u8(c + EMIT_PTR);
    v |= (desc & (1u << 23)) ? OP_B : OP_A;
    wr32(p, v);
    wr_ptr(c + EMIT_PTR, p + 4);
    return ret;
}
#undef EMIT_PTR
#undef OTHER_PTR
#undef OP_MOV
#undef OP_A
#undef OP_B



static uint32_t read_ctxval(jit_emitter_t *param_1)
{
    const jit_instruction_record_t *ctxp = param_1->record;
    return ctxp->pc_value;
}

uint32_t jit_emit_operand_word_ext(jit_emitter_t *param_1, uint32_t param_2, int32_t param_3)
{

    uint32_t n0f = param_2 & 0xfu;
    uint32_t hi4 = (param_2 >> 16) & 0xfu;

    uint32_t val_a;
    if (n0f == 0xf) {
        uint32_t ctxv = read_ctxval(param_1);
        jit_emit_mov_imm32(param_1, 0, ctxv);
        val_a = 0;
    } else {
        val_a = n0f + 0xd;
    }

    uint32_t val_b;
    if (hi4 == 0xf) {
        uint32_t ctxv = read_ctxval(param_1);
        jit_emit_mov_imm32(param_1, 1, ctxv);
        val_b = 1;
    } else {
        val_b = hi4 + 0xd;
    }

    uint32_t p7_5 = (param_2 >> 7) & 0x1fu;
    uint32_t p5_2 = (param_2 >> 5) & 0x3u;

    uint32_t raw6 = (param_3 != 0) ? p5_2 : 0u;
    uint32_t raw2 = (param_3 != 0) ? p7_5 : 0u;
    int bVar1 = (raw2 != 0);

    uint32_t mode, valA_o_1f, valC;

    if (raw6 == 1 && !bVar1) {
        mode = 0;
        valA_o_1f = 0x1fu;
    } else {
        mode = raw6;
        valA_o_1f = val_a;
    }

    if (!bVar1 && mode == 2) {
        valC = 0x1fu;
    } else {
        valC = raw2;
    }

    if (mode == 3) {
        unsigned char *outp = param_1->code;
        uint32_t w9v, w12v;

        if (valC == 0) {
            uint32_t word0 = ((valA_o_1f & 0x1fu) << 5) | 0x123f7802u;
            wr32(outp, word0);
            wr32(outp + 4, 0x1a1f0042u);
            outp += 8;
            w9v = 0x20000u;
            w12v = 0x400u;
        } else {
            w9v = valA_o_1f << 16;
            w12v = valC << 10;
        }

        wr32(outp, w9v | w12v | 0x2ac003e2u);
        outp += 4;

        mode = 0;
        valC = 0;
        valA_o_1f = 2;

        param_1->code = outp;
    }

    if ((param_2 & (1u << 24)) == 0) {
        unsigned char *outp = param_1->code;

        uint32_t word1 = 0x2a0003e0u | ((val_b & 0x1fu) << 16);
        uint32_t packed = (val_b & 0x1fu)
                         | ((val_b & 0x1fu) << 5)
                         | ((valA_o_1f & 0x1fu) << 16)
                         | ((mode & 0x3u) << 22)
                         | ((valC & 0x1fu) << 10);
        uint32_t sel = (param_2 & (1u << 23)) ? 0xb000000u : 0x4b000000u;
        uint32_t word2 = packed | sel;

        wr32(outp, word1);
        wr32(outp + 4, word2);
        outp += 8;

        param_1->code = outp;
        return 0;
    } else {
        unsigned char *outp = param_1->code;

        uint32_t ret = ((param_2 >> 21) & 1u) ? val_b : 0u;
        uint32_t packed = ret
                         | ((val_b & 0x1fu) << 5)
                         | ((valA_o_1f & 0x1fu) << 16)
                         | ((mode & 0x3u) << 22)
                         | ((valC & 0x1fu) << 10);
        uint32_t sel = (param_2 & (1u << 23)) ? 0xb000000u : 0x4b000000u;
        uint32_t word = packed | sel;

        wr32(outp, word);
        outp += 4;

        param_1->code = outp;
        return ret;
    }
}




static uint32_t bl_rel(const void *dest, const void *here) {
    int64_t diff = (int64_t)((uintptr_t)dest - (uintptr_t)here);
    int64_t adjusted = (diff < 0) ? diff + 3 : diff;
    uint32_t bits26 = ((uint32_t)adjusted >> 2) & 0x3ffffffu;
    return 0x94000000u | bits26;
}

void jit_emit_dispatch_by_flags(jit_emitter_t *ctx, unsigned int param_2) {

    if (param_2 & (1u << 22)) {
        uint32_t field = (param_2 >> 16) & 0xfu;
        uint8_t *p = ctx->code;

        if (field == 0xf) {
            wr32(p, 0x2a1f03e1u);
            p += 4;
        } else if (field == 0) {
            uint64_t dword = jit_tpl_zero_w0_ones_w1;
            wr64(p, dword);
            p += 8;
        } else {
            uint32_t r = field ^ 0xfu;
            static const uint8_t tab0_bits[16] = { 0x00, 0x00, 0x38, 0x00, 0x30, 0x00, 0x38, 0x00, 0x28, 0x28, 0x38, 0x28, 0x30, 0x30, 0x38, 0x00 };
            static const uint8_t tab1_bits[16] = { 0x00, 0x07, 0x07, 0x0f, 0x07, 0x27, 0x0f, 0x17, 0x07, 0x0f, 0x27, 0x17, 0x0f, 0x17, 0x17, 0x00 };
            const uint8_t *tab0 = tab0_bits;
            const uint8_t *tab1 = tab1_bits;
            uint32_t w0 = ((uint32_t)tab0[field] << 16) |
                          ((uint32_t)tab1[field] << 10) | 0x12000000u;
            uint32_t w1 = ((uint32_t)tab0[r] << 16) |
                          ((uint32_t)tab1[r] << 10) | 0x320003e1u;
            wr32(p,     w0);
            wr32(p + 4, w1);
            p += 8;
        }

        const char *dest = jit_spsr_write;
        wr32(p, bl_rel(dest, p));
        p += 4;
        ctx->code = p;
        return;
    }

    if (param_2 & (1u << 19)) {
        uint8_t *p = ctx->code;
        wr32(p, 0xd51b4200u);
        p += 4;
        ctx->code = p;
    }

    if (param_2 & (1u << 16)) {
        uint8_t *p = ctx->code;

        const char *dest = jit_cpsr_write_mode;
        wr32(p, bl_rel(dest, p));
        p += 4;
        ctx->code = p;

        uint32_t base_408  = (uint32_t)(uintptr_t)ctx->block_code;
        uint32_t field_448 = ctx->pc;
        uint32_t field_440 = ctx->block_pc;
        uint8_t *q = (uint8_t *)ctx->resume_table;

        uint32_t height = ((uint32_t)(uintptr_t)p - base_408) << 14;
        uint32_t low = field_448 - field_440;
        height &= 0xffff0000u;
        wr32(q, height | low);
        q += 4;
        ctx->resume_table = (uint32_t *)q;
    }
}

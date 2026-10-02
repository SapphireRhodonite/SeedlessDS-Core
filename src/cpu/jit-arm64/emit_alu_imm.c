#include "hires_runtime.h"
#include "blob_symbols.h"
#include "emit_contract.h"
#include "jit.h"
#include "jit_hooks.h"

extern void jit_emit_dispatch_by_flags(jit_emitter_t *param_1, unsigned int param_2);
extern uint32_t jit_emit_encode_logical_imm(uint32_t param_1, void *param_2, void *param_3);

static inline uint32_t ror32(uint32_t v, uint32_t rot)
{
    return (rot == 0) ? v : ((v >> rot) | (v << (32u - rot)));
}

static inline void emit_word(jit_emitter_t *machine, uint32_t word)
{
    unsigned char *cur = machine->code;
    wr32(cur, word);
    machine->code = (unsigned char *)(cur + 4);
}

static inline uint32_t encode_ands_self(uint32_t rdHost)
{
    return UINT32_C(0x6a000000) | rdHost | (rdHost << 5) | (rdHost << 16);
}

static inline uint32_t bit_field_insert(uint32_t d, uint32_t s, unsigned lsb, unsigned width)
{
    uint32_t mask = (width >= 32u) ? 0xffffffffu : (((uint32_t)1 << width) - 1u);
    return (d & ~(mask << lsb)) | ((s & mask) << lsb);
}

unsigned char *jit_emit_arm_data_processing_imm(jit_emitter_t *machine, uint32_t w20)
{

    uint32_t w10 = (w20 >> 28) & 15u;
    uint32_t w11 = (w20 >> 29) & 7u;

    unsigned char *x22;
    if (w11 > 6u) {

        x22 = NULL;
    } else {

        x22 = machine->code;

        int64_t x22i = (int64_t)(uintptr_t)x22;
        uint64_t sel = (x22i < 0) ? (uint64_t)(x22i + 3) : (uint64_t)x22i;

        uint32_t w11n = 0u - ((uint32_t)sel >> 2);
        uint32_t w10b = bit_field_insert(w10, w11n, 5, 19);
        uint32_t w9 = w10b ^ UINT32_C(0x54000001);

        wr32(x22, w9);
        machine->code = (unsigned char *)(x22 + 4);
    }

    int pathA = ((w20 & UINT32_C(0x1900000)) == UINT32_C(0x1000000)) &&
                ((w20 & UINT32_C(0xf000)) == UINT32_C(0xf000));

    if (pathA) {

        uint32_t w8v = w20 & UINT32_C(0xff);
        uint32_t rot = (w20 >> 7) & UINT32_C(0x1e);
        uint32_t w2 = ror32(w8v, rot);

        jit_emit_mov_imm32(machine, 0u, w2);
        jit_emit_dispatch_by_flags(machine, w20);

        return x22;
    }

    uint32_t w26  = (w20 >> 21) & 0xfu;
    uint32_t w23  = w20 & 0xffu;
    uint32_t w27  = (w20 >> 7) & UINT32_C(0x1e);
    uint32_t w24  = 0u - w27;
    uint32_t w22  = ror32(w23, w27);
    uint32_t w21pre = w20 & UINT32_C(0x1800000);
    uint32_t w8pre = w23 << (w24 & 31u);

    uint32_t w15;
    if ((w26 | 2u) == 0xfu) {
        w15 = 0xffu;
    } else {
        uint32_t w9b = (w20 >> 16) & 0xfu;
        if (w9b == 0xfu) {

            const jit_instruction_record_t *ctx = machine->record;
            uint32_t w2 = ctx->pc_value;
            jit_emit_mov_imm32(machine, 0u, w2);
            w15 = 0u;
        } else {
            w15 = w9b + 0xdu;
        }
    }

    uint32_t w13, w14, w21, w28;
    int logical_with_s = 0;
    if (w21pre == UINT32_C(0x1000000)) {

        w13 = 0u; w21 = 0xffu; w14 = 0u; w28 = 1u;

        logical_with_s = ((UINT32_C(1) << w26) & UINT32_C(0xf303)) != 0u;
    } else {

        uint32_t w10rd = (w20 >> 12) & 0xfu;
        w21 = (w10rd == 0xfu) ? 0u : (w10rd + 0xdu);
        w14 = 0u;
        w28 = 0u;
        w13 = 1u;
        if ((w20 & (UINT32_C(1) << 20)) != 0u) {

            w14 = 0u; w28 = 1u;
            logical_with_s = ((UINT32_C(1) << w26) & UINT32_C(0xf303)) != 0u;
        }
    }

    if (logical_with_s) {

        const jit_instruction_record_t *p1112 = machine->record;
        if ((p1112->live_flags & 3u) != 0u) {
            unsigned char *cur = machine->code;
            wr32(cur, UINT32_C(0xd53b4202));
            machine->code = (unsigned char *)(cur + 4);
            w14 = 1u;
            if (w27 != 0u) {
                uint32_t bit = UINT32_C(1) << ((w27 - 1u) & 31u);

                wr32(cur + 4, ((w22 & bit) != 0u)
                                   ? UINT32_C(0x32230042)
                                   : UINT32_C(0x12227842));
                machine->code = (unsigned char *)(cur + 8);
            }
        } else {
            w14 = 0u;
        }
        w28 = 1u;
    }

    switch (w26) {

    case 0u: {
        uint32_t rotc = 0, szc = 0;
        if (jit_emit_encode_logical_imm(w22, &rotc, &szc)) {

            uint32_t w8 = w21 | (w15 << 5);
            w8 = bit_field_insert(w8, rotc, 16, 16);
            w8 |= (szc << 10);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x12000000)
                                             : UINT32_C(0x72000000)));
        } else {

            jit_emit_mov_imm32(machine, 1u, w23);
            uint32_t w8 = (w15 << 5) | (w27 << 10) | w21;
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x0ac10000)
                                             : UINT32_C(0x6ac10000)));
        }
        break;
    }

    case 1u: {

        uint32_t rotc = 0, szc = 0;
        if (jit_emit_encode_logical_imm(w22, &rotc, &szc)) {
            uint32_t w9 = w21 | (w15 << 5);
            w9 = bit_field_insert(w9, rotc, 16, 16);
            w9 |= (szc << 10);
            emit_word(machine, w9 | UINT32_C(0x52000000));
        } else {

            jit_emit_mov_imm32(machine, 1u, w23);
            uint32_t w9 = (w27 << 10) | (w15 << 5) | w21;
            emit_word(machine, w9 | UINT32_C(0x4ac10000));
        }
        if (w28 != 0u)
            emit_word(machine, encode_ands_self(w21));
        break;
    }

    case 2u: {
        if (w22 < UINT32_C(0x1000)) {
            uint32_t w8 = w21 | (w15 << 5) | (w22 << 10);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x51000000)
                                             : UINT32_C(0x71000000)));
        } else if ((w8pre & UINT32_C(0xfff000)) == w22) {
            uint32_t imm = w8pre >> 12;
            uint32_t w8 = w21 | (w15 << 5) | (imm << 10) | UINT32_C(0x400000);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x51000000)
                                             : UINT32_C(0x71000000)));
        } else {

            jit_emit_mov_imm32(machine, 1u, w22);
            uint32_t w8 = w21 | (w15 << 5);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x4b010000)
                                             : UINT32_C(0x6b010000)));
        }
        break;
    }

    case 3u: {
        uint32_t w21b;
        if (w23 == 0u) {

            w21b = bit_field_insert(w21, w15, 16, 8);
            emit_word(machine, w21b | (w28 == 0u ? UINT32_C(0x4b0003e0)
                                               : UINT32_C(0x6b0003e0)));
        } else {
            jit_emit_mov_imm32(machine, 1u, w22);
            w21b = bit_field_insert(w21, w15, 16, 8);
            emit_word(machine, w21b | (w28 == 0u ? UINT32_C(0x4b000020)
                                               : UINT32_C(0x6b000020)));
        }
        break;
    }

    case 4u: {
        if (w22 < UINT32_C(0x1000)) {
            uint32_t w8 = w21 | (w15 << 5) | (w22 << 10);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x11000000)
                                             : UINT32_C(0x31000000)));
        } else if ((w8pre & UINT32_C(0xfff000)) == w22) {
            uint32_t imm = w8pre >> 12;
            uint32_t w8 = w21 | (w15 << 5) | (imm << 10) | UINT32_C(0x400000);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x11000000)
                                             : UINT32_C(0x31000000)));
        } else {

            jit_emit_mov_imm32(machine, 1u, w22);
            uint32_t w8 = w21 | (w15 << 5);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x0b010000)
                                             : UINT32_C(0x2b010000)));
        }
        break;
    }

    case 5u: {
        jit_emit_mov_imm32(machine, 1u, w22);
        uint32_t w8 = w21 | (w15 << 5);
        emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x1a010000)
                                         : UINT32_C(0x3a010000)));
        break;
    }

    case 6u: {
        jit_emit_mov_imm32(machine, 1u, w22);
        uint32_t w8 = w21 | (w15 << 5);
        emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x5a010000)
                                         : UINT32_C(0x7a010000)));
        break;
    }

    case 7u: {
        jit_emit_mov_imm32(machine, 1u, w22);
        uint32_t w21b = bit_field_insert(w21, w15, 16, 8);
        emit_word(machine, w21b | (w28 == 0u ? UINT32_C(0x5a000020)
                                           : UINT32_C(0x7a000020)));
        break;
    }

    case 8u: {
        uint32_t rotc = 0, szc = 0;
        if (jit_emit_encode_logical_imm(w22, &rotc, &szc)) {
            uint32_t w8 = (w15 << 5);
            w8 = bit_field_insert(w8, rotc, 16, 16);
            w8 |= (szc << 10);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x12000003)
                                             : UINT32_C(0x72000003)));
        } else {

            jit_emit_mov_imm32(machine, 1u, w23);
            uint32_t w8 = (w15 << 5) | (w27 << 10);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x0ac10003)
                                             : UINT32_C(0x6ac10003)));
        }
        break;
    }

    case 9u: {

        uint32_t rotc = 0, szc = 0;
        if (jit_emit_encode_logical_imm(w22, &rotc, &szc)) {
            uint32_t w9 = (rotc << 16);
            w9 = bit_field_insert(w9, w15, 5, 8);
            w9 |= (szc << 10);
            emit_word(machine, w9 | UINT32_C(0x52000003));
        } else {

            jit_emit_mov_imm32(machine, 1u, w23);
            uint32_t w9 = (w27 << 10) | (w15 << 5);
            emit_word(machine, w9 | UINT32_C(0x4ac10003));
        }
        if (w28 != 0u)
            emit_word(machine, UINT32_C(0x6a030063));
        break;
    }

    case 10u: {
        if (w22 < UINT32_C(0x1000)) {
            uint32_t w8 = (w22 << 10) | (w15 << 5);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x51000003)
                                             : UINT32_C(0x71000003)));
        } else if ((w8pre & UINT32_C(0xfff000)) == w22) {
            uint32_t imm = w8pre >> 12;
            uint32_t w8 = (imm << 10) | (w15 << 5) | UINT32_C(0x400000);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x51000003)
                                             : UINT32_C(0x71000003)));
        } else {

            jit_emit_mov_imm32(machine, 1u, w22);
            uint32_t w8 = (w15 << 5);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x4b010003)
                                             : UINT32_C(0x6b010003)));
        }
        break;
    }

    case 11u: {
        if (w22 < UINT32_C(0x1000)) {
            uint32_t w8 = (w22 << 10) | (w15 << 5);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x11000003)
                                             : UINT32_C(0x31000003)));
        } else if ((w8pre & UINT32_C(0xfff000)) == w22) {
            uint32_t imm = w8pre >> 12;
            uint32_t w8 = (imm << 10) | (w15 << 5) | UINT32_C(0x400000);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x11000003)
                                             : UINT32_C(0x31000003)));
        } else {

            jit_emit_mov_imm32(machine, 1u, w22);
            uint32_t w8 = (w15 << 5);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x0b010003)
                                             : UINT32_C(0x2b010003)));
        }
        break;
    }

    case 12u: {

        uint32_t rotc = 0, szc = 0;
        if (jit_emit_encode_logical_imm(w22, &rotc, &szc)) {
            uint32_t w9 = w21 | (w15 << 5);
            w9 = bit_field_insert(w9, rotc, 16, 16);
            w9 |= (szc << 10);
            emit_word(machine, w9 | UINT32_C(0x32000000));
        } else {

            jit_emit_mov_imm32(machine, 1u, w23);
            uint32_t w9 = (w27 << 10) | (w15 << 5) | w21;
            emit_word(machine, w9 | UINT32_C(0x2ac10000));
        }
        if (w28 != 0u)
            emit_word(machine, encode_ands_self(w21));
        break;
    }

    case 13u: {

        jit_emit_mov_imm32(machine, w21, w22);
        if (w28 != 0u)
            emit_word(machine, encode_ands_self(w21));
        break;
    }

    case 14u: {
        uint32_t rotc = 0, szc = 0;
        if (jit_emit_encode_logical_imm(~w22, &rotc, &szc)) {

            uint32_t w8 = w21 | (w15 << 5);
            w8 = bit_field_insert(w8, rotc, 16, 16);
            w8 |= (szc << 10);
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x12000000)
                                             : UINT32_C(0x72000000)));
        } else {

            jit_emit_mov_imm32(machine, 1u, w23);
            uint32_t w8 = (w15 << 5) | (w27 << 10) | w21;
            emit_word(machine, w8 | (w28 == 0u ? UINT32_C(0x0ae10000)
                                             : UINT32_C(0x6ae10000)));
        }
        break;
    }

    case 15u: {

        jit_emit_mov_imm32(machine, w21, ~w22);
        if (w28 != 0u)
            emit_word(machine, encode_ands_self(w21));
        break;
    }

    default:
        break;
    }

    if (w14 != 0u) {
        unsigned char *cur = machine->code;
        wr32(cur,     UINT32_C(0xd53b4201));
        wr32(cur + 4, UINT32_C(0x33007441));
        wr32(cur + 8, UINT32_C(0xd51b4200) | 1u);
        machine->code = (unsigned char *)(cur + 12);
    }

    if (w13 == 0u)
        return x22;
    {
        const jit_instruction_record_t *ctxc = machine->record;
        int16_t end = (int16_t)ctxc->writes;
        if (end >= 0)
            return x22;
    }
    {
        unsigned char *cur;
        uint64_t dest;

        if ((w20 & (UINT32_C(1) << 20)) != 0u) {

            cur = machine->code;
            dest = (uint64_t)(uintptr_t)jit_irq_enter;
        } else {
            uint8_t mode = machine->thumb;
            cur = machine->code;
            if (mode == 0u) {
                wr32(cur, UINT32_C(0x123e7400));
                cur += 4;
                machine->code = (unsigned char *)cur;
            } else if (mode == 1u) {
                wr32(cur, UINT32_C(0x32000000));
                cur += 4;
                machine->code = (unsigned char *)cur;
            }
            dest = (uint64_t)(uintptr_t)jit_dispatch_block_cache;
        }

        {
            int64_t d  = (int64_t)dest - (int64_t)(uintptr_t)cur;
            int64_t dr = (d < 0) ? d + 3 : d;
            wr32(cur,
                      (UINT32_C(0x14000000) & ~UINT32_C(0x03ffffff))
                      | ((uint32_t)(dr >> 2) & UINT32_C(0x03ffffff)));
            machine->code = (unsigned char *)(cur + 4);
        }
    }

    return x22;
}

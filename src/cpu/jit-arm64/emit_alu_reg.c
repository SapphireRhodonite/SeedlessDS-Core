#include "hires_runtime.h"
#include "cpu/arm.h"
#include "blob_symbols.h"
#include "emit_contract.h"
#include "core/nds_state.h"
#include "jit.h"
#include "jit_hooks.h"
#include "core_internals.h"
#define CPU(p) ((arm_t *)(p))

extern void jit_emit_dispatch_by_flags(jit_emitter_t *param_1, unsigned int param_2);

unsigned char *jit_emit_arm_data_processing_reg(jit_emitter_t *machine, uint32_t w20)
{
    uint32_t w10     = (w20 >> 28) & 15u;
    uint32_t w11_sel = (w20 >> 29) & 7u;

    unsigned char *x22;

    if (w11_sel <= 6u) {

        x22 = machine->code;
        uint32_t w11 = w10;
        {
            int64_t  x22i = (int64_t)(uintptr_t)x22;
            uint64_t sel  = (x22i < 0) ? (uint64_t)(x22i + 3) : (uint64_t)x22i;
            uint32_t w12  = 0u - ((uint32_t)sel >> 2);
            w11 = (w11 & ~UINT32_C(0x00ffffe0)) | ((w12 & UINT32_C(0x7ffff)) << 5);
            w11 = w11 ^ UINT32_C(0x54000001);
        }
        unsigned char *cur = x22;
        wr32(cur, w11);
        cur += 4;
        machine->code = (unsigned char *)cur;
    } else {

        x22 = NULL;
    }

    if ((w20 & UINT32_C(0x90)) == UINT32_C(0x90)) {

        if ((w20 & UINT32_C(0x60)) == 0u) {

            uint32_t mop  = (w20 >> 21) & 0xfu;
            unsigned char *x28 = x22;

            if (mop > 7u) {

                if (mop != 8u && mop != 10u) {
                    return x22;
                }

                int esByte = (mop == 10u);
                uint32_t rnF2 = (w20 >> 16) & 0xfu;
                uint32_t rmF2 = w20 & 0xfu;
                uint32_t rdF2 = (w20 >> 12) & 0xfu;

                uint32_t rnS;
                if (rnF2 == 0xfu) {
                    const jit_instruction_record_t *cx = machine->record;
                    jit_emit_mov_imm32(machine, 0u,
                                       cx->pc_value);
                    rnS = 0u;
                } else {
                    rnS = rnF2 + 0xdu;
                }

                uint32_t rmS;
                if (rmF2 == 0xfu) {
                    const jit_instruction_record_t *cx = machine->record;
                    jit_emit_mov_imm32(machine, 1u,
                                       cx->pc_value);
                    rmS = 1u;
                } else {
                    rmS = rmF2 + 0xdu;
                }

                const uint32_t movBase = UINT32_C(0x2a0003e0);
                uint32_t rdH   = (rdF2 == 0xfu) ? 0u : (rdF2 + 0xdu);
                uint32_t movRn = movBase | (rnS << 16);

                unsigned char *x14 = machine->code;
                if (rnS != 0u) {
                    wr32(x14, movRn);
                    x14 += 4;
                    machine->code = (unsigned char *)x14;
                }

                {
                    uint64_t hRead = (uint64_t)(uintptr_t)(esByte ? jit_mem_read8_generic
                                                                : jit_hook_mem_read32);
                    int64_t d  = (int64_t)hRead - (int64_t)(uintptr_t)x14;
                    int64_t dr = (d < 0) ? d + 3 : d;
                    wr32(x14, (((uint32_t)dr >> 2) & UINT32_C(0x03ffffff))
                                   | UINT32_C(0x94000000));
                    machine->code = (unsigned char *)(x14 + 4);
                }

                uint64_t hWr;
                {
                    uint8_t modeM = machine->is_arm9;
                                                            hWr = (uint64_t)(uintptr_t)(modeM == 1u
                              ? (esByte ? jit_mem_write8_arm9 : jit_mem_write32_arm9)
                              : (esByte ? jit_mem_write8_arm7 : jit_mem_write32_arm7));
                }

                unsigned char *x11 = x14 + 4;

                if (rdF2 == rnF2 || rdF2 == rmF2) {

                    const uint32_t sturW0 = UINT32_C(0xb8160120);

                    wr32(x14 + 4, sturW0);
                    x11 = x14 + 8;
                    machine->code = (unsigned char *)x11;

                    if (rnS != 0u) {
                        wr32(x14 + 8, movRn);
                        x11 = x14 + 12;
                        machine->code = (unsigned char *)x11;
                    }
                    if (rmS != 1u) {
                        wr32(x11, (movBase + 1u) | (rmS << 16));
                        x11 += 4;
                        machine->code = (unsigned char *)x11;
                    }

                    {
                        int64_t d  = (int64_t)hWr - (int64_t)(uintptr_t)x11;
                        int64_t dr = (d < 0) ? d + 3 : d;
                        wr32(x11, (((uint32_t)dr >> 2) & UINT32_C(0x03ffffff))
                                       | UINT32_C(0x94000000));
                    }

                    wr32(x11 + 4, rdH | (sturW0 + UINT32_C(0x400000)));

                    {
                        uint32_t w15t = (uint32_t)(uintptr_t)machine->block_code;
                        uint32_t w9t  = machine->pc;
                        uint32_t w14t = machine->block_pc;
                        uint32_t w12t = (uint32_t)(uintptr_t)(x11 + 4) - w15t;
                        w12t = (w12t << 14) & UINT32_C(0xffff0000);
                        w9t  = (w9t - w14t) | w12t;
                        uint32_t *lst = machine->resume_table;
                        wr32(lst, w9t);
                        machine->resume_table = lst + 1;
                    }

                    machine->code = (unsigned char *)(x11 + 8);
                    x22 = x28;

                    return x22;
                }

                if (rdF2 != 0xfu) {
                    wr32(x14 + 4, rdH | movBase);
                    x11 = x14 + 8;
                    machine->code = (unsigned char *)x11;
                }
                if (rnS != 0u) {
                    wr32(x11, movRn);
                    x11 += 4;
                    machine->code = (unsigned char *)x11;
                }
                if (rmS != 1u) {
                    wr32(x11, (movBase + 1u) | (rmS << 16));
                    x11 += 4;
                    machine->code = (unsigned char *)x11;
                }
                {

                    int64_t d  = (int64_t)hWr - (int64_t)(uintptr_t)x11;
                    int64_t dr = (d < 0) ? d + 3 : d;
                    wr32(x11, (((uint32_t)dr >> 2) & UINT32_C(0x03ffffff))
                                   | UINT32_C(0x94000000));
                    x11 += 4;
                    machine->code = (unsigned char *)x11;
                }
                x22 = x28;
                {

                    uint32_t w8t  = machine->pc;
                    uint32_t w9t  = machine->block_pc;
                    uint32_t w10t = (uint32_t)(uintptr_t)machine->block_code;
                    uint32_t diff  = (uint32_t)(uintptr_t)x11 - w10t;
                    diff = (diff << 14) & UINT32_C(0xffff0000);
                    uint32_t *lst = machine->resume_table;
                    wr32(lst, diff | (w8t - w9t));
                    machine->resume_table = lst + 1;
                }

                return x22;
            }

            uint32_t rmF   = w20 & 0xfu;
            uint32_t rsF   = (w20 >> 8) & 0xfu;
            uint32_t rdloF = (w20 >> 12) & 0xfu;
            uint32_t rdhiF = (w20 >> 16) & 0xfu;

            if ((w20 & (UINT32_C(1) << 23)) == 0u) {

                uint32_t rdF = rdhiF;
                uint32_t rnF = rdloF;

                if (rmF == 0xfu) {

                    return x22;
                }
                uint32_t rmH2 = rmF + 0xdu;

                if (rsF == 0xfu) {

                    return x22;
                }
                uint32_t rsH2 = rsF + 0xdu;

                uint32_t rdH2 = (rdF == 0xfu) ? 0u : (rdF + 0xdu);

                uint32_t w21c = 0u;
                {
                    const jit_instruction_record_t *ctxs = machine->record;
                    if ((ctxs->live_flags & 3u) != 0u &&
                        (w20 & (UINT32_C(1) << 20)) != 0u) {
                        unsigned char *c = machine->code;
                        wr32(c, UINT32_C(0xd53b4202) | 1u);
                        machine->code = (unsigned char *)(c + 4);
                        w21c = 1u;
                    }
                }

                uint32_t fieldRa;
                uint32_t topmad;
                if (w20 & (UINT32_C(1) << 21)) {
                    if (rnF == 0xfu) {

                        const jit_instruction_record_t *ctxr = machine->record;
                        jit_emit_mov_imm32(machine, 2u,
                                           ctxr->pc_value);
                        fieldRa = UINT32_C(0x800);
                    } else {

                        fieldRa = (rnF + 0xdu) << 10;
                    }
                    topmad = UINT32_C(0x1b000000);
                } else {
                    fieldRa = 0u;
                    topmad = UINT32_C(0x1b007c00);
                }

                unsigned char *cm = machine->code;
                wr32(cm, topmad | rdH2 | (rmH2 << 5) | fieldRa | (rsH2 << 16));
                cm += 4;
                machine->code = (unsigned char *)cm;

                if (w20 & (UINT32_C(1) << 20)) {

                    wr32(cm, UINT32_C(0x6a000000) | rdH2 |
                                  (rdH2 << 5) | (rdH2 << 16));
                    cm += 4;
                    machine->code = (unsigned char *)cm;
                }

                x22 = x28;

                if (w21c) {

                    wr32(cm,     UINT32_C(0xd53b4200));
                    wr32(cm + 4, UINT32_C(0x33007460));
                    wr32(cm + 8, UINT32_C(0xd51b4200));
                    machine->code = (unsigned char *)(cm + 12);
                }

                return x22;
            }

            if (rmF == 0xfu) {

                return x22;
            }
            uint32_t rmH = rmF + 0xdu;

            if (rsF == 0xfu) {

                return x22;
            }
            uint32_t rsH = rsF + 0xdu;

            uint32_t w21f = 0u;
            {
                const jit_instruction_record_t *ctxm = machine->record;
                if ((ctxm->live_flags & 3u) != 0u &&
                    (w20 & (UINT32_C(1) << 20)) != 0u) {
                    unsigned char *cur = machine->code;
                    wr32(cur, UINT32_C(0xd53b4202) + 2u);
                    machine->code = (unsigned char *)(cur + 4);
                    w21f = 1u;
                }
            }

            if (w20 & (UINT32_C(1) << 21)) {

                uint32_t rdloA, rdhiA;

                if (rdloF == 0xfu) {

                    const jit_instruction_record_t *cxa = machine->record;
                    jit_emit_mov_imm32(machine, 2u,
                                       cxa->pc_value);
                    rdloA = 2u;
                } else {
                    rdloA = rdloF + 0xdu;
                }
                if (rdhiF == 0xfu) {

                    const jit_instruction_record_t *cxb = machine->record;
                    jit_emit_mov_imm32(machine, 3u,
                                       cxb->pc_value);
                    rdhiA = 3u;
                } else {
                    rdhiA = rdhiF + 0xdu;
                }

                unsigned char *ca = machine->code;

                wr32(ca, UINT32_C(0xb3607c00) | rdloA | (rdhiA << 5));
                machine->code = (unsigned char *)(ca + 4);

                {
                    uint32_t w10a = (rsH << 16) | (rmH << 5)
                                  | rdloA | (rdloA << 10);
                    if (w20 & (UINT32_C(1) << 22))
                        w10a |= UINT32_C(0x9b200000);
                    else
                        w10a = w10a + UINT32_C(0x9b210000)
                                    + UINT32_C(0x7f0000);
                    wr32(ca + 4, w10a);
                }

                machine->code = (unsigned char *)(ca + 8);
                wr32(ca + 8, UINT32_C(0xd360fc00) | rdhiA | (rdloA << 5));
                machine->code = (unsigned char *)(ca + 12);

                unsigned char *ca2 = ca + 12;
                if (w20 & (UINT32_C(1) << 20)) {

                    wr32(ca + 12, UINT32_C(0xea000000) | rdloA |
                                       (rdloA << 5) | (rdloA << 16));
                    ca2 = ca + 16;
                    machine->code = (unsigned char *)ca2;
                }

                x22 = x28;

                if (w21f) {

                    wr32(ca2,     UINT32_C(0xd53b4200));
                    wr32(ca2 + 4, UINT32_C(0x33007480));
                    wr32(ca2 + 8, UINT32_C(0xd51b4200));
                    machine->code = (unsigned char *)(ca2 + 12);
                }

                return x22;
            }

            uint32_t rdloH = (rdloF == 0xfu) ? 2u : (rdloF + 0xdu);
            uint32_t rdhiH = (rdhiF == 0xfu) ? 3u : (rdhiF + 0xdu);

            uint32_t base = (rsH << 16) | (rmH << 5) | rdloH;

            uint32_t topmul = (w20 & (UINT32_C(1) << 22))
                            ? UINT32_C(0x9b207c00)
                            : UINT32_C(0x9ba07c00);

            unsigned char *cur = machine->code;
            wr32(cur, base | topmul);

            machine->code = (unsigned char *)(cur + 4);
            wr32(cur + 4, UINT32_C(0xd360fc00) | rdhiH | (rdloH << 5));
            machine->code = (unsigned char *)(cur + 8);

            unsigned char *cur2 = cur + 8;
            if (w20 & (UINT32_C(1) << 20)) {

                wr32(cur + 8, UINT32_C(0xea000000) | rdloH |
                                   (rdloH << 5) | (rdloH << 16));
                cur2 = cur + 12;
                machine->code = (unsigned char *)cur2;
            }

            x22 = x28;

            if (w21f) {

                wr32(cur2,     UINT32_C(0xd53b4200));
                wr32(cur2 + 4, UINT32_C(0x33007480));
                wr32(cur2 + 8, UINT32_C(0xd51b4200));
                machine->code = (unsigned char *)(cur2 + 12);
            }

            return x22;
        }

        uint32_t w5 = 0u;
        uint32_t w2 = (w20 >> 20) & 1u;
        uint32_t x8b = (w20 >> 6) & UINT32_C(0x3ffffff);
        uint32_t w4 = (w20 >> 5) & 1u;

        if ((x8b & 1u) != 0u && w2 == 0u) {
            if (w20 & (UINT32_C(1) << 12)) {
                return x22;
            }
            w2 = w4 ^ 1u;
            w5 = 1u;
        }
        uint32_t w6 = (w20 >> 6) & 1u;

        if (w20 & (UINT32_C(1) << 22)) {

            uint32_t w8i = ((w20 >> 4) & UINT32_C(0xf0)) | (w20 & 0xfu);
            jit_emit_template_call(machine, w20, w2, 1u,
                               w4, w5, w6, 0u, w8i);
            return x22;
        }

        jit_emit_template_call(machine, w20, w2, 1u,
                           w4, w5, w6, 1u, 0u);
        return x22;
    }
    if ((w20 & UINT32_C(0x1900000)) != UINT32_C(0x1000000)) {

        uint32_t opcode = (w20 >> 21) & 0xfu;
        uint32_t esMovMvn = ((opcode | 2u) == 0xfu);
        uint32_t w21pre = w20 & UINT32_C(0x1800000);
        uint32_t rmField = w20 & 0xfu;

        uint32_t rmHost = rmField + 0xdu;

        uint32_t rnHost;
        if (esMovMvn) {
            rnHost = 0xffu;
        } else {
            uint32_t rnField = (w20 >> 16) & 0xfu;
            if (rnField == 0xfu) {

                const jit_instruction_record_t *ctx = machine->record;
                uint32_t w2 = ctx->pc_value;
                jit_emit_mov_imm32(machine, 0u, w2);
                rnHost = 0u;
            } else {
                rnHost = rnField + 0xdu;
            }
        }

        int isComparison = (w21pre == UINT32_C(0x1000000));
        uint32_t rdField = (w20 >> 12) & 0xfu;
        uint32_t rdHost;
        if (isComparison) {
            rdHost = 0xffu;
        } else {
            rdHost = (rdField == 0xfu) ? 0u : (rdField + 0xdu);
        }

        uint32_t shiftType = (w20 >> 5) & 3u;
        uint32_t shiftImm  = (w20 >> 7) & 0x1fu;
        uint32_t sBit = (w20 >> 20) & 1u;

        uint32_t w17 = 0u;
        if (sBit != 0u && ((UINT32_C(0xf203) >> opcode) & 1u) != 0u) {
            const jit_instruction_record_t *ctxf = machine->record;
            if ((ctxf->live_flags & 3u) != 0u) {
                unsigned char *cur = machine->code;
                wr32(cur, UINT32_C(0xd53b4202));
                machine->code = (unsigned char *)(cur + 4);
                w17 = 1u;
            }
        }

        int perReg = 0;
        int w10_skips    = 0;
        if ((w20 & UINT32_C(0x10)) != 0u) {
            uint32_t rsField = (w20 >> 8) & 0xfu;
            if (rsField == 0xfu) {

                return x22;
            }
            uint32_t rsHost = rsField + 0xdu;
            if (rmField == 0xfu) {

                return x22;
            }
            if (w17 != 0u) {

                unsigned char *base = machine->code;
                unsigned char *x11, *x12;

                uint32_t andMask = UINT32_C(0x12001c03) | (rsHost << 5);

                if (rmHost != 1u) {

                    wr32(base, UINT32_C(0x2a0003e0)
                                    | (rmHost << 16) | 1u);
                    x11 = base + 4;
                    machine->code = (unsigned char *)x11;
                    wr32(x11, andMask);
                    x12 = x11 + 4;
                    machine->code = (unsigned char *)x12;
                } else {
                    x11 = base;
                    wr32(x11, andMask);
                    x12 = x11 + 4;
                    machine->code = (unsigned char *)x12;
                }

                {

                    static const char *const ofs[4] = { jit_shift_lsl_reg,
                                                        jit_shift_lsr_reg,
                                                        jit_shift_asr_reg,
                                                        jit_shift_ror_reg };
                    uint64_t stub = (uint64_t)(uintptr_t)ofs[shiftType & 3u];
                    int64_t  d  = (int64_t)stub - (int64_t)(uintptr_t)x12;
                    int64_t  dr = (d < 0) ? d + 3 : d;
                    wr32(x12,
                              (UINT32_C(0x94000000) & ~UINT32_C(0x03ffffff))
                              | ((uint32_t)(dr >> 2) & UINT32_C(0x03ffffff)));
                }
                machine->code = (unsigned char *)(x11 + 8);

                rmHost    = 1u;
                shiftImm  = 0u;
                shiftType = 0u;
                perReg = 1;
                goto after_offset;
            }

            uint32_t w8f  = ((opcode != 13u) ? 1u : 0u) | sBit;
            uint32_t w11d = (w8f == 0u) ? rdHost : 1u;

            w10_skips = (w8f == 0u);

            uint32_t w13d = w11d | (rsHost << 16);
            unsigned char *cur = machine->code;

            if (shiftType == 3u) {

                wr32(cur, w13d | (rmHost << 5)
                             | UINT32_C(0x1ac02000) | UINT32_C(0xc00));
                machine->code = (unsigned char *)(cur + 4);
            } else {

                uint32_t p1 = UINT32_C(0x123b0803) | (rsHost << 5);
                uint32_t p2 = w13d | (rmHost << 5) | UINT32_C(0x1ac02000);

                if (shiftType == 1u)
                    p2 |= UINT32_C(0x400);
                else if (shiftType == 2u)
                    p2 |= UINT32_C(0x800);

                wr32(cur,     p1);
                wr32(cur + 4, p2);

                if (shiftType == 2u) {

                    wr32(cur + 8,  UINT32_C(0x34000043));
                    wr32(cur + 12, w11d | UINT32_C(0x131f7c20));
                    machine->code = (unsigned char *)(cur + 16);
                } else {

                    unsigned char *end = cur + 12;
                    if (w11d != UINT32_C(0x1f)) {
                        wr32(cur + 12, UINT32_C(0x2a0003e0) | w11d
                                          | UINT32_C(0x1f0000));
                        end = cur + 16;
                    }
                    machine->code = (unsigned char *)end;
                    {
                        uint32_t jump = (uint32_t)(end - (cur + 8));
                        wr32(cur + 8, UINT32_C(0x34000003)
                                         | ((jump << 3) & UINT32_C(0xffffe0)));
                    }
                }
            }

            rmHost    = 1u;
            shiftImm  = 0u;
            shiftType = 0u;
            perReg = 1;
        }
    after_offset:

        if (!perReg && opcode == 13u && rdField == rmField &&
            shiftType == 0u && shiftImm == 0u && sBit == 0u) {

            return x22;
        }

        if (!perReg && rmField == 0xfu) {

            const jit_instruction_record_t *ctxp = machine->record;
            jit_emit_mov_imm32(machine, 1u,
                               ctxp->pc_value);
            rmHost = 1u;
        }

        if (!perReg && w17 != 0u) {

            uint32_t posBit  = 0u;
            int hasCarry = 1;
            int carryRrx = 0;

            if (shiftType == 3u) {

                if (shiftImm == 0u) {
                    carryRrx = 1;
                    hasCarry = 0;
                } else {
                    posBit = shiftImm - 1u;
                }
            } else if (shiftType == 0u) {
                if (shiftImm == 0u)
                    hasCarry = 0;
                posBit = 32u - shiftImm;
            } else {
                posBit = (shiftImm == 0u) ? 31u
                                          : (shiftImm - 1u);
            }

            if (carryRrx) {

                unsigned char *cc = machine->code;
                wr32(cc, UINT32_C(0x33030062) + (rmHost << 5)
                            - UINT32_C(0x60));
                machine->code = (unsigned char *)(cc + 4);
            } else if (hasCarry) {
                unsigned char *cc = machine->code;
                wr32(cc, UINT32_C(0x53000003) | (posBit << 16)
                            | (posBit << 10) | (rmHost << 5));
                wr32(cc + 4, UINT32_C(0x33030062));
                machine->code = (unsigned char *)(cc + 8);
            }
        }

        if (!perReg) {

            uint32_t esLsr0 = (shiftType == 1u) && (shiftImm == 0u);
            if (esLsr0) {
                shiftType = 0u;
                rmHost    = 31u;
            }
            if (shiftImm == 0u && shiftType == 2u)
                shiftImm = 31u;

            if (shiftType == 3u) {

                if (shiftImm == 0u) {
                    unsigned char *cr =
                        machine->code;
                    wr32(cr,     UINT32_C(0x123f7801)
                                    | ((rmHost & 0x1fu) << 5));
                    wr32(cr + 4, UINT32_C(0x1a1f0021));
                    machine->code = (unsigned char *)(cr + 8);
                    rmHost   = 1u;
                    shiftImm = 1u;
                }
                if (opcode <= 0xbu &&
                    ((UINT32_C(0xc14) >> opcode) & 1u) != 0u) {
                    unsigned char *cr =
                        machine->code;
                    wr32(cr, UINT32_C(0x2ac003e1)
                                | (shiftImm << 10)
                                | ((rmHost & 0x1fu) << 16));
                    machine->code = (unsigned char *)(cr + 4);
                    shiftType = 0u;
                    rmHost    = 1u;
                    shiftImm  = 0u;
                }

            }
        }

        uint32_t fields = (rnHost << 5) | (rmHost << 16) | (shiftImm << 10) | (shiftType << 22);
        uint32_t word;

        uint32_t rmEff = rmHost;
        if ((opcode == 3u || opcode == 5u || opcode == 6u || opcode == 7u) &&
            shiftImm != 0u) {
            uint32_t pre = UINT32_C(0x2a0003e0) | 1u | (shiftImm << 10)
                         | (rmHost << 16) | (shiftType << 22);
            unsigned char *pcur = machine->code;
            wr32(pcur, pre);
            machine->code = (unsigned char *)(pcur + 4);
            rmEff = 1u;
        }

        uint32_t extra = 0u;

        switch (opcode) {
        case 4u:
            word = rdHost | fields | (sBit == 0u ? UINT32_C(0x0b000000) : UINT32_C(0x2b000000));
            break;

        case 10u:

            word = fields | (sBit == 0u ? UINT32_C(0x4b000003)
                                        : UINT32_C(0x6b000003));
            break;

        case 2u:

            word = rdHost | fields | (sBit == 0u ? UINT32_C(0x4b000000) : UINT32_C(0x6b000000));
            break;

        case 5u:

            word = rdHost | (rnHost << 5) | (rmEff << 16)
                 | (sBit == 0u ? UINT32_C(0x1a000000)
                               : UINT32_C(0x3a000000));
            break;

        case 12u:

            word = rdHost | fields | UINT32_C(0x2a000000);
            if (sBit != 0u)
                extra = UINT32_C(0x6a000000) | rdHost | (rdHost << 5)
                      | (rdHost << 16);
            break;

        case 13u:
            if (shiftImm == 0u && shiftType == 0u) {

                word = rdHost | (rmHost << 5) | (rmHost << 16) |
                       (sBit == 0u ? UINT32_C(0x0a000000) : UINT32_C(0x6a000000));
                break;
            }

            word = rdHost | (rmHost << 16) | (shiftImm << 10) | (shiftType << 22) | UINT32_C(0x2a0003e0);
            if (sBit != 0u)
                extra = UINT32_C(0x6a000000) | rdHost | (rdHost << 5)
                      | (rdHost << 16);
            break;

        case 14u:
            word = rdHost | fields | (sBit == 0u ? UINT32_C(0x0a200000) : UINT32_C(0x6a200000));
            break;

        case 0u:

            word = rdHost | fields | (sBit == 0u ? UINT32_C(0x0a000000)
                                                 : UINT32_C(0x6a000000));
            break;

        case 1u:

            word = rdHost | fields | UINT32_C(0x4a000000);
            if (sBit != 0u)
                extra = UINT32_C(0x6a000000) | rdHost | (rdHost << 5)
                      | (rdHost << 16);
            break;

        case 3u:

            word = rdHost | (rmEff << 5) | (rnHost << 16)
                 | (sBit == 0u ? UINT32_C(0x4b000000)
                               : UINT32_C(0x6b000000));
            break;

        case 6u:

            word = rdHost | (rnHost << 5) | (rmEff << 16)
                 | (sBit == 0u ? UINT32_C(0x5a000000)
                               : UINT32_C(0x7a000000));
            break;

        case 7u:

            word = rdHost | (rmEff << 5) | (rnHost << 16)
                 | (sBit == 0u ? UINT32_C(0x5a000000)
                               : UINT32_C(0x7a000000));
            break;

        case 8u:

            word = fields | (sBit == 0u ? UINT32_C(0x0a000003)
                                        : UINT32_C(0x6a000003));
            break;

        case 9u:

            word = fields | UINT32_C(0x4a000003);
            if (sBit != 0u)
                extra = UINT32_C(0x6a030063);
            break;

        case 11u:

            word = fields | (sBit == 0u ? UINT32_C(0x0b000003)
                                        : UINT32_C(0x2b000003));
            break;

        case 15u:

            word = rdHost | (rmHost << 16) | (shiftImm << 10) | (shiftType << 22)
                 | UINT32_C(0x2a2003e0);
            if (sBit != 0u)
                extra = UINT32_C(0x6a000000) | rdHost | (rdHost << 5)
                      | (rdHost << 16);
            break;

        default:

            return x22;
        }

        if (w10_skips) {

            word = 0u; extra = 0u; (void)word;
        } else {
            unsigned char *cur = machine->code;
            wr32(cur, word);
            machine->code = (unsigned char *)(cur + 4);
            if (extra != 0u) {

                wr32(cur + 4, extra);
                machine->code = (unsigned char *)(cur + 8);
            }
        }

        if (w17 != 0u) {

            unsigned char *cur = machine->code;
            wr32(cur,     UINT32_C(0xd53b4201));
            wr32(cur + 4, UINT32_C(0x33007441));
            wr32(cur + 8, UINT32_C(0xd51b4200) | 1u);
            machine->code = (unsigned char *)(cur + 12);
        }

        if (isComparison)
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

            if (sBit != 0u) {

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
    if (w20 & (UINT32_C(1) << 7)) {

        {
            const arm_t *ctxh = machine->cpu;
            if (ctxh->is_arm9 != 1u) {

                return x22;
            }
        }
        {
            uint32_t vari = (w20 >> 21) & 3u;
            if (vari == 1u || vari == 2u) {

                return x22;
            }

            uint32_t rmF = w20 & 0xfu;
            uint32_t rsF = (w20 >> 8) & 0xfu;
            unsigned char *x25 = x22;

            if (rmF == 0xfu) {

                return x22;
            }
            uint32_t rmH5 = (rmF + 0xdu) << 5;

            if (rsF == 0xfu) {

                return x22;
            }
            uint32_t rsH5 = (rsF + 0xdu) << 5;

            uint32_t rdF   = (w20 >> 16) & 0xfu;
            uint32_t mold = UINT32_C(0x13003c00);
            uint32_t rdH   = (rdF == 0xfu) ? 0u : (rdF + 0xdu);

            unsigned char *cb = machine->code;

            {
                uint32_t p1;
                if (w20 & (UINT32_C(1) << 5)) {

                    p1 = (rmH5 | UINT32_C(0x13017c00)) + UINT32_C(0xf0000);
                } else {
                    p1 = rmH5 | mold;
                }
                wr32(cb, p1);
            }

            {
                uint32_t p2 = ((w20 & UINT32_C(0x40)) != 0u)
                              ? UINT32_C(0x13107c01)
                              : (mold + 1u);
                wr32(cb + 4, p2 | rsH5);
            }
            machine->code = (unsigned char *)(cb + 4);
            machine->code = (unsigned char *)(cb + 8);

            if (w20 & (UINT32_C(1) << 21)) {

                wr32(cb + 8, rdH | UINT32_C(0x1b007c00)
                                      | UINT32_C(0x10000));
                machine->code = (unsigned char *)(cb + 12);
                x22 = x25;

                return x22;
            }

            uint32_t rnF = (w20 >> 12) & 0xfu;
            uint32_t rnH;
            unsigned char *x8;
            if (rnF == 0xfu) {

                const jit_instruction_record_t *cxn = machine->record;
                jit_emit_mov_imm32(machine, 2u,
                                   cxn->pc_value);
                rnH = 2u;
                x8  = machine->code;
            } else {
                rnH = rnF + 0xdu;
                x8  = cb + 8;
            }

            {
                uint32_t p3 = rnH | (rnH << 5)
                            | (mold + UINT32_C(0x4000));
                uint32_t p4 = rdH | (rnH << 10)
                            | UINT32_C(0x9b210000);
                uint32_t p5 = (rdH << 5) | (rdH << 16)
                            | UINT32_C(0xcb20c000);

                wr32(x8,      p3);
                wr32(x8 + 4,  p4);
                wr32(x8 + 8,  p5);
                wr32(x8 + 12, UINT32_C(0xb4000080));
                wr32(x8 + 16, UINT32_C(0xb963c381));
                wr32(x8 + 20, UINT32_C(0x32250021));
                wr32(x8 + 24, UINT32_C(0xb923c381));
                machine->code = (unsigned char *)(x8 + 0x1c);
            }
            x22 = x25;

            return x22;
        }
    }
    if (w20 & (UINT32_C(1) << 4)) {

        uint32_t w11_b65 = (w20 >> 5) & 3u;

        if (w11_b65 == 0u) {
            if (w20 & (UINT32_C(1) << 22)) {

                const arm_t *ctxc = machine->cpu;
                if (ctxc->is_arm9 != 1u) {

                    return x22;
                }

                uint32_t rmC = w20 & 0xfu;
                uint32_t rdC = (w20 >> 12) & 0xfu;
                uint32_t insn_word = UINT32_C(0x5ac01000);

                if (rmC == 0xfu) {

                    const jit_instruction_record_t *cx = machine->record;
                    jit_emit_mov_imm32(machine, 0u,
                                       cx->pc_value);
                } else {
                    insn_word |= (rmC + 0xdu) << 5;
                }

                uint32_t rdHostC;
                if (rdC == 0xfu) {

                    const jit_instruction_record_t *cx = machine->record;
                    jit_emit_mov_imm32(machine, 0u,
                                       cx->pc_value);
                    rdHostC = 0u;
                } else {
                    rdHostC = rdC + 0xdu;
                }

                {
                    unsigned char *cur =
                        machine->code;
                    wr32(cur, insn_word | rdHostC);
                    machine->code = (unsigned char *)(cur + 4);
                }

                return x22;
            }

        } else if (w11_b65 == 1u) {

            const arm_t *ctx = machine->cpu;
            if (ctx->is_arm9 != 1u) {

                const struct cp15 *x22b = ctx->cp15;
                uint32_t w2 = machine->pc;
                unsigned char *x8 = machine->code;
                const uint32_t w21c = UINT32_C(0xb963c380);
                const uint32_t w20c = UINT32_C(0x528000a1);
                uint64_t x25 = (uint64_t)(uintptr_t)jit_bank_select_hook;
                unsigned char *x9;

                {
                    int64_t d = (int64_t)x25 - (int64_t)(uintptr_t)(x8 + 4);
                    int64_t dr = (d < 0) ? d + 3 : d;
                    wr32(x8, w20c);
                    wr32(x8 + 4,
                              ((uint32_t)(dr >> 2) & UINT32_C(0x03ffffff))
                              | UINT32_C(0x94000000));
                    machine->code = (unsigned char *)(x8 + 8);
                }
                jit_emit_mov_imm32(machine, 0x1bu, w2);

                x9 = machine->code;
                wr32(x9, w21c);
                x8 = x9 + 4;
                machine->code = (unsigned char *)x8;
                if (machine->thumb != 0u) {
                    wr32(x9 + 4, UINT32_C(0x323b0000));
                    x8 = x9 + 8;
                    machine->code = (unsigned char *)x8;
                }

                {
                    uint32_t field = arm_mode_by_bank[5];
                    wr32(x8,      UINT32_C(0xb920ff80));
                    wr32(x8 + 4,  UINT32_C(0x123a6400));
                    wr32(x8 + 8,
                              UINT32_C(0x11000000) | (field << 10));
                    wr32(x8 + 12, UINT32_C(0x32390000));
                    wr32(x8 + 16, UINT32_C(0xb923c380));
                    x9 = x8 + 0x14;
                    machine->code = (unsigned char *)x9;
                }

                if (x22b != NULL) {

                    wr32(x8 + 20, UINT32_C(0xf9512b80));
                    wr32(x8 + 24, UINT32_C(0xb9401000));
                    x9 = x8 + 0x1c;
                    machine->code = (unsigned char *)x9;
                    wr32(x8 + 28, UINT32_C(0x11001000));
                } else {

                    wr32(x9, w20c - UINT32_C(0x21));
                }

                {
                    int64_t d = (int64_t)(uint64_t)(uintptr_t)jit_dispatch_block_cache
                              - (int64_t)(uintptr_t)(x9 + 4);
                    int64_t dr = (d < 0) ? d + 3 : d;
                    wr32(x9 + 4,
                              (UINT32_C(0x14000000) & ~UINT32_C(0x03ffffff))
                              | ((uint32_t)(dr >> 2) & UINT32_C(0x03ffffff)));
                    machine->code = (unsigned char *)(x9 + 8);
                }

                return x22;
            }

        } else {
            return x22;
        }

        uint32_t rm = w20 & 0xfu;
        const uint32_t w21 = UINT32_C(0x2a0003e0);
        unsigned char *cur;
        int has_link;

        if (rm == 0xfu) {

            const jit_instruction_record_t *p1112 = machine->record;
            jit_emit_mov_imm32(machine, 1u,
                               p1112->pc_value);

            cur = machine->code;
            wr32(cur, w21 + UINT32_C(0x10000));
            cur += 4;
            machine->code = (unsigned char *)cur;
            has_link = (w20 & (UINT32_C(1) << 5)) != 0u;
        } else {

            uint32_t word = (rm << 16) + UINT32_C(0xd0000);
            word |= w21;
            cur = machine->code;
            wr32(cur, word);
            cur += 4;
            machine->code = (unsigned char *)cur;
            has_link = (w20 & (UINT32_C(1) << 5)) != 0u;
        }

        if (has_link) {

            uint32_t w8 = machine->pc;
            uint32_t w9 = (uint32_t)machine->thumb;
            jit_emit_mov_imm32(machine, 0x1bu, w8 | w9);
            cur = machine->code;
        }

        {
            int64_t d = (int64_t)(uint64_t)(uintptr_t)jit_dispatch_block_cache
                      - (int64_t)(uintptr_t)cur;
            int64_t dr = (d < 0) ? d + 3 : d;
            uint32_t w10 = (UINT32_C(0x14000000) & ~UINT32_C(0x03ffffff))
                         | ((uint32_t)(dr >> 2) & UINT32_C(0x03ffffff));
            wr32(cur, w10);
            cur += 4;
            machine->code = (unsigned char *)cur;
        }

        return x22;
    }
    if (w20 & (UINT32_C(1) << 21)) {

        uint32_t rm = w20 & 0xfu;
        if (rm != 0xfu) {

            uint32_t hostreg = rm + 0xdu;
            uint32_t word = (hostreg << 16);
            word |= UINT32_C(0x2a0003e0);
            unsigned char *cur = machine->code;
            wr32(cur, word);
            cur += 4;
            machine->code = (unsigned char *)cur;

            jit_emit_dispatch_by_flags(machine, w20);
            return x22;
        }

        return x22;
    }

    uint32_t w8 = (w20 >> 12) & 15u;
    uint32_t w9 = w8 + UINT32_C(0xd);
    w8 = (w8 == 0xfu) ? 0u : w9;

    if (w20 & (UINT32_C(1) << 22)) {

        unsigned char *x9c = machine->code;
        wr32(x9c,     UINT32_C(0xd10ba121));
        wr32(x9c + 4, UINT32_C(0xb9610782));
        wr32(x9c + 8, w8 | UINT32_C(0xb8625820));
        machine->code = (unsigned char *)(x9c + 12);
        return x22;
    }

    unsigned char *x9 = machine->code;
    uint32_t ww10 = (UINT32_C(0xd51b4200) | w8) | UINT32_C(0x200000);
    uint32_t ww11 = (UINT32_C(0xb963c380)) | 1u;
    uint32_t ww12 = UINT32_C(0x33006c20) | w8;
    wr32(x9,     ww10);
    wr32(x9 + 4, ww11);
    wr32(x9 + 8, ww12);

    uint8_t flag = machine->thumb;
    machine->code = (unsigned char *)(x9 + 12);

    if (flag != 0u) {

        uint32_t src_low5 = w8 & UINT32_C(0x1f);
        w8 = (w8 & ~(UINT32_C(0x1f) << 5)) | (src_low5 << 5);
        w8 = w8 | UINT32_C(0x323b0000);
        wr32(x9 + 12, w8);
        machine->code = (unsigned char *)(x9 + 16);
    }

    return x22;
}

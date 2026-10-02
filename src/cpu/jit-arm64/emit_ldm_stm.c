#include "hires_runtime.h"
#include "blob_symbols.h"
#include "emit_contract.h"
#include <stdint.h>
#include <string.h>
#include "jit.h"
#include "jit_hooks.h"
#include "mem_access.h"

static const char *const jit_stm_table[16] = {
    jit_stm_1,  jit_stm_2,  jit_stm_3,  jit_stm_4,
    jit_stm_5,  jit_stm_6,  jit_stm_7,  jit_stm_8,
    jit_stm_9,  jit_stm_10, jit_stm_11, jit_stm_12,
    jit_stm_13, jit_stm_14, jit_stm_15, jit_stm_16,
};

static const char *const jit_ldm_table[16] = {
    jit_ldm_1,  jit_ldm_2,  jit_ldm_3,  jit_ldm_4,
    jit_ldm_5,  jit_ldm_6,  jit_ldm_7,  jit_ldm_8,
    jit_ldm_9,  jit_ldm_10, jit_ldm_11, jit_ldm_12,
    jit_ldm_13, jit_ldm_14, jit_ldm_15, jit_ldm_16,
};



unsigned char *jit_emit_arm_block_data_transfer(jit_emitter_t *machine, uint32_t w20)
{
    uint32_t w11_sel = (w20 >> 29) & 7u;

    unsigned char *x22;

    if (w11_sel <= 6u) {

        x22 = machine->code;
        uint32_t w10 = (w20 >> 28) & 15u;

        int64_t x22i = (int64_t)(uintptr_t)x22;
        uint64_t sel = (x22i < 0) ? (uint64_t)(x22i + 3) : (uint64_t)x22i;

        uint32_t w9 = 0u - ((uint32_t)sel >> 2);
        w10 = (w10 & ~UINT32_C(0x00ffffe0)) | ((w9 & UINT32_C(0x7ffff)) << 5);
        w9 = w10 ^ UINT32_C(0x54000001);
        wr32(x22, w9);
        machine->code = (unsigned char *)(x22 + 4);
    } else {

        x22 = NULL;
    }

    uint32_t idx_low = w20 & 0xffu;
    uint32_t idx_height = (w20 >> 8) & 0xffu;
    uint32_t w27_rn = (w20 >> 16) & 0xfu;
    uint8_t table_low = arm_popcount_table[idx_low];
    uint8_t table_height = arm_popcount_table[idx_height];
    uint32_t w23 = (uint32_t)table_height + (uint32_t)table_low;

    uint32_t w10_base;
    if (w27_rn == 0xfu) {

        const jit_instruction_record_t *ctx1112 = machine->record;
        jit_emit_mov_imm32(machine, 0u,
                           ctx1112->pc_value);
        w10_base = 0u;
    } else {
        w10_base = w27_rn + 0xdu;
    }

    if (w23 == 0u) return x22;

    uint32_t w26_reglist = w20 & 0xffffu;
    int single_reg_without_s = (w23 == 1u) && ((w20 & (1u << 22)) == 0u);

    if (!single_reg_without_s) {

        uint32_t w0idx = w23 - 1u;

        uint32_t w11h = w10_base;
        uint32_t w12rn = UINT32_C(1) << (w27_rn & 31u);
        uint32_t w4 = (w20 >> 21) & 1u;

        uint32_t w11_field = w11h;
        uint32_t w15_flag  = 0u;

        if ((UINT32_C(0x108000) & ~w20) != 0u &&
            (w20 & UINT32_C(0x400000)) != 0u) {
            unsigned char *x13o = machine->code;
            uint64_t tab3400 = (uint64_t)(uintptr_t)jit_bank_select_hook;
            uint32_t w9o = (w11h | UINT32_C(0xb8160120))
                         | UINT32_C(0x4000);
            int64_t  d9  = (int64_t)tab3400
                         - (int64_t)(uintptr_t)(x13o + 0x10);
            int64_t  d9r = (d9 < 0) ? d9 + 3 : d9;
            uint32_t w9b = ((uint32_t)(d9r >> 2) & UINT32_C(0x03ffffff))
                         | UINT32_C(0x94000000);

            wr32(x13o,        w9o);
            wr32(x13o + 20,   UINT32_C(0xb8564120));
            wr32(x13o + 12,   UINT32_C(0x52a00001));
            wr32(x13o + 16,   w9b);

            wr32(x13o + 4,    UINT32_C(0xb9610780));
            wr32(x13o + 8,    UINT32_C(0xb8160120));
            machine->code = (unsigned char *)(x13o + 0x18);

            w11_field = 0u;
            w15_flag  = 1u;
        }

        {
            uint32_t w13 = w12rn | UINT32_C(0x100000);
            int ne = ((w13 & w20) != w13);
            if (machine->is_arm9 == 0u) {
                if (!ne) w4 = 0u;
            } else if (!ne && (w26_reglist & ~w12rn) != 0u) {
                if ((w26_reglist & (0u - w12rn)) == w12rn)
                    w4 = 0u;
            }
        }

        uint32_t w14b = w23 << 2;
        uint32_t w12m = (w20 >> 23) & 3u;
        uint32_t w28 = 4u;
        unsigned char *cur;
        int emit_first = 1;
        uint32_t first_row = 0u;

        if (w12m == 3u) {
            first_row = (UINT32_C(0x1000) | (w11_field << 5)) | UINT32_C(0x11000000);
        } else {
            if (w12m == 2u) w28 = 0u;
            if (w12m == 1u) {
                w28 = 0u;
                emit_first = 0;
            } else {
                uint32_t d = w28 - w14b;
                if (d == 0u) {
                    w28 = 0u;
                    emit_first = 0;
                } else {
                    w28 = d;
                    if ((int32_t)w28 < 0) {
                        first_row = ((0u - (w28 << 10)) | (w11_field << 5))
                                | UINT32_C(0x51000000);
                    } else {
                        first_row = ((w28 << 10) | (w11_field << 5))
                                | UINT32_C(0x11000000);
                    }
                }
            }
            if (!emit_first && w11_field != 0u) {

                first_row = (UINT32_C(0x2a0003e0) & ~(UINT32_C(0x1f) << 16))
                        | ((w11_field & UINT32_C(0x1f)) << 16);
                emit_first = 1;
            }
        }

        if (emit_first) {
            cur = machine->code;
            wr32(cur, first_row);
            machine->code = (unsigned char *)(cur + 4);
        }

        if ((w20 & (UINT32_C(1) << 20)) != 0u) {
            uint32_t w10r = w11h;

            uint32_t w3f = w15_flag;

            if (w4 != 0u) {

                int32_t w11v = ((w20 & UINT32_C(0x800000)) == 0u)
                             ? -(int32_t)w14b : (int32_t)w14b;
                uint32_t w12v;
                w11v -= (int32_t)w28;
                w12v = (uint32_t)w11v << 10;
                if (w11v < 0) {
                    w10r |= (0u - w12v);
                    w10r |= UINT32_C(0x51000000);
                } else {
                    w10r |= w12v;
                    w10r |= UINT32_C(0x11000000);
                }
                cur = machine->code;
                wr32(cur, w10r);
                cur += 4;
                machine->code = (unsigned char *)cur;
            } else {
                cur = machine->code;
            }

            if (machine->is_arm9 == 1u) {

            {
                unsigned char *x13 = machine->code;
                const uint32_t w14t = UINT32_C(0x8b204000) | UINT32_C(0x140);
                unsigned char *x12 = x13 + 8;
                unsigned char *x10;
                unsigned char *x11;
                unsigned char *x13b;
                uint32_t w15, w17, w10t;
                int64_t x14, x15, x16;

                {
                    int64_t x12s = (int64_t)(uintptr_t)x12;
                    if (x12s < 0) x12s += 3;
                    w15 = 0u - (((uint32_t)x12s) >> 2);
                }
                w10t = (UINT32_C(0x14000000) & ~UINT32_C(0x03ffffff))
                     | (w15 & UINT32_C(0x03ffffff));
                wr32(x13 + 8,  w10t);
                wr32(x13 + 12, w14t);
                x10 = x13 + 0x10;

                wr32(x13,     UINT32_C(0x12261c01));
                wr32(x13 + 4, UINT32_C(0x34000041));
                machine->block_transfer_continue = x10;
                machine->code = (unsigned char *)machine;

                {
                    uint64_t help = (uint64_t)(uintptr_t)jit_ldm_table[w0idx];
                    x13b = machine->table;
                    x15 = (int64_t)help - (int64_t)(uintptr_t)machine;
                    x14 = (int64_t)(uintptr_t)x10
                        - (int64_t)(uintptr_t)(machine->scratch + 1);
                    machine->code = (unsigned char *)(machine->scratch + 2);
                }

                if (x15 < 0) x15 += 3;
                x11 = x13b - 8;
                x16 = x14;
                if (x16 < 0) x16 += 3;
                w17 = (UINT32_C(0x14000000) & ~UINT32_C(0x03ffffff))
                    | ((uint32_t)(x16 >> 2) & UINT32_C(0x03ffffff));
                x14 = (int64_t)(uintptr_t)machine - (int64_t)(uintptr_t)x11;
                x14 >>= 2;
                w15 = ((uint32_t)(x15 >> 2) & UINT32_C(0x03ffffff))
                    | UINT32_C(0x94000000);
                {
                    uint32_t w16 = (uint32_t)x14 + (uint32_t)(x15 >> 2);
                    wr32((unsigned char *)machine->scratch,     w15);
                    wr32((unsigned char *)(machine->scratch + 1), w17);
                    w15 = (w16 & UINT32_C(0x3ffffff)) | UINT32_C(0x94000000);
                }
                machine->table = x11;
                wr32(x13b - 8, w15);

                {
                    uint32_t v = rd32((const unsigned char *)(machine->scratch + 1));
                    if ((v & UINT32_C(0x7c000000)) == UINT32_C(0x14000000)) {
                        uint32_t s = v + (uint32_t)x14;
                        v = (v & ~UINT32_C(0x03ffffff))
                          | (s & UINT32_C(0x03ffffff));
                    }
                    wr32(x13b - 4, v);
                }

                {
                    uint32_t v = rd32(x12);
                    uint32_t diff = (uint32_t)(uintptr_t)x11
                                 - (uint32_t)(uintptr_t)x12;
                    v = (v & ~UINT32_C(0x03ffffff))
                      | ((diff >> 2) & UINT32_C(0x03ffffff));
                    wr32(x12, v);
                }
                machine->code = (unsigned char *)x10;
                cur = x10;
            }
            } else {

                uint64_t dest = (uint64_t)(uintptr_t)jit_ldm_table[w0idx];
                int64_t d = (int64_t)dest - (int64_t)(uintptr_t)cur;
                int64_t dr = (d < 0) ? d + 3 : d;
                wr32(cur, ((uint32_t)(dr >> 2) & UINT32_C(0x03ffffff))
                               | UINT32_C(0x94000000));
                cur += 4;
                machine->code = (unsigned char *)cur;
            }

            if (w26_reglist != 0u) {
                uint32_t list = w26_reglist;
                uint32_t w11c = 0u, w12c = 0u, w13c;
                uint8_t pair[2];
                pair[0] = 0u; pair[1] = 0u;
                w13c = (w4 != 0u) ? 0u : 0xdu;

                while (list != 0u) {
                    if ((list & 1u) != 0u) {
                        uint32_t w16;
                        if (w4 != 0u) {
                            w16 = (w13c == 0xfu) ? 0u : (w13c + 0xdu);
                            if (w27_rn == w13c) w16 = 1u;
                        } else {
                            w16 = (w13c == 0x1cu) ? 0u : w13c;
                        }
                        pair[w12c] = (uint8_t)w16;
                        ++w12c;
                        if (w12c == 2u) {
                            uint32_t w8 = ((w11c << 13) & UINT32_C(0xffff9fff))
                                        | ((uint32_t)pair[1] << 10)
                                        | (uint32_t)pair[0]
                                        | UINT32_C(0x29400000);
                            wr32(cur, w8);
                            cur += 4;
                            machine->code = (unsigned char *)cur;
                            w11c += 8u;
                            w12c = 0u;
                        }
                    }
                    list >>= 1;
                    ++w13c;
                }

                if (w12c != 0u) {

                    uint32_t w8 = (uint32_t)pair[0];
                    uint32_t w11d = w11c >> 2;
                    w8 = (w8 & ~(UINT32_C(0xfff) << 10))
                       | ((w11d & UINT32_C(0xfff)) << 10);
                    w8 |= UINT32_C(0xb9400000);
                    wr32(cur, w8);
                    cur += 4;
                    machine->code = (unsigned char *)cur;
                }
            }

            if (w3f != 0u) {
                uint64_t tab3400 = (uint64_t)(uintptr_t)jit_bank_select_hook;
                int64_t  d1 = (int64_t)tab3400
                            - (int64_t)(uintptr_t)(cur + 4);
                int64_t  d1r = (d1 < 0) ? d1 + 3 : d1;
                wr32(cur, UINT32_C(0xb8560121));
                wr32(cur + 4,
                          ((uint32_t)(d1r >> 2) & UINT32_C(0x03ffffff))
                          | UINT32_C(0x94000000));
                cur += 8;
                machine->code = (unsigned char *)cur;
            }

            if ((w20 & (UINT32_C(1) << 15)) != 0u) {
                uint64_t dest;
                cur = machine->code;
                if ((w20 & UINT32_C(0x400000)) != 0u) {
                    dest = (uint64_t)(uintptr_t)jit_irq_enter;
                } else {

                    if (machine->is_arm9 != 1u) {
                        uint8_t f = machine->thumb;
                        uint32_t extra = 0u;
                        if (f == 1u)      extra = UINT32_C(0x32000000);
                        else if (f == 0u) extra = UINT32_C(0x123e7400);
                        if (extra != 0u) {
                            wr32(cur, extra);
                            cur += 4;
                            machine->code = (unsigned char *)cur;
                        }
                    }
                    dest = (uint64_t)(uintptr_t)jit_dispatch_block_cache;
                }
                {
                    int64_t d = (int64_t)dest - (int64_t)(uintptr_t)cur;
                    int64_t dr = (d < 0) ? d + 3 : d;
                    wr32(cur, (UINT32_C(0x14000000) & ~UINT32_C(0x03ffffff))
                                   | ((uint32_t)(dr >> 2) & UINT32_C(0x03ffffff)));
                    cur += 4;
                    machine->code = (unsigned char *)cur;
                }
            }
            return x22;
        }

        cur = machine->code;
        wr32(cur, UINT32_C(0xd1048121));
        machine->code = (unsigned char *)(cur + 4);

        {
            uint32_t list = w26_reglist;
            uint32_t w24 = 0u, w21 = 0u, w22r = 0xdu;
            uint8_t pair[2];
            pair[0] = 0u; pair[1] = 0u;

            while (list != 0u) {
                if ((list & 1u) != 0u) {
                    uint32_t w25;
                    if (w22r == 0x1cu) {
                        const jit_instruction_record_t *p1112 = machine->record;
                        jit_emit_mov_imm32(machine, 2u,
                                           p1112->pc_value);
                        w25 = 2u;
                    } else {
                        w25 = w22r;
                    }
                    pair[w21] = (uint8_t)w25;
                    ++w21;
                    if (w21 == 2u) {

                        uint32_t w8 = ((w24 << 13) & UINT32_C(0xffff9fff))
                                    | ((uint32_t)pair[1] << 10)
                                    | (uint32_t)pair[0]
                                    | UINT32_C(0x29000020);
                        cur = machine->code;
                        wr32(cur, w8);
                        machine->code = (unsigned char *)(cur + 4);
                        w24 += 8u;
                        w21 = 0u;
                    }
                }
                list >>= 1;
                ++w22r;
            }

            if (w21 != 0u) {

                uint32_t w8 = (uint32_t)pair[0];
                uint32_t w9 = w24 >> 2;
                w8 = (w8 & ~(UINT32_C(0xfff) << 10))
                   | ((w9 & UINT32_C(0xfff)) << 10);
                w8 |= UINT32_C(0xb9000020);
                cur = machine->code;
                wr32(cur, w8);
                machine->code = (unsigned char *)(cur + 4);
            }
        }

        if (w4 != 0u) {
            uint32_t w9 = (w27_rn == 0xfu) ? 0u : (w27_rn + 0xdu);

            int32_t w8 = ((w20 & UINT32_C(0x800000)) == 0u)
                       ? -(int32_t)w14b : (int32_t)w14b;
            uint32_t w10;
            w8 -= (int32_t)w28;
            w10 = (uint32_t)w8 << 10;
            if (w8 < 0) {
                w9 |= (0u - w10);
                w9 |= UINT32_C(0x51000000);
            } else {
                w9 |= w10;
                w9 |= UINT32_C(0x11000000);
            }
            cur = machine->code;
            wr32(cur, w9);
            machine->code = (unsigned char *)(cur + 4);
        }

        cur = machine->code;
        unsigned char *x8_bl = cur;
        {
            uint64_t dest = (uint64_t)(uintptr_t)jit_stm_table[w0idx];
            int64_t d = (int64_t)dest - (int64_t)(uintptr_t)cur;
            int64_t dr = (d < 0) ? d + 3 : d;
            wr32(cur, ((uint32_t)(dr >> 2) & UINT32_C(0x03ffffff))
                           | UINT32_C(0x94000000));
            cur += 4;
        }

        {
            uint32_t w9 = machine->pc - machine->block_pc;
            uint32_t w10 = (uint32_t)(uintptr_t)cur - (uint32_t)(uintptr_t)machine->block_code;
            uint32_t *x11 = machine->resume_table;
            machine->code = (unsigned char *)cur;
            w10 = (w10 << 14) & UINT32_C(0xffff0000);
            wr32(x11, w10 | w9);
            machine->resume_table = x11 + 1;
        }

        if (w15_flag != 0u) {
            uint64_t tab3400 = (uint64_t)(uintptr_t)jit_bank_select_hook;
            int64_t  d10 = (int64_t)tab3400
                         - (int64_t)(uintptr_t)(x8_bl + 8);
            int64_t  d10r = (d10 < 0) ? d10 + 3 : d10;
            wr32(x8_bl + 4, UINT32_C(0xb8560121));
            wr32(x8_bl + 8,
                      ((uint32_t)(d10r >> 2) & UINT32_C(0x03ffffff))
                      | UINT32_C(0x94000000));
            machine->code = (unsigned char *)(x8_bl + 0xc);
        }
        return x22;
    }

    uint32_t w9_clz = (uint32_t)__builtin_clz(w26_reglist);
    uint32_t w8_wraw = w20 & UINT32_C(0x200000);
    uint32_t P_bit = (w20 >> 24) & 1u;
    uint32_t U_bit = (w20 >> 23) & 1u;
    uint32_t W_bit = (w20 >> 21) & 1u;

    uint32_t w10_notP;
    int32_t  w11_ofs;

    if (W_bit != 0u) {

        w10_notP = (P_bit == 0u) ? 1u : 0u;
        w11_ofs  = (U_bit != 0u) ? -4 : 4;
    } else {

        w10_notP = 0u;
        w11_ofs  = 0;
    }

    uint32_t w9_dir = UINT32_C(0x1f000) - (w9_clz << 12);
    uint32_t w12_uflag = (w11_ofs < 0) ? UINT32_C(0x4800000)
                                       : UINT32_C(0x4000000);
    uint32_t w13_condrnl = w20 & UINT32_C(0xf01f0000);
    w9_dir |= w13_condrnl;
    w9_dir |= w12_uflag;
    uint32_t w8_withW = w8_wraw | w9_dir;
    int32_t  w11_abs = (w11_ofs < 0) ? -w11_ofs : w11_ofs;
    uint32_t w8_withP = w8_withW | UINT32_C(0x1000000);
    uint32_t w8_final = (w10_notP != 0u) ? w9_dir : w8_withP;
    uint32_t w1 = w8_final | (uint32_t)w11_abs;
    uint32_t w2 = (w20 >> 20) & 1u;
    uint32_t param9 = (uint32_t)w11_abs;

    jit_emit_template_call(machine, w1, w2, 0u, 0u, 0u, 0u, 0u, param9);

    return x22;
}

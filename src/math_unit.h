#ifndef SEEDLESSDS_MATH_UNIT_H
#define SEEDLESSDS_MATH_UNIT_H

#include <stdint.h>

typedef struct math_regs {
    uint16_t divcnt;
    uint8_t reserved_0[14];
    int64_t div_numer;
    int64_t div_denom;
    int64_t div_result;
    int64_t divrem_result;
    uint16_t sqrtcnt;
    uint8_t reserved_1[2];
    uint32_t sqrt_result;
    uint64_t sqrt_param;
} math_regs_t;

typedef struct math {
    uint8_t div_result_valid;
    uint8_t sqrt_result_valid;
} math_t;

struct bus;
void math_div_unit_execute(struct bus *bus);
void math_sqrt_cache_recompute(struct bus *bus);
uint32_t math_sqrt_u32(uint32_t num);
uint64_t math_sqrt_u64(uint64_t n);
int64_t math_div_round_away_from_zero(int64_t a, int64_t b);
#endif

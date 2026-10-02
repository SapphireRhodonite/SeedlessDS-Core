#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "math_unit.h"
#include "core/nds_state.h"
#include "core_internals.h"

uint32_t math_sqrt_u32(uint32_t num)
{

    uint32_t rest;
    uint32_t res;
    uint32_t bit;

    if (num == 0) {
        return 0;
    }

    rest = num;
    res = 0;
    bit = 0x40000000u;

    while (bit > rest) {
        bit >>= 2;
    }

    while (bit != 0) {
        uint32_t candidate = res | bit;
        if (rest >= candidate) {
            rest -= candidate;
            res = (res >> 1) | bit;
        } else {
            res = res >> 1;
        }
        bit >>= 2;
    }

    return res;
}

uint64_t math_sqrt_u64(uint64_t n) {
    if (n == 0) return 0;
    uint64_t rest = n, res = 0, bit = 0x1000000000000000ull, t;
    t = rest - (res | bit);
    if (rest >= (res | bit)) goto C;
B:
    res >>= 1;
    bit >>= 2;
    if (bit == 0) return res;
D:
    t = rest - (res | bit);
    if (rest < (res | bit)) goto B;
C:
    res = bit | (res >> 1);
    rest = t;
    bit >>= 2;
    if (bit != 0) goto D;
    return res;
}

void math_div_unit_execute(bus_t *bus) {
    math_regs_t *d = &bus->io_mirror[0].math;

    uint32_t ctrl = d->divcnt;
    bus->math.div_result_valid = 1;

    uint32_t mode = ctrl & 3;
    uint32_t out  = ctrl & 0xffffbfffu;

    int64_t num, den;

    if (mode == 0) {
        uint32_t den32 = (uint32_t)d->div_denom;
        num = (int32_t)d->div_numer;

        if (den32 == 0) {
            if (d->div_denom == 0) out = ctrl | 0x4000u;
            d->divrem_result = num;

            d->div_result = ((int32_t)num < 0)
                          ? (int64_t)0xffffffff00000001ll
                          : (int64_t)0x00000000ffffffffll;
            d->divcnt = (uint16_t)out;
            return;
        }
        if ((int32_t)num == (int32_t)0x80000000 && (int32_t)den32 == -1) {
            d->div_result = 0x80000000ll;
            d->divcnt = (uint16_t)out;
            return;
        }
        int32_t c = (int32_t)num / (int32_t)den32;
        int32_t r = (int32_t)num - c * (int32_t)den32;
        d->div_result = c;
        d->divrem_result = r;
        d->divcnt = (uint16_t)out;
        return;
    }

    if (mode == 2) {
        num = d->div_numer;
        den = d->div_denom;
        if (den == 0) {
            out = ctrl | 0x4000u;
            d->divrem_result = num;
            d->div_result = (num < 0) ? 1 : -1;
            d->divcnt = (uint16_t)out;
            return;
        }
    } else {
        den = (int32_t)d->div_denom;
        num = d->div_numer;
        if ((int32_t)den == 0) {
            if (d->div_denom == 0) out = ctrl | 0x4000u;
            d->divrem_result = num;
            d->div_result = (num < 0) ? 1 : -1;
            d->divcnt = (uint16_t)out;
            return;
        }
    }

    if (num == (int64_t)0x8000000000000000ll && den == -1) {
        d->div_result = (int64_t)0x8000000000000000ll;
        d->divcnt = (uint16_t)out;
        return;
    }

    int64_t c = num / den;
    d->div_result = c;
    d->divrem_result = num - c * den;
    d->divcnt = (uint16_t)out;
}

extern uint32_t math_sqrt_u32_3(uint32_t num) __asm__("math_sqrt_u32");

void math_sqrt_cache_recompute(bus_t *bus)
{
    math_regs_t *d = &bus->io_mirror[0].math;

    bus->math.sqrt_result_valid = 1;

    if ((d->sqrtcnt & 1) == 0) {
        uint32_t num  = (uint32_t)d->sqrt_param;

        uint32_t root = math_sqrt_u32(num);
        d->sqrt_result = root;
        return;
    }

    uint64_t rest = d->sqrt_param;
    if (rest == 0) {
        d->sqrt_result = 0;
        return;
    }

    uint64_t res = 0, bit = 0x1000000000000000ull, t;
    t = rest - (bit | res);
    if (rest >= (bit | res)) goto accepts;
low:
    res >>= 1;
    bit >>= 2;
    if (bit == 0) goto end;
test:
    t = rest - (bit | res);
    if (rest < (bit | res)) goto low;
accepts:
    res = bit | (res >> 1);
    rest = t;
    bit >>= 2;
    if (bit != 0) goto test;
end:
    d->sqrt_result = (uint32_t)res;
}

static int64_t div_as_sdiv(int64_t a, int64_t b) {
    if (b == -1) return (int64_t)(0u - (uint64_t)a);
    return a / b;
}

int64_t math_div_round_away_from_zero(int64_t a, int64_t b) {
    if (a < 0) {
        if (b < 0) return div_as_sdiv((int64_t)((uint64_t)b + (uint64_t)a + 1u), b);
        return div_as_sdiv(a, b);
    }
    if (b < 0) return div_as_sdiv(a, b);
    return div_as_sdiv((int64_t)((uint64_t)b + (uint64_t)a - 1u), b);
}

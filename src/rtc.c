#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <time.h>
#include "rtc.h"
#include "core/nds_state.h"
#include "seedlessds/platform.h"
#include "mem_access.h"



long rtc_time_now(rtc_t *rtc) {

    unsigned char *mod = (unsigned char *)rtc->machine;

    uint32_t real;
    real = ((nds_t *)mod)->config.rtc_system_time;
    if (real != 0) {
        return (long)nds_platform_default()->time.now_seconds(nds_platform_default()->user);
    }

    uint64_t offset = (uint64_t)rtc->offset_seconds;
    uint64_t base    = rd64(mod);
    return (long)(offset + base / 60u);
}

void rtc_time_set(rtc_t *rtc, uint64_t v) {
    const unsigned char *p = (const unsigned char *)rtc->machine;
    uint64_t n;
    memcpy(&n, p, 8);
    unsigned __int128 prod = (unsigned __int128)n * 0x8888888888888889ull;
    uint64_t height = (uint64_t)(prod >> 64);
    uint64_t r = v - (height >> 5);
    rtc->offset_seconds = (int64_t)r;
}

typedef struct tm *(*fn_localtime)(const time_t *);
typedef time_t    (*fn_mktime)(struct tm *);


static uint32_t bcd(uint32_t v) { return (v % 10u) | ((v / 10u) << 4); }

static uint32_t debcd(uint32_t v) { return (v >> 4) * 10u + (v & 0xfu); }

static time_t now(rtc_t *r) {
    uint8_t *mod = (uint8_t *)r->machine;
    uint32_t real = ((nds_t *)mod)->config.rtc_system_time;
    if (real != 0) return (long)nds_platform_default()->time.now_seconds(nds_platform_default()->user);
    return (time_t)((uint64_t)r->offset_seconds + rd64(mod) / 60u);
}

static uint32_t time_read(rtc_t *r, uint32_t h, uint32_t *flag) {
    if (h < 12u) { *flag = 0; return h; }
    *flag = 0x40;
    return (r->status1 & 2) ? h : (h - 12u);
}

static uint32_t time_written(rtc_t *r) {
    uint32_t h = ((r->datetime[2] >> 4) & 3u) * 10u + (r->datetime[2] & 0xfu);
    if (r->datetime[2] & 0x40) {
        if (!(r->status1 & 2) && h < 13u) h += 12u;
    }
    return h;
}

static void store_offset(rtc_t *r, struct tm *t) {
    time_t m = ((fn_mktime)sym_libc_mktime)(t);
    uint8_t *mod = (uint8_t *)r->machine;
    r->offset_seconds = (int64_t)((uint64_t)m - rd64(mod) / 60u);
}

static void dump(rtc_t *r, int all) {
    time_t t = now(r);
    struct tm *g = ((fn_localtime)sym_libc_localtime)(&t);
    uint32_t band;
    uint32_t h = time_read(r, (uint32_t)g->tm_hour, &band);
    r->datetime[0] = (uint8_t)bcd((uint32_t)g->tm_sec);
    r->datetime[1] = (uint8_t)bcd((uint32_t)g->tm_min);
    r->datetime[2] = (uint8_t)(bcd(h) | band);
    if (!all) return;
    r->datetime[3] = (uint8_t)g->tm_wday;
    r->datetime[4] = (uint8_t)bcd((uint32_t)g->tm_mday);
    r->datetime[5] = (uint8_t)bcd((uint32_t)g->tm_mon + 1u);
    r->datetime[6] = (uint8_t)bcd((uint32_t)g->tm_year - 100u);
}

uint32_t rtc_register_write(rtc_t *r, uint32_t arg) {

    if (!(arg & 4)) {
        r->data_out = 0;
        r->state = 0;
        r->shift = 0;
        r->bit_count = 0;
        return arg & ~1u;
    }

    uint32_t clk = arg & 2u;
    if (clk == r->clock) goto end;

    {
        uint32_t st = r->state;
        if (clk == 0) {
            if (st == 1) {
                r->data_out = (uint8_t)(r->shift & 1u);
                r->shift = (uint8_t)(r->shift >> 1);
            }
            goto end;
        }
        if (st != 1) r->shift = (uint8_t)((r->shift >> 1) | ((arg << 7) & 0x80u));
        uint32_t n = (uint32_t)(uint8_t)(r->bit_count + 1u);
        r->bit_count = (uint8_t)n;
        if (n != 8) goto end;

        if (st == 2) {

            r->datetime[r->byte_count] = r->shift;
            if (r->byte_count != 0) { r->byte_count--; r->bit_count = 0; goto end; }
            uint32_t reg = r->command;
            if (reg > 6) { r->state = 0; r->byte_count--; r->bit_count = 0; goto end; }
            switch (reg) {
            case 0:
                r->status1 = r->datetime[0];
                break;
            case 4:
                r->status2 = r->datetime[0];
                break;
            case 2: {
                struct tm t;
                memset(&t, 0, sizeof t);
                t.tm_sec  = (int)debcd(r->datetime[0]);
                t.tm_min  = (int)debcd(r->datetime[1]);
                t.tm_wday = (int)r->datetime[3];
                t.tm_mday = (int)debcd(r->datetime[4]);
                t.tm_mon  = (int)debcd(r->datetime[5]) - 1;
                t.tm_year = (int)debcd(r->datetime[6]) + 100;
                t.tm_hour = (int)time_written(r);
                store_offset(r, &t);
                break;
            }
            case 6: {
                time_t base = now(r);
                struct tm t = *((fn_localtime)sym_libc_localtime)(&base);
                t.tm_sec  = (int)debcd(r->datetime[0]);
                t.tm_min  = (int)debcd(r->datetime[1]);
                t.tm_hour = (int)time_written(r);
                store_offset(r, &t);
                break;
            }
            default:
                break;
            }
            r->state = 0;
            r->byte_count--;
            r->bit_count = 0;
            goto end;
        }

        if (st == 1) {

            if (r->byte_count == 0) { r->state = 0; r->bit_count = 0; goto end; }
            r->byte_count--;
            r->shift = r->datetime[r->byte_count];
            r->bit_count = 0;
            goto end;
        }

        if (st != 0) { r->bit_count = 0; goto end; }

        {
            uint32_t b = r->shift;
            if ((b & 0xfu) != 6u) { r->bit_count = 0; goto end; }
            uint32_t reg = (b >> 4) & 7u;
            r->command = (uint8_t)reg;
            if (!(b & 0x80u)) { r->state = 2; goto common; }
            r->state = 1;
            if (reg == 7) goto common;
            switch (reg) {
            case 0: r->datetime[0] = r->status1;  break;
            case 4: r->datetime[0] = r->status2; break;
            case 2: dump(r, 1); break;
            case 6: dump(r, 0); break;
            default: break;
            }
        }
    }

common:
    {
        static const uint8_t count[8] = { 1, 3, 7, 1, 1, 3, 3, 1 };
        uint32_t reg = r->command;
        uint32_t k = (uint32_t)(uint8_t)(count[reg & 7] - 1u);
        r->byte_count = (uint8_t)k;
        if (reg == 1 && (r->status2 & 1)) { k = 0; r->byte_count = 0; }
        if (r->state == 1) r->shift = r->datetime[k];
        r->bit_count = 0;
    }

end:
    {
        uint32_t out = r->data_out;
        r->clock = (uint8_t)clk;
        return (arg & ~1u) | out;
    }
}

void *rtc_attach(void **dest, void *value) {
    *dest = value;
    return dest;
}

long rtc_reset(rtc_t *rtc, int use_given, long mark) {
    rtc->state = 0;
    rtc->status1 = 0x02;
    rtc->status2 = 0;
    rtc->data_out = 0;
    rtc->clock = 0x02;
    rtc->shift = 0;
    rtc->bit_count = 0;

    if (use_given) {
        rtc->offset_seconds = mark;
        return (long)(intptr_t)rtc;
    }
    long now = (long)nds_platform_default()->time.now_seconds(nds_platform_default()->user);
    rtc->offset_seconds = now;
    return now;
}

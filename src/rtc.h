#ifndef SEEDLESSDS_RTC_H
#define SEEDLESSDS_RTC_H

#include <stdint.h>

struct nds;

typedef struct rtc {
    struct nds *machine;
    int64_t offset_seconds;
    uint8_t datetime[8];
    uint8_t state;
    uint8_t command;
    uint8_t status1;
    uint8_t status2;
    uint8_t data_out;
    uint8_t clock;
    uint8_t shift;
    uint8_t bit_count;
    uint8_t byte_count;
    uint8_t reserved_0[7];
} rtc_t;

long rtc_time_now(rtc_t *rtc);
void rtc_time_set(rtc_t *rtc, uint64_t v);
uint32_t rtc_register_write(rtc_t *rtc, uint32_t arg);
void *rtc_attach(void **dest, void *value);
long rtc_reset(rtc_t *rtc, int use_given, long mark);
#endif

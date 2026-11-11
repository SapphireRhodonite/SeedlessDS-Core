#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/platform.h"

uint64_t time_monotonic_microseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000u + (uint64_t)now.tv_nsec / 1000u;
}

void time_now_microseconds(uint64_t *dest) {

    const nds_platform_t *p = nds_platform_default();
    *dest = p->time.now_us(p->user);
}

int time_now_milliseconds(int64_t *dest) {

    const nds_platform_t *p = nds_platform_default();
    *dest = (int64_t)(p->time.now_us(p->user) / 1000ULL);
    return 0;
}

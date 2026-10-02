#include <stdint.h>
#include <stddef.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/platform.h"

void time_now_microseconds(uint64_t *dest) {

    const nds_platform_t *p = nds_platform_default();
    *dest = p->time.now_us(p->user);
}

int time_now_milliseconds(int64_t *dest) {

    const nds_platform_t *p = nds_platform_default();
    *dest = (int64_t)(p->time.now_us(p->user) / 1000ULL);
    return 0;
}

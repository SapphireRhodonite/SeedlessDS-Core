#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "spi.h"

void spi_tsc_set_position(spi_tsc_t *tsc, uint32_t a, uint32_t b) {
    tsc->channels[5] = (uint16_t)(a << 4);
    tsc->channels[1] = (uint16_t)(b << 4);
}

void spi_tsc_reset(spi_tsc_t *tsc)
{
    memset(tsc->channels, 0, sizeof tsc->channels);
    tsc->command = 0;
    tsc->state = 0;
}

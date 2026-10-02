#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "spi.h"

uint32_t spi_powerman_transfer_byte(spi_powerman_t *pm, uint32_t value) {
    uint32_t first = pm->active;
    if (first == 0) {
        uint32_t ret = 0;
        pm->index = (unsigned char)value;
        pm->active = 1;
        return ret;
    }
    uint32_t t = pm->index;
    uint64_t len = (uint64_t)(t & 0x7fu);
    unsigned char *q = pm->regs + len;
    uint32_t ret = *q;
    if ((t & 0x80u) != 0 || len == 1) {
        pm->active = 0;
        return ret;
    }
    *q = (unsigned char)value;
    pm->active = 0;
    return ret;
}

void spi_powerman_deselect(spi_powerman_t *pm)
{
    pm->active = 0;
}

void spi_powerman_reset(spi_powerman_t *pm) {
    pm->regs[0] = 0x7f;
    pm->regs[1] = 0;
    pm->regs[2] = 0;
    pm->regs[3] = 0;
    pm->regs[4] = 0x0f;
    pm->active = 0;
    pm->index = 0;
}

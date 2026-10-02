#ifndef SEEDLESSDS_MEMORY_IO9_H
#define SEEDLESSDS_MEMORY_IO9_H

#include <stdint.h>

uint32_t io9_read8(unsigned char *obj, uint32_t dir);
uint32_t io9_read16(unsigned char *ctx, uint32_t dir);
uint32_t io9_read32(uint8_t *ctx, uint32_t dir);
void io9_write8(void *ctx, uint32_t reg, uint32_t val);
void io9_write16(void *ctx, uint32_t reg, uint32_t val);
void io9_write32(void *param_1, uint32_t param_2, uint64_t param_3);
uint64_t io9_write32_noop_tail(uint64_t x0);

#endif

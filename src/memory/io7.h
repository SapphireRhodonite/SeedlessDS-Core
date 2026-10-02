#ifndef SEEDLESSDS_MEMORY_IO7_H
#define SEEDLESSDS_MEMORY_IO7_H

#include <stdint.h>

struct bus;

uint32_t io7_read8(unsigned char *obj, uint32_t dir);
uint32_t io7_read16(unsigned char *ctx, uint32_t dir);
uint32_t io7_read32(uint8_t *ctx, uint32_t dir);
void io7_write8(struct bus *param_1, uint32_t param_2, uint32_t param_3);
void io7_write16(void *ctx, uint32_t reg, uint32_t val);
void io7_write32(uint8_t *ctx, uint32_t dir, uint32_t val);
uint64_t io7_write8_noop_tail(uint64_t x0);

#endif

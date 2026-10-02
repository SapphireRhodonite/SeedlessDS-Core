#ifndef SEEDLESSDS_MEMORY_IO_COMMON_H
#define SEEDLESSDS_MEMORY_IO_COMMON_H

#include <stdint.h>

uint64_t io_slot2_backup_read8(const unsigned char *ctx, uint32_t dir);
int io_slot2_backup_read16_zero(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
int io_slot2_backup_read32_zero(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
void io_slot2_backup_write8(uint8_t *ctx, uint32_t dir, uint32_t value);
uint64_t io_slot2_backup_write16_ignore(uint64_t x0);
uint64_t io_slot2_backup_write32_ignore(uint64_t x0);

int32_t io_slot2_gpio_read8_idle(uint64_t param_1, uint32_t param_2);
int io_slot2_gpio_read16_idle(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
int io_slot2_gpio_read32_idle(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
void io_slot2_gpio_write8(uint8_t *ctx, uint64_t param_2, uint32_t param_3);
void io_slot2_gpio_write16(uint8_t *ctx, uint64_t param_2, uint32_t param_3);
void io_slot2_gpio_write32(uint8_t *ctx, uint64_t param_2, uint32_t param_3);

int32_t io_slot2_serial_read8_idle(uint64_t param_1, uint32_t param_2);
uint32_t io_slot2_serial_read16_idle(void);
unsigned int io_slot2_serial_read32_idle(void);
uint64_t io_slot2_serial_read8_pop(const unsigned char *ctx);
uint32_t io_slot2_serial_read16_pop(uint8_t *ctx);
uint32_t io_slot2_serial_read32_pop(uint8_t *ctx);
uint32_t io_slot2_serial_read8_field(uint8_t *ctx, uint32_t param_2);

uint32_t io_slot2_open_bus_read8(void);
void io_reg_cache_write8(unsigned char *state, uint32_t dir, uint32_t value);
void io_reg_cache_write16(uint8_t *state, unsigned dir, unsigned value);
void io_reg_cache_write32(uint8_t *state, unsigned dir, unsigned value);
int io_reg_read_returns_zero(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
unsigned int io_read_port16(const uint8_t *state, unsigned int dir);
int io_read32_unmapped_zero(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
void io_write_port16(unsigned char *state, uint32_t dir, uint32_t value);
int io_slot2_open_bus_read32(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
uint32_t io_slot2_serial_read32_pop_pair(uint8_t *ctx);
unsigned int io_slot2_open_bus_read16(void);

#endif

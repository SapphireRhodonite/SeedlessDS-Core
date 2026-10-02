#ifndef SEEDLESSDS_SPI_H
#define SEEDLESSDS_SPI_H

#include <stddef.h>
#include <stdint.h>

struct nds;

#define SPI_MEMORY_PATH_SIZE 1024u
#define SPI_MEMORY_DIRTY_WORDS 0x800u
#define SPI_MEMORY_SECTOR_SHIFT 14
#define SPI_MEMORY_SECTOR_BYTES (1u << SPI_MEMORY_SECTOR_SHIFT)
#define TSC_MIC_SAMPLE_BIAS 0x8000u
#define FIRMWARE_IMAGE_SIZE 0x40000u

typedef struct spi_memory {
    uint32_t dirty_sectors[SPI_MEMORY_DIRTY_WORDS];
    char path[SPI_MEMORY_PATH_SIZE];
    uint32_t type;
    uint32_t address;
    uint32_t mask;
    uint32_t truncate_size;
    void *file;
    uint8_t *data;
    uint8_t jedec_id[4];
    uint32_t save_countdown;
    uint16_t command;
    uint8_t state;
    uint8_t status;
    uint8_t address_bytes;
    uint8_t byte_count;
    uint8_t write_whole_file;
    uint8_t footer_written;
    uint8_t has_footer;
    uint8_t reserved_0[7];
} spi_memory_t;

typedef struct spi_tsc {
    uint16_t channels[8];
    uint8_t command;
    uint8_t state;
} spi_tsc_t;

typedef struct spi_powerman {
    uint8_t active;
    uint8_t index;
    uint8_t regs[5];
} spi_powerman_t;

typedef struct spi {
    spi_memory_t firmware;
    spi_tsc_t tsc;
    spi_powerman_t powerman;
    uint8_t reserved_0[0x2458 - 0x2451];
    struct nds *machine;
    uint16_t control;
    uint8_t reserved_1[6];
} spi_t;

uint32_t spi_memory_transfer_byte(spi_memory_t *memory, uint32_t value);
void spi_memory_deselect(spi_memory_t *memory);
uint32_t spi_transfer_byte(spi_t *spi, uint32_t value);
void spi_control_write(spi_t *spi, uint32_t value);
void spi_reset(spi_t *spi);
void spi_powerman_reset(spi_powerman_t *powerman);
void spi_tsc_reset(spi_tsc_t *tsc);
void spi_tsc_set_position(spi_tsc_t *tsc, uint32_t x, uint32_t y);
#endif

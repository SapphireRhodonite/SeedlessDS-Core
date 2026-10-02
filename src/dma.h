#ifndef SEEDLESSDS_DMA_H
#define SEEDLESSDS_DMA_H

#include <stdint.h>

struct bus;
struct arm;
struct bus_region;

typedef struct dma_regs {
    uint32_t sad;
    uint32_t dad;
    uint32_t cnt;
} dma_regs_t;

typedef struct dma_channel {
    uint64_t deadline_cycles;
    struct arm *cpu;
    dma_regs_t *regs;
    uint32_t src;
    uint32_t dst;
    uint32_t cnt;
    uint8_t start_mode;
    uint8_t index;
    uint8_t started;
    uint8_t reserved_2;
} dma_channel_t;

typedef struct dma {
    struct bus *bus;
    struct bus_region *region;
    dma_channel_t channels[4];
} dma_t;

struct io_mirror;
struct arm;
void dma_channels_schedule_deadlines(dma_t *dma, struct bus *bus, struct bus_region *region, struct io_mirror *mirror, struct arm *cpu);
void *dma_channels_reset_fields(dma_t *dma);
void dma_channel_clear_fields(dma_channel_t *channel);
void dma_channels_read_state_block(dma_t *dma, void *stream, uint32_t stream_version);
#endif

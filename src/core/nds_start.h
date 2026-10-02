#ifndef SEEDLESSDS_CORE_NDS_START_H
#define SEEDLESSDS_CORE_NDS_START_H

#include <stdint.h>

typedef struct nds_start_params {
    const char *cache_dir;
    const char *save_dir;
    const char *rom_path;
    struct nds **machine_slot;
    uint32_t benchmark_frames;
    uint32_t benchmark_slot;
    uint32_t memory_offset;
    uint32_t unused_44;
    const uint8_t *config;
    uint8_t texture_format;
    uint8_t unused_57[7];
} nds_start_params_t;

int nds_start_game(nds_start_params_t *params);

#endif

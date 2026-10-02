#ifndef SEEDLESSDS_GPU_H
#define SEEDLESSDS_GPU_H

#include <stddef.h>
#include <stdint.h>
#include "../memory/vram.h"
#include "gpu2d/gpu2d.h"
#include "gpu3d/gpu3d.h"
#include "gpu3d/raster/raster.h"

#define GPU_SIZE 0x356cb0u
#define GPU_OUTPUT_OF(g) ((gpu_output_t *)((uint8_t *)(g) + GPU_SIZE + GPU3D_SIZE))
#define GPU3D_OF(g) ((gpu3d_t *)((uint8_t *)(g) + GPU_SIZE))

typedef struct gpu {
    vram_map_t vram;
    gpu2d_engine_t engine[GPU2D_ENGINES];
    uint8_t unmapped_0[8];
    uint8_t frame[2][GPU3D_FRAME_SIZE];
    gpu3d_band_bucket_t bucket[2][GPU3D_BANDS];
    uint8_t unmapped_1[0x20];
    uint8_t band_context[GPU3D_BAND_CONTEXTS][GPU3D_BAND_CONTEXT_SIZE];
    uint8_t band_rows[GPU3D_BAND_ROWS_SIZE];
    gpu3d_raster_t raster;
    gpu3d_texture_cache_t texture_cache;
} gpu_t;
#endif

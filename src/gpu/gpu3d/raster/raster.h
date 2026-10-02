#ifndef SEEDLESSDS_RASTER_H
#define SEEDLESSDS_RASTER_H

#include <stddef.h>
#include <stdint.h>

#define GPU3D_FRAME_SIZE 0xc0000u
#define GPU3D_BANDS 12
#define GPU3D_BAND_CONTEXTS 4
#define GPU3D_BAND_CONTEXT_SIZE 0x24100u
#define GPU3D_BAND_HEADER_OFFSET 0x24000u
#define GPU3D_BAND_PLANE_SIZE 0x10000u
#define GPU3D_BAND_ROW_BYTES 0x400u
#define GPU3D_BAND_ROWS_BACK 0x16000u
#define GPU3D_BAND_PLANE2_SIZE 0x4000u
#define GPU3D_BAND_ROWS_SIZE 0x21000u
#define GPU3D_TEXTURE_BUCKETS 4096
#define GPU3D_RASTER_SIZE 0x140u
#define GPU3D_TEXTURE_CACHE_SIZE 0x8030u
#define GPU_OUTPUT_SIZE 0x160u
#define RASTER_STEP4_FRACTION_MASK 0xfffu
#define RASTER_STEP8_NEGATIVE_BIAS 0x40000000
#define RASTER_BAND_COPY_BYTES 0x4000u
#define RASTER_TEXCOORD_BIAS 0x38000u
#define RASTER_RECIPROCAL_30 0x40000000u
#define RASTER_RECIPROCAL_31 0x80000000u
extern unsigned recon_block_header;
#define GPU3D_BAND_HEADER(ctx) ((gpu3d_band_header_t *)((uint8_t *)(ctx) + recon_block_header))
#define GPU3D_BAND(ctx) ((gpu3d_band_context_t *)(ctx))

struct gpu;
struct gpu3d;

typedef struct gpu3d_band_bucket {
    uint8_t list[0x1000];
    uint32_t count;
} gpu3d_band_bucket_t;

typedef struct gpu3d_band_header {
    struct gpu *gpu;
    struct gpu3d *gpu3d;
    uint32_t band_mark;
    uint32_t band_dirty;
    uint64_t thread;
    uint8_t mutex[40];
    uint8_t mutex_done[40];
    uint8_t cond[48];
    uint8_t cond_done[48];
    uint8_t start;
    uint8_t done;
    uint8_t index;
    uint8_t threads;
    uint8_t pass;
    uint8_t unmapped_0[0x100 - 0xd5];
} gpu3d_band_header_t;

typedef struct gpu3d_band_context {
    uint8_t plane0[GPU3D_BAND_PLANE_SIZE];
    uint8_t plane1[GPU3D_BAND_PLANE_SIZE];
    uint8_t plane2[GPU3D_BAND_PLANE2_SIZE];
    gpu3d_band_header_t header;
} gpu3d_band_context_t;

typedef struct gpu3d_raster {
    uint32_t disp3dcnt;
    uint32_t alpha_test_ref;
    uint32_t clear_color_rgb;
    uint32_t clear_depth_word;
    uint32_t frame_ready;
    uint32_t frame_pending;
    uint8_t *frame_front;
    uint8_t *frame_back;
    uint8_t *frame_last;
    uint8_t mutex_work[40];
    uint8_t mutex_done[40];
    uint8_t mutex_idle[40];
    uint8_t cond_work[48];
    uint8_t cond_done[48];
    uint8_t cond_idle[48];
    uint8_t frame_request;
    uint8_t frame_busy;
    uint8_t frame_idle;
    uint8_t frame_mode;
    uint8_t unmapped_0[4];
} gpu3d_raster_t;

typedef struct gpu3d_texture_entry {
    uint32_t key;
    uint32_t bank_mask_a;
    uint32_t bank_mask_b;
    uint32_t unmapped_0;
    uint8_t *data;
    uint8_t *palette;
    struct gpu3d_texture_entry *bucket_next;
    struct gpu3d_texture_entry *bucket_prev;
    struct gpu3d_texture_entry *next;
    struct gpu3d_texture_entry *prev;
    uint16_t width;
    uint16_t height;
    uint16_t bucket;
    uint16_t sub;
    uint16_t palette_count;
    uint8_t dirty;
    uint8_t format;
    uint8_t unmapped_2[4];
} gpu3d_texture_entry_t;

typedef struct gpu3d_texture_cache {
    struct gpu *gpu;
    gpu3d_texture_entry_t *bucket[GPU3D_TEXTURE_BUCKETS];
    gpu3d_texture_entry_t *head;
    uint32_t dirty_pending[2];
    uint32_t bank_bits_a;
    uint32_t bank_bits_a_previous;
    uint32_t bank_bits_b;
    uint32_t bank_bits_b_previous;
    uint32_t occupied;
    uint32_t count;
} gpu3d_texture_cache_t;

typedef struct gpu_capture {
    uint8_t *source;
    uint8_t *shadow;
    uint8_t *write_line;
    uint8_t *source_b_line;
    uint32_t read_offset;
    uint16_t width;
    uint8_t bank;
    uint8_t display_mode;
    uint8_t height;
    uint8_t source_a_mode;
    uint8_t source_b_mode;
    uint8_t blend;
    uint8_t eva;
    uint8_t evb;
    uint8_t active;
    uint8_t unmapped_1;
    uint32_t bank_bits;
    uint8_t unmapped_2[0x44 - 0x34];
    float hud_fps;
    float hud_percent;
    uint16_t lines_done;
    uint8_t unmapped_4[2];
} gpu_capture_t;

typedef struct gpu_output {
    uint8_t *capture_shadow[4];
    uint8_t bank_texture_bits[8];
    gpu_capture_t capture;
    uint64_t worker_thread;
    uint8_t worker_mutex[40];
    uint8_t worker_mutex_done[40];
    uint8_t worker_cond[48];
    uint8_t worker_cond_done[48];
    uint8_t worker_start;
    uint8_t worker_done;
    uint8_t unmapped_0[GPU_OUTPUT_SIZE - 0x132];
} gpu_output_t;
#define RASTER_RECIPROCAL_TABLE_WORDS 1024u
#define RASTER_RECIPROCAL_FILLED 512u
extern uint32_t raster_reciprocal_30[RASTER_RECIPROCAL_TABLE_WORDS];
extern uint32_t raster_reciprocal_31[RASTER_RECIPROCAL_TABLE_WORDS];
extern uint64_t raster_frame_thread[2];

#define RASTER_BUCKET_ROWS_1X 193u
#define RASTER_BUCKET_ROWS_2X 385u
#define RASTER_BUCKET_ROW_BYTES 4096u
extern unsigned char raster_bucket_list_1x[RASTER_BUCKET_ROWS_1X * RASTER_BUCKET_ROW_BYTES];
extern int16_t raster_bucket_count_1x[RASTER_BUCKET_ROWS_1X];
extern unsigned char raster_bucket_list_2x[RASTER_BUCKET_ROWS_2X * RASTER_BUCKET_ROW_BYTES];
extern int16_t raster_bucket_count_2x[RASTER_BUCKET_ROWS_2X];

#define RASTER_DOWNSAMPLE_LINE_BYTES 0x400u
extern unsigned char raster_downsample_line[RASTER_DOWNSAMPLE_LINE_BYTES];

#endif

#ifndef SEEDLESSDS_GPU2D_H
#define SEEDLESSDS_GPU2D_H

#include <stddef.h>
#include <stdint.h>

struct vram_map;

#define GPU2D_ENGINES 2u
#define GPU2D_BG_LAYERS 4u
#define GPU2D_RECIPROCAL_NUMERATOR 0x7fffffffu
#define GPU2D_ENGINE_SIZE 0x81420u
#define GPU2D_BG_SIZE 176u

struct gpu2d_bg;
typedef void (*gpu2d_bg_line_fn)(struct gpu2d_bg *bg, uint8_t *dst, uint8_t *mask, uint32_t line);

typedef struct gpu2d_bg {
    struct gpu2d_engine *engine;
    uint8_t *vram_window;
    uint8_t *palette;
    uint8_t *ext_palette;
    void *direct_ptr;
    void *direct_ptr_alt;
    gpu2d_bg_line_fn line_handler;
    uint32_t screen_base;
    uint32_t char_base;
    uint32_t affine_screen_offset;
    uint32_t screen_offset;
    uint32_t char_offset;
    uint32_t pa_reciprocal;
    uint32_t pc_reciprocal;
    uint32_t unmapped_1;
    int64_t span_x[3];
    int64_t span_y[3];
    int32_t ref_x;
    int32_t ref_y;
    int32_t current_x;
    int32_t current_y;
    uint16_t bgcnt;
    uint16_t hofs;
    uint16_t vofs;
    int16_t pa;
    int16_t pc;
    int16_t pb;
    int16_t pd;
    uint16_t mask_x;
    uint16_t mask_y;
    uint8_t width_shift;
    uint8_t tiles_per_row_minus_one;
    uint8_t size_code;
    uint8_t ext_palette_enabled;
    uint8_t affine_dirty;
    uint8_t unmapped_2;
} gpu2d_bg_t;

void gpu2d_bg_text_draw_tile_line(gpu2d_bg_t *ctx, uint8_t *dst, uint8_t *out, uint32_t shift);
void gpu2d_bg_affine_draw_line(gpu2d_bg_t *bg, uint8_t *p2, uint8_t *p3, uint32_t line);
void gpu2d_bg_affine_draw_line_dual_style(gpu2d_bg_t *bg, uint8_t *p2, uint8_t *p3, uint32_t line);

typedef struct gpu2d_sprite {
    int64_t span_x[3];
    int64_t span_y[3];
    const uint8_t *palette;
    const uint8_t *data;
    int16_t origin_x;
    int16_t origin_y;
    uint16_t row_stride;
    int16_t x;
    int16_t y;
    int16_t pa;
    int16_t pb;
    int16_t pc;
    int16_t pd;
    uint8_t kind;
    uint8_t attribute;
    uint8_t flip_v;
    uint8_t width;
    uint8_t unmapped_0[2];
} gpu2d_sprite_t;

typedef struct gpu2d_deferred_write {
    uint32_t address;
    uint32_t value;
    uint8_t line;
    uint8_t size;
    uint8_t unmapped_0[2];
} gpu2d_deferred_write_t;

#define GPU2D_SPRITES 128u
#define GPU2D_SPRITE_PRIORITIES 5u
#define GPU2D_LINES 192u
#define GPU2D_DEFERRED_WRITES 0x8000u

struct gpu;

typedef struct gpu2d_engine {
    struct gpu *gpu;
    uint8_t *vram_window;
    uint8_t *display_vram_bank;
    uint8_t *palette;
    uint8_t **bg_ext_palette;
    uint8_t *obj_ext_palette;
    uint8_t *oam;
    uint8_t *framebuffer;
    uint32_t framebuffer_line_size;
    uint32_t window_x_mask[2][8];
    uint8_t draw_order[8];
    uint8_t bg_order[4];
    uint32_t dispcnt;
    uint32_t screen_base;
    uint32_t char_base;
    union {
        struct {
            uint16_t winin;
            uint16_t winout;
        };
        uint32_t window_control;
    };
    uint16_t bldcnt;
    uint16_t bldy;
    uint16_t bldalpha;
    uint16_t master_bright;
    uint16_t mosaic;
    uint16_t win0h;
    uint16_t win1h;
    uint16_t win0v;
    uint16_t win1v;
    uint8_t bg_count;
    uint8_t draw_count;
    uint8_t window_flags;
    uint8_t window_dirty;
    uint8_t oam_dirty;
    uint8_t index;
    uint8_t no_framebuffer;
    uint8_t unmapped_2;
    uint8_t unmapped_3[192 - 186];
    gpu2d_bg_t bg[GPU2D_BG_LAYERS];
    gpu2d_sprite_t sprite[GPU2D_SPRITES];
    uint8_t sprite_list[GPU2D_SPRITE_PRIORITIES][GPU2D_LINES][128];
    uint8_t sprite_count[GPU2D_SPRITE_PRIORITIES][GPU2D_LINES];
    uint8_t line_mode[GPU2D_LINES];
    uint8_t *sprite_screen;
    uint8_t *sprite_screen_alt;
    uint8_t sprite_screen_priority;
    uint8_t unmapped_4[7];
    gpu2d_deferred_write_t deferred[GPU2D_DEFERRED_WRITES];
    uint32_t deferred_read;
    uint32_t deferred_count;
} gpu2d_engine_t;
#endif

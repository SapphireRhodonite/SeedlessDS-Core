#ifndef SEEDLESSDS_FRONTEND_FONT_H
#define SEEDLESSDS_FRONTEND_FONT_H

#include <stddef.h>
#include <stdint.h>

#define FONT_GLYPHS 256
#define FONT_IMAGE_ROWS 0x1000
#define FONT_BLEND_STEPS 128

typedef struct video_out_font_image {
    uint32_t width;
    uint32_t height;
    uint16_t rows[FONT_IMAGE_ROWS];
    uint32_t glyph_row[FONT_GLYPHS];
} video_out_font_image_t;

typedef struct video_out_font {
    uint32_t initialised;
    uint32_t unmapped_0;
    const video_out_font_image_t *image;
    uint8_t *glyph_mask[FONT_GLYPHS];
    uint32_t glyph_width[FONT_GLYPHS];
    uint32_t width;
    uint32_t height;
    uint8_t unmapped_1[8];
    uint32_t blend_table[FONT_BLEND_STEPS];
    uint32_t blend_key;
    uint32_t blend_color_a;
    uint32_t blend_color_b;
    uint8_t unmapped_2[4];
} video_out_font_t;

extern video_out_font_t video_out_font_state;

#define VIDEO_OUT_FONT (&video_out_font_state)
extern const video_out_font_image_t video_out_font_image_data;
#define VIDEO_OUT_FONT_IMAGE (&video_out_font_image_data)
#endif

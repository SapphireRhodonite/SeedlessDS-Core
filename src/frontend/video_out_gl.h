#ifndef SEEDLESSDS_FRONTEND_VIDEO_OUT_GL_H
#define SEEDLESSDS_FRONTEND_VIDEO_OUT_GL_H

#include <stddef.h>
#include <stdint.h>

#define VIDEO_OUT_GL_RENDERERS 2
#define VIDEO_OUT_GL_SCREENS 2
#define VIDEO_OUT_GL_PATH_BYTES 0x400
#define VIDEO_OUT_GL_NAME_BYTES 0x20
#define VIDEO_OUT_GL_PASS_SAMPLERS 8
#define VIDEO_OUT_GL_MAX_TEXTURES 16
#define VIDEO_OUT_GL_SCREEN_PAGE_BYTES 0xc0000u
#define VIDEO_OUT_GL_PAGE_PAIR_BYTES 0x180000u
#define VIDEO_OUT_GL_DEFAULT_SCREEN_HEIGHT 0xc0u
#define VIDEO_OUT_GL_MUTEX_WORDS 10
#define VIDEO_OUT_GL_COND_WORDS 12
#define VIDEO_OUT_GL_RECIPE_TAG_COUNT 7
#define GL_TEXTURE0 0x84c0u

enum {
    GL_RECIPE_TAG_OPTIONS = 0,
    GL_RECIPE_TAG_HEADER = 1,
    GL_RECIPE_TAG_VHEADER = 2,
    GL_RECIPE_TAG_FHEADER = 3,
    GL_RECIPE_TAG_INCLUDE = 4,
    GL_RECIPE_TAG_TEXTURE = 5,
    GL_RECIPE_TAG_PASS = 6
};

enum {
    GL_RECIPE_TEXTURE_UNSET = 0,
    GL_RECIPE_TEXTURE_FRAMEBUFFER = 1,
    GL_RECIPE_TEXTURE_NULL = 2,
    GL_RECIPE_TEXTURE_FILE = 3
};

typedef struct gl_program {
    uint32_t program;
    int32_t vertex_coordinate_attrib;
    int32_t texture_coordinate_attrib;
    int32_t texture_size_uniform;
    int32_t target_size_uniform;
    int32_t time_uniform;
} gl_program_t;

typedef struct gl_default_program {
    gl_program_t program;
    const void *vertex_coordinates;
    const void *texture_coordinates;
} gl_default_program_t;

typedef struct gl_pass_sampler {
    char name[VIDEO_OUT_GL_NAME_BYTES];
    uint32_t texture_unit;
    uint32_t texture_index;
} gl_pass_sampler_t;

typedef struct gl_pass {
    gl_program_t program;
    gl_pass_sampler_t sampler[VIDEO_OUT_GL_PASS_SAMPLERS];
    uint32_t framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t output_texture;
    uint32_t output_scale;
    uint32_t sampler_count;
    struct gl_pass *next;
} gl_pass_t;

typedef struct gl_recipe_texture {
    uint32_t id;
    uint32_t format;
    uint32_t internal_format;
    uint32_t type;
    uint32_t mag_filter;
    uint32_t min_filter;
    uint32_t framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t source;
} gl_recipe_texture_t;

typedef struct gl_text_block {
    char *text;
    uint64_t size;
} gl_text_block_t;

typedef struct gl_renderer {
    gl_pass_t *passes;
    gl_recipe_texture_t *textures;
    char base_dir[VIDEO_OUT_GL_PATH_BYTES];
    char name[VIDEO_OUT_GL_NAME_BYTES];
    uint64_t start_time_us;
    gl_text_block_t header;
    gl_text_block_t vertex_header;
    gl_text_block_t fragment_header;
    const void *vertex_coordinates;
    const void *texture_coordinates;
    uint8_t unmapped_0[8];
    uint32_t texture_count;
    int32_t viewport_x;
    int32_t viewport_y;
    int32_t viewport_width;
    int32_t viewport_height;
    uint32_t uses_time;
    uint32_t loaded;
    uint8_t unmapped_1[4];
} gl_renderer_t;

struct gl_renderer;

typedef struct gl_recipe_tag {
    const char *open;
    const char *close;
    int32_t (*read)(struct gl_renderer *renderer, void *stream, char *line,
                    const struct gl_recipe_tag *tag);
    int32_t id;
    uint8_t unmapped_0[4];
} gl_recipe_tag_t;

typedef struct video_out_gl {
    void *page[2];
    gl_renderer_t renderer[VIDEO_OUT_GL_RENDERERS];
    void *page_memory;
    uint32_t page_parity;
    int32_t bits_per_pixel;
    uint32_t gl_type;
    uint32_t gl_format;
    uint32_t screen_mode[VIDEO_OUT_GL_SCREENS];
    uint32_t screen_changed[VIDEO_OUT_GL_SCREENS];
    uint32_t screen_width[VIDEO_OUT_GL_SCREENS];
    uint32_t screen_height[VIDEO_OUT_GL_SCREENS];
    uint8_t upload_request;
    uint8_t unmapped_0[3];
    uint32_t mutex[VIDEO_OUT_GL_MUTEX_WORDS];
    uint32_t cond[VIDEO_OUT_GL_COND_WORDS];
    uint8_t unmapped_1[4];
} video_out_gl_t;

extern video_out_gl_t video_out_gl_state;

#define VIDEO_OUT_GL (&video_out_gl_state)
extern gl_default_program_t gl_default_program_state;

#define VIDEO_OUT_GL_DEFAULT_PROGRAM (&gl_default_program_state)
extern const gl_recipe_tag_t gl_recipe_tags[7];
#define VIDEO_OUT_GL_RECIPE_TAGS gl_recipe_tags
#endif

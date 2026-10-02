#ifndef SEEDLESSDS_GPU3D_H
#define SEEDLESSDS_GPU3D_H

#include <stddef.h>
#include <stdint.h>

#define GPU3D_SIZE 0x101b30u
#define GPU3D_BATCH_VERTICES 64
#define GPU3D_LIST_VERTICES 196
#define GPU3D_SCREEN_VERTICES 1568
#define GPU3D_PRIMITIVE_RUNS 193
#define GPU3D_TEXTURE_RUNS 198
#define GPU3D_MATRIX_STACK_DEPTH 32
#define GPU3D_BANK_VERTICES 6144
#define GPU3D_BANK_POLYGONS 2048
#define GPU3D_RGB5_MAX_PER_CHANNEL 0x001f1f1fu
#define GPU3D_BAND_MMAP_GUARD_BYTES 0x10000ul
#define GPU3D_STATE_PACKED_VERTEX_BYTES 32u
#define GPU3D_STATE_PACKED_POLYGON_BYTES 36u
#define GPU3D_STATE_VERTEX_BLOCK_BYTES 0x30200u
#define GPU3D_STATE_POLYGON_BLOCK_BYTES (GPU3D_BANK_POLYGONS * GPU3D_STATE_PACKED_POLYGON_BYTES)

typedef struct gpu3d_state_blocks {
    uint8_t unmapped_0[0x6c];
    uint8_t command_word[4];
    uint8_t polygons[2][GPU3D_STATE_POLYGON_BLOCK_BYTES];
    uint8_t pending_vertices[2][GPU3D_STATE_PACKED_VERTEX_BYTES];
    uint8_t unmapped_1[8];
    uint8_t vertices[2][GPU3D_STATE_VERTEX_BLOCK_BYTES];
    uint8_t header[0x200];
    uint8_t unmapped_2[0x18];
} gpu3d_state_blocks_t;

#define GPU3D_COMMAND_RING_SIZE 0x8000
#define GPU3D_PARAM_RING_WORDS 0x20000
#define GPU3D_LIGHTS 4

struct nds;
struct gpu;
struct gpu3d_texture_entry;

typedef struct gpu3d_vertex {
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t w;
} gpu3d_vertex_t;

typedef struct gpu3d_primitive_run {
    uint32_t polygon_attr;
    uint8_t type;
    uint8_t first_vertex;
    uint8_t polygon_count;
    uint8_t flipped;
} gpu3d_primitive_run_t;

typedef struct gpu3d_texture_run {
    uint32_t texture_param;
    uint16_t palette_base;
    uint8_t first_vertex;
    uint8_t unmapped_0;
} gpu3d_texture_run_t;

typedef struct gpu3d_matrix_pair {
    int32_t position[16];
    int32_t vector[16];
} gpu3d_matrix_pair_t;

typedef struct gpu3d_bank_vertex {
    int32_t w;
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint16_t color;
    uint16_t s;
    uint16_t t;
} gpu3d_bank_vertex_t;

typedef struct gpu3d_vertex_bank {
    gpu3d_bank_vertex_t vertex[GPU3D_BANK_VERTICES];
    uint32_t count;
} gpu3d_vertex_bank_t;

typedef struct gpu3d_polygon {
    uint32_t texture_param;
    uint32_t polygon_attr;
    uint32_t vertex_count;
    uint32_t unmapped_0;
    struct gpu3d_texture_entry *texture;
    uint16_t palette_base;
    uint16_t first_vertex;
    uint8_t unmapped_1[4];
} gpu3d_polygon_t;

extern const uint32_t gpu3d_vertex_pattern_table[87];

typedef struct gpu3d_polygon_list {
    gpu3d_polygon_t polygon[GPU3D_BANK_POLYGONS];
    uint32_t count;
    uint32_t unmapped_0;
} gpu3d_polygon_list_t;

typedef struct gpu3d_trace {
    void *command_file;
    void *param_file;
    void *vram_file;
    void *video_file;
    uint8_t mode;
    uint8_t unmapped_0[7];
} gpu3d_trace_t;

typedef struct gpu3d {
    int32_t batch_x[GPU3D_BATCH_VERTICES];
    int32_t batch_y[GPU3D_BATCH_VERTICES];
    int32_t batch_z[GPU3D_BATCH_VERTICES];
    uint32_t batch_count;
    uint32_t texture_run_mark;
    uint32_t texture_run_count;
    uint32_t texture_run_index;
    uint32_t attribute_mark;
    uint8_t unmapped_0[4];
    uint8_t *normal_cursor;
    uint8_t *color_cursor;
    uint8_t *attribute_mark_cursor;
    uint32_t vertex_count;
    uint32_t primitive_run_count;
    uint32_t vertex_total;
    uint32_t batch_normal[GPU3D_BATCH_VERTICES + 4];
    uint16_t batch_color[GPU3D_BATCH_VERTICES + 8];
    uint8_t batch_attribute_mark[GPU3D_BATCH_VERTICES + 16];
    uint32_t batch_texcoord[GPU3D_BATCH_VERTICES];
    uint8_t unmapped_1[16];
    uint32_t run_texture_param;
    uint32_t run_palette_base;
    uint32_t last_color;
    uint32_t run_split_flag;
    uint32_t emitted_count;
    gpu3d_vertex_t vertex[GPU3D_LIST_VERTICES];
    uint8_t clip_code[GPU3D_LIST_VERTICES];
    uint16_t vertex_color[GPU3D_LIST_VERTICES];
    uint32_t vertex_texcoord[GPU3D_LIST_VERTICES];
    uint8_t unmapped_2[4];
    int32_t screen_x[GPU3D_SCREEN_VERTICES];
    int32_t screen_y[GPU3D_SCREEN_VERTICES];
    int32_t screen_z[GPU3D_SCREEN_VERTICES];
    uint32_t screen_w[GPU3D_SCREEN_VERTICES];
    gpu3d_primitive_run_t primitive_run[GPU3D_PRIMITIVE_RUNS];
    gpu3d_texture_run_t texture_run[GPU3D_TEXTURE_RUNS];
    gpu3d_matrix_pair_t matrix_stack[GPU3D_MATRIX_STACK_DEPTH];
    int32_t projection_stack[16];
    int32_t texture_stack[16];
    uint32_t light_vector_raw[GPU3D_LIGHTS];
    uint32_t light_color[GPU3D_LIGHTS];
    int32_t light_direction[GPU3D_LIGHTS][3];
    int32_t light_half_vector[GPU3D_LIGHTS][3];
    uint16_t light_diffuse[GPU3D_LIGHTS][3];
    uint16_t light_specular[GPU3D_LIGHTS][3];
    uint32_t ambient_accum[3];
    int32_t position_matrix[16];
    int32_t vector_matrix[16];
    int32_t clip_matrix[16];
    int32_t projection_matrix[16];
    int32_t texture_matrix[16];
    uint8_t shininess_table[128];
    uint16_t edge_color[8];
    uint16_t toon_table[32];
    uint8_t fog_table[32];
    uint8_t unmapped_3[31];
    uint8_t unmapped_4;
    uint8_t edge_color_expanded[3][8];
    uint8_t toon_table_expanded[3][32];
    uint8_t unmapped_5[4];
    struct nds *machine;
    struct gpu3d_texture_cache *texture_cache;
    uint32_t specular_emission_raw;
    uint32_t diffuse_ambient_raw;
    uint32_t diffuse_color;
    uint32_t ambient_color;
    uint32_t specular_color;
    uint32_t emission_color;
    int32_t *position_matrix_ptr;
    int32_t *vector_matrix_ptr;
    uint8_t *command_cursor;
    uint8_t *param_cursor;
    uint8_t *command_pending_cursor;
    uint8_t *param_pending_cursor;
    uint32_t disp3dcnt;
    uint32_t clear_color;
    uint32_t polygon_attr;
    uint32_t texture_param;
    uint16_t texcoord[2];
    uint32_t fog_color;
    uint16_t polygon_count;
    uint16_t texcoord_raw[2];
    uint16_t clear_depth;
    uint16_t clear_image_offset;
    uint16_t fog_offset;
    uint16_t dot_depth;
    uint16_t texture_palette_base;
    uint16_t vertex_xyz[3];
    uint16_t viewport_size[2];
    uint16_t viewport_origin[2];
    uint8_t box_test_result;
    uint8_t alpha_test_ref;
    uint8_t bank;
    uint8_t params_remaining;
    uint8_t matrix_mode;
    uint8_t light_mask;
    uint8_t texcoord_mode;
    uint8_t position_stack_level;
    uint8_t projection_stack_level;
    uint8_t texture_stack_level;
    uint8_t swap_pending;
    uint8_t side_command_pending;
    uint8_t swap_seen;
    uint8_t swap_params_requested;
    uint8_t swap_params_previous;
    uint8_t swap_params;
    uint8_t shininess_enabled;
    uint8_t render_dirty;
    uint8_t clip_matrix_dirty;
    uint8_t light_dirty;
    uint8_t unmapped_6[2];
    gpu3d_vertex_bank_t vertex_bank[2];
    uint8_t unmapped_7[4];
    gpu3d_polygon_list_t opaque[2];
    gpu3d_polygon_list_t translucent[2];
    uint8_t command_ring[GPU3D_COMMAND_RING_SIZE];
    uint32_t param_ring[GPU3D_PARAM_RING_WORDS];
    uint32_t side_command[2];
    gpu3d_trace_t trace;
} gpu3d_t;
#endif

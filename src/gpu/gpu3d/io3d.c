#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <string.h>
#include "gpu3d.h"
#include "core_internals.h"

uint32_t gpu3d_io3d_gxstat_read(gpu3d_t *obj) {

    gpu3d_gxfifo_trace_buffer_rewind(obj);

    uint32_t a = obj->position_stack_level;
    uint32_t b = obj->projection_stack_level;
    uint32_t c = obj->swap_pending;
    uint32_t d = obj->box_test_result;

    uint32_t v = (a << 8) | (b << 13);
    v = (v & ~(0x1fu << 27)) | ((c & 0x1fu) << 27);
    v |= d << 1;
    return v | 0x6000000u;
}

extern void gpu3d_geometry_apply_lighting(gpu3d_t *p1);

uint32_t gpu3d_ram_count_polygons_read(gpu3d_t *obj) {

    gpu3d_geometry_apply_lighting(obj);
    gpu3d_poly_commit_pending_batch(obj);

    uint64_t idx = obj->bank;

    uint32_t a = obj->opaque[idx].count;
    uint32_t b = obj->translucent[idx].count;
    return a + b;
}

uint32_t gpu3d_io3d_ram_count_read(gpu3d_t *obj) {

    gpu3d_geometry_apply_lighting(obj);
    gpu3d_poly_commit_pending_batch(obj);

    uint64_t bank = obj->bank;
    return obj->vertex_bank[bank].count;
}

uint32_t gpu3d_io3d_clipmtx_result_read(gpu3d_t *obj, uint32_t n) {

    if (obj->clip_matrix_dirty) {
        gpu3d_matrix_mult_4x4_neon(obj->clip_matrix, obj->projection_matrix,
                                  obj->position_matrix_ptr);
        obj->clip_matrix_dirty = 0;
    }

    return (uint32_t)obj->clip_matrix[n];
}

uint32_t gpu3d_io3d_vecmtx_result_read(gpu3d_t *obj, uint32_t n) {
    const uint32_t *vec = (const uint32_t *)obj->vector_matrix_ptr;

    if (n >= 6) return vec[n + 2];
    return vec[n > 2 ? n + 1 : n];
}

unsigned int gpu3d_io3d_color_cache_primary_read(const gpu3d_t *state, unsigned int idx) {

    return state->toon_table[idx];
}

static inline uint32_t expand5(uint32_t v) {
    return (v == 0) ? 0u : (2u * v + 1u);
}

void gpu3d_io3d_color_cache_primary_store(gpu3d_t *base, uint32_t idx, uint32_t color) {
    base->toon_table[idx] = (uint16_t)color;

    base->toon_table_expanded[0][idx] = (unsigned char)expand5(color & 0x1f);
    base->toon_table_expanded[1][idx] = (unsigned char)expand5((color >> 5) & 0x1f);
    base->toon_table_expanded[2][idx] = (unsigned char)expand5((color >> 10) & 0x1f);
}

void gpu3d_io3d_color_cache_secondary_store(gpu3d_t *base, uint32_t idx, uint32_t color) {
    base->edge_color[idx] = (uint16_t)color;

    base->edge_color_expanded[0][idx] = (unsigned char)expand5(color & 0x1f);
    base->edge_color_expanded[1][idx] = (unsigned char)expand5((color >> 5) & 0x1f);
    base->edge_color_expanded[2][idx] = (unsigned char)expand5((color >> 10) & 0x1f);
}

unsigned int gpu3d_io3d_color_cache_secondary_read(const gpu3d_t *state, unsigned int idx) {

    return state->edge_color[idx];
}

void gpu3d_io3d_store_7bit_value(gpu3d_t *base, uint32_t idx, uint32_t value) {
    base->fog_table[idx] = (uint8_t)(value & 0x7f);
}

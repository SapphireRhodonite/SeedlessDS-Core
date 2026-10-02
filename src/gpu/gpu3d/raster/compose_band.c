#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "../../gpu.h"
#include <string.h>
#include <stddef.h>
#include "core_internals.h"
#include "mem_access.h"







void gpu3d_raster_band_compose_plain(unsigned char *machine, unsigned char *dest,
                        uint32_t row) {

    void *(*c_memcpy)(void *, const void *, unsigned long) =
        (void *(*)(void *, const void *, unsigned long))sym_libc_memcpy;

    gpu3d_band_header_t *cfg = (gpu3d_band_header_t *)(machine + GPU3D_BAND_HEADER_OFFSET);
    gpu_t *a = cfg->gpu;
    gpu3d_t *ctx = cfg->gpu3d;
    uint32_t width = a->raster.clear_depth_word;
    unsigned char *tab2 = (unsigned char *)ctx->edge_color_expanded[0];

    unsigned char *top = GPU3D_BAND(machine)->plane1;

    unsigned char raw[512 + 8];
    unsigned char *scratch = raw;
    if (((uintptr_t)scratch & 8) != 0) scratch += 8;

    if (row != 0) {
        uint32_t k = row - 1;
        c_memcpy(a->band_rows + 2048 + (uint64_t)(uint32_t)(k << 10) * 4,
                 top, 2048);
        c_memcpy(a->band_rows + GPU3D_BAND_ROWS_BACK + GPU3D_BAND_ROW_BYTES + (uint64_t)(uint32_t)(k << 9) * 4,
                 machine, 1024);
    } else {
        gpu3d_raster_test_neighbor_edge_bordered(
            scratch, top, GPU3D_BAND(machine)->plane1 + 0x400, width);
        gpu3d_raster_palette_blit_256(dest, machine, scratch, tab2);
    }

    for (uint64_t g = 0; g < 14; g++) {
        uint64_t d = g * 0x400;
        gpu3d_raster_test_neighbor_edge_dual(scratch, GPU3D_BAND(machine)->plane1 + d, GPU3D_BAND(machine)->plane1 + 0x400 + d,
              GPU3D_BAND(machine)->plane1 + RECON_ROW_STEP + d, width);
        gpu3d_raster_palette_blit_256(dest + 0x400 + d, machine + 0x400 + d, scratch, tab2);
    }

    if (row == 11) {
        gpu3d_raster_test_neighbor_edge_bordered_alt(
            scratch, GPU3D_BAND(machine)->plane1 + 14u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(machine)->plane1 + 15u * GPU3D_BAND_ROW_BYTES, width);
        gpu3d_raster_palette_blit_256(dest + 15u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(machine)->plane0 + 15u * GPU3D_BAND_ROW_BYTES, scratch, tab2);
        return;
    }

    c_memcpy((unsigned char *)a->band_rows + (uint64_t)(uint32_t)(row << 10) * 4,
             GPU3D_BAND(machine)->plane1 + 14u * GPU3D_BAND_ROW_BYTES, 2048);
    c_memcpy(a->band_rows + GPU3D_BAND_ROWS_BACK + (uint64_t)(uint32_t)(row << 9) * 4,
             GPU3D_BAND(machine)->plane0 + 15u * GPU3D_BAND_ROW_BYTES, 1024);
}

void gpu3d_raster_band_flush_plain(gpu_t *machine) {

    gpu_t *gpu = machine;
    uint32_t width = gpu->raster.clear_depth_word;
    unsigned char *out = gpu->raster.frame_front;

    unsigned char *table = (unsigned char *)((gpu3d_band_header_t *)(gpu->band_context[0] + GPU3D_BAND_HEADER_OFFSET))->gpu3d->edge_color_expanded[0];

    unsigned char raw[288 + 8];
    unsigned char *scratch = raw;
    if (((uintptr_t)scratch & 8) != 0) scratch += 8;

    for (uint64_t g = 0; g < 11; g++) {
        unsigned char *A = (unsigned char *)gpu->band_rows + g * 0x1000;
        unsigned char *t = gpu->band_rows + GPU3D_BAND_ROWS_BACK + g * 0x800;

        gpu3d_raster_test_neighbor_edge_dual(scratch, A, A + 0x400, A + 0x800, width);
        gpu3d_raster_palette_blit_256(out + 15u * GPU3D_BAND_ROW_BYTES + g * 16u * GPU3D_BAND_ROW_BYTES, t, scratch, table);

        gpu3d_raster_test_neighbor_edge_dual(scratch, A + 0x400, A + 0x800, A + 0xc00, width);
        gpu3d_raster_palette_blit_256(out + 16u * GPU3D_BAND_ROW_BYTES + g * 16u * GPU3D_BAND_ROW_BYTES, t + 0x400, scratch, table);
    }
}

void gpu3d_raster_pixel_alpha_blend_256(uint32_t *param_1, const uint32_t *param_2,
                         const uint8_t *param_3, uint32_t param_4) {

    uint32_t P = (param_4 >> 24) & 0x1f;

    long i = 0;
    do {
        uint8_t byte_table = param_3[i];
        uint32_t color = param_2[i];

        uint8_t bVar2 = (byte_table == 0x7f) ? 0x80 : byte_table;

        uint32_t a_current = (color >> 24) & 0x7f;

        uint32_t uVar3 = 0;
        if ((int32_t)color < 0) {
            uVar3 = (uint32_t)bVar2;
        }

        uint32_t updated_a = a_current + ((uVar3 * (P - a_current)) >> 7);

        param_1[i] = (color & 0xFFFFFFu) | (updated_a * 0x1000000u);

        i = i + 1;
    } while (i != 0x100);
}

void gpu3d_raster_band_compose_fogged(unsigned char *machine, unsigned char *dest,
                        uint32_t row) {

    void *(*c_memcpy)(void *, const void *, unsigned long) =
        (void *(*)(void *, const void *, unsigned long))sym_libc_memcpy;

    gpu3d_band_header_t *cfg = (gpu3d_band_header_t *)(machine + GPU3D_BAND_HEADER_OFFSET);
    gpu_t *a = cfg->gpu;

    if (cfg->band_dirty == 0
        || a->raster.frame_ready == 0) {

        gpu3d_raster_band_compose_plain(machine, dest, row);
        return;
    }

    gpu3d_raster_t *state = &a->raster;
    gpu3d_t *ctx = cfg->gpu3d;
    uint32_t width = state->clear_depth_word;
    unsigned char *count  = (unsigned char *)&ctx->fog_color;
    unsigned char *tab2  = (unsigned char *)ctx->edge_color_expanded[0];
    unsigned char *table = (unsigned char *)ctx->fog_table;

    uint32_t shift = (*(uint32_t *)state >> 8) & 0xf;
    uint32_t height = (0x400u >> shift) + (ctx->fog_offset & 0x7fffu);
    uint32_t pair = (shift & 0xffffu) | (height << 16);

    unsigned char raw[512 + 8];
    unsigned char *scratch = raw;
    if (((uintptr_t)scratch & 8) != 0) scratch += 8;

    unsigned char *top = GPU3D_BAND(machine)->plane1;

    if (row != 0) {
        uint32_t k = row - 1;
        c_memcpy(a->band_rows + 2048 + (uint64_t)(uint32_t)(k << 10) * 4,
                 top, 2048);
        c_memcpy(a->band_rows + GPU3D_BAND_ROWS_BACK + GPU3D_BAND_ROW_BYTES + (uint64_t)(uint32_t)(k << 9) * 4,
                 machine, 1024);
    } else {
        gpu3d_raster_fog_density_line(top, scratch, table, pair);
        gpu3d_raster_blend_fog_line(machine, machine, scratch, *(uint32_t *)count);
        gpu3d_raster_test_neighbor_edge_bordered(
            scratch, top, GPU3D_BAND(machine)->plane1 + 0x400, width);
        gpu3d_raster_palette_blit_256(dest, machine, scratch, tab2);
    }

    unsigned char *sig = dest + 0x400;
    uint64_t d = 0;
    for (int n = 0; n < 14; n++) {
        unsigned char *base = machine + d;
        unsigned char *layer = base + 0x400;
        unsigned char *high = base + GPU3D_BAND_PLANE_SIZE;
        unsigned char *meter = GPU3D_BAND(machine)->plane1 + 0x400 + d;

        gpu3d_raster_fog_density_line(meter, scratch, table, pair);
        gpu3d_raster_blend_fog_line(layer, layer, scratch, *(uint32_t *)count);
        gpu3d_raster_test_neighbor_edge_dual(
            scratch, high, meter, GPU3D_BAND(machine)->plane1 + RECON_ROW_STEP + d, width);
        gpu3d_raster_palette_blit_256(sig + d, layer, scratch, tab2);
        d += 0x400;
    }

    unsigned char *base = machine + d;
    unsigned char *layer = base + 0x400;
    unsigned char *high = base + GPU3D_BAND_PLANE_SIZE;

    if (row == 11) {
        gpu3d_raster_fog_density_line(high, scratch, table, pair);
        gpu3d_raster_blend_fog_line(layer, layer, scratch, *(uint32_t *)count);
        gpu3d_raster_test_neighbor_edge_bordered_alt(
            scratch, high, base + GPU3D_BAND_PLANE_SIZE + 0x400, width);
        gpu3d_raster_palette_blit_256(sig + d, layer, scratch, tab2);
        return;
    }

    c_memcpy((unsigned char *)a->band_rows + (uint64_t)(uint32_t)(row << 10) * 4,
             high, 2048);
    c_memcpy(a->band_rows + GPU3D_BAND_ROWS_BACK + (uint64_t)(uint32_t)(row << 9) * 4,
             layer, 1024);
}

void gpu3d_raster_band_flush_fogged(gpu_t *machine) {

    gpu_t *gpu = machine;
    gpu3d_t *ctx = ((gpu3d_band_header_t *)(gpu->band_context[0] + GPU3D_BAND_HEADER_OFFSET))->gpu3d;
    unsigned char *layer = gpu->band_rows + GPU3D_BAND_ROWS_BACK;
    unsigned char *bl0 = (unsigned char *)gpu->band_rows;
    unsigned char *bl1 = gpu->band_rows + 1024;
    unsigned char *bl2 = gpu->band_rows + 2048;
    unsigned char *bl3 = gpu->band_rows + 3072;
    gpu3d_raster_t *cfg = &gpu->raster;

    unsigned char *count  = (unsigned char *)&ctx->fog_color;
    unsigned char *tab2  = (unsigned char *)ctx->edge_color_expanded[0];
    unsigned char *table = (unsigned char *)ctx->fog_table;

    unsigned char raw[288 + 8];
    unsigned char *scratch = raw;
    if (((uintptr_t)scratch & 8) != 0) scratch += 8;

    uint32_t field = *(uint32_t *)cfg;
    uint32_t width = cfg->clear_depth_word;
    unsigned char *output = cfg->frame_front + 16u * GPU3D_BAND_ROW_BYTES;

    uint32_t shift = (field >> 8) & 0xf;
    uint32_t height = (ctx->fog_offset & 0x7fff) + (0x400u >> shift);
    uint32_t pair  = (shift & 0xffffu) | (height << 16);

    for (uint64_t d = 0; d != 44u * GPU3D_BAND_ROW_BYTES; d += 4u * GPU3D_BAND_ROW_BYTES) {
        unsigned char *b = bl0 + d;
        unsigned char *a = bl1 + d;
        unsigned char *before = output - 0x400;

        gpu3d_raster_fog_density_line(a, scratch, table, pair);
        gpu3d_raster_blend_fog_line(layer, layer, scratch, *(uint32_t *)count);
        unsigned char *c = bl2 + d;
        gpu3d_raster_test_neighbor_edge_dual(scratch, b, a, c, width);
        gpu3d_raster_palette_blit_256(before, layer, scratch, tab2);

        unsigned char *layer2 = layer + 0x400;
        gpu3d_raster_fog_density_line(c, scratch, table, pair);
        gpu3d_raster_blend_fog_line(layer2, layer2, scratch, *(uint32_t *)count);
        gpu3d_raster_test_neighbor_edge_dual(scratch, a, c, bl3 + d, width);
        gpu3d_raster_palette_blit_256(output, layer2, scratch, tab2);

        output += 0x4000;
        layer   += 0x800;
    }
}



void gpu3d_raster_page_compose_fog_alpha(uint8_t *state, uint8_t *output)
{

    gpu3d_band_header_t *hdr = (gpu3d_band_header_t *)(state + GPU3D_BAND_HEADER_OFFSET);
    gpu3d_raster_t *tables;
    gpu3d_t *origin;
    uint8_t *idx;
    uint32_t mode;
    uint32_t word;
    uint16_t base;
    uint8_t tmp[256] __attribute__((aligned(16)));

    if (hdr->band_dirty == 0) {
        gpu3d_raster_words_copy_mask29((uint32_t *)(output), (const uint32_t *)(state));
        return;
    }

    tables = &hdr->gpu->raster;
    if (tables->frame_ready == 0) {
        gpu3d_raster_words_copy_mask29((uint32_t *)(output), (const uint32_t *)(state));
        return;
    }

    origin = hdr->gpu3d;
    idx = (unsigned char *)&origin->fog_color;
    word = tables->disp3dcnt;
    mode = (word >> 8) & 0xf;
    base = (uint16_t)(origin->fog_offset & 0x7fff);
    word = (UINT32_C(0x400) >> mode) + base;
    mode = (mode & UINT32_C(0xffff)) | ((word & UINT32_C(0xffff)) << 16);

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output, state, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 0x400, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 0x400, state + 0x400, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + RECON_ROW_STEP, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 0x800, state + 0x800, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 0xc00, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 0xc00, state + 0xc00, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 2u * RECON_ROW_STEP, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 4u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 4u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 5u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 5u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 5u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 6u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 6u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 6u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 7u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 7u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 7u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 8u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 8u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 8u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 9u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 9u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 9u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 10u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 10u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 10u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 11u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 11u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 11u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 12u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 12u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 12u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 13u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 13u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 13u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 14u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 14u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 14u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));

    gpu3d_raster_fog_density_line(GPU3D_BAND(state)->plane1 + 15u * GPU3D_BAND_ROW_BYTES, tmp, (unsigned char *)origin->fog_table, mode);
    gpu3d_raster_set_alpha_plane(output + 15u * GPU3D_BAND_ROW_BYTES, GPU3D_BAND(state)->plane0 + 15u * GPU3D_BAND_ROW_BYTES, tmp, rd32(idx));
}

void gpu3d_raster_band_compose_fog_alpha(unsigned char *machine, unsigned char *dest,
                        uint32_t row) {

    void *(*c_memcpy)(void *, const void *, unsigned long) =
        (void *(*)(void *, const void *, unsigned long))sym_libc_memcpy;

    gpu3d_band_header_t *cfg = (gpu3d_band_header_t *)(machine + GPU3D_BAND_HEADER_OFFSET);
    gpu_t *a = cfg->gpu;

    if (cfg->band_dirty == 0
        || a->raster.frame_ready == 0) {

        gpu3d_raster_band_compose_plain(machine, dest, row);
        return;
    }

    gpu3d_raster_t *state = &a->raster;
    gpu3d_t *ctx = cfg->gpu3d;
    uint32_t width = state->clear_depth_word;
    unsigned char *count  = (unsigned char *)&ctx->fog_color;
    unsigned char *tab2  = (unsigned char *)ctx->edge_color_expanded[0];
    unsigned char *table = (unsigned char *)ctx->fog_table;

    uint32_t shift = (*(uint32_t *)state >> 8) & 0xf;
    uint32_t height = (0x400u >> shift) + (ctx->fog_offset & 0x7fffu);
    uint32_t pair = (shift & 0xffffu) | (height << 16);

    unsigned char raw[512 + 8];
    unsigned char *scratch = raw;
    if (((uintptr_t)scratch & 8) != 0) scratch += 8;

    unsigned char *top = GPU3D_BAND(machine)->plane1;

    if (row != 0) {
        uint32_t k = row - 1;
        c_memcpy(a->band_rows + 2048 + (uint64_t)(uint32_t)(k << 10) * 4,
                 top, 2048);
        c_memcpy(a->band_rows + GPU3D_BAND_ROWS_BACK + GPU3D_BAND_ROW_BYTES + (uint64_t)(uint32_t)(k << 9) * 4,
                 machine, 1024);
    } else {
        gpu3d_raster_fog_density_line(top, scratch, table, pair);
        gpu3d_raster_set_alpha_plane(machine, machine, scratch, *(uint32_t *)count);
        gpu3d_raster_test_neighbor_edge_bordered(
            scratch, top, GPU3D_BAND(machine)->plane1 + 0x400, width);
        gpu3d_raster_palette_blit_256(dest, machine, scratch, tab2);
    }

    unsigned char *sig = dest + 0x400;
    uint64_t d = 0;
    for (int n = 0; n < 14; n++) {
        unsigned char *base = machine + d;
        unsigned char *layer = base + 0x400;
        unsigned char *high = base + GPU3D_BAND_PLANE_SIZE;
        unsigned char *meter = GPU3D_BAND(machine)->plane1 + 0x400 + d;

        gpu3d_raster_fog_density_line(meter, scratch, table, pair);
        gpu3d_raster_set_alpha_plane(layer, layer, scratch, *(uint32_t *)count);
        gpu3d_raster_test_neighbor_edge_dual(
            scratch, high, meter, GPU3D_BAND(machine)->plane1 + RECON_ROW_STEP + d, width);
        gpu3d_raster_palette_blit_256(sig + d, layer, scratch, tab2);
        d += 0x400;
    }

    unsigned char *base = machine + d;
    unsigned char *layer = base + 0x400;
    unsigned char *high = base + GPU3D_BAND_PLANE_SIZE;

    if (row == 11) {
        gpu3d_raster_fog_density_line(high, scratch, table, pair);
        gpu3d_raster_set_alpha_plane(layer, layer, scratch, *(uint32_t *)count);
        gpu3d_raster_test_neighbor_edge_bordered_alt(
            scratch, high, base + GPU3D_BAND_PLANE_SIZE + 0x400, width);
        gpu3d_raster_palette_blit_256(sig + d, layer, scratch, tab2);
        return;
    }

    c_memcpy((unsigned char *)a->band_rows + (uint64_t)(uint32_t)(row << 10) * 4,
             high, 2048);
    c_memcpy(a->band_rows + GPU3D_BAND_ROWS_BACK + (uint64_t)(uint32_t)(row << 9) * 4,
             layer, 1024);
}




void gpu3d_raster_row_scatter_flagged(uint8_t *param_1, uint8_t *param_2,
                         uint8_t *param_3, uint32_t param_4,
                         uint32_t param_5, uint8_t *param_6,
                         uint8_t *param_7)
{

    uint8_t *table_height;
    uint8_t *table_offset;
    uint8_t *output_1;
    uint8_t *output_2;
    uint8_t *source_1;
    uint8_t *source_2;
    uint32_t mark;
    uint32_t row;

    if (param_4 == 0)
        return;

    table_height = param_1 + 0x630;
    table_offset = param_1 + 0x580;
    output_1 = param_2;
    output_2 = param_3;
    source_1 = param_6;
    source_2 = param_7;
    mark = param_5 << 24;

    for (row = 0;; ++row) {
        uint16_t height = rd16(table_height);
        uint16_t offset = rd16(table_offset);

        table_height += 4;
        table_offset += 4;

        if (height != 0) {
            uint32_t block = 0;
            uintptr_t output_2_start = (uintptr_t)output_2 +
                                      (uintptr_t)offset * 4;
            uintptr_t output_1_start = (uintptr_t)output_1 +
                                      (uintptr_t)offset * 4;
            uintptr_t output_2_end = (uintptr_t)output_2 +
                                      (uintptr_t)(height + offset) * 4;
            uintptr_t output_1_end = (uintptr_t)output_1 +
                                      (uintptr_t)(height + offset) * 4;
            uintptr_t source_1_end = (uintptr_t)source_1 + (uintptr_t)height * 4;
            uintptr_t source_2_end = (uintptr_t)source_2 + (uintptr_t)height * 4;
            int overlap;

            overlap =
                (output_2_start < output_1_end && output_1_start < output_2_end) ||
                (output_2_start < source_1_end && (uintptr_t)source_1 < output_2_end) ||
                (output_2_start < source_2_end && (uintptr_t)source_2 < output_2_end) ||
                (output_1_start < source_1_end && (uintptr_t)source_1 < output_1_end) ||
                (output_1_start < source_2_end && (uintptr_t)source_2 < output_1_end);

            if (height >= 8 && !overlap) {
                block = (uint32_t)height & ~7u;

                for (uint32_t done = 0; done < block; done += 8) {
                    uint32_t from_source_2[8];
                    uint32_t from_source_1[8];
                    uint8_t *dest_2 = output_2 +
                                          ((uint32_t)offset + done) * 4;
                    uint8_t *dest_1 = output_1 +
                                          ((uint32_t)offset + done) * 4;

                    for (uint32_t i = 0; i != 8; ++i)
                        from_source_2[i] = rd32(source_2 + (done + i) * 4);
                    for (uint32_t i = 0; i != 8; ++i)
                        from_source_1[i] = rd32(source_1 + (done + i) * 4);
                    for (uint32_t i = 0; i != 8; ++i)
                        wr32(dest_2 + i * 4, from_source_2[i] | mark);
                    for (uint32_t i = 0; i != 8; ++i)
                        wr32(dest_1 + i * 4, from_source_1[i]);
                }

                source_1 += block * 4;
                source_2 += block * 4;
            }

            for (uint32_t done = block; done < (uint32_t)height; ++done) {
                uint32_t from_source_2 = rd32(source_2);
                uint32_t from_source_1 = rd32(source_1);

                wr32(output_2 + ((uint32_t)offset + done) * 4,
                             from_source_2 | mark);
                wr32(output_1 + ((uint32_t)offset + done) * 4,
                             from_source_1);
                source_1 += 4;
                source_2 += 4;
            }
        }

        output_1 += 0x800;
        if (row + 1 == param_4)
            return;
        output_2 += 0x800;
    }
}

void gpu3d_raster_row_scatter_planes(const unsigned char *base,
                        unsigned char *dst_a,
                        unsigned char *dst_b,
                        unsigned char *dst_c,
                        uint32_t rows,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *src_c)
{

    if (rows == 0) return;

    uint32_t f = 0;
    const unsigned char *p_cnt = base + 0x630;
    const unsigned char *p_hue = base + 0x580;

    uint16_t n16, hue16;
    memcpy(&n16,   p_cnt, 2); p_cnt += 4;
    memcpy(&hue16, p_hue, 2); p_hue += 4;

    uint64_t n   = n16;
    uint64_t hue = hue16;

    for (;;) {
        if (n != 0) {
            uint64_t shift = hue << 2;
            int64_t  back = 0;
            uint64_t i = 0;

            unsigned char *wa = dst_a + shift;
            unsigned char *wb = dst_b + shift;
            unsigned char *wc = dst_c + hue;

            do {
                uint64_t off = i << 2;
                uint32_t v;

                memcpy(&v, src_a + off, 4);
                back -= 4;
                memcpy(wa + off, &v, 4);

                memcpy(&v, src_b + off, 4);
                memcpy(wb + off, &v, 4);

                wc[i] = src_c[i];

                i += 1;
            } while (n != i);

            src_a -= back;
            src_b -= back;
            src_c += i;
        }

        f += 1;
        dst_a += 0x800;
        dst_b += 0x800;
        dst_c += 0x200;
        if (f == rows) return;

        memcpy(&n16,   p_cnt, 2); p_cnt += 4;
        memcpy(&hue16, p_hue, 2); p_hue += 4;
        n   = n16;
        hue = hue16;
    }
}

void gpu3d_raster_row_scatter_masked(unsigned char *dst_a,
                        unsigned char *dst_b,
                        uint32_t width,
                        uint32_t rows,
                        const unsigned char *src,
                        uint32_t value,
                        const unsigned char *mask)
{

    uint32_t empty = (uint32_t)(rows == 0) | (uint32_t)(width == 0);
    uint32_t rest = width & 3u;

    uint32_t f;
    uint32_t v;

    if (rest != 0) {

        if (empty & 1u) return;

        uint64_t step_mask = (uint64_t)(uint32_t)(width - 1u) + 1u;
        uint64_t width64   = (uint64_t)width;

        f = 0;

        for (;;) {
            int64_t  acc = 0;
            uint64_t i   = 0;
            unsigned char c;

            c = mask[i];
            if (c != 0) goto copy;
            goto skip;

        reread:
            c = mask[i];
            if (c == 0) goto skip;
            goto copy;

        skip:
            i   += 1;
            acc -= 4;
            if (width64 == i) goto end_row;
            goto reread;

        copy:
            {
                uint64_t off = i << 2;
                memcpy(&v, src + off, 4);
                memcpy(dst_b + off, &value, 4);
                memcpy(dst_a + off, &v, 4);
            }
            i   += 1;
            acc -= 4;
            if (width64 != i) goto reread;

        end_row:
            f += 1;
            mask  += step_mask;
            dst_a += 0x800;
            dst_b += 0x800;

            src   -= acc;
            if (f == rows) return;
        }
    } else {

        if (empty & 1u) return;

        uint64_t i;
        const unsigned char *p;
        unsigned char c;
        uint64_t off;

        f = 0;

    row_b:
        i = 0;
        p = mask + i;
        c = *p;
        if (c != 0) goto slot0;
        goto slot1;

    group:
        i   += 4;
        src += 16;
        if ((uint32_t)i >= width) goto end_row_b;
        p = mask + i;
        c = *p;
        if (c == 0) goto slot1;

    slot0:
        memcpy(&v, src, 4);
        off = (uint64_t)(uint32_t)i << 2;
        memcpy(dst_b + off, &value, 4);
        memcpy(dst_a + off, &v, 4);

    slot1:
        c = p[1];
        if (c != 0) {
            memcpy(&v, src + 4, 4);
            off = (uint64_t)((uint32_t)i + 1u) << 2;
            memcpy(dst_b + off, &value, 4);
            memcpy(dst_a + off, &v, 4);
        }

        c = p[2];
        if (c != 0) {
            memcpy(&v, src + 8, 4);
            off = (uint64_t)((uint32_t)i + 2u) << 2;
            memcpy(dst_b + off, &value, 4);
            memcpy(dst_a + off, &v, 4);
        }

        c = p[3];
        if (c != 0) {
            memcpy(&v, src + 12, 4);
            off = (uint64_t)((uint32_t)i + 3u) << 2;
            memcpy(dst_b + off, &value, 4);
            memcpy(dst_a + off, &v, 4);
        }
        goto group;

    end_row_b:
        f += 1;
        dst_a += 0x800;
        dst_b += 0x800;
        mask  += i;
        if (f == rows) return;
        goto row_b;
    }
}

void gpu3d_raster_row_unpack_planes(unsigned char *dst_a, unsigned char *dst_b,
                        unsigned char *dst_c, uint32_t width, uint32_t rows,
                        const unsigned char *src_a, const unsigned char *src_b,
                        const unsigned char *src_c)
{

    if (rows == 0) return;
    if (width == 0) return;

    uint32_t f = 0;
    uint64_t lim = (uint64_t)width;

    do {
        uint64_t acc = 0;
        uint64_t i = 0;

        do {
            uint64_t off = i << 2;
            uint32_t w;

            memcpy(&w, src_a + off, 4);
            acc -= 4;
            memcpy(dst_a + off, &w, 4);

            memcpy(&w, src_b + off, 4);
            memcpy(dst_b + off, &w, 4);

            dst_c[i] = src_c[i];

            i += 1;
        } while (lim != i);

        f += 1;
        dst_a += 0x800;
        dst_b += 0x800;
        dst_c += 0x200;
        src_c += i;

        src_b -= (int64_t)acc;
        src_a -= (int64_t)acc;
    } while (f != rows);
}






void gpu3d_raster_row_gather_planes(unsigned char *dst,
                        const unsigned char *src,
                        const unsigned char *tables,
                        uint32_t rows)
{

    if (rows == 0) return;

    uint64_t row = 0;
    const unsigned char *p_n = tables + 0x630;
    const unsigned char *p_col = tables + 0x580;

    uint16_t n16 = rd16(p_n);
    p_n += 4;

    for (;;) {
        const unsigned char *origin_row = src;

        if (n16 != 0) {
            uint16_t col16 = rd16(p_col);
            uint64_t n = n16;
            uint64_t col = col16;
            const unsigned char *read_p = origin_row + (col << 2);
            uint64_t done = 0;

            uintptr_t start_dst = (uintptr_t)dst;
            uintptr_t start_src = (uintptr_t)read_p;
            uintptr_t end_dst = start_dst + (uintptr_t)(n << 2);
            uintptr_t end_src = start_src + (uintptr_t)(n << 2);

            if (n >= 8 && start_dst < end_src && start_src < end_dst) {
                uint64_t block = n & UINT64_C(0x1ffffffff8);
                const unsigned char *q = read_p;
                unsigned char *d = dst;
                uint64_t left = block;

                do {
                    uint64_t a = rd64(q);
                    uint64_t b = rd64(q + 8);
                    uint64_t c = rd64(q + 16);
                    uint64_t e = rd64(q + 24);
                    left -= 8;
                    wr64(d, a);
                    wr64(d + 8, b);
                    wr64(d + 16, c);
                    wr64(d + 24, e);
                    q += 32;
                    d += 32;
                } while (left != 0);

                read_p = q;
                dst = d;
                done = block;
            }

            uint32_t left = (uint32_t)n - (uint32_t)done;
            while (left != 0) {
                uint32_t v = rd32(read_p);
                read_p += 4;
                left -= 1;
                wr32(dst, v);
                dst += 4;
            }
        }

        row += 1;
        src += 0x800;
        p_col += 4;
        if ((uint32_t)row == rows) return;

        n16 = rd16(p_n);
        p_n += 4;
    }
}

void gpu3d_raster_band_gather_rows_tabled(unsigned char *dst_a,
                        unsigned char *dst_b,
                        unsigned char *dst_c,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *src_c,
                        const unsigned char *base,
                        uint32_t rows)
{

    if (rows == 0) return;

    uint32_t f = 0;
    const unsigned char *p_cnt = base + 0x630;
    const unsigned char *p_hue = base + 0x580;

    uint16_t n16, hue16;
    memcpy(&n16,   p_cnt, 2); p_cnt += 4;
    memcpy(&hue16, p_hue, 2); p_hue += 4;

    uint64_t n   = n16;
    uint64_t hue = hue16;

    for (;;) {
        if (n != 0) {
            uint64_t shift = hue << 2;
            int64_t  back = 0;
            uint64_t i = 0;

            const unsigned char *rc = src_c + hue;
            const unsigned char *rb = src_b + shift;
            const unsigned char *ra = src_a + shift;

            do {
                uint64_t off = i << 2;
                uint32_t v;

                memcpy(&v, ra + off, 4);
                back -= 4;
                memcpy(dst_a + off, &v, 4);

                memcpy(&v, rb + off, 4);
                memcpy(dst_b + off, &v, 4);

                dst_c[i] = rc[i];

                i += 1;
            } while ((uint32_t)n != (uint32_t)i);

            dst_a -= back;
            dst_b -= back;
            dst_c += i;
        }

        f += 1;
        src_a += 0x800;
        src_b += 0x800;
        src_c += 0x200;
        if (f == rows) return;

        memcpy(&n16,   p_cnt, 2); p_cnt += 4;
        memcpy(&hue16, p_hue, 2); p_hue += 4;
        n   = n16;
        hue = hue16;
    }
}





void *gpu3d_raster_band_copy_rows_compact(void *param_1, const void *param_2,
                          uint32_t param_3, uint32_t param_4)
{

    uintptr_t dest = (uintptr_t)param_1;
    uintptr_t origin = (uintptr_t)param_2;
    uint64_t width = param_3;
    uint64_t block = width & UINT64_C(0xfffffff8);

    if (param_4 == 0 || param_3 == 0)
        return (void *)dest;

    for (uint64_t row = 0; row != (uint64_t)param_4; ++row) {
        uintptr_t origin_row = origin + (row << 11);
        uintptr_t final_dest;

        if (param_3 < 8 ||
            (dest < origin_row + (width << 2) &&
             origin_row < dest + (width << 2))) {
            uintptr_t cursor = dest;

            for (uint64_t idx = 0; idx != width; ++idx) {
                uint32_t value = rd32(
                    (const void *)(origin_row + (idx << 2)));
                wr32((void *)cursor, value);
                cursor += 4;
            }
            final_dest = cursor;
        } else {
            uint64_t offset = 0;

            do {
                uint64_t a = rd64(
                    (const void *)(origin_row + offset));
                uint64_t b = rd64(
                    (const void *)(origin_row + offset + 8));
                uint64_t c = rd64(
                    (const void *)(origin_row + offset + 16));
                uint64_t d = rd64(
                    (const void *)(origin_row + offset + 24));

                wr64((void *)(dest + offset), a);
                wr64((void *)(dest + offset + 8), b);
                wr64((void *)(dest + offset + 16), c);
                wr64((void *)(dest + offset + 24), d);
                offset += 32;
            } while (offset != (block << 2));

            final_dest = dest + (block << 2);
            for (uint64_t idx = block; idx != width; ++idx) {
                uint32_t value = rd32(
                    (const void *)(origin_row + (idx << 2)));
                wr32((void *)final_dest, value);
                final_dest += 4;
            }
        }

        dest = final_dest;
    }

    return (void *)dest;
}

void gpu3d_raster_band_gather_rows_fixed(unsigned char *dst_a,
                        unsigned char *dst_b,
                        unsigned char *dst_c,
                        const unsigned char *src_a,
                        const unsigned char *src_b,
                        const unsigned char *src_c,
                        uint32_t cols,
                        uint32_t rows)
{

    if (rows == 0) return;

    if ((int32_t)cols <= 0) return;

    uint32_t w9  = (cols - 1u) & 0xfffffff8u;
    uint32_t w11 = 512u;
    uint32_t w10 = (cols + 7u) & 0xfffffff8u;
    uint32_t w12 = cols - w9;

    w9  = w11 - w10;
    w10 = w12 - 8u;

    int64_t  x10 = (int32_t)w10;
    uint64_t x9  = w9;

    uint32_t f   = 0;
    int64_t  x11 = x10 * 4;
    uint64_t x12 = x9 << 2;

    do {
        uint64_t l   = 0;
        uint32_t rem = cols;

        do {
            const unsigned char *pa = src_a + l;
            unsigned char       *qa = dst_a + l;
            const unsigned char *pb = src_b + l;
            unsigned char       *qb = dst_b + l;
            uint32_t v;
            int k;

            rem -= 8u;
            l   += 0x20u;

            for (k = 0; k < 8; k++) {
                memcpy(&v, pa + 4 * k, 4);
                memcpy(qa + 4 * k, &v, 4);
                memcpy(&v, pb + 4 * k, 4);
                memcpy(qb + 4 * k, &v, 4);
                dst_c[k] = src_c[k];
            }

            src_c += 8;
            dst_c += 8;
        } while ((int32_t)rem > 0);

        f     += 1;
        dst_c += x10;
        dst_a += x11 + (int64_t)l;
        dst_b += x11 + (int64_t)l;
        src_a += x12 + l;
        src_b += x12 + l;
        src_c += x9;
    } while (f != rows);
}

void gpu3d_raster_edge_mark_line(unsigned char *dst,
                        const unsigned char *lin,
                        const unsigned char *other,
                        uint32_t edge)
{

    uint32_t edge_id  = (edge >> 24) & 0x3fu;
    uint32_t edge_pro = edge & 0xffffffu;

    uint32_t v;

    {
        uint32_t c;
        memcpy(&c, lin, 4);

        if ((c & 0x40000000u) == 0) {
            v = 0xffu;
        } else {
            uint32_t a1, b0;
            memcpy(&a1, lin + 4, 4);
            memcpy(&b0, other, 4);

            uint32_t k = (((c >> 24) & 0x7fu) ^ 0x40u);
            uint32_t d = c & 0xffffffu;

            uint32_t any =
                (uint32_t)((d < edge_pro)        & (k != edge_id))
              | (uint32_t)((d < (a1 & 0xffffffu)) & (k != ((a1 >> 24) & 0x3fu)))
              | (uint32_t)((d < (b0 & 0xffffffu)) & (k != ((b0 >> 24) & 0x3fu)));

            v = (any != 0u ? 0u : 0xffu) | (k >> 3);
        }
        dst[0] = (unsigned char)v;
        dst += 1;
    }

    uint64_t i = 0;
    uint32_t c;
    memcpy(&c, lin + 4, 4);

    for (;;) {
        if (((c >> 30) & 1u) == 0) {
            v = 0xffu;
        } else {
            uint32_t left, right, ot;
            memcpy(&left, lin + i * 4, 4);
            memcpy(&right, lin + i * 4 + 8, 4);
            memcpy(&ot,  other + 4 + i * 4, 4);

            uint32_t d = c & 0xffffffu;
            uint32_t k = (((c >> 24) & 0x7fu) ^ 0x40u);

            uint32_t any =
                (uint32_t)((d < (left & 0xffffffu)) & (k != ((left >> 24) & 0x3fu)))
              | (uint32_t)((d < (right & 0xffffffu)) & (k != ((right >> 24) & 0x3fu)))
              | (uint32_t)((d < edge_pro)         & (k != edge_id))
              | (uint32_t)((d < (ot  & 0xffffffu)) & (k != ((ot  >> 24) & 0x3fu)));

            v = (any != 0u ? 0u : 0xffu) | (k >> 3);
        }

        dst[i] = (unsigned char)v;

        memcpy(&c, lin + i * 4 + 8, 4);
        i += 1;
        if ((uint32_t)i == 0x1feu) break;
    }

    {
        v = 0xffu;
        if (((c >> 30) & 1u) != 0) {
            uint32_t left, ot;
            memcpy(&left, lin + i * 4, 4);
            memcpy(&ot,  other + i * 4 + 4, 4);

            uint32_t d = c & 0xffffffu;
            uint32_t k = (((c >> 24) & 0x7fu) ^ 0x40u);

            uint32_t any =
                (uint32_t)((d < (left & 0xffffffu)) & (k != ((left >> 24) & 0x3fu)))
              | (uint32_t)((d < edge_pro)         & (k != edge_id))
              | (uint32_t)((d < (ot  & 0xffffffu)) & (k != ((ot  >> 24) & 0x3fu)));

            v = (any != 0u ? 0u : 0xffu) | (k >> 3);
        }
        dst[i] = (unsigned char)v;
    }
}


void gpu3d_raster_edge_mark_line_crossed(unsigned char *dst,
                        const unsigned char *sec,
                        const unsigned char *pri,
                        uint32_t ext)
{

    uint32_t w8 = rd32_at(pri, 0);
    uint32_t w9;
    uint32_t w11;

    if ((w8 >> 30) & 1u) {
        uint32_t w12 = rd32_at(pri, 4);
        uint32_t w13 = rd32_at(sec, 0);
        uint32_t w10;

        w11 = ((w8 >> 24) & 0x7fu) ^ 0x40u;
        w9  = (ext >> 24) & 0x3fu;
        w10 = w8 & 0xffffffu;
        w8  = ext & 0xffffffu;

        uint32_t t_ext = (uint32_t)(w10 < w8) & (uint32_t)(w11 != w9);
        uint32_t t_pri = (uint32_t)(w10 < (w12 & 0xffffffu))
                       & (uint32_t)(w11 != ((w12 >> 24) & 0x3fu));
        uint32_t t_sec = (uint32_t)(w10 < (w13 & 0xffffffu))
                       & (uint32_t)(w11 != ((w13 >> 24) & 0x3fu));
        uint32_t cond = t_sec | (t_ext | t_pri);

        w11 = (cond != 0u ? 0u : 0xffu) | (w11 >> 3);
    } else {
        w9  = (ext >> 24) & 0x3fu;
        w8  = ext & 0xffffffu;
        w11 = 0xffu;
    }

    dst[0] = (unsigned char)w11;
    dst += 1;

    uint32_t w13 = rd32_at(pri, 4);
    uint64_t i = 0;
    uint32_t bit30 = (w13 >> 30) & 1u;

    for (;;) {
        uint32_t byte;

        if (bit30) {
            uint32_t w17 = rd32_at(pri, i * 4);
            uint32_t w16 = rd32_at(pri, i * 4 + 8);
            uint32_t w14 = w13 & 0xffffffu;
            uint32_t tag = ((w13 >> 24) & 0x7fu) ^ 0x40u;
            uint32_t w15 = rd32_at(sec + 4, i * 4);

            uint32_t t_left = (uint32_t)(w14 < (w17 & 0xffffffu))
                           & (uint32_t)(tag != ((w17 >> 24) & 0x3fu));
            uint32_t t_right = (uint32_t)(w14 < (w16 & 0xffffffu))
                           & (uint32_t)(tag != ((w16 >> 24) & 0x3fu));
            uint32_t t_sec = (uint32_t)(w14 < (w15 & 0xffffffu))
                           & (uint32_t)(tag != ((w15 >> 24) & 0x3fu));
            uint32_t t_ext = (uint32_t)(w14 < w8) & (uint32_t)(tag != w9);
            uint32_t cond = t_ext | (t_sec | (t_left | t_right));

            byte = (cond != 0u ? 0u : 0xffu) | (tag >> 3);
        } else {
            byte = 0xffu;
        }

        dst[i] = (unsigned char)byte;

        w13 = rd32_at(pri, i * 4 + 8);
        i += 1;
        bit30 = (w13 >> 30) & 1u;
        if ((uint32_t)i == 0x1feu) break;
    }

    w11 = 0xffu;
    if ((w13 >> 30) & 1u) {
        uint32_t w15 = rd32_at(pri, i * 4);
        uint32_t w12 = w13 & 0xffffffu;
        uint32_t tag = ((w13 >> 24) & 0x7fu) ^ 0x40u;
        uint32_t w14 = rd32_at(sec + i * 4, 4);

        uint32_t t_left = (uint32_t)(w12 < (w15 & 0xffffffu))
                       & (uint32_t)(tag != ((w15 >> 24) & 0x3fu));
        uint32_t t_ext = (uint32_t)(w12 < w8) & (uint32_t)(tag != w9);
        uint32_t t_sec = (uint32_t)(w12 < (w14 & 0xffffffu))
                       & (uint32_t)(tag != ((w14 >> 24) & 0x3fu));
        uint32_t cond = t_sec | (t_ext | t_left);

        w11 = (cond != 0u ? 0u : 0xffu) | (tag >> 3);
    }

    dst[i] = (unsigned char)w11;
}

static uint8_t calc_one(uint32_t self, uint32_t left, uint32_t right,
                         uint32_t p2, uint32_t p4) {
    if ((self & 0x40000000u) == 0u) {
        return 0xff;
    }

    uint32_t self_low24 = self & 0xffffffu;
    uint32_t self_hi = ((self >> 24) & 0x7fu) ^ 0x40u;

    uint32_t left_low24 = left & 0xffffffu;
    uint32_t left_hi = (left >> 24) & 0x3fu;
    int term_left = (self_low24 < left_low24) && (self_hi != left_hi);

    uint32_t right_low24 = right & 0xffffffu;
    uint32_t right_hi = (right >> 24) & 0x3fu;
    int term_right = (self_low24 < right_low24) && (self_hi != right_hi);

    uint32_t p2_low24 = p2 & 0xffffffu;
    uint32_t p2_hi = (p2 >> 24) & 0x3fu;
    int term_p2 = (self_low24 < p2_low24) && (self_hi != p2_hi);

    uint32_t p4_low24 = p4 & 0xffffffu;
    uint32_t p4_hi = (p4 >> 24) & 0x3fu;
    int term_p4 = (self_low24 < p4_low24) && (self_hi != p4_hi);

    uint8_t base = (term_left || term_right || term_p2 || term_p4) ? 0u : 0xffu;
    return (uint8_t)(base | (self_hi >> 3));
}

void gpu3d_raster_edge_mark_grid(uint8_t *out, const uint32_t *param2,
                         const uint32_t *param3, const uint32_t *param4,
                         uint32_t param5) {

    out[0] = calc_one(param3[0], param5, param3[1], param2[0], param4[0]);

    for (uint32_t j = 1; j <= 510u; j++) {
        out[j] = calc_one(param3[j], param3[j - 1], param3[j + 1],
                           param2[j], param4[j]);
    }

    out[511] = calc_one(param3[511], param3[510], param5,
                         param2[511], param4[511]);
}

void gpu3d_raster_palette_deinterleave_512(unsigned char *dst,
                        const unsigned char *src,
                        const unsigned char *type,
                        const unsigned char *pal)
{

    for (int pass = 0; pass < 2; pass++) {
        uint64_t i = (uint64_t)pass;

        uint32_t t = type[i];

        for (;;) {
            uint32_t v;

            if (t <= 7u) {
                uint32_t c0 = pal[t];
                const unsigned char *q = pal + t;
                uint32_t c1 = q[8];
                uint32_t c2 = q[16];

                v = c0;
                v = (v & ~0x0000ff00u) | ((c1 & 0xffu) << 8);
                v = (v & ~0x00ff0000u) | ((c2 & 0xffu) << 16);
                v |= 0x1f000000u;
            } else {
                memcpy(&v, src + i * 4, 4);
            }

            v &= 0x1fffffffu;

            i += 2;

            memcpy(dst, &v, 4);
            dst += 4;

            if (i >= 0x200) break;

            t = type[i];
        }
    }
}

typedef void *(*fn_memcpy)(void *, const void *, size_t);

void gpu3d_raster_band_compose_scaled_plain(void *param_1, void *param_2, uint32_t param_3) {

    static fn_memcpy copy;
    if (!copy)
        copy = (fn_memcpy)sym_libc_memcpy;

    uint8_t *ctx = (uint8_t *)param_1;
    uint8_t *output = (uint8_t *)param_2;

    gpu3d_band_header_t *hdr = GPU3D_BAND_HEADER(ctx);
    gpu_t *table = hdr->gpu;
    gpu3d_t *palette_base = hdr->gpu3d;
    uint32_t edge = table->raster.clear_depth_word;
    const void *palette = (unsigned char *)palette_base->edge_color_expanded[0];

    uint8_t tmp[8u * 256u] __attribute__((aligned(16)));

    if (param_3 != 0) {
        uint32_t prev = param_3 - 1u;
        unsigned char *base = recon_tables_base((unsigned char *)table);

        uint64_t step_a = (uint64_t)(uint32_t)(prev * RECON_ROW_STEP) << 2;
        uint64_t step_b = (uint64_t)(uint32_t)(prev * (RECON_ROW_STEP / 2u)) << 2;

        copy(base + step_a + RECON_ARENA_A, ctx + RECON_3D_PLANE, 2u * RECON_ROW_STEP);
        copy(base + step_b + RECON_ARENA_B, ctx, RECON_ROW_STEP);
    } else {
        gpu3d_raster_test_neighbor_edge_bordered_x2(tmp, ctx + RECON_3D_PLANE,
                                ctx + RECON_3D_PLANE + RECON_ROW_STEP, edge);
        gpu3d_raster_palette_blit_deinterleave_512(output, ctx, tmp, palette);
    }

    for (uint32_t x28 = 0; x28 != RECON_ROW_STEP * RECON_WRAP_ROWS; x28 += RECON_ROW_STEP) {
        gpu3d_raster_test_neighbor_edge_dual_x2(tmp, ctx + x28 + RECON_3D_PLANE,
                                ctx + x28 + RECON_3D_PLANE + RECON_ROW_STEP,
                                ctx + x28 + RECON_3D_PLANE + 2u * RECON_ROW_STEP, edge);
        gpu3d_raster_palette_blit_deinterleave_512(output + RECON_ROW_STEP + x28,
                                ctx + x28 + RECON_ROW_STEP, tmp, palette);
    }

    if (param_3 == RECON_BANDS - 1u) {
        gpu3d_raster_test_neighbor_edge_bordered_x2_alt(tmp, ctx + RECON_3D_PLANE + RECON_ROW_STEP * RECON_WRAP_ROWS,

                                ctx + RECON_3D_PLANE + RECON_LAST_ROW, edge);
        gpu3d_raster_palette_blit_deinterleave_512(output + RECON_LAST_ROW, ctx + RECON_LAST_ROW,
                                tmp, palette);
    } else {
        unsigned char *base = recon_tables_base((unsigned char *)table);
        uint64_t step_a = (uint64_t)(uint32_t)(param_3 * RECON_ROW_STEP) << 2;
        uint64_t step_b = (uint64_t)(uint32_t)(param_3 * (RECON_ROW_STEP / 2u)) << 2;

        copy(base + step_a + RECON_ARENA_A0, ctx + RECON_3D_PLANE + RECON_ROW_STEP * RECON_WRAP_ROWS, 2u * RECON_ROW_STEP);
        copy(base + step_b + RECON_ARENA_B0, ctx + RECON_LAST_ROW, RECON_ROW_STEP);
    }
}


void gpu3d_raster_band_flush_scaled(gpu_t *machine)
{

    gpu_t *gpu = machine;
    unsigned char *tb = recon_tables_base((unsigned char *)gpu);

    uint32_t width = gpu->raster.clear_depth_word;
    uint8_t *output = gpu->raster.frame_front;
    uint8_t *palette = GPU3D_BAND_HEADER(recon_ctx3d_base((unsigned char *)gpu))->gpu3d->edge_color_expanded[0];
    _Alignas(16) uint8_t tmp[8u * 256u];

    for (uint32_t g = 0; g != RECON_BANDS - 1u; g++) {

        uint8_t *block = tb + RECON_ARENA_A0 + g * 4u * RECON_ROW_STEP;
        uint8_t *layer   = tb + RECON_ARENA_B0 + g * 2u * RECON_ROW_STEP;

        gpu3d_raster_test_neighbor_edge_dual_x2(tmp, block, block + RECON_ROW_STEP,
                                      block + 2u * RECON_ROW_STEP, width);
        gpu3d_raster_palette_blit_deinterleave_512(output + RECON_LAST_ROW + g * RECON_3D_BAND,
                                       layer, tmp, palette);

        gpu3d_raster_test_neighbor_edge_dual_x2(tmp, block + RECON_ROW_STEP,
                                      block + 2u * RECON_ROW_STEP, block + 3u * RECON_ROW_STEP, width);

        gpu3d_raster_palette_blit_deinterleave_512(output + RECON_3D_BAND + g * RECON_3D_BAND,
                                       layer + RECON_ROW_STEP, tmp, palette);
    }
}

void gpu3d_raster_depth_curve_lookup_512(const unsigned char *entry, unsigned char *output,
                        const unsigned char *table, uint32_t param) {
    const unsigned char *pend = table + 0x20;
    uint32_t shift = param & 0xffffu;
    uint32_t bias = param >> 16;
    int32_t cap = 0x7fff;
    for (uint64_t i = 0; i < 0x200; i++) {
        uint32_t v;
        memcpy(&v, entry + i * 4, 4);
        int32_t t = (int32_t)((v >> 9) & 0x7fffu);
        t = t - (int32_t)bias;
        t = (int32_t)((uint32_t)t & ~(uint32_t)(t >> 31));
        t = (int32_t)((uint32_t)t << (shift & 31u));
        if (!(t < cap)) t = cap;
        int64_t idx = (int64_t)(t >> 10);
        int32_t p = (int8_t)pend[idx];
        uint32_t base = table[idx];
        uint32_t frac = (uint32_t)t & 0x3ffu;
        uint32_t prod = (uint32_t)p * frac;
        output[i] = (unsigned char)(base + (prod >> 10));
    }
}



static uint32_t channel_lerp_128(uint32_t origin, uint32_t weight,
                                   uint32_t target)
{
    return origin + ((weight * (target - origin)) >> 7);
}

static uint32_t pixel_lerp_to_target(uint32_t word, uint32_t weight,
                                   uint32_t blue_obj, uint32_t green_obj,
                                   uint32_t red_obj, uint32_t alpha_obj)
{
    uint32_t blue = word & 0x3fu;
    uint32_t green = (word >> 8) & 0x3fu;
    uint32_t red = (word >> 16) & 0x3fu;
    uint32_t alpha = (word >> 24) & 0x7fu;

    if ((word & 0x80000000u) == 0)
        weight = 0;

    blue = channel_lerp_128(blue, weight, blue_obj);
    green = channel_lerp_128(green, weight, green_obj);
    red = channel_lerp_128(red, weight, red_obj);
    alpha = channel_lerp_128(alpha, weight, alpha_obj);

    return blue | (green << 8) | (red << 16) | (alpha << 24);
}

void gpu3d_raster_pixel_blend_bgra_512(uint8_t *output, const uint8_t *source,
                        const uint8_t *weights, uint32_t param)
{

    uint32_t blue_obj = param & 0x3fu;
    uint32_t green_obj = (param >> 8) & 0x3fu;
    uint32_t red_obj = (param >> 16) & 0x3fu;
    uint32_t alpha_obj = (param >> 24) & 0x1fu;
    uintptr_t limit_source = (uintptr_t)source + 0x800u;
    uintptr_t limit_output = (uintptr_t)output + 0x800u;
    uintptr_t limit_weights = (uintptr_t)weights + 0x200u;
    int overwrites_source = limit_source > (uintptr_t)output &&
                       limit_output > (uintptr_t)source;
    int overwrites_weights = limit_weights > (uintptr_t)output &&
                     limit_output > (uintptr_t)weights;

    if (!overwrites_source && !overwrites_weights) {
        for (size_t block = 0; block != 0x800u; block += 16u) {
            uint32_t word[4];
            uint32_t result[4];
            uint32_t weight[4];

            weight[0] = weights[0];
            weight[1] = weights[1];
            weight[2] = weights[2];
            weight[3] = weights[3];
            weights += 4;
            for (unsigned int lane = 0; lane != 4; ++lane) {
                if (weight[lane] == 0x7fu)
                    weight[lane] = 0x80u;
            }

            memcpy(word, source + block, sizeof(word));
            for (unsigned int lane = 0; lane != 4; ++lane)
                result[lane] = pixel_lerp_to_target(word[lane],
                                                        weight[lane], blue_obj,
                                                        green_obj, red_obj,
                                                        alpha_obj);
            memcpy(output + block, result, sizeof(result));
        }
        return;
    }

    for (size_t idx = 0; idx != 0x200u; ++idx) {
        uint32_t word = rd32(source + idx * 4u);
        uint32_t weight = weights[idx];

        if (weight == 0x7fu)
            weight = 0x80u;
        wr32(output + idx * 4u,
                    pixel_lerp_to_target(word, weight, blue_obj, green_obj,
                                        red_obj, alpha_obj));
    }
}

#define N 512

void gpu3d_raster_pixel_alpha_blend_512(uint32_t *dest, const uint32_t *origin,
                        const uint8_t *table, uint32_t param4) {

    uint32_t target = (param4 >> 24) & 0x1f;

    for (int i = 0; i < N; i++) {
        uint8_t  tb   = table[i];
        uint32_t oval = origin[i];

        uint32_t step = (tb == 0x7f) ? 0xffffff80u : (uint32_t)tb;
        step &= 0xff;

        uint32_t level  = (oval >> 24) & 0x7f;
        uint32_t factor = ((int32_t)oval < 0) ? step : 0u;

        uint32_t diff  = target - level;
        uint32_t prod  = factor * diff;
        uint32_t updated = level + (prod >> 7);

        dest[i] = (oval & 0x00ffffffu) | ((updated & 0xff) << 24);
    }
}
#undef N




static uint32_t blend_pixel(uint32_t pixel, uint32_t weight, uint32_t dest) {
    uint32_t c0 = pixel & 0x3fu;
    uint32_t c1 = (pixel >> 8) & 0x3fu;
    uint32_t c2 = (pixel >> 16) & 0x3fu;
    uint32_t c3 = (pixel >> 24) & 0x7fu;
    uint32_t d0 = dest & 0x3fu;
    uint32_t d1 = (dest >> 8) & 0x3fu;
    uint32_t d2 = (dest >> 16) & 0x3fu;
    uint32_t d3 = (dest >> 24) & 0x1fu;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;

    if (weight == 0x7fu)
        weight = 0x80u;
    if ((pixel & UINT32_C(0x80000000)) == 0)
        weight = 0;

    r0 = c0 + ((weight * (d0 - c0)) >> 7);
    r1 = c1 + ((weight * (d1 - c1)) >> 7);
    r2 = c2 + ((weight * (d2 - c2)) >> 7);
    r3 = c3 + ((weight * (d3 - c3)) >> 7);
    return r0 | (r1 << 8) | (r2 << 16) | (r3 << 24);
}

void gpu3d_raster_pixel_blend_bgra_deinterleaved(void *param_1, const void *param_2,
                         const void *param_3, uint32_t param_4) {

    uint8_t *output = (uint8_t *)param_1;
    const uint8_t *entry = (const uint8_t *)param_2;
    const uint8_t *weights = (const uint8_t *)param_3;
    uint32_t idx;

    for (idx = 0; idx < 0x200u; idx += 2) {
        uint32_t weight = rd8(weights + idx);
        uint32_t pixel = rd32(entry + idx * 4u);
        wr32(output + (idx / 2u) * 4u,
              blend_pixel(pixel, weight, param_4));
    }

    for (idx = 1; idx < 0x200u; idx += 2) {
        uint32_t weight = rd8(weights + idx);
        uint32_t pixel = rd32(entry + idx * 4u);
        wr32(output + 0x400u + ((idx - 1u) / 2u) * 4u,
              blend_pixel(pixel, weight, param_4));
    }
}




static uint32_t alpha_lerp_by_control(uint32_t pixel, uint8_t control,
                              uint32_t dest)
{
    uint32_t alpha = (pixel >> 24) & 0x7fu;
    uint32_t factor = control == 0x7fu ? 0x80u : control;
    uint32_t updated;

    if ((int32_t)pixel >= 0)
        factor = 0;
    updated = alpha + ((factor * (dest - alpha)) >> 7);
    return (pixel & 0x00ffffffu) | ((updated & 0xffu) << 24);
}

static void alpha_lerp_row_scalar(uint8_t *dst, const uint8_t *src,
                                 const uint8_t *control, uint32_t dest)
{
    unsigned int i;

    for (i = 0; i < 256; i++) {
        uint8_t c = rd8(control + 2u * i);
        uint32_t pixel = rd32(src + 8u * i);
        wr32(dst + 4u * i, alpha_lerp_by_control(pixel, c, dest));
    }
}

static void alpha_lerp_row_unrolled(uint8_t *dst, const uint8_t *src,
                                const uint8_t *control, uint32_t dest)
{
    unsigned int i;

    for (i = 0; i < 252; i += 4) {
        uint32_t p0 = rd32(src + 8u * (i + 0));
        uint32_t p1 = rd32(src + 8u * (i + 1));
        uint32_t p2 = rd32(src + 8u * (i + 2));
        uint32_t p3 = rd32(src + 8u * (i + 3));
        uint8_t c0 = rd8(control + 2u * (i + 0));
        uint8_t c1 = rd8(control + 2u * (i + 1));
        uint8_t c2 = rd8(control + 2u * (i + 2));
        uint8_t c3 = rd8(control + 2u * (i + 3));

        wr32(dst + 4u * (i + 0), alpha_lerp_by_control(p0, c0, dest));
        wr32(dst + 4u * (i + 1), alpha_lerp_by_control(p1, c1, dest));
        wr32(dst + 4u * (i + 2), alpha_lerp_by_control(p2, c2, dest));
        wr32(dst + 4u * (i + 3), alpha_lerp_by_control(p3, c3, dest));
    }

    for (; i < 256; i++) {
        uint8_t c = rd8(control + 2u * i);
        uint32_t pixel = rd32(src + 8u * i);
        wr32(dst + 4u * i, alpha_lerp_by_control(pixel, c, dest));
    }
}

void gpu3d_raster_alpha_blend_row_halves(uint8_t *dst, const uint8_t *src,
                         const uint8_t *control, uint32_t param)
{

    uint32_t dest = (param >> 24) & 0x1fu;
    uintptr_t d = (uintptr_t)dst;
    uintptr_t s = (uintptr_t)src;
    uintptr_t c = (uintptr_t)control;
    uint8_t *dst_second = dst + 0x400;

    if (((s + 0x7fcu > d) && (d + 0x400u > s)) ||
        ((c + 0x1ffu > d) && (d + 0x400u > c))) {
        alpha_lerp_row_scalar(dst, src, control, dest);
    } else {
        alpha_lerp_row_unrolled(dst, src, control, dest);
    }

    d = (uintptr_t)dst_second;
    if (((d < s + 0x800u) && (s + 4u < d + 0x400u)) ||
        ((c + 0x200u > d) && (c + 1u < d + 0x400u))) {
        alpha_lerp_row_scalar(dst_second, src + 4, control + 1, dest);
    } else {
        alpha_lerp_row_unrolled(dst_second, src + 4, control + 1, dest);
    }
}



void gpu3d_raster_plane_convert_scaled(unsigned char *param_1, unsigned char *param_2)
{

    gpu3d_band_header_t *hdr = GPU3D_BAND_HEADER(param_1);
    if (hdr->band_dirty == 0) {
        gpu3d_raster_deinterleave_words_blocks(param_2, param_1);
        return;
    }

    {
        const gpu3d_raster_t *control = &hdr->gpu->raster;

        if (control->frame_ready == 0) {
            gpu3d_raster_deinterleave_words_blocks(param_2, param_1);
            return;
        }

        {
            const gpu3d_t *table = hdr->gpu3d;
            uint32_t mode = control->disp3dcnt;
            uint16_t sample = table->fog_offset;
            uint32_t selector = (mode >> 8) & 0xfu;
            uint32_t scale = 0x400u >> selector;
            uint32_t control_color = selector |
                ((uint32_t)(uint16_t)(scale + (sample & 0x7fffu)) << 16);
            const unsigned char *factors = (unsigned char *)table->fog_table;
            const unsigned char *color = (unsigned char *)&table->fog_color;
            unsigned char tmp[2048] __attribute__((aligned(16)));
            uint32_t offset;

            for (offset = 0; offset != 0x10000u;
                 offset += 0x800u) {
                gpu3d_raster_fog_density_line_512((const unsigned int *)(param_1 + offset + RECON_3D_PLANE),
                                        tmp, factors, control_color);
                gpu3d_raster_fog_blend_color_split_parity(param_2 + offset,
                                        param_1 + offset, tmp,
                                        rd32(color));
            }
        }
    }
}

typedef void *(*fnp_memcpy)(void *, const void *, size_t);

void gpu3d_raster_band_compose_scaled_fogged(void *param_1, void *param_2, uint32_t param_3)
{

    uint8_t *ctx = (uint8_t *)param_1;
    uint8_t *output = (uint8_t *)param_2;
    gpu3d_band_header_t *hdr = GPU3D_BAND_HEADER(ctx);
    gpu_t *table;
    gpu3d_t *data;
    uint32_t width;
    uint32_t offset;
    uint32_t param;
    uint8_t tmp[8u * 256u] __attribute__((aligned(16)));

    if (hdr->band_dirty == 0u) {
        gpu3d_raster_band_compose_scaled_plain(param_1, param_2, param_3);
        return;
    }

    table = hdr->gpu;
    if (table->raster.frame_ready == 0u) {
        gpu3d_raster_band_compose_scaled_plain(param_1, param_2, param_3);
        return;
    }

    data = hdr->gpu3d;
    width = table->raster.clear_depth_word;
    offset = (table->raster.disp3dcnt >> 8) & 0xfu;
    param = offset |
        ((((0x400u >> (offset & 31u)) +
           (uint32_t)(data->fog_offset & 0x7fffu)) &
          0xffffu) << 16);

    if (param_3 != 0u) {
        uint32_t prev = param_3 - 1u;
        unsigned char *base = recon_tables_base((unsigned char *)table);

        uint64_t step_a = (uint64_t)(uint32_t)(prev * RECON_ROW_STEP) << 2;
        uint64_t step_b = (uint64_t)(uint32_t)(prev * (RECON_ROW_STEP / 2u)) << 2;
        static fnp_memcpy copy;

        if (!copy)
            copy = (fnp_memcpy)sym_libc_memcpy;
        copy(base + step_a + RECON_ARENA_A, ctx + RECON_3D_PLANE, 2u * RECON_ROW_STEP);
        copy(base + step_b + RECON_ARENA_B, ctx, RECON_ROW_STEP);
    } else {
        gpu3d_raster_fog_density_line_512((const unsigned int *)(ctx + RECON_3D_PLANE), tmp,
                                      (unsigned char *)data->fog_table, param);
        gpu3d_raster_fog_blend_color_line_512(ctx, ctx, tmp,
                                      data->fog_color);
        gpu3d_raster_test_neighbor_edge_bordered_x2(tmp, ctx + RECON_3D_PLANE,
                                      ctx + RECON_3D_PLANE + RECON_ROW_STEP, width);
        gpu3d_raster_palette_blit_deinterleave_512(output, ctx, tmp,
                                      (unsigned char *)data->edge_color_expanded[0]);
    }

    for (uint32_t shift_block = 0u;
         shift_block != RECON_3D_SPAN;
         shift_block += RECON_ROW_STEP) {
        uint8_t *block = ctx + shift_block;

        gpu3d_raster_fog_density_line_512((const unsigned int *)(block + RECON_3D_PLANE + RECON_ROW_STEP), tmp,
                                      (unsigned char *)data->fog_table, param);
        gpu3d_raster_fog_blend_color_line_512(block + RECON_ROW_STEP, block + RECON_ROW_STEP, tmp,
                                      data->fog_color);
        gpu3d_raster_test_neighbor_edge_dual_x2(tmp, block + RECON_3D_PLANE,
                                      block + RECON_3D_PLANE + RECON_ROW_STEP, block + RECON_3D_PLANE + 2u * RECON_ROW_STEP, width);

        gpu3d_raster_palette_blit_deinterleave_512(output + RECON_ROW_STEP + shift_block,
                                      block + RECON_ROW_STEP, tmp,
                                      (unsigned char *)data->edge_color_expanded[0]);
    }

    if (param_3 == RECON_BANDS - 1u) {
        gpu3d_raster_fog_density_line_512((const unsigned int *)(ctx + RECON_3D_PLANE + RECON_ROW_STEP * RECON_WRAP_ROWS), tmp,
                                      (unsigned char *)data->fog_table, param);
        gpu3d_raster_fog_blend_color_line_512(ctx + RECON_LAST_ROW, ctx + RECON_LAST_ROW,
                                      tmp,
                                      data->fog_color);
        gpu3d_raster_test_neighbor_edge_bordered_x2_alt(tmp, ctx + RECON_3D_PLANE + RECON_ROW_STEP * RECON_WRAP_ROWS,

                                      ctx + RECON_3D_PLANE + RECON_LAST_ROW, width);
        gpu3d_raster_palette_blit_deinterleave_512(output + RECON_LAST_ROW, ctx + RECON_LAST_ROW,
                                      tmp, (unsigned char *)data->edge_color_expanded[0]);
    } else {
        uint64_t step_a = (uint64_t)(uint32_t)(param_3 * RECON_ROW_STEP) << 2;
        uint64_t step_b = (uint64_t)(uint32_t)(param_3 * (RECON_ROW_STEP / 2u)) << 2;
        unsigned char *base = recon_tables_base((unsigned char *)table);
        static fnp_memcpy copy;

        if (!copy)
            copy = (fnp_memcpy)sym_libc_memcpy;

        copy(base + step_a + RECON_ARENA_A0, ctx + RECON_3D_PLANE + RECON_ROW_STEP * RECON_WRAP_ROWS, 2u * RECON_ROW_STEP);
        copy(base + step_b + RECON_ARENA_B0, ctx + RECON_LAST_ROW, RECON_ROW_STEP);
    }
}


void gpu3d_raster_band_compose_block_pairs(gpu_t *machine)
{

    gpu_t *gpu = machine;
    unsigned char *tb = recon_tables_base((unsigned char *)gpu);

    gpu3d_t *ctx = GPU3D_BAND_HEADER(recon_ctx3d_base((unsigned char *)gpu))->gpu3d;

    unsigned char *layer = tb + RECON_ARENA_B0;
    unsigned char *block0 = tb + RECON_ARENA_A0;
    unsigned char *block1 = block0 + RECON_ROW_STEP;
    unsigned char *block2 = block0 + 2u * RECON_ROW_STEP;
    unsigned char *block3 = block0 + 3u * RECON_ROW_STEP;
    gpu3d_raster_t *cfg = &gpu->raster;

    uint32_t field = cfg->disp3dcnt;
    uint16_t height_base = ctx->fog_offset;
    uint32_t width = cfg->clear_depth_word;
    unsigned char *output = cfg->frame_front + RECON_3D_BAND;
    uint32_t shift = (field >> 8) & 0x0fu;
    uint32_t height = (uint32_t)(height_base & 0x7fffu) +
                    (0x400u >> (shift & 31u));
    uint32_t param = shift | ((height & 0xffffu) << 16);

    _Alignas(16) unsigned char scratch[2048];

    const uint64_t step_slot = 4u * RECON_ROW_STEP;
    for (uint64_t d = 0; d != (RECON_BANDS - 1u) * step_slot; d += step_slot) {
        unsigned char *before = output - RECON_ROW_STEP;

        gpu3d_raster_fog_density_line_512((const unsigned int *)(block1 + d), scratch, (unsigned char *)ctx->fog_table,
                                param);

        gpu3d_raster_fog_blend_color_line_512(layer, layer, scratch,
                                 ctx->fog_color);
        gpu3d_raster_test_neighbor_edge_dual_x2(scratch, block0 + d, block1 + d,
                                block2 + d, width);

        gpu3d_raster_palette_blit_deinterleave_512(before, layer, scratch, (unsigned char *)ctx->edge_color_expanded[0]);

        gpu3d_raster_fog_density_line_512((const unsigned int *)(block2 + d), scratch, (unsigned char *)ctx->fog_table,
                                param);
        gpu3d_raster_fog_blend_color_line_512(layer + RECON_ROW_STEP, layer + RECON_ROW_STEP, scratch,
                                 ctx->fog_color);
        gpu3d_raster_test_neighbor_edge_dual_x2(scratch, block1 + d, block2 + d,
                                block3 + d, width);
        gpu3d_raster_palette_blit_deinterleave_512(output, layer + RECON_ROW_STEP, scratch,
                                 (unsigned char *)ctx->edge_color_expanded[0]);

        output += RECON_3D_BAND;
        layer += 2u * RECON_ROW_STEP;
    }
}

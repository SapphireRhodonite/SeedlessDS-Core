#include <stdint.h>
#include <stddef.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/nds.h"
#include "core/nds_state.h"
#include "frontend/frontend.h"
#include "core_internals.h"

#define MAIN_RAM_BASE 0x02000000u
#define MAIN_RAM_MASK 0x003fffffu
#define MAIN_RAM_BYTES 0x00400000u

int32_t nds_fx_load(nds_t *nds, const char *recipe_path, uint32_t vbo_vertex_offset,
                    uint32_t vbo_texcoord_offset)
{
    (void)nds;
    return video_out_gl_load_shader_recipe_slot0(recipe_path, vbo_vertex_offset,
                                                 vbo_texcoord_offset);
}

void nds_fx_setup(nds_t *nds, int source_width, int source_height, int x, int y,
                  int view_width, int view_height)
{
    (void)nds;
    video_out_gl_call_renderer_object_slot0(source_width,
                                                                   source_height,
                                                                   x, y,
                                                                   view_width,
                                                                   view_height);
}

void nds_fx_render(nds_t *nds, const nds_fx_frame_t *frame)
{
    (void)nds;
    video_out_gl_render_frame(frame->texture_top,
                                                     frame->texture_bottom,
                                                     frame->vertex_top,
                                                     frame->vertex_bottom,
                                                     frame->vertex_intermediate,
                                                     frame->top_width,
                                                     frame->top_height,
                                                     frame->bottom_width,
                                                     frame->bottom_height,
                                                     (uint32_t)(frame->flag != 0));
}

int32_t nds_extfx_load(nds_t *nds, const char *recipe_path, uint32_t vbo_vertex_offset,
                       uint32_t vbo_texcoord_offset)
{
    (void)nds;
    return video_out_gl_load_shader_recipe_slot1(recipe_path, vbo_vertex_offset,
                                                 vbo_texcoord_offset);
}

void nds_extfx_setup(nds_t *nds, int source_width, int source_height, int x, int y,
                     int view_width, int view_height)
{
    (void)nds;
    video_out_gl_call_renderer_object_slot1(source_width,
                                                                   source_height,
                                                                   x, y,
                                                                   view_width,
                                                                   view_height);
}

void nds_extfx_render(nds_t *nds, uint32_t texture, uint32_t index, uint32_t a,
                      uint32_t b, uint32_t c, uint32_t d)
{
    (void)nds;
    (void)video_out_gl_render_single_screen(texture, index,
                                                                    a, b, c, d);
}

size_t nds_memory_read(nds_t *nds, uint32_t address, void *buffer, size_t size)
{
    const uint8_t *ram = nds_main_ram(nds);
    uint8_t *out = buffer;
    size_t done = 0;

    if (ram == NULL)
        return 0;

    while (done < size) {
        uint32_t at = address + (uint32_t)done;

        if ((at & ~MAIN_RAM_MASK) != MAIN_RAM_BASE)
            break;
        out[done] = ram[at & MAIN_RAM_MASK];
        done++;
    }
    return done;
}

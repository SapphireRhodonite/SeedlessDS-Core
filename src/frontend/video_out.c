#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "present_hook.h"
#include "core/nds_state.h"
#include "frontend/video_out_gl.h"
#include "gpu/gpu2d/gpu2d.h"
#include "frontend/font.h"
#include "frontend/script/script_io.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"
#include "seedlessds/control.h"

uint64_t script_register_library(const char *name, const void *functions, unsigned count);
void video_out_snapshot_filter(uint32_t *dst, unsigned char *src,
                         uint32_t width, uint32_t height) {

    if ((uint32_t)(height - 1u) < 2u) return;
    if ((uint32_t)(width - 1u) < 2u) return;

    uint32_t row_off = 0;
    uint32_t row_cnt = 1;

    do {
        row_cnt = row_cnt + 1;
        uint32_t col_remaining = (width - 1u) - 1u;
        uint32_t k = row_off;

        do {
            uint32_t k1 = (width + 1u) + k;
            uint32_t k2 = (width * 2u + 2u) + k;

            uint32_t s0 = src[k];
            uint32_t s1 = src[k1];
            uint32_t s2 = src[k2];

            k = k + 1u;

            int32_t diff = (int32_t)(s0 * 2u) - (int32_t)s1;
            diff = diff - (int32_t)s2;
            diff = diff + 0x80;

            int32_t half = diff >> 1;
            uint32_t v = (uint32_t)half & (uint32_t)~(diff >> 31);

            if (!((int32_t)v < 0xff)) v = 0xff;

            uint32_t rgba = v | (v << 8) | (v << 16) | 0xff000000u;

            col_remaining = col_remaining - 1u;
            dst[k1] = rgba;
        } while (col_remaining != 0u);

        row_off = row_off + width;
    } while (row_cnt != height - 1u);
}
void script_io_reset(void) {
    volatile script_io_t *g = SCRIPT_IO;
    g->axis_lx = 0;
    g->axis_ly = 0;
    g->axis_rx = 0;
    g->axis_ry = 0;
    memset((void *)&g->rotation, 0, sizeof g->rotation + sizeof g->unmapped_0);
    g->layout = SCRIPT_IO_NONE;
    g->screen_swap = SCRIPT_IO_NONE;
    g->overlay = SCRIPT_IO_NONE;
}

void script_io_init(void) {

    script_io_t *e = SCRIPT_IO;
    e->axis_lx = 0;
    e->axis_ly = 0;
    e->axis_rx = 0;
    e->axis_ry = 0;
    memset(&e->rotation, 0, sizeof e->rotation + sizeof e->unmapped_0);
    e->layout = SCRIPT_IO_NONE;
    e->screen_swap = SCRIPT_IO_NONE;
    e->overlay = SCRIPT_IO_NONE;
    static const char script_io_module[] = "android";
    static const script_io_entry_t script_io_registry[9] = {
        { "get_axis_lx",     script_io_get_axis_lx },
        { "get_axis_ly",     script_io_get_axis_ly },
        { "get_axis_rx",     script_io_get_axis_rx },
        { "get_axis_ry",     script_io_get_axis_ry },
        { "get_rotation",    script_io_get_rotation },
        { "set_layout",      script_io_set_layout },
        { "show_overlay",    script_io_show_overlay },
        { "set_screen_swap", script_io_set_screen_swap },
        { 0, 0 },
    };

    (void)script_register_library(script_io_module, script_io_registry, 9);
}

void *script_io_publish_inputs(script_io_t *obj) {
    script_io_t *dest = SCRIPT_IO;

    memcpy(&dest->rotation, &obj->rotation, sizeof dest->rotation + sizeof dest->unmapped_0);
    memcpy(&dest->axis_lx, &obj->axis_lx, sizeof dest->axis_lx + sizeof dest->axis_ly);
    memcpy(&dest->axis_rx, &obj->axis_rx, sizeof dest->axis_rx + sizeof dest->axis_ry);
    return obj;
}

void *script_io_consume_outputs(uint32_t *dest) {
    script_io_t *g = SCRIPT_IO;

    uint32_t height = g->overlay;
    dest[2] = height;
    memcpy(dest, &g->layout, sizeof g->layout + sizeof g->screen_swap);

    g->layout = SCRIPT_IO_NONE;
    g->screen_swap = SCRIPT_IO_NONE;
    g->overlay = SCRIPT_IO_NONE;
    return dest;
}

uint64_t video_out_hook_noop(uint64_t x0)
{
    return x0;
}

void video_out_layer_toggle_hook_noop(void) {
}

void video_out_option_toggle_hook_noop(void) {
}
#define SCREEN_BYTES  0xC0000ULL

void *video_out_screen_buffer(unsigned int screen) {

    { extern void nds_output_read_begin(void); nds_output_read_begin(); }

    int32_t  idx = *(volatile int32_t *)&VIDEO_OUT_GL->page_parity;

    {
        extern unsigned long nds_output_frames;
        static volatile int32_t sel_seen = -1;
        if (idx != sel_seen) {
            if (sel_seen >= 0) __atomic_add_fetch(&nds_output_frames, 1, __ATOMIC_RELAXED);
            sel_seen = idx;
        }
    }
    uint64_t ptr = (uint64_t)(uintptr_t)*(void *volatile *)&VIDEO_OUT_GL->page[idx];

    { extern void nds_capture_screens(const void *, const volatile uint32_t *);
      nds_capture_screens((const void *)(uintptr_t)ptr, VIDEO_OUT_GL->screen_mode); }

    recon_native_read();
    uint64_t step_screen = recon_native ? (uint64_t)RECON_SCREEN_BYTES : SCREEN_BYTES;
    return (void *)(uintptr_t)(ptr + (uint64_t)(screen & 1u) * step_screen);
}
#undef SCREEN_BYTES

int nds_control_snapshot(int fd, int raw)
{
    nds_control_status_t status;
    nds_control_status(&status);
    if (!status.active || !status.paused || !nds_control_pause_requested()) return 0;
    const void *page = video_out_screen_buffer(0);
    unsigned long frame = __atomic_load_n(&nds_output_frames, __ATOMIC_RELAXED);
    if (!nds_control_capture(fd, frame, frame, raw)) return 0;
    nds_capture_screens(page, VIDEO_OUT_GL->screen_mode);
    nds_control_status(&status);
    return status.capture_complete && !status.error;
}

int video_out_screen_line_size(unsigned int screen) {
    int32_t bpp  = *(volatile int32_t *)&VIDEO_OUT_GL->bits_per_pixel;
    int32_t mode = *(volatile int32_t *)&VIDEO_OUT_GL->screen_mode[screen];
    int32_t short_form = (bpp == 16) ?  512 : 1024;

    recon_native_read();
    int32_t width = (bpp == 16) ? 2048 : 4096;
    if (recon_native)
        width = short_form * (int32_t)(recon_scale_out * recon_scale_out);
    return (mode == 0) ? short_form : width;
}

uint32_t video_out_screen_table_size(uint32_t i) {
    uint32_t mode = (uint32_t)VIDEO_OUT_GL->bits_per_pixel;
    uint32_t v = VIDEO_OUT_GL->screen_mode[i];
    uint32_t height = 0x800, mid = 0x400;
    const uint32_t low = 0x200;
    if (mode == 0x10) { height = mid; mid = low; }
    return (v == 0) ? mid : height;
}

uint32_t video_out_get_screen_mode(uint32_t i) {
    return VIDEO_OUT_GL->screen_mode[i];
}

unsigned int video_out_pixel_size(void) {

    uint32_t fmt = (uint32_t)VIDEO_OUT_GL->bits_per_pixel;
    return (fmt == 16) ? 2u : 4u;
}

int video_out_hook_returns_true(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return 1;
}

uint64_t video_out_present_hook_noop(uint64_t x0)
{
    return x0;
}
typedef void (*fn_bind)(unsigned int, unsigned int);
typedef void (*fn_sub)(unsigned int, int, int, int, int, int,
                       unsigned int, unsigned int, const void *);
typedef void (*fn_draw)(unsigned int, int, int);

static uint8_t *upload_snapshot;
static size_t upload_snapshot_size;

static uint8_t *upload_snapshot_reserve(size_t size)
{
    if (upload_snapshot_size < size) {
        void *p = NULL;
        if (posix_memalign(&p, 0x40, size) != 0 || p == NULL) return NULL;
        free(upload_snapshot);
        upload_snapshot = p;
        upload_snapshot_size = size;
    }
    return upload_snapshot;
}

void video_out_upload_screen_textures(uint32_t texture_first, uint32_t texture_second,
                        uint32_t idx)
{

    if (VIDEO_OUT_GL->page_memory == NULL)
        return;

    recon_native_read();
    uint64_t step_screen = recon_native ? (uint64_t)RECON_SCREEN_BYTES : (uint64_t)VIDEO_OUT_GL_SCREEN_PAGE_BYTES;

    nds_platform_default()->threads.mutex_lock(VIDEO_OUT_GL->mutex);

    uint32_t bit = idx & 1u;
    uint32_t other_bit = bit ^ 1u;
    uint32_t selector = VIDEO_OUT_GL->page_parity;
    uint32_t selector_table = (~selector) & 1u;
    uint32_t first_level = VIDEO_OUT_GL->screen_mode[bit];
    uint32_t level_second = VIDEO_OUT_GL->screen_mode[other_bit];
    uint32_t request = VIDEO_OUT_GL->upload_request;
    const uint8_t *page_pixels = (const uint8_t *)VIDEO_OUT_GL->page[selector_table];
    uint8_t *snapshot = upload_snapshot_reserve(2u * (size_t)step_screen);
    if (snapshot) {
        if (request & 1u) memcpy(snapshot + (size_t)bit * step_screen, page_pixels + (size_t)bit * step_screen, (size_t)step_screen);
        if (request & 2u) memcpy(snapshot + (size_t)other_bit * step_screen, page_pixels + (size_t)other_bit * step_screen, (size_t)step_screen);
    }
    VIDEO_OUT_GL->upload_request = 0;

    nds_platform_default()->threads.mutex_unlock(VIDEO_OUT_GL->mutex);

    const uint8_t *base_pixels = snapshot ? snapshot : page_pixels;
    ((fn_bind)sym_libc_glBindTexture)(0x0de1u, texture_first);

    if ((request & 1u) != 0) {
        uint32_t type = VIDEO_OUT_GL->gl_type;
        uint32_t format = VIDEO_OUT_GL->gl_format;
        uint32_t dimension = first_level + 1u;
        uint32_t n = (recon_native && dimension >= 2u)
                     ? recon_scale_output() : dimension;
        uint32_t width = n << 8;
        uint32_t height = (n + (n << 1)) << 6;
        const void *pixels = base_pixels + (uint64_t)bit * step_screen;

        ((fn_sub)sym_libc_glTexSubImage2D)(0x0de1u, 0, 0, 0,
                                                  (int)width, (int)height,
                                                  format, type, pixels);
    }

    ((fn_draw)sym_libc_glDrawArrays)(4u, 0, 6);

    if (texture_second != 0) {
        ((fn_bind)sym_libc_glBindTexture)(0x0de1u, texture_second);

        if ((request & 2u) != 0) {
            uint32_t type = VIDEO_OUT_GL->gl_type;
            uint32_t format = VIDEO_OUT_GL->gl_format;
            uint32_t dimension = level_second + 1u;
            uint32_t n = (recon_native && dimension >= 2u)
                         ? recon_scale_output() : dimension;
            uint32_t width = n << 8;
            uint32_t height = (n + (n << 1)) << 6;
            const void *pixels = base_pixels + (uint64_t)other_bit * step_screen;

            ((fn_sub)sym_libc_glTexSubImage2D)(0x0de1u, 0, 0, 0,
                                                      (int)width, (int)height,
                                                      format, type, pixels);
        }

        ((fn_draw)sym_libc_glDrawArrays)(4u, 6, 6);
    }
}
#define AXIS_OFF       0x3e6f0
#define MODE_OFF      171
#define X_FACTOR_OFF  158
#define Y_FACTOR_OFF  160
#define X_RECIP_OFF    76
#define Y_RECIP_OFF    80
#define READY_OFF     174

static int value_abs(int v) { return v < 0 ? -v : v; }




void video_out_prepare_scale_axes(uint8_t *state) {

    unsigned mode = 0x7ffu | ((unsigned)state[MODE_OFF] << 11);

    gpu2d_bg_text_compute_span_range(rd32(state + 144), rd16s(state + X_FACTOR_OFF), mode,
        rd16s(state + 162), (int64_t *)(state + 0x58), (int64_t *)(state + 0x68),
        (int64_t *)(state + 0x60));

    gpu2d_bg_text_compute_span_range(rd32(state + 148), rd16s(state + Y_FACTOR_OFF), mode,
        rd16s(state + 164), (int64_t *)(state + 0x70), (int64_t *)(state + 0x80),
        (int64_t *)(state + 0x78));

    uint32_t fx = (uint32_t)value_abs(rd16s(state + X_FACTOR_OFF));
    if (fx != 0) wr32(state + X_RECIP_OFF, (fx + GPU2D_RECIPROCAL_NUMERATOR) / fx);

    uint32_t fy = (uint32_t)value_abs(rd16s(state + Y_FACTOR_OFF));
    if (fy != 0) wr32(state + Y_RECIP_OFF, (fy + GPU2D_RECIPROCAL_NUMERATOR) / fy);

    state[READY_OFF] = 0;
}
#undef AXIS_OFF
#undef MODE_OFF
#undef X_FACTOR_OFF
#undef Y_FACTOR_OFF
#undef X_RECIP_OFF
#undef Y_RECIP_OFF
#undef READY_OFF

uint32_t video_out_blend_color_888(uint32_t c0, uint32_t c1, uint32_t weight) {
    uint32_t a0 = c0 & 0xffu, a1 = (c0 >> 8) & 0xffu, a2 = (c0 >> 16) & 0xffu;
    uint32_t b0 = c1 & 0xffu, b1 = (c1 >> 8) & 0xffu, b2 = (c1 >> 16) & 0xffu;
    a0 += ((b0 - a0) * weight) >> 7;
    a1 += ((b1 - a1) * weight) >> 7;
    a2 += ((b2 - a2) * weight) >> 7;
    return a0 | (a1 << 8) | (a2 << 16);
}

uint32_t video_out_blend_color_565(uint32_t c0, uint32_t c1, uint32_t weight) {
    uint32_t a0 = c0 & 0x1fu, a1 = (c0 >> 5) & 0x3fu, a2 = (c0 >> 11) & 0x1fu;
    uint32_t b0 = c1 & 0x1fu, b1 = (c1 >> 5) & 0x3fu, b2 = (c1 >> 11) & 0x1fu;
    a0 += ((b0 - a0) * weight) >> 7;
    a1 += ((b1 - a1) * weight) >> 7;
    a2 += ((b2 - a2) * weight) >> 7;
    return a0 | (a1 << 5) | (a2 << 11);
}






typedef void *(*fn_malloc)(size_t);





static uint32_t interpolate_7(uint32_t start, uint32_t delta, uint32_t step)
{
    return start + ((step * delta) >> 7);
}

static uint32_t color_32(uint32_t c)
{
    return ((c & 0x1fu) << 3) | (((c >> 5) & 0x3fu) << 10) |
           (((c >> 11) & 0x1fu) << 19);
}

void video_out_draw_text(void *param_1, const uint8_t *param_2,
                         uint32_t param_3, uint32_t param_4,
                         uint32_t param_5, uint32_t param_6)
{

    video_out_font_t *global = VIDEO_OUT_FONT;
    video_out_font_t *ctx = param_1 != NULL ? (video_out_font_t *)param_1 : global;
    uint32_t *table = global->blend_table;
    uint32_t *cache = &global->blend_key;
    uint32_t row = param_6;
    uint32_t column_initial = param_5;
    uint32_t column = column_initial;
    uint32_t color_a = param_4;
    uint32_t color_b = param_3;
    uint8_t *output = (uint8_t *)video_out_screen_buffer(0);
    uint32_t step_row = video_out_screen_line_size(0);
    uint32_t mode;
    uint8_t character;
    uint32_t pos;

    if (output == NULL) {
        output = (uint8_t *)video_out_screen_buffer(1);
        if (output == NULL)
            return;
    }

    mode = video_out_pixel_size();
    character = rd8(param_2);
    {
        uint32_t advance_line = ctx->height;

        if (mode == 2u) {
            if (cache[1] != color_a || cache[2] != color_b) {
                uint32_t low_a = color_a & 0x1fu;
                uint32_t mid_a = (color_a >> 5) & 0x3fu;
                uint32_t height_a = (color_a >> 11) & 0x1fu;
                uint32_t low_b = color_b & 0x1fu;
                uint32_t mid_b = (color_b >> 5) & 0x3fu;
                uint32_t height_b = (color_b >> 11) & 0x1fu;
                uint32_t delta_low = low_b - low_a;
                uint32_t delta_mid = mid_b - mid_a;
                uint32_t delta_height = height_b - height_a;
                uint32_t step;

                for (step = 0; step != 128u; ++step) {
                    uint32_t low = interpolate_7(low_a, delta_low, step);
                    uint32_t mid = interpolate_7(mid_a, delta_mid, step);
                    uint32_t height = interpolate_7(height_a, delta_height, step);
                    wr32(table + step,
                          low | (mid << 5) | (height << 11));
                }

                wr32(cache, (delta_low & 0x1ffffffu) + low_a |
                             (((delta_mid & 0x1ffffffu) + mid_a) << 5) |
                             (height_b << 11));
                wr32(cache + 1, color_a);
                wr32(cache + 2, color_b);
            }
            step_row >>= 1;
            pos = 1u;

            for (;;) {
                uint8_t **entry_glyph;
                uint8_t *glyph;
                uint32_t width;
                uint32_t height;

                if (character == 10u) {
                    row += advance_line;
                    column = column_initial;
                    character = rd8(param_2 + pos);
                    ++pos;
                    continue;
                }
                if (character == 0u)
                    return;

                entry_glyph = &ctx->glyph_mask[character];
                glyph = *entry_glyph;
                if (glyph == NULL) {
                    const video_out_font_image_t *source = ctx->image;
                    uint32_t index_source = source->glyph_row[character];
                    uint32_t scan_row;
                    uint32_t dest = 0u;

                    height = ctx->height;
                    width = ctx->width;
                    glyph = (uint8_t *)((fn_malloc)sym_libc_malloc)(
                        (size_t)(width * height));

                    if (height != 0u && width != 0u) {
                        for (scan_row = 0u; scan_row < height; ++scan_row) {
                            uint32_t bits = source->rows[index_source + scan_row];
                            uint32_t n;
                            uint32_t output_glyph = dest;

                            for (n = width; n != 0u; --n) {
                                wr8(glyph + output_glyph,
                                     (uint8_t)((bits >> 8) & 0x80u));
                                ++output_glyph;
                                bits <<= 1;
                            }
                            dest += width;
                        }
                    }

                    ctx->glyph_width[character] = width;
                    *entry_glyph = glyph;
                    height = ctx->height;
                    if (height == 0u) {
                        column += width;
                        character = rd8(param_2 + pos);
                        ++pos;
                        continue;
                    }
                } else {
                    width = ctx->glyph_width[character];
                    height = ctx->height;
                    if (height == 0u || width == 0u) {
                        column += width;
                        character = rd8(param_2 + pos);
                        ++pos;
                        continue;
                    }
                }

                if (width != 0u) {
                    uint32_t scan_row;
                    uint32_t origin = 0u;

                    for (scan_row = 0u; scan_row < height; ++scan_row) {
                        uint32_t idx = (row + scan_row) * step_row + column;
                        uint32_t n;
                        uint32_t origin_glyph = origin;

                        for (n = width; n != 0u; --n) {
                            uint8_t mask = rd8(glyph + origin_glyph);
                            uint32_t pixel = table[mask];
                            wr16(output + (size_t)idx * 2u, (uint16_t)pixel);
                            ++origin_glyph;
                            ++idx;
                        }
                        origin += width;
                    }
                }

                column += width;
                character = rd8(param_2 + pos);
                ++pos;
            }
        } else {
            uint32_t cache_a = color_32(color_a);
            uint32_t cache_b = color_32(color_b);

            if (cache[1] != cache_a || cache[2] != cache_b) {
                uint32_t low_a = (color_a & 0x1fu) << 3;
                uint32_t mid_a = (((color_a << 5) & 0xfc00u) >> 8);
                uint32_t height_a = (((color_a << 8) & 0xf80000u) >> 16);
                uint32_t low_b = (color_b & 0x1fu) << 3;
                uint32_t mid_b = (((color_b << 5) & 0xfc00u) >> 8);
                uint32_t height_b = (((color_b << 8) & 0xf80000u) >> 16);
                uint32_t delta_low = low_b - low_a;
                uint32_t delta_mid = mid_b - mid_a;
                uint32_t delta_height = height_b - height_a;
                uint32_t step;

                for (step = 0u; step != 128u; ++step) {
                    uint32_t low = interpolate_7(low_a, delta_low, step);
                    uint32_t mid = interpolate_7(mid_a, delta_mid, step);
                    uint32_t height = interpolate_7(height_a, delta_height, step);
                    wr32(table + step,
                          low | (mid << 8) | (height << 16));
                }

                wr32(cache, (delta_low & 0x1fffff8u) + low_a |
                             ((color_b << 5) & 0xfc00u) |
                             ((color_b << 8) & 0xf80000u));
                wr32(cache + 1, cache_a);
                wr32(cache + 2, cache_b);
            }
            step_row >>= 2;
            pos = 1u;

            for (;;) {
                uint8_t **entry_glyph;
                uint8_t *glyph;
                uint32_t width;
                uint32_t height;

                if (character == 10u) {
                    row += advance_line;
                    column = column_initial;
                    character = rd8(param_2 + pos);
                    ++pos;
                    continue;
                }
                if (character == 0u)
                    return;

                entry_glyph = &ctx->glyph_mask[character];
                glyph = *entry_glyph;
                if (glyph == NULL) {
                    const video_out_font_image_t *source = ctx->image;
                    uint32_t index_source = source->glyph_row[character];
                    uint32_t scan_row;
                    uint32_t dest = 0u;

                    height = ctx->height;
                    width = ctx->width;
                    glyph = (uint8_t *)((fn_malloc)sym_libc_malloc)(
                        (size_t)(width * height));

                    if (height != 0u && width != 0u) {
                        for (scan_row = 0u; scan_row < height; ++scan_row) {
                            uint32_t bits = source->rows[index_source + scan_row];
                            uint32_t n;
                            uint32_t output_glyph = dest;

                            for (n = width; n != 0u; --n) {
                                wr8(glyph + output_glyph,
                                     (uint8_t)((bits >> 8) & 0x80u));
                                ++output_glyph;
                                bits <<= 1;
                            }
                            dest += width;
                        }
                    }

                    ctx->glyph_width[character] = width;
                    *entry_glyph = glyph;
                    height = ctx->height;
                    if (height == 0u) {
                        column += width;
                        character = rd8(param_2 + pos);
                        ++pos;
                        continue;
                    }
                } else {
                    width = ctx->glyph_width[character];
                    height = ctx->height;
                    if (height == 0u || width == 0u) {
                        column += width;
                        character = rd8(param_2 + pos);
                        ++pos;
                        continue;
                    }
                }

                if (width != 0u) {
                    uint32_t scan_row;
                    uint32_t origin = 0u;

                    for (scan_row = 0u; scan_row < height; ++scan_row) {
                        uint32_t idx = (row + scan_row) * step_row + column;
                        uint32_t n;
                        uint32_t origin_glyph = origin;

                        for (n = width; n != 0u; --n) {
                            uint8_t mask = rd8(glyph + origin_glyph);
                            uint32_t pixel = table[mask];
                            wr32(output + (size_t)idx * 4u, pixel);
                            ++origin_glyph;
                            ++idx;
                        }
                        height = ctx->height;
                        origin += width;
                    }
                }

                column += width;
                character = rd8(param_2 + pos);
                ++pos;
            }
        }
    }
}
typedef void *(*fn_malloc_21)(uint64_t);



void *video_out_build_string_glyph_masks(video_out_font_t *ctx, uint8_t *list,
                          uint32_t limit) {

    void *result = ctx;
    uint8_t *current = list;
    uint32_t accumulated = 0;
    uint32_t offset = 0;
    uint8_t idx = rd8(current);

    while (idx != 0) {
        uint8_t **slot = &ctx->glyph_mask[idx];
        uint32_t width;

        if (*slot == 0) {
            const video_out_font_image_t *data = ctx->image;
            uint32_t height = ctx->height;
            width = ctx->width;
            uint32_t start = data->glyph_row[idx];
            uint32_t bytes = width * height;

            result = ((fn_malloc_21)sym_libc_malloc)((uint64_t)bytes);

            if (height != 0 && width != 0) {
                uint32_t output = 0;
                uint64_t row = 0;

                do {
                    uint32_t base = start + (uint32_t)row;
                    uint16_t word = data->rows[base];
                    uint32_t column = output;
                    uint64_t left = width;

                    do {
                        uint8_t bit = (uint8_t)((word >> 8) & 0x80u);
                        word = (uint16_t)(word << 1);
                        wr8((uint8_t *)result + column, bit);
                        column += 1u;
                    } while (--left != 0);

                    row += 1u;
                    output += width;
                } while (row != (uint64_t)height);
            }

            ctx->glyph_width[idx] = width;
            *slot = (uint8_t *)result;
        } else {
            width = ctx->glyph_width[idx];
        }

        accumulated += width;
        if (accumulated > limit) {
            wr8(current, 0);
            return result;
        }

        offset += 1u;
        current = list + offset;
        idx = rd8(current);
    }

    return result;
}
extern void        *video_out_screen_buffer_22(unsigned int screen) __asm__("video_out_screen_buffer");
extern int          video_out_screen_line_size_22(unsigned int screen) __asm__("video_out_screen_line_size");
extern uint32_t     video_out_get_screen_mode_22(uint32_t i) __asm__("video_out_get_screen_mode");
extern unsigned int video_out_pixel_size_22(void) __asm__("video_out_pixel_size");
typedef void *(*fn_memset)(void *, int, size_t);
#define ROWS      0xc0u
#define ROW_BYTES 0x200u
#define TOTAL_SIZE  0x18000u




static uint16_t a565(uint32_t px) {
    uint32_t green = (px >> 5) & 0x7e0u;
    uint32_t red  = (px >> 8) & 0xf800u;

    green = (green & ~0x1fu) | ((px >> 3) & 0x1fu);
    return (uint16_t)(green | red);
}

void video_out_dump_screen_rgb565(void *dest, unsigned int screen) {

    unsigned char *dst = (unsigned char *)dest;

    const unsigned char *src =
        (const unsigned char *)video_out_screen_buffer(screen);
    uint32_t line = (uint32_t)video_out_screen_line_size(screen);
    uint32_t mode  = video_out_get_screen_mode(screen);

    if (src == NULL) {
        static fn_memset s_memset;
        if (!s_memset) s_memset = (fn_memset)sym_libc_memset;
        s_memset(dst, 0, TOTAL_SIZE);
        return;
    }

    uint32_t step = mode + 1u;
    uint32_t row;

    if (video_out_pixel_size() == 2u) {
        uint32_t pal = line >> 1;
        for (row = 0; row != ROWS; row++) {
            uint32_t idx = 0;
            uint32_t off = 0;
            do {
                uint16_t v = rd16(src + (uint64_t)idx * 2u);
                idx += step;
                wr16(dst + off, v);
                off += 2u;
            } while (off != ROW_BYTES);
            src += (uint64_t)pal * 2u;
            dst += ROW_BYTES;
        }
        return;
    }

    uint32_t absp = ((int32_t)step >= 0) ? step : ~mode;
    uint64_t prod = (uint64_t)absp * 255u;
    int negative  = ((int32_t)step < 0);
    int has_high  = ((prod >> 32) != 0);
    int has_low  = ((uint32_t)prod != 0);
    int scalar   = (negative && has_low) || has_high;

    uint32_t inc2 = (mode << 1) + 2u;
    uint32_t pal4 = line >> 2;

    for (row = 0; row != ROWS; row++) {
        uint32_t idx = 0;
        uint32_t off = 0;

        if (scalar) {
            do {
                uint32_t px = rd32(src + (uint64_t)idx * 4u);
                idx += step;
                wr16(dst + off, a565(px));
                off += 2u;
            } while (off != ROW_BYTES);
        } else {
            do {

                uint32_t px0 = rd32(src + (uint64_t)idx * 4u);
                uint32_t idx1 = step + idx;
                uint32_t px1 = rd32(src + (uint64_t)idx1 * 4u);
                unsigned char *q = dst + off;
                off += 4u;
                idx += inc2;
                wr16(q, a565(px0));
                wr16(q + 2, a565(px1));
            } while (off != ROW_BYTES);
        }

        src += (uint64_t)pal4 * 4u;
        dst += ROW_BYTES;
    }
}
#undef ROWS
#undef ROW_BYTES
#undef TOTAL_SIZE


void video_out_fill_rect16(uint16_t color, uint32_t x_dest,
                        uint32_t row_dest, uint32_t width,
                        uint32_t height)
{

    uintptr_t dest = (uintptr_t)video_out_screen_buffer(0u);
    uint32_t step = video_out_screen_line_size(0u);

    if (height == 0 || width == 0)
        return;

    uint32_t pixels_per_row = step >> 1;
    uint32_t vertical = pixels_per_row * row_dest;
    dest += ((uint64_t)x_dest << 1) + ((uint64_t)vertical << 1);

    uint64_t jump = (uint64_t)(step >> 1) << 1;
    uint64_t blocks = (uint64_t)(width & UINT32_C(0xfffffff0));
    uint16_t pattern[16] = {
        color, color, color, color, color, color, color, color,
        color, color, color, color, color, color, color, color
    };
    uint32_t row = 0;

    do {
        uint64_t column = 0;

        if (width >= 16) {
            uint64_t left = blocks;
            uintptr_t output = dest;

            do {
                memcpy((void *)output, pattern, sizeof(pattern));
                left -= 16;
                output += 32;
            } while (left != 0);

            column = blocks;
        }

        while (column != width) {
            wr16((void *)(dest + (column << 1)), color);
            column++;
        }

        row++;
        dest += jump;
    } while (row != height);
}



void video_out_blit_rect16(void *origin_arg, uint32_t x_dest,
                        uint32_t row_dest, uint32_t width,
                        uint32_t height)
{

    uintptr_t origin = (uintptr_t)origin_arg;
    uintptr_t dest = (uintptr_t)video_out_screen_buffer(0u);
    uint32_t step_bytes = video_out_screen_line_size(0u);

    if (height == 0 || width == 0)
        return;

    uint32_t pixels_per_row = step_bytes >> 1;
    uint32_t vertical = pixels_per_row * row_dest;
    dest += ((uint64_t)x_dest << 1) + ((uint64_t)vertical << 1);

    uint64_t step_bytes_pair = (uint64_t)(step_bytes & 0xfffffffeu);
    uint64_t start_origin = 0;
    uint64_t row = 0;

    for (;;) {
        int fast = 0;

        if (width >= 16) {
            uint32_t product = (uint32_t)row * width;
            uint64_t last = (uint64_t)width - 1;

            if ((uint64_t)(~product) >= last) {
                uintptr_t origin_final = origin + ((uint64_t)width << 1) +
                                          ((uint64_t)product << 1);
                uintptr_t dest_row = dest;

                if (dest_row >= origin_final) {
                    fast = 1;
                } else {
                    uintptr_t dest_final = dest_row +
                                               ((uint64_t)width << 1);
                    uintptr_t origin_row = origin + ((uint64_t)product << 1);
                    if (dest_final <= origin_row)
                        fast = 1;
                }
            }
        }

        uint64_t column = 0;
        if (fast) {
            uint64_t block = (uint64_t)(width & 0xfffffff0u);
            uint32_t idx = (uint32_t)start_origin;
            uintptr_t output = dest + 16;

            while (block != 0) {
                memcpy((void *)(output - 16),
                       (const void *)(origin + ((uint64_t)idx << 1)), 32);
                block -= 16;
                idx += 16;
                output += 32;
            }
            column = (uint64_t)(width & 0xfffffff0u);
        }

        while (column != width) {
            uint32_t idx = (uint32_t)(start_origin + column);
            uint16_t pixel = rd16((const void *)(origin + ((uint64_t)idx << 1)));
            wr16((void *)(dest + (column << 1)), pixel);
            column++;
        }

        row++;
        start_origin += width;
        dest += step_bytes_pair;
        if ((uint32_t)row == height)
            return;
    }
}




void video_out_scale_blit(void *param_1, uint32_t param_2, uint32_t param_3,
                         uint32_t param_4, uint32_t param_5,
                         uint32_t param_6)
{

    uint8_t *source = (uint8_t *)param_1;
    uint8_t *root_dest = video_out_screen_buffer(0);
    uint32_t step = video_out_screen_line_size(0);
    uint32_t half_step = step >> 1;
    uint32_t product = (uint32_t)(half_step * param_3);
    uint8_t *dest = root_dest + ((uint64_t)product << 1) +
                        ((uint64_t)param_2 << 1);

    if ((uint32_t)(param_6 - 2) > 4) {
        uint32_t width = param_4;
        uint32_t height = param_5;
        uint64_t step_row = (uint64_t)half_step << 1;
        uint64_t pixels_block = (uint64_t)width & ~15ULL;
        uint64_t row;
        uint64_t index_source = 0;
        uint8_t *output = dest;

        if (height == 0 || width == 0)
            return;

        for (row = 0; row != height; ++row) {
            uint64_t pixel = 0;
            uint32_t start_row = (uint32_t)index_source;
            uint32_t product_row = (uint32_t)((uint32_t)row * width);
            int blocks = 0;

            if (width >= 16 && (uint32_t)~product_row >= width - 1) {
                uintptr_t begin_output = (uintptr_t)output;
                uintptr_t end_source = (uintptr_t)(source +
                    (((uint64_t)product_row << 1) + ((uint64_t)width << 1)));
                uintptr_t end_output = begin_output + ((uint64_t)width << 1);
                uintptr_t begin_source = (uintptr_t)(source +
                    ((uint64_t)product_row << 1));

                if (begin_output >= end_source || end_output <= begin_source)
                    blocks = 1;
            }

            if (blocks) {
                for (; pixel != pixels_block; pixel += 16) {
                    uint8_t copy[32];
                    uint32_t from = (uint32_t)(start_row + (uint32_t)pixel);
                    memcpy(copy, source + ((uint64_t)from << 1), sizeof(copy));
                    memcpy(output + (pixel << 1), copy, sizeof(copy));
                }
            }

            for (; pixel != width; ++pixel) {
                uint32_t from = (uint32_t)(start_row + (uint32_t)pixel);
                wr16(output + (pixel << 1),
                            rd16(source + ((uint64_t)from << 1)));
            }

            index_source += width;
            output += step_row;
        }
        return;
    }

    if (param_5 == 0 || param_4 == 0)
        return;

    if (param_6 == 2) {
        uint32_t row_source = 0;
        uint32_t row;
        uint32_t step_dest = step & 0xfffffffeU;

        for (row = 0; row != param_5; ++row) {
            uint64_t outside;
            uint32_t inside = row_source;

            for (outside = 0; outside != ((uint64_t)param_4 << 1); outside += 2) {
                uint16_t value = rd16(source + ((uint64_t)inside << 1));
                uint32_t a = (uint32_t)outside;
                uint32_t b = half_step + a;
                ++inside;
                wr16(dest + ((uint64_t)a << 1), value);
                wr16(dest + ((uint64_t)(a + 1) << 1), value);
                wr16(dest + ((uint64_t)b << 1), value);
                wr16(dest + ((uint64_t)(b + 1) << 1), value);
            }
            dest += (uint64_t)step_dest << 1;
            row_source += param_4;
        }
        return;
    }

    if (param_6 == 3) {
        uint32_t row_source = 0;
        uint32_t row;
        uint32_t step_dest = (uint32_t)(half_step * 3);

        for (row = 0; row != param_5; ++row) {
            uint64_t outside;
            uint32_t inside = row_source;

            for (outside = 0; outside != (uint64_t)param_4 * 3; outside += 3) {
                uint16_t value = rd16(source + ((uint64_t)inside << 1));
                uint32_t a = (uint32_t)outside;
                uint32_t b = half_step + a;
                uint32_t c = (uint32_t)(half_step * 2) + a;
                ++inside;
                wr16(dest + ((uint64_t)a << 1), value);
                wr16(dest + ((uint64_t)(a + 1) << 1), value);
                wr16(dest + ((uint64_t)(a + 2) << 1), value);
                wr16(dest + ((uint64_t)b << 1), value);
                wr16(dest + ((uint64_t)(b + 1) << 1), value);
                wr16(dest + ((uint64_t)(b + 2) << 1), value);
                wr16(dest + ((uint64_t)c << 1), value);
                wr16(dest + ((uint64_t)(c + 1) << 1), value);
                wr16(dest + ((uint64_t)(c + 2) << 1), value);
            }
            dest += (uint64_t)step_dest << 1;
            row_source += param_4;
        }
        return;
    }

    if (param_6 == 4) {
        uint32_t row_source = 0;
        uint32_t row;
        uint32_t step_dest = (uint32_t)(half_step * 4);
        uint32_t row2 = (uint32_t)(half_step * 2);
        uint32_t row3 = (uint32_t)(half_step * 3);

        for (row = 0; row != param_5; ++row) {
            uint64_t outside;
            uint32_t inside = row_source;

            for (outside = 0; outside != (uint64_t)param_4 * 4; outside += 4) {
                uint16_t value = rd16(source + ((uint64_t)inside << 1));
                uint64_t four = (uint64_t)value | ((uint64_t)value << 16) |
                                  ((uint64_t)value << 32) | ((uint64_t)value << 48);
                uint32_t a = (uint32_t)outside;
                uint32_t b = half_step + a;
                uint32_t c = row2 + a;
                uint32_t d = row3 + a;
                ++inside;
                wr64(dest + ((uint64_t)a << 1), four);
                wr16(dest + ((uint64_t)b << 1), value);
                wr16(dest + ((uint64_t)(b + 1) << 1), value);
                wr16(dest + ((uint64_t)(b + 2) << 1), value);
                wr16(dest + ((uint64_t)(b + 3) << 1), value);
                wr16(dest + ((uint64_t)c << 1), value);
                wr16(dest + ((uint64_t)(c + 1) << 1), value);
                wr16(dest + ((uint64_t)(c + 2) << 1), value);
                wr16(dest + ((uint64_t)(c + 3) << 1), value);
                wr16(dest + ((uint64_t)d << 1), value);
                wr16(dest + ((uint64_t)(d + 1) << 1), value);
                wr16(dest + ((uint64_t)(d + 2) << 1), value);
                wr16(dest + ((uint64_t)(d + 3) << 1), value);
            }
            dest += (uint64_t)step_dest << 1;
            row_source += param_4;
        }
        return;
    }

    if (param_6 == 5) {
        uint32_t row_source = 0;
        uint32_t row;
        uint32_t step_dest = (uint32_t)(half_step * 5);
        uint32_t row2 = (uint32_t)(half_step * 2);
        uint32_t row3 = (uint32_t)(half_step * 3);
        uint32_t row4 = (uint32_t)(half_step * 4);

        for (row = 0; row != param_5; ++row) {
            uint64_t outside;
            uint32_t inside = row_source;

            for (outside = 0; outside != (uint64_t)param_4 * 5; outside += 5) {
                uint16_t value = rd16(source + ((uint64_t)inside << 1));
                uint32_t a = (uint32_t)outside;
                uint32_t b = half_step + a;
                uint32_t c = row2 + a;
                uint32_t d = row3 + a;
                uint32_t e = row4 + a;
                ++inside;
                wr16(dest + ((uint64_t)a << 1), value);
                wr16(dest + ((uint64_t)(a + 1) << 1), value);
                wr16(dest + ((uint64_t)(a + 2) << 1), value);
                wr16(dest + ((uint64_t)(a + 3) << 1), value);
                wr16(dest + ((uint64_t)(a + 4) << 1), value);
                wr16(dest + ((uint64_t)b << 1), value);
                wr16(dest + ((uint64_t)(b + 1) << 1), value);
                wr16(dest + ((uint64_t)(b + 2) << 1), value);
                wr16(dest + ((uint64_t)(b + 3) << 1), value);
                wr16(dest + ((uint64_t)(b + 4) << 1), value);
                wr16(dest + ((uint64_t)c << 1), value);
                wr16(dest + ((uint64_t)(c + 1) << 1), value);
                wr16(dest + ((uint64_t)(c + 2) << 1), value);
                wr16(dest + ((uint64_t)(c + 3) << 1), value);
                wr16(dest + ((uint64_t)(c + 4) << 1), value);
                wr16(dest + ((uint64_t)d << 1), value);
                wr16(dest + ((uint64_t)(d + 1) << 1), value);
                wr16(dest + ((uint64_t)(d + 2) << 1), value);
                wr16(dest + ((uint64_t)(d + 3) << 1), value);
                wr16(dest + ((uint64_t)(d + 4) << 1), value);
                wr16(dest + ((uint64_t)e << 1), value);
                wr16(dest + ((uint64_t)(e + 1) << 1), value);
                wr16(dest + ((uint64_t)(e + 2) << 1), value);
                wr16(dest + ((uint64_t)(e + 3) << 1), value);
                wr16(dest + ((uint64_t)(e + 4) << 1), value);
            }
            dest += (uint64_t)step_dest << 1;
            row_source += param_4;
        }
        return;
    }

    {
        uint32_t row_source = 0;
        uint32_t row;
        uint32_t step_dest = (uint32_t)(half_step * 6);
        uint32_t row2 = (uint32_t)(half_step * 2);
        uint32_t row3 = (uint32_t)(half_step * 3);
        uint32_t row4 = (uint32_t)(half_step * 4);
        uint32_t row5 = (uint32_t)(half_step * 5);

        for (row = 0; row != param_5; ++row) {
            uint64_t outside;
            uint32_t inside = row_source;

            for (outside = 0; outside != (uint64_t)param_4 * 6; outside += 6) {
                uint16_t value = rd16(source + ((uint64_t)inside << 1));
                uint32_t a = (uint32_t)outside;
                uint32_t b = half_step + a;
                uint32_t c = row2 + a;
                uint32_t d = row3 + a;
                uint32_t e = row4 + a;
                uint32_t f = row5 + a;
                ++inside;
                for (uint32_t k = 0; k != 6; ++k)
                    wr16(dest + ((uint64_t)(a + k) << 1), value);
                for (uint32_t k = 0; k != 6; ++k)
                    wr16(dest + ((uint64_t)(b + k) << 1), value);
                for (uint32_t k = 0; k != 6; ++k)
                    wr16(dest + ((uint64_t)(c + k) << 1), value);
                for (uint32_t k = 0; k != 6; ++k)
                    wr16(dest + ((uint64_t)(d + k) << 1), value);
                for (uint32_t k = 0; k != 6; ++k)
                    wr16(dest + ((uint64_t)(e + k) << 1), value);
                for (uint32_t k = 0; k != 6; ++k)
                    wr16(dest + ((uint64_t)(f + k) << 1), value);
            }
            dest += (uint64_t)step_dest << 1;
            row_source += param_4;
        }
    }
}
typedef void *(*fn_memset_26)(void *, int, size_t);

void *video_out_init_glyph_cache(video_out_font_t *param_1, const video_out_font_image_t *param_2)
{

    param_1->image = param_2;

    memcpy(&param_1->width, param_2, sizeof param_1->width + sizeof param_1->height);

    param_1->initialised = 1;

    static fn_memset_26 core_memset;
    if (!core_memset)
        core_memset = (fn_memset_26)sym_libc_memset;
    return core_memset(param_1->glyph_mask, 0, sizeof param_1->glyph_mask);
}
typedef void *(*fn_memset_27)(void *, int, size_t);

void video_out_init_global_glyph_cache(void)
{

    video_out_font_t *obj = VIDEO_OUT_FONT;
    const video_out_font_image_t *ext = VIDEO_OUT_FONT_IMAGE;

    obj->image = ext;

    memcpy(&obj->width, ext, sizeof obj->width + sizeof obj->height);

    obj->initialised = 1;

    static fn_memset_27 core_memset;
    if (!core_memset)
        core_memset = (fn_memset_27)sym_libc_memset;

    core_memset(obj->glyph_mask, 0, sizeof obj->glyph_mask);

    core_memset(obj->blend_table, 0, sizeof obj->blend_table + sizeof obj->blend_key + sizeof obj->blend_color_a + sizeof obj->blend_color_b);
}

void *video_out_clear_large_buffer(unsigned char *obj) {
    *(uint64_t *)&((spu_t *)obj)->output_read = 0;
    return memset(obj, 0, 131072);
}

#undef OFF_G
#undef SCREEN_BYTES
#undef AXIS_OFF
#undef MODE_OFF
#undef X_FACTOR_OFF
#undef Y_FACTOR_OFF
#undef X_RECIP_OFF
#undef Y_RECIP_OFF
#undef READY_OFF
#undef ROWS
#undef ROW_BYTES
#undef TOTAL_SIZE
#undef OFF_PTR2
#undef OFF_BUF
#undef BUF_LEN

unsigned recon_present_mode;
unsigned recon_present_flips;
unsigned recon_present_seen;

void recon_present_set_mode(unsigned mode)
{
    recon_present_mode = mode;
}
void *(*recon_pages_reserve)(unsigned long size);
int recon_present_nice_emu = -16;

void recon_present_set_nice_emu(int nice) { recon_present_nice_emu = nice; }
__thread unsigned recon_present_fbo_dest;

void recon_present_set_fbo_dest(unsigned fbo) { recon_present_fbo_dest = fbo; }
#include <string.h>
#include "blob_symbols.h"

unsigned recon_page_list(void)
{
    uint32_t sel;
    if (recon_page_latched <= 1u) return recon_page_latched;
    nds_platform_default()->threads.mutex_lock(VIDEO_OUT_GL->mutex);
    sel = VIDEO_OUT_GL->page_parity;
    nds_platform_default()->threads.mutex_unlock(VIDEO_OUT_GL->mutex);
    return (~sel) & 1u;
}

video_out_font_t video_out_font_state;

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/nds.h"
#include "core/nds_state.h"
#include "frontend/frontend.h"
#include "core_internals.h"
#include "mem_access.h"

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#define SCREEN_PIXELS (SCREEN_WIDTH * SCREEN_HEIGHT)
#define SCREEN_RGBA_BYTES (SCREEN_PIXELS * 4)
#define SCREEN_RGB16_BYTES (SCREEN_PIXELS * 2)
#define SCREEN_ROW_RGBA 0x400
#define SCREEN_ROW_16 0x800
#define SCREEN_ROW_32 0x1000
#define SCREEN_STEP_16 4
#define SCREEN_STEP_32 8
#define SCREEN_MODE_WIDE 2
#define PIXEL_OPAQUE 0xff000000u
#define HUD_SCALE 16.0
#define HUD_LIMIT 0xffffu
#define HUD_SHIFT 16


static uint32_t from_rgb565(uint32_t s)
{
    return ((s & 0x1fu) << 3) | ((s & 0x7e0u) << 5) | ((s & 0xf800u) << 8) | PIXEL_OPAQUE;
}

static uint32_t from_bgra(uint32_t v)
{
    return (v & 0xff00u) | (v << 16) | ((v >> 16) & 0xffu) | PIXEL_OPAQUE;
}

static int screens_are_16bit(const frontend_t *g)
{
    return (g->config_bits & FRONTEND_CONFIG_SCREENSHOT_SWAP_RB) != 0;
}

static int screens_are_wide(const frontend_t *g)
{
    return ((g->config_bits >> FRONTEND_CONFIG_SCREEN_MODE_SHIFT) & SCREEN_MODE_WIDE) != 0;
}

void nds_screen_buffers(nds_t *nds, const void **top, const void **bottom)
{
    (void)nds;
    *top = video_out_screen_buffer(0);
    *bottom = video_out_screen_buffer(1);
}

int nds_screens_rgba(nds_t *nds, void *top, void *bottom)
{
    frontend_t *g = FRONTEND;
    const uint8_t *s0;
    const uint8_t *s1;
    uint8_t *d0 = top;
    uint8_t *d1 = bottom;
    uint32_t x;
    uint32_t y;
    uint32_t i;

    (void)nds;

    s0 = video_out_screen_buffer(0);
    s1 = video_out_screen_buffer(1);

    if (screens_are_16bit(g)) {
        if (screens_are_wide(g)) {
            for (y = 0; y < SCREEN_HEIGHT; y++) {
                const uint8_t *a = s0 + (size_t)y * SCREEN_ROW_16;
                const uint8_t *b = s1 + (size_t)y * SCREEN_ROW_16;
                uint8_t *p = d0 + (size_t)y * SCREEN_ROW_RGBA;
                uint8_t *q = d1 + (size_t)y * SCREEN_ROW_RGBA;

                for (x = 0; x < SCREEN_WIDTH; x++) {
                    wr32(p + (size_t)x * 4, from_rgb565(rd16(a + (size_t)x * SCREEN_STEP_16)));
                    wr32(q + (size_t)x * 4, from_rgb565(rd16(b + (size_t)x * SCREEN_STEP_16)));
                }
            }
        } else {
            for (i = 0; i < SCREEN_PIXELS; i++) {
                wr32(d0 + (size_t)i * 4, from_rgb565(rd16(s0 + (size_t)i * 2)));
                wr32(d1 + (size_t)i * 4, from_rgb565(rd16(s1 + (size_t)i * 2)));
            }
        }
        return 1;
    }

    if (s0 == NULL || s1 == NULL)
        return 0;

    if (screens_are_wide(g)) {
        for (y = 0; y < SCREEN_HEIGHT; y++) {
            const uint8_t *a = s0 + (size_t)y * SCREEN_ROW_32;
            const uint8_t *b = s1 + (size_t)y * SCREEN_ROW_32;
            uint8_t *p = d0 + (size_t)y * SCREEN_ROW_RGBA;
            uint8_t *q = d1 + (size_t)y * SCREEN_ROW_RGBA;

            for (x = 0; x < SCREEN_WIDTH; x++) {
                wr32(p + (size_t)x * 4, from_bgra(rd32(a + (size_t)x * SCREEN_STEP_32)));
                wr32(q + (size_t)x * 4, from_bgra(rd32(b + (size_t)x * SCREEN_STEP_32)));
            }
        }
    } else {
        for (i = 0; i < SCREEN_PIXELS; i++) {
            wr32(d0 + (size_t)i * 4, from_bgra(rd32(s0 + (size_t)i * 4)));
            wr32(d1 + (size_t)i * 4, from_bgra(rd32(s1 + (size_t)i * 4)));
        }
    }
    return 1;
}

int nds_screenshot(nds_t *nds, void *rgba, size_t size)
{
    frontend_t *g = FRONTEND;
    const uint8_t *s0;
    const uint8_t *s1;
    uint8_t *out = rgba;
    size_t i;

    (void)nds;

    if (size < (size_t)SCREEN_RGBA_BYTES * 2)
        return 0;

    s0 = video_out_screen_buffer(0);
    s1 = video_out_screen_buffer(1);

    if (screens_are_16bit(g)) {
        for (i = 0; i < SCREEN_RGB16_BYTES; i += 2) {
            wr32(out + i * 2, from_rgb565(rd16(s0 + i)));
            wr32(out + i * 2 + SCREEN_RGBA_BYTES, from_rgb565(rd16(s1 + i)));
        }
        return 1;
    }

    if (s0 == NULL || s1 == NULL)
        return 0;

    for (i = 0; i < SCREEN_RGBA_BYTES; i += 4) {
        wr32(out + i, from_bgra(rd32(s0 + i)));
        wr32(out + i + SCREEN_RGBA_BYTES, from_bgra(rd32(s1 + i)));
    }
    return 1;
}

int nds_snapshot_slot(nds_t *nds, int slot, void *top16, void *bottom16)
{
    (void)nds;
    return state_load_slot((unsigned char *)(FRONTEND->machine), (uint32_t)slot,
                                                top16, bottom16, 1);
}

int nds_snapshot_file(nds_t *nds, const char *path, void *top16, void *bottom16)
{
    (void)nds;
    return state_load_file((unsigned char *)(FRONTEND->machine), path,
                                                top16, bottom16, 1);
}

static uint32_t hud_word(float value)
{
    double scaled = (double)value * HUD_SCALE;

    if (scaled != scaled || scaled < 0.0)
        return 0;
    if (scaled >= 4294967296.0)
        return UINT32_MAX;
    return (uint32_t)scaled;
}

uint32_t nds_perf_word(nds_t *nds)
{
    frontend_t *g = FRONTEND;
    const gpu_capture_t *capture;
    uint32_t speed;
    uint32_t composed;

    (void)nds;

    if (g->frame_state != 0)
        return UINT32_MAX;

    capture = &g->machine->output.capture;
    speed = hud_word(capture->hud_fps);
    composed = hud_word(capture->hud_percent);

    if (speed > HUD_LIMIT)
        speed = HUD_LIMIT;
    if (composed > HUD_LIMIT)
        composed = HUD_LIMIT;

    return composed | (speed << HUD_SHIFT);
}

void nds_clear_screens(nds_t *nds, uint32_t texture_top, uint32_t texture_bottom)
{
    (void)nds;
    video_out_gl_clear_screens(texture_top, texture_bottom);
}

void nds_render_frame(nds_t *nds, uint32_t first, uint32_t second, int flag)
{
    (void)nds;
    video_out_upload_screen_textures(first, second, (uint32_t)(flag & 0xff));
}

void nds_draw_screen(nds_t *nds, uint32_t texture, uint32_t index)
{
    (void)nds;
    video_out_gl_draw_screen_rect4(texture, index);
}

void nds_draw_screen_ext(nds_t *nds, uint32_t texture, uint32_t index)
{
    (void)nds;
    video_out_gl_draw_screen_rect2(texture, index);
}

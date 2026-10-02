#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <string.h>
#include <stddef.h>
#include "core_internals.h"
#include "mem_access.h"


void gpu2d_engine_reset_bg_cache_ptrs(gpu2d_engine_t *obj) {

    uint32_t mode = obj->dispcnt & 7;
    obj->bg[2].direct_ptr = 0;
    obj->bg[3].direct_ptr = 0;

    if ((uint32_t)(mode - 3) < 2) {
        gpu2d_bg_text_cache_direct_ptr(obj, 3);
        return;
    }
    if (mode != 5) return;

    gpu2d_bg_text_cache_direct_ptr(obj, 2);
    gpu2d_bg_text_cache_direct_ptr(obj, 3);
}

extern void gpu2d_engine_select_bg_handlers_1(unsigned char *o) __asm__("gpu2d_engine_select_bg_handlers");






void gpu2d_engine_write_bgcnt(gpu2d_engine_t *ctx, uint32_t layer, uint32_t value) {

    gpu2d_bg_t *bg = &ctx->bg[layer];

    uint32_t field_a = (value << 12) & 0x3c000u;
    uint32_t high5   = (value >> 8) & 0x1fu;
    uint32_t field_b = high5 << 11;

    bg->char_offset = field_a;
    bg->screen_offset = field_b;

    uint32_t g1 = ctx->screen_base;
    uint32_t g2 = ctx->char_base;
    uint32_t v1 = g2 + field_a;
    uint32_t v2 = g1 + field_b;

    bg->char_base = v1;
    bg->screen_base = v2;

    uint32_t old = bg->bgcnt;
    bg->bgcnt = (uint16_t)value;

    if (ctx->index == 1) {
        v1 &= 0xffe1ffffu;
        v2 &= 0xffe1ffffu;
        bg->char_base = v1;
        bg->screen_base = v2;
    }

    uint32_t change = old ^ value;
    if ((change & 3) != 0)
        gpu2d_compose_build_layer_order(ctx);

    if (layer <= 1) {

        uint32_t idx = ((value >> 12) & 2u) + layer;
        bg->ext_palette = ctx->bg_ext_palette[idx];
        return;
    }

    if ((ctx->dispcnt & 7u) == 6) {

        bg->affine_screen_offset = 0;
        uint32_t a, b, c;
        if (bg->bgcnt & 0x4000) { a = 1023; b = 511;  c = 10; }
        else                 { a = 511;  b = 1023; c = 9;  }
        bg->mask_x = (uint16_t)a;
        bg->mask_y = (uint16_t)b;
        bg->width_shift = (uint8_t)c;
        return;
    }

    uint32_t sel = (value >> 14) & 3u;
    uint32_t size = 128u << sel;
    uint32_t field_c = high5 << 14;

    bg->size_code = (uint8_t)(4u | sel);
    bg->tiles_per_row_minus_one = (uint8_t)((size >> 3) - 1);
    bg->affine_screen_offset = field_c;

    if (ctx->index == 1)
        bg->affine_screen_offset = field_c | 0x200000u;

    if ((change & 0x84u) != 0)
        gpu2d_engine_select_bg_handlers(ctx);

    if (size > 0x100u) {
        bg->mask_x = 511;
        bg->mask_y = (uint16_t)((size >> 1) - 1);
        bg->width_shift = 9;
    } else {
        bg->mask_x = (uint16_t)(size - 1);
        bg->mask_y = (uint16_t)(size - 1);
        bg->width_shift = (uint8_t)(sel + 7);
    }
}

void gpu2d_engine_select_bg_handlers(gpu2d_engine_t *o) {

    gpu2d_bg_line_fn A  = gpu2d_bg_text_draw_tile_line;
    gpu2d_bg_line_fn C  = gpu2d_bg_affine_draw_line;
    gpu2d_bg_line_fn D  = gpu2d_bg_affine_draw_line_dual_style;
    gpu2d_bg_line_fn E1 = gpu2d_bg_text_identity;
    gpu2d_bg_line_fn E2 = gpu2d_bg_text_decode_indexed_line;
    gpu2d_bg_line_fn E3 = gpu2d_bg_affine_sample_bitmap_affine_line;

    uint32_t mode = o->dispcnt & 7;
    o->bg[0].line_handler = A;
    o->bg[1].line_handler = A;

    switch (mode) {
    case 0:
        o->bg[2].line_handler = A;
        o->bg[3].line_handler = A;
        return;

    case 1:
        o->bg[2].line_handler = A;
        o->bg[3].line_handler = C;
        return;

    case 2:
        o->bg[2].line_handler = C;
        o->bg[3].line_handler = C;
        return;

    case 3: {
        uint16_t f = o->bg[3].bgcnt;
        o->bg[2].line_handler = A;
        if ((f & 0x80) == 0)      o->bg[3].line_handler = D;
        else if (f & 4)           o->bg[3].line_handler = E3;
        else                      o->bg[3].line_handler = E2;
        return;
    }

    case 4: {
        uint16_t f = o->bg[3].bgcnt;
        o->bg[2].line_handler = C;
        if ((f & 0x80) == 0)      o->bg[3].line_handler = D;
        else if ((f & 4) == 0)    o->bg[3].line_handler = E2;
        else                      o->bg[3].line_handler = E3;
        return;
    }

    case 5: {
        uint16_t g = o->bg[2].bgcnt;
        if ((g & 0x80) == 0)
            o->bg[2].line_handler = D;
        else if ((g & 4) == 0)
            o->bg[2].line_handler = E2;
        else
            o->bg[2].line_handler = E3;

        uint16_t f = o->bg[3].bgcnt;
        if ((f & 0x80) == 0)      o->bg[3].line_handler = D;
        else if (f & 4)           o->bg[3].line_handler = E3;
        else                      o->bg[3].line_handler = E2;
        return;
    }

    case 6:
        o->bg[1].line_handler = E1;
        o->bg[2].line_handler = E2;
        o->bg[3].line_handler = E1;
        return;

    case 7:
        return;
    }
}

#define NO_MASK 0xc0b1fff7u
#define WARN_MASK 0x1f08u



void gpu2d_engine_write_dispcnt(gpu2d_engine_t *obj, unsigned value) {

    uint32_t old = obj->dispcnt;
    uint8_t bit30 = (uint8_t)((value >> 30) & 1);

    obj->bg[0].ext_palette_enabled = bit30;
    obj->bg[1].ext_palette_enabled = bit30;
    obj->bg[2].ext_palette_enabled = bit30;
    obj->bg[3].ext_palette_enabled = bit30;

    if (obj->index != 0) {
        value &= NO_MASK;
    } else {
        uint32_t a = (value >> 11) & 0x70000u;
        uint32_t b = (value >> 8)  & 0x70000u;
        unsigned idx = (value >> 18) & 3u;

        uint8_t *p = obj->gpu->vram.bank_data[idx];

        obj->screen_base = a;
        obj->char_base = b;

        obj->bg[0].screen_base = obj->bg[0].screen_offset + a;
        obj->bg[0].char_base = obj->bg[0].char_offset + b;
        obj->bg[1].screen_base = obj->bg[1].screen_offset + a;
        obj->bg[1].char_base = obj->bg[1].char_offset + b;
        obj->bg[2].screen_base = obj->bg[2].screen_offset + a;
        obj->bg[2].char_base = obj->bg[2].char_offset + b;
        obj->bg[3].screen_base = obj->bg[3].screen_offset + a;
        obj->bg[3].char_base = obj->bg[3].char_offset + b;

        obj->display_vram_bank = p;
    }

    unsigned mode_updated = value  & 7;
    unsigned mode_old = old  & 7;
    obj->dispcnt = value;

    if (mode_updated != mode_old) {
        gpu2d_engine_select_bg_handlers(obj);
        if (mode_old == 6 || mode_updated == 6) {
            uint16_t field;
            memcpy(&field, &obj->bg[2].bgcnt, 2);
            gpu2d_engine_write_bgcnt(obj, 2, field);
        }
    }

    if (((value ^ old) & WARN_MASK) != 0)
        gpu2d_compose_build_layer_order(obj);
}
#undef NO_MASK
#undef WARN_MASK

extern void *video_out_screen_buffer(unsigned int screen);
extern int   video_out_screen_line_size(unsigned int screen);




void gpu2d_engine_setup_screen_buffer(gpu2d_engine_t *p1, unsigned int param_2) {

    unsigned int screen = p1->index;

    if ((param_2 & 0x80u) == 0) {
        uint8_t *p_b = (uint8_t *)p1->gpu->vram.bus->machine;
        int32_t  val = (int32_t)((nds_t *)p_b)->config.fix_main_screen;
        unsigned int is_zero = (val == 0) ? 1u : 0u;
        screen = is_zero ^ screen;
    }

    void *fb = video_out_screen_buffer(screen);

    uint8_t flag_null = p1->no_framebuffer;
    p1->framebuffer = (flag_null == 0) ? (uint8_t *)fb : 0;

    int line = video_out_screen_line_size(screen);
    p1->framebuffer_line_size = (uint32_t)line;
}

extern void  gpu2d_engine_write_dispcnt_5(uint8_t *obj, unsigned value) __asm__("gpu2d_engine_write_dispcnt");
extern void  gpu2d_engine_write_bgcnt_5(uint8_t *ctx, uint32_t layer, uint32_t v) __asm__("gpu2d_engine_write_bgcnt");









static uint32_t blend(uint32_t old, uint32_t val, unsigned shift, uint32_t size)
{
    uint32_t mask = ~(uint32_t)(0xffffffffu << ((size * 8u) & 31u));
    return (old & ~(mask << shift)) | (val << shift);
}

static unsigned shift32(uint32_t dir) { return (unsigned)(dir & 3u) * 8u; }

static unsigned shift16(uint32_t dir) { return (unsigned)(dir & 1u) * 8u; }

static uint32_t sext28(uint32_t v)
{
    return (uint32_t)(((v & 0x0fffffffu) ^ 0x08000000u) - 0x08000000u);
}

void gpu2d_engine_write_register(gpu2d_engine_t *obj, const void *request)
{

    const uint8_t *req = (const uint8_t *)request;
    uint32_t dir = rd32(req + 0);
    uint32_t val = rd32(req + 4);
    uint32_t size = req[9];

    if ((dir >> 20) != 0) {
        uint8_t *mem;
        uint64_t idx;
        if ((dir >> 21) != 0) {
            mem = obj->oam;
            idx = dir & 0x3ffu;
            obj->oam_dirty = 1;
        } else {
            mem = obj->palette;
            idx = dir & 0x3ffu;
        }
        if (size == 4)      wr32(mem + idx, val);
        else if (size == 2) wr16(mem + idx, (uint16_t)val);
        else if (size == 1) mem[idx] = (uint8_t)val;
        return;
    }

    {
        uint32_t kind = dir & 0xfffu;
        if (kind > 0x305u) return;

        switch (kind) {

        case 0x00: case 0x01: case 0x02: case 0x03:
            if (size <= 3) {
                val = blend(obj->dispcnt, val, shift32(dir), size);
            }

            gpu2d_engine_write_dispcnt(obj, val);
            return;

        case 0x08: case 0x09: {
            uint32_t b148 = obj->screen_base;
            uint32_t b152 = obj->char_base;
            uint32_t car  = (val << 12) & 0x3c000u;
            uint32_t pan;
            uint32_t sup, inf;
            uint8_t  engine;
            uint16_t old;

            obj->bg[0].char_offset = car;
            sup = b152 + car;
            engine = obj->index;
            pan = (val << 3) & 0xf800u;
            obj->bg[0].screen_offset = pan;
            inf = b148 + pan;
            old = obj->bg[0].bgcnt;
            obj->bg[0].screen_base = inf;
            obj->bg[0].char_base = sup;
            obj->bg[0].bgcnt = (uint16_t)val;
            if (engine == 1) {
                sup &= 0xffe1ffffu;
                inf &= 0xffe1ffffu;
                obj->bg[0].screen_base = inf;
                obj->bg[0].char_base = sup;
            }
            if (((val ^ (uint32_t)old) & 3u) != 0) {
                gpu2d_compose_build_layer_order(obj);
            }
            obj->bg[0].ext_palette = obj->bg_ext_palette[(val >> 12) & 2u];
            if (size < 2 || size == 2) return;
            val >>= 16;
        }

        goto case_0a;

        case_0a:
        case 0x0a: case 0x0b: {
            uint32_t b148 = obj->screen_base;
            uint32_t b152 = obj->char_base;
            uint32_t car  = (val << 12) & 0x3c000u;
            uint32_t pan;
            uint32_t sup, inf;
            uint8_t  engine;
            uint16_t old;

            obj->bg[1].char_offset = car;
            sup = b152 + car;
            engine = obj->index;
            pan = (val << 3) & 0xf800u;
            obj->bg[1].screen_offset = pan;
            inf = b148 + pan;
            old = obj->bg[1].bgcnt;
            obj->bg[1].char_base = sup;
            obj->bg[1].screen_base = inf;
            obj->bg[1].bgcnt = (uint16_t)val;
            if (engine == 1) {
                sup &= 0xffe1ffffu;
                inf &= 0xffe1ffffu;
                obj->bg[1].char_base = sup;
                obj->bg[1].screen_base = inf;
            }
            if (((val ^ (uint32_t)old) & 3u) != 0) {
                gpu2d_compose_build_layer_order(obj);
            }
            obj->bg[1].ext_palette = obj->bg_ext_palette[((val >> 12) & 2u) | 1u];
            return;
        }

        case 0x0c: case 0x0d:
            gpu2d_engine_write_bgcnt(obj, 2, val);
            if (size < 2 || size == 2) return;
            val >>= 16;
            goto case_0e;

        case_0e:
        case 0x0e: case 0x0f:

            gpu2d_engine_write_bgcnt(obj, 3, val);
            return;

        case 0x10: case 0x11: {
            int height;
            if (size <= 1) val = blend(obj->bg[0].hofs, val, shift16(dir), size);
            height = (size >= 3);
            if (height) size = 2;
            obj->bg[0].hofs = (uint16_t)(val & 0x1ffu);
            if (!height) return;
            val >>= 16;
        }
        goto case_12;

        case_12:
        case 0x12: case 0x13:
            if (size <= 1) val = blend(obj->bg[0].vofs, val, shift16(dir), size);
            obj->bg[0].vofs = (uint16_t)(val & 0x1ffu);
            return;

        case 0x14: case 0x15: {
            int height;
            if (size <= 1) val = blend(obj->bg[1].hofs, val, shift16(dir), size);
            height = (size >= 3);
            if (height) size = 2;
            obj->bg[1].hofs = (uint16_t)(val & 0x1ffu);
            if (!height) return;
            val >>= 16;
        }
        goto case_16;

        case_16:
        case 0x16: case 0x17:
            if (size <= 1) val = blend(obj->bg[1].vofs, val, shift16(dir), size);
            obj->bg[1].vofs = (uint16_t)(val & 0x1ffu);
            return;

        case 0x18: case 0x19: {
            int height;
            if (size <= 1) val = blend(obj->bg[2].hofs, val, shift16(dir), size);
            height = (size >= 3);
            if (height) size = 2;
            obj->bg[2].hofs = (uint16_t)(val & 0x1ffu);
            if (!height) return;
            val >>= 16;
        }
        goto case_1a;

        case_1a:
        case 0x1a: case 0x1b:
            if (size <= 1) val = blend(obj->bg[2].vofs, val, shift16(dir), size);
            obj->bg[2].vofs = (uint16_t)(val & 0x1ffu);
            return;

        case 0x1c: case 0x1d: {
            int height;
            if (size <= 1) val = blend(obj->bg[3].hofs, val, shift16(dir), size);
            height = (size >= 3);
            if (height) size = 2;
            obj->bg[3].hofs = (uint16_t)(val & 0x1ffu);
            if (!height) return;
            val >>= 16;
        }
        goto case_1e;

        case_1e:
        case 0x1e: case 0x1f:
            if (size <= 1) val = blend(obj->bg[3].vofs, val, shift16(dir), size);
            obj->bg[3].vofs = (uint16_t)(val & 0x1ffu);
            return;

        case 0x20: case 0x21: {
            int height;
            if (size <= 1) {
                val = blend((uint32_t)(int32_t)obj->bg[2].pa,
                             val, shift16(dir), size);
            }
            height = (size >= 3);
            if (height) size = 2;
            obj->bg[2].pa = (uint16_t)val;
            obj->bg[2].affine_dirty = 1;
            if (!height) return;
            val >>= 16;
        }
        goto case_22;

        case_22:
        case 0x22: case 0x23:
            if (size <= 1) {
                val = blend((uint32_t)(int32_t)obj->bg[2].pb,
                             val, shift16(dir), size);
            }
            obj->bg[2].affine_dirty = 1;
            obj->bg[2].pb = (uint16_t)val;
            return;

        case 0x24: case 0x25: {
            int height;
            if (size <= 1) {
                val = blend((uint32_t)(int32_t)obj->bg[2].pc,
                             val, shift16(dir), size);
            }
            height = (size >= 3);
            if (height) size = 2;
            obj->bg[2].pc = (uint16_t)val;
            obj->bg[2].affine_dirty = 1;
            if (!height) return;
            val >>= 16;
        }
        goto case_26;

        case_26:
        case 0x26: case 0x27:
            if (size <= 1) {
                val = blend((uint32_t)(int32_t)obj->bg[2].pd,
                             val, shift16(dir), size);
            }
            obj->bg[2].affine_dirty = 1;
            obj->bg[2].pd = (uint16_t)val;
            return;

        case 0x30: case 0x31: {
            int height;
            if (size <= 1) {
                val = blend((uint32_t)(int32_t)obj->bg[3].pa,
                             val, shift16(dir), size);
            }
            height = (size >= 3);
            if (height) size = 2;
            obj->bg[3].pa = (uint16_t)val;
            obj->bg[3].affine_dirty = 1;
            if (!height) return;
            val >>= 16;
        }
        goto case_32;

        case_32:
        case 0x32: case 0x33:
            if (size <= 1) {
                val = blend((uint32_t)(int32_t)obj->bg[3].pb,
                             val, shift16(dir), size);
            }
            obj->bg[3].affine_dirty = 1;
            obj->bg[3].pb = (uint16_t)val;
            return;

        case 0x34: case 0x35: {
            int height;
            if (size <= 1) {
                val = blend((uint32_t)(int32_t)obj->bg[3].pc,
                             val, shift16(dir), size);
            }
            height = (size >= 3);
            if (height) size = 2;
            obj->bg[3].pc = (uint16_t)val;
            obj->bg[3].affine_dirty = 1;
            if (!height) return;
            val >>= 16;
        }
        goto case_36;

        case_36:
        case 0x36: case 0x37:
            if (size <= 1) {
                val = blend((uint32_t)(int32_t)obj->bg[3].pd,
                             val, shift16(dir), size);
            }
            obj->bg[3].affine_dirty = 1;
            obj->bg[3].pd = (uint16_t)val;
            return;

        case 0x28: case 0x29: case 0x2a: case 0x2b: {
            uint32_t new = sext28(val);
            if (size <= 3) {
                new = blend(obj->bg[2].ref_x, new, shift32(dir), size);
            }
            obj->bg[2].ref_x = new;
            obj->bg[2].affine_dirty = 1;
            obj->bg[2].current_x = new;
            return;
        }

        case 0x2c: case 0x2d: case 0x2e: case 0x2f: {
            uint32_t new = sext28(val);
            if (size <= 3) {
                new = blend(obj->bg[2].ref_y, new, shift32(dir), size);
            }
            obj->bg[2].ref_y = new;
            obj->bg[2].affine_dirty = 1;
            obj->bg[2].current_y = new;
            return;
        }

        case 0x38: case 0x39: case 0x3a: case 0x3b: {
            uint32_t new = sext28(val);
            if (size <= 3) {
                new = blend(obj->bg[3].ref_x, new, shift32(dir), size);
            }
            obj->bg[3].ref_x = new;
            obj->bg[3].affine_dirty = 1;
            obj->bg[3].current_x = new;
            return;
        }

        case 0x3c: case 0x3d: case 0x3e: case 0x3f: {
            uint32_t new = sext28(val);
            if (size <= 3) {
                new = blend(obj->bg[3].ref_y, new, shift32(dir), size);
            }
            obj->bg[3].ref_y = new;
            obj->bg[3].affine_dirty = 1;
            obj->bg[3].current_y = new;
            return;
        }

        case 0x40: case 0x41: {
            uint8_t flags;
            if (size <= 1) val = blend(obj->win0h, val, shift16(dir), size);
            flags = obj->window_dirty;
            obj->win0h = (uint16_t)val;
            obj->window_dirty = (uint8_t)(flags | 1u);
            if (size < 3) return;
            val >>= 16;
        }
        goto tail_172;

        case 0x42: case 0x43:
            if (size <= 1) val = blend(obj->win1h, val, shift16(dir), size);
            goto tail_172;

        tail_172: {
            uint8_t flags = obj->window_dirty;
            obj->win1h = (uint16_t)val;
            obj->window_dirty = (uint8_t)(flags | 2u);
            return;
        }

        case 0x44: case 0x45:
            if (size <= 1) {
                uint32_t v = blend(obj->win0v, val, shift16(dir), size);
                obj->win0v = (uint16_t)v;
                return;
            }
            obj->win0v = (uint16_t)val;
            if (size == 2) return;
            val >>= 16;
            goto tail_176;

        case 0x46: case 0x47:
            if (size <= 1) val = blend(obj->win1v, val, shift16(dir), size);
            goto tail_176;

        tail_176:
            obj->win1v = (uint16_t)val;
            return;

        case 0x48: case 0x49: case 0x4a: case 0x4b:
            if (size <= 3) val = blend(obj->window_control, val, shift32(dir), size);
            obj->window_control = val & 0x3f3f3f3fu;
            return;

        case 0x4c:
            if (size <= 1) val = blend(obj->mosaic, val, shift16(dir), size);
            obj->mosaic = (uint16_t)val;
            return;

        case 0x50: case 0x51:
            if (size <= 1) {
                uint32_t v = blend(obj->bldcnt, val, shift16(dir), size);
                obj->bldcnt = (uint16_t)v;
                return;
            }
            obj->bldcnt = (uint16_t)val;
            if (size == 2) return;
            val >>= 16;
            obj->bldalpha = (uint16_t)val;
            return;

        case 0x52: case 0x53:
            if (size <= 1) val = blend(obj->bldalpha, val, shift16(dir), size);
            obj->bldalpha = (uint16_t)val;
            return;

        case 0x54:
            if (size <= 1) val = blend(obj->bldy, val, shift16(dir), size);
            obj->bldy = (uint16_t)(val & 0x1fu);
            return;

        case 0x6c:
            if (size <= 1) val = blend(obj->master_bright, val, shift16(dir), size);
            obj->master_bright = (uint16_t)val;
            return;

        case 0x305: {
            uint32_t screen = obj->index;
            uint8_t *fb;
            if ((val & 0x80u) == 0) {
                uint8_t *c = (uint8_t *)obj->gpu->vram.bus->machine;

                uint32_t g = ((nds_t *)c)->config.fix_main_screen;
                screen = (uint32_t)(g == 0) ^ screen;
            }
            fb = (uint8_t *)video_out_screen_buffer(screen);
            if (obj->no_framebuffer != 0) fb = 0;
            obj->framebuffer = fb;
            obj->framebuffer_line_size = (uint32_t)video_out_screen_line_size(screen);
            return;
        }

        default:
            return;
        }
    }
}

void *gpu2d_engine_init_slot(gpu2d_engine_t *obj, uint32_t idx, gpu_t *gpu) {
    vram_map_t *ctx = &gpu->vram;
    obj->gpu = gpu;

    uint8_t *table_a = ctx->engine_palette[idx];
    uint8_t *table_b = ctx->engine_oam[idx];

    uint8_t **block = ctx->bg_ext_palette[idx];
    uint32_t mark = idx << 21;
    obj->palette = table_a;
    obj->screen_base = mark;
    obj->char_base = mark;
    obj->index = (unsigned char)idx;
    obj->bg_ext_palette = block;
    obj->oam = table_b;

    uint8_t *system = ctx->bus->vram_window;

    obj->bg[2].engine = obj;
    obj->bg[3].engine = obj;
    obj->vram_window   = system;
    obj->bg[0].engine = obj;
    obj->bg[0].vram_window = system;
    obj->bg[1].engine = obj;
    obj->bg[1].vram_window = system;
    obj->bg[2].vram_window = system;
    obj->bg[3].vram_window = system;
    return obj;
}

static void constants(gpu2d_bg_t *b) {
    b->hofs = 0;
    b->vofs = 0;
    b->mask_x = 0x7f;
    b->mask_y = 0x7f;
    b->width_shift = 7;
    b->tiles_per_row_minus_one = 0xf;
    b->size_code = 4;
    b->affine_dirty = 1;
}

static void initial_layer(gpu2d_engine_t *o, uint32_t k) {
    gpu2d_engine_t *p = o->bg[k].engine;
    gpu2d_bg_t *b = &o->bg[k];
    b->bgcnt = 0;
    b->palette = p->gpu->vram.engine_palette[p->index];
    b->ext_palette = 0;
    b->screen_base = p->screen_base;
    b->char_base = p->char_base;
    b->screen_offset = 0;
    b->char_offset = 0;
    b->ref_x = 0;
    b->ref_y = 0;
    b->pa = 0;
    b->pc = 0;
    b->pb = 0;
    b->pd = 0;
    constants(b);
    b->direct_ptr = 0;
    b->direct_ptr_alt = 0;
}

void gpu2d_engine_init_substructures(gpu2d_engine_t *o) {

    void *v0 = o->gpu->vram.obj_ext_palette[o->index];
    o->dispcnt = 0;
    o->window_flags = 0;
    o->window_control = 0;
    o->bldcnt = 0;
    o->bldy = 0;
    o->bldalpha = 0;
    o->master_bright = 0;
    o->win0h = 0;
    o->win1h = 0;
    o->win0v = 0;
    o->win1v = 0;
    o->window_dirty = 3;
    o->oam_dirty = 0;
    o->no_framebuffer = 0;
    o->unmapped_2 = 1;
    o->obj_ext_palette = v0;
    o->deferred_read = 0;
    o->deferred_count = 0;
    initial_layer(o, 0);
    initial_layer(o, 1);
    initial_layer(o, 2);
    initial_layer(o, 3);

    gpu2d_compose_build_layer_order(o);
    gpu2d_engine_select_bg_handlers(o);
}









static const uint8_t *take_and_advance(uint8_t *slot, uint64_t increment)
{
    uint8_t *p = rd_ptr_u8(slot);
    wr_ptr(slot, p + increment);
    return p;
}

void gpu2d_engine_load_state_registers(gpu2d_engine_t *ctx, uint8_t *entry, uint32_t count)
{

    uint8_t *slot = entry + 32;
    const uint8_t *p = rd_ptr_u8(slot);
    uint64_t base;
    uint32_t adjust_a;
    uint32_t adjust_b;

    ctx->dispcnt = rd32(p);

    p = take_and_advance(slot, 4);
    ctx->window_control = rd32(p + 4);
    p = take_and_advance(slot, 4);
    ctx->bldcnt = rd16(p + 4);
    p = take_and_advance(slot, 2);
    ctx->bldalpha = rd16(p + 2);
    p = take_and_advance(slot, 2);
    ctx->bldy = rd16(p + 2);
    p = take_and_advance(slot, 2);
    ctx->master_bright = rd16(p + 2);
    p = take_and_advance(slot, 2);

    if (count >= 10) {
        ctx->mosaic = rd16(p + 2);
        (void)take_and_advance(slot, 2);
    }

    p = rd_ptr_u8(slot);
    ctx->win0h = rd16(p);
    ctx->win1h = rd16(p + 2);
    p = take_and_advance(slot, 4);
    ctx->win0v = rd16(p + 4);
    ctx->win1v = rd16(p + 6);
    p = take_and_advance(slot, 4);
    ctx->window_flags = rd8(p + 4);

    p = take_and_advance(slot, 1);
    for (uint32_t i = 0; i != 4; i++) {
        gpu2d_bg_t *reg = &ctx->bg[i];

        reg->bgcnt = rd16(p + 1);
        p = take_and_advance(slot, 2);
        reg->affine_screen_offset = rd32(p + 2);
        p = take_and_advance(slot, 4);
        reg->screen_offset = rd32(p + 4);
        p = take_and_advance(slot, 4);
        reg->char_offset = rd32(p + 4);
        p = take_and_advance(slot, 4);
        reg->ref_x = rd32(p + 4);
        p = take_and_advance(slot, 4);
        reg->ref_y = rd32(p + 4);
        p = take_and_advance(slot, 4);
        reg->current_x = rd32(p + 4);
        p = take_and_advance(slot, 4);
        reg->current_y = rd32(p + 4);
        p = take_and_advance(slot, 4);
        reg->hofs = rd16(p + 4);
        p = take_and_advance(slot, 2);
        reg->vofs = rd16(p + 2);
        p = take_and_advance(slot, 2);
        reg->pa = rd16(p + 2);
        p = take_and_advance(slot, 2);
        reg->pc = rd16(p + 2);
        p = take_and_advance(slot, 2);
        reg->pb = rd16(p + 2);
        p = take_and_advance(slot, 2);
        reg->pd = rd16(p + 2);
        p = take_and_advance(slot, 2);
        reg->mask_x = rd16(p + 2);
        p = take_and_advance(slot, 2);
        reg->mask_y = rd16(p + 2);
        p = take_and_advance(slot, 2);
        reg->width_shift = rd8(p + 2);
        p = take_and_advance(slot, 1);
        reg->tiles_per_row_minus_one = rd8(p + 1);
        p = take_and_advance(slot, 1);
        reg->size_code = rd8(p + 1);
        p = take_and_advance(slot, 1);
        reg->ext_palette_enabled = rd8(p + 1);

        p = take_and_advance(slot, 1);
    }

    base = ctx->dispcnt;
    ctx->window_dirty = 3;
    if (ctx->index != 0) {
        adjust_a = ctx->screen_base;
        adjust_b = ctx->char_base;
    } else {
        adjust_a = (uint32_t)((base >> 11) & 0x1fffffu) & 0x70000u;
        adjust_b = (uint32_t)((base >> 8) & 0xffffffu) & 0x70000u;
        ctx->screen_base = adjust_a;
        ctx->char_base = adjust_b;
        ctx->display_vram_bank = ctx->gpu->vram.bank_data[(base >> 18) & 3u];
    }

    {
        uint8_t mode = (uint8_t)((base >> 30) & 1u);
        uint32_t a = ctx->bg[0].screen_offset;
        uint32_t b = ctx->bg[1].screen_offset;
        uint32_t c = ctx->bg[2].screen_offset;
        uint32_t d = ctx->bg[3].screen_offset;
        uint32_t e = ctx->bg[0].char_offset;
        uint32_t f = ctx->bg[1].char_offset;
        uint32_t g = ctx->bg[2].char_offset;
        uint32_t h = ctx->bg[3].char_offset;

        ctx->bg[0].ext_palette_enabled = mode;
        ctx->bg[1].ext_palette_enabled = mode;
        ctx->bg[2].ext_palette_enabled = mode;
        ctx->bg[3].ext_palette_enabled = mode;
        ctx->bg[0].screen_base = a + adjust_a;
        ctx->bg[0].char_base = e + adjust_b;
        ctx->bg[1].screen_base = b + adjust_a;
        ctx->bg[1].char_base = f + adjust_b;
        ctx->bg[0].affine_dirty = 1;
        ctx->bg[1].affine_dirty = 1;
        ctx->bg[2].affine_dirty = 1;
        ctx->bg[2].screen_base = c + adjust_a;
        ctx->bg[2].char_base = g + adjust_b;
        ctx->bg[3].screen_base = d + adjust_a;
        ctx->bg[3].char_base = h + adjust_b;
        ctx->bg[3].affine_dirty = 1;
    }

    gpu2d_engine_select_bg_handlers(ctx);
    gpu2d_compose_build_layer_order(ctx);
    gpu2d_obj_scan_oam(ctx);
}



void gpu2d_engine_save_state_registers(const gpu2d_engine_t *param_1, unsigned char *param_2, uint32_t param_3)
{

    unsigned char *slot = param_2 + 0x20;
    unsigned char *cur;
    memcpy(&cur, slot, 8);

    wr32(cur, param_1->dispcnt); cur += 4;
    wr32(cur, param_1->window_control); cur += 4;
    wr16(cur, param_1->bldcnt); cur += 2;
    wr16(cur, param_1->bldalpha); cur += 2;
    wr16(cur, param_1->bldy); cur += 2;
    wr16(cur, param_1->master_bright); cur += 2;

    if (param_3 > 9) {
        wr16(cur, param_1->mosaic); cur += 2;
    }

    wr16(cur, param_1->win0h); cur += 2;
    wr16(cur, param_1->win1h); cur += 2;
    wr16(cur, param_1->win0v); cur += 2;
    wr16(cur, param_1->win1v); cur += 2;
    *cur = param_1->window_flags;          cur += 1;

    for (uint64_t base = 0; base != 0x2c0; base += 0xb0) {
        const gpu2d_bg_t *src = &param_1->bg[base / 0xb0];

        wr16(cur, src->bgcnt); cur += 2;
        wr32(cur, src->affine_screen_offset); cur += 4;
        wr32(cur, src->screen_offset); cur += 4;
        wr32(cur, src->char_offset); cur += 4;
        wr32(cur, src->ref_x); cur += 4;
        wr32(cur, src->ref_y); cur += 4;
        wr32(cur, src->current_x); cur += 4;
        wr32(cur, src->current_y); cur += 4;
        wr16(cur, src->hofs); cur += 2;
        wr16(cur, src->vofs); cur += 2;
        wr16(cur, src->pa); cur += 2;
        wr16(cur, src->pc); cur += 2;
        wr16(cur, src->pb); cur += 2;
        wr16(cur, src->pd); cur += 2;
        wr16(cur, src->mask_x); cur += 2;
        wr16(cur, src->mask_y); cur += 2;
        *cur = src->width_shift; cur += 1;
        *cur = src->tiles_per_row_minus_one; cur += 1;
        *cur = src->size_code; cur += 1;
        *cur = src->ext_palette_enabled; cur += 1;
    }

    memcpy(slot, &cur, 8);
}



void gpu2d_engine_gen_interp_samples(unsigned char *param_1, float *param_2, float *param_3,
                         uint32_t param_4)
{

    if (param_4 == 0)
        return;

    uint32_t idx = 0;
    unsigned char *reg = param_1 + 0x630;
    uint16_t count16 = rd16(reg);

    for (;;) {
        if (count16 != 0) {
            uint32_t count = count16;
            int32_t  iv2    = rd32s(param_1 + (size_t)idx * 4);
            int32_t  iv3    = rd32s(param_1 + 0xb0 + (size_t)idx * 4);

            float f_amp = (float)count * (float)(iv2 + iv3);
            float f_t   = 0.0f;
            float f_iv2 = (float)iv2;
            float f_iv3 = (float)iv3;

            do {
                *param_2++ = f_t;
                *param_3++ = f_amp;
                f_t   = f_t + f_iv2;
                count -= 1;
                f_amp = f_amp - f_iv3;
            } while (count != 0);
        }

        idx++;
        reg += 4;
        if (idx == param_4)
            break;
        count16 = rd16(reg);
    }
}



static int32_t fcvtzs_q15(float v)
{
    if (v != v)
        return 0;

    double scaled = (double)v * 32768.0;

    if (scaled >= 2147483648.0)
        return 0x7fffffff;
    if (scaled <= -2147483648.0)
        return (int32_t)0x80000000u;

    return (int32_t)scaled;
}

static void process_one(unsigned char *out, const unsigned char *a,
                         const unsigned char *b, unsigned long i)
{
    float qa = rd_f32(a + (size_t)i * 4);
    float qb = rd_f32(b + (size_t)i * 4);
    float q  = qa / qb;
    int32_t fixed = fcvtzs_q15(q);
    wr16(out + (size_t)i * 2, (int16_t)fixed);
}

void gpu2d_engine_div_to_q15(unsigned char *param_1, const unsigned char *param_2,
                         const unsigned char *param_3, unsigned int param_4)
{

    if (param_4 == 0)
        return;

    unsigned int block = (param_4 < 8) ? 0 : (param_4 & ~7u);
    unsigned int i;

    for (i = 0; i < block; i++)
        process_one(param_1, param_2, param_3, i);

    for (; i < param_4; i++)
        process_one(param_1, param_2, param_3, i);
}




void gpu2d_engine_gen_scale_table(uint8_t *dest, const uint8_t *origin,
                         uint32_t count, const uint8_t *table)
{

    if (count == 0)
        return;

    const uint8_t *entry = origin + 0x630;
    do {
        uint16_t idx = rd16(entry);

        if (idx != 0) {
            uint32_t step = rd32(table + ((uint32_t)idx << 2));
            uint32_t i;

            for (i = 0; i < (uint32_t)idx; i++) {
                uint32_t product = step * i;
                wr16(dest + i * 2u, (uint16_t)(product >> 16));
            }
        }

        count--;
        entry += 4;
    } while (count != 0);
}

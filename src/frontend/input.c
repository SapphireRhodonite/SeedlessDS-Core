#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "frontend/script/script_io.h"
#include <string.h>
#include <time.h>
#include "frontend/frontend.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"
#include "seedlessds/control.h"

uint64_t input_hook_noop(uint64_t x0)
{
    return x0;
}

void input_state_reset(void) {
    frontend_t *g = FRONTEND;
    *(volatile uint32_t *)&g->buttons      = 0;
    *(volatile uint8_t  *)&g->touch_down   = 0;
    *(volatile uint64_t *)&g->touch_x      = 0;
    *(volatile uint8_t  *)&g->accel_valid = 0;
    *(volatile uint8_t  *)&g->gyro_valid = 0;
}
#undef OFF_G

#define GLOB     0x14c000
#define TABLE    0x106cf8
#define C_MODE   0x80050
#define C_DST    0x80010
typedef void     (*fn_lj)(void *, int);
extern void *script_io_publish_inputs(script_io_t *obj);
extern void *script_io_consume_outputs(uint32_t *dest);
extern void  pacer_init(unsigned char *ctx);
extern int   platform_doublebuffer_signal(void);
extern int   cart_open_rom(cart_t *cart, char *path);







static void load(frontend_t *g, int extra) {
    nds_config_t *d = &g->machine->config;
    g->reset_request = 0;
    for (int i = 0; i < 4; i++)
        d->nickname[i] = g->nickname[i];
    for (int i = 0; i < 4; i++)
        d->nickname[4 + i] = g->nickname[4 + i];
    d->nickname[8] = g->nickname[8];
    d->nickname[9] = g->nickname[9];
    memcpy(&d->language, g->config, sizeof g->config);

    platform_config_record_reset();

    nds_machine_reset((unsigned char *)g->machine);

    if (extra) audio_out_restart_playback();

    ((fn_lj)sym_libc_longjmp)(g->machine->fatal_jump, 0);
}

void input_poll_and_pause(uint8_t *ctx) {

    frontend_t *g = FRONTEND;
    uint8_t *dst = ctx + C_DST;

    if (ctx[C_MODE] != 2) {
        uint32_t bot = g->buttons;
        uint32_t masked = g->buttons_mask;
        if ((masked & bot) != 0) {
            static const uint32_t turbo_pattern[5] = {
                0xAAAAAAAAu, 0xE0E0E0E0u, 0xF01E03C0u, 0xFC00FC00u, 0xFC000000u,
            };
            unsigned profile = g->button_profile;
            if (profile > 4u) profile = 4u;
            uint32_t tab = turbo_pattern[profile];
            uint32_t n   = (uint32_t)g->frame_counter;
            if (((0x80000000u >> (n & 31u)) & tab) == 0)
                bot &= ~masked;
        }
        wr32(dst, (g->lid_closed != 0) ? (bot | 0x1000u) : bot);
    }

    dst[12] = g->touch_down;
    memcpy(dst + 4, &g->touch_x, sizeof g->touch_x + sizeof g->touch_y);

    uint32_t has = 0;
    if (g->accel_valid != 0) {
        memcpy(dst + 32, &g->accel_x, sizeof g->accel_x + sizeof g->accel_y);
        wr32(dst + 40, rd32(&g->accel_z));
        g->accel_valid = 0;
        has = 1;
    }
    dst[44] = (uint8_t)has;

    has = 0;
    if (g->gyro_valid != 0) {
        wr32(dst + 48, rd32(&g->gyro));
        g->gyro_valid = 0;
        has = 1;
    }
    dst[52] = (uint8_t)has;

    g->frame_counter = g->frame_counter + 1u;
    nds_control_frame_boundary();
    nds_control_poll_state(g->machine);
    uint32_t request = (g->state_saving != 0);

    if ((g->pause_request != 0 || nds_control_pause_requested()) && g->quit_request == 0) {
        do {
            nds_control_pause_acknowledge(1);
            nds_control_poll_state(g->machine);
            if (request) { state_save_slot_with_screens(g->state_slot); g->state_saving = 0; }
            if (g->state_load_request != 0 && g->rom_load_request == 0) {

                state_load_slot((unsigned char *)g->machine, g->state_slot, 0, 0, 0);
                g->state_load_request = 0;
            }
            if (g->cheats_update_request != 0) {
                nds_t *c = g->machine;
                uint8_t *q = (uint8_t *)rd_ptr((uint8_t *)c + 0x7b0);
                cheats_load_entry_by_key(&c->cart.cheats, rd32(q), rd32(q + 4));
                g->cheats_update_request = 0;
            }
            nds_platform_default()->threads.sleep_us(50000);

            pacer_init((unsigned char *)g->machine);

            platform_doublebuffer_signal();
            request = (g->state_saving != 0);
        } while ((g->pause_request != 0 || nds_control_pause_requested()) && g->quit_request == 0);
        nds_control_pause_acknowledge(0);
    }

    if (request) {
        state_save_slot_with_screens(g->state_slot);
        g->state_saving = 0;
        g->notice_pulse = 1;
    }

    if (g->autosave_seconds != 0) {
        time_t now = (time_t)nds_platform_default()->time.now_seconds(nds_platform_default()->user);
        if ((int64_t)now >= (int64_t)g->autosave_deadline) {
            g->notice_pulse = 1;
            g->autosave_deadline = (uint64_t)now + g->autosave_seconds;
            state_save_slot_with_screens(9);
        }
    }

    if (g->quit_request != 0) { wr32(dst, 0x4000000u); return; }

    if (g->rom_load_request != 0) {
        nds_t *c = g->machine;
        uint32_t t = g->rom_in_cache_dir;
        g->rom_load_request = 0;
        g->frame_state = 10;
        c->config.rom_in_cache_dir = t;

        if (cart_open_rom(&c->cart, g->rom_path) == 0)
            load(g, 1);
        g->quit_request = 1;
        return;
    }

    if (g->reset_request != 0) load(g, 0);

    if (g->cheats_update_request != 0) {
        nds_t *c = g->machine;
        uint8_t *q = (uint8_t *)rd_ptr((uint8_t *)c + 0x7b0);
        cheats_load_entry_by_key(&c->cart.cheats, rd32(q), rd32(q + 4));
        g->cheats_update_request = 0;
    }

    if (g->state_load_request != 0) {
        uint32_t expected = g->state_load_delay;
        if (expected != 0) { g->state_load_delay = expected - 1u; return; }

        state_load_slot((unsigned char *)g->machine, g->state_slot, 0, 0, 0);
        g->state_load_request = 0;
    }

    {
        int32_t n = (int32_t)g->frame_state - 1;
        if (n >= 0) g->frame_state = (uint32_t)n;
    }

    script_io_t *e = &g->script;

    script_io_publish_inputs(e);
    script_io_consume_outputs(&e->layout);

    int32_t  a = (int32_t)e->layout;
    uint32_t v = g->script_overrides;

    if (a != -1) {
        g->script_overrides = v & 0xff00ffffu;
        v = (v & ~(0xffu << 16)) | (((uint32_t)a & 0xffu) << 16);
        v |= 1u;
    } else {
        v &= ~1u;
    }

    int32_t b = (int32_t)e->screen_swap;
    g->script_overrides = v;
    if (b != -1) {
        v = (b == 0) ? (v & 0x7fffffffu) : (v | 0x80000000u);
        g->script_overrides = v;
        v |= 0x8000u;
    } else {
        v &= ~0x8000u;
    }

    int32_t c2 = (int32_t)e->overlay;
    g->script_overrides = v;
    if (c2 != -1) {
        v = (c2 == 0) ? (v & 0xbfffbfffu) : (v | 0x40000000u);
        v |= 0x4000u;
    } else {
        v &= ~0x4000u;
    }
    g->script_overrides = v;
}
#undef GLOB
#undef TABLE
#undef C_MODE
#undef C_DST

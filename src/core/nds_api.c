#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/nds.h"
#include "seedlessds/script.h"
#include "core/nds_state.h"
#include "frontend/frontend.h"
#include "core_internals.h"


#define FRONTEND_BOOT_KEY UINT64_C(0x1AB10BF4DBBE1F0F)
#define FRONTEND_BOOT_WORD_OFFSET (0x430 - 0x2c)

nds_t *nds_create(const nds_platform_t *platform, uint32_t api_level,
                  uint32_t script_overrides_high)
{
    frontend_t *g = FRONTEND;
    uint64_t magic;

    (void)platform;

    g->api_level = api_level;
    g->script_overrides_high = script_overrides_high;

    magic = (uint64_t)(uintptr_t)sym_libc_glViewport;
    magic ^= FRONTEND_BOOT_KEY;
    memcpy(g->rom_path + FRONTEND_BOOT_WORD_OFFSET, &magic, sizeof magic);
    static const uint32_t config_defaults[FRONTEND_CONFIG_WORDS] = { 1u, 0u, 7u, 7u };
    memcpy(g->config, config_defaults, sizeof g->config);

    g->cheats_update_request = 0;
    g->machine = mirror_get_fixed_page_ptr();
    g->maps_dump_enabled = 0;

    video_out_gl_context_init();
    return g->machine;
}

void nds_destroy(nds_t *nds)
{
    (void)nds;
    nds_subsystem_shutdown_release_objects();
}

void nds_reset(nds_t *nds)
{
    (void)nds;
    recon_drs_reset();
    FRONTEND->reset_request = 1;
}

void nds_pause(nds_t *nds, int paused)
{
    frontend_t *g = FRONTEND;
    uint8_t want = (uint8_t)(paused & 0xff);

    (void)nds;

    if (g->pause_request == want)
        return;

    g->pause_request = want;

    if ((int32_t)(uint32_t)g->config_bits < 0) {
        if (want == 0)
            audio_out_restart_playback();
        else
            audio_out_flush_queues();
    }
}

void nds_quit(nds_t *nds)
{
    frontend_t *g = FRONTEND;

    (void)nds;

    audio_out_flush_queues();
    g->lid_closed = 0;
    g->quit_request = 1;
    g->pause_request = 0;
}

#define CONFIG_BUTTON_PROFILE_SHIFT 32
#define CONFIG_BUTTON_PROFILE_MASK 7u
#define CONFIG_VOLUME_SHIFT 37
#define CONFIG_VOLUME_MASK 3u
#define TEXTURE_FORMAT_16 16
#define TEXTURE_FORMAT_32 32

void nds_apply_config(nds_t *nds, uint64_t bits)
{
    frontend_t *g = FRONTEND;
    nds_t *machine;
    uint8_t format;

    (void)nds;

    g->config_bits = bits;
    g->button_profile = (uint8_t)((bits >> CONFIG_BUTTON_PROFILE_SHIFT)
                                  & CONFIG_BUTTON_PROFILE_MASK);

    machine = g->machine;
    if (machine == NULL)
        return;

    config_unpack_word((unsigned char *)&machine->config);

    format = (g->config_bits & FRONTEND_CONFIG_SCREENSHOT_SWAP_RB)
             ? TEXTURE_FORMAT_16 : TEXTURE_FORMAT_32;
    machine->runtime.texture_format = format;
    video_out_gl_set_texture_format((uint32_t)format);

    audio_out_set_volume_level((uint32_t)((g->config_bits >> CONFIG_VOLUME_SHIFT)
                                                        & CONFIG_VOLUME_MASK));
}

void nds_set_firmware_user(nds_t *nds, const uint16_t *nickname, size_t units,
                           uint32_t packed)
{
    frontend_t *g = FRONTEND;

    (void)nds;

    if (nickname != NULL && units != 0) {
        if (units > NDS_NICKNAME_UNITS)
            units = NDS_NICKNAME_UNITS;
        memset(g->nickname, 0, sizeof g->nickname);
        memcpy(g->nickname, nickname, units * sizeof *g->nickname);
    }

    g->config[0] = packed & 0xffu;
    g->config[1] = (packed >> 8) & 0xffu;
    g->config[2] = (packed >> 16) & 0xffu;
    g->config[3] = packed >> 24;
}

void nds_run_frame(nds_t *nds, const nds_input_t *input)
{
    if (input != NULL)
        nds_input_set(nds, input);
    input_poll_and_pause((uint8_t *)nds);
}

uint32_t nds_frame_word(nds_t *nds)
{
    (void)nds;
    frontend_t *g = FRONTEND;
    uint32_t state = g->frame_state;
    uint32_t word = state & NDS_FRAME_STATE_MASK;

    if (g->lid_closed != 0)
        word |= NDS_FRAME_LID_CLOSED;

    if (g->notice_pulse != 0) {
        word |= NDS_FRAME_NOTICE;
        g->notice_pulse = 0;
    }

    if (state != 0) {
        uint32_t percent = (uint32_t)nds_loading_percent();

        if (percent > 100)
            percent = 100;
        word |= percent << NDS_FRAME_LOADING_SHIFT;
    }

    return word;
}

void nds_frame_info(nds_t *nds, nds_frame_info_t *info)
{
    frontend_t *g = FRONTEND;
    uint32_t word = nds_frame_word(nds);

    info->frame_counter = (uint32_t)g->frame_counter;
    info->state = word & NDS_FRAME_STATE_MASK;
    info->loading_percent = (word >> NDS_FRAME_LOADING_SHIFT) & 0xffu;
    info->lid_closed = (word & NDS_FRAME_LID_CLOSED) != 0;
    info->notice = (word & NDS_FRAME_NOTICE) != 0;
}

void nds_signal_screen(nds_t *nds)
{
    (void)nds;
    platform_doublebuffer_signal();
}

void nds_wait_screen(nds_t *nds)
{
    (void)nds;
    platform_doublebuffer_wait();
}

int nds_state_save(nds_t *nds, int slot)
{
    (void)nds;
    frontend_t *g = FRONTEND;
    g->state_slot = (uint8_t)slot;
    g->state_saving = 1;
    return 0;
}

int nds_state_load(nds_t *nds, int slot)
{
    (void)nds;
    frontend_t *g = FRONTEND;
    g->lid_closed = 0;
    g->state_load_delay = 0;
    g->state_slot = (uint8_t)slot;
    g->state_load_request = 1;
    recon_drs_reset();
    return 0;
}

int nds_state_saving(nds_t *nds)
{
    (void)nds;
    if (FRONTEND->state_saving != 0)
        return 1;
    return (int)state_write_pending();
}

int nds_state_slot(nds_t *nds)
{
    (void)nds;
    return (int)FRONTEND->state_slot;
}

void nds_autosave_interval(nds_t *nds, unsigned seconds)
{
    (void)nds;
    FRONTEND->autosave_seconds = seconds;
}

void nds_input_set(nds_t *nds, const nds_input_t *input)
{
    (void)nds;
    frontend_t *g = FRONTEND;
    g->buttons = input->keys;
    g->buttons_mask = input->keys_mask;
    g->touch_down = (uint8_t)(input->touch_down != 0);
    g->touch_x = (uint32_t)input->touch_x;
    g->touch_y = (uint32_t)input->touch_y;
}

void nds_accel(nds_t *nds, float x, float y, float z)
{
    (void)nds;
    frontend_t *g = FRONTEND;
    g->accel_x = x;
    g->accel_y = y;
    g->accel_z = z;
    g->accel_valid = 1;
}

void nds_gyro(nds_t *nds, float x, float y, float z)
{
    (void)nds;
    (void)y;
    (void)z;
    frontend_t *g = FRONTEND;
    g->gyro = x;
    g->gyro_valid = 1;
}

uint32_t nds_rumble_state(nds_t *nds)
{
    (void)nds;
    return (uint32_t)(FRONTEND->rumble != 0);
}

void nds_whitenoise(nds_t *nds, int enabled)
{
    (void)nds;
    audio_out_set_runtime_word((uint32_t)(enabled != 0));
}

void nds_hinge(nds_t *nds, int closed)
{
    (void)nds;
    FRONTEND->lid_closed = (uint8_t)(closed != 0);
}

void nds_audio_volume(nds_t *nds, int volume)
{
    (void)nds;
    audio_out_set_volume_db((uint32_t)volume);
}

int nds_script_active(nds_t *nds)
{
    return nds_script_is_active(nds);
}

uint32_t nds_script_overrides(nds_t *nds)
{
    return nds_script_get_overrides(nds);
}

void nds_script_axis(nds_t *nds, const float *axes, size_t count)
{
    nds_script_set_axis(nds, axes, count);
}

void nds_script_rotation(nds_t *nds, float rotation)
{
    nds_script_set_rotation(nds, rotation);
}

void *nds_main_ram(nds_t *nds)
{
    if (nds == NULL)
        nds = FRONTEND->machine;
    if (nds == NULL)
        return NULL;
    return nds->bus.main_ram;
}

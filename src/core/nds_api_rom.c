#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/data_paths.h"
#include "seedlessds/nds.h"
#include "seedlessds/platform.h"
#include "core/nds_state.h"
#include "core/nds_start.h"
#include "frontend/frontend.h"
#include "core_internals.h"

#define ROM_PATH_FORMAT_BYTES 0x400
#define MAPS_DUMP_UNKNOWN 0
#define MAPS_DUMP_ON 1
#define MAPS_DUMP_OFF 2
#define STATE_LOAD_DELAY_FRAMES 30
#define FRAME_STATE_LOADING 10
#define BOOT_SEED_MODULUS 0x9acu
#define BOOT_SEED_MINIMUM 0x469u
#define BOOT_SEED_FALLBACK 0x519u
#define TEXTURE_FORMAT_16 16
#define TEXTURE_FORMAT_32 32
#define VERSION_STRING_BYTES 0x10
#define ICON_BLOCK_BYTES 800
#define ICON_PALETTE_ENTRIES 15
#define ICON_PALETTE_SOURCE 514
#define ICON_PIXEL_BYTES 512
#define ICON_TITLE_SOURCE 544
#define ICON_TITLE_ROW_BYTES 32
#define ICON_CLEARED_PALETTE_BYTES 64
#define ICON_CLEARED_PIXEL_BYTES 1024
#define ICON_CLEARED_TITLE_BYTES 256

typedef int (*fn_fclose)(void *);
typedef int64_t (*fn_time)(int64_t *);

static uint32_t icon_color(uint16_t entry)
{
    uint32_t value = entry;
    uint32_t color = (value << 6) & 0xf800u;

    color |= (value & 0x1fffu) << 19;
    color |= (value >> 7) & 0xf8u;
    return color | 0xff000000u;
}

static void icon_nibbles(uint8_t *out, uint32_t word)
{
    unsigned i;

    for (i = 0; i < 8; i++)
        out[i] = (uint8_t)((word >> (i * 4)) & 0xfu);
}

static void icon_clear(uint8_t *out, unsigned bytes)
{
    unsigned i;

    for (i = 0; i < bytes; i++)
        out[i] = 0;
}

static void script_clear(script_io_t *script)
{
    script->axis_lx = 0.0f;
    script->axis_ly = 0.0f;
    script->axis_rx = 0.0f;
    script->axis_ry = 0.0f;
    script->rotation = 0;
    memset(script->unmapped_0, 0, sizeof script->unmapped_0);
}

static void frontend_prepare(frontend_t *g, const nds_rom_open_t *open)
{
    g->frame_state = FRAME_STATE_LOADING;
    g->frame_counter = 0;
    g->config_bits = open->config_bits;
    g->lid_closed = 0;
    g->quit_request = 0;
    g->pause_request = 0;
    g->buttons = 0;
    g->state_slot = 0;
    g->state_saving = 0;
    g->state_load_request = 0;
    g->cheats_update_request = 0;
    g->savestate_extra = (uint8_t)((open->config_bits >> 25) & 1u);
    g->button_profile = (uint8_t)((open->config_bits >> 32) & 7u);
    g->run_limit_us = open->run_limit_us;
    g->script_overrides = 0;
    g->rom_in_cache_dir = (uint8_t)(open->in_cache_dir != 0);
    script_clear(&g->script);
}

static void audio_prepare(uint32_t config_low)
{
    audio_out_set_mode_word((config_low >> 8) & 3u);
    audio_out_set_runtime_flag((~config_low) >> 31);
    audio_out_set_enabled_flag((config_low >> 26) & 1u);
    audio_out_set_runtime_word(0);
}

static void autosave_rearm(frontend_t *g)
{
    if (g->autosave_seconds != 0) {
        int64_t now = ((fn_time)sym_libc_time)(NULL);

        g->autosave_deadline = (uint64_t)now + g->autosave_seconds;
    }
}

static void maps_dump_probe(frontend_t *g)
{
    char path[1024];
    void *file;

    if (g->maps_dump_enabled != MAPS_DUMP_UNKNOWN)
        return;

    str_vsprintf_caller_limit_1(path, ROM_PATH_FORMAT_BYTES,
                                                 "%s/%s",
                                                 DATA_SYSTEM_DIR,
                                                 "config/dbg_mo.de");
    file = platform_file_open(path, "rb");
    if (file != NULL) {
        ((fn_fclose)sym_libc_fclose)(file);
        g->maps_dump_enabled = MAPS_DUMP_ON;
    } else {
        g->maps_dump_enabled = MAPS_DUMP_OFF;
    }
}

int nds_load_rom(nds_t *nds, const nds_rom_open_t *open)
{
    frontend_t *g = FRONTEND;
    nds_start_params_t params;
    uint64_t seed;

    (void)nds;

    (void)video_out_gl_clear_framebuffers(NULL);

    frontend_prepare(g, open);

    script_io_reset();
    time_now_microseconds(&g->startup_seed);
    seed = g->startup_seed % BOOT_SEED_MODULUS;
    g->startup_seed = (seed < BOOT_SEED_MINIMUM) ? BOOT_SEED_FALLBACK : seed;

    audio_prepare((uint32_t)open->config_bits);

    if (open->state_slot >= 0 && open->benchmark_frames < 1) {
        g->state_load_delay = STATE_LOAD_DELAY_FRAMES;
        g->state_slot = (uint8_t)open->state_slot;
        g->state_load_request = 1;
    }

    memset(&params, 0, sizeof params);
    params.cache_dir = DATA_SYSTEM_DIR;
    params.save_dir = DATA_USER_DIR;
    params.rom_path = open->path;
    params.machine_slot = &g->machine;
    params.benchmark_frames = (open->benchmark_frames < 1) ? 0u : (uint32_t)open->benchmark_frames;
    params.benchmark_slot = (uint32_t)open->state_slot;
    params.memory_offset = g->memory_offset;
    params.config = (const uint8_t *)g->config;
    params.texture_format = (g->config_bits & FRONTEND_CONFIG_SCREENSHOT_SWAP_RB)
                            ? TEXTURE_FORMAT_16 : TEXTURE_FORMAT_32;

    maps_dump_probe(g);
    autosave_rearm(g);

    return nds_start_game(&params) == 0;
}

int nds_insert_gba(nds_t *nds, const char *path, int state_slot, int in_cache_dir,
                   uint64_t run_limit_us)
{
    frontend_t *g = FRONTEND;

    (void)nds;

    if (path == NULL)
        return 0;

    audio_out_flush_queues();
    (void)video_out_gl_clear_framebuffers(NULL);
    jni_system_store_formatted_message(0, 0, 0, 0, path);

    g->frame_state = FRAME_STATE_LOADING;
    g->run_limit_us = run_limit_us;
    g->lid_closed = 0;
    g->quit_request = 0;
    g->buttons = 0;
    g->state_slot = 0;
    g->state_saving = 0;
    g->state_load_request = 0;
    g->rom_load_request = 1;
    g->cheats_update_request = 0;
    g->script_overrides = 0;
    g->rom_in_cache_dir = (uint8_t)((in_cache_dir & 0xff) != 0);
    script_clear(&g->script);

    script_io_reset();

    if (state_slot >= 0) {
        g->state_load_delay = STATE_LOAD_DELAY_FRAMES;
        g->state_slot = (uint8_t)state_slot;
        g->state_load_request = 1;
    }

    audio_out_set_runtime_word(0);

    autosave_rearm(g);

    return 1;
}

const char *nds_version_string(void)
{
    static char version[VERSION_STRING_BYTES];

    memset(version, 0, sizeof version);
    str_vsprintf_caller_limit_1(version, sizeof version,
                                                 "%s",
                                                 "r2.6.0.4a");
    return version;
}

const char *nds_info_string(nds_t *nds, char *buffer, size_t size)
{
    frontend_t *g = FRONTEND;
    uint32_t modifier = g->memory_modifier;

    (void)nds;

    memset(buffer, 0, size);
    str_vsprintf_caller_limit_1(buffer, size,
                                                 "MemOffset: %08X\nMemModifier: %d%d%d%d",
                                                 g->memory_offset,
                                                 (modifier >> 3) & 1u,
                                                 (modifier >> 2) & 1u,
                                                 (modifier >> 1) & 1u,
                                                 modifier & 1u);
    return buffer;
}

int nds_rom_is_nds(const char *path)
{
    if (path == NULL)
        return 0;
    return archive_rom_extension_is_supported(path) == 0;
}

int nds_rom_type(const char *path)
{
    int kind;

    if (path == NULL)
        return 0;

    kind = archive_rom_classify_header(path);
    if ((uint32_t)(kind - 1) < 3u)
        return kind;
    return 0;
}

uint32_t nds_rom_size(const char *path)
{
    uint32_t size = 0;

    if (path == NULL)
        return 0;

    if (archive_rom_read_chunk(path, &size, NULL, 0, 0) != 0)
        size = 0;
    return size;
}

int nds_rom_icon(const char *path, uint32_t *palette, uint8_t *pixels, uint8_t *title)
{
    uint8_t block[ICON_BLOCK_BYTES] __attribute__((aligned(16)));
    unsigned i;
    int read;

    if (path == NULL)
        return 0;

    read = archive_rom_read_reordered_block(path, block);

    if (read != 0) {
        icon_clear((uint8_t *)palette, ICON_CLEARED_PALETTE_BYTES);
        icon_clear(pixels, ICON_CLEARED_PIXEL_BYTES);
        icon_clear(title, ICON_CLEARED_TITLE_BYTES);
        return 0;
    }

    palette[0] = 0;
    for (i = 0; i < ICON_PALETTE_ENTRIES; i++) {
        uint16_t entry;

        memcpy(&entry, block + ICON_PALETTE_SOURCE + i * 2, sizeof entry);
        palette[1 + i] = icon_color(entry);
    }

    for (i = 0; i < ICON_PIXEL_BYTES; i += 16) {
        unsigned word;

        for (word = 0; word < 4; word++) {
            uint32_t bits;

            memcpy(&bits, block + i + word * 4, sizeof bits);
            icon_nibbles(pixels + i * 2 + word * 8, bits);
        }
    }

    for (i = 0; i < ICON_CLEARED_TITLE_BYTES; i += ICON_TITLE_ROW_BYTES)
        memcpy(title + i, block + ICON_TITLE_SOURCE + i, ICON_TITLE_ROW_BYTES);

    return 1;
}

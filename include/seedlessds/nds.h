#ifndef SEEDLESSDS_NDS_H
#define SEEDLESSDS_NDS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nds nds_t;
typedef struct nds_platform nds_platform_t;

typedef struct nds_input {
    uint32_t keys;
    uint32_t keys_mask;
    int touch_down;
    int touch_x;
    int touch_y;
} nds_input_t;

#define NDS_FRAME_NOTICE (1u << 30)
#define NDS_FRAME_LID_CLOSED (1u << 31)
#define NDS_FRAME_STATE_MASK 0xffffu
#define NDS_FRAME_LOADING_SHIFT 16

typedef struct nds_frame_info {
    uint32_t frame_counter;
    uint32_t state;
    uint32_t loading_percent;
    int lid_closed;
    int notice;
} nds_frame_info_t;

typedef struct nds_frame_size {
    uint32_t width;
    uint32_t height;
    uint32_t scale;
} nds_frame_size_t;

typedef struct nds_rom_open {
    const char *path;
    uint64_t config_bits;
    uint64_t run_limit_us;
    int state_slot;
    int benchmark_frames;
    int in_cache_dir;
} nds_rom_open_t;

nds_t *nds_create(const nds_platform_t *platform, uint32_t api_level,
                  uint32_t script_overrides_high);
void nds_destroy(nds_t *nds);
int nds_load_rom(nds_t *nds, const nds_rom_open_t *open);
int nds_insert_gba(nds_t *nds, const char *path, int state_slot, int in_cache_dir, uint64_t run_limit_us);
void nds_reset(nds_t *nds);
void nds_pause(nds_t *nds, int paused);
void nds_quit(nds_t *nds);
void nds_apply_config(nds_t *nds, uint64_t bits);
#define NDS_NICKNAME_UNITS 10

void nds_set_firmware_user(nds_t *nds, const uint16_t *nickname, size_t units,
                           uint32_t packed);
const char *nds_version_string(void);
const char *nds_info_string(nds_t *nds, char *buffer, size_t size);

#define NDS_ROM_ICON_PALETTE_WORDS 16
#define NDS_ROM_ICON_PIXEL_BYTES 1024
#define NDS_ROM_ICON_TITLE_BYTES 256

int nds_rom_is_nds(const char *path);
int nds_rom_type(const char *path);
uint32_t nds_rom_size(const char *path);
int nds_rom_icon(const char *path, uint32_t *palette, uint8_t *pixels, uint8_t *title);

void nds_run_frame(nds_t *nds, const nds_input_t *input);
void nds_frame_info(nds_t *nds, nds_frame_info_t *info);
uint32_t nds_frame_word(nds_t *nds);
uint32_t nds_perf_word(nds_t *nds);
void nds_screen_buffers(nds_t *nds, const void **top, const void **bottom);
int nds_screens_rgba(nds_t *nds, void *top, void *bottom);
int nds_screenshot(nds_t *nds, void *rgba, size_t size);
void nds_clear_screens(nds_t *nds, uint32_t texture_top, uint32_t texture_bottom);
void nds_signal_screen(nds_t *nds);
void nds_wait_screen(nds_t *nds);
void nds_render_frame(nds_t *nds, uint32_t first, uint32_t second, int flag);
void nds_draw_screen(nds_t *nds, uint32_t texture, uint32_t index);
void nds_draw_screen_ext(nds_t *nds, uint32_t texture, uint32_t index);
int nds_snapshot_slot(nds_t *nds, int slot, void *top16, void *bottom16);
int nds_snapshot_file(nds_t *nds, const char *path, void *top16, void *bottom16);
nds_frame_size_t nds_frame_size(nds_t *nds);
void nds_set_internal_resolution(nds_t *nds, unsigned scale);
void nds_set_present_mode(nds_t *nds, uint32_t bits);
void nds_set_compose_threads(nds_t *nds, unsigned threads);
void nds_set_frameskip(nds_t *nds, unsigned frames);
int nds_dump_page(nds_t *nds, void *buffer, size_t size);

int nds_state_save(nds_t *nds, int slot);
int nds_state_load(nds_t *nds, int slot);
int nds_state_saving(nds_t *nds);
int nds_state_slot(nds_t *nds);
void nds_autosave_interval(nds_t *nds, unsigned seconds);

void nds_input_set(nds_t *nds, const nds_input_t *input);
void nds_accel(nds_t *nds, float x, float y, float z);
void nds_gyro(nds_t *nds, float x, float y, float z);
uint32_t nds_rumble_state(nds_t *nds);
void nds_whitenoise(nds_t *nds, int enabled);
void nds_hinge(nds_t *nds, int closed);

void nds_audio_volume(nds_t *nds, int volume);

int nds_script_active(nds_t *nds);
uint32_t nds_script_overrides(nds_t *nds);
void nds_script_axis(nds_t *nds, const float *axes, size_t count);
void nds_script_rotation(nds_t *nds, float rotation);

#define NDS_CHEAT_ADDED 0
#define NDS_CHEAT_ADD_FAILED 1
#define NDS_CHEAT_WRITE_FAILED 2

int nds_cheat_add_custom(nds_t *nds, const char *name, const int32_t *codes,
                         uint32_t words, int enabled);
int nds_cheat_find_custom(nds_t *nds, const int32_t *codes, uint32_t words);
void nds_cheat_remove_custom(nds_t *nds, int index);
int nds_cheat_custom_count(nds_t *nds);
const int32_t *nds_cheat_custom_data(nds_t *nds, int index, uint32_t *words);
const char *nds_cheat_custom_name(nds_t *nds, int index);
int nds_cheat_custom_enabled(nds_t *nds, int index);
void nds_cheat_set_custom_enabled(nds_t *nds, int index, int enabled);
void nds_cheat_update(nds_t *nds, int enabled);
int nds_cheat_count(nds_t *nds);
int nds_cheat_enabled(nds_t *nds, int index);
const char *nds_cheat_name(nds_t *nds, int index);
const char *nds_cheat_note(nds_t *nds, int index);
void nds_cheat_set_enabled(nds_t *nds, int index, int enabled);
int nds_cheat_folder_count(nds_t *nds);
int nds_cheat_folder_id(nds_t *nds, int index);
const char *nds_cheat_folder_name(nds_t *nds, int index);
const char *nds_cheat_folder_note(nds_t *nds, int index);
int nds_cheat_folder_expanded(nds_t *nds, int index);
int nds_cheat_folder_multi_select(nds_t *nds, int index);
void nds_cheat_set_folder_expanded(nds_t *nds, int index, int expanded);

typedef struct nds_fx_frame {
    uint32_t texture_top;
    uint32_t texture_bottom;
    uint32_t vertex_top;
    uint32_t vertex_bottom;
    uint32_t vertex_intermediate;
    uint32_t top_width;
    uint32_t top_height;
    uint32_t bottom_width;
    uint32_t bottom_height;
    int flag;
} nds_fx_frame_t;

int32_t nds_fx_load(nds_t *nds, const char *recipe_path, uint32_t vbo_vertex_offset,
                    uint32_t vbo_texcoord_offset);
void nds_fx_setup(nds_t *nds, int source_width, int source_height, int x, int y,
                  int view_width, int view_height);
void nds_fx_render(nds_t *nds, const nds_fx_frame_t *frame);
int32_t nds_extfx_load(nds_t *nds, const char *recipe_path, uint32_t vbo_vertex_offset,
                       uint32_t vbo_texcoord_offset);
void nds_extfx_setup(nds_t *nds, int source_width, int source_height, int x, int y,
                     int view_width, int view_height);
void nds_extfx_render(nds_t *nds, uint32_t texture, uint32_t index, uint32_t a,
                      uint32_t b, uint32_t c, uint32_t d);

void *nds_main_ram(nds_t *nds);
size_t nds_memory_read(nds_t *nds, uint32_t address, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif

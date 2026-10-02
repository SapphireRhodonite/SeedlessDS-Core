#ifndef SEEDLESSDS_FRONTEND_H
#define SEEDLESSDS_FRONTEND_H

#include <stddef.h>
#include <stdint.h>
#include "script/script_io.h"

#define FRONTEND_ROM_PATH_BYTES 0x40c
#define FRONTEND_NICKNAME_CHARS 10
#define FRONTEND_CONFIG_WORDS 4

#define FRONTEND_CONFIG_SCREENSHOT_SWAP_RB (1ull << 23)
#define FRONTEND_CONFIG_SCREEN_MODE_SHIFT 40
#define FRONTEND_CONFIG_SCREEN_MODE_MASK 0xffull

typedef struct frontend_jni {
    void *vm;
    void *core_class;
    void *core_method_a;
    void *core_method_b;
    void *core_method_c;
    void *files_class;
    void *files_method_a;
    void *files_method_b;
    void *files_method_c;
} frontend_jni_t;

typedef struct frontend {
    struct nds *machine;
    uint32_t config[FRONTEND_CONFIG_WORDS];
    uint16_t nickname[FRONTEND_NICKNAME_CHARS];
    char rom_path[FRONTEND_ROM_PATH_BYTES];
    script_io_t script;
    uint8_t unmapped_0[4];
    uint64_t frame_counter;
    uint64_t config_bits;
    uint64_t startup_seed;
    uint64_t run_limit_us;
    uint32_t memory_offset;
    uint32_t memory_modifier;
    uint32_t state_load_delay;
    uint32_t buttons;
    uint32_t buttons_mask;
    uint32_t touch_x;
    uint32_t touch_y;
    uint32_t autosave_seconds;
    uint64_t autosave_deadline;
    uint32_t script_overrides;
    uint32_t script_overrides_high;
    uint32_t frame_state;
    uint8_t state_slot;
    uint8_t state_saving;
    uint8_t state_load_request;
    uint8_t button_profile;
    uint8_t savestate_extra;
    uint8_t quit_request;
    uint8_t pause_request;
    uint8_t reset_request;
    uint8_t rom_load_request;
    uint8_t cheats_update_request;
    uint8_t rom_in_cache_dir;
    uint8_t touch_down;
    uint8_t lid_closed;
    uint8_t notice_pulse;
    uint8_t maps_dump_enabled;
    uint8_t rumble;
    float accel_x;
    float accel_y;
    float accel_z;
    uint32_t accel_valid;
    float gyro;
    uint32_t gyro_valid;
    uint32_t api_level;
    frontend_jni_t jni;
    uint8_t unmapped_7[0xad8];
} frontend_t;

extern frontend_t frontend_state;

#define FRONTEND (&frontend_state)

#endif

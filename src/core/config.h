#ifndef SEEDLESSDS_CONFIG_H
#define SEEDLESSDS_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define NDS_CONFIG_SIZE 0x4d78u
#define NDS_PATH_SIZE 0x400u
#define NDS_FIRMWARE_NICKNAME_UNITS 10
#define NDS_BENCHMARK_PASSES 7
#define NDS_INPUT_HISTORY_SIZE 0x80000u

typedef struct nds_config {
    uint32_t nickname[NDS_FIRMWARE_NICKNAME_UNITS];
    uint32_t unmapped_0;
    uint32_t language;
    uint32_t favorite_color;
    uint32_t birthday_month;
    uint32_t birthday_day;
    char working_dir[NDS_PATH_SIZE];
    uint32_t unmapped_1;
    uint32_t frameskip_type;
    uint32_t frameskip_value;
    uint32_t show_fps;
    uint32_t always_one[2];
    uint32_t input_toggle;
    uint32_t state_slot;
    uint32_t fast_forward;
    uint32_t sound_enabled;
    uint32_t unmapped_3;
    uint32_t threaded_3d;
    uint32_t unmapped_4;
    uint32_t savestates_enabled;
    uint32_t savestate_copies;
    uint32_t cheats_enabled;
    uint32_t rom_in_cache_dir;
    uint32_t preload_roms;
    uint32_t savestate_extra;
    uint32_t ignore_gamecard_limit;
    uint32_t fast_forward_speed;
    uint32_t thread_count;
    uint32_t auto_trim;
    uint32_t fix_main_screen;
    uint32_t disable_edge_marking;
    uint32_t hires_3d;
    uint32_t unmapped_5;
    uint32_t lua_enabled;
    uint32_t frameskip_safe;
    uint32_t unmapped_6;
    uint32_t boot_from_firmware;
    uint32_t raw_sav_format;
    uint32_t slot2_type;
    uint32_t unmapped_7;
    uint32_t run_limit_enabled;
    uint64_t run_limit_ms;
    uint32_t rtc_system_time;
    uint8_t unmapped_8[NDS_CONFIG_SIZE - 1236];
} nds_config_t;

typedef struct nds_benchmark {
    struct nds *machine;
    uint64_t elapsed[NDS_BENCHMARK_PASSES];
    uint64_t start_time;
    uint32_t state_slot;
    uint32_t frames_per_pass;
    uint32_t pass_mask;
    uint32_t pass_index;
    uint32_t frame_counter;
    uint32_t pass_flags;
    uint32_t enabled;
    uint32_t full_run;
} nds_benchmark_t;

typedef struct nds_input_record {
    uint8_t history[NDS_INPUT_HISTORY_SIZE];
    uint8_t *cursor;
    struct nds *machine;
    uint32_t flags;
    uint32_t touch_x;
    uint32_t touch_y;
    uint32_t touch_down;
    uint32_t saved_flags;
    uint32_t saved_touch_x;
    uint32_t saved_touch_y;
    uint32_t saved_touch_down;
    uint32_t axes[3];
    uint32_t has_axes;
    uint32_t axis_4;
    uint32_t has_axis_4;
    void *file;
    uint32_t mode;
    uint32_t unmapped_0;
} nds_input_record_t;

typedef struct nds_runtime {
    uint64_t pacer_base;
    uint64_t pacer_accumulated;
    uint32_t extra_cycles;
    uint32_t unmapped_0[2];
    uint32_t dma_cost_low;
    uint32_t dma_cost_scale;
    uint8_t cart_cycles_per_word;
    uint8_t force_vcount_192;
    uint8_t dma_sets_cycle_mark;
    uint8_t gxfifo_swap_side_command;
    uint8_t force_line_finish;
    uint8_t pacer_flag;
    uint8_t pacer_count;
    uint8_t state_marker;
    uint8_t skip_frame;
    uint8_t skip_parity;
    uint8_t skip_run;
    uint8_t skip_count;
    uint8_t jit_enabled;
    uint8_t texture_format;
    uint8_t unmapped_2[0x3b30000u - 0x3b2f932u];
} nds_runtime_t;
#endif

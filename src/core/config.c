#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include <string.h>
#include "core/nds_state.h"
#include "frontend/frontend.h"
#include "core_internals.h"
#include "mem_access.h"

struct long_option { const char *name; int has_arg; int *flag; int val; };
static const struct long_option long_options[15] = {
    { "breakpoint-arm7", 1, 0, 0 },
    { "breakpoint-arm9", 1, 0, 0 },
    { "countdown-arm7", 1, 0, 0 },
    { "countdown-arm9", 1, 0, 0 },
    { "debug-arm7", 0, 0, 0 },
    { "debug-arm9", 0, 0, 0 },
    { "recompiler", 0, 0, 0 },
    { "color-depth", 1, 0, 0 },
    { "benchmark", 1, 0, 0 },
    { "bench-full", 1, 0, 0 },
    { "fast-forward", 0, 0, 0 },
    { "input-record", 1, 0, 0 },
    { "input-playback", 1, 0, 0 },
    { "interpreter", 0, 0, 0 },
    { 0, 0, 0, 0 },
};

void config_unpack_word(unsigned char *obj) {
    uint64_t cfg = FRONTEND->config_bits;

    #define BIT(n)      ((uint32_t)((cfg >> (n)) & 1u))
    #define FIELD(n, w) ((uint32_t)((cfg >> (n)) & ((1u << (w)) - 1u)))

    *(uint32_t *)(obj + 1096) = BIT(30);
    *(uint32_t *)(obj + 1144) = BIT(27);
    *(uint32_t *)(obj + 1208) = BIT(50);
    *(uint32_t *)(obj + 1092) = FIELD(0, 4);
    *(uint32_t *)(obj + 1116) = BIT(29);
    *(uint32_t *)(obj + 1120) = BIT(31);

    uint32_t mode = FIELD(5, 2);
    *(uint32_t *)(obj + 1088) = (mode == 1) ? 0u : (mode == 2 ? 1u : 2u);

    *(uint32_t *)(obj + 1156) = FRONTEND->savestate_extra;
    *(uint32_t *)(obj + 1160) = BIT(24);
    *(uint32_t *)(obj + 1200) = 0;

    uint32_t *four = (uint32_t *)(obj + 1172);
    four[0] = BIT(36);
    four[1] = BIT(35);
    four[2] = BIT(40);
    four[3] = BIT(41);

    *(uint32_t *)(obj + 1192) = BIT(42);
    *(uint32_t *)(obj + 1232) = BIT(39);
    *(uint32_t *)(obj + 1128) = BIT(28);
    *(uint32_t *)(obj + 1168) = FIELD(16, 4);
    *(uint32_t *)(obj + 1124) = 0;
    *(uint32_t *)(obj + 1188) = 0;
    *(uint32_t *)(obj + 1108) = 0;

    ((uint32_t *)(obj + 1100))[0] = 1;
    ((uint32_t *)(obj + 1100))[1] = 1;
    ((uint32_t *)(obj + 1136))[0] = 1;
    ((uint32_t *)(obj + 1136))[1] = 1;

    *(uint32_t *)(obj + 1212) = FIELD(43, 4);
    *(uint32_t *)(obj + 1148) = FRONTEND->rom_in_cache_dir;
    *(uint32_t *)(obj + 1216) = 3;
    *(uint32_t *)(obj + 1196) = BIT(47);
    *(uint32_t *)(obj + 1152) = BIT(48);

    nds_config_t *d = &FRONTEND->machine->config;
    d->run_limit_enabled = 0;
    d->run_limit_ms = 0;

    int64_t v = (int64_t)FRONTEND->run_limit_us;
    if (v != -1) {
        d->run_limit_enabled = 1;

        d->run_limit_ms = ((uint64_t)v) / 1000u;
    }

    uint32_t idx = 0;
    if (BIT(29) != 0) {
        static const uint32_t rate_table[13] = { 100000u, 33333u, 25000u, 16666u, 12500u, 1u,
                                                 10000u, 8333u, 6250u, 5000u, 4166u, 3125u, 2500u };
        uint32_t sel = FIELD(12, 4);
        if (sel <= 12)
            idx = rate_table[sel];
    }
    *(uint32_t *)(obj + 1164) = idx;

    audio_out_set_runtime_flag(BIT(31) ^ 1u);

    uint32_t low = (uint32_t)FRONTEND->config_bits;
    audio_out_set_enabled_flag((low >> 26) & 1u);
    #undef BIT
    #undef FIELD
}

typedef unsigned long long (*fn_strtoull)(const char *, char **, int);

void config_apply_hex_value_from_string(uint64_t param_1, unsigned char *param_2,
                         uint32_t param_3, char *param_4)
{
    (void)param_1;

    arm_debug_set_mode(param_2, param_3);

    if (param_4 != NULL) {
        unsigned long long v =
            ((fn_strtoull)sym_libc_strtoull)(param_4, NULL, 0x10);
        memcpy(param_2 + 272, &v, sizeof(v));
    }
}

typedef int (*fn_getopt_long)(int, char *const *, const char *,
                              const void *, int *);
typedef long (*fn_strtol)(const char *, char **, int);
typedef unsigned long long (*fn_strtoull_2)(const char *, char **, int);





static char *optarg_string(void *dir_optarg)
{
    return (char *)rd_ptr(dir_optarg);
}

void config_parse_args(uint8_t *param_1, int argc, char *const *argv)
{

    fn_getopt_long getopt_long_fn = (fn_getopt_long)sym_libc_getopt_long;
    fn_strtol convert_int = (fn_strtol)sym_libc_strtol;
    fn_strtoull_2 convert_hex = (fn_strtoull_2)sym_libc_strtoull;
    void *dir_optarg = (void *)&sym_libc_optarg;
    int idx;

    for (;;) {
        int result = getopt_long_fn(argc, argv, "",
                                     (const void *)long_options, &idx);

        if (result != 0) {
            if (result == -1)
                return;
            continue;
        }

        if ((uint32_t)idx > 13u)
            continue;

        switch ((uint32_t)idx) {
        case 0: {
            char *arg = optarg_string(dir_optarg);
            arm_debug_set_mode((unsigned char *)(&((nds_t *)param_1)->arm9.debug), 3);
            if (arg != NULL)
                ((nds_t *)param_1)->arm9.debug.argument =
                             convert_hex(arg, NULL, 16);
            break;
        }
        case 1: {
            char *arg = optarg_string(dir_optarg);
            arm_debug_set_mode((unsigned char *)(&((nds_t *)param_1)->arm7.debug), 4);
            if (arg != NULL)
                ((nds_t *)param_1)->arm7.debug.argument =
                             convert_hex(arg, NULL, 16);
            break;
        }
        case 2: {
            char *arg = optarg_string(dir_optarg);
            arm_debug_set_mode((unsigned char *)(&((nds_t *)param_1)->arm9.debug), 4);
            if (arg != NULL)
                ((nds_t *)param_1)->arm9.debug.argument =
                             convert_hex(arg, NULL, 16);
            break;
        }
        case 3: {
            char *arg = optarg_string(dir_optarg);
            arm_debug_set_mode((unsigned char *)(&((nds_t *)param_1)->arm7.debug), 0);
            if (arg != NULL)
                ((nds_t *)param_1)->arm7.debug.argument =
                             convert_hex(arg, NULL, 16);
            break;
        }
        case 4: {
            char *arg = optarg_string(dir_optarg);
            arm_debug_set_mode((unsigned char *)(&((nds_t *)param_1)->arm9.debug), 0);
            if (arg != NULL)
                ((nds_t *)param_1)->arm9.debug.argument =
                             convert_hex(arg, NULL, 16);
            break;
        }
        case 5:
            ((nds_t *)param_1)->runtime.jit_enabled = 1;
            break;
        case 6: {
            char *arg = optarg_string(dir_optarg);
            long value = convert_int(arg, NULL, 10);
            if (value == 0x20 || value == 0x10)
                ((nds_t *)param_1)->runtime.texture_format = (uint8_t)value;
            break;
        }
        case 7: {
            char *arg = optarg_string(dir_optarg);
            long value = convert_int(arg, NULL, 10);
            uint32_t adjust = ((nds_t *)param_1)->config.state_slot;
            benchmark_init((unsigned char *)(&((nds_t *)param_1)->benchmark), (nds_t *)param_1, value,
                                    0x7f, adjust, 1);
            ((nds_t *)param_1)->config.fast_forward = 1;
            break;
        }
        case 8: {
            char *arg = optarg_string(dir_optarg);
            long value = convert_int(arg, NULL, 10);
            uint32_t adjust = ((nds_t *)param_1)->config.state_slot;
            benchmark_init((unsigned char *)(&((nds_t *)param_1)->benchmark), (nds_t *)param_1, value,
                                    2, adjust, 1);
            ((nds_t *)param_1)->config.fast_forward = 1;
            break;
        }
        case 9:
            ((nds_t *)param_1)->config.fast_forward = 1;
            break;
        case 10:
            input_record_open_write_stream((unsigned char *)(&((nds_t *)param_1)->input_record),
                                     optarg_string(dir_optarg));
            break;
        case 11:
            input_record_load_playback_buffer((unsigned char *)(&((nds_t *)param_1)->input_record),
                                     optarg_string(dir_optarg));
            break;
        case 12:
            ((nds_t *)param_1)->runtime.jit_enabled = 0;
            break;
        case 13: {
            char *arg = optarg_string(dir_optarg);
            arm_debug_set_mode((unsigned char *)(&((nds_t *)param_1)->arm9.debug), 3);
            if (arg != NULL)
                ((nds_t *)param_1)->arm9.debug.argument =
                             convert_hex(arg, NULL, 16);
            break;
        }
        }
    }
}

uint64_t config_hook_noop(uint64_t x0)
{
    return x0;
}

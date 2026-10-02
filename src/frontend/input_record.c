#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <stddef.h>
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"

#define P_CORE     0x80008
#define S_BASE     0x80000
#define S_PTR      0
#define S_JMPBUF     8
#define S_FLAGS    16
#define S_TX       20
#define S_TY       24
#define S_TOUCH     28
#define S_VFLAGS   32
#define S_VTX      36
#define S_VTY      40
#define S_VTOUCH    44
#define S_FILE     72
#define S_MODE     80
#define S_AXES     48
#define S_AXIS4     64
#define S_HAS_AXES 60
#define S_HAS_AXIS4 68
#define HIST_LIMIT  0x7ffec
#define FORMAT    0x10fcc8
extern uint32_t script_is_active(void);
extern int      str_vsprintf_limit_2080_2(char *dest, long unused,
                                    const char *format, ...);
typedef void    *(*fn_malloc)(uint64_t);
typedef void     (*fn_free)(void *);
typedef uint64_t (*fn_fwrite)(const void *, uint64_t, uint64_t, void *);
typedef int      (*fn_fflush)(void *);








static void notify(arm_t *cpu, uint32_t bit) {
    io_mirror_t *r = cpu->io_mirror;
    uint32_t v = r->irq.if_pending | bit;
    r->irq.if_pending = (v);
    if ((uint8_t)cpu->halt_flags & 4) return;
    cpu->irq_pending = (r->irq.ie & v) & (0u - r->irq.ime);
}

static void check_keycnt(const uint8_t *reg, arm_t *cpu, uint32_t keys) {
    uint32_t cnt = rd16(reg + 2);
    if (!(cnt & (1u << 14))) return;
    uint32_t q = (keys & cnt) & 0x3ffu;
    if (cnt & (1u << 15)) { if (q == 0) return; }
    else                  { if (q != keys) return; }
    notify(cpu, 0x1000);
}

void input_record_end_of_frame(uint8_t *p) {

    uint8_t *C = (uint8_t *)rd64(p + P_CORE);
    uint32_t old = rd32(p + S_BASE + S_FLAGS);
    uint8_t *S = p + S_BASE;

    input_poll_and_pause(p);

    if (((nds_t *)C)->config.lua_enabled != 0 && script_is_active() != 0)
        script_run_on_frame_update();

    uint32_t f = rd32(S + S_FLAGS);

    if (S[S_MODE] == 2) {

        const uint8_t *q = rd_ptr(S + S_PTR);
        if (rd64(C) == (uint64_t)rd32(q)) {
            uint32_t v = rd32(q + 4);
            S[S_TOUCH] = (uint8_t)(v >> 31);
            f = v & 0x7fffffffu;
            wr32(S + S_TX, q[8]);
            wr32(S + S_FLAGS, f);
            wr_ptr(S + S_PTR, q + 10);
            wr32(S + S_TY, q[9]);
        }
    } else if (f != rd32(S + S_VFLAGS) || S[S_TOUCH] != S[S_VTOUCH]
               || (S[S_TOUCH] != 0 && (rd32(S + S_TX) != rd32(S + S_VTX)
                                   || rd32(S + S_TY) != rd32(S + S_VTY)))) {

        uint8_t *w = rd_ptr(S + S_PTR);
        if (w < p + HIST_LIMIT) {
            wr32(w, (uint32_t)rd64(C));
            wr32(w + 4, (f & 0x7fffffffu) | ((uint32_t)S[S_TOUCH] << 31));
            w[8] = (uint8_t)rd32(S + S_TX);
            w[9] = (uint8_t)rd32(S + S_TY);
            void *fi = (void *)rd64(S + S_FILE);
            if (fi) {
                ((fn_fwrite)sym_libc_fwrite)(w, 10, 1, fi);
                ((fn_fflush)sym_libc_fflush)((void *)rd64(S + S_FILE));
            }
            wr_ptr(S + S_PTR, w + 10);
        }

        wr32(S + S_VFLAGS, f);
        S[S_VTOUCH] = S[S_TOUCH];
        wr64(S + S_VTX, rd64(S + S_TX));
    }

    if (f & (1u << 28)) {
        arm_debug_set_mode((unsigned char *)(&((nds_t *)C)->arm9.debug), 0);
        wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xefffffffu);
    }
    if (f & (1u << 29)) {
        arm_debug_set_mode((unsigned char *)(&((nds_t *)C)->arm7.debug), 0);
        wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xdfffffffu);
    }

    nds_config_t *req = &((nds_t *)C)->config;
    arm_t *cpu7 = &((nds_t *)C)->arm7;
    arm_t *irq_a = &((nds_t *)C)->arm9;


    int resume = 1;
    if (f & (1u << 19)) {
        void *a = ((fn_malloc)sym_libc_malloc)(0x18000);
        void *b = ((fn_malloc)sym_libc_malloc)(0x18000);
        video_out_dump_screen_rgb565(a, 0);
        video_out_dump_screen_rgb565(b, 1);
        state_save_slot(C, req->state_slot, a, b);
        ((fn_free)sym_libc_free)(a);
        ((fn_free)sym_libc_free)(b);
        wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xfff7ffffu);
    }
    if (f & (1u << 20)) {
        wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xffefffffu);
        if (((nds_t *)C)->arm9.debug.mode == 7) arm_debug_set_mode((unsigned char *)(&((nds_t *)C)->arm9.debug), 0);
        if (cpu7->debug.mode == 7) arm_debug_set_mode((unsigned char *)(&cpu7->debug), 0);

        if (state_load_slot(C, req->state_slot, 0, 0, 0) == 0) resume = 0;
    }

    if (resume) {
        if (f & (1u << 18)) {
            wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xfffbffffu);
            mirror_pause_hook_noop();
            return;
        }
        if (f & (1u << 25)) {
            wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xfdffffffu);
            mirror_pause_hook_noop();
            return;
        }
        if (f & (1u << 21)) {
            wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xffdfffffu);
            if (req->fast_forward != 0) {
                req->fast_forward = 0;
            } else {
                ((nds_t *)C)->runtime.state_marker = 1;
                req->fast_forward = 1;
            }
        }
        if (f & (1u << 22)) {
            wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xffbfffffu);
            uint32_t v = req->input_toggle ^ 1u;
            req->input_toggle = v;
            video_out_option_toggle_hook_noop();
        }
        if (f & (1u << 23)) {
            wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xff7fffffu);
            uint32_t v = rd32(req) ^ 1u;
            wr32(req, v);
            video_out_layer_toggle_hook_noop();
        }
        if (f & (1u << 24)) {
            wr32(S + S_FLAGS, rd32(S + S_FLAGS) & 0xfeffffffu);
            uint32_t v = rd32(req) ^ 2u;
            wr32(req, v);
            video_out_layer_toggle_hook_noop();
        }
        if (f & (1u << 26)) mirror_fatal_reset_longjmp((void *)rd64(S + S_JMPBUF));

        if (f & (1u << 27)) {

            if (!(old & (1u << 27))) spu_recording_start(&((nds_t *)C)->spu);
        } else {

            if (old & (1u << 27)) spu_recording_stop(&((nds_t *)C)->spu);
        }

        uint32_t k = 0;
        k |= ((f >> 4) & 3u);
        k |= ((f >> 3) & 1u) << 4;
        k |= ((f >> 2) & 1u) << 5;
        k |= ((f >> 0) & 1u) << 6;
        k |= ((f >> 1) & 1u) << 7;
        k |= ((f >> 9) & 1u) << 8;
        k |= ((f >> 8) & 1u) << 9;
        k |= ((f >> 10) & 1u) << 3;
        k |= ((f >> 11) & 1u) << 2;

        uint32_t e = 0xff00u;
        if (f & 0x40u) e += 1;
        e |= (f >> 6) & 2u;
        if (!(f & 0x1000u)) e |= 0x80u;
        if (S[S_TOUCH] != 0) {
            e |= 0x40u;

            spi_tsc_set_position(&((nds_t *)C)->spi.tsc, rd32(S + S_TX), rd32(S + S_TY));
        }

        if (S[S_HAS_AXES] != 0) {
            cart_sensor_write_accel_axes_8bit(rd_f32(S + S_AXES), rd_f32(S + S_AXES + 4),
                                rd_f32(S + S_AXES + 8), &((nds_t *)C)->cart.slot2.sensor);
            cart_sensor_write_accel_axes_16bit(rd_f32(S + S_AXES), rd_f32(S + S_AXES + 4),
                                rd_f32(S + S_AXES + 8), &((nds_t *)C)->cart.slot2.sensor);
        }
        if (S[S_HAS_AXIS4] != 0) {
            slot2_sensor_t *sensor = &((nds_t *)C)->cart.slot2.sensor;
            cart_sensor_write_gyro_16bit(rd_f32(S + S_AXIS4), sensor);
        }

        uint8_t *ta = (uint8_t *)&((nds_t *)C)->bus.io_mirror[0].keyinput;
        uint8_t *tb = (uint8_t *)&((nds_t *)C)->bus.io_mirror[1].keyinput;
        check_keycnt(ta, irq_a, k);
        check_keycnt(tb, cpu7, k);

        wr16(ta, (uint16_t)(k ^ 0x3ffu));
        wr16(tb, (uint16_t)(k ^ 0x3ffu));
        wr16(tb + 6, (uint16_t)~e);

        if ((old & (1u << 12)) && !(f & (1u << 12)))
            notify(cpu7, 0x400000);
    }
}
#undef P_CORE
#undef S_BASE
#undef S_PTR
#undef S_JMPBUF
#undef S_FLAGS
#undef S_TX
#undef S_TY
#undef S_TOUCH
#undef S_VFLAGS
#undef S_VTX
#undef S_VTY
#undef S_VTOUCH
#undef S_FILE
#undef S_MODE
#undef S_AXES
#undef S_AXIS4
#undef S_HAS_AXES
#undef S_HAS_AXIS4
#undef HIST_LIMIT
#undef FORMAT

void input_record_reset_state(unsigned char *obj, void *ctx) {
    nds_input_record_t *b = (nds_input_record_t *)obj;
    b->machine = (struct nds *)ctx;
    b->mode = 0;
    b->file = 0;

    input_state_reset();
}

extern void *files_fopen_resolved(const char *path, const char *mode);

void input_record_open_write_stream(unsigned char *param_1, const char *param_2) {

    void *f = nds_platform_default()->files.open(nds_platform_default()->user, param_2, "wb");

    ((nds_input_record_t *)param_1)->file = f;

    if (f != 0) {
        ((nds_input_record_t *)param_1)->mode = 1;
    }
}

static long   (*core_fseek)(void *, long, int);
static long   (*core_ftell)(void *);
static size_t (*core_fread)(void *, size_t, size_t, void *);
static int    (*core_fclose)(void *);

int32_t input_record_load_playback_buffer(unsigned char *param_1, const char *param_2) {
    if (!core_fseek) {
        core_fseek  = (long (*)(void *, long, int))sym_libc_fseek;
        core_ftell  = (long (*)(void *))sym_libc_ftell;
        core_fread  = (size_t (*)(void *, size_t, size_t, void *))sym_libc_fread;
        core_fclose = (int (*)(void *))sym_libc_fclose;
    }
    const char *mode = "rb";

    void *f = nds_platform_default()->files.open(nds_platform_default()->user, param_2, mode);
    int32_t iVar1 = 0;
    if (f != NULL) {
        core_fseek(f, 0, 2);
        long file_size = core_ftell(f);
        core_fseek(f, 0, 0);

        uint32_t size32 = (uint32_t)file_size;
        size_t size = (size32 > 0x7ffce) ? (size_t)0x7ffce : (size_t)size32;

        core_fread(param_1, size, 1, f);
        iVar1 = (int32_t)core_fclose(f);

        uint32_t zero = 0;
        memcpy(param_1 + size, &zero, 4);
        param_1[0x80050] = 2;
    }
    return iVar1;
}

void input_record_close_write_stream(unsigned char *obj) {
    if (((nds_input_record_t *)obj)->mode != 1) return;
    void *f;
    f = ((nds_input_record_t *)obj)->file;
    if (!f) return;
    ((int (*)(void *))sym_libc_fclose)(f);
}

static long   (*core_ftell_5)(void *);
static int    (*core_fseek_5)(void *, long, int);
static size_t (*core_fread_5)(void *, size_t, size_t, void *);
static int    (*core_fclose_5)(void *);

long input_record_load(unsigned char *obj) {
    if (!core_ftell_5) {
        core_ftell_5  = (long (*)(void *))sym_libc_ftell;
        core_fseek_5  = (int (*)(void *, long, int))sym_libc_fseek;
        core_fread_5  = (size_t (*)(void *, size_t, size_t, void *))sym_libc_fread;
        core_fclose_5 = (int (*)(void *))sym_libc_fclose;
    }
    int  (*arm)(char *, long, const char *, ...) = str_vsprintf_limit_2080_2;
    int (*query)(const char *, void *) = platform_file_stat;
    void *(*open_fn)(const char *, const char *) = platform_file_open;
    void (*clear)(void) = input_state_reset;

    nds_input_record_t *rec = (nds_input_record_t *)obj;
    unsigned char *ctx = (unsigned char *)rec->machine;

    char path[0x800];
    unsigned char info[128];

    arm(path, 0, "%s%cinput_record%c%s.ir",
          ((nds_t *)ctx)->cache_dir, '/', '/', ((nds_t *)ctx)->rom_name);

    if (query(path, info) == 0) {
        void *f = open_fn(path, "rb");
        if (f != 0) {
            core_fseek_5(f, 0, 2);
            uint64_t file_size = (uint64_t)core_ftell_5(f);
            core_fseek_5(f, 0, 0);

            uint32_t low = (uint32_t)file_size;
            uint64_t n = (low > 0x7ffceu) ? 0x7ffceu : (uint64_t)low;
            core_fread_5(obj, (size_t)n, 1, f);
            core_fclose_5(f);
            *(uint32_t *)(obj + n) = 0;
            rec->mode = 2;
        }
    }

    rec->cursor = obj;
    rec->has_axes = 0;

    memset(&rec->flags, 0, 14);

    clear();
    return 0;
}

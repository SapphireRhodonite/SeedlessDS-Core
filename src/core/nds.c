#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "cpu/jit-arm64/jit_hooks.h"
#include "core/data_paths.h"
#include "rtc.h"
#include "core/nds_state.h"
#include "core/nds_start.h"
#include "frontend/script/script_state.h"

void audio_out_unmute_and_resume(spu_t *spu);
void *nds_clear_state_blocks(unsigned char *obj);
nds_t nds_machine __attribute__((aligned(0x4000)));
#define CPU(p) ((arm_t *)(p))
#include <string.h>
#include <stdarg.h>
#include "platform/android/audio.h"
#include "frontend/archive/progress.h"
#include "core_internals.h"
#include "mem_access.h"
static int (*core_mprotect)(void *, size_t, int);
static int (*core_setjmp)(void *);

int nds_start_game(nds_start_params_t *params) {
    if (!core_mprotect) {
        core_mprotect = (int (*)(void *, size_t, int))sym_libc_mprotect;
        core_setjmp   = (int (*)(void *))sym_libc__setjmp;
    }
    unsigned char *G = (unsigned char *)&nds_machine;
    platform_audio_t *ctl = PLATFORM_AUDIO;

    if (ctl->machine == 0) {
        ctl->machine = (struct nds *)G;
        if (core_mprotect(G, sizeof nds_machine, 7) != 0)
            return -1;
    }

    const char *fmt = "%s";
    int (*arm)(char *, long, const char *, ...) = str_vsprintf_limit_1024;

    *params->machine_slot = (struct nds *)G;

    arm(((nds_t *)G)->cache_dir, 0, fmt, params->cache_dir);
    arm(((nds_t *)G)->save_dir, 0, fmt, params->save_dir);

    ((nds_t *)G)->bus.address_window = (uint8_t *)(uintptr_t)(uint64_t)-1;
    nds_machine_init(G);

    uint32_t mode = params->texture_format;
    ((nds_t *)G)->runtime.texture_format = (unsigned char)mode;

    uint32_t n = params->benchmark_frames;
    if (n != 0) {
        benchmark_init((unsigned char *)&((nds_t *)G)->benchmark, (nds_t *)G, n, 127, params->benchmark_slot, 1);
        ((nds_t *)G)->config.fast_forward = 1;
        mode = ((nds_t *)G)->runtime.texture_format;
    }
    video_out_gl_set_texture_format(mode);

    if (cart_open_rom(
            &nds_machine.cart, (char *)params->rom_path) != 0)
        return -1;

    const unsigned char *src = params->config;
    nds_config_t *dst = &((nds_t *)G)->config;
    for (int i = 0; i < 4; i++)
        dst->nickname[i] = ((const uint16_t *)(src + 16))[i];
    for (int i = 0; i < 4; i++)
        dst->nickname[4 + i] = ((const uint16_t *)(src + 24))[i];
    dst->nickname[8] = *(const uint16_t *)(src + 32);
    dst->nickname[9] = *(const uint16_t *)(src + 34);
    for (int i = 0; i < 16; i++) ((unsigned char *)&dst->language)[i] = src[i];

    video_out_gl_init_hook_noop();
    nds_machine_reset(G);

    core_setjmp(((nds_t *)G)->fatal_jump);

    if (ctl->restart_pending == 1) { ctl->restart_pending = 0; return 0; }

    if (params->benchmark_frames != 0) ctl->restart_pending = 1;
    if (((nds_t *)G)->runtime.jit_enabled != 0)
        jit_hook_register_dual_slot(G, ((nds_t *)G)->bus.address_window);
    else
        sched_dispatch_next(G);
    return 0;
}

void nds_machine_init(unsigned char *obj) {

    script_lua_init(obj);
    script_io_init();
    config_unpack_word((unsigned char *)&((nds_t *)obj)->config);

    unsigned char *a = (unsigned char *)&((nds_t *)obj)->arm9;
    unsigned char *b = (unsigned char *)&((nds_t *)obj)->arm7;
    arm_init(a, obj, 1, b);
    arm_init(b, obj, 0, a);

    jit_arena_protect_rwx((unsigned char *)&((nds_t *)obj)->jit_arena);
    input_record_reset_state((unsigned char *)&((nds_t *)obj)->input_record, obj);

    unsigned char *map = (unsigned char *)&((nds_t *)obj)->bus;
    if (mirror_memory_subsystem_create(map, obj) < 0) {
        mirror_fatal_reset_longjmp(obj);
        return;
    }

    gpu2d_worker_init((unsigned char *)&((nds_t *)obj)->gpu, map);
    spu_init_channels_and_noise(&((nds_t *)obj)->spu, obj);
    cart_backup_init_paths(&((nds_t *)obj)->cart, obj);
    firmware_mount(&((nds_t *)obj)->spi, (nds_t *)obj);
    rtc_attach((void **)&((nds_t *)obj)->rtc.machine, obj);
    sched_init(&((nds_t *)obj)->sched, obj);

    ((nds_t *)obj)->runtime.jit_enabled = 1;
    ((nds_t *)obj)->runtime.texture_format = 32;
    ((nds_t *)obj)->benchmark.pass_mask = 0;
    ((nds_t *)obj)->rom_file_name[0] = 0;
    ((nds_t *)obj)->rom_name[0] = 0;
    ((nds_t *)obj)->benchmark.enabled = 0;
    #undef LL
}

void nds_machine_reset(unsigned char *machine) {

    int (*format)(char *, long, const char *, ...) = str_vsprintf_limit_1024;
    int (*note)(void *, const char *, uint32_t) = mirror_query_unsupported;
    void (*descriptors)(unsigned char *) = arm_reset;
    void (*clock)(uint64_t *)    = time_now_microseconds;
    void *(*reserve)(void *, uint32_t) = jit_cache_lookup_or_compile;

    nds_runtime_t *a = &((nds_t *)machine)->runtime;
    arm_t *arm9 = &((nds_t *)machine)->arm9;
    arm_t *arm7 = &((nds_t *)machine)->arm7;
    nds_config_t *cfg = &((nds_t *)machine)->config;

    char path[0x410];
    format(path, 0, DATA_CONFIG_PER_GAME_FMT,
             ((nds_t *)machine)->rom_name);
    note(machine, DATA_CONFIG_GLOBAL, 0);
    note(machine, path, 1);

    unsigned char *m1 = (unsigned char *)&((nds_t *)machine)->arm9;
    descriptors(m1);
    unsigned char *m2 = (unsigned char *)&((nds_t *)machine)->arm7;
    descriptors(m2);

    nds_clear_state_blocks(m1);
    jit_arena_regions_init((unsigned char *)&((nds_t *)machine)->jit_arena);
    mem_console_reset((uint8_t *)&((nds_t *)machine)->bus);
    bus_map_reset((unsigned char *)&((nds_t *)machine)->gpu);

    cart_t *e = &((nds_t *)machine)->cart;
    cart_backup_reset_zone_defaults(e);
    spi_reset(&((nds_t *)machine)->spi);

    spu_t *f = &((nds_t *)machine)->spu;
    spu_reset_slots_and_load_config(f);
    input_record_load((unsigned char *)&((nds_t *)machine)->input_record);

    {   uint32_t p1 = cfg->run_limit_enabled;
        uint64_t p2;
        __builtin_memcpy(&p2, &cfg->run_limit_ms, 8);
        rtc_reset(&((nds_t *)machine)->rtc, (int)p1, (long)p2);
    }

    sched_deadlines_clear(&((nds_t *)machine)->sched);

    ((nds_t *)machine)->sched.frame = 0;
    ((nds_t *)machine)->sched.cycles = 0;
    ((nds_t *)machine)->sched.scanline = SCHED_SCANLINE_RESET;
    ((nds_t *)machine)->sched.slice_cycles = 0;

    platform_doublebuffer_flip_signal();

    sched_event_insert(machine, NULL);
    sched_scanline_event(machine, NULL);
    audio_out_unmute_and_resume(f);
    cart_boot_from_header(e);

    gamedb_apply_fixes(machine);

    uint8_t *rest;
    if (a->jit_enabled != 0) {
        uint32_t base = arm9->cp15->exception_vector_base;
        arm9->run_entry = (uint64_t (*)(void *))jit_block_end_switch_processor;
        arm7->run_entry = (uint64_t (*)(void *))jit_block_end_run_scheduler;

        arm9->jit_swi_entry = reserve(m1, base + 8);
        arm9->jit_irq_entry = reserve(m1, base + 0x18);
        arm7->jit_swi_entry = reserve(m2, 8);
        arm7->jit_irq_entry = reserve(m2, 0x18);
        arm9->jit_block = (uint8_t *)reserve(m1, arm9->pc) + 8;
        rest = (uint8_t *)reserve(m2, arm7->pc) + 8;
    } else {
        arm9->jit_block = 0;
        arm9->run_entry = (uint64_t (*)(void *))sched_dispatch_arm7;
        arm7->run_entry = (uint64_t (*)(void *))sched_dispatch_next;
        rest = NULL;
    }
    arm7->jit_block = rest;

    gpu2d_worker_hook_noop();

    uint64_t now;
    clock(&now);
    a->pacer_flag = 0;
    *(uint32_t *)&a->skip_frame = 0;
    a->pacer_base = now + now * 2;
    a->pacer_accumulated = 0;
}

long nds_subsystem_notify_shutdown_pair(unsigned char *obj) {
    void *a = PLATFORM_AUDIO->record_itf;
    if (a == 0)
        return (long)(intptr_t)obj;

    obj[0x40023] = 0;
    obj[0x40025] = 0;

    void **vt_a = *(void ***)a;
    long (*m0)(void *, long) = (long (*)(void *, long))vt_a[0];
    m0(a, 1);

    void *b = PLATFORM_AUDIO->recorder_queue;
    void **vt_b = *(void ***)b;
    long (*m1)(void *) = (long (*)(void *))vt_b[1];
    long r = m1(b);

    PLATFORM_AUDIO->mic_started = 0;
    return r;
}

void nds_subsystem_hook_noop(void) {
}

int nds_frame_limiter_measure_returns_zero(void) {
    return 0;
}





static void *vt_slot(void *vt, unsigned n) {
    return rd_ptr((const uint8_t *)vt + (size_t)n * 8);
}
typedef void  (*fn_this)(void *this_);
typedef void  (*fn_this_i)(void *this_, uint32_t a1);
typedef void *(*fn_memset)(void *, int, size_t);

void nds_subsystem_shutdown_release_objects(void)
{

    if (PLATFORM_AUDIO->active == 0)
        return;

    void *obj_b = PLATFORM_AUDIO->play_itf;
    PLATFORM_AUDIO->active = 0;
    void *vt_b  = rd_ptr(obj_b);
    ((fn_this_i)vt_slot(vt_b, 0))(obj_b, 1);

    void *obj_a = PLATFORM_AUDIO->player_queue;
    void *vt_a  = rd_ptr(obj_a);
    ((fn_this)vt_slot(vt_a, 1))(obj_a);

    ((fn_memset)sym_libc_memset)(PLATFORM_AUDIO->buffers, 0,
                                       sizeof PLATFORM_AUDIO->buffers);

    void *obj_e = PLATFORM_AUDIO->player;
    if (obj_e != NULL) {
        void *vt_e = rd_ptr(obj_e);
        ((fn_this)vt_slot(vt_e, 6))(obj_e);
        PLATFORM_AUDIO->volume_itf = 0;
        PLATFORM_AUDIO->play_itf = 0;
        PLATFORM_AUDIO->player_queue = 0;
        PLATFORM_AUDIO->player = 0;
    }

    void *obj_f = PLATFORM_AUDIO->recorder;
    if (obj_f != NULL) {
        void *obj_g = PLATFORM_AUDIO->record_itf;
        if (obj_g != NULL) {

            void *vt_g = rd_ptr(obj_g);
            ((fn_this_i)vt_slot(vt_g, 0))(obj_g, 1);

            void *obj_h = PLATFORM_AUDIO->recorder_queue;
            void *vt_h  = rd_ptr(obj_h);
            ((fn_this)vt_slot(vt_h, 1))(obj_h);
        }

        void *obj_f2 = PLATFORM_AUDIO->recorder;
        void *vt_f   = rd_ptr(obj_f2);
        ((fn_this)vt_slot(vt_f, 6))(obj_f2);

        PLATFORM_AUDIO->recorder = 0;
        PLATFORM_AUDIO->record_itf = 0;
        PLATFORM_AUDIO->recorder_queue = 0;
    }

    void *obj_i = PLATFORM_AUDIO->output_mix;
    if (obj_i != NULL) {
        void *vt_i = rd_ptr(obj_i);
        ((fn_this)vt_slot(vt_i, 6))(obj_i);
        PLATFORM_AUDIO->output_mix = 0;
    }

    void *obj_j = PLATFORM_AUDIO->engine;
    if (obj_j == NULL)
        return;

    void *vt_j = rd_ptr(obj_j);
    ((fn_this)vt_slot(vt_j, 6))(obj_j);
    PLATFORM_AUDIO->engine = 0;
    PLATFORM_AUDIO->engine_itf = 0;
}

uint64_t nds_subsystem_hook_noop_1(uint64_t x0)
{
    return x0;
}

uint64_t nds_subsystem_hook_noop_2(uint64_t x0)
{
    return x0;
}

uint64_t nds_subsystem_hook_noop_3(uint64_t x0)
{
    return x0;
}

uint64_t nds_subsystem_hook_noop_4(uint64_t x0)
{
    return x0;
}

void audio_out_unmute_and_resume(spu_t *spu) {
    spu->mixer.muted = 0;
    audio_out_restart_playback();
}

static void *slot0(void *obj) {
    void **table = *(void ***)obj;
    return table[0];
}

void audio_out_restart_playback(void) {
    platform_audio_t *G = PLATFORM_AUDIO;

    if (G->active == 0)
        return;


    G->buffer_index = 0;
    G->accumulated = 0;
    G->queued = 0;
    G->mic_total = 0;
    G->mic_slot = 0;
    {
        void *obj = G->play_itf;
        long (*m)(void *, long) = (long (*)(void *, long))slot0(obj);
        m(obj, 3);
    }

    if (G->buffer_count != 0) {
        unsigned char *elem = G->buffers;
        for (uint64_t i = 0;; i++) {
            G->buffer_fill[i] = 0;
            void *obj = G->player_queue;
            uint32_t count = G->buffer_samples;
            long (*m)(void *, void *, uint32_t) =
                (long (*)(void *, void *, uint32_t))slot0(obj);
            m(obj, elem, count << 1);

            uint32_t n = G->buffer_count;
            elem += PLATFORM_AUDIO_BUFFER_BYTES;
            if (i + 1 >= (uint64_t)n) break;
        }
    }

    if (G->mic_started != 0 && G->record_itf != 0) {
        void *dst = G->recorder_queue;
        long (*m)(void *, void *, uint32_t) =
            (long (*)(void *, void *, uint32_t))slot0(dst);
        m(dst, G->mic_buffers[0], PLATFORM_AUDIO_MIC_BUFFER_BYTES);

        dst = G->recorder_queue;
        m = (long (*)(void *, void *, uint32_t))slot0(dst);
        m(dst, G->mic_buffers[1], PLATFORM_AUDIO_MIC_BUFFER_BYTES);

        G->mic_total = 2;
        void *obj = G->record_itf;
        long (*m2)(void *, long) = (long (*)(void *, long))slot0(obj);
        m2(obj, 3);
    }

    {
        void *obj = G->volume_itf;
        uint32_t v = G->volume_millibel;
        long (*m)(void *, uint32_t) = (long (*)(void *, uint32_t))slot0(obj);
        m(obj, v);
    }
    G->closed = 0;
}
static void *(*core_memset)(void *, int, size_t);
static int   (*core_mprotect_13)(void *, size_t, int);

void *nds_clear_state_blocks(unsigned char *obj) {
    if (!core_memset) {
        core_memset   = (void *(*)(void *, int, size_t))sym_libc_memset;
        core_mprotect_13 = (int (*)(void *, size_t, int))sym_libc_mprotect;
    }

    jit_arena_t *A = CPU(obj)->jit_arena;
    unsigned char *B = (unsigned char *)CPU(obj)->bus;
    unsigned char *C = (unsigned char *)((arm_t *)obj)->partner;
    unsigned char *b22 = B + 0xEF1B0;
    unsigned char *b23 = B + 0xAF170;

    core_memset(A->main_ram_blocks, 0, JIT_TABLES_RESET_BYTES);
    core_memset(A->other_blocks, 0, JIT_OTHER_TABLES_RESET_BYTES);

    core_memset(CPU(obj)->itcm_arm_blocks, 0, ARM_ITCM_ARM_BLOCKS * sizeof(uint32_t));
    core_memset(A->itcm_arm_hits, 0, sizeof A->itcm_arm_hits);

    core_memset(CPU(obj)->itcm_thumb_blocks, 0, ARM_ITCM_THUMB_BLOCKS * sizeof(uint32_t));

    core_memset(obj + 0x80, 0, 0x2000);
    core_memset(C   + 0x80, 0, 0x2000);

    core_memset(B + 0xAF070, 0, 256);

    *(uint64_t *)(b23 + 8)  = 0;
    *(uint32_t *)(b23 + 16) = 0;
    core_memset(B + 0xAF184, 0, 44);

    core_memset(B + 0xAF1B0, 0, 0x40000);

    if (*(void **)b22 != 0) {
        core_memset(*(void **)b23, 0, 0x800);
        core_memset(*(void **)b22, 0, 0x200000);
    }

    core_memset(B + 0xEF1B8, 0, 0x800);
    core_memset(B + 0xEF9B8, 0, 0x800);
    core_memset(B + 0xF01B8, 0, 0x1000);
    core_memset(B + 0xF11B8, 0, 0xa400);

    core_mprotect_13(A->code_main_ram, JIT_CODE_MAIN_RAM_SIZE, 7);
    core_mprotect_13(A->code_itcm,     JIT_CODE_ITCM_SIZE, 7);
    core_mprotect_13(A->code_other,    JIT_CODE_OTHER_SIZE, 7);

    core_memset(A->itcm_arm_hits, 0, sizeof A->itcm_arm_hits);
    A->itcm_variant_count = 0;
    return core_memset(A->main_ram_written_words, 0, sizeof A->main_ram_written_words);
}
static int    (*core_fseek)(void *, long, int);
static size_t (*core_fwrite)(const void *, size_t, size_t, void *);
static int    (*core_fflush)(void *);
static int    (*core_fclose)(void *);
static void   (*core_free)(void *);
static int    (*core_close)(int);

int nds_close_files(cart_t *cart) {
    if (!core_fseek) {
        core_fseek  = (int (*)(void *, long, int))            sym_libc_fseek;
        core_fwrite = (size_t (*)(const void *, size_t, size_t, void *)) sym_libc_fwrite;
        core_fflush = (int (*)(void *))                        sym_libc_fflush;
        core_fclose = (int (*)(void *))                        sym_libc_fclose;
        core_free   = (void (*)(void *))                       sym_libc_free;
        core_close  = (int (*)(int))                           sym_libc_close;
    }

    if (cart->backup.save_countdown != 0) {
        spi_memory_save_file(&cart->backup);
    }

    if (cart->slot2.save_countdown != 0) {
        if (cart->slot2.loaded != 0) {
            if (cart->slot2.save != 0) {
                void *stream = cart->slot2.file;
                if (stream != 0) {
                    core_fseek(stream, 0, 0);
                    core_fwrite(cart->slot2.save, (size_t)cart->slot2.save_size, 1, stream);
                    core_fflush(stream);
                }
            }
        }
    }

    if (cart->backup_data != 0) {
        core_free(cart->backup_data);
        cart->backup_data = 0;
    }

    spi_memory_close_file(&cart->backup);

    if (cart->slot2.file != 0) {
        core_fclose(cart->slot2.file);
        cart->slot2.file = 0;
    }

    archive_rom_reader_destroy((uint8_t *)(cart->rom));
    cart->rom = 0;

    int32_t fd = cart->argv_fd;
    if (fd >= 0) {
        return core_close(fd);
    }
    return fd;
}


void arm_debug_set_mode(unsigned char *obj, uint32_t v) {
    ((arm_debug_t *)obj)->mode_shadow = (unsigned char)v;
    ((arm_debug_t *)obj)->mode = (unsigned char)v;
}

void arm_debug_init(arm_debug_t *debug, arm_t *owner) {
    debug->owner = owner;
    *(uint32_t *)&debug->version = ARM_DEBUG_VERSION;
}

void arm_debug_reset(arm_debug_t *debug) {
    debug->count_b = 0;
    debug->instruction_count = 0;
    debug->count_a = 0;
}

uint64_t nds_hook_noop_identity(uint64_t x0)
{
    return x0;
}
int nds_sprintf_limited(char *dest, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, size, format, ap);
    va_end(ap);
    return r;
}
unsigned long nds_loading_percent(void) {
    uint64_t den = ((volatile archive_progress_t *)ARCHIVE_PROGRESS)->total;
    if (den == 0) return 0;
    return (unsigned long)((((volatile archive_progress_t *)ARCHIVE_PROGRESS)->done * 100ULL) / den);
}
#define TARGET_OFF 0x974e0u

void nds_vtable_slot_init_single(unsigned char *param_1) {

    uint64_t v = (uint64_t)archive_7z_window_sink_write;
    memcpy(param_1, &v, 8);
}
#undef TARGET_OFF
#undef TARGET_OFF

#define RA_OFF_MACHPTR  0x3f1e0b0UL
#define RA_OFF_ARENA    0xFD4E0UL

void *recon_main_ram(void)
{
    void *machine; memcpy(&machine, &script_state.bus, sizeof machine);
    if (!machine || machine == (void *)-1) return 0;
    void *arena; memcpy(&arena, (uint8_t *)machine + RA_OFF_ARENA, sizeof arena);
    if (!arena || arena == (void *)-1 || (uintptr_t)arena < 0x10000) return 0;
    return arena;
}
#undef RA_OFF_MACHPTR
#undef RA_OFF_ARENA

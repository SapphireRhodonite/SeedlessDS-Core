#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "platform/android/audio.h"
#include "seedlessds/platform.h"
#include "frontend/audio_out.h"
#include "frontend/mic_default_sample.h"
#include "frontend/audio_out_silence.h"
#include "mem_access.h"

void audio_out_unmute_and_resume(spu_t *spu);

int32_t audio_out_mode = 2;
uint32_t audio_out_enabled = 1;
float audio_out_volume = 4.0f;

#define BUFFER_OFF       0x107290UL




typedef uint32_t (*fn_vtable0)(void *this_, const void *buffer, uint32_t len);

void audio_out_queue_callback(void *caller, void *context) {
    unsigned int flag;
    unsigned int increment;
    uint64_t accumulator;
    unsigned int counter;
    void *obj;
    void *vtable;
    fn_vtable0 method;

    flag = PLATFORM_AUDIO->closed;
    if (flag != 0) {
        return;
    }

    increment = PLATFORM_AUDIO->buffer_samples;
    accumulator = PLATFORM_AUDIO->accumulated;
    accumulator = accumulator + (uint64_t)increment;
    PLATFORM_AUDIO->accumulated = accumulator;

    counter = PLATFORM_AUDIO->queued;

    if ((int)counter > 0) {
        PLATFORM_AUDIO->queued = counter - 1;
        return;
    }

    obj = PLATFORM_AUDIO->player_queue;
    vtable = rd_ptr(obj);
    method = (fn_vtable0)rd_ptr(vtable);

    method(obj, audio_out_silence, increment << 1);
}
#undef BUFFER_OFF

void audio_out_set_mode_word(uint32_t a0) {
    audio_out_mode = (int32_t)a0;
}

void audio_out_set_runtime_flag(uint32_t a0) {
    PLATFORM_AUDIO->mic_enabled = a0;
}

void audio_out_set_enabled_flag(uint32_t a0) {
    audio_out_enabled = a0;
}

void audio_out_set_volume_level(int32_t level) {

    int32_t v = (level < 3) ? level : 3;
    v = v & ~(v >> 31);

    static const uint8_t volume_levels[4] = { 0x02, 0x04, 0x08, 0x10 };
    uint8_t raw = volume_levels[(uint32_t)v];
    audio_out_volume = (float)raw;
}

long audio_out_set_runtime_word(long a0) {
    PLATFORM_AUDIO->sound_disabled = (uint32_t)a0;
    return a0;
}

static void *method(void *obj, int shift) {
    unsigned char **table = *(unsigned char ***)obj;
    return *(void **)((unsigned char *)table + shift);
}

int audio_out_start(unsigned char *machine) {

    platform_audio_t *g = PLATFORM_AUDIO;
    unsigned char *out = (unsigned char *)&((spu_t *)machine)->mixer.output_capacity;

    ((void *(*)(void *, int, unsigned long))sym_libc_memset)(g, 0, 0x1e038);

    static const uint32_t audio_out_buffer_count[4]   = { 4u, 4u, 3u, 4u };
    static const uint32_t audio_out_buffer_samples[4] = { 1470u, 2940u, 5880u, 5880u };

    int32_t which = audio_out_mode;
    uint32_t a, b;
    if ((uint32_t)which > 3) {
        a = 3; b = 0x16f8;
    } else {
        a = audio_out_buffer_count[which];
        b = audio_out_buffer_samples[which];
    }
    g->buffer_count = a;
    g->buffer_samples = b;
    g->mic_started = 0;

    ((void (*)(void *, uint32_t, void *, uint32_t, void *, void *))sym_libc_slCreateEngine)(
        &g->engine, 0, 0, 0, 0, 0);

    void *engine = g->engine;
    ((void (*)(void *, uint32_t))method(engine, 0))(engine, 0);

    engine = g->engine;
    ((void (*)(void *, SLInterfaceID, void *))method(engine, 24))(
        engine, SL_IID_ENGINE, &g->engine_itf);

    void *im = g->engine_itf;
    ((void (*)(void *, void *, uint32_t, void *, void *))method(im, 56))(
        im, &g->output_mix, 0, 0, 0);

    void *mix = g->output_mix;
    ((void (*)(void *, uint32_t))method(mix, 0))(mix, 0);

    unsigned char m[128];
    static const uint32_t pcm_format_head[4] = { 2u, 2u, 44100000u, 16u };
    static const uint32_t pcm_format_tail[4] = { 16u, 16u, 3u, 2u };
    __builtin_memcpy(m + 64, pcm_format_head, 16);
    __builtin_memcpy(m + 76, pcm_format_tail, 16);
    *(uint32_t *)(m + 32) = 4;
    *(void **)(m + 48) = m + 96;
    *(void **)(m + 56) = m + 64;
    *(void **)(m + 16) = m + 32;
    *(uint64_t *)(m + 24) = 0;
    *(void **)(m + 40) = g->output_mix;
    *(uint32_t *)(m + 96) = 0x800007bdu;
    *(uint32_t *)(m + 100) = g->buffer_count;
    *(uint64_t *)(m + 8) = 0x100000001ull;

    SLInterfaceID id_tail = SL_IID_BUFFERQUEUE;
    SLInterfaceID id_vol  = SL_IID_VOLUME;
    *(SLInterfaceID *)(m + 104) = id_tail;
    *(SLInterfaceID *)(m + 112) = id_vol;

    im = g->engine_itf;
    ((void (*)(void *, void *, void *, void *, uint32_t, void *, void *))
        method(im, 16))(im, &g->player, m + 48, m + 16, 2, m + 104, m + 8);

    void *rep = g->player;
    ((void (*)(void *, uint32_t))method(rep, 0))(rep, 0);

    rep = g->player;
    ((void (*)(void *, SLInterfaceID, void *))method(rep, 24))(
        rep, SL_IID_PLAY, &g->play_itf);

    rep = g->player;
    ((void (*)(void *, SLInterfaceID, void *))method(rep, 24))(rep, id_tail, &g->player_queue);

    void *queue = g->player_queue;
    ((void (*)(void *, void *, void *))method(queue, 24))(
        queue, (void *)audio_out_queue_callback, 0);

    rep = g->player;
    ((void (*)(void *, SLInterfaceID, void *))method(rep, 24))(rep, id_vol, &g->volume_itf);

    void *play = g->play_itf;
    ((void (*)(void *, uint32_t))method(play, 0))(play, 3);

    ((void *(*)(void *, int, unsigned long))sym_libc_memset)(g->buffers, 0, sizeof g->buffers);

    uint32_t n = g->buffer_count;
    if (n != 0) {
        unsigned char *value = g->buffers;
        for (uint64_t i = 0; i < n; i++) {
            g->buffer_fill[i] = 0;
            void *c = g->player_queue;
            uint32_t size = g->buffer_samples << 1;
            ((void (*)(void *, void *, uint32_t))method(c, 0))(c, value, size);
            n = g->buffer_count;
            value += 0x2df0;
        }
    }

    uint32_t v = g->buffer_samples;
    g->buffer_index = 0;
    *(uint16_t *)(out + 13) = 0x100;
    *(uint32_t *)out = v;
    *(uint8_t *)(out + 15) = 1;
    *(uint8_t *)(out + 11) = 0;
    g->active = 1;
    g->accumulated = 0;
    g->queued = 0;
    g->mic_samples = (void *)mic_default_sample;
    g->mic_sample_index = 0;
    return 0;
}

int audio_out_stop_and_mute(spu_t *spu) {
    platform_audio_t *bl = PLATFORM_AUDIO;

    if (bl->active != 0) {
        bl->closed = 1;

        void **o1 = (void **)bl->player_queue;
        ((void (*)(void *))(*(void ***)o1)[1])(o1);

        void **o2 = (void **)bl->play_itf;
        ((void (*)(void *, uint32_t))(*(void ***)o2)[0])(o2, 1);

        ((void *(*)(void *, int, unsigned long))sym_libc_memset)(
            bl->buffers, 0, sizeof bl->buffers);

        if (bl->mic_started != 0) {
            void **o3 = (void **)bl->record_itf;
            if (o3 != 0) {
                ((void (*)(void *, uint32_t))(*(void ***)o3)[0])(o3, 1);
                void **o4 = (void **)bl->recorder_queue;
                ((void (*)(void *))(*(void ***)o4)[1])(o4);
            }
        }
    }

    spu->mixer.muted = 1;
    return 0;
}

void audio_out_resume_after_stop(spu_t *spu, uint32_t stop_result)
{

    if (stop_result != 0)
        return;

    audio_out_unmute_and_resume(spu);
}

#define OFF_FLAG2    0x3c9b040UL





static void *vt_slot(void *vt, unsigned n) {
    return rd_ptr((const uint8_t *)vt + (size_t)n * 8);
}
typedef void  (*fn_this)(void *this_);
typedef void  (*fn_this_i)(void *this_, uint32_t a1);
typedef void *(*fn_memset)(void *, int, size_t);

void audio_out_flush_queues(void)
{

    if (PLATFORM_AUDIO->active == 0)
        return;

    PLATFORM_AUDIO->closed = 1;

    void *obj_a = PLATFORM_AUDIO->player_queue;
    void *vt_a  = rd_ptr(obj_a);
    ((fn_this)vt_slot(vt_a, 1))(obj_a);

    void *obj_b = PLATFORM_AUDIO->play_itf;
    void *vt_b  = rd_ptr(obj_b);
    ((fn_this_i)vt_slot(vt_b, 0))(obj_b, 1);

    ((fn_memset)sym_libc_memset)(PLATFORM_AUDIO->buffers, 0,
                                       sizeof PLATFORM_AUDIO->buffers);

    if (PLATFORM_AUDIO->mic_started == 0)
        return;

    void *obj_c = PLATFORM_AUDIO->record_itf;
    if (obj_c == NULL)
        return;

    void *vt_c = rd_ptr(obj_c);
    ((fn_this_i)vt_slot(vt_c, 0))(obj_c, 1);

    void *obj_d = PLATFORM_AUDIO->recorder_queue;
    void *vt_d  = rd_ptr(obj_d);
    ((fn_this)vt_slot(vt_d, 1))(obj_d);
}
#undef OFF_FLAG2

void audio_out_set_volume_db(uint32_t vol) {

    uint16_t *dst = &PLATFORM_AUDIO->volume_millibel;

    uint32_t v = (vol < 100) ? vol : 100u;
    if (v == 0) { *dst = 0x8000; return; }

    union { uint32_t u; float f; } hundred;   hundred.u = 0x42c80000u;
    union { uint64_t u; double d; } twothousand; twothousand.u = 0x409f400000000000ull;

    float s = (float)v / hundred.f;
    double d = ((double (*)(double))sym_libc_log10)((double)s);
    d *= twothousand.d;
    *dst = (uint16_t)(int32_t)(float)d;
}

void audio_out_channel_recalc(spu_t *spu, spu_channel_t *c) {

    uint8_t flags = c->recalc_flags;

    if (flags & 2) {
        uint32_t v = c->regs->cnt;
        uint32_t master = spu->regs->soundcnt & 0x7f;
        uint32_t vol = v & 0x7f;
        uint32_t div = (v >> 8) & 3;
        uint32_t sh = 4 - div;
        if (div == 3) sh = 0;
        if (vol == 0x7f) vol = 0x80;
        if (master == 0x7f) master = 0x80;
        uint32_t pan = (v >> 16) & 0x7f;
        uint32_t p = (master * vol) << sh;
        c->volume_left = (uint16_t)((p * (pan ^ 0x7f)) >> 13);
        c->volume_right = (uint16_t)((p * pan) >> 13);
    }

    if (flags & 1) {
        uint32_t freq = spu->mixer.sample_rate;
        uint32_t t = 0x10000u - c->regs->tmr;
        uint32_t divisor = t * freq;
        uint64_t step = divisor ? (0x01006f4300000000ull / (uint64_t)divisor) : 0;
        c->step = step;

        uint8_t slot = c->capture_slot;
        if (slot != 0xff)
            spu->capture[slot].step = step;
    }

    c->recalc_flags = 0;
}

int audio_out_load_wav(spu_t *spu, const char *path) {
    void *(*open_fn)(const char *, const char *) = platform_file_open;
    int    (*find)(void *, long, int)                 = (int (*)(void *, long, int))sym_libc_fseek;
    long   (*where)(void *)                             = (long (*)(void *))sym_libc_ftell;
    uint64_t (*read_fn)(void *, uint64_t, uint64_t, void *) =
        (uint64_t (*)(void *, uint64_t, uint64_t, void *))sym_libc_fread;
    void  *(*reserve)(uint64_t)                        = (void *(*)(uint64_t))sym_libc_malloc;
    void   (*release)(void *)                            = (void (*)(void *))sym_libc_free;
    int    (*close_fn)(void *)                            = (int (*)(void *))sym_libc_fclose;

    void *f = open_fn(path, "rb");
    if (f == 0)
        return -1;

    uint32_t hdr[3];
    uint32_t chunk[2];
    uint32_t format[4];

    find(f, 0, 2);
    long size = where(f);
    find(f, 0, 0);

    if (read_fn(hdr, 4, 3, f) != 3)                     goto fail;
    if (hdr[0] != 0x46464952u)                       goto fail;
    if (hdr[1] != (uint32_t)size - 8)                 goto fail;
    if (hdr[2] != 0x45564157u)                       goto fail;

    {
        unsigned char *dest = (unsigned char *)&spu->recording.data;
        int64_t limit = (int64_t)(uint32_t)size;

        for (;;) {
            if (read_fn(chunk, 4, 2, f) != 2)           goto fail;

            if (chunk[0] == 0x20746d66u) {
                if (chunk[1] != 16)                  goto fail;
                if (read_fn(format, 4, 4, f) != 4)     goto fail;
                if (format[0] != 0x10001u)          goto fail;
                uint16_t bits = *(uint16_t *)((unsigned char *)format + 14);
                if ((uint32_t)bits << 16 != 0x100000u) goto fail;
            } else if (chunk[0] == 0x61746164u) {
                uint32_t bytes = chunk[1];
                void *data = reserve(bytes);
                *(void **)dest = data;
                if (read_fn(data, bytes, 1, f) != 1) {
                    release(*(void **)dest);
                    *(void **)dest = 0;
                    goto fail;
                }
                *(uint32_t *)(dest + 8)  = format[1];
                *(uint32_t *)(dest + 12) = bytes >> 1;
                return 0;
            } else {
                find(f, (long)(uint32_t)chunk[1], 1);
            }

            if (where(f) >= limit)                  goto fail;
        }
    }

fail:
    close_fn(f);
    return -1;
}

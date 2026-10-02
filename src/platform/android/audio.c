#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <stdint.h>
#include <string.h>
#include "platform/android/audio.h"
#include "frontend/audio_out.h"
#include "frontend/mic_default_sample.h"
#include "core_internals.h"
#include "mem_access.h"

typedef void    (*fn_vcall3)(void*, void*, int32_t);

void platform_audio_output_feed_frame(spu_t *spu)
{

    static void *(*s_memcpy)(void*, const void*, size_t);
    static void *(*s_memset)(void*, int, size_t);
    if (!s_memcpy) s_memcpy = (void *(*)(void*, const void*, size_t))sym_libc_memcpy;
    if (!s_memset) s_memset = (void *(*)(void*, int, size_t))sym_libc_memset;

    unsigned char *ctx = (unsigned char *)spu;
    if (spu->mixer.muted != 0)      goto L_5;
    if (PLATFORM_AUDIO->closed != 0)          goto L_5;

    if (spu->mixer.mic_enabled != 0 &&
        spu->mixer.mic_init_pending != 0 &&
        spu->mixer.mic_init_failed == 0) {
        spu->mixer.mic_init_pending = 0;
        int32_t r = mic_init_recorder((long)(unsigned long)ctx);
        if (r < 0) spu->mixer.mic_init_failed = 1;
    }

    {
        uint32_t count = spu->output_written;
        platform_audio_t *au = PLATFORM_AUDIO;
        nds_capture_audio(ctx, count & 0x7fffffffu);

        if (spu->mixer.mic_loaded != 0) {

            if (PLATFORM_AUDIO->sound_disabled == 0) goto L_conv;
            {
                uint32_t n = count & 0x7fffffffu;
                if (n != 0) {
                    unsigned char *table = (unsigned char *)PLATFORM_AUDIO->mic_samples;
                    uint32_t idx   = PLATFORM_AUDIO->mic_sample_index;
                    uint32_t half = MIC_DEFAULT_SAMPLE_BYTES >> 1;
                    unsigned char *dst = spu->mic_buffer;
                    for (uint32_t i = 0; i < n; i++) {
                        int16_t sample = rd16s(table + (size_t)idx * 2);
                        uint32_t nidx = idx + 1;
                        idx = (nidx >= half) ? 0u : nidx;

                        int32_t v  = sample;
                        int32_t r1 = v >> 2;
                        int32_t r2 = v << 1;
                        int32_t av = (v < 0) ? -v : v;
                        int32_t sel = (av < 0x2000) ? r1 : r2;
                        if (sel > 32767)  sel = 32767;
                        if (sel < -32767) sel = -32767;

                        PLATFORM_AUDIO->mic_sample_index = idx;
                        wr16(dst, (int16_t)sel);
                        dst += 2;
                    }
                }
            }

        }

    L_1:

        if (PLATFORM_AUDIO->mic_enabled != 0) goto L_4;

    L_2:

        {
            uint32_t w9 = PLATFORM_AUDIO->queued;
            uint32_t w8 = PLATFORM_AUDIO->buffer_count;
            if (w9 >= w8) goto L_4;
        }

        {
            uint32_t idx = au->buffer_index;
            uint32_t table_val = au->buffer_fill[idx];
            unsigned char *channel = au->buffers + (size_t)idx * PLATFORM_AUDIO_BUFFER_BYTES;
            unsigned char *dst   = channel + (size_t)table_val * 2;

            s_memcpy(dst, ctx, (size_t)count << 1);

            uint32_t table_val2 = au->buffer_fill[idx];
            uint32_t updated = table_val2 + PLATFORM_AUDIO_SAMPLES_PER_FRAME;
            au->buffer_fill[idx] = updated;

            uint32_t cap = au->buffer_samples;
            if (updated < cap) goto L_4;

            {

                void *obj1 = au->player_queue;
                unsigned char *arg1 = channel;
                void *vt1 = rd_ptr(obj1);
                fn_vcall3 fn1 = (fn_vcall3)rd_ptr(vt1);
                fn1(obj1, arg1, (int32_t)(cap << 1));
            }
            {
                uint32_t idx2 = au->buffer_index;
                au->buffer_fill[idx2] = 0;
                uint32_t cap2 = au->buffer_count;
                uint32_t gen  = au->queued;
                uint32_t nw   = idx2 + 1;
                uint32_t q    = nw / cap2;
                uint32_t r    = nw - q * cap2;
                au->buffer_index = r;
                au->queued = gen + 1;
            }
            goto L_4;
        }

    L_conv:

        {
            void *engine = PLATFORM_AUDIO->record_itf;
            if (engine == NULL) goto L_3;

            uint32_t n = count & 0x7fffffffu;
            if (n != 0) {

                float gain = audio_out_volume;
                unsigned char *source  = (unsigned char *)au->mix;
                unsigned char *dest = spu->mic_buffer;
                for (uint32_t i = 0; i < n; i++) {
                    int16_t sample = rd16s(source + (size_t)i * 2);
                    float normalized = (float)sample * (1.0f / 32768.0f);
                    float mag = normalized * (gain * normalized);
                    float with_sign = (sample < 0) ? -(mag * 32768.0f) : (mag * 32768.0f);
                    int32_t v = (int32_t)with_sign;
                    if (v > 32767)  v = 32767;
                    if (v < -32767) v = -32767;
                    wr16(dest + (size_t)i * 2, (int16_t)v);
                }
            }

            if (audio_out_enabled != 0) {
                uint32_t slot = au->mic_slot;
                unsigned char *src = au->mic_buffers[slot];
                unsigned char *dst = (unsigned char *)au->mix;
                s_memcpy(dst, src, 0x1000);

                uint32_t total = au->mic_total;
                if (total <= 4) {
                    uint32_t updated_slot = ((slot + 1u) & 0xffu) % PLATFORM_AUDIO_MIC_QUEUE_DEPTH;
                    au->mic_slot = (uint8_t)updated_slot;
                    au->mic_total = total + 1u;

                    void *obj2 = au->recorder_queue;
                    unsigned char *arg2 = au->mic_buffers[updated_slot];
                    void *vt2 = rd_ptr(obj2);
                    fn_vcall3 fn2 = (fn_vcall3)rd_ptr(vt2);
                    fn2(obj2, arg2, (int32_t)n);

                    if (PLATFORM_AUDIO->mic_enabled != 0) goto L_4;
                    goto L_2;
                }
            }
            goto L_1;
        }

    L_3:

        s_memset(spu->mic_buffer, 0, (size_t)count << 2);
        if (PLATFORM_AUDIO->mic_enabled == 0) goto L_2;
        goto L_4;

    L_4:

        {
            uint64_t g68 = PLATFORM_AUDIO->accumulated;
            g68 -= (uint64_t)(count & 0x7fffffffu);
            PLATFORM_AUDIO->accumulated = g68;
        }
        spu->output_written = 0;
        return;
    }

L_5:

    spu->output_written = 0;
    return;
}
#undef G1
#undef G2
#undef G3
#undef RD

platform_audio_t platform_audio_state;

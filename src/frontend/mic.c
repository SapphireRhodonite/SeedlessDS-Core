#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "platform/android/audio.h"
#include "frontend/audio_out.h"
#include "mem_access.h"

#define OFF_AUDIO_ACTIVE  0x3c7d074UL
#define QUEUE_OFF          0x3c7d050UL
#define OFF_COUNTER      0x3c9b028UL
#define OFF_BUFFER        0x3c99004UL



typedef uint32_t (*fn_enqueue)(void *queue, void *buffer, uint32_t size);

void mic_capture_queue_callback(void *caller, void *context)
{
    (void)caller;
    (void)context;

    if (PLATFORM_AUDIO->closed != 0)
        return;

    uint32_t count = PLATFORM_AUDIO->mic_total;
    count = count - 1u;
    if (count != 0) {
        PLATFORM_AUDIO->mic_total = count;
        return;
    }

    void *queue = PLATFORM_AUDIO->recorder_queue;
    void *table = rd_ptr(queue);
    void *enqueue = rd_ptr(table);

    ((fn_enqueue)enqueue)(queue, PLATFORM_AUDIO->mic_buffers[5], 0x1000u);
}
#undef OFF_AUDIO_ACTIVE
#undef QUEUE_OFF
#undef OFF_COUNTER
#undef OFF_BUFFER

#define OFF_FLAG_AUDIO_ON   0x143ebc
#define OFF_ENGINE_OBJ      0x3c7d018
#define OFF_RECORD_OBJ      0x3c7d040
#define OFF_RECORD_ITF      0x3c7d048
#define OFF_BUFQ_ITF        0x3c7d050
#define OFF_CAPBUF1         0x3c94004
#define OFF_CAPBUF2         0x3c95004
#define OFF_FLAG_A          0x3c9a004
#define OFF_FLAG_B          0x3c9b028
#define OFF_FLAG_C          0x3c9b040
#define OFF_GOT_IID_ASBQ    0x138da0
#define OFF_GOT_IID_RECORD  0x138ef8
#define OFF_RODATA_SLES     0x10a0a0
#define OFF_CALLBACK_FN     0x1d6b4

int mic_init_recorder(long param_1)
{

    unsigned char *ctx  = (unsigned char *)(unsigned long)param_1;

    {
        uint32_t rate = 44100;
        ((spu_t *)ctx)->mixer.mic_rate = rate;
    }
    {
        void *(*p_memset)(void *, int, unsigned long) =
            (void *(*)(void *, int, unsigned long))sym_libc_memset;
        p_memset(((spu_t *)ctx)->mic_buffer, 0, sizeof ((spu_t *)ctx)->mic_buffer);
    }

    uint32_t audio_on = audio_out_enabled;

    if (audio_on != 0) {
        void *record_obj;
        record_obj = PLATFORM_AUDIO->recorder;

        if (record_obj == NULL) {

            static const uint32_t loc_io_init[6] = { 3u, 1u, 0xFFFFFFFFu, 0u, 0u, 0u };
            unsigned char loc_io[24];
            memcpy(loc_io, loc_io_init, sizeof(loc_io));
            struct { void *pLocator; void *pFormat; } audio_src = { loc_io, NULL };

            uint64_t loc_bq = 0x5800007bdULL;
            static const uint32_t fmt_pcm_init[7] = { 2u, 1u, 44100000u, 16u, 16u, 4u, 2u };
            unsigned char fmt_pcm[28];
            memcpy(fmt_pcm, fmt_pcm_init, sizeof(fmt_pcm));
            struct { void *pLocator; void *pFormat; } audio_snk = { &loc_bq, fmt_pcm };

            SLInterfaceID iid_asbq = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;

            uint32_t required = 1;

            void *engine_obj;
            engine_obj = PLATFORM_AUDIO->engine_itf;
            void *engine_vt;
            memcpy(&engine_vt, engine_obj, sizeof(void *));
            void *m_create;
            memcpy(&m_create, (unsigned char *)engine_vt + 0x18, sizeof(void *));

            int rc = ((int (*)(void *, void *, void *, void *, unsigned,
                                void *, void *))m_create)
                        (engine_obj, &PLATFORM_AUDIO->recorder,
                         &audio_src, &audio_snk, required,
                         &iid_asbq, &required);

            if (rc == 0) {
                record_obj = PLATFORM_AUDIO->recorder;
                void *vt;
                memcpy(&vt, record_obj, sizeof(void *));
                void *m_realize;
                memcpy(&m_realize, vt, sizeof(void *));
                rc = ((int (*)(void *, unsigned))m_realize)(record_obj, 0);
            }

            if (rc != 0) {

                return -1;
            }

            record_obj = PLATFORM_AUDIO->recorder;
            {
                void *vt;
                memcpy(&vt, record_obj, sizeof(void *));
                void *m_getiface;
                memcpy(&m_getiface, (unsigned char *)vt + 0x18, sizeof(void *));

                SLInterfaceID iid_record = SL_IID_RECORD;

                ((int (*)(void *, SLInterfaceID, void *))m_getiface)
                    (record_obj, iid_record, &PLATFORM_AUDIO->record_itf);
                ((int (*)(void *, SLInterfaceID, void *))m_getiface)
                    (record_obj, iid_asbq, &PLATFORM_AUDIO->recorder_queue);
            }
            {
                void *bufq_itf;
                bufq_itf = PLATFORM_AUDIO->recorder_queue;
                void *vt;
                memcpy(&vt, bufq_itf, sizeof(void *));
                void *m_regcb;
                memcpy(&m_regcb, (unsigned char *)vt + 0x18, sizeof(void *));
                void *callback = (void *)(mic_capture_queue_callback);
                ((int (*)(void *, void *, void *))m_regcb)(bufq_itf, callback, NULL);
            }
        }

        {
            void *record_itf;
            record_itf = PLATFORM_AUDIO->record_itf;
            void *bufq_itf;
            bufq_itf = PLATFORM_AUDIO->recorder_queue;

            void *vt_r;
            memcpy(&vt_r, record_itf, sizeof(void *));
            void *m_setstate;
            memcpy(&m_setstate, vt_r, sizeof(void *));
            ((int (*)(void *, unsigned))m_setstate)(record_itf, 1);

            void *vt_b;
            memcpy(&vt_b, bufq_itf, sizeof(void *));
            void *m_clear;
            memcpy(&m_clear, (unsigned char *)vt_b + 8, sizeof(void *));
            ((int (*)(void *))m_clear)(bufq_itf);

            void *m_enqueue;
            memcpy(&m_enqueue, vt_b, sizeof(void *));
            ((int (*)(void *, void *, unsigned))m_enqueue)
                (bufq_itf, PLATFORM_AUDIO->mic_buffers[0], PLATFORM_AUDIO_MIC_BUFFER_BYTES);
            ((int (*)(void *, void *, unsigned))m_enqueue)
                (bufq_itf, PLATFORM_AUDIO->mic_buffers[1], PLATFORM_AUDIO_MIC_BUFFER_BYTES);

            ((int (*)(void *, unsigned))m_setstate)(record_itf, 3);

            *(volatile uint8_t  *)&PLATFORM_AUDIO->mic_started = 1;
            *(volatile uint8_t  *)&PLATFORM_AUDIO->mic_slot = 0;
            *(volatile uint32_t *)&PLATFORM_AUDIO->mic_total = 2;
        }
    }

    ((spu_t *)ctx)->mixer.mic_sample_bytes = 1;
    ((spu_t *)ctx)->mixer.mic_loaded = 1;
    return 0;
}
#undef OFF_FLAG_AUDIO_ON
#undef OFF_ENGINE_OBJ
#undef OFF_RECORD_OBJ
#undef OFF_RECORD_ITF
#undef OFF_BUFQ_ITF
#undef OFF_CAPBUF1
#undef OFF_CAPBUF2
#undef OFF_FLAG_A
#undef OFF_FLAG_B
#undef OFF_FLAG_C
#undef OFF_GOT_IID_ASBQ
#undef OFF_GOT_IID_RECORD
#undef OFF_RODATA_SLES
#undef OFF_CALLBACK_FN

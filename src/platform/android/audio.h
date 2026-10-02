#ifndef SEEDLESSDS_PLATFORM_AUDIO_H
#define SEEDLESSDS_PLATFORM_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define PLATFORM_AUDIO_BUFFERS 8
#define PLATFORM_AUDIO_BUFFER_BYTES 0x2df0u
#define PLATFORM_AUDIO_MIC_BUFFERS 6
#define PLATFORM_AUDIO_MIC_BUFFER_BYTES 0x1000u
#define PLATFORM_AUDIO_MIC_QUEUE_DEPTH 5u
#define PLATFORM_AUDIO_SAMPLES_PER_FRAME 0x5beu

typedef struct platform_audio {
    uint8_t restart_pending;
    uint8_t unmapped_0[7];
    struct nds *machine;
    void *engine;
    void *engine_itf;
    void *output_mix;
    void *player;
    void *play_itf;
    void *player_queue;
    void *recorder;
    void *record_itf;
    void *recorder_queue;
    void *volume_itf;
    uint32_t buffer_index;
    uint32_t unmapped_2;
    uint64_t accumulated;
    uint32_t queued;
    uint32_t closed;
    uint32_t active;
    uint32_t buffer_count;
    uint32_t buffer_samples;
    uint8_t buffers[PLATFORM_AUDIO_BUFFERS * PLATFORM_AUDIO_BUFFER_BYTES];
    uint8_t mic_buffers[PLATFORM_AUDIO_MIC_BUFFERS][PLATFORM_AUDIO_MIC_BUFFER_BYTES];
    uint8_t mic_slot;
    uint8_t unmapped_3;
    int16_t mix[PLATFORM_AUDIO_MIC_BUFFER_BYTES / 2];
    uint8_t unmapped_4[2];
    uint32_t buffer_fill[PLATFORM_AUDIO_BUFFERS];
    uint32_t mic_total;
    uint32_t unmapped_5;
    void *mic_samples;
    uint32_t mic_sample_index;
    uint32_t sound_disabled;
    uint8_t mic_started;
    uint8_t unmapped_6[7];
    uint32_t mic_enabled;
    uint16_t volume_millibel;
    uint8_t unmapped_7[2];
} platform_audio_t;

extern platform_audio_t platform_audio_state;

typedef const struct SLInterfaceID_ *SLInterfaceID;

extern const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
extern const SLInterfaceID SL_IID_BUFFERQUEUE;
extern const SLInterfaceID SL_IID_ENGINE;
extern const SLInterfaceID SL_IID_PLAY;
extern const SLInterfaceID SL_IID_RECORD;
extern const SLInterfaceID SL_IID_VOLUME;

#define PLATFORM_AUDIO (&platform_audio_state)
#endif

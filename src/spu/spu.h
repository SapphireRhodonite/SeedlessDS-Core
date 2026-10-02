#ifndef SEEDLESSDS_SPU_H
#define SEEDLESSDS_SPU_H

#include <stddef.h>
#include <stdint.h>

struct nds;
struct bus;

#define SPU_CHANNELS 16u
#define SPU_CAPTURE_UNITS 2u
#define SPU_SIZE 0x40d50u
#define SPU_MIC_BUFFER_OFFSET 0x20000u

typedef struct sound_channel_regs {
    uint32_t cnt;
    uint32_t sad;
    uint16_t tmr;
    uint16_t pnt;
    uint32_t len;
} sound_channel_regs_t;

typedef struct sound_regs {
    sound_channel_regs_t channel[SPU_CHANNELS];
    uint32_t soundcnt;
    uint32_t soundbias;
    uint8_t sndcap0cnt;
    uint8_t sndcap1cnt;
    uint8_t unmapped_0[0x110 - 0x10a];
    uint32_t sndcap0dad;
    uint16_t sndcap0len;
    uint8_t unmapped_1[2];
    uint32_t sndcap1dad;
    uint16_t sndcap1len;
    uint8_t unmapped_2[2];
} sound_regs_t;

typedef struct spu_channel {
    int16_t adpcm_samples[64];
    uint64_t position;
    uint64_t step;
    uint32_t adpcm_decoded;
    uint32_t unmapped_0;
    sound_channel_regs_t *regs;
    const uint8_t *data;
    uint32_t source_address;
    uint32_t length;
    uint32_t loop_length;
    int16_t volume_left;
    int16_t volume_right;
    uint16_t adpcm_loop_sample;
    uint16_t adpcm_sample;
    uint8_t format;
    uint8_t recalc_flags;
    uint8_t active;
    uint8_t adpcm_loop_index;
    uint8_t adpcm_index;
    uint8_t looped;
    uint8_t capture_slot;
    uint8_t unmapped_1[0xc8 - 195];
} spu_channel_t;

typedef struct spu_capture {
    uint64_t position;
    uint64_t step;
    uint8_t *buffer;
    uint32_t length;
    uint8_t control;
    uint8_t unmapped_0[3];
} spu_capture_t;

typedef struct spu_mixer {
    uint32_t sample_rate;
    uint32_t mic_rate;
    uint32_t output_capacity;
    uint32_t unmapped_0;
    uint8_t output_flag;
    uint8_t mic_enabled;
    uint8_t mic_sample_bytes;
    uint8_t mic_loaded;
    uint8_t mic_init_pending;
    uint8_t mic_init_failed;
    uint8_t enabled;
    uint8_t muted;
} spu_mixer_t;

typedef struct spu_recording {
    uint64_t mic_time_base;
    uint64_t start_cycles;
    uint32_t cycles_per_sample;
    uint32_t unmapped_1;
    uint32_t samples_per_cycle;
    uint32_t mic_frequency;
    uint32_t frequency;
    uint32_t unmapped_2;
    uint8_t *data;
    uint32_t rate;
    uint32_t sample_count;
    uint8_t active;
    uint8_t unmapped_3[SPU_SIZE - 0x40d41];
} spu_recording_t;

typedef struct spu {
    int16_t output_samples[0x10000];
    uint8_t mic_buffer[0x20000];
    uint8_t unmapped_0[8];
    uint32_t output_read;
    uint32_t output_written;
    spu_mixer_t mixer;
    spu_channel_t channels[SPU_CHANNELS];
    spu_capture_t capture[SPU_CAPTURE_UNITS];
    sound_regs_t *regs;
    struct nds *machine;
    struct bus *bus;
    uint64_t mix_cycles;
    spu_recording_t recording;
} spu_t;

extern const uint8_t spu_psg_duty_table[128];
#define SPU_NOISE_TABLE_BYTES 0x8000u
extern uint8_t spu_noise_table[SPU_NOISE_TABLE_BYTES];

#endif

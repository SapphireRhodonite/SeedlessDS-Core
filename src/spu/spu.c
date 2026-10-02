#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include <string.h>
#include "core/nds_state.h"
#include "frontend/video_out_gl.h"
#include "core_internals.h"
#include "mem_access.h"

uint8_t spu_noise_table[SPU_NOISE_TABLE_BYTES];

const uint8_t spu_psg_duty_table[128] = {
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0x7f,
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0x7f, 0xff, 0x7f,
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f,
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f,
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f,
    0x01, 0x80, 0x01, 0x80, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f,
    0x01, 0x80, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f,
    0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f,
};

void spu_channel_set_mode_and_resize(uint32_t slot, uint32_t mode) {
    video_out_gl_t *g = VIDEO_OUT_GL;
    if (g->screen_mode[slot] == mode) return;

    recon_native_read();
    unsigned n = (recon_native && mode != 0u) ? recon_scale_out : (mode == 0u ? 1u : 2u);
    uint32_t a = 256u * n;
    uint32_t b = 192u * n;
    g->screen_width[slot] = a;
    g->screen_height[slot] = b;
    g->screen_mode[slot] = mode;
    g->screen_changed[slot] = 1;
}

void spu_recording_start(spu_t *spu) {
    const unsigned char *p = (const unsigned char *)spu->machine;
    spu->recording.active = 1;
    uint64_t v;
    memcpy(&v, p + 8, 8);
    spu->recording.start_cycles = v;
}

void spu_recording_stop(spu_t *spu) {
    spu->recording.active = 0;
}

void spu_channel_advance_position(spu_t *spu, void *unused, uint32_t samples,
                        uint32_t channel) {
    (void)unused;
    spu_capture_t *cap = &spu->capture[channel];
    uint32_t flags = cap->control;
    if ((flags & 0x80) == 0)
        return;

    uint32_t kind = flags & 0xb;
    if (kind > 9)
        return;
    if (kind != 0 && kind != 1 && kind != 8 && kind != 9)
        return;

    uint64_t pos = cap->position;

    if (samples != 0) {
        uint64_t step = cap->step;
        uint64_t top = cap->length;
        unsigned char *buf = cap->buffer;
        int erase16 = (kind == 0);
        int erase8  = (kind == 8);
        int stops    = (flags & 4) != 0;

        if (!stops) {
            do {
                if (erase16)
                    *(uint16_t *)(buf + ((pos >> 31) & 0x1fffffffeULL)) = 0;
                else if (erase8)
                    buf[pos >> 32] = 0;
                pos += step;
                pos -= (top > (pos >> 32)) ? 0 : (top << 32);
            } while (--samples != 0);
        } else {
            uint64_t prev;
            for (;;) {
                prev = pos;
                if (erase16)
                    *(uint16_t *)(buf + ((pos >> 31) & 0x1fffffffeULL)) = 0;
                else if (erase8)
                    buf[pos >> 32] = 0;
                pos += step;
                if (top <= (pos >> 32)) {
                    flags &= 0x7f;
                    cap->control = (unsigned char)flags;
                    (&spu->regs->sndcap0cnt)[channel] = (unsigned char)flags;
                    break;
                }
                if (--samples == 0)
                    break;
            }
            pos = step + prev;
        }
    }

    cap->position = pos;
}

#define B_STEP     0x60
#define B_MASK     0xfc698
#define B_BASE     0xfc6a0
#define B_MARK    0xfc6f0
#define TWO_TOP_BITS  0x60000000u
typedef uint64_t (*fn_res)(void *, uint64_t);




void spu_channel_start(spu_t *spu, uint32_t ch) {
    spu_channel_t *c = &spu->channels[ch];
    if (c->active) return;
    const sound_channel_regs_t *desc = c->regs;
    uint8_t *banks = (uint8_t *)spu->bus;
    uint32_t ctl  = desc->cnt;
    uint32_t dir  = desc->sad;
    uint32_t len = desc->len;
    uint32_t frames = desc->pnt;
    uint32_t total = len + frames;
    if ((ctl & TWO_TOP_BITS) != TWO_TOP_BITS && total == 0) return;

    c->position = 0;
    c->active = 1;
    uint64_t adr = dir & 0x7ffffffu;
    uint64_t bank = (dir >> 23) & 0xfu;
    c->source_address = (uint32_t)adr;

    uint8_t *e = banks + bank * B_STEP;
    uint32_t mark = e[B_MARK];
    const uint8_t *dat;
    if (mark == 1) {
        dat = (const uint8_t *)((fn_res)rd64(e + B_BASE))(banks, adr);
        c->data = dat;
    } else if (mark == 0) {
        uint64_t m = rd32(e + B_MASK);
        dat = (const uint8_t *)(rd64(e + B_BASE) + (m & adr));
        c->data = dat;
    } else {
        dat = 0;
        c->data = 0;
        if ((ctl & TWO_TOP_BITS) != TWO_TOP_BITS) c->active = 0;
    }

    switch ((ctl >> 29) & 3u) {
    case 0:
        c->format = 0;
        c->length = total << 2;
        c->loop_length = len << 2;
        return;
    case 1:
        c->format = 1;
        c->length = total << 1;
        c->loop_length = len << 1;
        return;
    case 2: {
        uint16_t hdr = rd16(dat);
        c->adpcm_sample = hdr;
        uint32_t ctl2 = c->regs->cnt;
        uint8_t step = (uint8_t)(dat[2] & 0x7f);
        if (step > 88) step = 88;
        c->adpcm_index = step;
        c->data = dat + 4;
        c->format = 2;
        c->adpcm_decoded = 0;
        if (!(ctl2 & (1u << 27))) {
            c->length = (total << 3) - 8u;
            return;
        }
        c->loop_length = len << 3;
        if (frames > 1) {
            c->length = (frames << 3) - 8u;
            c->looped = 0;
            return;
        }
        c->looped = 1;
        c->adpcm_loop_sample = hdr;
        c->adpcm_loop_index = step;
        c->length = len << 3;
        return;
    }
    case 3:
    default:
        if (ch >= 0xeu) {
            c->format = 4;
            c->data = spu_noise_table;
            c->length = 0x7fff;
            c->loop_length = 0x7fff;
            return;
        }
        if (ch >= 8u) {
            c->format = 3;
            c->data = spu_psg_duty_table + ((ctl >> 24) & 7u) * 0x10u;
            c->length = 8;
            c->loop_length = 8;
            return;
        }
        c->format = 5;
        return;
    }
}
#undef B_STEP
#undef B_MASK
#undef B_BASE
#undef B_MARK
#undef TWO_TOP_BITS

void spu_slot_activate(spu_t *spu, uint32_t slot, uint32_t state) {
    spu_capture_t *cap = &spu->capture[slot];
    cap->control = (uint8_t)state;
    if (!(state & 0x80)) return;

    sound_regs_t *desc = spu->regs;
    unsigned char *data = spu->bus->main_ram;
    uint32_t off = (slot == 0) ? desc->sndcap0dad : desc->sndcap1dad;
    uint32_t len = (slot == 0) ? desc->sndcap0len : desc->sndcap1len;
    cap->buffer = data + (uint64_t)(off & 0x3fffff);
    cap->position = 0;
    cap->length = len << 1;
}

void spu_noise_table_generate(void) {
    unsigned char *t = spu_noise_table;
    unsigned int state = 0x7fff;
    for (int i = 0; i < 32767; i++) {
        unsigned int next = state >> 1;
        if (state & 1) {
            *t++ = 0x7f;
            state = next ^ 0x6000;
        } else {
            *t++ = 0x80;
            state = next;
        }
    }
}

void spu_init_channels_and_noise(spu_t *spu, unsigned char *ctx) {
    nds_t *machine = (nds_t *)ctx;
    spu->regs = &machine->bus.io_mirror[1].sound;
    for (int k = 0; k < 16; k++) {
        spu->channels[k].regs = &spu->regs->channel[k];
        spu->channels[k].capture_slot = 255;
    }
    spu->channels[1].capture_slot = 0;
    spu->channels[3].capture_slot = 1;
    spu->mixer.enabled = 1;
    spu->mixer.sample_rate = 0xAC44;
    spu->mixer.mic_rate = 0xAC44;
    spu->mixer.output_flag = 1;
    spu->mixer.mic_enabled = 1;
    spu->machine = machine;
    spu->bus = &machine->bus;
    spu->recording.data = 0;
    (void)audio_out_start((unsigned char *)spu);
    uint64_t f = spu->mixer.sample_rate;
    spu->recording.cycles_per_sample = (uint32_t)(0x1006F43800ULL / f);
    __uint128_t p = (__uint128_t)(f << 22) * (__uint128_t)0xFF90ECC69F727E51ULL;
    spu->recording.samples_per_cycle = (uint32_t)((uint64_t)(p >> 64) >> 26);

    unsigned char *t = spu_noise_table;
    uint32_t lfsr = 0x7fff;
    for (int i = 0; i < 32767; i++) {
        uint32_t next = lfsr >> 1;
        if (lfsr & 1) {
            *t++ = 127;
            lfsr = next ^ 0x6000;
        } else {
            *t++ = 128;
            lfsr = next;
        }
    }
}

void spu_object_reset_defaults(spu_channel_t *c) {
    c->data = 0;
    c->source_address = 0;
    c->position = 0;
    c->adpcm_decoded = 0;
    c->volume_left = 0;
    c->volume_right = 0;
    c->adpcm_loop_sample = 0;
    c->adpcm_index = 0;
    c->recalc_flags = 3;
    c->active = 0;
}

static void (*core_free)(void *);

long spu_reset_slots_and_load_config(spu_t *spu) {
    if (!core_free) core_free = (void (*)(void *))sym_libc_free;
    unsigned char *ctx = (unsigned char *)spu->machine;
    for (int k = 0; k < 16; k++) {
        spu_channel_t *e = &spu->channels[k];
        e->data = 0;
        e->source_address = 0;
        e->position = 0;
        e->adpcm_decoded = 0;
        e->volume_left = 0;
        e->volume_right = 0;
        e->adpcm_loop_sample = 0;
        e->adpcm_index = 0;
        e->recalc_flags = 3;
        e->active = 0;
    }
    (void)video_out_clear_large_buffer((unsigned char *)spu);
    void *block = spu->recording.data;
    spu->mix_cycles = 0;
    spu->capture[0].control = 0;
    spu->capture[1].control = 0;
    spu->recording.unmapped_1 = 0;
    spu->recording.mic_frequency = 0;
    spu->recording.active = 0;
    if (block) {
        core_free(block);
        spu->recording.data = 0;
    }

    unsigned char *dir = (unsigned char *)((nds_t *)ctx)->cache_dir;
    char path[0x828];
    str_vsprintf_limit_2080_1(path, 0, "%s%cmicrophone%c%s.wav", dir, '/', '/', ((nds_t *)ctx)->rom_name);
    if (audio_out_load_wav(spu, path) == -1) {
        str_vsprintf_limit_2080_1(path, 0, "%s%cmicrophone%cmicrophone.wav", dir, '/', '/', ((nds_t *)ctx)->rom_name);
        audio_out_load_wav(spu, path);
    }
    return nds_subsystem_notify_shutdown_pair((unsigned char *)spu);
}

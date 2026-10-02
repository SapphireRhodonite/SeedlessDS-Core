#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <string.h>
#include "core/nds_state.h"
#include "core_internals.h"

#define SPU_OUTPUT_SAMPLES 0x10000
#define SPU_OUTPUT_MASK (SPU_OUTPUT_SAMPLES - 1)
#define SPU_MIX_MAX_SAMPLES (0x8000 / 8)

static inline void mix(unsigned char *out, int32_t m, int32_t vl, int32_t vr) {
    uint32_t *d = (uint32_t *)out;
    d[0] += (uint32_t)m * (uint32_t)vl;
    d[1] += (uint32_t)m * (uint32_t)vr;
}

static void recompute(spu_channel_t *c, spu_t *spu) {
    uint32_t f = c->recalc_flags;
    if (f == 0) return;

    if (f & 2) {
        uint32_t v = c->regs->cnt;
        uint32_t master = spu->regs->soundcnt & 0x7f;
        uint32_t vol = v & 0x7f;
        uint32_t pan = (v >> 16) & 0x7f;
        uint32_t div = (v >> 8) & 3;
        uint32_t sh = 4 - div;
        if (div == 3) sh = 0;
        if (vol == 0x7f) vol = 0x80;
        if (master == 0x7f) master = 0x80;
        uint32_t p = (master * vol) << sh;
        c->volume_left = (uint16_t)((p * (pan ^ 0x7f)) >> 13);
        c->volume_right = (uint16_t)((p * pan) >> 13);
    }

    if (f & 1) {
        uint32_t t = 0x10000u - c->regs->tmr;
        uint32_t freq = spu->mixer.sample_rate;
        uint64_t slot = c->capture_slot;
        uint64_t step = 0x01006f4300000000ull / (uint64_t)(t * freq);
        c->step = step;
        if (slot != 0xff)
            spu->capture[slot].step = step;
    }

    c->recalc_flags = 0;
}

void spu_mixer_run_channels(spu_t *spu, unsigned char *output, uint32_t n) {

    static const uint8_t format_shape[5] = { 0x00, 0x63, 0x16, 0x79, 0x50 };
    const uint8_t *table = format_shape;

    for (int channel = 0; channel < 16; channel++) {

        spu_channel_t *c = &spu->channels[channel];
        int capture = (channel < 4);

        if (c->active == 0) continue;
        recompute(c, spu);

        uint32_t type = c->format;
        if (capture ? (type > 2) : (type > 4)) {
            c->position = c->position;
            continue;
        }

        uint32_t shape = capture ? type : table[type];

        int is16, rewind_always, is_adpcm;
        if (capture) {
            is_adpcm = (type == 2);
            is16 = (type == 1);
            rewind_always = 0;
        } else {
            is_adpcm = (shape == 0x16);
            is16 = (shape == 0x63 || shape == 0x79);
            rewind_always = (shape == 0x79 || shape == 0x50);
        }

        int32_t vl = c->volume_left;
        int32_t vr = c->volume_right;
        uint64_t pos = c->position;
        uint64_t step = c->step;
        const unsigned char *data = c->data;
        uint32_t limit = c->length;

        if (n == 0) { c->position = pos; continue; }

        unsigned char *out = output;
        uint64_t cap = 0x4000 + (uint64_t)channel * 2;
        uint32_t left = n;
        int off = 0;

        for (;;) {
            int32_t m;

            if (is_adpcm) {
                uint32_t int_val = (uint32_t)(pos >> 32);
                while (c->adpcm_decoded <= int_val) spu_adpcm_decode_block(c);
                m = c->adpcm_samples[(pos >> 32) & 0x3f];
                if (capture) *(uint16_t *)(out + cap) = (uint16_t)(m << 12);
            } else if (is16) {
                uint64_t i = (pos >> 31) & 0x1fffffffeull;
                m = *(int16_t *)(data + i);
                if (capture) *(uint16_t *)(out + cap) = (uint16_t)(m << 12);
            } else {
                m = (int32_t)(*(int8_t *)(data + (pos >> 32))) << 8;
                if (capture) *(uint16_t *)(out + cap) = 0;
            }

            mix(out, m, vl, vr);
            pos += step;

            if ((uint32_t)(pos >> 32) >= limit) {

                uint32_t v = c->regs->cnt;
                if (!rewind_always && !(v & (1u << 27))) {
                    c->regs->cnt = v & 0x7fffffff;
                    off = 1;
                    break;
                }
                uint32_t len = c->loop_length;
                if (is_adpcm) {
                    if (c->looped != 0) {
                        uint32_t disp = c->adpcm_decoded;
                        uint16_t e = c->adpcm_loop_sample;
                        uint8_t g = c->adpcm_loop_index;
                        pos -= (uint64_t)len << 32;
                        c->adpcm_decoded = disp - len;
                        c->adpcm_sample = e;
                        c->adpcm_index = g;
                    } else {
                        limit += len;
                        uint16_t e = c->adpcm_sample;
                        uint8_t g = c->adpcm_index;
                        c->looped = 1;
                        c->length = limit;
                        c->adpcm_loop_sample = e;
                        c->adpcm_loop_index = g;
                    }
                } else {
                    pos -= (uint64_t)len << 32;
                }
            }

            out += 8;
            if (--left == 0) break;
        }

        if (off) c->active = 0;
        c->position = pos;
    }
}

void spu_mixer_run_frame(unsigned char *machine) {
    void *(*fill)(void *, int, uint64_t) =
        (void *(*)(void *, int, uint64_t))sym_libc_memset;

    spu_t *state  = &((nds_t *)machine)->spu;
    uint64_t accumulated = state->mix_cycles;
    uint32_t step = state->recording.samples_per_cycle;
    int32_t pending = (int32_t)(*(uint32_t *)(machine + 8) << 10) - (int32_t)accumulated;
    uint64_t product = (uint64_t)((int64_t)pending * (uint64_t)step);
    uint32_t samples = (uint32_t)(product >> 32);

    if (samples & 0x80000000u)
        return;
    if (samples > SPU_MIX_MAX_SAMPLES)
        samples = SPU_MIX_MAX_SAMPLES;

    __attribute__((aligned(16))) int32_t mix[0x8000 / 4];

    unsigned char *playing = (unsigned char *)&((nds_t *)machine)->config.sound_enabled;

    uint32_t written = state->output_written & SPU_OUTPUT_MASK;
    uint32_t perSample = state->recording.cycles_per_sample;
    uint32_t words = samples * 2;

    state->mix_cycles = accumulated + (uint64_t)(uint32_t)(perSample * samples);
    if (words != 0)
        fill(mix, 0, (uint64_t)samples * 8);

    if (((nds_t *)machine)->arm7.halt_flags <= 1)
        spu_mixer_run_channels(state, (unsigned char *)mix, samples);
    spu_channel_advance_position(state, mix, samples, 0);
    spu_channel_advance_position(state, mix, samples, 1);

    if (*(uint32_t *)playing == 0) {
        uint32_t first_row = SPU_OUTPUT_SAMPLES - written;
        if (first_row > words) first_row = words;
        fill(state->output_samples + written, 0, (uint64_t)first_row * 2);
        if (words > first_row)
            fill(state->output_samples, 0, (uint64_t)(words - first_row) * 2);
    } else if (words != 0) {
        for (uint32_t i = 0; i < words; i++) {
            int32_t v = mix[i] >> 12;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            state->output_samples[(written + i) & SPU_OUTPUT_MASK] = (int16_t)v;
        }
    }

    state->output_written = (written + words) & SPU_OUTPUT_MASK;
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>
#include "hires_runtime.h"

void spu_mixer_accumulate_pair(int16_t *dst, int32_t *acc, const int16_t *coef,
                        int32_t n, uint32_t off) {
#ifdef __ARM_NEON

    {
        const int16_t *pc = coef;
        const int32_t *ac = acc;
        const int16_t *p5 = (const int16_t *)(acc + off);
        int16_t *pd = dst;
        int32_t k = n;
        do {
            int16x8_t   c   = vld1q_s16(pc); pc += 8;
            int32x4x2_t a01 = vld2q_s32(ac); ac += 8;
            int16x8x2_t b   = vld2q_s16(p5); p5 += 16;
            int32x4x2_t a23 = vld2q_s32(ac); ac += 8;
            int16x8x2_t o;

            int32x4_t v1 = vmlal_s16(a01.val[0], vget_low_s16(b.val[0]), vget_low_s16(c));
            int32x4_t v2 = vmlal_s16(a01.val[1], vget_low_s16(b.val[1]), vget_low_s16(c));
            int32x4_t v3 = vmlal_high_s16(a23.val[0], b.val[0], c);
            int32x4_t v4 = vmlal_high_s16(a23.val[1], b.val[1], c);

            o.val[0] = vshrq_n_s16(vcombine_s16(vshrn_n_s32(v1, 16), vshrn_n_s32(v3, 16)), 3);
            o.val[1] = vshrq_n_s16(vcombine_s16(vshrn_n_s32(v2, 16), vshrn_n_s32(v4, 16)), 3);
            vst2q_s16(pd, o); pd += 16;
            k -= 8;
        } while (k > 0);
        return;
    }
#else

    const int16_t *m = (const int16_t *)((unsigned char *)acc
                                         + (uint64_t)off * 4);
    do {
        for (int j = 0; j < 16; j++) {
            int32_t a = acc[j] + (int32_t)m[j] * (int32_t)coef[j >> 1];
            int16_t t = (int16_t)(a >> 16);
            dst[j] = (int16_t)(t >> 3);
        }
        acc += 16;
        m += 16;
        coef += 8;
        dst += 16;
        n -= 8;
    } while (n > 0);
#endif
}

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

void spu_mixer_accumulate_triple(uint8_t *param_1, int16_t *param_2, int32_t *param_3,
                         int32_t param_4, uint32_t param_5)
{

    uint8_t *out_a = param_1;
    uint8_t *out_b = param_1 + (uint64_t)param_5;
    uint8_t *out_c = param_1 + (uint64_t)param_5 * 2u;

    uint8_t *in_a = (uint8_t *)param_2;
    uint8_t *in_b = in_a + (uint64_t)param_5;
    uint8_t *in_c = in_a + (uint64_t)param_5 * 2u;
    uint8_t *acc_a = in_c + (uint64_t)param_5;
    uint8_t *acc_b = acc_a + (uint64_t)param_5 * 2u;
    uint8_t *acc_c = acc_b + (uint64_t)param_5 * 2u;

    uint8_t *coef_p = (uint8_t *)param_3;

    int32_t count = param_4;

#ifdef __ARM_NEON

    do {
        int16x8_t c  = vld1q_s16((const int16_t *)coef_p);          coef_p += 16;
        int16x8_t sa = vld1q_s16((const int16_t *)in_a);            in_a   += 16;
        int16x8_t sb = vld1q_s16((const int16_t *)in_b);            in_b   += 16;
        int16x8_t sc = vld1q_s16((const int16_t *)in_c);            in_c   += 16;

        int32x4_t a0 = vld1q_s32((const int32_t *)acc_a);
        int32x4_t a1 = vld1q_s32((const int32_t *)acc_a + 4);       acc_a += 32;
        int32x4_t b0 = vld1q_s32((const int32_t *)acc_b);
        int32x4_t b1 = vld1q_s32((const int32_t *)acc_b + 4);       acc_b += 32;
        int32x4_t d0 = vld1q_s32((const int32_t *)acc_c);
        int32x4_t d1 = vld1q_s32((const int32_t *)acc_c + 4);       acc_c += 32;

        a0 = vmlal_s16(a0, vget_low_s16(sa),  vget_low_s16(c));
        a1 = vmlal_s16(a1, vget_high_s16(sa), vget_high_s16(c));
        b0 = vmlal_s16(b0, vget_low_s16(sb),  vget_low_s16(c));
        b1 = vmlal_s16(b1, vget_high_s16(sb), vget_high_s16(c));
        d0 = vmlal_s16(d0, vget_low_s16(sc),  vget_low_s16(c));
        d1 = vmlal_s16(d1, vget_high_s16(sc), vget_high_s16(c));

        uint16x8_t na = vcombine_u16(vshrn_n_u32(vreinterpretq_u32_s32(a0), 16),
                                     vshrn_n_u32(vreinterpretq_u32_s32(a1), 16));
        uint16x8_t nb = vcombine_u16(vshrn_n_u32(vreinterpretq_u32_s32(b0), 16),
                                     vshrn_n_u32(vreinterpretq_u32_s32(b1), 16));
        uint16x8_t nc = vcombine_u16(vshrn_n_u32(vreinterpretq_u32_s32(d0), 16),
                                     vshrn_n_u32(vreinterpretq_u32_s32(d1), 16));

        vst1_u8(out_a, vshrn_n_u16(na, 2));  out_a += 8;
        vst1_u8(out_b, vshrn_n_u16(nb, 2));  out_b += 8;
        vst1_u8(out_c, vshrn_n_u16(nc, 2));  out_c += 8;

        count -= 8;
    } while (count > 0);
#else
    do {
        int16_t coef[8];
        memcpy(coef, coef_p, sizeof(coef));
        coef_p += 16;

        int32_t acc[3][8];
        memcpy(acc[0], acc_a, sizeof(acc[0]));
        acc_a += 32;
        memcpy(acc[1], acc_b, sizeof(acc[1]));
        acc_b += 32;
        memcpy(acc[2], acc_c, sizeof(acc[2]));
        acc_c += 32;

        int16_t samp[3][8];
        memcpy(samp[0], in_a, sizeof(samp[0]));
        in_a += 16;
        memcpy(samp[1], in_b, sizeof(samp[1]));
        in_b += 16;
        memcpy(samp[2], in_c, sizeof(samp[2]));
        in_c += 16;

        uint8_t outbuf[3][8];
        for (int ch = 0; ch < 3; ch++) {
            for (int i = 0; i < 8; i++) {
                int32_t v = acc[ch][i] + (int32_t)samp[ch][i] * (int32_t)coef[i];
                outbuf[ch][i] = (uint8_t)((uint32_t)v >> 18);
            }
        }

        memcpy(out_a, outbuf[0], 8);
        out_a += 8;
        memcpy(out_b, outbuf[1], 8);
        out_b += 8;
        memcpy(out_c, outbuf[2], 8);
        out_c += 8;

        count -= 8;
    } while (count > 0);
#endif
}

#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "spi.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "mem_access.h"

uint32_t spi_tsc_transfer_byte(spi_tsc_t *tsc, unsigned char *base,
                            uint32_t value)
{

    uint32_t cmd    = tsc->command;
    uint32_t state = tsc->state;

    uint32_t channel = (cmd >> 4) & 7u;

    uint32_t val = tsc->channels[channel];

    if (channel == 6u) {
        spu_t *spu = &((nds_t *)base)->spu;
        if (spu->mixer.mic_loaded == 0)
            spu->mixer.mic_init_pending = 1;

        uint32_t m = spu_resample_sample_by_time(spu, ((nds_t *)base)->sched.cycles);
        uint32_t centered = ((uint32_t)(int32_t)(int16_t)m + TSC_MIC_SAMPLE_BIAS) >> 4;
        tsc->channels[6] = (uint16_t)centered;
    }

    if (state == 2u) {
        val = 0;
    } else if (state == 1u) {
        uint32_t x = val << 3;

        val = (cmd & 8u) ? (x & 0x7ff80u) : x;
        tsc->state = 2;
    } else if (state == 0u) {
        tsc->state = 1;
        val >>= 5;
    }

    if ((int8_t)(uint8_t)value < 0) {
        tsc->command = (unsigned char)value;
        tsc->state = 0;
    }

    return val;
}
void spi_control_write(spi_t *spi, uint32_t value) {

    if ((int16_t)value >= 0) {
        uint32_t old = spi->control;
        if ((old & 0x8300) == 0x8100)
            spi_memory_deselect(&spi->firmware);
    }
    spi->control = (uint16_t)value;
}



uint32_t spi_transfer_byte(spi_t *spi, uint32_t value) {

    uint32_t ctrl = spi->control;
    if (!(ctrl & 0x8000u)) return 0;

    spi_tsc_t *tsc = &spi->tsc;
    spi_powerman_t *pm = &spi->powerman;
    uint32_t acc = 0;

    switch ((ctrl >> 8) & 3u) {

    case 0: {
        uint32_t r, mark;

        if (pm->active == 0) {
            r = 0;
            mark = 1;
            pm->index = (uint8_t)value;
        } else {
            uint32_t prev = pm->index;
            uint32_t idx = prev & 0x7fu;
            uint8_t *cell = pm->regs + idx;
            r = *cell;
            mark = 0;

            if (!(prev & 0x80u) && idx != 1)
                *cell = (uint8_t)value;
        }
        pm->active = (uint8_t)(mark & (ctrl >> 11));
        return r;
    }

    case 1:

        acc = spi_memory_transfer_byte(&spi->firmware, value & 0xffu);
        if (!(ctrl & (1u << 11)))

            spi_memory_deselect(&spi->firmware);
        return acc;

    case 2: {
        uint32_t cmd    = tsc->command;
        uint32_t state = tsc->state;
        uint32_t channel  = (cmd >> 4) & 7u;

        acc = tsc->channels[channel];

        if (channel == 6) {
            uint8_t *b = (uint8_t *)spi->machine;
            spu_t *spu = &((nds_t *)b)->spu;
            if (spu->mixer.mic_loaded == 0) spu->mixer.mic_init_pending = 1;
            uint32_t v = spu_resample_sample_by_time(spu, ((nds_t *)b)->sched.cycles);
            tsc->channels[6] = (uint16_t)((((uint32_t)(int32_t)(int16_t)v)
                                            + TSC_MIC_SAMPLE_BIAS) >> 4);
        }

        if (state == 2) {
            acc = 0;
        } else if (state == 1) {
            uint32_t x = acc << 3;
            acc = (cmd & 8u) ? (x & 0x7ff80u) : x;
            tsc->state = 2;
        } else if (state == 0) {
            tsc->state = 1;
            acc >>= 5;
        }
        break;
    }

    default:
        return 0;
    }

    if ((int8_t)value < 0) {
        tsc->command = (uint8_t)value;
        tsc->state   = 0;
    }
    if (!(ctrl & (1u << 11)))
        tsc->state = 0;

    return acc;
}
void spi_reset(spi_t *spi) {

    spi_memory_reset(&spi->firmware);

    memset(spi->tsc.channels, 0, sizeof spi->tsc.channels);
    spi->tsc.command = 0;
    spi->tsc.state = 0;
    spi->powerman.active = 0;
    spi->powerman.index = 0;
    spi->powerman.regs[0] = 0x7f;
    spi->powerman.regs[1] = 0;
    spi->powerman.regs[2] = 0;
    spi->powerman.regs[3] = 0;
    spi->powerman.regs[4] = 15;
    spi->control = 0;
}

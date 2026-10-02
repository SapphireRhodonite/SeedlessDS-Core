#include <stdint.h>
#include "seedlessds/cpu_backend.h"
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "rtc.h"
#include "cpu/arm.h"
#include "ipc.h"
#include "dma.h"
#include "core/nds_state.h"
#include "core/state_format.h"
#include "core/data_paths.h"
#include <stdio.h>
state_write_work_t state_write_work;
#define SS(p) ((state_stream_t *)(p))
#define CPU(p) ((arm_t *)(p))
#include <zlib.h>
#include "mem_access.h"

static uint8_t *state_stream_cursor(const uint8_t *stream) { return SS(stream)->cursor; }

static void state_stream_seek(uint8_t *stream, uint8_t *c) { SS(stream)->cursor = c; }

static int state_stream_framed(const uint8_t *stream) { return SS(stream)->framed != 0; }

static uint8_t *state_section_open(uint8_t *stream, uint32_t id) {
    uint8_t *h = state_stream_cursor(stream);
    state_section_header_t hdr = { id, 0, 0, 0 };
    memcpy(h, &hdr, sizeof hdr);
    state_stream_seek(stream, h + sizeof hdr);
    return h;
}

static void state_section_close(uint8_t *stream, uint8_t *h) {
    uint8_t *body = h + sizeof(state_section_header_t);
    uint32_t size = (uint32_t)(state_stream_cursor(stream) - body);
    uint32_t crc = (uint32_t)crc32(0, body, size);
    memcpy(h + 4, &size, 4);
    memcpy(h + 8, &crc, 4);
}

static uint8_t *state_section_enter(uint8_t *stream) {
    uint8_t *h = state_stream_cursor(stream);
    if (state_stream_framed(stream)) state_stream_seek(stream, h + sizeof(state_section_header_t));
    return h;
}

static void state_section_leave(uint8_t *stream, const uint8_t *h) {
    uint32_t size;
    if (!state_stream_framed(stream)) return;
    memcpy(&size, h + 4, 4);
    state_stream_seek(stream, (uint8_t *)h + sizeof(state_section_header_t) + size);
}

static uint32_t state_expected_sections(uint32_t flags, uint32_t *ids) {
    uint32_t n = 0;
    if (flags & STATE_FLAG_SCREENS) ids[n++] = STATE_SECTION_SCREENS;
    if (flags & STATE_FLAG_SPI_MEMORY) ids[n++] = STATE_SECTION_SPI_MEMORY;
    ids[n++] = STATE_SECTION_ARM9;
    ids[n++] = STATE_SECTION_ARM7;
    ids[n++] = STATE_SECTION_MEMORY;
    ids[n++] = STATE_SECTION_DMA;
    ids[n++] = STATE_SECTION_IPC_FIFO;
    ids[n++] = STATE_SECTION_VRAM_CONTROL;
    ids[n++] = STATE_SECTION_GPU2D;
    ids[n++] = STATE_SECTION_SPU;
    ids[n++] = STATE_SECTION_CART;
    ids[n++] = STATE_SECTION_SPI;
    ids[n++] = STATE_SECTION_RTC;
    ids[n++] = STATE_SECTION_SCHED;
    ids[n++] = STATE_SECTION_MACHINE;
    return n;
}

static int state_sections_valid(const uint8_t *load, size_t len, uint32_t flags) {
    uint32_t ids[STATE_SECTION_COUNT_MAX];
    uint32_t count = state_expected_sections(flags, ids);
    const uint8_t *p = load, *end = load + len;
    for (uint32_t i = 0; i < count; i++) {
        state_section_header_t hdr;
        if ((size_t)(end - p) < sizeof hdr) return 0;
        memcpy(&hdr, p, sizeof hdr);
        p += sizeof hdr;
        if (hdr.id != ids[i] || (size_t)(end - p) < hdr.size) return 0;
        if ((uint32_t)crc32(0, p, hdr.size) != hdr.crc) return 0;
        p += hdr.size;
    }
    return 1;
}

void state_read_rtc_block(rtc_t *rtc, unsigned char *entry, uint32_t mode);
void state_write_rtc_block(rtc_t *rtc, void *param_2, uint32_t param_3);
void state_write_dma_block(dma_t *dma, unsigned char *obj, uint32_t mode);
void state_read_spi_block(spi_t *spi, uint8_t *reader, uint32_t size);
void state_write_spi_block(spi_t *spi, uint8_t *desc, uint32_t mode);
void state_read_spi_memory_status(spi_memory_t *m, unsigned char *reader);
static unsigned char *state_read_spi_memory_status_result(spi_memory_t *m, unsigned char *reader);
void state_write_spi_memory_status(const spi_memory_t *m, unsigned char *desc);
#include <stddef.h>
#include "frontend/frontend.h"
#include "seedlessds/platform.h"
#include "core_internals.h"

#define BUF_SIZE   0x30000u
#define GLOB      0x14c000
#define G_FLAG 0x14c46au
typedef void    *(*fn_malloc)(uint64_t);
typedef void     (*fn_free)(void *);

static uint16_t swap_rb(uint16_t v) {
    uint32_t vv = v;
    return (uint16_t)((vv & 0x7e0u) | (vv >> 11) | ((vv << 11) & 0xf800u));
}

int32_t state_save_slot_with_screens(uint32_t param_1) {

    void *buf = ((fn_malloc)sym_libc_malloc)(BUF_SIZE);
    if (buf == NULL)
        return -1;

    uint8_t *top = (uint8_t *)buf;
    uint8_t *bot = top + NDS_SCREEN_BYTES;

    video_out_dump_screen_rgb565(top, 0u);
    video_out_dump_screen_rgb565(bot, 1u);

    if ((FRONTEND->config_bits & FRONTEND_CONFIG_SCREENSHOT_SWAP_RB) != 0) {
        uint32_t off;
        for (off = 0; off < BUF_SIZE; off += 2u) {
            uint8_t *p = top + off;
            wr16(p, swap_rb(rd16(p)));
        }
    }

    void *ctx = FRONTEND->machine;
    int32_t r = state_save_slot(ctx, param_1, top, bot);

    ((fn_free)sym_libc_free)(top);
    return r;
}
#undef BUF_SIZE
#undef GLOB
#undef G_FLAG

#define GLOB 0x14c000
extern void state_load_slot_1(unsigned char *obj, uint32_t slot, void *a2,
                                void *a3, uint32_t a4) __asm__("state_load_slot");

void state_load_slot_request(uint32_t param_1) {

    void *ctx = FRONTEND->machine;

    state_load_slot((unsigned char *)ctx, param_1, 0, 0, 0);
}
#undef GLOB


void state_read_memory_blocks_finish(bus_t *bus);

void state_read_machine_blocks(uint8_t *machine, uint8_t *stream, uint32_t mode)
{

    nds_runtime_t *runtime = &((nds_t *)machine)->runtime;
    arm_t *arm9 = &((nds_t *)machine)->arm9;
    arm_t *arm7 = &((nds_t *)machine)->arm7;
    uint8_t *cursor;

    if (runtime->jit_enabled != 0) {
        arm7->jit_block = 0;
        nds_cpu_backend_default()->flush_all(arm9);
    }

    bus_t *bus = &((nds_t *)machine)->bus;
    uint8_t *section;

    section = state_section_enter(stream);
    state_read_cpu_block((uint8_t *)arm9, stream);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    state_read_cpu_block((uint8_t *)arm7, stream);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    state_read_memory_blocks(bus, stream, mode);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    dma_channels_read_state_block(&bus->dma[0], stream, mode);
    dma_channels_read_state_block(&bus->dma[1], stream, mode);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    ipc_fifo_state_read(&bus->ipc_fifo[0], stream);
    ipc_fifo_state_read(&bus->ipc_fifo[1], stream);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    cp15_load_state(&bus->cp15, stream, mode);
    state_section_leave(stream, section);
    state_read_memory_blocks_finish(bus);
    section = state_section_enter(stream);
    gpu2d_worker_reset_engine_entries((uint8_t *)&((nds_t *)machine)->gpu, stream, mode);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    state_read_spu_block(&((nds_t *)machine)->spu, stream, mode);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    state_read_cart_block(&((nds_t *)machine)->cart, stream, mode);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    state_read_spi_block(&((nds_t *)machine)->spi, stream, mode);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    state_read_rtc_block(&((nds_t *)machine)->rtc, stream, mode);
    state_section_leave(stream, section);
    section = state_section_enter(stream);
    state_read_sched_block(&((nds_t *)machine)->sched, stream, mode);
    state_section_leave(stream, section);

    section = state_section_enter(stream);
    cursor = SS(stream)->cursor;
    wr64(machine, rd64(cursor));

    cursor = SS(stream)->cursor;
    SS(stream)->cursor = cursor + 8;
    wr64(machine + 8, rd64(cursor + 8));

    cursor = SS(stream)->cursor;
    SS(stream)->cursor = cursor + 8;
    wr16(machine + 20, rd16(cursor + 8));

    cursor = SS(stream)->cursor;
    SS(stream)->cursor = cursor + 2;
    state_section_leave(stream, section);

    if (runtime->jit_enabled != 0) {
        uint8_t *table = (uint8_t *)arm9->cp15;
        uint32_t mask_a = arm9->cpsr;
        uint32_t address = rd32(table + 16);
        uint32_t mask_b;
        void *translation;

        arm9->cpsr = mask_a & UINT32_C(0xffffffdf);
        mask_b = arm7->cpsr;
        arm7->cpsr = mask_b & UINT32_C(0xffffffdf);

        translation = jit_cache_lookup_or_compile((uint8_t *)arm9, address + 8);
        arm9->jit_swi_entry = (uint8_t *)translation;
        translation = jit_cache_lookup_or_compile((uint8_t *)arm9, address + 24);
        arm9->jit_irq_entry = (uint8_t *)translation;
        translation = jit_cache_lookup_or_compile((uint8_t *)arm7, 8);
        arm7->jit_swi_entry = (uint8_t *)translation;
        translation = jit_cache_lookup_or_compile((uint8_t *)arm7, 24);
        arm7->jit_irq_entry = (uint8_t *)translation;
        translation = jit_cache_lookup_or_compile((uint8_t *)arm9,
                                              arm9->pc);
        arm9->jit_block = (uint8_t *)translation + 8;
        translation = jit_cache_lookup_or_compile((uint8_t *)arm7,
                                              arm7->pc);
        arm7->jit_block = (uint8_t *)translation + 8;
    }

    arm9->debug.instruction_count = 0;
    arm7->debug.instruction_count = 0;
    gpu2d_worker_hook_noop();

    {
        uint64_t clock;
        time_now_microseconds(&clock);
        runtime->pacer_flag = 0;
        wr32(&runtime->skip_frame, 0);
        runtime->pacer_base = clock * 3;
        runtime->pacer_accumulated = 0;
    }
}



void state_write_machine_blocks(uint8_t *machine, uint8_t *stream, uint32_t mode) {
    bus_t *bus = &((nds_t *)machine)->bus;
    uint8_t *section;

    section = state_section_open(stream, STATE_SECTION_ARM9);
    state_write_cpu_block(&((nds_t *)machine)->arm9, stream);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_ARM7);
    state_write_cpu_block(&((nds_t *)machine)->arm7, stream);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_MEMORY);
    state_write_memory_blocks((uint8_t *)bus, stream, mode);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_DMA);
    state_write_dma_block(&bus->dma[0], stream, mode);
    state_write_dma_block(&bus->dma[1], stream, mode);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_IPC_FIFO);
    ipc_fifo_state_write(&bus->ipc_fifo[0], stream);
    ipc_fifo_state_write(&bus->ipc_fifo[1], stream);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_VRAM_CONTROL);
    state_write_three_words((uint8_t *)&bus->cp15, stream);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_GPU2D);
    gpu2d_worker_save_state_chain((uint8_t *)&((nds_t *)machine)->gpu, stream, mode);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_SPU);
    state_write_spu_block(&((nds_t *)machine)->spu, stream);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_CART);
    state_write_cart_block(&((nds_t *)machine)->cart, stream, mode);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_SPI);
    state_write_spi_block(&((nds_t *)machine)->spi, stream, mode);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_RTC);
    state_write_rtc_block(&((nds_t *)machine)->rtc, stream, mode);
    state_section_close(stream, section);
    section = state_section_open(stream, STATE_SECTION_SCHED);
    state_write_sched_block(&((nds_t *)machine)->sched, stream);
    state_section_close(stream, section);

    section = state_section_open(stream, STATE_SECTION_MACHINE);
    uint8_t *cursor = SS(stream)->cursor;
    wr64(cursor, rd64(machine));

    cursor = SS(stream)->cursor;
    SS(stream)->cursor = cursor + 8;
    wr64(cursor + 8, rd64(machine + 8));

    cursor = SS(stream)->cursor;
    SS(stream)->cursor = cursor + 8;
    wr16(cursor + 8, rd16(machine + 20));

    cursor = SS(stream)->cursor;
    SS(stream)->cursor = cursor + 2;
    state_section_close(stream, section);
}


void state_read_memory_blocks(bus_t *bus, uint8_t *stream, uint32_t mode)
{
    uint8_t *origin;
    uint8_t *dest;

    dest = bus->main_ram;
    origin = SS(stream)->cursor;
    memcpy(dest, origin, NDS_MAIN_RAM_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_MAIN_RAM_SIZE;
    dest = bus->itcm;
    memcpy(dest, origin + NDS_MAIN_RAM_SIZE, NDS_ITCM_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_ITCM_SIZE;
    dest = bus->shared_wram;
    memcpy(dest, origin + NDS_ITCM_SIZE, NDS_SHARED_WRAM_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_SHARED_WRAM_SIZE;
    dest = bus->dtcm;
    memcpy(dest, origin + NDS_SHARED_WRAM_SIZE, NDS_DTCM_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_DTCM_SIZE;
    memcpy(bus->arm7_wram, origin + NDS_DTCM_SIZE, NDS_ARM7_WRAM_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_ARM7_WRAM_SIZE;
    dest = bus->vram_bank[0];
    memcpy(dest, origin + NDS_ARM7_WRAM_SIZE, NDS_VRAM_ABCD_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_ABCD_SIZE;
    dest = bus->vram_bank[1];
    memcpy(dest, origin + NDS_VRAM_ABCD_SIZE, NDS_VRAM_ABCD_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_ABCD_SIZE;
    dest = bus->vram_bank[2];
    memcpy(dest, origin + NDS_VRAM_ABCD_SIZE, NDS_VRAM_ABCD_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_ABCD_SIZE;
    dest = bus->vram_bank[3];
    memcpy(dest, origin + NDS_VRAM_ABCD_SIZE, NDS_VRAM_ABCD_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_ABCD_SIZE;
    dest = bus->vram_bank[4];
    memcpy(dest, origin + NDS_VRAM_ABCD_SIZE, NDS_VRAM_E_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_E_SIZE;
    dest = bus->vram_bank[5];
    memcpy(dest, origin + NDS_VRAM_E_SIZE, NDS_VRAM_FGI_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_FGI_SIZE;
    dest = bus->vram_bank[6];
    memcpy(dest, origin + NDS_VRAM_FGI_SIZE, NDS_VRAM_FGI_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_FGI_SIZE;
    dest = bus->vram_bank[7];
    memcpy(dest, origin + NDS_VRAM_FGI_SIZE, NDS_VRAM_H_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_H_SIZE;
    dest = bus->vram_bank[8];
    memcpy(dest, origin + NDS_VRAM_H_SIZE, NDS_VRAM_FGI_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_VRAM_FGI_SIZE;
    memcpy(bus->oam, origin + NDS_VRAM_FGI_SIZE, NDS_OAM_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_OAM_SIZE;
    memcpy(bus->palette, origin + NDS_OAM_SIZE, NDS_PALETTE_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_PALETTE_SIZE;
    memcpy(bus->arm7_io_scratch, origin + NDS_PALETTE_SIZE, NDS_WIFI_RAM_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + NDS_WIFI_RAM_SIZE;
    memcpy(bus->io_mirror, origin + NDS_WIFI_RAM_SIZE, IO_MIRROR_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + IO_MIRROR_SIZE;
    memcpy(&bus->io_mirror[1], origin + IO_MIRROR_SIZE, IO_MIRROR_SIZE);

    origin = SS(stream)->cursor;
    SS(stream)->cursor = origin + IO_MIRROR_SIZE;
    if (mode >= 2) {
        uint8_t q0[16];
        uint8_t q1[16];
        uint8_t q2[16];
        uint8_t q3[16];

        memcpy(bus->wifi_regs, origin + IO_MIRROR_SIZE, STATE_WIFI_REGS_SIZE);

        origin = SS(stream)->cursor;
        SS(stream)->cursor = origin + 0x400;

        memcpy(q0, origin + 0x460, sizeof(q0));
        memcpy(q1, origin + 0x470, sizeof(q1));
        memcpy(q2, origin + 0x440, sizeof(q2));
        memcpy(q3, origin + 0x450, sizeof(q3));
        memcpy(bus->wifi_regs + 0x400 + 0x60, q0, sizeof(q0));
        memcpy(bus->wifi_regs + 0x400 + 0x70, q1, sizeof(q1));
        memcpy(bus->wifi_regs + 0x400 + 0x40, q2, sizeof(q2));
        memcpy(bus->wifi_regs + 0x400 + 0x50, q3, sizeof(q3));

        memcpy(q0, origin + 0x420, sizeof(q0));
        memcpy(q1, origin + 0x430, sizeof(q1));
        memcpy(q2, origin + 0x400, sizeof(q2));
        memcpy(q3, origin + 0x410, sizeof(q3));
        memcpy(bus->wifi_regs + 0x400 + 0x20, q0, sizeof(q0));
        memcpy(bus->wifi_regs + 0x400 + 0x30, q1, sizeof(q1));
        memcpy(bus->wifi_regs + 0x400, q2, sizeof(q2));
        memcpy(bus->wifi_regs + 0x400 + 0x10, q3, sizeof(q3));

        origin = SS(stream)->cursor;
        SS(stream)->cursor = origin + 0x80;
    }

}

void state_read_memory_blocks_finish(bus_t *bus)
{
    if ((bus->bios_flags & 2u) != 0) {
        uint8_t q0[16];
        uint8_t q1[16];
        uint8_t q2[16];
        uint8_t q3[16];
        uint8_t *table = (uint8_t *)bus->cart->rom->data;
        memcpy(q0, table + 0xe0, sizeof(q0));
        memcpy(q3, table + 0xf0, sizeof(q3));
        memcpy(q1, table + 0x100, sizeof(q1));
        memcpy(q2, table + 0x110, sizeof(q2));
        memcpy(bus->bios9 + 0x40, q0, sizeof(q0));
        memcpy(bus->bios9 + 0x70, q2, sizeof(q2));
        memcpy(bus->bios9 + 0x60, q1, sizeof(q1));
        memcpy(bus->bios9 + 0x50, q3, sizeof(q3));

        memcpy(q1, table + 0xc0, sizeof(q1));
        memcpy(q0, table + 0xd0, sizeof(q0));
        memcpy(bus->bios9 + 0x30, q0, sizeof(q0));
        memcpy(bus->bios9 + 0x20, q1, sizeof(q1));

        memcpy(q0, table + 0x120, sizeof(q0));
        memcpy(q1, table + 0x130, sizeof(q1));
        memcpy(q2, table + 0x140, sizeof(q2));
        table += 0xc0;
        memcpy(q3, table + 0x8e, sizeof(q3));
        memcpy(bus->bios9 + 0x80, q0, sizeof(q0));
        memcpy(bus->bios9 + 0xae, q3, sizeof(q3));
        memcpy(bus->bios9 + 0xa0, q2, sizeof(q2));
        memcpy(bus->bios9 + 0x90, q1, sizeof(q1));
    }

    wram_apply_wramcnt(bus);
}

typedef void *(*fn_memcpy)(void *, const void *, size_t);
static void copy16(void *dst, const void *src)
{
    memcpy(dst, src, 16);
}

void *state_write_memory_blocks(unsigned char *ctx, unsigned char *output,
                         uint32_t mode)
{

    static fn_memcpy copy;
    state_stream_t *stream = SS(output);
    unsigned char *dst;
    unsigned char block0[16], block1[16], block2[16], block3[16];

    if (!copy)
        copy = (fn_memcpy)sym_libc_memcpy;

    dst = stream->cursor;
    copy(dst, ((bus_t *)ctx)->main_ram, NDS_MAIN_RAM_SIZE);

    dst = stream->cursor + NDS_MAIN_RAM_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->itcm, NDS_ITCM_SIZE);

    dst = stream->cursor + NDS_ITCM_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->shared_wram, NDS_SHARED_WRAM_SIZE);

    dst = stream->cursor + NDS_SHARED_WRAM_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->dtcm, NDS_DTCM_SIZE);

    dst = stream->cursor + NDS_DTCM_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->arm7_wram, NDS_ARM7_WRAM_SIZE);

    dst = stream->cursor + NDS_ARM7_WRAM_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[0], NDS_VRAM_ABCD_SIZE);

    dst = stream->cursor + NDS_VRAM_ABCD_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[1], NDS_VRAM_ABCD_SIZE);

    dst = stream->cursor + NDS_VRAM_ABCD_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[2], NDS_VRAM_ABCD_SIZE);

    dst = stream->cursor + NDS_VRAM_ABCD_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[3], NDS_VRAM_ABCD_SIZE);

    dst = stream->cursor + NDS_VRAM_ABCD_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[4], NDS_VRAM_E_SIZE);

    dst = stream->cursor + NDS_VRAM_E_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[5], NDS_VRAM_FGI_SIZE);

    dst = stream->cursor + NDS_VRAM_FGI_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[6], NDS_VRAM_FGI_SIZE);

    dst = stream->cursor + NDS_VRAM_FGI_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[7], NDS_VRAM_H_SIZE);

    dst = stream->cursor + NDS_VRAM_H_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->vram_bank[8], NDS_VRAM_FGI_SIZE);

    dst = stream->cursor + NDS_VRAM_FGI_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->oam, NDS_OAM_SIZE);

    dst = stream->cursor + NDS_OAM_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->palette, NDS_PALETTE_SIZE);

    dst = stream->cursor + NDS_PALETTE_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->arm7_io_scratch, NDS_WIFI_RAM_SIZE);

    dst = stream->cursor + NDS_WIFI_RAM_SIZE;
    stream->cursor = dst;
    copy(dst, ((bus_t *)ctx)->io_mirror, IO_MIRROR_SIZE);

    dst = stream->cursor + IO_MIRROR_SIZE;
    stream->cursor = dst;
    copy(dst, &((bus_t *)ctx)->io_mirror[1], IO_MIRROR_SIZE);

    dst = stream->cursor + IO_MIRROR_SIZE;
    stream->cursor = dst;

    if (mode >= 2) {
        copy(dst, ((bus_t *)ctx)->wifi_regs, STATE_WIFI_REGS_SIZE);

        dst = stream->cursor;
        stream->cursor = dst + STATE_WIFI_REGS_SIZE;

        copy16(block0, ((bus_t *)ctx)->wifi_regs + 0x400 + 96);
        copy16(block1, ((bus_t *)ctx)->wifi_regs + 0x400 + 112);
        copy16(block2, ((bus_t *)ctx)->wifi_regs + 0x400 + 64);
        copy16(block3, ((bus_t *)ctx)->wifi_regs + 0x400 + 80);
        memcpy(dst + 1136, block1, 16);
        memcpy(dst + 1120, block0, 16);
        memcpy(dst + 1104, block3, 16);
        memcpy(dst + 1088, block2, 16);

        copy16(block0, ((bus_t *)ctx)->wifi_regs + 0x400 + 32);
        copy16(block1, ((bus_t *)ctx)->wifi_regs + 0x400 + 48);
        copy16(block2, ((bus_t *)ctx)->wifi_regs + 0x400);
        copy16(block3, ((bus_t *)ctx)->wifi_regs + 0x400 + 16);
        memcpy(dst + 1072, block1, 16);
        memcpy(dst + 1056, block0, 16);
        memcpy(dst + 1040, block3, 16);
        memcpy(dst + 1024, block2, 16);

        dst = stream->cursor + 0x80;
        stream->cursor = dst;
    }

    return (unsigned char *)&((bus_t *)ctx)->cp15;
}

void state_write_sched_block(void *param_1, void *param_2)
{
    const sched_t *entry = param_1;
    uint8_t *state = param_2;
    uint8_t types[16] = {0};
    uint32_t values[16] = {0};
    const sched_deadline_t *node = entry->head;
    uint32_t count = 0;

    if (node != 0) {
        const sched_deadline_t *next = node;

        do {
            next = next->next;
            count++;
        } while (next != 0);

        {
            uint32_t idx = 0;

            do {
                wr8(types + idx, node->index);
                wr32((uint8_t *)values + (size_t)idx * 4, node->remaining);
                node = node->next;
                idx++;
            } while (node != 0);
        }
    }

    {
        uint8_t *cursor = rd_ptr_u8(state + 0x20);

        wr8(cursor, (uint8_t)count);

        cursor = rd_ptr_u8(state + 0x20);
        wr_ptr(state + 0x20, cursor + 1);
        memcpy(cursor + 1, types, sizeof(types));

        cursor = rd_ptr_u8(state + 0x20);
        wr_ptr(state + 0x20, cursor + 0x10);
        memcpy(cursor + 0x30,
               (const uint8_t *)values + 0x20, 0x20);
        memcpy(cursor + 0x10, values, 0x20);

        cursor = rd_ptr_u8(state + 0x20);
        wr_ptr(state + 0x20, cursor + 0x40);
    }
}

typedef void *(*fn_memcpy_chk)(void *, const void *, size_t, size_t);

void *state_read_sched_block(sched_t *param_1, uint8_t *param_2, uint32_t param_3)
{

    static fn_memcpy_chk core_memcpy_chk;
    if (core_memcpy_chk == NULL)
        core_memcpy_chk = (fn_memcpy_chk)fortify_memcpy;

    uint8_t *cursor = rd_ptr_u8(param_2 + 32);
    uint8_t count = rd8(cursor);
    cursor += 1;
    wr_ptr(param_2 + 32, cursor);

    size_t width = (param_3 > 3u) ? 16u : 11u;
    uint8_t indices[16];
    uint32_t values[16];

    core_memcpy_chk(indices, cursor, width, 16u);

    cursor = rd_ptr_u8(param_2 + 32);
    const uint8_t *origin_values = cursor + width;
    size_t bytes_values = width << 2;
    wr_ptr(param_2 + 32, origin_values);
    void *ret = core_memcpy_chk(values, origin_values, bytes_values, 64u);

    cursor = rd_ptr_u8(param_2 + 32);
    wr_ptr(param_2 + 32, cursor + bytes_values);

    if (count == 0) {
        param_1->head = 0;
        return ret;
    }

    uint8_t last_index = rd8(indices);
    sched_deadline_t *node = &param_1->deadline[last_index];
    node->prev = 0;
    node->remaining = rd32(values);
    param_1->head = node;

    if (count != 1) {
        uint32_t remaining = (uint32_t)count - 1u;
        size_t i = 1;
        do {
            last_index = rd8(indices + i);
            uint32_t value = rd32((const uint8_t *)values + i * 4u);
            sched_deadline_t *next = &param_1->deadline[last_index];

            node->next = next;
            next->prev = node;
            node = next;
            node->remaining = value;

            ++i;
            --remaining;
        } while (remaining != 0);
    }

    node = &param_1->deadline[last_index];
    node->next = 0;
    return ret;
}

static unsigned char *advance(state_stream_t *stream, uint64_t n) {
    unsigned char *cur = stream->cursor;
    stream->cursor = cur + n;
    return cur + n;
}

void state_write_dma_block(dma_t *dma, unsigned char *obj, uint32_t mode)
{

    state_stream_t *stream = SS(obj);
    unsigned char *cur;
    unsigned char *d;
    uint32_t w;
    unsigned char b;
    cur = stream->cursor;

    if (mode < 4) {
        for (int k = 0; k < 4; k++) {
            dma_channel_t *channel = &dma->channels[k];
            w = channel->src;
            memcpy(cur, &w, 4);

            d = advance(stream, 4);
            w = channel->dst;
            memcpy(d, &w, 4);

            d = advance(stream, 4);
            w = channel->cnt;
            memcpy(d, &w, 4);

            d = advance(stream, 4);
            b = channel->start_mode;
            *d = b;

            cur = advance(stream, 1);
            channel->deadline_cycles = 0;
            channel->started = 0;
        }
        return;
    }
    {
        for (int k = 0; k < 4; k++) {
            dma_channel_t *channel = &dma->channels[k];
            uint64_t q;
            w = channel->src;
            memcpy(cur, &w, 4);
            d = advance(stream, 4);
            w = channel->dst;
            memcpy(d, &w, 4);
            d = advance(stream, 4);
            w = channel->cnt;
            memcpy(d, &w, 4);
            d = advance(stream, 4);
            b = channel->start_mode;
            *d = b;
            d = advance(stream, 1);
            q = channel->deadline_cycles;
            memcpy(d, &q, 8);
            cur = advance(stream, 8);
            if (mode > 4) {
                b = channel->started;
                *cur = b;
                cur = advance(stream, 1);
            } else {
                channel->started = 0;
            }
        }
    }
}

void state_read_cpu_block(uint8_t *dst, uint8_t *arg)
{

    uint8_t *slot = arg + 0x20;
    uint8_t *cursor = rd_ptr_u8(slot);

    for (uint32_t i = 0; i != 4; i++) {
        nds_timer_t *d = &((arm_t *)dst)->timers[i];
        uint64_t q = rd64(cursor);
        d->start_cycles = q;

        cursor = rd_ptr_u8(slot);
        wr_ptr(slot, cursor + 8);
        d->period_cycles = rd32(cursor + 8);

        cursor = rd_ptr_u8(slot);
        wr_ptr(slot, cursor + 4);
        d->reload = rd16(cursor + 4);

        cursor = rd_ptr_u8(slot);
        wr_ptr(slot, cursor + 2);
        d->control = rd16(cursor + 2);

        cursor = rd_ptr_u8(slot);
        wr_ptr(slot, cursor + 2);
        d->prescaler_shift = rd8(cursor + 2);

        cursor = rd_ptr_u8(slot);
        wr_ptr(slot, cursor + 1);
        d->scheduled = rd8(cursor + 1);

        cursor = rd_ptr_u8(slot);
        cursor = cursor + 1;
        wr_ptr(slot, cursor);
    }

    {
        uint8_t q0[16], q1[16], q2[16];
        uint64_t v = rd64(cursor + 48);
        memcpy(q1, cursor + 16, 16);
        memcpy(q0, cursor + 32, 16);
        memcpy(q2, cursor, 16);
        wr64(CPU(dst)->banked_sp_lr[ARM_BANK_SYS], v);
        memcpy(CPU(dst)->banked_sp_lr[ARM_BANK_ABT], q0, 16);
        memcpy(CPU(dst)->banked_sp_lr[ARM_BANK_IRQ], q1, 16);
        memcpy(CPU(dst)->banked_sp_lr[ARM_BANK_USR], q2, 16);
    }

    cursor = rd_ptr_u8(slot);
    wr_ptr(slot, cursor + 56);
    {
        uint8_t q0[16], q1[16];
        memcpy(q0, cursor + 56, 16);
        memcpy(q1, cursor + 72, 16);
        memcpy(CPU(dst)->fiq_r8_r14, q0, 16);
        memcpy(&CPU(dst)->fiq_r8_r14[4], q1, 16);
    }

    cursor = rd_ptr_u8(slot);
    wr_ptr(slot, cursor + 32);
    {
        uint8_t q0[16], q1[16];
        memcpy(q0, cursor + 44, 16);
        memcpy(q1, cursor + 32, 16);
        memcpy(&CPU(dst)->spsr[3], q0, 16);
        memcpy(CPU(dst)->spsr, q1, 16);
    }

    cursor = rd_ptr_u8(slot);
    wr_ptr(slot, cursor + 28);
    CPU(dst)->bank = rd32(cursor + 28);

    cursor = rd_ptr_u8(slot);
    wr_ptr(slot, cursor + 4);
    {
        uint8_t b = rd8(cursor + 4);
        wr_ptr(slot, cursor + 5);
        ((arm_t *)dst)->halt_flags = (b);
        CPU(dst)->pc = rd32(cursor + 5);
    }

    cursor = rd_ptr_u8(slot);
    wr_ptr(slot, cursor + 4);
    ((arm_t *)dst)->cpsr = (rd32(cursor + 4));

    cursor = rd_ptr_u8(slot);
    wr_ptr(slot, cursor + 4);
    {
        uint8_t q0[16], q1[16], q2[16], q3[16];
        memcpy(q0, cursor + 52, 16);
        memcpy(q1, cursor + 36, 16);
        memcpy(q2, cursor + 20, 16);
        memcpy(q3, cursor + 4, 16);
        memcpy(&CPU(dst)->r[12], q0, 16);
        memcpy(&CPU(dst)->r[8], q1, 16);
        memcpy(&CPU(dst)->r[4], q2, 16);
        memcpy(CPU(dst)->r, q3, 16);
    }

    cursor = rd_ptr_u8(slot);
    wr_ptr(slot, cursor + 64);
    ((arm_t *)dst)->cycle_mark = (rd32(cursor + 64));

    cursor = rd_ptr_u8(slot);
    wr_ptr(slot, cursor + 4);
}

static void write128(void *p, const uint8_t v[16])
{
    memcpy(p, v, 16);
}

void state_write_cpu_block(void *param_1, void *param_2)
{

    uint8_t *ctx = (uint8_t *)param_1;
    uint8_t *cursor = (uint8_t *)param_2 + 32;
    uint32_t saved = ((arm_t *)ctx)->halt_flags;
    uint8_t *dest = NULL;
    uint32_t offset;

    for (offset = 0; offset != 0x80; offset += 0x20) {
        const nds_timer_t *tm = &((arm_t *)ctx)->timers[offset / 0x20];
        dest = rd_ptr_u8(cursor);
        wr64(dest, tm->start_cycles);

        dest = rd_ptr_u8(cursor);
        wr_ptr(cursor, dest + 8);
        wr32(dest + 8,
              tm->period_cycles);

        dest = rd_ptr_u8(cursor);
        wr_ptr(cursor, dest + 4);
        wr16(dest + 4,
              tm->reload);

        dest = rd_ptr_u8(cursor);
        wr_ptr(cursor, dest + 2);
        wr16(dest + 2,
              tm->control);

        dest = rd_ptr_u8(cursor);
        wr_ptr(cursor, dest + 2);
        wr8(dest + 2,
             tm->prescaler_shift);

        dest = rd_ptr_u8(cursor);
        wr_ptr(cursor, dest + 1);
        wr8(dest + 1,
             tm->scheduled);

        dest = rd_ptr_u8(cursor);
        dest += 1;
        wr_ptr(cursor, dest);
    }

    if (((arm_t *)ctx)->machine->runtime.jit_enabled != 0) {
        jit_cache_refresh_block_info(ctx);
        dest = rd_ptr_u8(cursor);
    }

    {
        uint64_t value = rd64(CPU(ctx)->banked_sp_lr[ARM_BANK_SYS]);
        uint8_t q0[16];
        uint8_t q1[16];
        uint8_t q2[16];

        copy16(q0, CPU(ctx)->banked_sp_lr[ARM_BANK_ABT]);
        copy16(q1, CPU(ctx)->banked_sp_lr[ARM_BANK_USR]);
        copy16(q2, CPU(ctx)->banked_sp_lr[ARM_BANK_IRQ]);
        wr64(dest + 48, value);
        write128(dest, q1);
        write128(dest + 16, q2);
        write128(dest + 32, q0);
    }

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 0x38);
    {
        uint8_t q1[16];
        uint8_t q0[16];

        copy16(q1, CPU(ctx)->fiq_r8_r14);
        copy16(q0, &CPU(ctx)->fiq_r8_r14[4]);
        write128(dest + 72, q0);
        write128(dest + 56, q1);
    }

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 0x20);
    {
        uint8_t q0[16];
        uint8_t q1[16];

        copy16(q0, &CPU(ctx)->spsr[3]);
        copy16(q1, CPU(ctx)->spsr);
        write128(dest + 44, q0);
        write128(dest + 32, q1);
    }

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 0x1c);
    wr32(dest + 28, CPU(ctx)->bank);

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 4);
    wr8(dest + 4, (uint8_t)saved);

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 1);
    {
        uint32_t value = CPU(ctx)->pc;
        ((arm_t *)ctx)->halt_flags = (saved & 0xff);
        wr32(dest + 1, value);
    }

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 4);
    wr32(dest + 4, ((arm_t *)ctx)->cpsr);

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 4);
    {
        uint8_t q0[16];
        uint8_t q1[16];
        uint8_t q2[16];
        uint8_t q3[16];

        copy16(q0, &CPU(ctx)->r[12]);
        copy16(q1, &CPU(ctx)->r[8]);
        copy16(q2, &CPU(ctx)->r[4]);
        copy16(q3, CPU(ctx)->r);
        write128(dest + 52, q0);
        write128(dest + 36, q1);
        write128(dest + 20, q2);
        write128(dest + 4, q3);
    }

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 0x40);
    wr32(dest + 64, ((arm_t *)ctx)->cycle_mark);

    dest = rd_ptr_u8(cursor);
    wr_ptr(cursor, dest + 4);
}

void state_write_three_words(unsigned char *param_1, unsigned char *param_2) {

    uint32_t field;
    unsigned char *x8;
    unsigned char *x9;

    memcpy(&x8, param_2 + 0x20, 8);

    memcpy(&field, param_1 + 0x14, 4);

    memcpy(x8, &field, 4);

    memcpy(&x8, param_2 + 0x20, 8);

    x9 = x8 + 4;

    memcpy(param_2 + 0x20, &x9, 8);

    memcpy(&field, param_1 + 0x18, 4);

    memcpy(x8 + 4, &field, 4);

    memcpy(&x8, param_2 + 0x20, 8);

    x9 = x8 + 4;

    memcpy(param_2 + 0x20, &x9, 8);

    memcpy(&field, param_1 + 0x1c, 4);

    memcpy(x8 + 4, &field, 4);

    memcpy(&x8, param_2 + 0x20, 8);

    x8 = x8 + 4;

    memcpy(param_2 + 0x20, &x8, 8);
}

static uint8_t *advance_12(uint8_t *entry, uint8_t *p, uint32_t n)
{
    p += n;
    wr_ptr(entry + 0x20, p);
    return p;
}

static void copy(void *dst, const void *src, uint32_t n)
{
    memcpy(dst, src, n);
}

static void copy_from_core(void *dst, const void *src, uint32_t n)
{
    typedef void *(*fn_memcpy)(void *, const void *, unsigned long);
    ((fn_memcpy)sym_libc_memcpy)(dst, src, n);
}

static uint8_t *load_listing(uint8_t *entry, uint8_t *p, uint8_t *dst,
                               uint32_t count, uint32_t version)
{
    uint64_t i;

    for (i = 0; i < count; ++i, dst += 0x20) {
        if (version > 10) {
            wr32(dst + 4, rd32(p));
            p = advance_12(entry, p, 4);
            wr32(dst, rd32(p));
            p = advance_12(entry, p, 4);
            wr16(dst + 24, rd16(p));
            p = advance_12(entry, p, 2);
            wr16(dst + 26, rd16(p));
            p = advance_12(entry, p, 2);

            wr32(dst + 8, (uint32_t)*p);
            p = advance_12(entry, p, 2);
        } else {
            uint16_t class = rd16(p);
            uint32_t value;

            p = advance_12(entry, p, 2);
            value = rd32(p);
            wr32(dst, value);
            wr32(dst + 4, (uint32_t)class | 0x001f0000u);
            p = advance_12(entry, p, 4);
            wr16(dst + 24, rd16(p));
            p = advance_12(entry, p, 2);
            wr16(dst + 26, rd16(p));
            p = advance_12(entry, p, 2);
            value = rd32(p);

            wr32(dst + 8, value & 0xffu);
            p = advance_12(entry, p, 4);
        }
    }
    return p;
}

void state_read_gpu3d_block(void *param_1, void *param_2, uint32_t param_3)
{
    gpu3d_t *state = (gpu3d_t *)param_1;
    uint8_t *entry = (uint8_t *)param_2;
    uint8_t *p;
    uint32_t v;
    uint32_t count1;
    uint32_t count2;
    uint32_t count3;
    uint32_t count4;
    uint64_t i;

    if (param_3 == 1) {
        gpu3d_state_read_block(param_1, param_2);
        return;
    }

    p = rd_ptr_u8(entry + 0x20);

#define TAKE32(d) do { (d) = rd32(p); p = advance_12(entry, p, 4); } while (0)
#define TAKE16(d) do { (d) = rd16(p); p = advance_12(entry, p, 2); } while (0)
#define TAKE8(d)  do { (d) = *p; p = advance_12(entry, p, 1); } while (0)
    TAKE32(state->vertex[0].x);
    TAKE32(state->vertex[0].y);
    TAKE32(state->vertex[0].z);
    TAKE32(state->vertex[0].w);
    TAKE8(state->clip_code[0]);
    TAKE16(state->vertex_color[0]);
    TAKE32(state->vertex_texcoord[0]);

    TAKE32(state->vertex[1].x);
    TAKE32(state->vertex[1].y);
    TAKE32(state->vertex[1].z);
    TAKE32(state->vertex[1].w);
    TAKE8(state->clip_code[1]);
    TAKE16(state->vertex_color[1]);
    TAKE32(state->vertex_texcoord[1]);

    TAKE32(state->vertex[2].x);
    TAKE32(state->vertex[2].y);
    TAKE32(state->vertex[2].z);
    TAKE32(state->vertex[2].w);
    TAKE8(state->clip_code[2]);
    TAKE16(state->vertex_color[2]);
    TAKE32(state->vertex_texcoord[2]);

    TAKE32(state->vertex_count);
    TAKE32(state->last_color);
    TAKE32(state->primitive_run[0].polygon_attr);
    TAKE8(state->primitive_run[0].type);
    TAKE8(state->primitive_run[0].flipped);
#undef TAKE8
#undef TAKE16
#undef TAKE32

    copy_from_core((unsigned char *)state->matrix_stack, p, 0x1000);
    p = advance_12(entry, p, 0x1000);

    copy((unsigned char *)state->projection_stack, p, 0x40);
    p = advance_12(entry, p, 0x40);

    if (param_3 >= 3) {
        copy((unsigned char *)state->texture_stack, p, 0x40);
        p = advance_12(entry, p, 0x40);
    }
    copy((unsigned char *)state->light_vector_raw, p, 0x10);
    p = advance_12(entry, p, 0x10);
    copy((unsigned char *)state->light_color, p, 0x10);
    p = advance_12(entry, p, 0x10);
    copy((unsigned char *)state->light_direction[0], p, 0x30);
    p = advance_12(entry, p, 0x30);

    copy((unsigned char *)state->light_half_vector[0], p, 0x30);
    p = advance_12(entry, p, 0x30);
    copy((unsigned char *)state->position_matrix, p, 0x80);
    p = advance_12(entry, p, 0x80);
    copy((unsigned char *)state->projection_matrix, p, 0x40);
    p = advance_12(entry, p, 0x40);
    copy((unsigned char *)state->clip_matrix, p, 0x40);
    p = advance_12(entry, p, 0x40);
    copy((unsigned char *)state->texture_matrix, p, 0x40);
    p = advance_12(entry, p, 0x40);
    copy((unsigned char *)state->shininess_table, p, 0x80);
    p = advance_12(entry, p, 0x80);

    state->specular_emission_raw = rd32(p);
    p = advance_12(entry, p, 4);
    state->diffuse_ambient_raw = rd32(p);
    p = advance_12(entry, p, 4);
    copy((unsigned char *)state->edge_color, p, 0x10);
    p = advance_12(entry, p, 0x10);
    copy((unsigned char *)state->toon_table, p, 0x40);
    p = advance_12(entry, p, 0x40);
    copy((unsigned char *)state->fog_table, p, 0x20);
    p = advance_12(entry, p, 0x20);


    state->disp3dcnt = rd32(p);
    p = advance_12(entry, p, 4);
    state->clear_color = rd32(p);
    p = advance_12(entry, p, 4);
    state->polygon_attr = rd32(p);
    p = advance_12(entry, p, 4);
    state->texture_param = rd32(p);
    p = advance_12(entry, p, 4);
    state->texcoord[0] = rd16(p);
    p = advance_12(entry, p, 2);
    state->texcoord[1] = rd16(p);
    p = advance_12(entry, p, 2);
    state->fog_color = rd32(p);
    p = advance_12(entry, p, 4);
    state->texcoord_raw[0] = rd16(p);
    p = advance_12(entry, p, 2);
    state->texcoord_raw[1] = rd16(p);
    p = advance_12(entry, p, 2);
    state->clear_depth = rd16(p);
    p = advance_12(entry, p, 2);
    state->clear_image_offset = rd16(p);
    p = advance_12(entry, p, 2);
    state->fog_offset = rd16(p);
    p = advance_12(entry, p, 2);
    state->dot_depth = rd16(p);
    p = advance_12(entry, p, 2);
    state->texture_palette_base = rd16(p);
    p = advance_12(entry, p, 2);
    state->vertex_xyz[0] = rd16(p);
    p = advance_12(entry, p, 2);
    state->vertex_xyz[1] = rd16(p);
    p = advance_12(entry, p, 2);
    state->vertex_xyz[2] = rd16(p);
    p = advance_12(entry, p, 2);
    state->viewport_size[0] = rd16(p);
    p = advance_12(entry, p, 2);

    state->box_test_result = *p;
    p = advance_12(entry, p, 1);
    state->alpha_test_ref = *p;

    p = advance_12(entry, p, 1);

    if (param_3 >= 13) {
        state->viewport_size[1] = rd16(p);
        p = advance_12(entry, p, 2);
        state->viewport_origin[0] = rd16(p);
        p = advance_12(entry, p, 2);
        state->viewport_origin[1] = rd16(p);
        p = advance_12(entry, p, 2);
    } else {
        state->viewport_size[1] = *p;
        p = advance_12(entry, p, 1);
        state->viewport_origin[0] = *p;
        p = advance_12(entry, p, 1);
        state->viewport_origin[1] = *p;
        p = advance_12(entry, p, 1);
    }

    state->bank = *p;
    p = advance_12(entry, p, 1);
    state->params_remaining = *p;
    p = advance_12(entry, p, 1);
    state->matrix_mode = *p;
    p = advance_12(entry, p, 1);
    state->light_mask = *p;
    p = advance_12(entry, p, 1);
    state->texcoord_mode = *p;
    p = advance_12(entry, p, 1);
    state->position_stack_level = *p;

    p = advance_12(entry, p, 1);
    if (param_3 >= 3) {
        state->texture_stack_level = *p;
        p = advance_12(entry, p, 1);
    }
    state->swap_pending = *p;
    p = advance_12(entry, p, 1);
    state->swap_params_requested = *p;
    p = advance_12(entry, p, 1);
    state->swap_params_previous = *p;
    p = advance_12(entry, p, 1);
    state->swap_params = *p;

    p = advance_12(entry, p, 1);
    state->vertex_bank[0].count = rd32(p);
    p = advance_12(entry, p, 4);

    for (i = 0; i < 0x1800; ++i) {
        uint8_t *d = (uint8_t *)&state->vertex_bank[0].vertex[i];
        uint16_t h;

        wr32(d, rd32(p));
        p = advance_12(entry, p, 4);
        wr16(d + 4, rd16(p));
        p = advance_12(entry, p, 2);
        wr16(d + 6, rd16(p));
        p = advance_12(entry, p, 2);
        h = rd16(p);
        if (param_3 <= 10)
            h = (uint16_t)((((uint32_t)h << 15) - h) >> 16);
        wr16(d + 8, h);
        p = advance_12(entry, p, 2);
        wr16(d + 10, rd16(p));
        p = advance_12(entry, p, 2);
        wr16(d + 12, rd16(p));
        p = advance_12(entry, p, 2);
        wr16(d + 14, rd16(p));
        p = advance_12(entry, p, 2);
    }
    state->vertex_bank[1].count = rd32(p);
    p = advance_12(entry, p, 4);

    for (i = 0; i < 0x1800; ++i) {
        uint8_t *d = (uint8_t *)&state->vertex_bank[1].vertex[i];
        uint16_t h;

        wr32(d, rd32(p));
        p = advance_12(entry, p, 4);
        wr16(d + 4, rd16(p));
        p = advance_12(entry, p, 2);
        wr16(d + 6, rd16(p));
        p = advance_12(entry, p, 2);
        h = rd16(p);
        if (param_3 <= 10)
            h = (uint16_t)((((uint32_t)h << 15) - h) >> 16);
        wr16(d + 8, h);
        p = advance_12(entry, p, 2);
        wr16(d + 10, rd16(p));
        p = advance_12(entry, p, 2);
        wr16(d + 12, rd16(p));
        p = advance_12(entry, p, 2);
        wr16(d + 14, rd16(p));
        p = advance_12(entry, p, 2);
    }

    count1 = rd32(p);
    state->opaque[0].count = count1;
    p = advance_12(entry, p, 4);
    p = load_listing(entry, p, (unsigned char *)state->opaque[0].polygon, count1, param_3);

    count2 = rd32(p);
    state->opaque[1].count = count2;
    p = advance_12(entry, p, 4);
    p = load_listing(entry, p, (unsigned char *)state->opaque[1].polygon, count2, param_3);

    count3 = rd32(p);
    state->translucent[0].count = count3;
    p = advance_12(entry, p, 4);
    p = load_listing(entry, p, (unsigned char *)state->translucent[0].polygon, count3, param_3);

    count4 = rd32(p);
    state->translucent[1].count = count4;
    p = advance_12(entry, p, 4);
    p = load_listing(entry, p, (unsigned char *)state->translucent[1].polygon, count4, param_3);

    v = count4 ? 0x1000u - count4 : 0x1000u;
    v -= state->opaque[0].count;
    v -= state->opaque[1].count;
    v -= state->translucent[0].count;
    p = advance_12(entry, p, v * 14u);

    {
        uint8_t idx = *p;
        p = advance_12(entry, p, 1);
        wr32(state->command_ring, rd32(p));
        p = advance_12(entry, p, 4);
        copy_from_core((unsigned char *)state->param_ring, p, 0x200);
        p = advance_12(entry, p, 0x200);
        state->command_pending_cursor = state->command_ring + idx;
    }

    gpu3d_init_color_product_tables(param_1);
}

static uint8_t  r8 (const void *p) { return *(const uint8_t *)p; }

static void     w8 (void *p, uint8_t v) { *(uint8_t *)p = v; }

static void channel(unsigned char *p1, unsigned long countOff, unsigned long arrayOff,
                   uint32_t param_3, unsigned char **pcur)
{
    unsigned char *countAddr = p1 + countOff;
    unsigned char *cur = *pcur;

    wr32(cur, rd32(countAddr)); cur += 4;

    uint32_t count = rd32(countAddr);
    if (count != 0) {
        unsigned char *elem = p1 + arrayOff;
        for (uint32_t i = 0; i < count; i++, elem += 0x20) {
            if (param_3 > 0xa) {

                uint32_t v8 = rd32(elem + 0x8);
                wr32(cur, rd32(elem + 0x4)); cur += 4;
                wr32(cur, rd32(elem + 0x0)); cur += 4;
                wr16(cur, rd16(elem + 0x18)); cur += 2;
                wr16(cur, rd16(elem + 0x1a)); cur += 2;
                wr16(cur, (uint16_t)v8); cur += 2;

                wr32(elem + 0x8, (uint32_t)(uint8_t)v8);
            } else {

                uint16_t v4 = rd16(elem + 0x4);
                uint32_t v8full = rd32(elem + 0x8);
                wr16(cur, v4); cur += 2;
                wr32(cur, rd32(elem + 0x0)); cur += 4;
                wr16(cur, rd16(elem + 0x18)); cur += 2;
                wr16(cur, rd16(elem + 0x1a)); cur += 2;
                wr32(cur, v8full); cur += 4;

                wr32(elem + 0x4, 0x1f0000u | (uint32_t)v4);
                wr32(elem + 0x8, (uint32_t)(uint8_t)r8(elem + 0x8));
            }
        }
    }
    *pcur = cur;
}

void state_write_gpu3d_block(unsigned char *param_1, unsigned char *param_2, uint32_t param_3)
{

    static void *(*s_memcpy)(void*, const void*, size_t);
    if (!s_memcpy) s_memcpy = (void *(*)(void*, const void*, size_t))sym_libc_memcpy;

    gpu3d_t *p1 = (gpu3d_t *)param_1;

    uint8_t diffByte = (uint8_t)(p1->command_pending_cursor - p1->command_cursor);

    unsigned char *slot = param_2 + 0x20;
    unsigned char *cur;
    memcpy(&cur, slot, 8);

    wr32(cur, p1->vertex[0].x); cur += 4;
    wr32(cur, p1->vertex[0].y); cur += 4;
    wr32(cur, p1->vertex[0].z); cur += 4;
    wr32(cur, p1->vertex[0].w); cur += 4;
    w8 (cur, p1->clip_code[0]); cur += 1;
    wr16(cur, p1->vertex_color[0]); cur += 2;
    wr32(cur, p1->vertex_texcoord[0]); cur += 4;
    wr32(cur, p1->vertex[1].x); cur += 4;
    wr32(cur, p1->vertex[1].y); cur += 4;
    wr32(cur, p1->vertex[1].z); cur += 4;
    wr32(cur, p1->vertex[1].w); cur += 4;
    w8 (cur, p1->clip_code[1]); cur += 1;
    wr16(cur, p1->vertex_color[1]); cur += 2;
    wr32(cur, p1->vertex_texcoord[1]); cur += 4;
    wr32(cur, p1->vertex[2].x); cur += 4;
    wr32(cur, p1->vertex[2].y); cur += 4;
    wr32(cur, p1->vertex[2].z); cur += 4;
    wr32(cur, p1->vertex[2].w); cur += 4;
    w8 (cur, p1->clip_code[2]); cur += 1;
    wr16(cur, p1->vertex_color[2]); cur += 2;
    wr32(cur, p1->vertex_texcoord[2]); cur += 4;
    wr32(cur, p1->vertex_count); cur += 4;
    wr32(cur, p1->last_color); cur += 4;
    wr32(cur, p1->primitive_run[0].polygon_attr); cur += 4;
    w8 (cur, p1->primitive_run[0].type); cur += 1;
    w8 (cur, p1->primitive_run[0].flipped); cur += 1;

    s_memcpy(cur, (unsigned char *)p1->matrix_stack, 0x1000); cur += 0x1000;

    memcpy(cur, (unsigned char *)p1->projection_stack, 0x40); cur += 0x40;

    if (param_3 >= 3) {

        memcpy(cur, (unsigned char *)p1->texture_stack, 0x40); cur += 0x40;
    }

    memcpy(cur, (unsigned char *)p1->light_vector_raw, 0x80); cur += 0x80;
    memcpy(cur, (unsigned char *)p1->position_matrix, 0x80); cur += 0x80;

    memcpy(cur, (unsigned char *)p1->projection_matrix, 0x40); cur += 0x40;
    memcpy(cur, (unsigned char *)p1->clip_matrix, 0x40); cur += 0x40;
    memcpy(cur, (unsigned char *)p1->texture_matrix, 0xc0); cur += 0xc0;

    wr32(cur, p1->specular_emission_raw); cur += 4;
    wr32(cur, p1->diffuse_ambient_raw); cur += 4;

    memcpy(cur, (unsigned char *)p1->edge_color, 0x70); cur += 0x70;

    wr32(cur, p1->disp3dcnt); cur += 4;
    wr32(cur, p1->clear_color); cur += 4;
    wr32(cur, p1->polygon_attr); cur += 4;
    wr32(cur, p1->texture_param); cur += 4;
    wr16(cur, p1->texcoord[0]); cur += 2;
    wr16(cur, p1->texcoord[1]); cur += 2;
    wr32(cur, p1->fog_color); cur += 4;

    memcpy(cur, p1->texcoord_raw, 22); cur += 22;

    w8(cur, p1->box_test_result); cur += 1;
    w8(cur, p1->alpha_test_ref); cur += 1;

    if (param_3 >= 13) {

        wr16(cur, p1->viewport_size[1]); cur += 2;
        wr16(cur, p1->viewport_origin[0]); cur += 2;
        wr16(cur, p1->viewport_origin[1]); cur += 2;
    } else {

        w8(cur, 0); cur += 1;
        w8(cur, 0); cur += 1;
        w8(cur, 0); cur += 1;

        p1->viewport_size[1] = 0;
        p1->viewport_origin[0] = 0;
        p1->viewport_origin[1] = 0;
    }

    w8(cur, p1->bank); cur += 1;
    w8(cur, p1->params_remaining); cur += 1;
    w8(cur, p1->matrix_mode); cur += 1;
    w8(cur, p1->light_mask); cur += 1;
    w8(cur, p1->texcoord_mode); cur += 1;

    w8(cur, p1->position_stack_level); cur += 1;
    if (param_3 >= 3) {
        w8(cur, p1->texture_stack_level); cur += 1;
    }
    w8(cur, p1->swap_pending); cur += 1;
    w8(cur, p1->swap_params_requested); cur += 1;
    w8(cur, p1->swap_params_previous); cur += 1;
    w8(cur, p1->swap_params); cur += 1;

    wr32(cur, p1->vertex_bank[0].count); cur += 4;

    memcpy(cur, (unsigned char *)p1->vertex_bank[0].vertex, sizeof(p1->vertex_bank[0].vertex)); cur += sizeof(p1->vertex_bank[0].vertex);

    wr32(cur, p1->vertex_bank[1].count); cur += 4;

    memcpy(cur, (unsigned char *)p1->vertex_bank[1].vertex, sizeof(p1->vertex_bank[1].vertex)); cur += sizeof(p1->vertex_bank[1].vertex);

    channel((unsigned char *)p1, 0x49ae0, 0x39ae0, param_3, &cur);
    channel((unsigned char *)p1, 0x59ae8, 0x49ae8, param_3, &cur);
    channel((unsigned char *)p1, 0x69af0, 0x59af0, param_3, &cur);
    channel((unsigned char *)p1, 0x79af8, 0x69af8, param_3, &cur);

    uint32_t count1 = p1->opaque[0].count;
    uint32_t count2 = p1->opaque[1].count;
    uint32_t count3 = p1->translucent[0].count;
    uint32_t count4 = p1->translucent[1].count;
    uint32_t remaining = 0x1000u - count4 - count1 - count2 - count3;
    cur += remaining * 0xe;

    w8(cur, diffByte); cur += 1;
    wr32(cur, rd32(p1->command_ring)); cur += 4;

    s_memcpy(cur, (unsigned char *)p1->param_ring, 0x200); cur += 0x200;

    memcpy(slot, &cur, 8);
}

void *state_read_spu_block(spu_t *spu, uint8_t *entry, uint32_t mode) {
    uint8_t *p = state_stream_cursor(entry);
    for (uint64_t idx = 0; idx != 16; idx++) {
        spu_channel_t *channel = &spu->channels[idx];
        uint8_t type;
        memcpy(&channel->position, p, 8);
        memcpy(&channel->source_address, p + 12, 4);
        memcpy(&channel->length, p + 16, 4);
        memcpy(&channel->loop_length, p + 20, 4);
        memcpy(&channel->adpcm_loop_sample, p + 24, 2);
        memcpy(&channel->adpcm_sample, p + 26, 2);
        channel->adpcm_loop_index = p[28];
        channel->adpcm_index = p[29];
        channel->looped = p[30];
        channel->format = p[31];
        channel->active = p[32];
        p += 33;
        state_stream_seek(entry, p);
        type = channel->format;
        if (type < 3) {
            uint8_t *root = (uint8_t *)spu->bus;
            if (mode <= 6) {
                uint32_t address = channel->source_address;
                uint8_t *base = spu->bus->main_ram;
                channel->data = base + address;
                channel->source_address = address + NDS_MAIN_RAM_BASE;
            } else {
                uint32_t address = channel->source_address;
                uint64_t page = address >> 23;
                uint8_t *entry_page = root + page * 96;
                uint8_t state_page = ((bus_t *)entry_page)->region[BUS_ARM9_REGIONS].read_kind;
                if (state_page == 1) {
                    void *dest = ((bus_t *)entry_page)->region[BUS_ARM9_REGIONS].read_memory;
                    void *result =
                        ((void *(*)(void *, uint32_t))dest)(root, address);
                    channel->data = (const uint8_t *)result;
                    type = channel->format;
                } else if (state_page == 0) {
                    uint8_t *map = (uint8_t *)&((bus_t *)entry_page)->region[BUS_ARM9_REGIONS];
                    uint32_t mask = rd32(map);
                    uint8_t *base = rd_ptr(map + 8);
                    channel->data = base + (mask & address);
                } else {
                    channel->data = 0;
                    channel->active = 0;
                }
            }
            if (type == 2) {
                uint32_t limit = (uint32_t)(channel->position >> 32);
                int32_t difference = (int32_t)(limit - 0x40u);
                uint32_t position = difference < 0 ? 0 : ((uint32_t)difference & 0xfffffff8u);
                channel->adpcm_decoded = position;
                if (position <= limit) {
                    do {
                        spu_adpcm_decode_block(channel);
                    } while (channel->adpcm_decoded <= limit);
                }
            }
        } else if (type == 3) {
            channel->data = spu_psg_duty_table + (((channel->regs->cnt >> 24) & 7u) << 4);
        } else if (type == 4) {
            channel->data = spu_noise_table;
        }
        channel->recalc_flags = 3;
    }
    memcpy(&spu->mix_cycles, p, 8);
    p += 8;
    state_stream_seek(entry, p);
    {
        sound_regs_t *ctx = spu->regs;
        int8_t flag = (int8_t)ctx->sndcap0cnt;
        spu->capture[0].control = (uint8_t)flag;
        if (flag < 0) {
            uint8_t *base = spu->bus->main_ram;
            spu->capture[0].buffer = base + (ctx->sndcap0dad & 0x3fffffu);
            spu->capture[0].position = 0;
            spu->capture[0].length = (uint32_t)ctx->sndcap0len << 1;
            flag = (int8_t)ctx->sndcap1cnt;
            spu->capture[1].control = (uint8_t)flag;
            if (flag < 0) {
                base = spu->bus->main_ram;
                spu->capture[1].buffer = base + (ctx->sndcap1dad & 0x3fffffu);
                spu->capture[1].position = 0;
                spu->capture[1].length = (uint32_t)ctx->sndcap1len << 1;
            }
        } else {
            flag = (int8_t)ctx->sndcap1cnt;
            spu->capture[1].control = (uint8_t)flag;
        }
    }
    return video_out_clear_large_buffer((unsigned char *)spu);
}

void state_write_spu_block(const spu_t *spu, unsigned char *obj) {
    unsigned char *cur = state_stream_cursor(obj);
    for (uint64_t i = 0; i != 16; i++) {
        const spu_channel_t *channel = &spu->channels[i];
        uint32_t zero = 0;
        memcpy(cur, &channel->position, 8);
        memcpy(cur + 8, &zero, 4);
        memcpy(cur + 12, &channel->source_address, 4);
        memcpy(cur + 16, &channel->length, 4);
        memcpy(cur + 20, &channel->loop_length, 4);
        memcpy(cur + 24, &channel->adpcm_loop_sample, 2);
        memcpy(cur + 26, &channel->adpcm_sample, 2);
        cur[28] = channel->adpcm_loop_index;
        cur[29] = channel->adpcm_index;
        cur[30] = channel->looped;
        cur[31] = channel->format;
        cur[32] = channel->active;
        cur += 33;
        state_stream_seek(obj, cur);
    }
    memcpy(cur, &spu->mix_cycles, 8);
    cur += 8;
    state_stream_seek(obj, cur);
}

unsigned char *state_read_cart_block(cart_t *cart, unsigned char *reader, uint32_t mode)
{
    unsigned char *cur = state_stream_cursor(reader);
    unsigned char *result;

    memcpy(&cart->read_address, cur, 4);
    memcpy(&cart->words_remaining, cur + 4, 4);
    memcpy(&cart->data_word, cur + 8, 4);
    cur += 12;
    state_stream_seek(reader, cur);
    if (mode < 4) {
        cart->transfer_deadline_cycles = 0;
        cart->irq_pending = 0;
        result = state_read_spi_memory_status_result(&cart->backup, reader);
        cart->chip_id = 0;
        return result;
    }
    memcpy(&cart->transfer_deadline_cycles, cur, 8);
    cur += 8;
    state_stream_seek(reader, cur);
    if (mode <= 4) {
        cart->irq_pending = 0;
        result = state_read_spi_memory_status_result(&cart->backup, reader);
        cart->chip_id = 0;
        return result;
    }
    cart->irq_pending = cur[0];
    cur += 1;
    state_stream_seek(reader, cur);
    if (mode <= 7) {
        result = state_read_spi_memory_status_result(&cart->backup, reader);
        cart->chip_id = 0;
        return result;
    }
    memcpy(&cart->backup_base, cur, 4);
    memcpy(&cart->backup_address, cur + 4, 4);
    cart->backup_write_mode = cur[8];
    cart->backup_write_enabled = cur[9];
    cur += 10;
    state_stream_seek(reader, cur);
    if (mode < 10) {
        result = state_read_spi_memory_status_result(&cart->backup, reader);
        cart->chip_id = 0;
        return result;
    }
    memcpy(&cart->chip_id, cur, 4);
    cur += 4;
    state_stream_seek(reader, cur);
    if (mode < 12)
        return state_read_spi_memory_status_result(&cart->backup, reader);
    memcpy(&cart->slot2.flash_bank, cur, 4);
    cart->slot2.backup_command = cur[4];
    cart->slot2.backup_phase = cur[5];
    cur += 6;
    state_stream_seek(reader, cur);
    if (mode < 14)
        return state_read_spi_memory_status_result(&cart->backup, reader);
    memcpy(&cart->slot2.sensor.index, cur, 4);
    cart->slot2.sensor.phase = cur[4];
    memcpy(&cart->slot2.sensor.accel16_x, cur + 5, 8);
    memcpy(&cart->slot2.sensor.ident, cur + 13, 2);
    cart->slot2.sensor.index16 = cur[15];
    cart->slot2.sensor.phase16 = cur[16];
    cart->slot2.gpio.bit1 = cur[17];
    cart->slot2.gpio.countdown = cur[18];
    cur += 19;
    state_stream_seek(reader, cur);
    return state_read_spi_memory_status_result(&cart->backup, reader);
}

void state_write_spi_memory_status_17(const unsigned char *ctx, unsigned char *desc) __asm__("state_write_spi_memory_status");

void state_write_cart_block(cart_t *cart, unsigned char *desc, uint32_t mode)
{
    unsigned char *cur = state_stream_cursor(desc);

    memcpy(cur, &cart->read_address, 4);
    memcpy(cur + 4, &cart->words_remaining, 4);
    memcpy(cur + 8, &cart->data_word, 4);
    cur += 12;
    state_stream_seek(desc, cur);
    if (mode < 4) {
        cart->transfer_deadline_cycles = 0;
        cart->irq_pending = 0;
        goto tail;
    }
    memcpy(cur, &cart->transfer_deadline_cycles, 8);
    cur += 8;
    state_stream_seek(desc, cur);
    if (mode <= 4) {
        cart->irq_pending = 0;
        goto tail;
    }
    cur[0] = cart->irq_pending;
    cur += 1;
    state_stream_seek(desc, cur);
    if (mode <= 7)
        goto tail;
    memcpy(cur, &cart->backup_base, 4);
    memcpy(cur + 4, &cart->backup_address, 4);
    cur[8] = cart->backup_write_mode;
    cur[9] = cart->backup_write_enabled;
    cur += 10;
    state_stream_seek(desc, cur);
    if (mode < 10)
        goto tail;
    memcpy(cur, &cart->chip_id, 4);
    cur += 4;
    state_stream_seek(desc, cur);
    if (mode < 12)
        goto tail;
    memcpy(cur, &cart->slot2.flash_bank, 4);
    cur[4] = cart->slot2.backup_command;
    cur[5] = cart->slot2.backup_phase;
    cur += 6;
    state_stream_seek(desc, cur);
    if (mode < 14)
        goto tail;
    memcpy(cur, &cart->slot2.sensor.index, 4);
    cur[4] = cart->slot2.sensor.phase;
    memcpy(cur + 5, &cart->slot2.sensor.accel16_x, 8);
    memcpy(cur + 13, &cart->slot2.sensor.ident, 2);
    cur[15] = cart->slot2.sensor.index16;
    cur[16] = cart->slot2.sensor.phase16;
    cur[17] = cart->slot2.gpio.bit1;
    cur[18] = cart->slot2.gpio.countdown;
    cur += 19;
    state_stream_seek(desc, cur);
tail:
    state_write_spi_memory_status(&cart->backup, desc);
}

void state_read_spi_block(spi_t *spi, uint8_t *reader, uint32_t size)
{
    uint8_t *cur = state_stream_cursor(reader);

    memcpy(&spi->control, cur, 2);
    cur += 2;
    state_stream_seek(reader, cur);

    state_read_spi_memory_status(&spi->firmware, reader);

    cur = state_stream_cursor(reader);
    memcpy(spi->tsc.channels, cur, sizeof spi->tsc.channels);
    cur += sizeof spi->tsc.channels;
    spi->tsc.command = cur[0];
    spi->tsc.state = cur[1];
    cur += 2;
    state_stream_seek(reader, cur);
    if (size < 6)
        return;

    spi->powerman.active = cur[0];
    spi->powerman.index = cur[1];
    cur += 2;
    memcpy(spi->powerman.regs, cur, sizeof spi->powerman.regs);
    memcpy(spi->reserved_0, cur + sizeof spi->powerman.regs, 8 - sizeof spi->powerman.regs);
    cur += 8;
    state_stream_seek(reader, cur);
}

void state_write_spi_block(spi_t *spi, uint8_t *desc, uint32_t mode)
{
    uint8_t *cursor = state_stream_cursor(desc);

    memcpy(cursor, &spi->control, 2);
    cursor += 2;
    state_stream_seek(desc, cursor);

    state_write_spi_memory_status(&spi->firmware, desc);

    cursor = state_stream_cursor(desc);
    memcpy(cursor, spi->tsc.channels, sizeof spi->tsc.channels);
    cursor += sizeof spi->tsc.channels;
    cursor[0] = spi->tsc.command;
    cursor[1] = spi->tsc.state;
    cursor += 2;
    state_stream_seek(desc, cursor);

    if (mode < 6u)
        return;

    cursor[0] = spi->powerman.active;
    cursor[1] = spi->powerman.index;
    cursor += 2;
    memcpy(cursor, spi->powerman.regs, sizeof spi->powerman.regs);
    memcpy(cursor + sizeof spi->powerman.regs, spi->reserved_0, 8 - sizeof spi->powerman.regs);
    cursor += 8;
    state_stream_seek(desc, cursor);
}


static uint64_t rd_rtc_datetime(const rtc_t *rtc) { uint64_t v; memcpy(&v, rtc->datetime, 8); return v; }
static void wr_rtc_datetime(rtc_t *rtc, uint64_t v) { memcpy(rtc->datetime, &v, 8); }

void state_read_rtc_block(rtc_t *rtc, unsigned char *entry,
                            uint32_t mode)
{

    const uint8_t *indirect = rd_ptr_u8(entry + 32);
    wr_rtc_datetime(rtc, rd64(indirect));

    uint8_t *cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 8);
    rtc->state = (uint8_t)(rd8(cursor + 8));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);
    rtc->command = (uint8_t)(rd8(cursor + 1));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);
    rtc->status1 = (uint8_t)(rd8(cursor + 1));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);
    rtc->status2 = (uint8_t)(rd8(cursor + 1));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);
    rtc->data_out = (uint8_t)(rd8(cursor + 1));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);
    rtc->clock = (uint8_t)(rd8(cursor + 1));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);
    rtc->shift = (uint8_t)(rd8(cursor + 1));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);
    rtc->bit_count = (uint8_t)(rd8(cursor + 1));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);
    rtc->byte_count = (uint8_t)(rd8(cursor + 1));

    cursor = rd_ptr_u8(entry + 32);
    wr_ptr(entry + 32, cursor + 1);

    if (mode > 8) {
        uint32_t value = rd32(cursor + 1);
        wr_ptr(entry + 32, cursor + 5);
        rtc->offset_seconds = (int64_t)(value);
        return;
    }

    uint64_t now = (uint64_t)nds_platform_default()->time.now_seconds(nds_platform_default()->user);
    uint64_t ticks = rd64(rtc->machine);
    rtc->offset_seconds = (int64_t)(now - ticks / 60u);
}





void state_write_rtc_block(rtc_t *rtc, void *param_2, uint32_t param_3)
{

    uint8_t *state = (uint8_t *)param_2;
    uint64_t x8 = (uint64_t)rtc->offset_seconds;
    uint64_t x9 = rd_rtc_datetime(rtc);
    uint8_t *cursor;

    cursor = rd_ptr_u8(state + 0x20);
    wr64(cursor, x9);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 8);
    wr8(cursor + 8, rtc->state);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 1);
    wr8(cursor + 1, rtc->command);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 1);
    wr8(cursor + 1, rtc->status1);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 1);
    wr8(cursor + 1, rtc->status2);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 1);
    wr8(cursor + 1, rtc->data_out);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 1);
    wr8(cursor + 1, rtc->clock);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 1);
    wr8(cursor + 1, rtc->shift);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 1);
    wr8(cursor + 1, rtc->bit_count);

    cursor = rd_ptr_u8(state + 0x20);
    wr_ptr(state + 0x20, cursor + 1);
    wr8(cursor + 1, rtc->byte_count);

    cursor = rd_ptr_u8(state + 0x20);
    cursor += 1;
    wr_ptr(state + 0x20, cursor);

    if (param_3 >= 9) {
        uint32_t part_low = (uint32_t)x8;

        wr32(cursor, part_low);
        cursor = rd_ptr_u8(state + 0x20);
        wr_ptr(state + 0x20, cursor + 4);
        rtc->offset_seconds = (int64_t)((uint64_t)part_low);
        return;
    }

    {
        uint64_t now = (uint64_t)nds_platform_default()->time.now_seconds(nds_platform_default()->user);
        uint64_t dividend = rd64(rtc->machine);
        uint64_t high_mul = (uint64_t)(((__uint128_t)dividend *
            UINT64_C(0x8888888888898888)) >> 64);

        rtc->offset_seconds = (int64_t)(now - (high_mul >> 5));
    }
}

typedef void *(*fn_realloc)(void *, uint64_t);
typedef void *(*fn_memcpy_22)(void *, const void *, uint64_t);


void state_read_spi_memory_data(spi_memory_t *m, uint8_t *reader) {

    uint8_t *cur = state_stream_cursor(reader);
    uint32_t old_count = m->mask;
    memcpy(&m->type, cur, 4);
    memcpy(&m->mask, cur + 4, 4);
    memcpy(m->jedec_id, cur + 8, 4);
    m->address_bytes = cur[12];
    uint8_t *src = cur + 13;
    state_stream_seek(reader, src);

    uint8_t *ptr = m->data;
    uint32_t count = m->mask;
    if (old_count != count) {
        ptr = (uint8_t *)((fn_realloc)sym_libc_realloc)(
                    ptr, (uint64_t)(count + 1u));
        m->data = ptr;
    }
    ((fn_memcpy_22)sym_libc_memcpy)(ptr, src, (uint64_t)(count + 1u));
    state_stream_seek(reader, src + (count + 1u));
}

typedef void *(*fn_memcpy_23)(void *, const void *, uint64_t);


void state_write_spi_memory_data(const spi_memory_t *m, uint8_t *writer)
{

    uint8_t *cur = state_stream_cursor(writer);
    memcpy(cur, &m->type, 4);
    memcpy(cur + 4, &m->mask, 4);
    memcpy(cur + 8, m->jedec_id, 4);
    cur[12] = m->address_bytes;
    uint8_t *dest = cur + 13;
    ((fn_memcpy_23)sym_libc_memcpy)(dest, m->data, (uint64_t)(m->mask + 1u));
    state_stream_seek(writer, dest + (m->mask + 1u));
}

static unsigned char *state_read_spi_memory_status_result(spi_memory_t *m, unsigned char *reader)
{
    state_read_spi_memory_status(m, reader);
    return state_stream_cursor(reader);
}

void state_read_spi_memory_status(spi_memory_t *m, unsigned char *reader)
{
    unsigned char *cur = state_stream_cursor(reader);

    memcpy(&m->address, cur, 4);
    memcpy(&m->command, cur + 4, 2);
    m->state = cur[6];
    m->status = cur[7];
    m->byte_count = cur[8];
    state_stream_seek(reader, cur + 9);
}

void state_write_spi_memory_status(const spi_memory_t *m, unsigned char *desc)
{
    unsigned char *p = state_stream_cursor(desc);

    memcpy(p, &m->address, 4);
    memcpy(p + 4, &m->command, 2);
    p[6] = m->state;
    p[7] = m->status;
    p[8] = m->byte_count;
    state_stream_seek(desc, p + 9);
}

typedef size_t (*fn_fwrite)(const void *, size_t, size_t, void *);
typedef unsigned long (*fn_compress_bound)(unsigned long);
typedef int (*fn_compress)(void *, unsigned long *, const void *, unsigned long);
typedef void *(*fn_malloc_26)(size_t);
typedef void (*fn_free_26)(void *);
typedef int (*fn_fclose)(void *);
extern int str_vsprintf_caller_limit_4(char *dest, size_t size,
                              const char *format, ...);

void *state_write_finish(void *arg) {
    uint8_t *state = arg;
    fn_fwrite write_fn = (fn_fwrite)sym_libc_fwrite;
    fn_free_26 release = (fn_free_26)sym_libc_free;
    fn_fclose close_fn = (fn_fclose)sym_libc_fclose;
    void *data = rd_ptr(state + 0x830);

    if ((rd8(state + 0x838) & 1u) == 0) {
        const uint8_t *start = rd_ptr_u8(state + 0x820);
        const uint8_t *end = rd_ptr_u8(state + 0x828);
        void *file = rd_ptr(state + 0x808);
        write_fn(start, (size_t)(end - start), 1,
                file);
    } else {
        fn_compress_bound bound =
            (fn_compress_bound)sym_libc_compressBound;
        fn_malloc_26 reserve = (fn_malloc_26)sym_libc_malloc;
        fn_compress compress = (fn_compress)sym_libc_compress;

        unsigned long compressed_size = bound(0x680000ul);
        const uint8_t *end = rd_ptr_u8(state + 0x828);
        const uint8_t *start = rd_ptr_u8(state + 0x820);
        uint64_t origin_size = (end - start) - 0x40u;
        void *output = reserve((size_t)compressed_size);

        if (compress(output, &compressed_size, (uint8_t *)data + 0x40,
                     (unsigned long)origin_size) != 0) {
            release(output);
            close_fn(rd_ptr(state + 0x808));
            release(data);
            wr32(state + 0x83c, 0);
            return NULL;
        }

        write_fn(data, 0x40, 1,
                rd_ptr(state + 0x808));
        volatile uint32_t in_file_size = (uint32_t)compressed_size;
        write_fn((const void *)&in_file_size, 4, 1,
                rd_ptr(state + 0x808));
        uint32_t for_data_size = in_file_size;
        write_fn(output, (size_t)for_data_size, 1,
                rd_ptr(state + 0x808));
        release(output);
    }

    close_fn(rd_ptr(state + 0x808));
    release(data);

    char tmp[0x420];
    char dest[0x800];
    const char *format = "%s%c%s";
    str_vsprintf_caller_limit_4(tmp, sizeof tmp, format, state, 0x2f,
                        "_savestate_temp.dss");
    str_vsprintf_caller_limit_4(dest, sizeof dest, format, state, 0x2f,
                        state + 0x400);

    if (files_jni_env_invoke_and_release(tmp, dest) != 0) {
        files_jni_env_invoke_alternate(dest);
        files_jni_env_invoke_and_release(tmp, dest);
    }

    wr32(state + 0x83c, 0);
    return NULL;
}

uint32_t state_write_pending(void)
{

    return state_write_work.pending;
}

int state_load_file(unsigned char *machine, const char *path,
                       unsigned char *top, unsigned char *bottom,
                       uint32_t only_look) {

    void *(*c_malloc)(unsigned long) = (void *(*)(unsigned long))sym_libc_malloc;
    void  (*c_free)(void *)          = (void (*)(void *))sym_libc_free;
    unsigned long (*c_fread)(void *, unsigned long, unsigned long, void *) =
        (unsigned long (*)(void *, unsigned long, unsigned long, void *))sym_libc_fread;
    long  (*c_ftell)(void *)         = (long (*)(void *))sym_libc_ftell;
    int   (*c_fseek)(void *, long, int) = (int (*)(void *, long, int))sym_libc_fseek;
    int   (*c_fclose)(void *)        = (int (*)(void *))sym_libc_fclose;
    int   (*c_memcmp)(const void *, const void *, unsigned long) =
        (int (*)(const void *, const void *, unsigned long))sym_libc_memcmp;
    void *(*c_memcpy)(void *, const void *, unsigned long) =
        (void *(*)(void *, const void *, unsigned long))sym_libc_memcpy;

    unsigned char *buffer = (unsigned char *)c_malloc(0x680000);
    spu_t *spu = &((nds_t *)machine)->spu;
    while (state_write_work.pending != 0)
        nds_platform_default()->threads.sleep_us(1);

    uint32_t token = 0;
    if (only_look == 0)
        token = audio_out_stop_and_mute(spu);

    state_stream_t reader_obj;
    unsigned char *reader = (unsigned char *)&reader_obj;
    reader_obj.framed = 0;
    void *fp = nds_platform_default()->files.open(nds_platform_default()->user, 
                   path, "rb");
    SS(reader)->file  = fp;
    SS(reader)->buffer = buffer;
    SS(reader)->cursor = buffer;

    unsigned char *mark = &((nds_t *)machine)->runtime.state_marker;
    int32_t ret;

    if (fp == 0) {
        if (only_look == 0)
            audio_out_resume_after_stop(spu, token);
        c_free(buffer);
        return -1;
    }

    state_file_header_t hdr;
    int good = 0;
    unsigned char *cursor = buffer + sizeof hdr;
    size_t len_load = 0;

    if (c_fread(buffer, sizeof hdr, 1, fp) != 0) {
        c_memcpy(&hdr, buffer, sizeof hdr);
        good = c_memcmp(hdr.magic, STATE_MAGIC, 4) == 0 && hdr.format_version == STATE_FORMAT_VERSION;
    }

    if (!good) {
    fail:
        ret = -1;
    finish_error:

        ((int (*)(void *))sym_libc_fflush)(stdout);
        c_fclose(SS(reader)->file);
        c_free(buffer);
        if (only_look == 0) {
            audio_out_resume_after_stop(spu, token);
            *(uint8_t *)mark = 1;
        }
        return ret;
    }

    uint32_t flag = hdr.flags;
    uint32_t stamp   = hdr.stream_version;
    SS(reader)->framed = 1;

    if (only_look == 1 && !(flag & STATE_FLAG_SCREENS)) {
        c_fclose(fp);
        c_free(buffer);
        return 0;
    }

    if (!(flag & STATE_FLAG_COMPRESSED)) {

        long pos = c_ftell(fp);
        c_fseek(fp, 0, 2);
        long end = c_ftell(fp);
        uint32_t len = (uint32_t)((uint32_t)end - (uint32_t)pos);
        c_fseek(fp, (long)((uint64_t)pos & 0xffffffffull), 0);
        if (c_fread(cursor, len, 1, fp) == 0) goto fail;
        len_load = len;
    } else {

        uint64_t dest = (only_look == 0) ? 0x680000 : 2 * NDS_SCREEN_BYTES + sizeof(state_section_header_t);
        uint32_t compr;
        if (c_fread(&compr, 4, 1, fp) == 0) goto fail;
        unsigned char *tmp = (unsigned char *)c_malloc(compr);
        if (c_fread(tmp, compr, 1, SS(reader)->file) == 0) {
            c_free(tmp);
            goto fail;
        }
        int32_t z = ((int32_t (*)(void *, uint64_t *, void *, uint64_t))sym_libc_uncompress)(
                        buffer, &dest, tmp, compr);
        c_free(tmp);
        if (z != 0) {

            if (only_look != 1 || z != -5) goto fail;
        }
        SS(reader)->buffer = buffer;
        SS(reader)->cursor = buffer;
        cursor = buffer;
        len_load = (size_t)dest;
    }

    if (only_look != 0) {
        state_section_header_t first_row;
        if (len_load < sizeof first_row) goto fail;
        c_memcpy(&first_row, cursor, sizeof first_row);
        if (first_row.id != STATE_SECTION_SCREENS || first_row.size != 2 * NDS_SCREEN_BYTES ||
            len_load < sizeof first_row + 2 * NDS_SCREEN_BYTES) goto fail;
        cursor += sizeof first_row;
        if (top != 0 && bottom != 0) {
            c_memcpy(top, cursor, NDS_SCREEN_BYTES);
            c_memcpy(bottom, cursor + NDS_SCREEN_BYTES, NDS_SCREEN_BYTES);
        }
        c_fclose(SS(reader)->file);
        c_free(buffer);
        return 0;
    }

    if (!state_sections_valid(cursor, len_load, flag)) goto fail;
    SS(reader)->cursor = cursor;
    if (flag & STATE_FLAG_SCREENS) {
        uint8_t *section = state_section_enter(reader);
        if (top != 0 && bottom != 0) {
            c_memcpy(top, cursor + sizeof(state_section_header_t), NDS_SCREEN_BYTES);
            c_memcpy(bottom, cursor + sizeof(state_section_header_t) + NDS_SCREEN_BYTES, NDS_SCREEN_BYTES);
        }
        state_section_leave(reader, section);
    }

    if (only_look != 0) {
        c_fclose(SS(reader)->file);
        c_free(buffer);
        return 0;
    }

    unsigned char *which = &((nds_t *)machine)->bus.bios_flags;
    uint32_t has = *(uint8_t *)which;
    uint32_t wants = (flag >> 2) & 3;
    uint32_t differs = wants ^ has;

    if (differs != 0) {
        int (*reload)(unsigned char *, const char *, void *, uint32_t) = files_read_exact_size;

        if (differs & 2) {
            void *d = ((nds_t *)machine)->bus.bios9;
            int r;
            if (flag & 8) {
                r = reload(machine, DATA_BIOS9_REPLACEMENT, d, 0x1000);
                has |= 2;
            } else {
                r = reload(machine, DATA_BIOS9_REAL, d, 0x1000);
                has &= ~2u;
            }
            if (r < 0) { ret = -2; goto finish_error; }
        }
        if (differs & 1) {
            void *d = ((nds_t *)machine)->bus.bios7;
            int r;
            if (flag & 4) {
                r = reload(machine, DATA_BIOS7_REPLACEMENT, d, 0x4000);
                has |= 1;
            } else {
                r = reload(machine, DATA_BIOS7_REAL, d, 0x4000);
                has &= ~1u;
            }
            if (r < 0) { ret = -2; goto finish_error; }
        }
        *(uint8_t *)which = (uint8_t)has;
    }

    if (flag & STATE_FLAG_SPI_MEMORY) {
        uint8_t *section = state_section_enter(reader);
        (void)stamp;
        state_read_spi_memory_data(&((nds_t *)machine)->cart.backup, reader + 0);
        state_section_leave(reader, section);
    }

    state_read_machine_blocks(machine, reader + 0, stamp);

    c_fclose(SS(reader)->file);
    c_free(buffer);
    audio_out_resume_after_stop(spu, token);
    *(uint8_t *)mark = 1;
    return 0;
}

#define WORK 0x3f1d860UL
typedef void *(*fn_malloc_29)(size_t);
typedef void (*fn_free_29)(void *);
typedef void *(*fn_memcpy_29)(void *, const void *, size_t);
typedef int (*fn_usleep)(unsigned int);
typedef char *(*fn_strncpy)(char *, const char *, size_t);
typedef void *(*fn_start_thread)(void *);
typedef int (*fn_pthread_create)(void *, const void *, fn_start_thread, void *);


static void copy_bytes(void *dest, const void *origin, size_t size) {
    memcpy(dest, origin, size);
}

int32_t state_save_file(uint8_t *machine, const char *path,
                            const char *name_final, const void *copy_a,
                            const void *copy_b) {

    static fn_malloc_29 reserve;
    static fn_free_29 release;
    static fn_memcpy_29 copy_core;
    static fn_usleep sleep;
    static fn_strncpy copy_name;
    static fn_pthread_create create_thread;

    if (!reserve) {
        reserve = (fn_malloc_29)sym_libc_malloc;
        release = (fn_free_29)sym_libc_free;
        copy_core = (fn_memcpy_29)sym_libc_memcpy;
        sleep = (fn_usleep)sym_libc_usleep;
        copy_name = (fn_strncpy)sym_libc_strncpy;
        create_thread = (fn_pthread_create)platform_thread_create;
    }

    uint8_t *block = (uint8_t *)reserve(0x680000);
    char tmp[0x480];
    str_vsprintf_caller_limit_4(tmp, sizeof tmp,
                               "%s%c%s", path, 0x2f,
                               "_savestate_temp.dss");
    void *file = nds_platform_default()->files.open(nds_platform_default()->user, tmp, "wb");

    if (file == NULL) {
        release(block);
        return -1;
    }

    uint32_t active = ((nds_t *)machine)->config.savestates_enabled != 0;
    uint32_t accepts_copies = ((nds_t *)machine)->config.savestate_copies;
    uint32_t has_extra = ((nds_t *)machine)->config.savestate_extra;
    uint32_t flags = active;

    if (accepts_copies != 0 && copy_a != NULL && copy_b != NULL)
        flags |= 2u;
    if (has_extra != 0 && ((nds_t *)machine)->cart.backup.mask < 0x80000u)
        flags |= 0x10u;

    uint32_t flags_with_machine =
        flags | ((uint32_t)((const nds_t *)machine)->bus.bios_flags << 2);
    uint32_t flags_final = flags_with_machine | STATE_FLAG_CURRENT;
    uint64_t mark = UINT64_C(0x02060004);
    uint64_t block_of_time = rd64(machine) / 60u;
    uint32_t field_machine = ((nds_t *)machine)->cart.game_code;

    state_file_header_t header;
    memset(&header, 0, sizeof header);
    memcpy(header.magic, STATE_MAGIC, 4);
    header.format_version = STATE_FORMAT_VERSION;
    header.flags = flags_final;
    {
        uint32_t ids[STATE_SECTION_COUNT_MAX];
        header.section_count = state_expected_sections(flags_final, ids);
    }
    header.stream_version = STATE_STREAM_VERSION;
    header.machine_field = field_machine;
    header.time_block = block_of_time;
    header.core_version = mark;
    copy_bytes(block, &header, sizeof header);

    uint8_t stream[40];
    wr_ptr(stream, file);
    wr64(stream + 8, 1);
    SS(stream)->unmapped_0 = 0;
    SS(stream)->buffer = block;
    SS(stream)->cursor = block + sizeof header;

    if ((flags & STATE_FLAG_SCREENS) != 0) {
        uint8_t *section = state_section_open(stream, STATE_SECTION_SCREENS);
        uint8_t *dest = SS(stream)->cursor;
        copy_core(dest, copy_a, NDS_SCREEN_BYTES);
        dest += NDS_SCREEN_BYTES;
        SS(stream)->cursor = dest;
        copy_core(dest, copy_b, NDS_SCREEN_BYTES);
        SS(stream)->cursor = dest + NDS_SCREEN_BYTES;
        state_section_close(stream, section);
    }

    if ((flags_with_machine & STATE_FLAG_SPI_MEMORY) != 0) {
        uint8_t *section = state_section_open(stream, STATE_SECTION_SPI_MEMORY);
        state_write_spi_memory_data(&((nds_t *)machine)->cart.backup, stream);
        state_section_close(stream, section);
    }
    state_write_machine_blocks(machine, stream, STATE_STREAM_VERSION);

    uint8_t *work = (uint8_t *)&state_write_work;
    while (rd32(work + 0x83c) != 0)
        sleep(1);

    copy_bytes(work + 0x808, stream, 32);
    wr_ptr(work + 0x828, SS(stream)->cursor);
    wr_ptr(work + 0x830, block);
    wr32(work + 0x838, flags_final);
    wr32(work + 0x83c, 1);
    copy_name((char *)work, path, 0x3ff);
    copy_name((char *)work + 0x400, name_final, 0x3ff);
    create_thread(work + 0x800, NULL, state_write_finish, work);
    return 0;
}
#undef WORK

int state_build_dir_path(unsigned char *obj, char *dest) {
    int (*format)(char *, size_t, const char *, ...) = str_vsprintf_caller_limit_4;

    return format(dest, (unsigned long)-1, "%s%csavestates",
                     ((nds_t *)obj)->save_dir, '/');
}

extern int str_vsprintf_caller_limit_4_31(char *dest, unsigned long size,
                              const char *format, ...) __asm__("str_vsprintf_caller_limit_4");

int state_build_file_name(unsigned char *obj, char *dest, uint32_t slot) {

    return str_vsprintf_caller_limit_4(dest, (unsigned long)-1,
                              "%s_%d.dss",
                              ((nds_t *)obj)->rom_name, slot);
}


int state_build_slot_path(unsigned char *obj, char *dest, uint32_t slot) {

    return str_vsprintf_caller_limit_4(dest, (size_t)-1, "%s%csavestates%c%s_%d.dss",
                     ((nds_t *)obj)->save_dir, '/', '/',
                     ((nds_t *)obj)->rom_name, slot);
}

int32_t state_save_slot(unsigned char *obj, uint32_t slot, void *a3, void *a4) {

    int (*format_s_c)(char *, size_t, const char *, ...) = str_vsprintf_caller_limit_4;
    int (*format_s_d)(char *, size_t, const char *, ...) = str_vsprintf_caller_limit_4;
    int32_t (*work)(uint8_t *, const char *, const char *, const void *, const void *) = state_save_file;

    char dir[1056];
    char name[1056];

    format_s_c(dir, 0x420, "%s%csavestates",
                 ((nds_t *)obj)->save_dir, '/');
    format_s_d(name, 0x420, "%s_%d.dss",
                 ((nds_t *)obj)->rom_name, slot);

    return work(obj, dir, name, a3, a4);
}

int state_load_slot(unsigned char *obj, uint32_t slot, void *a2, void *a3,
                       uint32_t a4) {
    int (*format)(char *, size_t, const char *, ...) = str_vsprintf_caller_limit_4;
    int (*work)(unsigned char *, const char *, unsigned char *, unsigned char *, uint32_t) = state_load_file;

    char path[2080];
    format(path, 2080, "%s%csavestates%c%s_%d.dss",
             ((nds_t *)obj)->save_dir, '/', '/',
             ((nds_t *)obj)->rom_name, slot);
    return work(obj, path, a2, a3, a4);
}

extern int str_vsprintf_caller_limit_4_35(char *dest, unsigned long size,
                               const char *format, ...) __asm__("str_vsprintf_caller_limit_4");
extern int files_stat_resolved(const char *path, void *buf);

int64_t state_slot_mtime(unsigned char *param_1, int32_t param_2) {

    char dest[0x820];
    unsigned char statbuf[0x80];

    str_vsprintf_caller_limit_4(dest, 0x820, "%s%csavestates%c%s_%d.dss",
                        ((nds_t *)param_1)->save_dir, '/', '/',
                        ((nds_t *)param_1)->rom_name, param_2);

    int rc = nds_platform_default()->files.stat(nds_platform_default()->user, dest, statbuf);

    int64_t mtime;
    memcpy(&mtime, statbuf + 88, sizeof(mtime));

    return (rc == 0) ? mtime : 0;
}

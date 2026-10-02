#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "memory/io9.h"
#include "math_unit.h"
#include "cpu/arm.h"
#include "ipc.h"
#include "dma.h"
#include "core/nds_state.h"
#include <string.h>
#include <stdlib.h>
#include "core_internals.h"
#include "mem_access.h"

uint32_t io9_read16(unsigned char *ctx, uint32_t dir) {
    bus_t *bus = (bus_t *)ctx;
    int32_t with_sign = (int32_t)dir;

    if (with_sign > 0x5ff) {
        if (dir - 0x600 <= 0xa2) {
            void *sub = bus->gpu3d;
            if (dir == 0x600) return gpu3d_io3d_gxstat_read(sub);
            if (dir == 0x604) return gpu3d_ram_count_polygons_read(sub);
            if (dir == 0x606) return gpu3d_io3d_ram_count_read(sub);
            if ((dir & 1) == 0 && dir >= 0x640 && dir <= 0x67e) {
                uint32_t v = gpu3d_io3d_clipmtx_result_read(
                                 sub, (dir - 0x640) >> 2);
                *(uint32_t *)((unsigned char *)bus->io_mirror + (dir & 0xfffffffcu)) = v;
            } else if ((dir & 1) == 0 && dir >= 0x680 && dir <= 0x6a2) {
                uint32_t v = gpu3d_io3d_vecmtx_result_read(
                                 sub, (dir - 0x680) >> 2);
                *(uint32_t *)((unsigned char *)bus->io_mirror + (dir & 0xfffffffcu)) = v;
            }
            goto backup;
        }
        if (dir - 0x100000 > 0x12) goto backup;
        {
            uint32_t v;
            if (dir == 0x100000 || dir == 0x100002)
                v = ipc_fifo_recv(&((bus_t *)ctx)->ipc_fifo[0]);
            else if (dir == 0x100010 || dir == 0x100012)
                v = cart_data_in_next_word(((bus_t *)ctx)->cart);
            else
                goto backup;
            return v >> ((dir << 3) & 0x10);
        }
    }

    {

        if (with_sign > 0x29f) {
            uint32_t k = dir - 0x2a0;
            if (k > 0x16) goto backup;
            bus_t *bus = (bus_t *)ctx;

            if ((1u << k) & 0x5555u) {
                if (bus->math.div_result_valid) goto backup;
                math_div_unit_execute(bus);
                goto backup;
            }
            if (!((1u << k) & 0x500000u)) goto backup;
            if (bus->math.sqrt_result_valid) goto backup;

            uint32_t mode = bus->io_mirror[0].math.sqrtcnt;
            bus->math.sqrt_result_valid = 1;

            if (!(mode & 1)) {
                bus->io_mirror[0].math.sqrt_result =
                    math_sqrt_u32((uint32_t)bus->io_mirror[0].math.sqrt_param);
                goto backup;
            }

            uint64_t rest = bus->io_mirror[0].math.sqrt_param;
            if (rest == 0) { bus->io_mirror[0].math.sqrt_result = 0; goto backup; }

            uint64_t res = 0, bit = 0x1000000000000000ull, t;
            t = rest - (bit | res);
            if (rest >= (bit | res)) goto accepts;
        low_half:
            res >>= 1;
            bit >>= 2;
            if (bit == 0) goto end;
        test:
            t = rest - (bit | res);
            if (rest < (bit | res)) goto low_half;
        accepts:
            res = bit | (res >> 1);
            rest = t;
            bit >>= 2;
            if (bit != 0) goto test;
        end:
            bus->io_mirror[0].math.sqrt_result = (uint32_t)res;
            goto backup;
        }

        {
            uint32_t t = dir - 0x100;
            if (t <= 0xc && ((1u << t) & 0x1111u)) {
                nds_t *block = ((bus_t *)ctx)->machine;
                uint32_t which = t >> 2;
                nds_timer_t *tmr = &block->arm9.timers[which];

                uint32_t m = tmr->control;
                if (m & 4) goto backup;
                if (!(m & 0x80)) return tmr->reload;

                unsigned char *machine = (unsigned char *)tmr->cpu;
                uint32_t from     = (uint32_t)tmr->start_cycles;
                unsigned char *rel = (unsigned char *)((arm_t *)machine)->machine;
                uint32_t mark     = ((arm_t *)machine)->cycle_mark;
                uint32_t cycles    = *(uint32_t *)(rel + 8) + *(uint32_t *)(rel + 16);
                uint32_t scale    = tmr->prescaler_shift;
                uint32_t initial   = tmr->reload;
                return ((((cycles - mark - from) >> (scale & 31)) + initial)
                        & 0xffff);
            }
        }

        if (dir == 0x1a6) {
            unsigned char *machine = (unsigned char *)((bus_t *)ctx)->machine;
            cart_t *sec = ((bus_t *)ctx)->cart;
            uint64_t now = *(uint64_t *)(machine + 8) + *(uint32_t *)(machine + 16);
            int64_t mark  = (int32_t)((nds_t *)machine)->arm9.cycle_mark;
            uint32_t v     = bus->io_mirror[0].cart.auxspicnt;
            uint64_t deadline = sec->transfer_deadline_cycles;
            uint32_t sel = ((now - (uint64_t)mark) < deadline)
                         ? (v & 0xffffdf7fu) : v;
            return sel | 0x2000u;
        }
        if (dir == 0x204)
            return (bus->io_mirror[0].exmemcnt & 0xa8ffu) | 0x4000u;
    }

backup:
    return *(uint16_t *)((unsigned char *)bus->io_mirror + (dir & 0x7fff));
}

uint32_t io9_read8(unsigned char *obj, uint32_t dir) {
    uint32_t (*read_word)(unsigned char *, uint32_t) = io9_read16;

    uint32_t v = read_word(obj, dir & 0xfffffffeu);
    uint32_t height = (v >> 8) & 0xff;
    return (dir & 1) == 0 ? v : height;
}

#define BUS      0x1b214
#define IO9_MIRROR  0x1b070
#define IE_IF    96
#define CLOCK_OFF  8792
#define N_MARK  8848
#define OTHER     6736
extern uint32_t math_sqrt_u32(uint32_t num);




uint32_t io9_read32(uint8_t *ctx, uint32_t dir) {

    bus_t *bus = (bus_t *)ctx;

    if ((int32_t)dir > 0x5ff) {
        if (dir - 0x600u <= 0xa0u) {
            void *sub = bus->gpu3d;

            if (dir == 0x600) return gpu3d_io3d_gxstat_read(sub);

            if (dir == 0x604) {

                uint32_t low = gpu3d_ram_count_polygons_read(sub);
                void *sub2 = bus->gpu3d;
                return low | (gpu3d_io3d_ram_count_read(sub2) << 16);
            }

            if ((dir & 3u) == 0 && dir >= 0x640 && dir <= 0x67c)
                return gpu3d_io3d_clipmtx_result_read(sub, (dir - 0x640u) >> 2);
            if ((dir & 3u) == 0 && dir >= 0x680 && dir <= 0x6a0)
                return gpu3d_io3d_vecmtx_result_read(sub, (dir - 0x680u) >> 2);

            goto backup;
        }
        if (dir == 0x100000) return ipc_fifo_recv(&((bus_t *)ctx)->ipc_fifo[0]);
        if (dir == 0x100010) return cart_data_in_next_word(((bus_t *)ctx)->cart);
        goto backup;
    }

    {
        uint8_t *R = ctx + BUS;

        if ((int32_t)dir > 0x29f) {
            uint32_t k = dir - 0x2a0u;
            if (k > 0x14u) goto backup;
            bus_t *bus = (bus_t *)ctx;

            if ((1u << k) & 0x1111u) {
                if (bus->math.div_result_valid) goto backup;
                math_div_unit_execute(bus);
                goto backup;
            }
            if (k != 0x14) goto backup;
            if (bus->math.sqrt_result_valid) goto backup;

            uint32_t mode = bus->io_mirror[0].math.sqrtcnt;
            bus->math.sqrt_result_valid = 1;

            if (!(mode & 1)) {
                bus->io_mirror[0].math.sqrt_result = math_sqrt_u32((uint32_t)bus->io_mirror[0].math.sqrt_param);
                goto backup;
            }

            uint64_t rest = bus->io_mirror[0].math.sqrt_param;
            if (rest == 0) { bus->io_mirror[0].math.sqrt_result = 0; goto backup; }

            uint64_t res = 0, bit = 0x1000000000000000ull, t;
            t = rest - (bit | res);
            if (rest >= (bit | res)) goto accepts;
        low_half:
            res >>= 1;
            bit >>= 2;
            if (bit == 0) goto end;
        test:
            t = rest - (bit | res);
            if (rest < (bit | res)) goto low_half;
        accepts:
            res = bit | (res >> 1);
            rest = t;
            bit >>= 2;
            if (bit != 0) goto test;
        end:
            bus->io_mirror[0].math.sqrt_result = (uint32_t)res;
            goto backup;
        }

        uint32_t k = dir - 0x100u;
        if (k <= 0xcu && ((1u << k) & 0x1111u)) {
            nds_t *n2 = ((bus_t *)ctx)->machine;
            uint32_t which = k >> 2;
            nds_timer_t *c = &n2->arm9.timers[which];

            uint32_t ctrl = c->control;
            if (ctrl & 4) goto backup;

            uint32_t val;
            if (ctrl & 0x80) {
                uint8_t *machine = (uint8_t *)c->cpu;
                uint32_t from = (uint32_t)c->start_cycles;
                uint8_t *rel = (uint8_t *)rd_ptr(machine + CLOCK_OFF);
                uint32_t mark = rd32(machine + N_MARK);
                uint32_t cycles = rd32(rel + 8) + rd32(rel + 16);
                uint32_t scale = c->prescaler_shift & 31u;
                uint32_t initial = c->reload;
                val = ((((cycles - mark) - from) >> scale) + initial) & 0xffffu;
            } else {
                val = c->reload;
            }
            return val | rd16(ctx + (dir + 2u) + IO9_MIRROR);
        }

        if (dir == 0x1a4) {
            nds_t *n2 = ((bus_t *)ctx)->machine;
            cart_t *sec = ((bus_t *)ctx)->cart;
            uint64_t now = n2->sched.cycles + (uint64_t)n2->sched.slice_cycles;
            int64_t  mark = (int32_t)n2->arm9.cycle_mark;
            uint64_t deadline = sec->transfer_deadline_cycles;
            uint32_t v = rd32(R);
            uint32_t sel = ((now - (uint64_t)mark) < deadline)
                         ? (v & 0xdf7fffffu) : v;
            return sel | 0x20000000u;
        }

        if (dir == 0x204)
            return (rd32(R + IE_IF) & 0xa8ffu) | 0x4000u;
    }

backup:
    return rd32(ctx + (dir & 0x7fffu) + IO9_MIRROR);
}
#undef BUS
#undef IO9_MIRROR
#undef IE_IF
#undef CLOCK_OFF
#undef N_MARK
#undef OTHER

extern void     vram_vramcnt_write_cascade(vram_map_t *vram, uint8_t *bank_data, unsigned idx, unsigned updated);
extern void     gpu2d_deferred_capture_queue_push(gpu2d_engine_t *engine, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
#define R        0x1b074
#define IO9_MIRROR   0x1b070
#define M2       0x231f0
#define BAND     0xfd4b8
#define DMA_STATE   0xfd2cc
#define DMA_LAUNCH 0xfd298



static uint8_t *io(uint8_t *g) { return (uint8_t *)((bus_t *)g)->arm9_pagetable->cpu; }

static uint8_t *io40(uint8_t *g) { return (uint8_t *)((bus_t *)g)->arm7_pagetable->cpu; }

static void dma_engine(uint8_t *g, uint32_t engine_idx, uint32_t reg, uint32_t v8)
{
    uint8_t *a = (uint8_t *)((bus_t *)g)->machine;
    uint8_t *b = (uint8_t *)((bus_t *)g)->gpu;
    uint32_t line = rd16(a + 20);
    void *dst = &((gpu_t *)b)->engine[engine_idx];
    if (line <= 0xbf) {

        gpu2d_deferred_capture_queue_push(dst, reg, v8, 1, line);
    } else {
        uint8_t e[16];
        memset(e, 0, sizeof(e));
        wr32(e + 0, reg);
        wr32(e + 4, v8);
        e[9] = 1;
        gpu2d_engine_write_register(dst, e);
    }
}

static int dma_start(bus_t *bus, uint32_t cnt, uint32_t i)
{
    dma_channel_t *p = &bus->dma[0].channels[i];
    if (p->cnt & 0x80000000u) return 0;
    uint32_t when = (cnt >> 27) & 7;
    dma_regs_t *o = p->regs;
    p->start_mode = (uint8_t)when;
    p->src = o->sad;
    p->dst = o->dad;
    p->cnt = cnt;
    if (when == 0 || when == 7)
        dma_channel_run_transfer(&bus->dma[0], p);
    return 1;
}

static void scan_irq(uint8_t *g, io_mirror_t *mir9)
{
    uint8_t *o = io(g);
    uint32_t p = (mir9->irq.if_pending & mir9->irq.ie)
               & (uint32_t)(-(int32_t)mir9->irq.ime);
    ((arm_t *)o)->irq_pending = (p);
    if (p) ((arm_t *)o)->wake_flags = (((arm_t *)o)->wake_flags | 2);
}

void io9_write8(void *ctx, uint32_t reg, uint32_t val)
{
    uint8_t *c  = (uint8_t *)ctx;
    uint8_t *g  = c;
    uint8_t *r  = c + R;
    bus_t *bus = (bus_t *)ctx;
    io_mirror_t *mir9 = &bus->io_mirror[0];
    uint8_t *m2 = c + M2;
    uint8_t *bd = c + BAND;
    uint32_t v8 = val & 0xff;

    if (reg > 0xfff) {
        if ((reg - 0x1008) < 0x4e || (reg - 0x1000) < 4 || (reg - 0x106c) < 4)
            dma_engine(g, 1, reg, v8);
        else
            goto mirror;
        goto mirror;
    }
    if (reg > 0x603) goto mirror;

    if (reg <= 3 || (reg >= 8 && reg <= 0x55) || (reg >= 0x6c && reg <= 0x6f)) {
        dma_engine(g, 0, reg, v8);
        goto mirror;
    }

    if (reg >= 0x380 && reg <= 0x3bf) {
        uint8_t *s = (unsigned char *)((bus_t *)g)->gpu3d;
        uint32_t ch = (reg - 0x380) >> 1;

        uint32_t o = gpu3d_io3d_color_cache_primary_read((const gpu3d_t *)(s), ch);
        uint32_t d = (reg & 1) ? o : val;

        gpu3d_io3d_color_cache_primary_store((gpu3d_t *)((gpu3d_t *)((unsigned char *)((bus_t *)g)->gpu3d)), ch, d & 0xff);
        goto mirror;
    }
    if (reg >= 0x360 && reg <= 0x37f) {

        gpu3d_io3d_store_7bit_value((gpu3d_t *)((gpu3d_t *)((unsigned char *)((bus_t *)g)->gpu3d)), reg - 0x360, v8);
        goto mirror;
    }
    if (reg >= 0x330 && reg <= 0x33f) {
        uint8_t *s = (unsigned char *)((bus_t *)g)->gpu3d;
        uint32_t ch = (reg - 0x330) >> 1;

        uint32_t o = gpu3d_io3d_color_cache_secondary_read((const gpu3d_t *)(s), ch);
        uint32_t d = (reg & 1) ? o : val;
        gpu3d_io3d_color_cache_secondary_store((gpu3d_t *)((gpu3d_t *)((unsigned char *)((bus_t *)g)->gpu3d)), ch, d & 0xff);
        goto mirror;
    }
    if (reg == 0x280 || (reg >= 0x290 && reg <= 0x29f)) { bus->math.div_result_valid = 0; goto mirror; }
    if (reg == 0x2b0 || (reg >= 0x2b8 && reg <= 0x2bb)) { bus->math.sqrt_result_valid = 0; goto mirror; }

    switch (reg) {

    case 0x006: case 0x007: case 0x130: case 0x131:
    case 0x180: case 0x1a3: case 0x209: case 0x20a: case 0x20b:
        return;

    case 0x004:
        val = (val & ~7u) | (r[0] & 7u);
        goto mirror;

    case 0x281: val &= 0x7fu; bus->math.div_result_valid = 0; goto mirror;
    case 0x2b1: val &= 0x7fu; bus->math.sqrt_result_valid = 0; goto mirror;

    case 0x0ba: { uint32_t v = (((mir9->dma[0].cnt >> 16) & 0xffu) | v8) << 16;
                  mir9->dma[0].cnt = v; bus->dma[0].channels[0].cnt = v; return; }
    case 0x0bb: { uint32_t v = (((mir9->dma[0].cnt >> 24) & 0xffu) | val) << 24;
                  mir9->dma[0].cnt = v;
                  if ((v & 0x80000000u) && dma_start(bus, v, 0)) return;
                  bus->dma[0].channels[0].cnt = v; return; }
    case 0x0c6: { uint32_t v = (((mir9->dma[1].cnt >> 16) & 0xffu) | v8) << 16;
                  mir9->dma[1].cnt = v; bus->dma[0].channels[1].cnt = v; return; }
    case 0x0c7: { uint32_t v = (((mir9->dma[1].cnt >> 24) & 0xffu) | val) << 24;
                  mir9->dma[1].cnt = v;
                  if ((v & 0x80000000u) && dma_start(bus, v, 1)) return;
                  bus->dma[0].channels[1].cnt = v; return; }
    case 0x0d2: { uint32_t v = (((mir9->dma[2].cnt >> 16) & 0xffu) | v8) << 16;
                  mir9->dma[2].cnt = v; bus->dma[0].channels[2].cnt = v; return; }
    case 0x0d3: { uint32_t v = (((mir9->dma[2].cnt >> 24) & 0xffu) | val) << 24;
                  mir9->dma[2].cnt = v;
                  if ((v & 0x80000000u) && dma_start(bus, v, 2)) return;
                  bus->dma[0].channels[2].cnt = v; return; }
    case 0x0de: { uint32_t v = (((mir9->dma[3].cnt >> 16) & 0xffu) | v8) << 16;
                  mir9->dma[3].cnt = v; bus->dma[0].channels[3].cnt = v; return; }
    case 0x0df: { uint32_t v = (((mir9->dma[3].cnt >> 24) & 0xffu) | val) << 24;
                  mir9->dma[3].cnt = v;
                  if ((v & 0x80000000u) && dma_start(bus, v, 3)) return;
                  bus->dma[0].channels[3].cnt = v; return; }

    case 0x210: case 0x211: case 0x212: case 0x213: {
        uint32_t ie = mir9->irq.ie, updated, has;
        uint32_t b = reg - 0x210;
        if (b == 3) {
            uint32_t height = val << 24;
            has = (height & ~ie) != 0;
            updated = (height & 0xff000000u) | (ie & 0x00ffffffu);
            if (!has) { mir9->irq.ie = (updated); return; }
        } else {
            uint32_t is_set = v8 << (8 * b);
            has = (is_set & ~ie) != 0;
            updated = (ie & ~(0xffu << (8 * b))) | is_set;
        }
        mir9->irq.ie = (updated);
        if (!has) return;
        scan_irq(g, mir9);
        return;
    }

    case 0x214: case 0x215: case 0x216: case 0x217: {
        uint32_t b = reg - 0x214;
        uint32_t plus;
        if (b == 0)      plus = ~val | 0xffffff00u;
        else if (b == 3) plus = ~(val << 24);
        else             plus = ~(v8 << (8 * b));

        if (b == 2) plus = 0xff20ffffu | ~(val << 16);
        mir9->irq.if_pending = (mir9->irq.if_pending & plus);
        uint8_t *o = io(g);
        ((arm_t *)o)->irq_pending = (((arm_t *)o)->irq_pending & plus);
        return;
    }

    case 0x208: {
        uint8_t *o = io(g);

        if ((val & 1) && mir9->irq.ime == 0) {
            uint32_t p = mir9->irq.ie & mir9->irq.if_pending;
            ((arm_t *)o)->irq_pending = (p);
            if (p) ((arm_t *)o)->wake_flags = (((arm_t *)o)->wake_flags | 2);
            val = 1;
            goto mirror;
        }
        val &= 1;
        ((arm_t *)o)->irq_pending = (0);
        goto mirror;
    }

    case 0x240: case 0x241: case 0x242: case 0x243:
    case 0x244: case 0x245: case 0x246: case 0x248: case 0x249: {
        static const uint8_t idx[10]    = {0,1,2, 3, 4, 5, 6,0, 7, 8};
        uint32_t k = reg - 0x240;

        vram_vramcnt_write_cascade(&bus->gpu->vram,
                            bus->vram_bank[idx[k]], idx[k], v8);
        goto mirror;
    }

    case 0x247:
        val &= 3u;
        if (r[579] != val) {

            wram_apply_wramcnt(ctx);
            m2[193] = (uint8_t)val;
        }
        goto mirror;

    case 0x181: {
        if ((val & 0x20) && (m2[1] & 0x40)) {
            uint8_t *o = io(g);
            if (((arm_t *)o)->is_arm9 == 1) ((arm_t *)o)->wake_flags = (((arm_t *)o)->wake_flags | 4);
            uint8_t *o2 = io40(g);
            uint8_t *e  = (uint8_t *)((arm_t *)o2)->io_mirror;
            uint32_t v  = ((io_mirror_t *)e)->irq.if_pending | 0x10000u;
            ((io_mirror_t *)e)->irq.if_pending = (v);
            uint8_t *o3 = io40(g);
            if (!(((arm_t *)o3)->halt_flags & 6)) {
                uint32_t p = (((io_mirror_t *)e)->irq.ie & v)
                           & (uint32_t)(-(int32_t)((io_mirror_t *)e)->irq.ime);
                ((arm_t *)o3)->irq_pending = (p);
            }
        }
        m2[0] = (uint8_t)(val & 0xf);
        wr16(r + 0x17d, val & 0x4fu);
        goto mirror;
    }

    case 0x184: {
        uint32_t v9 = v8;
        if ((val & 4) && (bus->ipc_fifo[1].flags & IPC_FIFO_EMPTY)) {
            uint8_t *o = io(g);
            uint8_t *e = (uint8_t *)((arm_t *)o)->io_mirror;
            uint32_t v = ((io_mirror_t *)e)->irq.if_pending | 0x20000u;
            ((io_mirror_t *)e)->irq.if_pending = (v);
            uint8_t *o2 = io(g);
            if (((arm_t *)o2)->halt_flags & 6) {
                if (((arm_t *)o2)->irq_pending) ((arm_t *)o2)->wake_flags = (((arm_t *)o2)->wake_flags | 2);
            } else {
                uint32_t p = (((io_mirror_t *)e)->irq.ie & v)
                           & (uint32_t)(-(int32_t)((io_mirror_t *)e)->irq.ime);
                ((arm_t *)o2)->irq_pending = (p);
                if (p) ((arm_t *)o2)->wake_flags = (((arm_t *)o2)->wake_flags | 2);
            }
        }
        val &= 4u;

        if (v9 & 8) ipc_fifo_reset(&bus->ipc_fifo[1]);
        r[384] = (uint8_t)((r[384] & ~4u) | val);
        return;
    }

    case 0x185: {
        uint32_t vi = r[385];

        if ((val & 4) && !(bus->ipc_fifo[0].flags & IPC_FIFO_EMPTY)) {
            uint8_t *o = io(g);
            uint8_t *e = (uint8_t *)((arm_t *)o)->io_mirror;
            uint32_t v = ((io_mirror_t *)e)->irq.if_pending | 0x40000u;
            ((io_mirror_t *)e)->irq.if_pending = (v);
            uint8_t *o2 = io(g);
            if (!(((arm_t *)o2)->halt_flags & 6)) {
                uint32_t p = (((io_mirror_t *)e)->irq.ie & v)
                           & (uint32_t)(-(int32_t)((io_mirror_t *)e)->irq.ime);
                ((arm_t *)o2)->irq_pending = (p);
                if (p) ((arm_t *)o2)->wake_flags = (((arm_t *)o2)->wake_flags | 2);
            } else if (((arm_t *)o2)->irq_pending) {
                ((arm_t *)o2)->wake_flags = (((arm_t *)o2)->wake_flags | 2);
            }
        }

        uint32_t base = (v8 & 0x40) ? (vi & 0x3bu) : vi;
        r[385] = (uint8_t)((base & 0x7bu) | (val & 0x84u));
        return;
    }

    case 0x1a2: {
        uint32_t ctl = rd16(r + 412);
        uint8_t *b0 = rd_ptr_u8(bd);

        uint32_t res = spi_memory_transfer_byte(&((cart_t *)b0)->backup, v8);
        if (!(ctl & 0x40))

            spi_memory_deselect(&((cart_t *)rd_ptr(bd))->backup);
        r[414] = (uint8_t)res;
        return;
    }

    case 0x305: {
        uint8_t *a = (uint8_t *)((bus_t *)g)->machine;
        uint32_t line = rd16(a + 20);
        if (line > 0xbf) goto mirror;
        uint8_t *b = (uint8_t *)((bus_t *)g)->gpu;
        if ((r[769] & 0x80u) == (val & 0x80u)) goto mirror;

        gpu2d_deferred_capture_queue_push(&((gpu_t *)b)->engine[0],  0x305, v8, 1, line);
        gpu2d_deferred_capture_queue_push(&((gpu_t *)b)->engine[1], 0x305, v8, 1, line);
        goto mirror;
    }

    case 0x603: {
        val = (val >> 6) & 3u;
        if ((uint32_t)((val - 1) & 0xff) <= 1) {
            uint8_t *o = io(g);
            uint8_t *e = (uint8_t *)((arm_t *)o)->io_mirror;
            uint32_t v = ((io_mirror_t *)e)->irq.if_pending | 0x200000u;
            ((io_mirror_t *)e)->irq.if_pending = (v);
            uint8_t *o2 = io(g);
            if (((arm_t *)o2)->halt_flags & 6) {
                if (((arm_t *)o2)->irq_pending) ((arm_t *)o2)->wake_flags = (((arm_t *)o2)->wake_flags | 2);
            } else {
                uint32_t p = (((io_mirror_t *)e)->irq.ie & v)
                           & (uint32_t)(-(int32_t)((io_mirror_t *)e)->irq.ime);
                ((arm_t *)o2)->irq_pending = (p);
                if (p) ((arm_t *)o2)->wake_flags = (((arm_t *)o2)->wake_flags | 2);
            }
        } else {
            mir9->irq.if_pending = (mir9->irq.if_pending & 0xffdfffffu);
            uint8_t *o = io(g);
            ((arm_t *)o)->irq_pending = (((arm_t *)o)->irq_pending & 0xffdfffffu);
        }
        goto mirror;
    }

    case 0x340:
        bus->gpu3d->alpha_test_ref = (uint8_t)val;
        goto mirror;

    default:
        goto mirror;
    }

mirror:
    c[IO9_MIRROR + (reg & 0x7fff)] = (uint8_t)val;
}
#undef R
#undef IO9_MIRROR
#undef M2
#undef BAND
#undef DMA_STATE
#undef DMA_LAUNCH

extern void io9_write8_4(void *ctx, uint32_t reg, uint32_t val) __asm__("io9_write8");
#define IO9_MIRROR   0x1b070
#define DMAREG   0x1b128
#define DMA_STATE   0xfd2c8
#define DMA_LAUNCH 0xfd298






static uint8_t *io_4(uint8_t *g)
{
    return (uint8_t *)((bus_t *)g)->arm9_pagetable->cpu;
}

static void dma_engine_4(uint8_t *g, uint32_t engine_idx, uint32_t reg, uint32_t v16)
{
    uint8_t *a = (uint8_t *)((bus_t *)g)->machine;
    uint8_t *b = (uint8_t *)((bus_t *)g)->gpu;
    uint32_t line = rd16(a + 20);
    void *dst = &((gpu_t *)b)->engine[engine_idx];
    if (line <= 0xbf) {

        gpu2d_deferred_capture_queue_push(dst, reg, v16, 2, line);
    } else {
        uint8_t e[16];
        memset(e, 0, sizeof(e));
        wr32(e + 0, reg);
        wr32(e + 4, v16);
        e[9] = 2;
        gpu2d_engine_write_register(dst, e);
    }
}

void io9_write16(void *ctx, uint32_t reg, uint32_t val)
{
    uint8_t *c  = (uint8_t *)ctx;
    uint8_t *g  = c;
    bus_t *bus = (bus_t *)ctx;
    io_mirror_t *mir9w16 = &bus->io_mirror[0];

    uint32_t v16 = val & 0xffff;

    if (reg > 0xfff) {

        if ((reg - 0x1008) < 0x4e || (reg - 0x1000) < 4 || (reg - 0x106c) < 4) {
            dma_engine_4(g, 1, reg, v16);
            wr16(c + IO9_MIRROR + (reg & 0x7fff), val);
            return;
        }
        goto fallback;
    }
    if (reg > 0x610) goto fallback;

    if (reg <= 3 || (reg >= 8 && reg <= 0x55) || (reg >= 0x6c && reg <= 0x6f)) {
        dma_engine_4(g, 0, reg, v16);
        wr16(c + IO9_MIRROR + (reg & 0x7fff), val);
        return;
    }

    if (reg == 0xba || reg == 0xc6 || reg == 0xd2 || reg == 0xde) {
        uint32_t i = (reg - 0xba) / 12;
        dma_regs_t *r = &mir9w16->dma[i];
        dma_channel_t *p = &bus->dma[0].channels[i];
        uint32_t cnt = (r->cnt & 0xffffu) | (v16 << 16);
        r->cnt = (r->cnt & 0xffffu) | ((uint32_t)(uint16_t)val << 16);
        if ((v16 & 0x8000) && !(p->cnt & 0x80000000u)) {
            uint32_t when = (v16 >> 11) & 7;
            dma_regs_t *o = p->regs;
            p->start_mode = (uint8_t)when;
            p->src = o->sad;
            p->dst = o->dad;
            p->cnt = cnt;
            if (when == 0 || when == 7)
                dma_channel_run_transfer(&bus->dma[0], p);
            return;
        }
        p->cnt = cnt;
        return;
    }

    if (reg >= 0x100 && reg <= 0x10e && !(reg & 1)) {
        uint32_t i = (reg - 0x100) / 4;
        uint8_t *o = io_4(g);
        nds_timer_t *t = &((arm_t *)o)->timers[i];
        if ((reg & 2) == 0) {
            t->reload = (uint16_t)val;
            wr16(c + IO9_MIRROR + (reg & 0x7fff), val);
            return;
        }
        uint32_t pre  = val & 3;
        uint32_t shift = pre ? (pre * 2 + 5) : 1;
        uint32_t count = (0x10000u - t->reload) << shift;
        t->prescaler_shift = (uint8_t)shift;
        t->period_cycles = count;

        uint8_t *ev = (uint8_t *)((bus_t *)g)->machine;
        uint32_t idx = SCHED_DEADLINE_TIMER_BASE + 4 + i;
        if ((val & 0x80) && !(v16 & 4) && !(t->control & 0x80)) {

            int64_t base = (int32_t)((arm_t *)o)->cycle_mark;
            uint32_t live = t->scheduled;
            uint64_t start = rd64(ev + 8);
            uint32_t len = rd32(ev + 16);
            uint64_t now = (start + len) - (uint64_t)base;
            uint32_t d = (uint32_t)(count - (uint32_t)start) + (uint32_t)now;
            uint32_t deadline = (len > d) ? len : d;
            t->start_cycles = now;

            if (live) sched_deadline_remove(&((nds_t *)ev)->sched, idx);
            sched_deadline_insert(&((nds_t *)ev)->sched, deadline, idx);
            t->scheduled = 1;
        } else if (!(val & 0x80) && t->scheduled) {
            sched_deadline_remove(&((nds_t *)ev)->sched, idx);
            t->scheduled = 0;
        }
        t->control = (uint16_t)val;
        wr16(c + IO9_MIRROR + (reg & 0x7fff), val);
        return;
    }

    switch (reg) {
    case 0x188:
        return;

    case 0x060: {
        bus->gpu3d->disp3dcnt = val & 0xcfffu;
        val &= ~0x3000u;
        break;
    }

    case 0x208: {
        uint8_t *o = io_4(g);
        if ((val & 1) && mir9w16->irq.ime == 0) {
            uint32_t p = mir9w16->irq.ie & mir9w16->irq.if_pending;
            ((arm_t *)o)->irq_pending = (p);
            if (p) ((arm_t *)o)->wake_flags = (((arm_t *)o)->wake_flags | 2);
            val = 1;
        } else {
            ((arm_t *)o)->irq_pending = (0);
            val &= 1;
        }
        break;
    }

    case 0x210: case 0x212: {
        uint32_t ie = mir9w16->irq.ie, updated, has;
        if (reg == 0x210) {
            has = (v16 & ~ie) != 0;
            updated = (ie & 0xffff0000u) | v16;
        } else {
            uint32_t height = val << 16;
            has = (height & ~ie) != 0;
            updated = height | (ie & 0xffffu);
        }
        mir9w16->irq.ie = (updated);
        if (has) {
            uint8_t *o = io_4(g);
            uint32_t p = (mir9w16->irq.if_pending & updated)
                       & (uint32_t)(-(int32_t)mir9w16->irq.ime);
            ((arm_t *)o)->irq_pending = (p);
            if (p) ((arm_t *)o)->wake_flags = (((arm_t *)o)->wake_flags | 2);
        }
        return;
    }

    case 0x214: case 0x216: {
        uint32_t plus = (reg == 0x214) ? (~val | 0xffff0000u)
                                      : (0x20ffffu | ~(val << 16));
        mir9w16->irq.if_pending = (mir9w16->irq.if_pending & plus);
        uint8_t *o = io_4(g);
        ((arm_t *)o)->irq_pending = (((arm_t *)o)->irq_pending & plus);
        return;
    }

    case 0x280:  bus->math.div_result_valid = 0; val &= 0x7fffu; break;
    case 0x2b0:  bus->math.sqrt_result_valid = 0; val &= 0x7fffu; break;
    case 0x2b8: case 0x2ba: bus->math.sqrt_result_valid = 0; break;

    case 0x350: wr16(&bus->gpu3d->clear_color, val); break;
    case 0x352: wr16((uint8_t *)&bus->gpu3d->clear_color + 2, val); break;
    case 0x354: bus->gpu3d->clear_depth = (uint16_t)val; break;
    case 0x356: bus->gpu3d->clear_image_offset = (uint16_t)val; break;
    case 0x35c: bus->gpu3d->fog_offset = (uint16_t)val; break;
    case 0x610: bus->gpu3d->dot_depth = (uint16_t)val; break;

    default:
        if (reg >= 0x290 && reg <= 0x29e && !(reg & 1)) {
            bus->math.div_result_valid = 0;
        } else if (reg >= 0x330 && reg <= 0x33e && !(reg & 1)) {
            gpu3d_io3d_color_cache_secondary_store((gpu3d_t *)((gpu3d_t *)((unsigned char *)((bus_t *)g)->gpu3d)), (reg - 0x330) >> 1, v16);
        } else if (reg >= 0x360 && reg <= 0x37e && !(reg & 1)) {
            void *m = (unsigned char *)((bus_t *)g)->gpu3d;

            gpu3d_io3d_store_7bit_value((gpu3d_t *)((gpu3d_t *)((unsigned char *)m)), reg - 0x360, v16);
            gpu3d_io3d_store_7bit_value((gpu3d_t *)((gpu3d_t *)((unsigned char *)((bus_t *)g)->gpu3d)), reg - 0x35f,
                                (val >> 8) & 0xff);
        } else if (reg >= 0x380 && reg <= 0x3be && !(reg & 1)) {

            gpu3d_io3d_color_cache_primary_store((gpu3d_t *)((gpu3d_t *)((unsigned char *)((bus_t *)g)->gpu3d)), (reg - 0x380) >> 1, v16);
        } else {
            goto fallback;
        }
        break;
    }

    wr16(c + IO9_MIRROR + (reg & 0x7fff), val);
    return;

fallback:

    io9_write8(ctx, reg, val);
    io9_write8(ctx, reg + 1, (val >> 8) & 0xff);
}
#undef IO9_MIRROR
#undef DMAREG
#undef DMA_STATE
#undef DMA_LAUNCH

static void mirror_write(void *param_1, uint32_t idx, uint32_t val)
{
    uint8_t *dst = (uint8_t *)((bus_t *)param_1)->io_mirror + (idx & 0x7fffu);
    memcpy(dst, &val, 4);
}

static void generic_two_call(void *param_1, uint32_t idx, uint32_t val)
{
    io9_write16(param_1, idx, val);
    io9_write16(param_1, idx + 2, val >> 16);
}

static void generic_io_write(void *param_1, uint32_t idx, uint32_t val, uint32_t engine_idx)
{
    bus_t *bus = (bus_t *)param_1;
    void *p_x8 = bus->machine;
    uint16_t w4; memcpy(&w4, (uint8_t *)p_x8 + 0x14, 2);
    void *x0 = &bus->gpu->engine[engine_idx];

    if (w4 > 0xbf) {
        struct { uint32_t idx; uint32_t val; uint8_t pad; uint8_t width; } buf;
        buf.idx = idx; buf.val = val; buf.width = 4;
        gpu2d_engine_write_register(x0, &buf);
    } else {

        gpu2d_deferred_capture_queue_push(x0, idx, val, 4, w4);
    }
    mirror_write(param_1, idx, val);
}

static void shared_ipc_tail(dma_t *a, dma_channel_t *b)
{
    dma_channel_run_transfer(a, b);
}

void io9_write32(void *param_1, uint32_t param_2, uint64_t param_3)
{

    uint32_t w20 = (uint32_t)param_3;

    uint32_t w21 = param_2;

    uint8_t *ctx = (uint8_t *)param_1;
    bus_t *bus = (bus_t *)ctx;
    io_mirror_t *mir9w32 = &bus->io_mirror[0];
    uint8_t *cnt9 = (uint8_t *)&mir9w32->ipc.ipcfifocnt;

    if ((int32_t)param_2 <= 0x1007) {
        if (param_2 <= 0x5c4) {
            switch (param_2) {

            case 0x440: case 0x444: case 0x448: case 0x44c: case 0x450: case 0x454: case 0x458: case 0x45c: case 0x460: case 0x464: case 0x468: case 0x46c: case 0x470: case 0x474: case 0x478: case 0x47c: case 0x480: case 0x484: case 0x488: case 0x48c: case 0x490: case 0x494: case 0x498: case 0x49c: case 0x4a0: case 0x4a4: case 0x4a8: case 0x4ac: case 0x4b0: case 0x4b4: case 0x4b8: case 0x4bc: case 0x4c0: case 0x4c4: case 0x4c8: case 0x4cc: case 0x4d0: case 0x4d4: case 0x4d8: case 0x4dc: case 0x4e0: case 0x4e4: case 0x4e8: case 0x4ec: case 0x4f0: case 0x4f4: case 0x4f8: case 0x4fc: case 0x500: case 0x504: case 0x508: case 0x50c: case 0x510: case 0x514: case 0x518: case 0x51c: case 0x520: case 0x524: case 0x528: case 0x52c: case 0x530: case 0x534: case 0x538: case 0x53c: case 0x540: case 0x544: case 0x548: case 0x54c: case 0x550: case 0x554: case 0x558: case 0x55c: case 0x560: case 0x564: case 0x568: case 0x56c: case 0x570: case 0x574: case 0x578: case 0x57c: case 0x580: case 0x584: case 0x588: case 0x58c: case 0x590: case 0x594: case 0x598: case 0x59c: case 0x5a0: case 0x5a4: case 0x5a8: case 0x5ac: case 0x5b0: case 0x5b4: case 0x5b8: case 0x5bc: case 0x5c0: case 0x5c4: {
                void *x0 = bus->gpu3d;
                uint32_t idx_channel = ((param_2 - 0x440u) >> 2) + 0x10u;
                gpu3d_gxfifo_pack_command(x0, idx_channel, w20);
                return;
            }

            case 0x0: case 0x1: case 0x2: case 0x3:
            case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe: case 0xf:
            case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
            case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
            case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
            case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
            case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
            case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
            case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
            case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
            case 0x6c: case 0x6d: case 0x6e: case 0x6f:
                generic_io_write(param_1, w21, w20, 0);
                return;

            case 0x380: case 0x384: case 0x388: case 0x38c: case 0x390: case 0x394: case 0x398: case 0x39c: case 0x3a0: case 0x3a4: case 0x3a8: case 0x3ac: case 0x3b0: case 0x3b4: case 0x3b8: case 0x3bc: {
                void *x0a = bus->gpu3d;
                uint32_t base_idx = (param_2 - 0x380u) >> 1;
                gpu3d_io3d_color_cache_primary_store(x0a, base_idx, w20);
                void *x0b = bus->gpu3d;
                gpu3d_io3d_color_cache_primary_store(x0b, base_idx + 1, w20 >> 16);
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x400: case 0x404: case 0x408: case 0x40c: case 0x410: case 0x414: case 0x418: case 0x41c: case 0x420: case 0x424: case 0x428: case 0x42c: case 0x430: case 0x434: case 0x438: case 0x43c: {
                void *x0 = bus->gpu3d;
                gpu3d_gxfifo_push_word(x0, w20);
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x360: case 0x364: case 0x368: case 0x36c: case 0x370: case 0x374: case 0x378: case 0x37c: {
                uint32_t idx0 = param_2 - 0x360u;
                void *x0;
                x0 = bus->gpu3d;
                gpu3d_io3d_store_7bit_value(x0, idx0 + 0, (w20 >> 0) & 0xffu);
                x0 = bus->gpu3d;
                gpu3d_io3d_store_7bit_value(x0, idx0 + 1, (w20 >> 8) & 0xffu);
                x0 = bus->gpu3d;
                gpu3d_io3d_store_7bit_value(x0, idx0 + 2, (w20 >> 16) & 0xffu);
                x0 = bus->gpu3d;
                gpu3d_io3d_store_7bit_value(x0, idx0 + 3, (w20 >> 24) & 0xffu);
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x330: case 0x334: case 0x338: case 0x33c: {
                void *x0a = bus->gpu3d;
                uint32_t base_idx = (param_2 - 0x330u) >> 1;
                gpu3d_io3d_color_cache_secondary_store(x0a, base_idx, w20);
                void *x0b = bus->gpu3d;
                gpu3d_io3d_color_cache_secondary_store(x0b, base_idx + 1, w20 >> 16);
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x208: {
                arm_t *eng = bus->arm9_pagetable->cpu;
                if (w20 & 1u) {
                    uint32_t flag; flag = mir9w32->irq.ime;
                    if (flag == 0) {
                        uint32_t a, b, r;
                        a = mir9w32->irq.if_pending;
                        b = mir9w32->irq.ie;
                        r = a & b;
                        eng->irq_pending = r;
                        if (r != 0) {
                            uint32_t f2 = eng->wake_flags;
                            f2 |= 2u;
                            eng->wake_flags = f2;
                        }
                        w20 = 1u;
                        mirror_write(param_1, w21, w20);
                        return;
                    }
                }
                w20 &= 1u;
                { uint32_t zero = 0; eng->irq_pending = zero; }
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x350: {
                bus->gpu3d->clear_color = w20;
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x60: {
                w20 &= 0xffffcfffu;
                bus->gpu3d->disp3dcnt = w20;
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0xb8: {
                dma_channel_t *ch = &bus->dma[0].channels[0];
                mir9w32->dma[0].cnt = w20;
                if ((int32_t)w20 >= 0) {
                    ch->cnt = w20;
                    return;
                }
                {
                    uint32_t old = ch->cnt;
                    if ((int32_t)old < 0) {
                        ch->cnt = w20;
                        return;
                    }
                    uint32_t w8v = (w20 >> 27) & 7u;
                    ch->start_mode = (uint8_t)w8v;
                    dma_regs_t *tbl = ch->regs;
                    uint32_t t0 = tbl->sad, t1 = tbl->dad;
                    ch->src = t0;
                    ch->cnt = w20;
                    ch->dst = t1;
                    if (w8v == 7u || w8v == 0u) {
                        shared_ipc_tail(&bus->dma[0], &bus->dma[0].channels[0]);
                    }
                    return;
                }
            }

            case 0xc4: {
                dma_channel_t *ch = &bus->dma[0].channels[1];
                mir9w32->dma[1].cnt = w20;
                if ((int32_t)w20 >= 0) {
                    ch->cnt = w20;
                    return;
                }
                {
                    uint32_t old = ch->cnt;
                    if ((int32_t)old < 0) {
                        ch->cnt = w20;
                        return;
                    }
                    uint32_t w8v = (w20 >> 27) & 7u;
                    ch->start_mode = (uint8_t)w8v;
                    dma_regs_t *tbl = ch->regs;
                    uint32_t t0 = tbl->sad, t1 = tbl->dad;
                    ch->src = t0;
                    ch->dst = t1;
                    ch->cnt = w20;
                    if (w8v == 7u || w8v == 0u) {
                        shared_ipc_tail(&bus->dma[0], &bus->dma[0].channels[1]);
                    }
                    return;
                }
            }

            case 0x210: {
                uint32_t prev_val; prev_val = mir9w32->irq.ie;
                mir9w32->irq.ie = w20;
                if ((w20 & ~prev_val) == 0) {
                    return;
                }
                {
                    pagetable_t *baseA = bus->arm9_pagetable;
                    arm_t *eng = baseA->cpu;
                    uint32_t a, thresh, r;
                    a = mir9w32->irq.if_pending;
                    thresh = mir9w32->irq.ime;
                    r = a & w20;
                    r &= (uint32_t)(-(int32_t)thresh);
                    eng->irq_pending = r;
                    if (r != 0) {
                        uint32_t f2 = eng->wake_flags;
                        f2 |= 2u;
                        eng->wake_flags = f2;
                    }
                    return;
                }
            }

            case 0xd0: {
                dma_channel_t *ch = &bus->dma[0].channels[2];
                mir9w32->dma[2].cnt = w20;
                if ((int32_t)w20 >= 0) {
                    ch->cnt = w20;
                    return;
                }
                {
                    uint32_t old = ch->cnt;
                    if ((int32_t)old < 0) {
                        ch->cnt = w20;
                        return;
                    }
                    uint32_t w8v = (w20 >> 27) & 7u;
                    ch->start_mode = (uint8_t)w8v;
                    dma_regs_t *tbl = ch->regs;
                    uint32_t t0 = tbl->sad, t1 = tbl->dad;
                    ch->src = t0;
                    ch->dst = t1;
                    ch->cnt = w20;
                    if (w8v == 7u || w8v == 0u) {
                        shared_ipc_tail(&bus->dma[0], &bus->dma[0].channels[2]);
                    }
                    return;
                }
            }

            case 0xdc: {
                dma_channel_t *ch = &bus->dma[0].channels[3];
                mir9w32->dma[3].cnt = w20;
                if ((int32_t)w20 >= 0) {
                    ch->cnt = w20;
                    return;
                }
                {
                    uint32_t old = ch->cnt;
                    if ((int32_t)old < 0) {
                        ch->cnt = w20;
                        return;
                    }
                    uint32_t w8v = (w20 >> 27) & 7u;
                    ch->start_mode = (uint8_t)w8v;
                    dma_regs_t *tbl = ch->regs;
                    uint32_t t0 = tbl->sad, t1 = tbl->dad;
                    ch->src = t0;
                    ch->dst = t1;
                    ch->cnt = w20;
                    if (w8v == 7u || w8v == 0u) {
                        shared_ipc_tail(&bus->dma[0], &bus->dma[0].channels[3]);
                    }
                    return;
                }
            }

            case 0x188: {
                int8_t sb = (int8_t)cnt9[1];
                if (sb >= 0) {
                    return;
                }
                uint8_t flagbyte = bus->ipc_fifo[1].flags;
                if (flagbyte & 2u) {
                    cnt9[1] |= 0x40u;
                    return;
                }
                uint8_t *x21p = (uint8_t *)&bus->io_mirror[1].ipc.ipcfifocnt + 1;
                ipc_fifo_push(&bus->ipc_fifo[1], w20);

                uint8_t v21; memcpy(&v21, x21p, 1);
                v21 &= 0xfeu;
                memcpy(x21p, &v21, 1);

                uint8_t v_cc = cnt9[0];
                uint8_t v10 = v_cc & 0xfeu;
                cnt9[0] = v10;

                uint8_t v_1e6 = bus->ipc_fifo[1].flags;
                if (v_1e6 & 2u) {
                    v10 |= 2u;
                    v21 |= 2u;
                    cnt9[0] = v10;
                    memcpy(x21p, &v21, 1);
                }
                if (v_1e6 & 1u) {
                    return;
                }
                if ((v21 & 4u) == 0u) {
                    return;
                }

                pagetable_t *baseA = bus->arm9_pagetable;
                arm_t *engA = baseA->cpu;
                uint32_t g = engA->is_arm9;
                if (g == 1u) {
                    uint32_t f2 = engA->wake_flags;
                    f2 |= 4u;
                    engA->wake_flags = f2;
                }

                pagetable_t *baseB = bus->arm7_pagetable;
                arm_t *engB = baseB->cpu;
                void *deep = engB->io_mirror;

                uint8_t sflag = (uint8_t)engB->halt_flags;
                if (sflag & 6u) {
                    return;
                }
                uint32_t d214; memcpy(&d214, (uint8_t *)deep + 0x214, 4);
                d214 |= 0x40000u;
                memcpy((uint8_t *)deep + 0x214, &d214, 4);

                uint32_t d210; memcpy(&d210, (uint8_t *)deep + 0x210, 4);
                uint32_t d208; memcpy(&d208, (uint8_t *)deep + 0x208, 4);
                uint32_t res = d210 & d214;
                res &= (uint32_t)(-(int32_t)d208);
                engB->irq_pending = res;
                return;
            }

            case 0x1a4: {
                if ((int32_t)w20 >= 0) {
                    mirror_write(param_1, w21, w20);
                    return;
                }
                void *ptr = bus->cart;
                cart_command_execute(ptr, w20);
                return;
            }

            case 0x214: {
                uint32_t v; v = mir9w32->irq.if_pending;
                uint32_t mask = ~w20;
                mask |= 0x200000u;
                v &= mask;
                mir9w32->irq.if_pending = v;
                pagetable_t *base = bus->arm9_pagetable;
                arm_t *eng = base->cpu;
                uint32_t e = eng->irq_pending;
                e &= mask;
                eng->irq_pending = e;
                return;
            }

            case 0x280: {
                w20 &= 0xffff7fffu;
                bus->math.div_result_valid = 0;
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x290: case 0x294: case 0x298: case 0x29c: {
                bus->math.div_result_valid = 0;
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x2b0: {
                w20 &= 0xffff7fffu;
                bus->math.sqrt_result_valid = 0;
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x2b8: {
                bus->math.sqrt_result_valid = 0;
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x354: {
                bus->gpu3d->clear_depth = (uint16_t)w20;
                mirror_write(param_1, w21, w20);
                return;
            }

            case 0x358: {
                uint32_t field = (w20 >> 16) & 0x1fu;
                uint32_t ret = gpu3d_geometry_color_expand_bgr555_to_rgb8_alpha(w20, field);
                bus->gpu3d->fog_color = ret;
                mirror_write(param_1, w21, w20);
                return;
            }

            default:
                generic_two_call(param_1, w21, w20);
                return;
            }
        }

        if (param_2 - 0x1000u < 4u) {
            generic_io_write(param_1, w21, w20, 1);
            return;
        }
        generic_two_call(param_1, w21, w20);
        return;

    } else {

        if (param_2 - 0x1008u < 0x4eu) {
            generic_io_write(param_1, w21, w20, 1);
            return;
        }
        if (param_2 - 0x106cu < 4u) {
            generic_io_write(param_1, w21, w20, 1);
            return;
        }
        if (param_2 == 0x100010u) {
            void *ptr = bus->cart;
            cart_data_transfer_step(ptr, w20);
            return;
        }
        generic_two_call(param_1, w21, w20);
        return;
    }
}

uint64_t io9_write32_noop_tail(uint64_t x0)
{
    return x0;
}

#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "memory/io7.h"
#include "cpu/arm.h"
#include "ipc.h"
#include "dma.h"
#include "core/nds_state.h"
#include <string.h>
#include "core_internals.h"
#include "mem_access.h"

uint32_t io7_read16(unsigned char *ctx, uint32_t dir) {
    bus_t *bus = (bus_t *)ctx;
    int32_t with_sign = (int32_t)dir;

    if (with_sign > 0x203) {
        if (with_sign > 0x10000f) {
            if (dir != 0x100010 && dir != 0x100012) goto backup;
            uint32_t v = cart_data_in_next_word(
                             bus->cart);
            return v >> ((dir << 3) & 0x10);
        }
        if (dir == 0x204) {
            uint32_t base = bus->io_mirror[0].exmemcnt;
            uint32_t low = bus->io_mirror[1].exmemcnt;
            base = (base & 0xa880u) | (low & 0x3fu);
            return base | 0x4000u;
        }
        if (dir != 0x100000) goto backup;
        return ipc_fifo_recv(&((bus_t *)ctx)->ipc_fifo[1]);
    }

    {
        uint32_t r = dir - 0x1a0;
        if (r <= 0xe) {
            if ((1u << r) & 0x5505u)
                return *(uint16_t *)((unsigned char *)bus->io_mirror + dir);
            if (r == 6) {
                unsigned char *machine = (unsigned char *)((bus_t *)ctx)->machine;
                cart_t *sec = ((bus_t *)ctx)->cart;
                uint64_t now = *(uint64_t *)(machine + 8)
                               + *(uint32_t *)(machine + 16);
                int64_t mark = (int32_t)((nds_t *)machine)->arm9.cycle_mark;
                uint32_t v = bus->io_mirror[0].cart.auxspicnt;
                uint64_t deadline = sec->transfer_deadline_cycles;
                uint32_t sel = ((now - (uint64_t)mark) < deadline)
                             ? (v & 0xffffdf7fu) : v;
                return sel | 0x2000u;
            }
        }
    }

    {
        uint32_t t = dir - 0x100;
        if (t > 0xc) goto backup;
        if (!((1u << t) & 0x1111u)) goto backup;

        nds_t *block = ((bus_t *)ctx)->machine;
        uint32_t which = t >> 2;
        nds_timer_t *tmr = &block->arm7.timers[which];

        uint32_t mode = tmr->control;
        if (mode & 4) goto backup;
        if (!(mode & 0x80)) return tmr->reload;

        unsigned char *machine  = (unsigned char *)tmr->cpu;
        uint32_t from      = (uint32_t)tmr->start_cycles;
        unsigned char *rel  = (unsigned char *)((arm_t *)machine)->machine;
        uint32_t mark      = ((arm_t *)machine)->cycle_mark;
        uint32_t cycles     = *(uint32_t *)(rel + 8) + *(uint32_t *)(rel + 16);
        uint32_t scale     = tmr->prescaler_shift;
        uint32_t initial    = tmr->reload;
        return ((((cycles - mark - from) >> (scale & 31)) + initial)
                & 0xffff);
    }

backup:
    return *(uint16_t *)((unsigned char *)&bus->io_mirror[1] + (dir & 0x7fff));
}

uint32_t io7_read8(unsigned char *obj, uint32_t dir) {
    uint32_t (*read_word)(unsigned char *, uint32_t) = io7_read16;

    uint32_t v = read_word(obj, dir & 0xfffffffeu);
    uint32_t height = (v >> 8) & 0xff;
    return (dir & 1) == 0 ? v : height;
}





static uint32_t read_flat(const uint8_t *ctx, uint32_t dir) {
    return rd32((const uint8_t *)&((const bus_t *)ctx)->io_mirror[1] + (dir & 0x7fffu));
}

uint32_t io7_read32(uint8_t *ctx, uint32_t dir) {

    bus_t *bus7 = (bus_t *)ctx;

    if ((int32_t)dir <= 0x19f) {
        uint32_t k = dir - 0x100u;
        if (k > 12) return read_flat(ctx, dir);
        if (((1u << k) & 0x1111u) == 0)
            return read_flat(ctx, dir);

        nds_t *core = ((bus_t *)ctx)->machine;
        uint32_t i = k >> 2;
        nds_timer_t *c = &core->arm7.timers[i];
        uint32_t ctrl = c->control;
        if (ctrl & 4) return read_flat(ctx, dir);

        uint32_t val;
        if (ctrl & 0x80) {
            uint8_t *r = (uint8_t *)c->cpu;
            uint32_t start = (uint32_t)c->start_cycles;
            uint8_t *clock = (uint8_t *)((arm_t *)r)->machine;
            uint32_t base = ((arm_t *)r)->cycle_mark;
            uint32_t cycles = rd32(clock + 8) + rd32(clock + 16);
            uint32_t scale = c->prescaler_shift & 31u;
            uint32_t v0 = c->reload;
            val = (((cycles - base) - start) >> scale) + v0;
            val &= 0xffffu;
        } else {
            val = c->reload;
        }

        return val | rd16((uint8_t *)&bus7->io_mirror[1] + (dir + 2));
    }

    uint8_t *bus = (uint8_t *)&bus7->io_mirror[0].cart + 4;

    if ((int32_t)dir > 0x203) {
        if (dir == 0x204) {
            uint32_t a = bus7->io_mirror[0].exmemcnt & 0xa880u;
            uint32_t b = bus7->io_mirror[1].exmemcnt;
            a = (a & ~0x3fu) | (b & 0x3fu);
            return a | 0x4000u;
        }

        if (dir == 0x100000) return ipc_fifo_recv(&((bus_t *)ctx)->ipc_fifo[1]);

        if (dir == 0x100010) return cart_data_in_next_word(((bus_t *)ctx)->cart);
        return read_flat(ctx, dir);
    }

    uint32_t k = dir - 0x1a0u;
    if (k > 12) return read_flat(ctx, dir);

    switch (k) {
    case 0x0: case 0x8: case 0xc:
        return rd32((uint8_t *)bus7->io_mirror + dir);

    case 0x4: {
        uint8_t *n = (uint8_t *)bus7->machine;
        cart_t *o = ((bus_t *)ctx)->cart;
        int64_t  begin = (int32_t)((nds_t *)n)->arm9.cycle_mark;

        uint64_t now = rd64(n + 8) + (uint64_t)rd32(n + 16);
        uint64_t lim = o->transfer_deadline_cycles;
        uint32_t v = rd32(bus);
        uint32_t no_bit = v & 0xdf7fffffu;
        uint32_t r = ((now - (uint64_t)begin) < lim) ? no_bit : v;
        return r | 0x20000000u;
    }

    default:
        return read_flat(ctx, dir);
    }
}

uint64_t io7_write8_noop_tail(uint64_t x0)
{
    return x0;
}






static void tail_generic(bus_t *param_1, uint32_t idx, uint32_t val)
{
    wr8((uint8_t *)&param_1->io_mirror[1] + (idx & 0x7fffu), (uint8_t)val);
}

static void tail_default(bus_t *param_1, uint32_t idx, uint32_t val)
{
    if ((idx >> 23) != 0) return;
    tail_generic(param_1, idx, val);
}

static void mirror_write8_arm9(bus_t *param_1, uint32_t idx, uint32_t val)
{
    wr8((uint8_t *)param_1->io_mirror + idx, (uint8_t)val);
    tail_generic(param_1, idx, val);
}

static void arm7_irq_raise(bus_t *param_1, io_mirror_t *mir7, uint32_t w9)
{
    pagetable_t *v = param_1->arm7_pagetable;
    arm_t *ptr2    = v->cpu;
    uint32_t f528 = mir7->irq.if_pending;
    uint32_t f516 = mir7->irq.ime;
    uint32_t merged = w9 & f528;
    uint32_t res = merged & (uint32_t)(-(int32_t)f516);
    ((arm_t *)ptr2)->irq_pending = (res);
    if (res == 0) return;
    uint32_t wake = ((arm_t *)ptr2)->wake_flags;
    ((arm_t *)ptr2)->wake_flags = (wake | 2u);
}

void io7_write8(bus_t *param_1, uint32_t param_2, uint32_t param_3)
{

    bus_t *bus8 = (bus_t *)param_1;

    if ((param_2 - 0x400u) < 0x100u) {
        spu_t *spu = ((bus_t *)param_1)->spu;
        uint8_t *pbVar1 = (uint8_t *)&bus8->io_mirror[1] + param_2;
        uint32_t nib  = param_2 & 0xfu;
        uint32_t chan = (param_2 >> 4) & 0xfu;
        spu_channel_t *channel = &spu->channels[chan];
        wr8(pbVar1, (uint8_t)param_3);

        if (nib < 3u) {
            channel->recalc_flags |= 2u;
            wr8(pbVar1, (uint8_t)param_3);
            return;
        }
        if (nib - 8u < 2u) {
            channel->recalc_flags |= 1u;
            wr8(pbVar1, (uint8_t)param_3);
            return;
        }
        if (nib != 3u) {
            wr8(pbVar1, (uint8_t)param_3);
            return;
        }

        wr8(pbVar1, (uint8_t)param_3);
        if ((int8_t)(uint8_t)param_3 < 0) {
            spu_channel_start(spu, chan);
        } else {
            channel->active = 0;
        }
        channel->recalc_flags |= 2u;
        return;
    }

    uint8_t *x22 = (uint8_t *)&bus8->io_mirror[1] + 4;
    io_mirror_t *mir7 = &((bus_t *)param_1)->io_mirror[1];

    if ((int32_t)param_2 <= 0x12f) {
        if ((param_2 - 6u) < 2u) return;
        if (param_2 != 4u) { tail_default(param_1, param_2, param_3); return; }
        uint8_t b = rd8(x22);
        param_3 = (param_3 & ~0x7u) | (b & 0x7u);
        tail_generic(param_1, param_2, param_3);
        return;
    }

    if ((param_2 - 0x130u) > 0x1d1u) {
        if ((param_2 - 0x508u) < 2u) {
            spu_slot_activate
                (((bus_t *)param_1)->spu, param_2 - 0x508u, param_3 & 0xffu);
            tail_generic(param_1, param_2, param_3);
            return;
        }
        tail_default(param_1, param_2, param_3);
        return;
    }

    uint8_t *x23 = (uint8_t *)&bus8->io_mirror[0].ipc;

    switch (param_2) {

    case 0x130: case 0x131: case 0x136: case 0x137:
    case 0x188: case 0x189: case 0x18a: case 0x18b:
    case 0x1a3: case 0x1c3: case 0x209: case 0x20a: case 0x20b: case 0x241:
        return;

    case 0x138: {
        rtc_t *v = bus8->rtc;
        param_3 = rtc_register_write
                      (v, param_3 & 0xffu);
        tail_generic(param_1, param_2, param_3);
        return;
    }

    case 0x180: case 0x1a4: case 0x1a5: case 0x1a6: case 0x1a7:
    case 0x1c0: case 0x1c1:
        tail_generic(param_1, param_2, param_3);
        return;

    case 0x181: {
        if ((param_3 & 0x20u) != 0) {
            uint8_t b1 = rd8(x23 + 1);
            if ((b1 & 0x40u) != 0) {
                pagetable_t *v1 = bus8->arm7_pagetable;
                arm_t *ptrA = ((pagetable_t *)v1)->cpu;
                uint32_t cpu_is_arm9 = ((arm_t *)ptrA)->is_arm9;
                if (cpu_is_arm9 == 1u) {
                    uint32_t wake = ((arm_t *)ptrA)->wake_flags;
                    ((arm_t *)ptrA)->wake_flags = (wake | 4u);
                }
                pagetable_t *v2 = bus8->arm9_pagetable;
                arm_t *ptrB = ((pagetable_t *)v2)->cpu;
                io_mirror_t *ptrC = ((arm_t *)ptrB)->io_mirror;
                uint32_t merged = ((io_mirror_t *)ptrC)->irq.if_pending | 0x10000u;
                ((io_mirror_t *)ptrC)->irq.if_pending = (merged);
                pagetable_t *v2b = bus8->arm9_pagetable;
                arm_t *ptrB2 = ((pagetable_t *)v2b)->cpu;
                uint8_t status = (uint8_t)((arm_t *)ptrB2)->halt_flags;
                if ((status & 6u) == 0) {
                    uint32_t p210 = ((io_mirror_t *)ptrC)->irq.ie;
                    int32_t  p208 = (int32_t)((io_mirror_t *)ptrC)->irq.ime;
                    uint32_t res = (p210 & merged) & (uint32_t)(-p208);
                    ((arm_t *)ptrB2)->irq_pending = (res);
                }
            }
        }
        wr8(x23, (uint8_t)(param_3 & 0xfu));
        wr16(x22 + 0x17d, (uint16_t)(param_3 & 0x4fu));
        tail_generic(param_1, param_2, param_3);
        return;
    }

    case 0x184: {
        uint8_t saved = (uint8_t)(param_3 & 0xffu);
        if ((param_3 & 0x4u) != 0) {
            if ((((bus_t *)param_1)->ipc_fifo[0].flags & IPC_FIFO_EMPTY) != 0) {
                pagetable_t *v1 = bus8->arm7_pagetable;
                arm_t *ptrA = ((pagetable_t *)v1)->cpu;
                io_mirror_t *ptrB = ((arm_t *)ptrA)->io_mirror;
                uint32_t f214 = ((io_mirror_t *)ptrB)->irq.if_pending | 0x20000u;
                ((io_mirror_t *)ptrB)->irq.if_pending = (f214);
                pagetable_t *v1b = bus8->arm7_pagetable;
                arm_t *ptrA2 = ((pagetable_t *)v1b)->cpu;
                uint8_t status = (uint8_t)((arm_t *)ptrA2)->halt_flags;
                if ((status & 6u) == 0) {
                    uint32_t p210 = ((io_mirror_t *)ptrB)->irq.ie;
                    int32_t  p208 = (int32_t)((io_mirror_t *)ptrB)->irq.ime;
                    uint32_t res = (p210 & f214) & (uint32_t)(-p208);
                    ((arm_t *)ptrA2)->irq_pending = (res);
                    if (res != 0) {
                        uint32_t wake = ((arm_t *)ptrA2)->wake_flags;
                        ((arm_t *)ptrA2)->wake_flags = (wake | 2u);
                    }
                }
            }
        }
        param_3 = param_3 & 4u;
        if ((saved & 0x8u) != 0) {
            ipc_fifo_reset(&((bus_t *)param_1)->ipc_fifo[0]);
        }
        uint8_t fifocnt_hi = rd8(&bus8->io_mirror[1].ipc.ipcfifocnt);
        wr8(&bus8->io_mirror[1].ipc.ipcfifocnt, (uint8_t)((fifocnt_hi & 0xfbu) | (param_3 & 0xffu)));
        return;
    }

    case 0x185: {
        uint8_t prev  = rd8(x22 + 385);
        uint8_t byte3 = (uint8_t)(param_3 & 0xffu);
        if ((param_3 & 0x4u) != 0) {
            if ((((bus_t *)param_1)->ipc_fifo[1].flags & IPC_FIFO_EMPTY) == 0) {
                pagetable_t *v1 = bus8->arm7_pagetable;
                arm_t *ptrA = ((pagetable_t *)v1)->cpu;
                io_mirror_t *ptrB = ((arm_t *)ptrA)->io_mirror;
                uint32_t f214 = ((io_mirror_t *)ptrB)->irq.if_pending | 0x40000u;
                ((io_mirror_t *)ptrB)->irq.if_pending = (f214);
                pagetable_t *v1b = bus8->arm7_pagetable;
                arm_t *ptrA2 = ((pagetable_t *)v1b)->cpu;
                uint8_t status = (uint8_t)((arm_t *)ptrA2)->halt_flags;
                if ((status & 6u) == 0) {
                    uint32_t p210 = ((io_mirror_t *)ptrB)->irq.ie;
                    int32_t  p208 = (int32_t)((io_mirror_t *)ptrB)->irq.ime;
                    uint32_t res = (p210 & f214) & (uint32_t)(-p208);
                    ((arm_t *)ptrA2)->irq_pending = (res);
                    if (res != 0) {
                        uint32_t wake = ((arm_t *)ptrA2)->wake_flags;
                        ((arm_t *)ptrA2)->wake_flags = (wake | 2u);
                    }
                }
            }
        }

        uint8_t part = ((byte3 & 0x40u) == 0) ? prev : (uint8_t)(prev & 0x3bu);
        uint8_t merged = (uint8_t)((part & 0x7bu) | (param_3 & 0x84u));
        wr8(x22 + 385, merged);
        return;
    }

    case 0x1a0:
        wr8(x23 + 32, (uint8_t)(param_3 & 0x7fu));
        tail_generic(param_1, param_2, param_3);
        return;

    case 0x1a1: case 0x1a8: case 0x1a9: case 0x1aa: case 0x1ab:
    case 0x1ac: case 0x1ad: case 0x1ae: case 0x1af:
        mirror_write8_arm9(param_1, param_2, param_3);
        return;

    case 0x1a2: {
        cart_t *base   = bus8->cart;
        uint16_t w20 = rd16(x23 + 32);
        param_3 = spi_memory_transfer_byte
                      (&((cart_t *)base)->backup, param_3 & 0xffu);
        if ((w20 & 0x40u) == 0) {
            cart_t *base2 = bus8->cart;
            spi_memory_deselect(&((cart_t *)base2)->backup);
        }
        wr8(x23 + 34, (uint8_t)param_3);
        return;
    }

    case 0x1c2: {
        spi_t *v = bus8->spi;
        uint32_t ret = spi_transfer_byte
                           (v, param_3);
        wr8(x22 + 446, (uint8_t)ret);
        return;
    }

    case 0x208: {
        pagetable_t *v = bus8->arm7_pagetable;
        arm_t *ptr2 = ((pagetable_t *)v)->cpu;
        int simple;
        if ((param_3 & 1u) == 0) {
            simple = 1;
        } else {
            uint32_t f516 = mir7->irq.ime;
            simple = (f516 != 0);
        }
        if (simple) {
            param_3 &= 1u;
            ((arm_t *)ptr2)->irq_pending = (0);
            tail_generic(param_1, param_2, param_3);
            return;
        }
        uint32_t f528 = mir7->irq.if_pending;
        uint32_t f524 = mir7->irq.ie;
        uint32_t res = f524 & f528;
        ((arm_t *)ptr2)->irq_pending = (res);
        param_3 = 1u;
        if (res != 0) {
            uint32_t wake = ((arm_t *)ptr2)->wake_flags;
            ((arm_t *)ptr2)->wake_flags = (wake | 2u);
        }
        tail_generic(param_1, param_2, param_3);
        return;
    }

    case 0x210: {
        uint32_t reg = mir7->irq.ie;
        uint8_t  b0  = (uint8_t)(param_3 & 0xffu);
        uint32_t probe = (uint32_t)b0 & ~reg;
        uint32_t merged = (reg & ~0xffu) | b0;
        mir7->irq.ie = (merged);
        if (probe != 0) arm7_irq_raise(param_1, mir7, merged);
        return;
    }
    case 0x211: {
        uint32_t reg = mir7->irq.ie;
        uint32_t shifted = ((uint32_t)(param_3 & 0xffu)) << 8;
        uint32_t probe = shifted & ~reg;
        uint32_t merged = (reg & ~(0xffu << 8)) | shifted;
        mir7->irq.ie = (merged);
        if (probe != 0) arm7_irq_raise(param_1, mir7, merged);
        return;
    }
    case 0x212: {
        uint32_t reg = mir7->irq.ie;
        uint32_t shifted = ((uint32_t)(param_3 & 0xffu)) << 16;
        uint32_t probe = shifted & ~reg;
        uint32_t merged = (reg & ~(0xffu << 16)) | shifted;
        mir7->irq.ie = (merged);
        if (probe != 0) arm7_irq_raise(param_1, mir7, merged);
        return;
    }
    case 0x213: {
        uint32_t reg = mir7->irq.ie;
        uint32_t top = param_3 << 24;
        uint32_t probe = top & ~reg;
        uint32_t merged = (top & 0xff000000u) | (reg & 0x00ffffffu);
        mir7->irq.ie = (merged);
        if (probe != 0) arm7_irq_raise(param_1, mir7, merged);
        return;
    }

    case 0x214: {
        uint32_t f528 = mir7->irq.if_pending;
        uint32_t mask = ~param_3 | 0xffffff00u;
        mir7->irq.if_pending = (f528 & mask);
        pagetable_t *v = bus8->arm7_pagetable;
        arm_t *ptr2 = ((pagetable_t *)v)->cpu;
        uint32_t pending = ((arm_t *)ptr2)->irq_pending;
        ((arm_t *)ptr2)->irq_pending = (pending & mask);
        return;
    }
    case 0x215: {
        uint32_t f528 = mir7->irq.if_pending;
        uint32_t b0 = param_3 & 0xffu;
        uint32_t mask = ~(b0 << 8);
        mir7->irq.if_pending = (f528 & mask);
        pagetable_t *v = bus8->arm7_pagetable;
        arm_t *ptr2 = ((pagetable_t *)v)->cpu;
        uint32_t pending = ((arm_t *)ptr2)->irq_pending;
        ((arm_t *)ptr2)->irq_pending = (pending & mask);
        return;
    }
    case 0x216: {
        uint32_t f528 = mir7->irq.if_pending;
        uint32_t mask = 0xff20ffffu | ~(param_3 << 16);
        mir7->irq.if_pending = (f528 & mask);
        pagetable_t *v = bus8->arm7_pagetable;
        arm_t *ptr2 = ((pagetable_t *)v)->cpu;
        uint32_t pending = ((arm_t *)ptr2)->irq_pending;
        ((arm_t *)ptr2)->irq_pending = (pending & mask);
        return;
    }
    case 0x217: {
        uint32_t f528 = mir7->irq.if_pending;
        uint32_t mask = ~(param_3 << 24);
        mir7->irq.if_pending = (f528 & mask);
        pagetable_t *v = bus8->arm7_pagetable;
        arm_t *ptr2 = ((pagetable_t *)v)->cpu;
        uint32_t pending = ((arm_t *)ptr2)->irq_pending;
        ((arm_t *)ptr2)->irq_pending = (pending & mask);
        return;
    }

    case 0x301: {
        uint32_t sel = (param_3 >> 6) & 0x3u;
        if (sel == 3u || sel == 2u) {
            pagetable_t *v = bus8->arm7_pagetable;
            arm_t *ptr2 = ((pagetable_t *)v)->cpu;
            uint32_t wake = ((arm_t *)ptr2)->wake_flags;
            ((arm_t *)ptr2)->wake_flags = (wake | 0x10u);
            arm_halt_enter(ptr2);
            if (sel == 3u) {
                arm_t *inner = ((arm_t *)ptr2)->partner;
                ((arm_t *)ptr2)->halt_flags = (2u);
                uint32_t iv = ((arm_t *)inner)->halt_flags;
                ((arm_t *)inner)->halt_flags = (iv | 2u);
            }
            tail_generic(param_1, param_2, param_3);
            return;
        }
        tail_generic(param_1, param_2, param_3);
        return;
    }

    default:
        tail_default(param_1, param_2, param_3);
        return;
    }
}




static uint8_t *io(uint8_t *c)
{
    return (uint8_t *)((bus_t *)c)->arm7_pagetable->cpu;
}

void io7_write16(void *ctx, uint32_t reg, uint32_t val)
{
    uint8_t *c = (uint8_t *)ctx;
    io_mirror_t *mir7w16 = &((bus_t *)ctx)->io_mirror[1];
    uint32_t v16 = val & 0xffff;

    if (reg >= 0x400 && reg <= 0x4fe && !(reg & 1)) {
        spu_t *spu = ((bus_t *)c)->spu;
        uint32_t sel = reg & 0xfffff00f;
        uint32_t ch  = (reg >> 4) & 0xf;
        spu_channel_t *e = &spu->channels[ch];
        if (sel == 0 || sel == 8) {
             e->recalc_flags |= (sel == 0) ? 2u : 1u;
        } else if (sel == 2) {
            wr16((uint8_t *)&((bus_t *)c)->io_mirror[1] + reg, val);
            if ((int16_t)val < 0) spu_channel_start(spu, ch);
            else                  e->active = 0;
            e->recalc_flags |= 2;
            return;
        }
        wr16((uint8_t *)&((bus_t *)c)->io_mirror[1] + reg, val);
        return;
    }

    if (reg >= 0x188 && reg <= 0x18b) return;

    if (reg == 0xba || reg == 0xc6 || reg == 0xd2 || reg == 0xde) {
        uint32_t i = (reg - 0xba) / 12;
        dma_regs_t *r = &mir7w16->dma[i];
        dma_channel_t *p = &((bus_t *)ctx)->dma[1].channels[i];
        uint32_t cnt = (r->cnt & 0xffffu) | (v16 << 16);
        r->cnt = (r->cnt & 0xffffu) | ((uint32_t)(uint16_t)val << 16);

        if ((v16 & 0x8000) && !(p->cnt & 0x80000000u)) {
            uint32_t when = (v16 >> 12) & 3;
            dma_regs_t *o = p->regs;
            p->start_mode = (uint8_t)when;
            p->src = o->sad;
            p->dst = o->dad;
            p->cnt = cnt;
            if (when == 0)
                dma_channel_run_transfer(&((bus_t *)ctx)->dma[1], p);
            return;
        }
        p->cnt = cnt;
        return;
    }

    if (reg >= 0x100 && reg <= 0x10e && !(reg & 1)) {
        uint32_t i = (reg - 0x100) / 4;
        uint8_t *o = io(c);
        nds_timer_t *t = &((arm_t *)o)->timers[i];
        if ((reg & 2) == 0) {
            t->reload = (uint16_t)val;
            wr16((uint8_t *)&((bus_t *)c)->io_mirror[1] + reg, val);
            return;
        }

        uint32_t pre = val & 3;
        uint32_t shift = pre ? (pre * 2 + 5) : 1;
        uint32_t count = (0x10000u - t->reload) << shift;
        t->prescaler_shift = (uint8_t)shift;
        t->period_cycles = count;

        uint8_t *ev = (uint8_t *)((bus_t *)c)->machine;
        if ((val & 0x80) && !(v16 & 4) && !(t->control & 0x80)) {

            int64_t base = (int32_t)((arm_t *)o)->cycle_mark;
            uint32_t live = t->scheduled;
            uint64_t begin = rd64(ev + 8);
            uint32_t len = rd32(ev + 16);
            uint64_t now = (begin + len) - (uint64_t)base;
            uint32_t d = (uint32_t)(count - (uint32_t)begin) + (uint32_t)now;
            uint32_t deadline = (len > d) ? len : d;
            t->start_cycles = now;

            if (live) sched_deadline_remove(&((nds_t *)ev)->sched, SCHED_DEADLINE_TIMER_BASE + i);
            sched_deadline_insert(&((nds_t *)ev)->sched, deadline, SCHED_DEADLINE_TIMER_BASE + i);
            t->scheduled = 1;
        } else if (!(val & 0x80) && t->scheduled) {
            sched_deadline_remove(&((nds_t *)ev)->sched, SCHED_DEADLINE_TIMER_BASE + i);
            t->scheduled = 0;
        }
        t->control = (uint16_t)val;
        wr16((uint8_t *)&((bus_t *)c)->io_mirror[1] + reg, val);
        return;
    }

    if (reg == 0x1c0) {

        spi_control_write(((bus_t *)c)->spi, val);
        wr16((uint8_t *)&((bus_t *)c)->io_mirror[1] + reg, val);
        return;
    }

    if (reg == 0x208) {
        uint8_t *o = io(c);
        if ((val & 1) && mir7w16->irq.ime == 0) {
            uint32_t p = mir7w16->irq.ie & mir7w16->irq.if_pending;
            ((arm_t *)o)->irq_pending = (p);
            if (p) ((arm_t *)o)->wake_flags = (((arm_t *)o)->wake_flags | 2);
            wr16((uint8_t *)&((bus_t *)c)->io_mirror[1] + reg, 1);
            return;
        }
        ((arm_t *)o)->irq_pending = (0);
        wr16((uint8_t *)&((bus_t *)c)->io_mirror[1] + reg, val & 1);
        return;
    }

    if (reg == 0x210 || reg == 0x212) {
        uint32_t ie = mir7w16->irq.ie, updated, has;
        if (reg == 0x210) {
            has = (v16 & ~ie) != 0;
            updated = (ie & 0xffff0000u) | v16;
        } else {
            uint32_t height = val << 16;
            has = (height & ~ie) != 0;
            updated = height | (ie & 0xffffu);
        }
        mir7w16->irq.ie = (updated);
        if (!has) return;
        uint8_t *o = io(c);
        uint32_t p = (mir7w16->irq.if_pending & updated) & (uint32_t)(-(int32_t)mir7w16->irq.ime);
        ((arm_t *)o)->irq_pending = (p);
        if (p) ((arm_t *)o)->wake_flags = (((arm_t *)o)->wake_flags | 2);
        return;
    }

    if (reg == 0x214 || reg == 0x216) {
        uint32_t plus = (reg == 0x214) ? (~val | 0xffff0000u)
                                      : (0x20ffffu | ~(val << 16));
        uint32_t f = mir7w16->irq.if_pending & plus;
        mir7w16->irq.if_pending = (f);
        uint8_t *o = io(c);
        ((arm_t *)o)->irq_pending = (((arm_t *)o)->irq_pending & plus);
        return;
    }

    if (reg < 0x800000) {

        io7_write8(ctx, reg, val);
        io7_write8(ctx, reg + 1, (val >> 8) & 0xff);
        return;
    }
    if ((reg & 0xc000) == 0x4000) {
        wr16(((bus_t *)c)->arm7_io_scratch + (reg & 0x3fff), val);
        return;
    }
    if ((reg & 0x3fff) == 0x158 && (val & 0x3000) == 0x1000)
        ((bus_t *)c)->wifi_regs[0x400 + (v16 & 0x7f)] = ((bus_t *)c)->wifi_regs[0x15a];
    wr16(((bus_t *)c)->wifi_regs + (reg & 0x3fff), val);
}

#define CHANNEL    0xfd368
#define CHANNEL_STRIDE  40
#define C_PTR    0
#define C_SRC   8
#define C_CTRL   16
#define C_MODE   20
#define DMA_ENGINE    0xfd348
#define P_PEND   8456
#define R_ENV    336
extern void io7_write16_6(void *ctx, uint32_t reg, uint32_t val) __asm__("io7_write16");



void io7_write32(uint8_t *ctx, uint32_t dir, uint32_t val) {

    bus_t *busw = (bus_t *)ctx;
    io_mirror_t *mir7w32 = &((bus_t *)ctx)->io_mirror[1];
    uint8_t *cnt7 = (uint8_t *)&mir7w32->ipc.ipcfifocnt;

    if ((int32_t)dir > 0x1a3) {

        if ((int32_t)dir <= 0x20f) {
            if (dir == 0x1a4) {
                if ((int32_t)val < 0)
                    { cart_command_execute(((bus_t *)ctx)->cart, val); return; }
                goto window;
            }
            if (dir != 0x208) goto backup;

            uint8_t *irq = (uint8_t *)busw->arm7_pagetable->cpu;
            if ((val & 1) && mir7w32->irq.ime == 0) {
                uint32_t p = mir7w32->irq.ie & mir7w32->irq.if_pending;
                wr32(irq + P_PEND, p);
                if (p != 0) ((arm_t *)irq)->wake_flags |= 2u;
                val = 1;
            } else {
                val &= 1;
                wr32(irq + P_PEND, 0);
            }
            goto window;
        }

        if (dir == 0x210) {
            uint32_t before = mir7w32->irq.ie;
            mir7w32->irq.ie = (val);
            if ((val & ~before) == 0) return;
            uint8_t *irq = (uint8_t *)busw->arm7_pagetable->cpu;
            uint32_t p = mir7w32->irq.if_pending & val & (0u - mir7w32->irq.ime);
            wr32(irq + P_PEND, p);
            if (p == 0) return;
            ((arm_t *)irq)->wake_flags |= 2u;
            return;
        }

        if (dir == 0x214) {
            uint32_t m = (~val) | 0x200000u;
            mir7w32->irq.if_pending = (mir7w32->irq.if_pending & m);
            uint8_t *irq = (uint8_t *)busw->arm7_pagetable->cpu;
            wr32(irq + P_PEND, rd32(irq + P_PEND) & m);
            return;
        }
        goto backup;
    }

    if (dir - 0xb8u <= 0x24u) {
        uint32_t channel;
        switch (dir) {
        case 0xb8: channel = 0; break;
        case 0xc4: channel = 1; break;
        case 0xd0: channel = 2; break;
        case 0xdc: channel = 3; break;
        default:   goto backup;
        }
        dma_channel_t *c = &((bus_t *)ctx)->dma[1].channels[channel];
        mir7w32->dma[channel].cnt = val;

        if ((int32_t)val < 0 && (int32_t)c->cnt >= 0) {
            uint32_t mode = (val >> 28) & 3u;
            dma_regs_t *p = c->regs;
            c->start_mode = (uint8_t)mode;
            c->src = p->sad;
            uint32_t seg = p->dad;
            c->cnt = val;
            c->dst = seg;
            if (mode == 0)
                dma_channel_run_transfer(&((bus_t *)ctx)->dma[1], c);
            return;
        }
        c->cnt = val;
        return;
    }

    if (dir != 0x188) goto backup;

    if ((int8_t)cnt7[1] >= 0) return;
    ipc_fifo_t *fifo0 = &((bus_t *)ctx)->ipc_fifo[0];
    if (fifo0->flags & IPC_FIFO_FULL) { cnt7[1] |= 0x40; return; }

    {
        uint8_t *bit = (uint8_t *)&busw->io_mirror[0].ipc.ipcfifocnt + 1;
        ipc_fifo_push(fifo0, val);

        uint32_t a = bit[0] & ~1u;
        bit[0] = (uint8_t)a;
        uint32_t b = cnt7[0] & ~1u;
        cnt7[0] = (uint8_t)b;

        uint32_t f = fifo0->flags;
        if (f & 2) { b |= 2u; a |= 2u; cnt7[0] = (uint8_t)b; bit[0] = (uint8_t)a; }
        if (f & 1) return;
        if (!(a & 4)) return;

        uint8_t *irq = (uint8_t *)busw->arm7_pagetable->cpu;
        if (((arm_t *)irq)->is_arm9 == 1)
            ((arm_t *)irq)->wake_flags |= 4u;

        uint8_t *m = (uint8_t *)busw->arm9_pagetable->cpu;
        uint8_t *ie = (uint8_t *)((arm_t *)m)->io_mirror;
        uint32_t v = ((io_mirror_t *)ie)->irq.if_pending | 0x40000u;
        ((io_mirror_t *)ie)->irq.if_pending = (v);

        uint8_t *m2 = (uint8_t *)busw->arm9_pagetable->cpu;
        if (((arm_t *)m2)->halt_flags & 6) return;
        uint32_t r = (((io_mirror_t *)ie)->irq.ie & v) & (0u - ((io_mirror_t *)ie)->irq.ime);
        wr32(m2 + P_PEND, r);
        return;
    }

window:
    wr32((uint8_t *)busw->io_mirror + (dir & 0x7fffu), val);
    return;

backup:
    if ((dir >> 23) != 0) return;
    io7_write16(ctx, dir, val);
    io7_write16(ctx, dir + 2u, val >> 16);
}
#undef CHANNEL
#undef CHANNEL_STRIDE
#undef C_PTR
#undef C_SRC
#undef C_CTRL
#undef C_MODE
#undef DMA_ENGINE
#undef P_PEND
#undef R_ENV

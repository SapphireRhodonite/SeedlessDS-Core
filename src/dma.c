#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"

uint64_t dma_channel_run_transfer(dma_t *machine, dma_channel_t *channel);
#define CPU(p) ((arm_t *)(p))
#include "dma.h"
#include "core/sched.h"
#include "core_internals.h"
#include "mem_access.h"

void dma_dispatch_trigger(dma_t *dma, uint32_t trigger)
{

    if ((dma->channels[0].cnt & 0x80000000u) != 0 &&
        (uint32_t)dma->channels[0].start_mode == trigger)
        (void)dma_channel_run_transfer(dma, &dma->channels[0]);

    if ((dma->channels[1].cnt & 0x80000000u) != 0 &&
        (uint32_t)dma->channels[1].start_mode == trigger)
        (void)dma_channel_run_transfer(dma, &dma->channels[1]);

    if ((dma->channels[2].cnt & 0x80000000u) != 0 &&
        (uint32_t)dma->channels[2].start_mode == trigger)
        (void)dma_channel_run_transfer(dma, &dma->channels[2]);

    if ((dma->channels[3].cnt & 0x80000000u) != 0 &&
        (uint32_t)dma->channels[3].start_mode == trigger)
        (void)dma_channel_run_transfer(dma, &dma->channels[3]);
}

void dma_transfer_end_event(void *unused, void *argument) {
    (void)unused;
    unsigned char *channel = argument;

    uint32_t state = *(uint32_t *)(channel + 32);
    if (!(state & (1u << 25))) {
        unsigned char *p = *(unsigned char **)(channel + 16);
        state &= 0x7fffffffu;
        *(uint32_t *)(channel + 32) = state;
        *(uint32_t *)(p + 8) = state;
    }

    if (state & (1u << 30)) {
        unsigned char *node = *(unsigned char **)(channel + 8);
        unsigned char *reg = (unsigned char *)((arm_t *)node)->io_mirror;
        uint32_t bit = 0x100u << (*(uint8_t *)(channel + 37) & 31);
        uint32_t pend = bit | ((io_mirror_t *)reg)->irq.if_pending;
        ((io_mirror_t *)reg)->irq.if_pending = pend;

        unsigned char *n2 = *(unsigned char **)(channel + 8);
        if (!((uint8_t)((arm_t *)n2)->halt_flags & 6)) {
            uint32_t enabled = ((io_mirror_t *)reg)->irq.ie;
            uint32_t tap = ((io_mirror_t *)reg)->irq.ime;
            ((arm_t *)n2)->irq_pending = (enabled & pend) & (uint32_t)(-(int32_t)tap);
        }
    }

    *(uint8_t *)(channel + 38) = 0;
}

extern uint32_t mirror_dirty_bitmap_test(const uint32_t *coarse, const uint32_t *fine,
                                   uint32_t address, uint32_t len);







typedef uint32_t       (*fn_read)(void *arena, uint32_t dir);
typedef void           (*fn_write)(void *arena, uint32_t dir, uint32_t value);
typedef unsigned char *(*fn_ptr)(void *arena, uint32_t dir);
typedef const uint32_t *(*fn_map)(void *arena, unsigned char *entry,
                                   uint32_t dir);

uint64_t dma_channel_run_transfer(dma_t *machine, dma_channel_t *channel)
{

    unsigned char *arena = (unsigned char *)machine->bus;
    unsigned char *nds   = (unsigned char *)channel->cpu;
    unsigned char *clock = (unsigned char *)machine->bus->machine;

    uint32_t field_d = channel->dst;
    uint32_t cnt     = channel->cnt;
    uint32_t field_o = channel->src;

    int32_t  pending = (int32_t)((arm_t *)nds)->cycle_mark;
    uint32_t selt      = ((arm_t *)nds)->is_arm9;

    if ((field_d >> 28) != 0) field_d = 0;
    if ((field_o >> 28) != 0) field_o = 0;

    uint32_t units = cnt & 0x1fffff;
    if (units == 0) units = 0x200000;
    uint32_t width32 = (cnt >> 26) & 1;

    uint32_t reg_d = field_d >> 24;
    uint32_t reg_o = field_o >> 24;

    const int32_t *t_ig = bus_cost_same_region;
    const int32_t *t_di = bus_cost_cross_region;
    uint64_t row = (uint64_t)selt * 32u + (uint64_t)width32 * 16u;
    int32_t  cost;
    uint32_t product;
    uint64_t mark;
    uint32_t mode, org, dst, width, shift, result;

    if (reg_o == reg_d) {
        cost = t_ig[row + reg_o];
    } else {
        cost = t_di[row + reg_d];
        if (reg_d != 6) cost = t_di[row + reg_o] + cost;
    }

    product = (uint32_t)cost * units * ((nds_t *)clock)->runtime.dma_cost_scale;
    mark = (((nds_t *)clock)->sched.cycles + (uint64_t)((nds_t *)clock)->sched.slice_cycles)
            - (uint64_t)(int64_t)pending + (uint64_t)product;

    wr64(channel, mark);
    if (((nds_t *)clock)->runtime.dma_sets_cycle_mark != 0)
        ((arm_t *)nds)->cycle_mark = ((uint32_t)pending - product);

    mode = (cnt >> 21) & 0xf;

    if ((cnt & (1u << 26)) == 0) {
        width = 2; shift = 1;
        org = field_o & 0xfffffffeu;
        dst = field_d & 0xfffffffeu;
    } else {
        if (field_d == 0x4000400u && selt == 1) {
            dma_transfer_copy_via_pagetable(machine, channel);
            return 0;
        }
        width = 4; shift = 2;
        org = field_o & 0xfffffffcu;
        dst = field_d & 0xfffffffcu;
    }

    result = 0;

    if (mode <= 0xb) {
        int step_o = (mode >> 2) == 1 ? -1 : ((mode >> 2) == 2 ? 0 : 1);
        int step_d = (mode & 3)  == 1 ? -1 : ((mode & 3)  == 2 ? 0 : 1);
        uint32_t left = units;

        while (left != 0) {

            unsigned char *table = (unsigned char *)rd_ptr((unsigned char *)machine + 8);
            unsigned char *ent_o = table + (uint64_t)(org >> 23) * 96;
            unsigned char *ent_d = table + (uint64_t)(dst >> 23) * 96;
            uint32_t run = left;
            uint32_t bytes;
            uint32_t off_o = 0;
            uint32_t flags = 0;
            unsigned char *po = 0, *pd = 0;
            fn_read  read_fn = 0;
            fn_write  write_fn = 0;
            const uint32_t *coarse, *fine;
            uint8_t class;

            if (step_o > 0) {
                uint32_t lim = rd32(ent_o);
                off_o = lim & org;
                if (off_o + run * width > lim)
                    run = (lim - off_o + 1) >> shift;
            } else if (step_o < 0) {
                uint32_t lim = rd32(ent_o);
                off_o = lim & org;
                if ((uint32_t)(off_o - run * width) > lim)
                    run = (off_o >> shift) + 1;
            }

            bytes = run * width;
            if (step_d > 0) {
                uint32_t lim = rd32(ent_d);
                uint32_t off = lim & dst;
                if (off + bytes > lim)
                    run = (lim - off + 1) >> shift;
            } else if (step_d < 0) {
                uint32_t lim = rd32(ent_d);
                uint32_t off = lim & dst;
                if ((uint32_t)(off - bytes) > lim)
                    run = (off >> shift) + 1;
            }

            class = rd8(ent_o + 0x58);
            if (class == 2) {
                read_fn = (fn_read)rd_ptr(ent_o + (width == 2 ? 0x10 : 0x18));
                flags = 1;
            } else if (class == 1) {
                po = ((fn_ptr)rd_ptr(ent_o + 8))(arena, org);
            } else if (class == 0) {

                uint32_t off = (step_o != 0) ? off_o : (rd32(ent_o) & org);
                po = (unsigned char *)rd_ptr(ent_o + 8) + off;
            } else {
                po = ((bus_t *)arena)->blank_page;
            }

            class = rd8(ent_d + 0x59);
            if (class == 2) {
                write_fn = (fn_write)rd_ptr(ent_d + (width == 2 ? 0x28 : 0x30));
                flags |= 2;
            } else if (class == 1) {
                pd = ((fn_ptr)rd_ptr(ent_d + 0x20))(arena, dst);
            } else if (class == 0) {

                pd = (unsigned char *)rd_ptr(ent_d + 0x20) + (rd32(ent_d) & dst);
            } else {
                pd = ((bus_t *)arena)->scratch_page;
            }

            coarse = ((fn_map)rd_ptr(ent_d + 0x48))(arena, ent_d, dst);
            fine   = ((fn_map)rd_ptr(ent_d + 0x50))(arena, ent_d, dst);

            left -= run;
            bytes = run * width;

            if (coarse != 0)
                result |= mirror_dirty_bitmap_test(coarse, fine, dst, bytes);

            {
                uint32_t i = run;
                uint32_t dir_o = org, dir_d = dst;
                do {
                    uint32_t v;
                    if (flags & 1) {
                        v = read_fn(arena, rd32(ent_o) & dir_o);
                    } else {
                        v = (width == 2) ? (uint32_t)rd16(po) : rd32(po);
                        po += step_o * (int)width;
                    }
                    if (flags & 2) {
                        write_fn(arena, rd32(ent_d) & dir_d, v);
                    } else {
                        if (width == 2) wr16(pd, (uint16_t)v);
                        else            wr32(pd, v);
                        pd += step_d * (int)width;
                    }
                    dir_o += (uint32_t)(step_o * (int)width);
                    dir_d += (uint32_t)(step_d * (int)width);
                } while (--i);
            }

            org += (uint32_t)(step_o * (int)bytes);
            dst += (uint32_t)(step_d * (int)bytes);
        }
    }

    if (product != 0 && ((arm_t *)nds)->is_arm9 == 1) {
        uint32_t deadline;

        if (channel->started != 0)
            sched_deadline_remove(&((nds_t *)clock)->sched,
                               (uint32_t)channel->index + SCHED_DEADLINE_DMA_BASE);
        channel->started = 1;

        deadline = (uint32_t)mark - (uint32_t)((nds_t *)clock)->sched.cycles;
        sched_deadline_insert(&((nds_t *)clock)->sched, deadline,
                           (uint32_t)channel->index + SCHED_DEADLINE_DMA_BASE);

        if (rd32(clock + 0x10) > deadline)
            ((arm_t *)nds)->wake_flags = (((arm_t *)nds)->wake_flags | 4);

    } else {

        if ((cnt & (1u << 25)) == 0) {
            dma_regs_t *mirror = channel->regs;
            cnt &= 0x7fffffffu;
            channel->cnt = cnt;
            mirror->cnt = cnt;
        }
        if (cnt & (1u << 30)) {
            unsigned char *n2  = (unsigned char *)channel->cpu;
            unsigned char *blk = (unsigned char *)((arm_t *)n2)->io_mirror;
            uint32_t v = (0x100u << (channel->index & 31))
                         | ((io_mirror_t *)blk)->irq.if_pending;
            unsigned char *n3;
            uint32_t live;

            ((io_mirror_t *)blk)->irq.if_pending = (v);
            n3 = (unsigned char *)channel->cpu;
            if (((uint8_t)((arm_t *)n3)->halt_flags & 6) == 0) {
                uint32_t a = ((io_mirror_t *)blk)->irq.ie;
                uint32_t b = ((io_mirror_t *)blk)->irq.ime;
                live = (a & v) & (uint32_t)(0u - b);
                ((arm_t *)n3)->irq_pending = (live);
            } else {
                live = ((arm_t *)n3)->irq_pending;
            }
            if (live != 0)
                ((arm_t *)n3)->wake_flags = (((arm_t *)n3)->wake_flags | 2);
        }
    }

    if ((~cnt) & 0x600000u)
        channel->dst = dst;
    channel->src = org;
    if (result != 0) {
        unsigned char *n = (unsigned char *)channel->cpu;
        ((arm_t *)n)->wake_flags = (((arm_t *)n)->wake_flags | 1);
    }
    return result;
}

typedef uint8_t *(*recon_entry_fn_t)(bus_t *core, uint32_t pos);
#define ENTRY_STRIDE 96u

void dma_transfer_copy_via_pagetable(dma_t *machine, dma_channel_t *channel) {

    uint8_t *p2 = (uint8_t *)channel;

    bus_t *core = machine->bus;

    struct gpu3d *subA = core->gpu3d;

    nds_t *table_ptr = subA->machine;
    uint32_t w10_orig = ((dma_channel_t *)p2)->cnt;
    uint32_t w20_pos  = ((dma_channel_t *)p2)->src;

    uint32_t w26_ivar4 = table_ptr->runtime.dma_cost_low;

    nds_t **machine_slot = &core->machine;

    uint32_t uvar2 = w10_orig & 0x1fffffu;
    uint32_t slot_uvar2  = uvar2;
    uint32_t slot_uvar10 = w10_orig;
    nds_t **slot_ptr     = machine_slot;

    uint32_t w28_acc = 0;
    uint32_t w20 = w20_pos;

    if (uvar2 != 0) {
        uint32_t w24, w25, w27, take;
        bus_region_t *entry_base;
        uint8_t *x1_addr;
        uint32_t entry_count;
        uint8_t  entry_flag;

        if (w26_ivar4 != 0) {

            w27 = ENTRY_STRIDE;
            w25 = uvar2;
            for (;;) {
                for (;;) {

                    entry_base  = (bus_region_t *)((uint8_t *)machine->region + (uint64_t)(w20 >> 23) * ENTRY_STRIDE);
                    entry_count = entry_base->mask;
                    entry_flag  = entry_base->read_kind;
                    w24 = entry_count + 1u;

                    if (entry_flag == 0) {

                        uint8_t *dataptr = entry_base->read_memory;
                        uint32_t idxmask = entry_count & w20;
                        x1_addr = dataptr + idxmask;
                        break;
                    }
                    if (entry_flag == 1) {

                        recon_entry_fn_t fnptr = (recon_entry_fn_t)entry_base->read_translate;
                        x1_addr = fnptr(core, w20);
                        break;
                    }

                    take = (w24 > w25) ? w25 : w24;
                    w20 += take;
                    w25 -= take;
                    if (w25 == 0) goto A_done_31678;

                }

                take = (w24 > w25) ? w25 : w24;
                w20 += take;
                w25 -= take;

                if (x1_addr != NULL) {

                    uint32_t r = gpu3d_gxfifo_decode_commands_with_cycles(subA, (uint32_t *)x1_addr, take);
                    w28_acc = w28_acc + r * w26_ivar4;
                }
                if (w25 == 0) goto A_done_31678;

            }
        } else {

            w25 = ENTRY_STRIDE;
            w27 = uvar2;
            for (;;) {
                for (;;) {
                    entry_base  = (bus_region_t *)((uint8_t *)machine->region + (uint64_t)(w20 >> 23) * ENTRY_STRIDE);
                    entry_count = entry_base->mask;
                    entry_flag  = entry_base->read_kind;
                    w24 = entry_count + 1u;

                    if (entry_flag == 0) {
                        uint8_t *dataptr = entry_base->read_memory;
                        uint32_t idxmask = entry_count & w20;
                        x1_addr = dataptr + idxmask;
                        break;
                    }
                    if (entry_flag == 1) {
                        recon_entry_fn_t fnptr = (recon_entry_fn_t)entry_base->read_translate;
                        x1_addr = fnptr(core, w20);
                        break;
                    }
                    take = (w24 > w27) ? w27 : w24;
                    w20 += take;
                    w27 -= take;
                    if (w27 == 0) { w28_acc = 0; goto A_done_31678; }
                }

                take = (w24 > w27) ? w27 : w24;
                w20 += take;
                w27 -= take;

                if (x1_addr != NULL) {

                    gpu3d_gxfifo_decode_commands(subA, (uint32_t *)x1_addr, take);
                }
                if (w27 == 0) { w28_acc = 0; goto A_done_31678; }

            }
        }
    }

A_done_31678:

    {
        arm_t *p2_1  = ((dma_channel_t *)p2)->cpu;
        nds_t *sub2  = *slot_ptr;
        uint32_t w13   = slot_uvar10;
        int64_t  x11   = (int64_t)p2_1->cycle_mark;
        uint64_t cycles = sub2->sched.cycles;
        uint32_t lim32    = sub2->sched.slice_cycles;
        uint32_t w10_reload = slot_uvar2;

        uint64_t x12 = cycles + (uint64_t)w28_acc;
        uint64_t x9_64 = x12 + (uint64_t)lim32;
        uint64_t x23_val = x9_64 - (uint64_t)x11;

        int tst_no_eq = (((~w13) & 0x600000u) != 0u);
        if (tst_no_eq) {
            uint32_t w9 = w20 + w10_reload;
            ((dma_channel_t *)p2)->dst = (w9);
        }
        w20 = (uint32_t)x23_val - (uint32_t)cycles;

        if (w26_ivar4 != 0) {

            if (w10_reload <= 0x103u && ((w13 & (1u << 25)) == 0u)) {
                dma_regs_t *p2_2 = ((dma_channel_t *)p2)->regs;
                uint32_t t = w13 & 0x7fffffffu;
                ((dma_channel_t *)p2)->cnt = (t);
                wr32(&p2_2->cnt, t);
            }

            uint8_t started = (uint8_t)((dma_channel_t *)p2)->started;
            if (started != 0) {
                uint8_t b37 = (uint8_t)((dma_channel_t *)p2)->index;
                sched_deadline_remove(&sub2->sched, (uint32_t)b37 + SCHED_DEADLINE_DMA_BASE);
            }
            {
                uint8_t b37 = (uint8_t)((dma_channel_t *)p2)->index;
                ((dma_channel_t *)p2)->started = 1;
                ((dma_channel_t *)p2)->deadline_cycles = (x23_val);
                sched_deadline_insert(&sub2->sched, w20, (uint32_t)b37 + SCHED_DEADLINE_DMA_BASE);
            }

            uint32_t lim2 = sub2->sched.slice_cycles;
            if (lim2 <= w20) {
                goto A_queue_31798;
            }
            {
                uint32_t f = p2_1->wake_flags;
                f |= 4u;
                p2_1->wake_flags = (f);
            }
            {
                uint32_t lim3 = sub2->sched.slice_cycles;
                if (lim3 > w20) {

                    uint32_t f2 = p2_1->wake_flags;
                    f2 |= 4u;
                    p2_1->wake_flags = (f2);
                }
            }
            goto A_epilogue;
        } else {

            if ((w13 & (1u << 25)) == 0u) {
                dma_regs_t *p2_2 = ((dma_channel_t *)p2)->regs;
                w13 = w13 & 0x7fffffffu;
                ((dma_channel_t *)p2)->cnt = (w13);
                wr32(&p2_2->cnt, w13);
            }

            if ((w13 & (1u << 30)) != 0u) {
                io_mirror_t *chreg = p2_1->io_mirror;
                uint8_t  b37   = (uint8_t)((dma_channel_t *)p2)->index;
                uint32_t v532  = chreg->irq.if_pending;
                uint32_t shifted = (0x100u << (b37 & 0x1fu));
                v532 |= shifted;
                chreg->irq.if_pending = (v532);

                arm_t *p2_1_reload = ((dma_channel_t *)p2)->cpu;
                uint8_t byte2110 = (uint8_t)p2_1_reload->halt_flags;
                if ((byte2110 & 0x6u) == 0u) {

                    uint32_t v528 = chreg->irq.ie;
                    uint32_t v520 = chreg->irq.ime;
                    uint32_t masked = v528 & v532;
                    uint32_t negv520 = (uint32_t)(-(int32_t)v520);
                    uint32_t res = masked & negv520;
                    p2_1_reload->irq_pending = (res);
                    if (res == 0) goto A_queue_31798;

                } else {
                    uint32_t v8456 = p2_1_reload->irq_pending;
                    if (v8456 == 0) goto A_queue_31798;
                }
                {
                    uint32_t f = p2_1_reload->wake_flags;
                    f |= 2u;
                    p2_1_reload->wake_flags = (f);
                }
            }

        }

A_queue_31798:
        {
            uint32_t limf = sub2->sched.slice_cycles;
            if (limf > w20) {
                uint32_t f = p2_1->wake_flags;
                f |= 4u;
                p2_1->wake_flags = (f);
            }
        }
    }

A_epilogue:
    return;
}
#undef ENTRY_STRIDE



void dma_channel_finish_transfer(void *const *pair, dma_channel_t *channel, cart_t *ctx) {

    uint8_t *core = (uint8_t *)channel->cpu;
    uint8_t *audio  = (uint8_t *)ctx->machine;
    uint8_t *alt = (uint8_t *)ctx->rom->data;
    uint32_t count = channel->dst;
    uint32_t ctrl   = channel->cnt;
    uint32_t pos    = ctx->read_address;
    uint32_t size    = ctx->words_remaining;
    uint32_t mode   = ctx->transfer_mode;
    uint32_t limit = ((nds_t *)audio)->sched.slice_cycles;
    uint64_t cycles = ((nds_t *)audio)->sched.cycles;
    int64_t  adjust = (int32_t)CPU(core)->cycle_mark;

    uint32_t base_cnt = ((count >> 28) != 0) ? 0u : count;
    uint32_t bytes    = size << 2;

    uint8_t *dest = (mode == 8)
        ? ctx->secure_area + (uint64_t)(uint32_t)(pos - 0x4000u)
        : alt + (uint64_t)pos;

    int32_t res = mirror_write_block_via_regions(pair[1], pair[0], base_cnt, dest, bytes);

    ctx->read_address = ctx->read_address + bytes;
    if (!(ctrl & (1u << 25))) {
        dma_regs_t *mirror = channel->regs;
        ctrl &= 0x7fffffffu;
        channel->cnt = ctrl;
        mirror->cnt = ctrl;
    }
    if (~ctrl & 0x600000u)
        channel->dst = base_cnt + bytes;

    ctx->words_remaining = 0;
    uint64_t clock = cycles + (uint64_t)(uint32_t)((size * 5u) << 3)
                   + (uint64_t)limit - (uint64_t)adjust;
    ctx->transfer_deadline_cycles = clock;
    io_mirror_t *regs = ctx->mirror;
    regs->cart.romctrl &= 0x7f7fffffu;
    uint32_t counter = (uint32_t)((nds_t *)audio)->sched.cycles;
    if (regs->cart.auxspicnt & 0x4000) {
        if (((nds_t *)audio)->runtime.cart_cycles_per_word == 0) {
            uint8_t *nu = (uint8_t *)channel->cpu;
            uint8_t *ie = (uint8_t *)CPU(nu)->io_mirror;
            uint32_t v = ((io_mirror_t *)ie)->irq.if_pending | 0x80000u;
            ((io_mirror_t *)ie)->irq.if_pending = (v);

            nu = (uint8_t *)channel->cpu;
            uint32_t pend;
            if ((CPU(nu)->halt_flags & 6) == 0) {

                pend = ((io_mirror_t *)ie)->irq.ie & v & (0u - ((io_mirror_t *)ie)->irq.ime);
                CPU(nu)->irq_pending = pend;
            } else {
                pend = CPU(nu)->irq_pending;
            }
            if (pend != 0)
                CPU(nu)->wake_flags = CPU(nu)->wake_flags | 2u;
        } else {
            uint32_t deadline = (uint32_t)clock - counter;

            if (ctx->irq_pending != 0)
                sched_deadline_remove(&((nds_t *)audio)->sched, SCHED_DEADLINE_CART_TRANSFER);
            ctx->irq_pending = 1;
            sched_deadline_insert(&((nds_t *)audio)->sched, deadline, SCHED_DEADLINE_CART_TRANSFER);

            if (((nds_t *)audio)->sched.slice_cycles > deadline)
                CPU(core)->wake_flags = CPU(core)->wake_flags | 4u;
        }
    }

    if (res != 0) {
        uint8_t *nu = (uint8_t *)channel->cpu;
        CPU(nu)->wake_flags = CPU(nu)->wake_flags | 1u;
    }
}
#undef A_TEMPO

#define E_STEP      96u
#define E_MASK   0
#define E_DATA     8
#define E_FLAG   88
#define B_CORE    0
#define B_TABLE     8
#define I_MASK   0x208
#define I_ENABLE     0x210
#define I_IF        0x214
#define DATA_FLAG      0
#define HANDLER_FLAG  1
#define CTRL_REPEAT   (1u << 25)
#define CTRL_IRQ       (1u << 30)
#define CTRL_ENABLE     (1u << 31)
typedef void *(*fn_entry)(bus_t *core, uint32_t dir);





void *dma_main_memory_display_step(uint8_t *bus, dma_channel_t *channel, uint32_t line) {

    uint32_t origin = channel->src;
    uint8_t *table  = (uint8_t *)rd_ptr(bus + B_TABLE);
    uint32_t ctrl   = channel->cnt;

    uint32_t raw = origin + (line << 9);

    uint32_t dir = ((raw >> 28) != 0) ? 0u : raw;

    uint32_t page = dir >> 23;
    uint8_t *entry = table + (uint64_t)page * E_STEP;
    uint8_t flag = entry[E_FLAG];

    void *res;
    if (flag == HANDLER_FLAG) {

        bus_t *core_bus = (bus_t *)rd_ptr(bus + B_CORE);
        fn_entry fn = (fn_entry)rd_ptr(entry + E_DATA);
        res = fn(core_bus, dir);
    } else if (flag == DATA_FLAG) {

        uint32_t mask = rd32(entry + E_MASK);
        uint8_t *data   = (uint8_t *)rd_ptr(entry + E_DATA);
        res = data + (uint64_t)(mask & dir);
    } else {
        res = (void *)0;
    }

    if (!(ctrl & CTRL_REPEAT)) {
        dma_regs_t *mirror = channel->regs;
        ctrl &= ~CTRL_ENABLE;
        channel->cnt = ctrl;
        mirror->cnt = ctrl;
    }

    if (ctrl & CTRL_IRQ) {

        uint8_t *core = (uint8_t *)channel->cpu;
        uint8_t *ie     = (uint8_t *)CPU(core)->io_mirror;

        uint32_t bit = 0x100u << (channel->index & 31u);
        uint32_t updated_if = bit | rd32(ie + I_IF);
        wr32(ie + I_IF, updated_if);

        uint8_t *nu = (uint8_t *)channel->cpu;

        uint32_t pend;
        if ((CPU(nu)->halt_flags & 6u) == 0) {

            uint32_t enable = rd32(ie + I_ENABLE);
            uint32_t mask_bits  = rd32(ie + I_MASK);
            pend = (enable & updated_if) & (0u - mask_bits);
            CPU(nu)->irq_pending = pend;
        } else {
            pend = CPU(nu)->irq_pending;
        }

        if (pend != 0)
            CPU(nu)->wake_flags = CPU(nu)->wake_flags | 2u;
    }

    return res;
}
#undef E_STEP
#undef E_MASK
#undef E_DATA
#undef E_FLAG
#undef B_CORE
#undef B_TABLE
#undef I_MASK
#undef I_ENABLE
#undef I_IF
#undef DATA_FLAG
#undef HANDLER_FLAG
#undef CTRL_REPEAT
#undef CTRL_IRQ
#undef CTRL_ENABLE

void dma_channels_schedule_deadlines(dma_t *dma, bus_t *a, bus_region_t *b,
                         io_mirror_t *base, arm_t *ctx) {

    for (int k = 0; k < 4; k++) {
        dma_channel_t *e = &dma->channels[k];
        e->cpu = ctx;
        e->regs = &base->dma[k];
        e->index = (uint8_t)k;

        if (ctx->is_arm9 == 1) {
            unsigned char *dst = (unsigned char *)ctx->machine;
            sched_deadline_set(&((nds_t *)dst)->sched, SCHED_DEADLINE_DMA_BASE + k, dma_transfer_end_event, e);
        }
    }
    dma->bus = a;
    dma->region = b;
}

void dma_channel_clear_fields(dma_channel_t *channel) {
    channel->deadline_cycles = 0;
    channel->started = 0;
    channel->src = 0;
    channel->dst = 0;
    channel->cnt = 0;
    channel->start_mode = 0;
}

typedef struct { uint64_t v; } __attribute__((packed, aligned(1))) u64_unaligned;

void *dma_channels_reset_fields(dma_t *dma) {
    for (int k = 0; k < 4; k++) {
        dma_channel_t *e = &dma->channels[k];
        e->deadline_cycles = 0;
        e->started = 0;
        e->src = 0;
        e->dst = 0;
        e->cnt = 0;
        e->start_mode = 0;
    }
    return dma;
}

#define REC_STRIDE 0x28u
#define REC_COUNT  4




static void write_register(dma_t *dma, int idx,
                              uint32_t a, uint32_t b, uint32_t c, uint8_t d,
                              uint64_t g, uint8_t e)
{
    dma_channel_t *channel = &dma->channels[idx];
    channel->deadline_cycles = g;
    channel->src = a;
    channel->dst = b;
    channel->cnt = c;
    channel->start_mode = d;
    channel->started = e;
}

void dma_channels_read_state_block(dma_t *dst, void *param_2, uint32_t param_3)
{

    unsigned char *cursor_slot = (unsigned char *)param_2 + 0x20;
    unsigned char *cur;
    memcpy(&cur, cursor_slot, sizeof(cur));

    if (param_3 <= 3) {

        for (int i = 0; i < REC_COUNT; i++) {
            uint32_t a = rd32(cur + 0);
            uint32_t b = rd32(cur + 4);
            uint32_t c = rd32(cur + 8);
            uint8_t  d = rd8(cur + 12);
            cur += 13;
            write_register(dst, i, a, b, c, d, 0, 0);
        }
    } else {

        for (int i = 0; i < REC_COUNT; i++) {
            uint32_t a = rd32(cur + 0);
            uint32_t b = rd32(cur + 4);
            uint32_t c = rd32(cur + 8);
            uint8_t  d = rd8(cur + 12);
            uint64_t g = rd64(cur + 13);
            cur += 21;

            uint8_t e;
            if (param_3 > 4) {
                e = rd8(cur);
                cur += 1;
            } else {
                e = 0;
            }
            write_register(dst, i, a, b, c, d, g, e);
        }
    }

    memcpy(cursor_slot, &cur, sizeof(cur));
}
#undef REC_STRIDE
#undef REC_COUNT

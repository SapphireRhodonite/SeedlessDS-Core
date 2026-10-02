#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include <string.h>
#include "core/sched.h"
#include "core_internals.h"
#include "mem_access.h"


const int32_t bus_cost_same_region[64] = {
    1, 1, 9, 1, 1, 1, 1, 1,
    6, 6, 6, 6, 6, 6, 6, 6,
    1, 1, 10, 1, 1, 2, 2, 1,
    12, 12, 12, 12, 12, 12, 12, 12,
    0, 0, 9, 4, 4, 4, 4, 4,
    13, 13, 13, 13, 13, 13, 13, 13,
    0, 0, 10, 4, 4, 5, 5, 4,
    19, 19, 19, 19, 19, 19, 19, 19,
};

const int32_t bus_cost_cross_region[64] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    6, 6, 6, 6, 6, 6, 6, 6,
    1, 1, 2, 1, 1, 2, 2, 1,
    12, 12, 12, 12, 12, 12, 12, 12,
    0, 0, 1, 1, 1, 1, 1, 1,
    6, 6, 6, 6, 6, 6, 6, 6,
    0, 0, 2, 1, 1, 2, 2, 1,
    12, 12, 12, 12, 12, 12, 12, 12,
};

void sched_deadline_insert(sched_t *base, uint32_t deadline, uint32_t idx) {
    sched_deadline_t *new = &base->deadline[idx];
    sched_deadline_t *head = base->head;

    sched_deadline_t *prev = 0;
    sched_deadline_t *next = head;
    uint32_t adjust = 0;

    if (next == 0) {
        adjust = 0;
    } else if (next->remaining >= deadline) {
        adjust = 1;
    } else {
        uint32_t own = next->remaining;
        for (;;) {
            prev = next;
            next = next->next;
            deadline -= own;
            if (next == 0) { adjust = 0; break; }
            own = next->remaining;
            if (deadline <= own) { adjust = 1; break; }
        }
    }

    sched_deadline_t **where = (prev == 0) ? &base->head
                                               : &prev->next;
    new->remaining = deadline;
    new->next = next;
    new->prev = prev;
    *where = new;

    if (adjust == 0)
        return;

    next->prev = new;
    next->remaining = next->remaining - deadline;
}

void sched_deadline_remove(sched_t *base, uint32_t idx) {
    sched_deadline_t *e = &base->deadline[idx];

    sched_deadline_t *next = e->next;
    sched_deadline_t *prev  = e->prev;

    if (prev != 0) {
        prev->next = next;
        next = e->next;
    } else {
        base->head = next;
    }

    if (next == 0)
        return;

    next->prev = prev;

    uint32_t own = base->deadline[idx].remaining;
    next->remaining = next->remaining + own;
}

void sched_deadline_set(sched_t *base, uint32_t idx,
                        void (*callback)(void *machine, void *argument), void *argument) {
    sched_deadline_t *e = &base->deadline[idx];
    e->callback = callback;
    e->argument = argument;
    e->index = (uint8_t)idx;
}

void *sched_init(sched_t *obj, unsigned char *ctx) {

    obj->deadline[SCHED_DEADLINE_SCANLINE].index = SCHED_DEADLINE_SCANLINE;
    obj->deadline[SCHED_DEADLINE_CYCLE_EVENT].index = SCHED_DEADLINE_CYCLE_EVENT;

    obj->deadline[SCHED_DEADLINE_SCANLINE].callback = sched_scanline_event;
    obj->deadline[SCHED_DEADLINE_SCANLINE].argument = ctx;

    obj->deadline[SCHED_DEADLINE_CART_TRANSFER].callback = cart_transfer_irq_raise;
    obj->head = 0;
    obj->deadline[SCHED_DEADLINE_VBLANK].index = SCHED_DEADLINE_VBLANK;

    obj->deadline[SCHED_DEADLINE_VBLANK].callback = gpu_vblank_dispatch;
    obj->deadline[SCHED_DEADLINE_VBLANK].argument = ctx;

    obj->deadline[SCHED_DEADLINE_CYCLE_EVENT].callback = sched_event_insert;
    obj->deadline[SCHED_DEADLINE_CYCLE_EVENT].argument = 0;

    obj->deadline[SCHED_DEADLINE_CART_TRANSFER].argument = &((nds_t *)ctx)->cart;
    obj->deadline[SCHED_DEADLINE_CART_TRANSFER].index = SCHED_DEADLINE_CART_TRANSFER;
    return obj;
}

static void mask_a(arm_t *cpu, uint32_t bit) {
    io_mirror_t *reg = cpu->io_mirror;
    uint32_t pend = reg->irq.if_pending | bit;
    reg->irq.if_pending = pend;
    if (!((uint8_t)cpu->halt_flags & 6))
        cpu->irq_pending = (reg->irq.ie & pend)
                         & (uint32_t)(-(int32_t)reg->irq.ime);
}

static void mask_b(arm_t *cpu, uint32_t bit) {
    io_mirror_t *reg = cpu->io_mirror;
    uint32_t pend = reg->irq.if_pending | bit;
    reg->irq.if_pending = pend;
    if (!((uint8_t)cpu->halt_flags & 6))
        cpu->irq_pending = (reg->irq.ie & pend)
                         & (uint32_t)(-(int32_t)reg->irq.ime);
}

void sched_scanline_event(void *machine, void *argument) {
    (void)argument;

    bus_t *bus = &((nds_t *)machine)->bus;
    unsigned char *bB  = (unsigned char *)&bus->io_mirror[1].dispstat;
    unsigned char *bA  = (unsigned char *)&bus->io_mirror[0].dispstat;
    arm_t *iB  = &((nds_t *)machine)->arm7;
    arm_t *iA  = &((nds_t *)machine)->arm9;

    sched_t *sched = &((nds_t *)machine)->sched;
    int32_t code = (int32_t)sched->scanline;
    uint32_t sig = (uint32_t)code + 1;

    uint64_t (*run_transfer)(dma_t *, dma_channel_t *) = dma_channel_run_transfer;


    if (code > 0x104) {
        if (code == 0x106) {
            for (int k = 0; k < 4; k++)
                if (((int32_t)bus->dma[0].channels[k].cnt < 0)
                    && bus->dma[0].channels[k].start_mode == 3)
                    run_transfer(&bus->dma[0], &bus->dma[0].channels[k]);
            gpu_frame_open((uint8_t *)&((nds_t *)machine)->gpu);
            sig = 0;
        } else if (code == 0x105) {
            *(uint8_t *)bA &= 0xfe;
            *(uint8_t *)bB &= 0xfe;
            sig = 0x106;
        }
    } else if (code == 0xbf) {
        uint32_t tok = cart_slot2_gpio_countdown_tick(&((nds_t *)machine)->cart.slot2.gpio);

        uint32_t v = *(uint8_t *)bA;
        *(uint8_t *)bA = (uint8_t)(v | 1);
        if (v & 8) mask_a(iA, 1);

        v = *(uint8_t *)bB;
        *(uint8_t *)bB = (uint8_t)(v | 1);
        if (v & 8) mask_b(iB, 1);

        gpu_frame_close((uint8_t *)&((nds_t *)machine)->gpu);
        input_record_end_of_frame((uint8_t *)&((nds_t *)machine)->input_record);
        benchmark_step((unsigned char *)&((nds_t *)machine)->benchmark);
        spi_memory_save_tick(&((nds_t *)machine)->cart.backup);
        spi_memory_save_tick(&((nds_t *)machine)->spi.firmware);
        cart_slot2_backup_autosave_tick(&((nds_t *)machine)->cart.slot2);

        nds_config_t *cfg = &((nds_t *)machine)->config;
        if (cfg->cheats_enabled != 0
            && ((uint8_t)iB->irq_pending & 1)
            && !((uint8_t)iB->cpsr & 0x80))
            cheats_apply_active(
                machine, &((nds_t *)machine)->cart.cheats, (uint32_t)sched->frame);

        (void)system_set_config_byte(tok);
        ((nds_t *)machine)->spu.recording.mic_time_base = sched->cycles;
        if (!((uint8_t)((nds_t *)machine)->benchmark.pass_flags & 0x40)) {
            spu_mixer_run_frame(machine);
            uint32_t f = cfg->fast_forward;
            uint32_t arg = (f != 0) ? 1u : (uint32_t)(cfg->fast_forward_speed != 0);
            (void)arg;
            platform_audio_output_feed_frame(&((nds_t *)machine)->spu);
        }

        for (int k = 0; k < 8; k++) {
            dma_t *g = &bus->dma[k / 4];
            if (((int32_t)g->channels[k % 4].cnt < 0)
                && g->channels[k % 4].start_mode == 1)
                run_transfer(g, &g->channels[k % 4]);
        }
        sched->frame += 1;
        sig = 0xc0;
    } else if (code == 0xd6) {
        unsigned char *p = &((nds_t *)machine)->gpu.engine[0].no_framebuffer;
        pacer_run(machine);
        uint32_t m = (uint8_t)((nds_t *)machine)->benchmark.pass_flags;
        uint32_t n = *(uint8_t *)p;
        uint32_t arg = ((m & 8) == 0) ? n : 1u;
        void *dst = &((nds_t *)machine)->gpu;
        if (((nds_t *)machine)->config.threaded_3d == 0) {
            gpu3d_raster_frame_dispatch_select(dst, arg);
            recon_scale_commit_output(recon_scale_render_tag);
        } else
            gpu3d_raster_frame_thread_submit(dst, arg);
        sig = 0xd7;
    }

    uint32_t v = *(uint16_t *)bA;
    uint32_t expected = ((v >> 8) & ~0x100u) | (((v >> 7) & 1u) << 8);
    if (sig == expected) {
        *(uint8_t *)bA = (uint8_t)(v | 4);
        if (v & 0x20) mask_a(iA, 4);
    } else {
        *(uint8_t *)bA = (uint8_t)(v & 0xfb);
    }

    v = *(uint16_t *)bB;
    expected = ((v >> 8) & ~0x100u) | (((v >> 7) & 1u) << 8);
    if (sig == expected) {
        *(uint8_t *)bB = (uint8_t)(v | 4);
        if (v & 0x20) mask_b(iB, 4);
    } else {
        *(uint8_t *)bB = (uint8_t)(v & 0xfb);
    }

    *(uint16_t *)(bA + 2) = (uint16_t)sig;
    *(uint16_t *)(bB + 2) = (uint16_t)sig;
    *(uint8_t *)bA &= 0xfd;
    *(uint8_t *)bB &= 0xfd;

    sched_deadline_t *n = sched->head;
    sched->scanline = (uint16_t)sig;
    sched_deadline_t *self = &sched->deadline[SCHED_DEADLINE_VBLANK];

    sched_deadline_t *prev = 0;
    uint32_t credit = SCHED_SCANLINE_CYCLES, adjust = 0;

    if (n != 0) {
        uint32_t c = n->remaining;
        if (c > SCHED_SCANLINE_CYCLES - 1u) {
            prev = 0;
            adjust = 1;
        } else {
            for (;;) {
                prev = n;
                n = n->next;
                credit -= c;
                if (n == 0) { adjust = 0; break; }
                c = n->remaining;
                if (credit > c) continue;
                adjust = 1;
                break;
            }
        }
    }

    sched_deadline_t **where = (prev == 0) ? &sched->head
                                          : &prev->next;
    self->remaining = credit;
    self->next = n;
    self->prev = prev;
    *where = self;
    if (adjust != 0) {
        uint32_t c = n->remaining;
        n->prev = self;
        n->remaining = c - credit;
    }
}

void sched_event_insert(void *obj, void *argument) {
    (void)argument;
    sched_t *sched = &((nds_t *)obj)->sched;
    uint32_t remaining = SCHED_CYCLE_EVENT_PERIOD - ((uint32_t)sched->cycles & (SCHED_CYCLE_EVENT_PERIOD - 1u));
    sched_deadline_t *node = &sched->deadline[SCHED_DEADLINE_CYCLE_EVENT];
    sched_deadline_t **slot_head = &sched->head;

    sched_deadline_t *sig = sched->head;
    sched_deadline_t *prev = 0;
    uint32_t deduct = 0;

    if (sig != 0) {
        uint32_t r = sig->remaining;
        if (remaining <= r) {
            prev = 0;
            deduct = 1;
        } else {
            for (;;) {
                prev = sig;
                sig = sig->next;
                remaining -= r;
                if (sig == 0) { deduct = 0; break; }
                r = sig->remaining;
                if (remaining > r) continue;
                deduct = 1;
                break;
            }
        }
    }

    sched_deadline_t **dest = (prev == 0) ? slot_head : &prev->next;

    node->remaining = remaining;
    node->next = sig;
    node->prev = prev;
    *dest = node;

    if (deduct) {
        uint32_t own = sig->remaining;
        sig->prev = node;
        sig->remaining = own - remaining;
    }
}

void sched_deadlines_clear(sched_t *base) {
    base->head = 0;
}

uint64_t sched_hook_noop(uint64_t x0)
{
    return x0;
}

typedef void (*fn_exit)(int);

void sched_list_check(uint8_t *param_1) {

    static fn_exit p_exit;
    if (!p_exit) p_exit = (fn_exit)sym_libc_exit;

    const sched_deadline_t *node = ((nds_t *)param_1)->sched.head;
    for (;;) {
        if (node == NULL) return;
        const sched_deadline_t *next = node->next;
        int distinct = (node != next);
        node = next;
        if (!distinct) break;
    }
    p_exit(-1);
}

void sched_advance(unsigned char *machine) {
    sched_t *sched = &((nds_t *)machine)->sched;
    uint32_t cycles = sched->slice_cycles;
    sched->cycles += (uint64_t)cycles;

    sched_deadline_t *ev = sched->head;
    uint32_t remaining = ev->remaining;

    if (remaining > cycles) {
        ev->remaining = remaining - cycles;
        return;
    }

    for (;;) {
        sched_deadline_t *next = ev->next;
        sched->head = next;

        void (*cb)(void *, void *) = ev->callback;
        void *arg                  = ev->argument;
        cb(machine, arg);

        ev = sched->head;

        if (ev == 0) break;
        uint32_t pending = ev->remaining;
        ev->prev = 0;
        if (pending != 0) break;
    }
}

#define TABLE_NE 0x10d8a0u
#define TABLE_EQ 0x10d9a0u


uint32_t sched_table_lookup_scaled(uint32_t param_1, uint32_t param_2,
                            uint32_t param_3, uint32_t param_4) {

    uint32_t masked = param_3 & 0x1fffffu;
    uint32_t factor = (masked != 0u) ? masked : 0x200000u;

    uint32_t u2 = ((param_1 >> 28) == 0u) ? ((param_1 >> 24) & 0xffu) : 0u;
    uint32_t u3 = ((param_2 >> 28) == 0u) ? ((param_2 >> 24) & 0xffu) : 0u;
    uint32_t half = (param_3 >> 26) & 1u;

    unsigned long shift = (unsigned long)param_4 * 0x80u + (unsigned long)half * 0x40u;

    int32_t val;
    if (u2 == u3) {
        val = rd32s((const uint8_t *)bus_cost_same_region + shift + (unsigned long)u2 * 4u);
    } else {
        val = rd32s((const uint8_t *)bus_cost_cross_region + shift + (unsigned long)u3 * 4u);
        if (u3 != 6u)
            val += rd32s((const uint8_t *)bus_cost_cross_region + shift + (unsigned long)u2 * 4u);
    }

    return (uint32_t)val * factor;
}
#undef TABLE_NE
#undef TABLE_EQ

typedef uint64_t (*fn_descriptor)(void *);

uint64_t sched_dispatch_arm7(uint8_t *machine) {

    arm_t *arm7 = &((nds_t *)machine)->arm7;
    uint32_t increment = ((nds_t *)machine)->sched.slice_cycles;
    uint32_t counter = arm7->cycle_mark;
    fn_descriptor dest = arm7->run_entry;
    void *arg = arm7->machine;

    arm7->cycle_mark = counter + increment;
    return dest(arg);
}

static void drain_queue(arm_t *block)
{
    uint32_t pending;

    if (block->irq_pending == 0)
        return;

    pending = block->halt_flags;
    block->halt_flags = 0;
    block->wake_flags = 0;

    if (((uint8_t)block->cpsr & 0x80u) == 0)
        arm_exception_raise((unsigned char *)block, 6);

    if (pending == 0 || block->is_arm9 != 0)
        return;

    if (pending >= 2) {
        arm_t *state = block->partner;
        uint32_t flags = state->halt_flags;
        state->halt_flags = (flags & 0xfffffffdu);
    }

    sched_event_insert(block->machine, NULL);
}

uint64_t sched_dispatch_next(uint8_t *machine)
{

    arm_t *first = &((nds_t *)machine)->arm9;
    arm_t *second = &((nds_t *)machine)->arm7;

    sched_advance(machine);

    drain_queue(first);
    drain_queue(second);

    {
        const sched_deadline_t *clock = ((nds_t *)machine)->sched.head;
        uint32_t cycles = clock->remaining;
        uint32_t accumulated = first->cycle_mark;
        uint32_t pending = first->halt_flags;

        ((nds_t *)machine)->sched.slice_cycles = cycles;
        first->cycle_mark = accumulated + cycles;

        if (pending != 0) {
            uint64_t (*dest)(void *) = first->run_entry;
            first->cycle_mark = UINT32_MAX;
            return dest(machine);
        }

        return first->run_entry((uint8_t *)first->machine);
    }
}

#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "timers.h"
#include "core/nds_state.h"
#include "core/sched.h"
#include "core_internals.h"
#define CPU(p) ((arm_t *)(p))

void timer_overflow_event(void *machine, void *argument) {
    unsigned char *timer = argument;

    unsigned char *ctx = *(unsigned char **)timer;
    uint32_t control = *(uint16_t *)(timer + 26);
    uint32_t index  = timer[29];
    uint32_t sel   = ((arm_t *)ctx)->is_arm9;

    int count = 0;
    if (sel == 0 && index == 1) {
        if (((nds_t *)machine)->config.sound_enabled != 0 &&
            ((uint8_t)((nds_t *)machine)->benchmark.pass_flags & 0x40) == 0) {
            spu_mixer_run_frame(machine);
            count = (timer[58] & 4) != 0;
        } else {
            count = (timer[58] & 4) != 0;
        }
    } else if (index != 3) {
        count = (timer[58] & 4) != 0;
    }

    if (count) {
        unsigned char *c = *(unsigned char **)timer;
        uint32_t s = ((arm_t *)c)->is_arm9;
        unsigned char *t = (unsigned char *)CPU(c)->bus + (index << 2) + 0x104;
        uint32_t off = (s == 1) ? 0x1b070 : 0x23070;
        *(uint16_t *)(t + off) = (uint16_t)(*(uint16_t *)(t + off) + 1);
    }

    if (control & 0x40) {
        unsigned char *c = *(unsigned char **)timer;
        unsigned char *regs = (unsigned char *)((arm_t *)c)->io_mirror;
        uint32_t requested_n = ((io_mirror_t *)regs)->irq.if_pending;
        requested_n |= *(uint32_t *)(timer + 16);
        ((io_mirror_t *)regs)->irq.if_pending = requested_n;
        if ((((arm_t *)c)->halt_flags & 6u) == 0) {
            uint32_t enabled = ((io_mirror_t *)regs)->irq.ie;
            uint32_t msk = ((io_mirror_t *)regs)->irq.ime;
            uint32_t r = enabled & requested_n;
            r &= (uint32_t)(-(int32_t)msk);
            ((arm_t *)c)->irq_pending = r;
        }
    }

    sched_t *sched = &((nds_t *)machine)->sched;
    ((nds_timer_t *)timer)->start_cycles = sched->cycles;
    uint32_t remaining = *(uint32_t *)(timer + 20);
    uint32_t idx = (SCHED_DEADLINE_TIMER_BASE | (sel << 2)) + index;
    sched_deadline_t *node = &sched->deadline[idx];
    sched_deadline_t **slot_head = &sched->head;

    sched_deadline_t *sig = sched->head;
    sched_deadline_t *prev = 0;
    uint32_t deduct = 0;

    if (sig != 0) {
        uint32_t r = sig->remaining;
        if (remaining <= r) { prev = 0; deduct = 1; }
        else {
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


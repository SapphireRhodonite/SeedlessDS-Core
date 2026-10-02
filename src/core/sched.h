#ifndef SEEDLESSDS_CORE_SCHED_H
#define SEEDLESSDS_CORE_SCHED_H

#include <stddef.h>
#include <stdint.h>

#define SCHED_DEADLINES 16
#define SCHED_DEADLINE_VBLANK 0
#define SCHED_DEADLINE_SCANLINE 1
#define SCHED_DEADLINE_CYCLE_EVENT 2
#define SCHED_DEADLINE_TIMER_BASE 3
#define SCHED_DEADLINE_CART_TRANSFER 11
#define SCHED_DEADLINE_DMA_BASE 12
#define SCHED_CYCLE_EVENT_PERIOD 128u
#define SCHED_SCANLINE_CYCLES 0xc00u
#define SCHED_SCANLINE_RESET 0x106u

typedef struct sched_deadline {
    uint32_t remaining;
    uint32_t unmapped_0;
    void (*callback)(void *machine, void *argument);
    void *argument;
    struct sched_deadline *next;
    struct sched_deadline *prev;
    uint8_t index;
    uint8_t unmapped_1[7];
} sched_deadline_t;

typedef struct sched {
    uint64_t frame;
    uint64_t cycles;
    uint32_t slice_cycles;
    uint16_t scanline;
    uint8_t unmapped_0[2];
    sched_deadline_t deadline[SCHED_DEADLINES];
    sched_deadline_t *head;
} sched_t;
#endif

#ifndef SEEDLESSDS_TIMERS_H
#define SEEDLESSDS_TIMERS_H

#include <stdint.h>

struct arm;

typedef struct nds_timer {
    struct arm *cpu;
    uint64_t start_cycles;
    uint32_t irq_bit;
    uint32_t period_cycles;
    uint16_t reload;
    uint16_t control;
    uint8_t prescaler_shift;
    uint8_t index;
    uint8_t scheduled;
    uint8_t reserved_2;
} nds_timer_t;

void timer_overflow_event(void *machine, void *argument);

#endif

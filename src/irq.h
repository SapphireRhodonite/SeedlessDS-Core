#ifndef SEEDLESSDS_IRQ_H
#define SEEDLESSDS_IRQ_H

#include <stdint.h>

typedef struct irq_regs {
    uint32_t ime;
    uint8_t reserved_0[4];
    uint32_t ie;
    uint32_t if_pending;
} irq_regs_t;

#define IRQ_TIMER0 0x8u
#endif

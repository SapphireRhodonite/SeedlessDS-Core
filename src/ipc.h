#ifndef SEEDLESSDS_IPC_H
#define SEEDLESSDS_IPC_H

#include <stdint.h>

struct arm;
struct io_mirror;

typedef struct ipc_fifo {
    struct arm *cpu;
    struct ipc_fifo *other;
    struct io_mirror *mirror;
    uint32_t entries[16];
    uint8_t head;
    uint8_t tail;
    uint8_t flags;
    uint8_t reserved_0[5];
} ipc_fifo_t;

typedef struct ipc_regs {
    uint16_t ipcsync;
    uint8_t reserved_0[2];
    uint16_t ipcfifocnt;
    uint8_t reserved_1[2];
    uint32_t ipcfifosend;
} ipc_regs_t;

#define IPC_FIFO_EMPTY 1u
#define IPC_FIFO_FULL 2u

void ipc_fifo_init(ipc_fifo_t *fifo, struct arm *cpu, ipc_fifo_t *other);
void ipc_fifo_clear(ipc_fifo_t *fifo);
void *ipc_fifo_reset(ipc_fifo_t *fifo);
void ipc_fifo_push(ipc_fifo_t *fifo, uint32_t value);
uint32_t ipc_fifo_recv(ipc_fifo_t *fifo);
void ipc_fifo_state_read(ipc_fifo_t *fifo, unsigned char *reader);
void ipc_fifo_state_write(const ipc_fifo_t *fifo, unsigned char *writer);
#endif

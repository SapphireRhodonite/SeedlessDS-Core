#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "ipc.h"
#include "cpu/arm.h"
#include "core/nds_state.h"

void ipc_fifo_init(ipc_fifo_t *fifo, struct arm *cpu, ipc_fifo_t *other) {
    fifo->cpu = cpu;
    struct io_mirror *mirror = cpu->io_mirror;
    fifo->other = other;
    fifo->mirror = mirror;
}

void ipc_fifo_clear(ipc_fifo_t *fifo) {
    memset(fifo->entries, 0, sizeof fifo->entries);
    fifo->head = 0;
    fifo->tail = 0;
    fifo->flags = IPC_FIFO_EMPTY;
}

void *ipc_fifo_reset(ipc_fifo_t *fifo) {
    uint32_t state = fifo->flags;
    fifo->head = 0;
    fifo->tail = 0;
    state = (state & 0xfcu) | IPC_FIFO_EMPTY;
    fifo->flags = (uint8_t)state;
    return fifo;
}

void ipc_fifo_push(ipc_fifo_t *fifo, uint32_t value) {
    uint32_t flags = fifo->flags;
    if (flags & IPC_FIFO_FULL)
        return;

    uint32_t write_fn = fifo->tail;
    flags &= 0xfe;

    fifo->entries[write_fn] = value;

    uint32_t read_idx = fifo->head;
    write_fn = (write_fn + 1) & 0xf;
    fifo->tail = (uint8_t)write_fn;
    read_idx = (read_idx - 1) & 0xf;

    fifo->flags = (uint8_t)flags;
    if (write_fn == read_idx)
        fifo->flags = (uint8_t)(flags | IPC_FIFO_FULL);
}

uint32_t ipc_fifo_recv(ipc_fifo_t *fifo) {
    io_mirror_t *first = fifo->mirror;
    uint32_t mark = first->ipc.ipcfifocnt;
    if ((mark & 0x8000u) == 0)
        return 0;

    uint32_t flags = fifo->flags;
    if (flags & IPC_FIFO_EMPTY) {
        first->ipc.ipcfifocnt = (uint16_t)(mark | 0x4000u);
        return 0;
    }

    uint32_t read_idx = fifo->head;
    ipc_fifo_t *second_owner = fifo->other;
    uint32_t write_fn = fifo->tail;
    flags &= 0xfffffffdu;
    io_mirror_t *second = second_owner->mirror;

    uint32_t value = fifo->entries[read_idx];

    read_idx = (read_idx + 1) & 0xf;
    fifo->head = (unsigned char)read_idx;
    fifo->flags = (unsigned char)flags;
    if (write_fn == read_idx) {
        flags |= IPC_FIFO_EMPTY;
        fifo->flags = (unsigned char)flags;
    }

    second->ipc.ipcfifocnt = (uint16_t)(second->ipc.ipcfifocnt & 0xfffdu);

    if ((fifo->flags & IPC_FIFO_EMPTY) == 0)
        return value;

    first->ipc.ipcfifocnt = (uint16_t)(first->ipc.ipcfifocnt | 0x100u);
    uint32_t before = second->ipc.ipcfifocnt & 0xffu;
    second->ipc.ipcfifocnt = (uint16_t)(second->ipc.ipcfifocnt | 1u);
    if ((before & 2) == 0)
        return value;

    unsigned char *third = (unsigned char *)fifo->cpu;
    if (((arm_t *)third)->is_arm9 == 1)
        ((arm_t *)third)->wake_flags = ((arm_t *)third)->wake_flags | 4;

    unsigned char *base = (unsigned char *)second_owner->cpu;
    unsigned char *quarter = (unsigned char *)((arm_t *)base)->io_mirror;
    uint32_t v532 = ((io_mirror_t *)quarter)->irq.if_pending | 0x20000;
    ((io_mirror_t *)quarter)->irq.if_pending = v532;

    base = (unsigned char *)second_owner->cpu;
    if ((((arm_t *)base)->halt_flags & 6) != 0)
        return value;

    uint32_t v528 = ((io_mirror_t *)quarter)->irq.ie;
    uint32_t v520 = ((io_mirror_t *)quarter)->irq.ime;
    ((arm_t *)base)->irq_pending = (v528 & v532) & (uint32_t)(-(int32_t)v520);
    return value;
}

void ipc_fifo_state_read(ipc_fifo_t *fifo, unsigned char *reader)
{
    const uint8_t *cursor;
    memcpy(&cursor, reader + 32, 8);

    memcpy(fifo->entries, cursor, sizeof fifo->entries);

    memcpy(&cursor, reader + 32, 8);
    const uint8_t *x8_frozen = cursor;
    cursor += 0x40; memcpy(reader + 32, &cursor, 8);
    fifo->head = x8_frozen[64];

    memcpy(&cursor, reader + 32, 8);
    x8_frozen = cursor;
    cursor += 1; memcpy(reader + 32, &cursor, 8);
    fifo->tail = x8_frozen[1];

    memcpy(&cursor, reader + 32, 8);
    x8_frozen = cursor;
    cursor += 1; memcpy(reader + 32, &cursor, 8);
    fifo->flags = x8_frozen[1];

    memcpy(&cursor, reader + 32, 8);
    cursor += 1; memcpy(reader + 32, &cursor, 8);
}

void ipc_fifo_state_write(const ipc_fifo_t *fifo, unsigned char *writer) {
    unsigned char **pcursor = (unsigned char **)(writer + 0x20);
    unsigned char *dst = *pcursor;

    memcpy(dst, fifo->entries, sizeof fifo->entries);
    dst += 0x40;
    *pcursor = dst;

    dst[0] = fifo->head;
    dst += 1;
    *pcursor = dst;

    dst[0] = fifo->tail;
    dst += 1;
    *pcursor = dst;

    dst[0] = fifo->flags;
    dst += 1;
    *pcursor = dst;
}

#include "wifi_core.h"
#include "wm_host.h"
#include "frontend/frontend.h"
#include "memory/io7.h"
#include "seedlessds/wifi.h"
#include "seedlessds/multiplayer.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "spi/firmware.h"
#include <string.h>

static seedless_wifi_t *controller;
static nds_t *owner;
static sched_deadline_t timer;
static int scheduled;
static int active;

static struct seedless_wm *wm;

extern void bus_write8(uint8_t *, uint64_t, uint64_t);
extern void bus_write16(uint8_t *, uint32_t, uint16_t);
extern void bus_write32(uint8_t *, uint32_t, uint32_t);

static unsigned wm_level(void *context, unsigned cpu)
{
    ipc_fifo_t *fifo = &((nds_t *)context)->bus.ipc_fifo[cpu];
    return fifo->flags & IPC_FIFO_FULL ? 16 : (fifo->tail - fifo->head) & 15u;
}
static uint32_t wm_peek(void *context)
{
    ipc_fifo_t *fifo = &((nds_t *)context)->bus.ipc_fifo[1];
    return fifo->entries[fifo->head];
}
static void wm_pop(void *context) { ipc_fifo_recv(&((nds_t *)context)->bus.ipc_fifo[1]); }
static void wm_reply(void *context, uint32_t word) { io7_write32((uint8_t *)&((nds_t *)context)->bus, 0x188, word); }
static void wm_interrupt(void *context, unsigned index, unsigned irq)
{
    nds_t *machine = context;
    arm_t *cpu = index ? &machine->arm7 : &machine->arm9;
    io_mirror_t *mirror = cpu->io_mirror;
    mirror->irq.if_pending |= 1u << irq;
    if (!(cpu->halt_flags & 6u)) {
        cpu->irq_pending = mirror->irq.ie & mirror->irq.if_pending & (0u - mirror->irq.ime);
        if (cpu->irq_pending) cpu->wake_flags |= 2u;
    }
}
static void wm_write8(void *c, uint32_t a, uint8_t v) { bus_write8((uint8_t *)((nds_t *)c)->bus.arm7_pagetable, a, v); }
static void wm_write16(void *c, uint32_t a, uint16_t v) { bus_write16((uint8_t *)((nds_t *)c)->bus.arm7_pagetable, a, v); }
static void wm_write32(void *c, uint32_t a, uint32_t v) { bus_write32((uint8_t *)((nds_t *)c)->bus.arm7_pagetable, a, v); }
static void wm_stop(void *context) { (void)context; __atomic_store_n(&FRONTEND->quit_request, 1, __ATOMIC_RELEASE); }
void wifi_wm_poll(void) { if (wm) seedless_wm_poll(wm); }
void wifi_wm_vblank(uint64_t edge_us)
{
    if (wm && !owner->config.fast_forward && !owner->config.fast_forward_speed
        && !owner->benchmark.enabled && !nds_control_requires_fixed_pacing()
        && !wifi_core_client_pacing())
        seedless_wm_vblank(wm, edge_us);
}
void wifi_wm_capture(uint32_t word) { if (wm) seedless_wm_capture(wm, word); }
int wifi_wm_blocks(void) { return wm && seedless_wm_blocks(wm); }
void wifi_wm_clear(void) { if (wm) seedless_wm_clear(wm); }
void wifi_wm_frame(uint32_t budget) { if (wm) seedless_wm_frame(wm, budget); }
int wifi_wm_host_pending(void) { return wm && seedless_wm_host_pending(wm); }
int wifi_wm_needs_guest_cycles(void) { return wm && seedless_wm_needs_guest_cycles(wm); }
void wifi_wm_poll_host(void) { if (wm) seedless_wm_poll_host(wm); }

int wifi_core_active(void)
{
    return __atomic_load_n(&active, __ATOMIC_ACQUIRE);
}

int wifi_core_client_pacing(void)
{
    return controller && seedless_mp_enabled() && seedless_wifi_is_client(controller);
}

void wifi_core_flush(void)
{
    if (controller) seedless_wifi_sync(controller);
}

static void cancel(void *context)
{
    nds_t *machine = context;
    if (!scheduled) return;
    sched_deadline_t *previous = timer.prev;
    sched_deadline_t *next = timer.next;
    if (previous) previous->next = next;
    else machine->sched.head = next;
    if (next) {
        next->prev = previous;
        next->remaining += timer.remaining;
    }
    scheduled = 0;
}

static void tick(void *machine, void *argument)
{
    (void)machine;
    (void)argument;
    scheduled = 0;
    seedless_wifi_tick(controller);
}

static void schedule(void *context, uint32_t arm7_cycles, int first)
{
    nds_t *machine = context;
    sched_t *sched = &machine->sched;
    uint32_t remaining = arm7_cycles * 2;
    if (first) {
        int32_t elapsed = (int32_t)sched->slice_cycles - (int32_t)machine->arm7.cycle_mark;
        if (elapsed > 0) remaining += (uint32_t)elapsed;
        if (remaining < sched->slice_cycles) remaining = sched->slice_cycles;
    }
    cancel(context);
    sched_deadline_t *previous = NULL;
    sched_deadline_t *next = sched->head;
    while (next && remaining > next->remaining) {
        remaining -= next->remaining;
        previous = next;
        next = next->next;
    }
    timer.remaining = remaining;
    timer.callback = tick;
    timer.argument = NULL;
    timer.prev = previous;
    timer.next = next;
    timer.index = 0xFF;
    if (previous) previous->next = &timer;
    else sched->head = &timer;
    if (next) {
        next->remaining -= remaining;
        next->prev = &timer;
    }
    scheduled = 1;
}

static void interrupt(void *context)
{
    arm_t *cpu = &((nds_t *)context)->arm7;
    io_mirror_t *registers = cpu->io_mirror;
    registers->irq.if_pending |= 1u << 24;
    if (!(cpu->halt_flags & 6u)) {
        cpu->irq_pending = registers->irq.ie & registers->irq.if_pending & (0u - registers->irq.ime);
        if (cpu->irq_pending) cpu->wake_flags |= 2u;
    }
}

void wifi_core_prepare_firmware(uint8_t *firmware)
{
    if (!seedless_mp_enabled()) return;
    if (!memcmp(firmware + 8, "MACP", 4)) seedless_wifi_default_calibration(firmware);
    seedless_mp_mac(firmware + 0x36);
    uint32_t length = firmware[0x2C] | ((uint32_t)firmware[0x2D] << 8);
    if (length > 0x1D4) length = 0x138;
    uint16_t crc = (uint16_t)firmware_crc16(0, firmware + 0x2C, length);
    memcpy(firmware + 0x2A, &crc, sizeof(crc));
}

void wifi_core_release(void)
{
    __atomic_store_n(&active, 0, __ATOMIC_RELEASE);
    if (wm) {
        seedless_wm_destroy(wm);
        wm = NULL;
        seedless_mp_bind_wm(NULL);
    }
    if (controller) seedless_wifi_destroy(controller);
    controller = NULL;
    owner = NULL;
    scheduled = 0;
}

int wifi_core_reset(nds_t *machine)
{
    wifi_core_release();
    if (!seedless_mp_enabled()) return 0;
    owner = machine;
    seedless_wifi_callbacks_t callbacks = {
        .context = machine,
        .firmware = machine->bus.firmware_image,
        .schedule = schedule,
        .cancel = cancel,
        .irq = interrupt,
    };
    seedless_mp_bind(&callbacks);
    controller = seedless_wifi_create(&callbacks);
    if (!controller) { owner = NULL; return -1; }
    const seedless_wm_host host = {
        .owner = machine, .ram = machine->bus.main_ram, .ram_mask = machine->bus.region[36].mask,
        .firmware = machine->bus.firmware_image, .cycles = &machine->sched.cycles, .frame = &machine->sched.frame,
        .vcount = &machine->sched.scanline, .control = {&machine->bus.io_mirror[0].ipc.ipcfifocnt, &machine->bus.io_mirror[1].ipc.ipcfifocnt},
        .directory = machine->cache_dir, .jit = machine->runtime.jit_enabled,
        .level = wm_level, .peek = wm_peek, .pop = wm_pop, .reply = wm_reply, .interrupt = wm_interrupt,
        .write8 = wm_write8, .write16 = wm_write16, .write32 = wm_write32, .stop = wm_stop
    };
    wm = seedless_wm_create(&host);
    if (!wm) { wifi_core_release(); return -1; }
    seedless_mp_bind_wm(machine);
    __atomic_store_n(&active, 1, __ATOMIC_RELEASE);
    const uint8_t *registers = (const uint8_t *)&machine->bus.io_mirror[1];
    seedless_wifi_power(controller, registers[0x304]);
    return 0;
}

uint16_t wifi_core_read(uint32_t address)
{
    const uint8_t *registers = (const uint8_t *)&owner->bus.io_mirror[1];
    if (!(registers[0x304] & 2)) return 0;
    return seedless_wifi_read(controller, 0x04800000u | (address & 0x7FFFFFu));
}

void wifi_core_write(uint32_t address, uint16_t value)
{
    const uint8_t *registers = (const uint8_t *)&owner->bus.io_mirror[1];
    if (registers[0x304] & 2)
        seedless_wifi_write(controller, 0x04800000u | (address & 0x7FFFFFu), value);
}

void wifi_core_power(uint32_t value)
{
    if (controller) seedless_wifi_power(controller, value);
}

#include <stdint.h>
#include "jit_hooks.h"
#include <stddef.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/cpu_backend.h"
#include "core/nds_state.h"
#include "core_internals.h"

static int jit_arm64_init(nds_t *machine, arm_t *cpu)
{
    (void)cpu;
    jit_arena_regions_init((unsigned char *)&machine->jit_arena);
    return 0;
}

static void jit_arm64_reset(arm_t *cpu)
{
    jit_cache_flush((uint8_t *)(cpu), 0xffffffffu);
}

static uint32_t jit_arm64_run(arm_t *cpu, uint32_t cycles)
{
    (void)cycles;
    return (uint32_t)cpu->run_entry(cpu->machine);
}

static void jit_arm64_invalidate(arm_t *cpu, uint32_t address, uint32_t length)
{
    (void)length;
    jit_cache_flush((uint8_t *)(cpu), address);
}

static void jit_arm64_flush_all(arm_t *cpu)
{
    jit_cache_flush((uint8_t *)(cpu), 0xffffffffu);
}

static void jit_arm64_irq(arm_t *cpu)
{
    ((void (*)(void *))cpu->jit_irq_entry)(cpu);
}

static void jit_arm64_save(arm_t *cpu, state_stream_t *stream)
{
    (void)cpu;
    (void)stream;
}

static void jit_arm64_shutdown(arm_t *cpu)
{
    (void)cpu;
}

const cpu_backend_t *nds_cpu_backend_jit_arm64(void)
{
    static const cpu_backend_t backend = {
        "jit-arm64",
        jit_arm64_init,
        jit_arm64_reset,
        jit_arm64_run,
        jit_arm64_invalidate,
        jit_arm64_flush_all,
        jit_arm64_irq,
        jit_arm64_save,
        jit_arm64_shutdown,
    };
    return &backend;
}

const cpu_backend_t *nds_cpu_backend_default(void)
{
    return nds_cpu_backend_jit_arm64();
}

#ifndef SEEDLESSDS_CPU_BACKEND_H
#define SEEDLESSDS_CPU_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nds nds_t;
typedef struct arm arm_t;
typedef struct state_stream state_stream_t;

typedef struct cpu_backend {
    const char *name;
    int (*init)(nds_t *machine, arm_t *cpu);
    void (*reset)(arm_t *cpu);
    uint32_t (*run)(arm_t *cpu, uint32_t cycles);
    void (*invalidate)(arm_t *cpu, uint32_t address, uint32_t length);
    void (*flush_all)(arm_t *cpu);
    void (*irq)(arm_t *cpu);
    void (*save)(arm_t *cpu, state_stream_t *stream);
    void (*shutdown)(arm_t *cpu);
} cpu_backend_t;

const cpu_backend_t *nds_cpu_backend_jit_arm64(void);
const cpu_backend_t *nds_cpu_backend_interp(void);
const cpu_backend_t *nds_cpu_backend_default(void);

#ifdef __cplusplus
}
#endif

#endif

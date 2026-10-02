#ifndef SEEDLESSDS_CP15_H
#define SEEDLESSDS_CP15_H

#include <stddef.h>
#include <stdint.h>

struct arm;
struct bus;

#define CP15_SIZE 0x78u

typedef struct cp15 {
    struct arm *arm;
    struct bus *bus;
    uint32_t exception_vector_base;
    uint32_t control;
    uint32_t dtcm_region;
    uint32_t itcm_region;
    uint32_t dtcm_base;
    uint32_t dtcm_enabled;
    uint32_t dtcm_load_mode;
    uint32_t dtcm_size;
    uint32_t itcm_enabled;
    uint32_t itcm_load_mode;
    uint32_t itcm_size;
    uint8_t dtcm_below_64mb;
    uint8_t unmapped_0[CP15_SIZE - 61];
} cp15_t;
#endif

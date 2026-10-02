#ifndef SEEDLESSDS_SPU_ADPCM_TABLES_H
#define SEEDLESSDS_SPU_ADPCM_TABLES_H

#include <stdint.h>

#define SPU_ADPCM_STEP_COUNT 89u
#define SPU_ADPCM_INDEX_DELTA_COUNT 8u

typedef struct spu_adpcm_tables {
    int16_t step[SPU_ADPCM_STEP_COUNT];
    int8_t  index_delta[SPU_ADPCM_INDEX_DELTA_COUNT];
} spu_adpcm_tables_t;

extern const spu_adpcm_tables_t spu_adpcm_tables;

#endif

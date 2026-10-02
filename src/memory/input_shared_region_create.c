#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "frontend/frontend.h"
#include "core_internals.h"


extern int32_t mirror_shm_create(const char *name, uint32_t size,
                                   int32_t level_api);

int32_t input_shared_region_create(const char *name, uint32_t size)
{

    int32_t level_api = (int32_t)FRONTEND->api_level;

    return mirror_shm_create(name, size, level_api);
}

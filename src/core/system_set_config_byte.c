#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "frontend/frontend.h"

long system_set_config_byte(long a0) {
    FRONTEND->rumble = (uint8_t)a0;
    return a0;
}

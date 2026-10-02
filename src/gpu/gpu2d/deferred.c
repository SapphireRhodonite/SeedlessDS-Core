#include <stdint.h>
#include "hires_runtime.h"
#include "core/nds_state.h"

void gpu2d_deferred_capture_queue_push(gpu2d_engine_t *engine, uint32_t a, uint32_t b,
                        uint32_t c, uint32_t d) {

    uint32_t n = engine->deferred_count;
    gpu2d_deferred_write_t *e = &engine->deferred[n];

    e->line = (uint8_t)d;
    e->address = a;
    e->value = b;
    e->size = (uint8_t)c;

    engine->deferred_count = n + 1;
}

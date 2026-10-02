#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "frontend/frontend.h"

#include <stdint.h>
#include "core_internals.h"


extern void script_io_reset(void);

void system_reset_module_state(void) {

    frontend_t *g = FRONTEND;

    g->script_overrides = 0;
    g->script.axis_rx = 0;
    g->script.axis_ry = 0;
    g->script.rotation = 0;
    memset(g->script.unmapped_0, 0, sizeof g->script.unmapped_0);
    g->script.axis_lx = 0;
    g->script.axis_ly = 0;

    script_io_reset();
}

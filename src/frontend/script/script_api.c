#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/script.h"
#include "core/nds_state.h"
#include "frontend/frontend.h"
#include "frontend/script/script_io.h"
#include "frontend/script/script_state.h"
#include "core_internals.h"


int nds_script_load(nds_t *nds, const char *path)
{
    (void)nds;
    script_invoke_on_load(path);
    return script_is_active() != 0 ? 0 : -1;
}

void nds_script_unload(nds_t *nds)
{
    (void)nds;
    script_run_on_unload();
}

int nds_script_is_active(nds_t *nds)
{
    (void)nds;
    return (int)script_is_active();
}

void nds_script_on_frame(nds_t *nds)
{
    (void)nds;
    script_run_on_frame_update();
}

uint32_t nds_script_get_overrides(nds_t *nds)
{
    (void)nds;
    return FRONTEND->script_overrides;
}

void nds_script_set_axis(nds_t *nds, const float *axes, size_t count)
{
    (void)nds;
    script_io_t *io = SCRIPT_IO;
    if (count > 0) io->axis_lx = axes[0];
    if (count > 1) io->axis_ly = axes[1];
    if (count > 2) io->axis_rx = axes[2];
    if (count > 3) io->axis_ry = axes[3];
}

void nds_script_set_rotation(nds_t *nds, float rotation)
{
    (void)nds;
    SCRIPT_IO->rotation = (int16_t)rotation;
}

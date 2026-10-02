


#include <stdlib.h>
#include <string.h>

#include "seedlessds/nds.h"
#include "seedlessds/cpu_backend.h"

struct nds { uint32_t frame; };

static int  b_init(nds_t *m, arm_t *c)               { (void)m; (void)c; return 0; }
static void b_reset(arm_t *c)                        { (void)c; }
static uint32_t b_run(arm_t *c, uint32_t cycles)     { (void)c; return cycles; }
static void b_invalidate(arm_t *c, uint32_t a, uint32_t n) { (void)c; (void)a; (void)n; }
static void b_flush(arm_t *c)                        { (void)c; }
static void b_irq(arm_t *c)                          { (void)c; }
static void b_save(arm_t *c, state_stream_t *s)      { (void)c; (void)s; }
static void b_shutdown(arm_t *c)                     { (void)c; }

static const cpu_backend_t interp = {
    "interp-stub", b_init, b_reset, b_run, b_invalidate, b_flush, b_irq, b_save, b_shutdown
};

const cpu_backend_t *nds_cpu_backend_interp(void)   { return &interp; }
const cpu_backend_t *nds_cpu_backend_default(void)  { return &interp; }

const char *nds_version_string(void) { return "SeedlessDS port x86 (stubs)"; }

nds_t *nds_create(const nds_platform_t *platform, uint32_t api_level, uint32_t script_overrides_high)
{
    (void)platform; (void)api_level; (void)script_overrides_high;
    return (nds_t *)calloc(1, sizeof(nds_t));
}

void nds_destroy(nds_t *nds) { free(nds); }
int  nds_load_rom(nds_t *nds, const nds_rom_open_t *open) { (void)open; return nds != 0; }
void nds_quit(nds_t *nds) { (void)nds; }
void nds_run_frame(nds_t *nds, const nds_input_t *input) { (void)input; if (nds) nds->frame++; }
uint32_t nds_frame_word(nds_t *nds) { return nds ? nds->frame : 0u; }

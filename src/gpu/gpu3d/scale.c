#include "hires_runtime.h"

#ifndef RECON_BANDS_DEFAULT
#define RECON_BANDS_DEFAULT 24u
#endif

static unsigned recon_app_n;
static int recon_app_nat, recon_app_present;

int recon_app_scale(unsigned *n, int *native)
{
    *n = recon_app_n;
    *native = recon_app_nat;
    return recon_app_present;
}

__attribute__((weak)) int recon_dynamic_share(void) { return recon_scale_3d > 2u; }

__attribute__((weak)) void recon_bands_set(void)
{
    unsigned height = 192u * recon_scale_3d;
    unsigned b = (recon_scale_3d > 2u) ? (unsigned)RECON_BANDS_DEFAULT : 12u;
    if (b == 24u) {
        for (unsigned bands = 32u; bands <= RECON_BAND_CAPACITY; bands += 16u) {
            unsigned rows = height / bands;
            unsigned tag_bytes = rows * RECON_3D_WIDTH;
            if (tag_bytes < 0x4000u) tag_bytes = 0x4000u;
            if (height % bands == 0u && rows % 8u == 0u
                && tag_bytes % RECON_3D_WIDTH == 0u
                && tag_bytes + rows / 8u <= RECON_PLANE2_SIZE)
                b = bands;
        }
    }
    recon_bands_3d = b;
    recon_band_rows = height / b;
}

void recon_set_scale(unsigned n, int native)
{
    recon_app_n = n; recon_app_nat = native ? 1 : 0; recon_app_present = 1;
}

__attribute__((weak)) void recon_scale_3d_apply(void)
{
    if (recon_scale_3d_loaded) return;
    if (!recon_app_present) return;
    if (recon_app_n >= 2u && recon_app_n <= 8u) recon_scale_3d = recon_app_n;
    recon_scale_ceiling_set(recon_scale_3d);
    recon_bands_set();
    recon_scale_3d_loaded = 1;
}

__attribute__((weak)) void recon_native_apply(void)
{
    if (recon_native_loaded) return;
    recon_native_loaded = 1;
    recon_native = (recon_app_present && recon_app_nat) ? 1u : 0u;
}

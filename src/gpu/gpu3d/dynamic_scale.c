#include "hires_runtime.h"
#include "core_internals.h"
#include "gpu/gpu.h"
#include "gpu/gpu3d/gpu3d.h"
#include "frontend/video_out_gl.h"
#include "core/nds_state.h"

unsigned recon_scale_geometry = 2u;
unsigned recon_scale_out = 2u;
unsigned recon_scale_ceiling;
unsigned recon_block_header = 0x24000u;
unsigned long recon_block_stride = 0x24100u;
unsigned recon_scale_render_tag = 2u;
unsigned recon_page_scale_tag[2] = { 2u, 2u };
unsigned recon_page_latched = 0xffu;
unsigned recon_pacer_behind;

static unsigned pending_geometry;
static unsigned pending_render;
static unsigned render_ready;

#define DRS_WINDOW_MASK 0xffffu
#define DRS_WINDOW_MIN 8u
#define DRS_COOLDOWN 30u
#define DRS_DWELL_BASE 300u
#define DRS_DWELL_MAX 3600u
#define DRS_PROBE 300u
#define DRS_INHIBIT 120u

void recon_drs_reset(void);

static unsigned drs_enabled;
static unsigned drs_floor = 2u;
static unsigned drs_level;
static unsigned drs_cooldown;
static unsigned drs_dwell;
static unsigned drs_dwell_needed = DRS_DWELL_BASE;
static unsigned drs_probe_left;
static unsigned drs_inhibit = DRS_INHIBIT;
static uint32_t drs_window;
static unsigned drs_transitions;

static unsigned drs_level_scale(unsigned level)
{
    unsigned e = recon_scale_ceiling >> level;
    if (level != 0u)
        while (e & (e - 1u)) e &= e - 1u;
    return e < drs_floor ? drs_floor : e;
}

static unsigned drs_max_level(void)
{
    unsigned level = 0;
    while (drs_level_scale(level + 1u) < drs_level_scale(level)) level++;
    return level;
}

void recon_drs_set(unsigned enabled, unsigned floor)
{
    unsigned was = drs_enabled;
    drs_enabled = enabled ? 1u : 0u;
    drs_floor = (floor < 2u) ? 2u : (floor > 8u ? 8u : floor);
    recon_drs_reset();
    if (was && !drs_enabled) recon_scale_request(recon_scale_ceiling);
}

void recon_drs_reset(void)
{
    drs_level = 0;
    drs_cooldown = 0;
    drs_dwell = 0;
    drs_dwell_needed = DRS_DWELL_BASE;
    drs_probe_left = 0;
    drs_inhibit = DRS_INHIBIT;
    drs_window = 0;
}

unsigned recon_drs_transitions(void)
{
    return drs_transitions;
}

static void drs_step(unsigned behind, unsigned fast_forward)
{
    if (!drs_enabled || recon_scale_ceiling <= 2u) return;
    if (fast_forward) return;
    if (drs_inhibit) { drs_inhibit--; return; }

    drs_window = (drs_window << 1) | (behind & 1u);
    if (drs_cooldown) drs_cooldown--;
    unsigned debt = (unsigned)__builtin_popcount(drs_window & DRS_WINDOW_MASK) >= DRS_WINDOW_MIN;
    unsigned max_level = drs_max_level();
    unsigned level = drs_level;

    if (drs_probe_left) {
        drs_probe_left--;
        if (debt && level < max_level) {
            level++;
            drs_probe_left = 0;
            drs_cooldown = DRS_COOLDOWN;
            drs_dwell_needed = drs_dwell_needed * 2u > DRS_DWELL_MAX ? DRS_DWELL_MAX : drs_dwell_needed * 2u;
            drs_window = 0;
        } else if (drs_probe_left == 0) {
            drs_dwell_needed = DRS_DWELL_BASE;
        }
        drs_dwell = 0;
    } else if (debt && !drs_cooldown && level < max_level) {
        level++;
        drs_cooldown = DRS_COOLDOWN;
        drs_window = 0;
        drs_dwell = 0;
    } else if (!behind) {
        drs_dwell++;
        if (drs_dwell >= drs_dwell_needed && level > 0) {
            level--;
            drs_probe_left = DRS_PROBE;
            drs_dwell = 0;
            drs_window = 0;
        }
    } else {
        drs_dwell = 0;
    }

    if (level != drs_level) {
        drs_level = level;
        drs_transitions++;
    }
    unsigned target = drs_level_scale(drs_level);
    if (target != recon_scale_geometry) recon_scale_request(target);
}

static unsigned plane_bytes(unsigned e)
{
    unsigned v = 0x4000u * e * e;
    return v < 0x10000u ? 0x10000u : v;
}

static unsigned plane2_bytes(unsigned e)
{
    unsigned v = 0x1000u * e * e;
    if (v < 0x4000u) v = 0x4000u;
    return v * (e > 2u ? 2u : 1u);
}

unsigned recon_block_header_bytes(unsigned e)
{
    return 2u * plane_bytes(e) + plane2_bytes(e);
}

unsigned long recon_block_stride_bytes(unsigned e)
{
    return (unsigned long)recon_block_header_bytes(e) + 0x100u;
}

void recon_scale_ceiling_set(unsigned e)
{
    if (e < 2u) e = 2u;
    if (e > 8u) e = 8u;
    recon_scale_ceiling = e;
    recon_scale_geometry = e;
    recon_scale_out = e;
    recon_scale_render_tag = e;
    recon_page_scale_tag[0] = recon_page_scale_tag[1] = e;
    recon_block_header = recon_block_header_bytes(e);
    recon_block_stride = recon_block_stride_bytes(e);
    pending_geometry = 0;
    pending_render = 0;
    render_ready = 0;
}

void recon_scale_ceiling_read(void)
{
    if (recon_scale_ceiling != 0u) return;
    recon_scale_3d_read();
    if (recon_scale_ceiling == 0u) recon_scale_ceiling_set(recon_scale_3d);
}

unsigned recon_shadow_stride_max(void)
{
    recon_scale_ceiling_read();
    return recon_scale_ceiling > 2u ? recon_scale_ceiling * recon_scale_ceiling : 3u;
}

void recon_scale_request(unsigned n)
{
    recon_scale_ceiling_read();
    if (n < 2u) n = 2u;
    if (n > recon_scale_ceiling) n = recon_scale_ceiling;
    pending_geometry = n;
}

void recon_scale_note_swap(void)
{
    if (pending_render) {
        render_ready = pending_render;
        pending_render = 0;
    }
    if (pending_geometry) {
        recon_scale_geometry = pending_geometry;
        pending_render = pending_geometry;
        pending_geometry = 0;
    }
}

unsigned recon_scale_requested(void)
{
    return recon_scale_geometry;
}

void recon_scale_commit_render(void *gpu)
{
    unsigned n = render_ready;
    if (n == 0u) return;
    render_ready = 0;
    if (n == recon_scale_3d) return;
    recon_scale_3d = n;
    recon_bands_set();
    recon_height_reset();
    GPU3D_OF((gpu_t *)gpu)->render_dirty = 1;
}

void recon_scale_align_ceiling(void)
{
    recon_scale_ceiling_read();
    unsigned e = recon_scale_ceiling;
    pending_geometry = 0;
    pending_render = 0;
    render_ready = 0;
    recon_scale_geometry = e;
    recon_scale_3d = e;
    recon_scale_out = e;
    recon_scale_render_tag = e;
    recon_page_scale_tag[0] = recon_page_scale_tag[1] = e;
    recon_page_latched = 0xffu;
    recon_bands_set();
    recon_drs_reset();
}

void recon_scale_commit_output(unsigned e)
{
    recon_native_read();
    if (recon_native && recon_scale_pages && e > recon_scale_pages) e = recon_scale_pages;
    if (e == recon_scale_out) return;
    recon_scale_out = e;
    {
        gpu_output_t *o = GPU_OUTPUT_OF(&nds_machine.gpu);
        for (unsigned k = 0; k < 4u; k++) o->bank_texture_bits[k] = 0;
    }
    video_out_gl_t *g = VIDEO_OUT_GL;
    for (unsigned slot = 0; slot < 2u; slot++) {
        uint32_t mode = g->screen_mode[slot];
        unsigned n = (recon_native && mode != 0u) ? e : (mode == 0u ? 1u : 2u);
        g->screen_width[slot] = 256u * n;
        g->screen_height[slot] = 192u * n;
        g->screen_changed[slot] = 1;
    }
}

void recon_scale_frame_close(void *gpu)
{
    recon_scale_commit_render(gpu);
    drs_step(recon_pacer_behind, nds_machine.config.fast_forward);
}

unsigned recon_page_scale(void)
{
    extern unsigned recon_page_list(void);
    recon_page_latched = 0xffu;
    unsigned sel = recon_page_list() & 1u;
    recon_page_latched = sel;
    return recon_page_scale_tag[sel];
}

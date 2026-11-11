#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "present_hook.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"
#include "frontend/frontend.h"
#include "seedlessds/nds.h"
#include "seedlessds/control.h"
#include "wifi/wifi_core.h"

static int early_input_pacing;
static uint64_t early_wait_ticks;
static uint64_t early_wait_base;
static uint64_t early_wait_accumulated;
static uint64_t wireless_wait_deadline;
static uint64_t input_work_last;
static uint64_t input_work_base;

#define DEFAULT_STEP 50000u
#define FRAME_CYCLES (263u * 4260u)

static struct {
    const nds_t *owner;
    uint64_t frame;
    uint64_t cycles;
    uint32_t step;
    uint32_t remainder;
} frame_budget;

static uint32_t pacing_step(const nds_t *nds, int complete_frame, int external_pacing)
{
    uint32_t speed = nds->config.fast_forward_speed;
    if (speed || nds->config.fast_forward || nds->benchmark.enabled || external_pacing) {
        frame_budget.owner = NULL;
        return speed ? speed : DEFAULT_STEP;
    }

    const sched_t *sched = &nds->sched;
    uint64_t cycles = sched->cycles - frame_budget.cycles;
    int continuous = frame_budget.owner == nds && sched->cycles >= frame_budget.cycles
        && sched->frame - frame_budget.frame == (unsigned)complete_frame;
    if (complete_frame && (cycles < (FRAME_CYCLES + DEFAULT_STEP - 1) / DEFAULT_STEP
        || cycles > (uint64_t)INT32_MAX * FRAME_CYCLES / DEFAULT_STEP))
        continuous = 0;
    uint32_t step = continuous ? frame_budget.step : DEFAULT_STEP;
    if (complete_frame) {
        uint64_t scaled = continuous ? cycles * DEFAULT_STEP + frame_budget.remainder
                                     : (uint64_t)DEFAULT_STEP * FRAME_CYCLES;
        frame_budget.owner = nds;
        frame_budget.frame = sched->frame;
        frame_budget.cycles = sched->cycles;
        frame_budget.step = (uint32_t)(scaled / FRAME_CYCLES);
        frame_budget.remainder = (uint32_t)(scaled % FRAME_CYCLES);
    }
    return step;
}

static void input_work_reset(void)
{
    input_work_last = 0;
}


void nds_set_early_input_pacing(nds_t *nds, int enabled)
{
    (void)nds;
    early_input_pacing = enabled != 0;
    early_wait_ticks = 0;
    input_work_reset();
}




void pacer_init(unsigned char *ctx)
{

    nds_runtime_t *g = &((nds_t *)ctx)->runtime;

    gpu2d_worker_hook_noop();

    uint64_t now;
    time_now_microseconds(&now);

    g->pacer_flag = 0;
    wr32(&g->skip_frame, 0);

    g->pacer_base = now * 3u;
    g->pacer_accumulated = 0;
    frame_budget.owner = NULL;
    wireless_wait_deadline = 0;
    early_wait_ticks = 0;
    input_work_reset();
}

#define LATE_THRESHOLD  (-50001)

void pacer_set_wireless_deadline(uint8_t *ctx)
{
    if (!wifi_core_active()) return;
    nds_t *nds = (nds_t *)ctx;
    if (nds->config.fast_forward || nds->benchmark.enabled || nds_control_requires_fixed_pacing()) {
        frame_budget.owner = NULL;
        wifi_wm_frame(0);
        return;
    }
    uint64_t now;
    time_now_microseconds(&now);
    uint32_t elapsed = (uint32_t)(now * 3u - nds->runtime.pacer_base);
    uint32_t step = pacing_step(nds, 0, wifi_core_client_pacing());
    uint32_t remaining = (uint32_t)nds->runtime.pacer_accumulated + step - elapsed;
    if (remaining & 0x80000000u) remaining = 0;
    if (remaining > step) remaining = step;
    wifi_wm_frame(remaining / 3u);
}

static uint64_t wait_until(uint64_t deadline, int allow_guest)
{
    const nds_platform_t *platform = nds_platform_default();
    uint64_t now = time_monotonic_microseconds();
    while (now < deadline) {
        if (allow_guest && wifi_wm_needs_guest_cycles()) return deadline;
        uint64_t slice = deadline - now;
        if (wifi_wm_host_pending() && slice > 200u) slice = 200u;
        platform->threads.sleep_us(slice);
        wifi_wm_poll_host();
        now = time_monotonic_microseconds();
    }
    return 0;
}

void pacer_continue_wireless_wait(int final)
{
    if (wireless_wait_deadline)
        wireless_wait_deadline = wait_until(wireless_wait_deadline, !final);
}

static void pacer_sleep(uint64_t remaining, int allow_guest)
{
    const nds_platform_t *platform = nds_platform_default();
    if (!wifi_wm_host_pending()) {
        platform->threads.sleep_us(remaining);
        return;
    }
    wireless_wait_deadline = wait_until(time_monotonic_microseconds() + remaining, allow_guest);
}

void pacer_wait_before_input(uint8_t *ctx)
{
    if (!early_input_pacing) return;
    early_wait_ticks = 0;
    nds_t *nds = (nds_t *)ctx;
    nds_runtime_t *g = &nds->runtime;
    if (nds->config.fast_forward || nds->config.fast_forward_speed
        || nds->benchmark.enabled || g->pacer_flag
        || __atomic_load_n(&FRONTEND->pause_request, __ATOMIC_ACQUIRE)
        || __atomic_load_n(&FRONTEND->quit_request, __ATOMIC_ACQUIRE)
        || nds_control_pause_requested() || nds_control_requires_fixed_pacing()) {
        input_work_reset();
        return;
    }

    if (wifi_core_client_pacing()) {
        input_work_reset();
        recon_output_sample_input();
        return;
    }

    uint64_t clock;
    time_now_microseconds(&clock);
    if (clock > UINT64_MAX / 3u || clock * 3u < g->pacer_base) {
        input_work_reset();
        return;
    }
    if (input_work_last && (clock < input_work_last || input_work_base != g->pacer_base))
        input_work_reset();
    uint64_t frame_work = input_work_last ? clock - input_work_last
        : (DEFAULT_STEP + 2u) / 3u;
    uint64_t ref = clock * 3u - g->pacer_base;
    uint32_t step = pacing_step(nds, 0, 0);
    uint32_t deadline = (uint32_t)g->pacer_accumulated + step;
    early_wait_base = g->pacer_base;
    early_wait_accumulated = g->pacer_accumulated;
    uint32_t remaining = deadline - (uint32_t)ref;
    uint64_t nominal_wait = remaining <= step ? remaining / 3u : 0;
    if (nominal_wait > DEFAULT_STEP / 3u) nominal_wait = DEFAULT_STEP / 3u;
    uint32_t nominal_debt = remaining & 0x80000000u ? 0u - remaining : 0u;
    int waited = recon_wait_output_phase(frame_work, nominal_wait, nominal_debt, wifi_core_active());
    if (waited) {
        uint64_t after;
        time_now_microseconds(&after);
        if (after < clock || after > UINT64_MAX / 3u) {
            input_work_reset();
            return;
        }
        ref = after * 3u - g->pacer_base;
    }
    remaining = deadline - (uint32_t)ref;
    if (remaining && remaining <= step) {
        pacer_sleep(remaining / 3u, 0);
        waited = 1;
    }
    uint64_t after;
    time_now_microseconds(&after);
    if (after >= clock && after - clock <= UINT64_MAX / 3u) {
        if (waited) early_wait_ticks = (after - clock) * 3u;
        input_work_last = after;
        input_work_base = g->pacer_base;
        recon_output_sample_input();
    } else input_work_reset();
}



extern void time_now_microseconds(uint64_t *dest);

void pacer_run(uint8_t *ctx) {

    nds_config_t *p = &((nds_t *)ctx)->config;
    nds_runtime_t *g = &((nds_t *)ctx)->runtime;

    int client_pacing = wifi_core_client_pacing();
    uint32_t step = pacing_step((nds_t *)ctx, 1, client_pacing || nds_control_requires_fixed_pacing());

    uint32_t state  = nds_frame_limiter_measure_returns_zero();
    uint32_t mode = p->frameskip_type;
    uint32_t lim  = p->frameskip_value;

    uint64_t clock;

    time_now_microseconds(&clock);
    uint64_t ref = clock * 3u - g->pacer_base;
    uint64_t prepaid = g->pacer_base == early_wait_base
        && g->pacer_accumulated == early_wait_accumulated
        && ref >= early_wait_ticks ? early_wait_ticks : 0;
    early_wait_ticks = 0;
    ref -= prepaid;

    uint64_t acc = g->pacer_accumulated + step;
    g->pacer_accumulated = acc;

    if (client_pacing) {
        acc = ref;
        g->pacer_accumulated = acc;
        g->pacer_flag = 0;
    }

    uint32_t w20 = 0, w21 = 0, w8;

    if (p->fast_forward != 0 && (step >> 4) <= 0xc34u && ((nds_t *)ctx)->benchmark.enabled == 0) {
        mode = 1; lim = 6;
        if (step <= 1u) {
            acc = ref;
            g->pacer_accumulated = acc;
            goto L884;
        }
        if (g->pacer_flag != 0) goto L88c;
        goto La34;
    }

L884:
    recon_pacer_behind = ((int32_t)((uint32_t)acc - (uint32_t)ref) < 1) ? 1u : 0u;
    if (g->pacer_flag == 0) goto La34;

L88c:
    if (g->pacer_count != 0x14) {
        if (state == 0) goto L8f8;
        g->pacer_count = (uint8_t)(g->pacer_count + 1);
        w20 = 1;
        w21 = (uint32_t)acc - (uint32_t)ref;
        if ((int32_t)w21 >= 1) goto L928;
        goto L8c8;
    }
    nds_subsystem_hook_noop();
    acc = g->pacer_accumulated;

L8f8:
    {
        uint32_t d = (uint32_t)acc - (uint32_t)ref;
        d = (step + d - 1u) / step;
        acc = acc - (uint64_t)(uint32_t)(d * step);
        w20 = 0;
        g->pacer_flag = 0;
        g->pacer_accumulated = acc;
        w21 = (uint32_t)acc - (uint32_t)ref;
        if ((int32_t)w21 < 1) goto L8c8;
    }

L928:
    if (w20 == 0) {
        if (w21 > prepaid)
            pacer_sleep((w21 - prepaid) / 3u, 1);
        if (mode != 2) goto L8dc;
        goto L998;
    }
L92c:
    nds_platform_default()->threads.sleep_us(0);
    w20 = 1;
    if (mode != 2) goto L8dc;
    goto L998;

L8c8:
    if ((int32_t)w21 <= LATE_THRESHOLD) goto L92c;
    if (mode == 2) goto L998;

L8dc:
    if (mode == 1) goto L944;
    if (mode != 0) return;
    g->skip_frame = 0;
    if (w20 == 0) return;
    w8 = step + w21;
    goto L9f4;

L944:
    {
        uint32_t v = g->skip_count;
        uint32_t n = lim + 1u;
        uint32_t mark = (v != 0);

        uint32_t updated = (n == ((v + 1u) & 0xffu)) ? 0u : (v + 1u);
        g->skip_frame = (uint8_t)mark;
        g->skip_count = (uint8_t)updated;
        return;
    }

L998:
    {
        uint32_t v = g->skip_parity;
        uint32_t drop = rd32(&((nds_t *)ctx)->bus.io_mirror[0].dispcapcnt);
        uint32_t inv = v ^ 1u;
        g->skip_parity = (uint8_t)inv;

        if ((int32_t)drop < 0) {
            if ((inv & 0xffu) != 0) return;
            uint32_t c = g->skip_run;
            g->skip_run = 0;
            if (c >= 0x79u) { g->skip_frame = 0; return; }
            goto L9cc;
        }
        if (g->skip_run != 0xff) g->skip_run = (uint8_t)(g->skip_run + 1);
    }

L9cc:
    g->skip_frame = 0;
    if (w20 == 0) { g->skip_count = 0; return; }
    {
        uint32_t v = g->skip_count + 1u;
        uint32_t n = lim + 1u;
        g->skip_count = (uint8_t)v;
        if (n != (v & 0xffu)) { g->skip_frame = 1; return; }
        w8 = step + w21;
        g->skip_count = 0;
    }

L9f4:
    {
        uint64_t a = g->pacer_accumulated;
        uint32_t k = ((w8 - 1u) / step) * step;
        g->pacer_flag = 0;
        g->pacer_accumulated = a - (uint64_t)k;
    }
    return;

La34:

    if (state == 2) {
        w20 = 1;
        g->pacer_flag = 1;
        g->pacer_count = 0;
    } else {
        w20 = 0;
    }
    w21 = (uint32_t)acc - (uint32_t)ref;
    if ((int32_t)w21 >= 1) goto L928;
    goto L8c8;
}
#undef DEFAULT_STEP
#undef LATE_THRESHOLD

#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "mem_access.h"

static uint32_t bank_size(uint32_t control, uint32_t enabled) {
    uint32_t field = (control >> 1) & 0x1f;
    if (field == 0 || enabled == 0) return 0;
    return (uint32_t)512u << field;
}

static uint32_t bank_size_27(uint32_t control, uint32_t witness) {
    uint32_t field = (control >> 1) & 0x1f;
    if (field == 0 || witness == 0) return 0;
    return (uint32_t)512u << field;
}



static uint32_t bank_size_33900(uint32_t control, uint32_t witness) {
    uint32_t field = (control >> 1) & 0x1fu;
    if (field == 0 || witness == 0)
        return 0;
    return 0x200u << field;
}

int cp15_control_apply(cp15_t *cp) {
    uint32_t prev = cp->exception_vector_base;
    uint32_t ctrl   = cp->control;

    uint32_t b16 = (ctrl >> 16) & 1u;
    uint32_t b17 = (ctrl >> 17) & 1u;
    uint32_t b18 = (ctrl >> 18) & 1u;
    uint32_t b19 = (ctrl >> 19) & 1u;

    uint32_t s13 = (uint32_t)(((int32_t)(ctrl << 18)) >> 31);

    cp->exception_vector_base = s13 & 0xffff0000u;

    int remap1 = (b16 != cp->dtcm_enabled) ||
                 (b17 != cp->dtcm_load_mode);
    if (remap1) {
        cp->dtcm_enabled = b16;
        cp->dtcm_load_mode = b17;

        uint32_t c1  = cp->dtcm_region;
        uint32_t dir = c1 & 0xffff000u;
        uint32_t size = bank_size(c1, b16);
        cp->dtcm_size = size;
        uint32_t red = (size + 0xfffu) & 0xfffff000u;
        cp->dtcm_base = dir;
        pagetable_dtcm_remap(
            cp->bus, dir, red);
        cp->dtcm_below_64mb = (unsigned char)(((red + dir) >> 26) == 0);
    }

    int remap2 = (b18 != cp->itcm_enabled) ||
                 (b19 != cp->itcm_load_mode);
    if (remap2) {
        cp->itcm_enabled = b18;
        cp->itcm_load_mode = b19;

        uint32_t c2  = cp->itcm_region;
        uint32_t size = bank_size(c2, b18);
        cp->itcm_size = size;
        uint32_t red = (size + 0xfffu) & 0xfffff000u;
        pagetable_itcm_resize(cp->bus, red);
    }

    return cp->exception_vector_base != prev;
}

uint32_t cp15_dtcm_apply(cp15_t *cp) {
    void (*map)(bus_t *, uint32_t, uint32_t) = pagetable_dtcm_remap;

    uint32_t cfg = cp->dtcm_region;
    uint32_t exp = (cfg >> 1) & 0x1f;
    uint32_t base = cfg & 0x0ffff000u;

    uint32_t size = 0;
    if (exp != 0 && cp->dtcm_enabled != 0)
        size = 512u << (exp & 31);

    void *dest = cp->bus;
    cp->dtcm_size = size;
    uint32_t rounded = (size + 0xfff) & 0xfffff000u;
    cp->dtcm_base = base;
    map(dest, base, rounded);

    uint32_t flag = (((rounded + base) >> 26) == 0) ? 1 : 0;
    uint32_t before = cp->dtcm_below_64mb;
    cp->dtcm_below_64mb = (unsigned char)flag;
    return before ^ flag;
}

void cp15_bind(cp15_t *cp, arm_t *arm) {
    cp->arm = arm;
    cp->bus = *(struct bus *volatile *)&arm->pagetable.bus;
}

void cp15_reset(cp15_t *cp) {
    static const uint64_t template_reset[2] = { 0x00012078ffff0000ULL, 0x0000000000000000ULL };
    const uint64_t *template = template_reset;
    cp->dtcm_below_64mb = 0;
    memcpy(&cp->exception_vector_base, template, 16);

    cp15_control_apply(cp);

    uint32_t ctrl1 = cp->dtcm_region;
    uint32_t dir1  = ctrl1 & 0xffff000u;
    uint32_t size1  = bank_size_27(ctrl1, cp->dtcm_enabled);
    cp->dtcm_size = size1;
    uint32_t red1 = (size1 + 0xfffu) & 0xfffff000u;
    cp->dtcm_base = dir1;
    pagetable_dtcm_remap(
        cp->bus, dir1, red1);

    cp->dtcm_below_64mb = (unsigned char)(((red1 + dir1) >> 26) == 0);

    uint32_t ctrl2 = cp->itcm_region;
    uint32_t size2  = bank_size_27(ctrl2, cp->itcm_enabled);
    cp->itcm_size = size2;
    uint32_t red2 = (size2 + 0xfffu) & 0xfffff000u;

    pagetable_itcm_resize(cp->bus, red2);
}

void cp15_load_state(cp15_t *cp, unsigned char *state,
                        uint32_t limit) {

    void *p = rd_ptr(state + 32);
    cp->control = rd32(p);

    p = rd_ptr(state + 32);
    wr_ptr(state + 32, (unsigned char *)p + 4);
    cp->dtcm_region = rd32((unsigned char *)p + 4);

    p = rd_ptr(state + 32);
    wr_ptr(state + 32, (unsigned char *)p + 4);
    cp->itcm_region = rd32((unsigned char *)p + 4);

    p = rd_ptr(state + 32);
    wr_ptr(state + 32, (unsigned char *)p + 4);

    if (limit < 15)
        cp->control |= 0x50000u;

    cp15_control_apply(cp);

    uint32_t control = cp->itcm_region;
    uint32_t size = bank_size_33900(control, cp->itcm_enabled);
    cp->itcm_size = size;
    uint32_t rounded = (size + 0xfffu) & 0xfffff000u;
    pagetable_itcm_resize(cp->bus, rounded);

    control = cp->dtcm_region;
    uint32_t address = control & 0x0ffff000u;
    size = bank_size_33900(control, cp->dtcm_enabled);
    cp->dtcm_size = size;
    rounded = (size + 0xfffu) & 0xfffff000u;
    cp->dtcm_base = address;
    pagetable_dtcm_remap(
        cp->bus, address, rounded);

    cp->dtcm_below_64mb =
        (uint8_t)(((uint32_t)(rounded + address) >> 26) == 0);
}

void cp15_itcm_apply(cp15_t *cp) {
    void (*redo)(bus_t *, uint32_t) = pagetable_itcm_resize;

    uint32_t exp = (cp->itcm_region >> 1) & 0x1f;
    uint32_t size = 0;
    if (exp != 0 && cp->itcm_enabled != 0)
        size = 512u << (exp & 31);

    cp->itcm_size = size;
    redo(cp->bus, (size + 0xfff) & 0xfffff000u);
}

#ifndef SEEDLESS_WM_HOST_H
#define SEEDLESS_WM_HOST_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct seedless_wm;
typedef struct {
    void *owner;
    uint8_t *ram;
    uint32_t ram_mask;
    const uint8_t *firmware;
    const uint64_t *cycles, *frame;
    const uint16_t *vcount, *control[2];
    const char *directory;
    int jit;
    unsigned (*level)(void *, unsigned);
    uint32_t (*peek)(void *);
    void (*pop)(void *);
    void (*reply)(void *, uint32_t);
    void (*interrupt)(void *, unsigned, unsigned);
    void (*write8)(void *, uint32_t, uint8_t);
    void (*write16)(void *, uint32_t, uint16_t);
    void (*write32)(void *, uint32_t, uint32_t);
    void (*stop)(void *);
} seedless_wm_host;
struct seedless_wm *seedless_wm_create(const seedless_wm_host *host);
void seedless_wm_destroy(struct seedless_wm *wm);
void seedless_wm_poll(struct seedless_wm *wm);
void seedless_wm_vblank(struct seedless_wm *wm, uint64_t edge_us);
void seedless_wm_capture(struct seedless_wm *wm, uint32_t word);
void seedless_wm_clear(struct seedless_wm *wm);
int seedless_wm_blocks(struct seedless_wm *wm);
void seedless_wm_frame(struct seedless_wm *wm, uint32_t budget_us);
int seedless_wm_host_pending(struct seedless_wm *wm);
int seedless_wm_needs_guest_cycles(struct seedless_wm *wm);
void seedless_wm_poll_host(struct seedless_wm *wm);
void seedless_mp_bind_wm(void *owner);
#ifdef __cplusplus
}
#endif
#endif

#ifndef RECON_PRESENT_H
#include "hires_runtime.h"
#define RECON_PRESENT_H
#define RECON_PRESENT_PREDICATE       1u
#define RECON_PRESENT_EXT_NO_UPLOAD  2u

#define RECON_PRESENT_NO_UPLOAD      4u

#define RECON_PRESENT_EMU_PRIORITY   8u
extern int recon_present_nice_emu;
extern unsigned recon_present_mode;
extern unsigned recon_present_flips;
extern unsigned recon_present_seen;
void recon_present_set_mode(unsigned mode);
void recon_present_set_nice_emu(int nice);

extern __thread unsigned recon_present_fbo_dest;
void recon_present_set_fbo_dest(unsigned fbo);

extern void *(*recon_pages_reserve)(unsigned long size);

unsigned recon_page_list(void);
extern uint64_t recon_output_page_token[2];
extern __thread uint64_t recon_output_render_token;
void recon_output_publish_page(unsigned page);
void recon_output_sample_input(void);
int recon_wait_output_phase(uint64_t work_us, uint64_t nominal_wait_us, uint32_t nominal_debt_ticks, int allow_overlap);

#endif

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

#endif

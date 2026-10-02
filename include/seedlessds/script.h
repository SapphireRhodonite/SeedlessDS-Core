#ifndef SEEDLESSDS_SCRIPT_H
#define SEEDLESSDS_SCRIPT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nds nds_t;

#define NDS_SCRIPT_BUTTON_A 0
#define NDS_SCRIPT_BUTTON_B 1
#define NDS_SCRIPT_BUTTON_SELECT 2
#define NDS_SCRIPT_BUTTON_START 3
#define NDS_SCRIPT_BUTTON_RIGHT 4
#define NDS_SCRIPT_BUTTON_LEFT 5
#define NDS_SCRIPT_BUTTON_UP 6
#define NDS_SCRIPT_BUTTON_DOWN 7
#define NDS_SCRIPT_BUTTON_R 8
#define NDS_SCRIPT_BUTTON_L 9
#define NDS_SCRIPT_BUTTON_X 10
#define NDS_SCRIPT_BUTTON_Y 11
#define NDS_SCRIPT_BUTTON_TOUCH 12
#define NDS_SCRIPT_BUTTON_LID 13

#define NDS_SCRIPT_OVERRIDE_LAYOUT 0x00000001u
#define NDS_SCRIPT_OVERRIDE_SCREEN_SWAP 0x00004000u
#define NDS_SCRIPT_OVERRIDE_OVERLAY 0x40000000u

int nds_script_load(nds_t *nds, const char *path);
void nds_script_unload(nds_t *nds);
int nds_script_is_active(nds_t *nds);
void nds_script_on_frame(nds_t *nds);

uint32_t nds_script_get_overrides(nds_t *nds);
void nds_script_set_axis(nds_t *nds, const float *axes, size_t count);
void nds_script_set_rotation(nds_t *nds, float rotation);

#ifdef __cplusplus
}
#endif

#endif

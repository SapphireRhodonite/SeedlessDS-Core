#ifndef SEEDLESSDS_FRONTEND_SCRIPT_IO_H
#define SEEDLESSDS_FRONTEND_SCRIPT_IO_H

#include <stddef.h>
#include <stdint.h>

#define SCRIPT_IO_NONE 0xffffffffu

typedef struct script_io {
    float axis_lx;
    float axis_ly;
    float axis_rx;
    float axis_ry;
    int16_t rotation;
    uint8_t unmapped_0[6];
    uint32_t layout;
    uint32_t screen_swap;
    uint32_t overlay;
} script_io_t;

extern script_io_t script_io_state;

typedef uint32_t (*script_io_fn)(void *obj);

typedef struct script_io_entry {
    const char *name;
    script_io_fn fn;
} script_io_entry_t;

uint32_t script_io_get_axis_lx(void *obj);
uint32_t script_io_get_axis_ly(void *obj);
uint32_t script_io_get_axis_rx(void *obj);
uint32_t script_io_get_axis_ry(void *obj);
uint32_t script_io_get_rotation(void *obj);
uint32_t script_io_set_layout(void *obj);
uint32_t script_io_show_overlay(void *obj);
uint32_t script_io_set_screen_swap(void *obj);

#define SCRIPT_IO (&script_io_state)
#endif

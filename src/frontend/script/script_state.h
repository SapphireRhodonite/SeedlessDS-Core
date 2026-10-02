#ifndef SEEDLESSDS_FRONTEND_SCRIPT_STATE_H
#define SEEDLESSDS_FRONTEND_SCRIPT_STATE_H

#include <stddef.h>
#include <stdint.h>

struct nds;
struct nds_input_record;
struct bus;
struct lua_State;

typedef struct script_state {
    struct nds *machine;
    struct nds_input_record *input_record;
    struct bus *bus;
    struct lua_State *lua;
    uint8_t loaded;
    uint8_t on_load_called;
    uint8_t unmapped_0[6];
    void *on_frame_update;
    uint8_t running;
    uint8_t unmapped_1[7];
} script_state_t;

extern script_state_t script_state;

#define SCRIPT_STATE (&script_state)

#endif

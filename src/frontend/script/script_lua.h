#ifndef SEEDLESSDS_FRONTEND_SCRIPT_SCRIPT_LUA_H
#define SEEDLESSDS_FRONTEND_SCRIPT_SCRIPT_LUA_H

unsigned int script_lua_get_path(void);
uint32_t script_lua_get_buttons(unsigned char *state);
uint32_t script_lua_set_buttons(void *state);
uint32_t script_lua_get_touch(void *state);
uint32_t script_lua_set_touch(void *state);
uint32_t script_lua_get_ds_memory_arm9_8(void *state);
uint32_t script_lua_get_ds_memory_arm9_16(void *state);
uint32_t script_lua_get_ds_memory_arm9_32(void *state);
uint32_t script_lua_get_ds_memory_arm7_8(void *state);
uint32_t script_lua_get_ds_memory_arm7_16(void *state);
uint32_t script_lua_get_ds_memory_arm7_32(void *state);
uint32_t script_lua_set_ds_memory_arm9_8(void *state);
uint32_t script_lua_set_ds_memory_arm9_16(void *state);
uint32_t script_lua_set_ds_memory_arm9_32(void *state);
uint32_t script_lua_set_ds_memory_arm7_8(void *state);
uint32_t script_lua_set_ds_memory_arm7_16(void *state);
uint32_t script_lua_set_ds_memory_arm7_32(void *state);

#endif

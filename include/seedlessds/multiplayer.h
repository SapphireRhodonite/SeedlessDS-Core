#ifndef SEEDLESSDS_MULTIPLAYER_H
#define SEEDLESSDS_MULTIPLAYER_H

#include <stddef.h>
#include <stdint.h>
#include "wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEEDLESS_MP_HOST 1
#define SEEDLESS_MP_JOIN 2
#define SEEDLESS_MP_DISCOVER 3

int seedless_mp_start(int mode, const char *name, const char *address,
                      const uint8_t mac[6]);
int seedless_mp_start_maximum(int mode, const char *name, const char *address,
                              const uint8_t mac[6], int maximum);
int seedless_mp_start_direct(int mode, const char *name, uint32_t local, uint32_t owner,
                            unsigned interface_index, const uint8_t mac[6], int maximum);
int seedless_mp_bind_interface(int socket_fd, unsigned interface_index);
int seedless_mp_stop(void);
int seedless_mp_pause(int paused);
size_t seedless_mp_snapshot(char *output, size_t capacity);
int seedless_mp_enabled(void);
int seedless_mp_game_started(int firmware_boot);
void seedless_mp_game_stopped(void);
void seedless_mp_mac(uint8_t mac[6]);
void seedless_mp_bind(seedless_wifi_callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif
#endif

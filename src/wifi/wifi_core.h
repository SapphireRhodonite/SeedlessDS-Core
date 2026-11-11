#ifndef SEEDLESSDS_WIFI_CORE_H
#define SEEDLESSDS_WIFI_CORE_H

#include <stdint.h>

struct nds;
int wifi_core_active(void);
void wifi_wm_poll(void);
void wifi_wm_vblank(uint64_t edge_us);
void wifi_wm_capture(uint32_t word);
int wifi_wm_blocks(void);
void wifi_wm_clear(void);
void wifi_wm_frame(uint32_t budget);
int wifi_wm_host_pending(void);
int wifi_wm_needs_guest_cycles(void);
void wifi_wm_poll_host(void);
int wifi_core_client_pacing(void);
void wifi_core_flush(void);
void wifi_core_prepare_firmware(uint8_t *firmware);
int wifi_core_reset(struct nds *machine);
void wifi_core_release(void);
uint16_t wifi_core_read(uint32_t address);
void wifi_core_write(uint32_t address, uint16_t value);
void wifi_core_power(uint32_t value);

#endif

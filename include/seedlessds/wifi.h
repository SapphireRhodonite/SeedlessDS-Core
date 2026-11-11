#ifndef SEEDLESSDS_WIFI_H
#define SEEDLESSDS_WIFI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct seedless_wifi_callbacks {
    void *context;
    const uint8_t *firmware;
    void (*schedule)(void *context, uint32_t arm7_cycles, int first);
    void (*cancel)(void *context);
    void (*irq)(void *context);
    void *transport;
    void (*begin)(void *transport);
    void (*end)(void *transport);
    int (*send)(void *transport, unsigned type, const uint8_t *data, int length,
                uint64_t timestamp, uint16_t aid);
    int (*receive)(void *transport, int host, uint8_t *data, uint64_t *timestamp);
    uint16_t (*replies)(void *transport, uint8_t *data, uint64_t timestamp,
                        uint16_t aid_mask);
    int (*conservative_ack)(void *transport);
} seedless_wifi_callbacks_t;

typedef struct seedless_wifi seedless_wifi_t;

seedless_wifi_t *seedless_wifi_create(const seedless_wifi_callbacks_t *callbacks);
void seedless_wifi_destroy(seedless_wifi_t *wifi);
void seedless_wifi_reset(seedless_wifi_t *wifi);
void seedless_wifi_power(seedless_wifi_t *wifi, uint32_t power);
void seedless_wifi_tick(seedless_wifi_t *wifi);
void seedless_wifi_flush(seedless_wifi_t *wifi);
void seedless_wifi_sync(seedless_wifi_t *wifi);
int seedless_wifi_is_client(const seedless_wifi_t *wifi);
uint16_t seedless_wifi_read(seedless_wifi_t *wifi, uint32_t address);
void seedless_wifi_write(seedless_wifi_t *wifi, uint32_t address, uint16_t value);
void seedless_wifi_default_calibration(uint8_t *firmware);

#ifdef __cplusplus
}
#endif
#endif

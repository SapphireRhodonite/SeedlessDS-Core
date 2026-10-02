#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include "spi.h"
#include "firmware.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include "mem_access.h"

uint32_t firmware_crc16(uint32_t acc, const unsigned char *data,
                            uint32_t count)
{
    if (count == 0)
        return acc;

    const uint32_t c0 = 0x00606080u;
    const uint32_t c1 = 0x00306040u;
    const uint32_t c2 = 0x00186020u;
    const uint32_t c3 = 0x000c6010u;
    const uint32_t c4 = 0x00066008u;
    const uint32_t c5 = 0x00036004u;
    const uint32_t c6 = 0x0001e002u;
    const uint32_t c7 = 0x0000a001u;

    do {
        uint32_t x = (uint32_t)*data;
        data++;
        x ^= acc;

        { uint32_t t = x >> 1; x = (x & 1u) ? (t ^ c0) : t; }
        { uint32_t t = x >> 1; x = (x & 1u) ? (t ^ c1) : t; }
        { uint32_t t = x >> 1; x = (x & 1u) ? (t ^ c2) : t; }
        { uint32_t t = x >> 1; x = (x & 1u) ? (t ^ c3) : t; }
        { uint32_t t = x >> 1; x = (x & 1u) ? (t ^ c4) : t; }
        { uint32_t t = x >> 1; x = (x & 1u) ? (t ^ c5) : t; }
        { uint32_t t = x >> 1; x = (x & 1u) ? (t ^ c6) : t; }
        { uint32_t t = x >> 1; acc = (x & 1u) ? (t ^ c7) : t; }

        count--;
    } while (count != 0);

    return acc;
}

void *firmware_header_build(firmware_header_t *h) {

    static const unsigned char mac_and_channels[8] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0xfe, 0x3f };
    memcpy(h->mac_address, mac_and_channels, 6);
    memcpy(&h->wifi_enabled_channels, mac_and_channels + 6, 2);
    memcpy(h->identifier, "MACP", 4);
    h->console_type = 32;
    h->user_settings_offset = 0x7fc0;
    h->wifi_data_length = 312;
    h->unmapped_4 = 0xffff;
    h->rf_type = 2;
    h->rf_bits_per_entry = 0x18;
    h->rf_entry_count = 12;
    h->unmapped_5 = 1;
    h->wifi_calibration_rest[0x162 - 0x44] = 25;

    memset(h->wifi_calibration_rest + (0x1f0 - 0x44), 0xFF, 16);
    memset(h->wifi_calibration_rest + (0x163 - 0x44), 0xFF, 144);

    static const uint32_t K[8] = {
        0x00606080u, 0x00306040u, 0x00186020u, 0x000C6010u,
        0x00066008u, 0x00036004u, 0x0001E002u, 0x0000A001u,
    };
    const uint8_t *bytes = (const uint8_t *)h;
    uint32_t crc = 0;
    for (uint32_t i = FIRMWARE_WIFI_CALIBRATION_START; i != FIRMWARE_WIFI_CALIBRATION_END; i++) {
        crc ^= bytes[i];
        for (int k = 0; k < 8; k++) {
            uint32_t shifted = crc >> 1;

            crc = (crc & 1u) ? (shifted ^ K[k]) : shifted;
        }
    }
    h->wifi_crc16 = (uint16_t)crc;
    return h;
}

typedef size_t (*fn_wcslen)(const uint32_t *);

static const uint32_t POLY[8] = {
    0x606080u, 0x306040u, 0x186020u, 0xc6010u,
    0x66008u,  0x36004u,  0x1e002u,  0xa001u
};


static void decode_block(firmware_user_settings_t *b, const uint32_t *str, const uint16_t *conv,
                   size_t len, const uint8_t *text)
{
    b->version = 5;
    b->favorite_color = (uint8_t)rd32((const uint8_t *)str + 48);
    b->birthday_month = (uint8_t)(((const uint8_t *)str)[52] + 1);
    b->birthday_day = (uint8_t)rd32((const uint8_t *)str + 56);
    b->unmapped_0 = 0;

    memcpy(b->nickname, conv, sizeof b->nickname);
    b->nickname_length = (uint16_t)len;

    memcpy(b->message, text, 48);
    b->message[24] = 0x65;
    b->message[25] = 0;

    b->message_length = 0x19;
    b->alarm_hour = 0;
    b->alarm_minute = 0;
    b->unmapped_1[0] = 0;
    b->unmapped_1[1] = 0;
    b->alarm_flags = 0;
    b->touch_adc_x1 = 0x0200;
    b->touch_adc_y1 = 0x0200;
    b->touch_screen_x1 = 0x20;
    b->touch_screen_y1 = 0x20;
    b->touch_adc_x2 = 0x0e00;
    b->touch_adc_y2 = 0x0800;
    b->touch_screen_x2 = 0xe0;
    b->touch_screen_y2 = 0x80;
    b->language_flags = (uint16_t)rd32((const uint8_t *)str + 44);
    b->rtc_offset = 0;
    b->unmapped_2 = 0xff;
    b->update_counter = 0;

    const uint8_t *bytes = (const uint8_t *)b;
    uint32_t crc = 0xffffu;
    for (int i = 0; i < 0x70; i++) {
        crc ^= bytes[i];
        for (int k = 0; k < 8; k++) {
            uint32_t d = crc >> 1;
            crc = (crc & 1) ? (d ^ POLY[k]) : d;
        }
    }
    b->crc16 = (uint16_t)crc;

    memset(b->extended, 0, sizeof b->extended);
}

void firmware_user_settings_build(const uint32_t *str, uint8_t *image) {

    fn_wcslen p_wcslen = (fn_wcslen)sym_libc_wcslen;
    size_t len = p_wcslen(str);
    uint32_t n = (uint32_t)len;

    uint16_t conv[40];
    memset(conv, 0, sizeof conv);
    for (uint32_t i = 0; i < n && i < 39; i++)
        conv[i] = (uint16_t)str[i];
    conv[n < 40 ? n : 39] = 0;

    static const uint8_t default_message[48] = {
        0x64, 0x00, 0x65, 0x00, 0x73, 0x00, 0x70, 0x00, 0x65, 0x00, 0x72, 0x00,
        0x61, 0x00, 0x74, 0x00, 0x65, 0x00, 0x20, 0x00, 0x64, 0x00, 0x72, 0x00,
        0x61, 0x00, 0x73, 0x00, 0x74, 0x00, 0x69, 0x00, 0x63, 0x00, 0x20, 0x00,
        0x6d, 0x00, 0x65, 0x00, 0x61, 0x00, 0x73, 0x00, 0x75, 0x00, 0x72, 0x00,
    };
    const uint8_t *text = default_message;

    decode_block((firmware_user_settings_t *)(image + FIRMWARE_USER_SETTINGS_1), str, conv, len, text);
    decode_block((firmware_user_settings_t *)(image + FIRMWARE_USER_SETTINGS_2), str, conv, len, text);
}

void firmware_mount(spi_t *spi, nds_t *machine) {

    spi->machine = machine;

    char path[0x830];
    firmware_build_path(path, (long)machine, 0, 0, (uint8_t *)((nds_t *)machine)->save_dir, '/', '/');

    spi_memory_mount(&spi->firmware, 1, machine->bus.firmware_image, FIRMWARE_IMAGE_SIZE, 0, 1, 0);
    spi_memory_set_path(&spi->firmware, path);
}

static int (*core_vsnprintf)(char *, size_t, const char *, va_list);

int firmware_build_path(char *dest, long a, long b, long c, ...) {
    (void)a; (void)b; (void)c;
    if (!core_vsnprintf)
        core_vsnprintf = (int (*)(char *, size_t, const char *, va_list))
                         sym_libc_vsnprintf;

    const char *format = "%s%csystem%cnds_firmware_modified.bin";
    va_list ap;
    va_start(ap, c);
    int r = core_vsnprintf(dest, 2080, format, ap);
    va_end(ap);
    return r;
}

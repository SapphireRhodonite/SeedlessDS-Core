#ifndef SEEDLESSDS_FIRMWARE_H
#define SEEDLESSDS_FIRMWARE_H

#include <stddef.h>
#include <stdint.h>

#define FIRMWARE_USER_SETTINGS_1 0x3fe00u
#define FIRMWARE_USER_SETTINGS_2 0x3ff00u
#define FIRMWARE_WIFI_CALIBRATION_START 0x2cu
#define FIRMWARE_WIFI_CALIBRATION_END 0x164u

typedef struct firmware_header {
    uint16_t part3_rom_address;
    uint16_t part4_rom_address;
    uint16_t part3_crc16;
    uint16_t part5_crc16;
    char identifier[4];
    uint8_t unmapped_0[0x1d - 0x0c];
    uint8_t console_type;
    uint8_t unmapped_1[0x20 - 0x1e];
    uint16_t user_settings_offset;
    uint8_t unmapped_2[0x2a - 0x22];
    uint16_t wifi_crc16;
    uint16_t wifi_data_length;
    uint8_t unmapped_3[0x36 - 0x2e];
    uint8_t mac_address[6];
    uint16_t wifi_enabled_channels;
    uint16_t unmapped_4;
    uint8_t rf_type;
    uint8_t rf_bits_per_entry;
    uint8_t rf_entry_count;
    uint8_t unmapped_5;
    uint8_t wifi_calibration_rest[0x200 - 0x44];
} firmware_header_t;

typedef struct firmware_user_settings {
    uint16_t version;
    uint8_t favorite_color;
    uint8_t birthday_month;
    uint8_t birthday_day;
    uint8_t unmapped_0;
    uint16_t nickname[10];
    uint16_t nickname_length;
    uint16_t message[26];
    uint16_t message_length;
    uint8_t alarm_hour;
    uint8_t alarm_minute;
    uint8_t unmapped_1[2];
    uint16_t alarm_flags;
    uint16_t touch_adc_x1;
    uint16_t touch_adc_y1;
    uint8_t touch_screen_x1;
    uint8_t touch_screen_y1;
    uint16_t touch_adc_x2;
    uint16_t touch_adc_y2;
    uint8_t touch_screen_x2;
    uint8_t touch_screen_y2;
    uint16_t language_flags;
    uint16_t birth_year;
    uint32_t rtc_offset;
    uint32_t unmapped_2;
    uint16_t update_counter;
    uint16_t crc16;
    uint8_t unmapped_3[0x78 - 0x74];
    uint8_t extended[0xfe - 0x78];
    uint8_t unmapped_4[0x100 - 0xfe];
} firmware_user_settings_t;
#endif

#ifndef SEEDLESSDS_CART_H
#define SEEDLESSDS_CART_H

#include <stddef.h>
#include <stdint.h>
#include "../spi/spi.h"
#include "../dma.h"
#include "../core/gamedb.h"
#include "../frontend/cheats/cheats.h"
struct nds;
struct io_mirror;

#define CART_SECURE_AREA_SIZE 0x4000u
#define CART_KEY1_TABLE_WORDS 0x412u
#define CART_HEADER_SIZE 0x200u
#define CART_CHIP_ID_SIZE_FIELD 0xff00u
#define NDS_ROM_SECURE_AREA_OFFSET 0x4000u
#define NDS_ROM_ARM7_MIN_OFFSET 0x8000u
#define NDS_ROM_LOAD_LIMIT 0x003bfe00u
typedef struct cart_regs {
    uint16_t auxspicnt;
    uint16_t auxspidata;
    uint32_t romctrl;
    uint8_t command[8];
} cart_regs_t;

typedef struct rom_image {
    uint8_t unmapped_0[8];
    const uint8_t *data;
    uint32_t size;
    uint32_t secure_limit;
} rom_image_t;

typedef struct slot2_sensor {
    uint8_t phase;
    uint8_t index;
    uint8_t accel8_x;
    uint8_t accel8_y;
    uint8_t accel8_z;
    uint8_t unmapped_0;
    uint8_t phase16;
    uint8_t index16;
    uint16_t accel16_x;
    uint16_t accel16_y;
    uint16_t accel16_z;
    uint16_t gyro16;
    uint16_t ident;
    uint8_t unmapped_1[6];
} slot2_sensor_t;

typedef struct slot2_gpio {
    void *config;
    uint8_t bit1;
    uint8_t countdown;
    uint8_t unmapped_0[6];
} slot2_gpio_t;

typedef struct slot2 {
    char path[0x420];
    void *file;
    uint8_t *rom;
    uint8_t *save;
    uint32_t rom_size;
    uint32_t save_size;
    uint32_t flash_bank;
    uint32_t save_countdown;
    uint8_t backup_type;
    uint8_t backup_command;
    uint8_t backup_phase;
    uint8_t loaded;
    uint8_t unmapped_1[4];
    slot2_sensor_t sensor;
    slot2_gpio_t gpio;
} slot2_t;

typedef struct cart {
    game_database_t database[2];
    cheats_t cheats;
    slot2_t slot2;
    uint8_t secure_area[CART_SECURE_AREA_SIZE];
    uint32_t key1_table[CART_KEY1_TABLE_WORDS];
    uint32_t keycode[3];
    uint32_t unmapped_1;
    uint64_t transfer_deadline_cycles;
    struct nds *machine;
    rom_image_t *rom;
    struct io_mirror *mirror;
    uint32_t rom_mask;
    uint32_t unmapped_2;
    uint32_t game_code;
    int32_t argv_fd;
    uint32_t write_words_remaining;
    uint32_t unmapped_3;
    dma_t *dma9;
    dma_t *dma7;
    uint32_t chip_id;
    uint32_t read_address;
    uint32_t words_remaining;
    uint32_t unmapped_4;
    spi_memory_t backup;
    uint8_t *backup_data;
    uint32_t data_word;
    uint32_t backup_base;
    uint32_t backup_address;
    uint32_t key1_id1;
    uint32_t key1_id2;
    uint8_t transfer_mode;
    uint8_t irq_pending;
    uint8_t secure_area_pending;
    uint8_t backup_write_mode;
    uint8_t backup_write_enabled;
    uint8_t key1_active;
    uint8_t use_user_database;
    uint8_t unmapped_6[0x7e50 - 0x7e4b];
} cart_t;
struct bus;
uint32_t cart_queue_pop_pair(struct bus *bus);

#endif

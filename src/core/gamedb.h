#ifndef SEEDLESSDS_CORE_GAMEDB_H
#define SEEDLESSDS_CORE_GAMEDB_H

#include <stddef.h>
#include <stdint.h>

#define GAME_DATABASE_CODE_BYTES 13
#define GAME_DATABASE_INITIAL_CAPACITY 0x20u
#define GAME_DATABASE_CODE_MASK 0xffffffu

typedef struct game_database_entry {
    char *title;
    char code[GAME_DATABASE_CODE_BYTES];
    uint8_t unmapped_0[3];
    uint32_t rom_size;
    uint32_t rom_crc;
    uint32_t save_size;
    uint32_t game_code;
    uint32_t save_id;
    uint32_t features;
    uint8_t save_type;
    uint8_t unmapped_1[7];
} game_database_entry_t;

typedef struct game_database {
    game_database_entry_t *entries;
    game_database_entry_t **by_crc;
    game_database_entry_t **by_code;
    int64_t file_time;
    uint32_t count;
    uint32_t unmapped_0;
} game_database_t;

typedef struct game_database_key {
    game_database_entry_t *entry;
    game_database_entry_t fake;
} game_database_key_t;
#endif

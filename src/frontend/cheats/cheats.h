#ifndef SEEDLESSDS_FRONTEND_CHEATS_H
#define SEEDLESSDS_FRONTEND_CHEATS_H

#include <stddef.h>
#include <stdint.h>

#define CHEATS_DATABASE_PATH_BYTES 0x400
#define CHEAT_FOLDER_EXCLUSIVE 17u
#define CHEAT_DATABASE_FOLDER_FLAG 0x10000000u
#define CHEAT_DATABASE_COUNT_MASK 0x0fffffffu
#define CHEAT_DATABASE_WORDS_MASK 0x00ffffffu
#define CHEAT_FOLDER_NONE 0xffffffffu

typedef struct cheat_index_entry {
    uint32_t game_code;
    uint32_t crc;
    uint32_t offset;
    uint32_t size;
} cheat_index_entry_t;

typedef struct cheat_entry {
    int32_t *codes;
    char *name;
    char *description;
    uint32_t code_words;
    uint32_t folder;
    uint8_t *enabled;
} cheat_entry_t;

typedef struct cheat_folder {
    char *name;
    char *description;
    uint32_t cheat_count;
    uint32_t unmapped_0;
    uint8_t *enabled;
    uint8_t type;
    uint8_t unmapped_1[7];
} cheat_folder_t;

typedef struct cheats {
    char database_path[CHEATS_DATABASE_PATH_BYTES];
    cheat_index_entry_t *index;
    uint32_t index_count;
    uint32_t unmapped_0;
    cheat_entry_t *user_entries;
    uint8_t *user_flags;
    uint32_t user_count;
    uint32_t unmapped_1;
    cheat_entry_t **active;
    uint32_t active_count;
    uint32_t index_loaded;
    uint32_t database_loaded;
    uint32_t unmapped_2;
    cheat_index_entry_t *entry;
    uint8_t *data;
    uint8_t *loaded_data;
    cheat_entry_t *entries;
    cheat_folder_t *folders;
    uint32_t folder_count;
    uint32_t entry_count;
    uint8_t unmapped_3[8];
} cheats_t;
#endif

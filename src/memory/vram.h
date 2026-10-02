#ifndef SEEDLESSDS_VRAM_H
#define SEEDLESSDS_VRAM_H

#include <stddef.h>
#include <stdint.h>

struct bus;

#define VRAM_BANKS 9u
#define VRAM_SLOTS 0x400u
#define VRAM_SLOT_SHIFT 14u
#define VRAM_MAP_SIZE 0x2e78u

enum vram_map_kind {
    VRAM_MAP_BG_EXT_PALETTE_A = 0,
    VRAM_MAP_BG_EXT_PALETTE_B = 1,
    VRAM_MAP_OBJ_EXT_PALETTE_A = 2,
    VRAM_MAP_OBJ_EXT_PALETTE_B = 3,
    VRAM_MAP_TEXTURE = 4,
    VRAM_MAP_TEXTURE_PALETTE = 5,
    VRAM_MAP_SLOTS = 6,
    VRAM_MAP_ARM7 = 7,
    VRAM_MAP_SLOTS_TWO_BLOCKS = 8,
    VRAM_MAP_SLOTS_BANK_I = 9,
    VRAM_MAP_SLOTS_BANK_H = 10,
    VRAM_MAP_RESET = 11,
    VRAM_MAP_NONE = 12
};

typedef struct vram_bank {
    uint32_t map_kind;
    uint32_t control;
    uint32_t first_slot;
    uint32_t mapped_kb;
} vram_bank_t;

struct config;

typedef struct vram_map {
    struct bus *bus;
    struct nds_config *config;
    vram_bank_t bank[VRAM_BANKS];
    uint8_t *slot_base[VRAM_SLOTS];
    uint8_t *bank_data[VRAM_BANKS];
    const uint8_t *bank_control_reg[VRAM_BANKS];
    uint8_t *bg_ext_palette[2][4];
    uint8_t *obj_ext_palette[2];
    uint8_t *texture[4];
    uint8_t *texture_palette[6];
    uint8_t *arm7_vram[2];
    uint32_t slot_touched[16];
    uint32_t slot_touched_groups;
    uint16_t slot_banks[VRAM_SLOTS];
    uint8_t slot_tag[VRAM_SLOTS];
    uint16_t bg_ext_palette_banks[2][4];
    uint16_t obj_ext_palette_banks[2];
    uint16_t texture_banks[4];
    uint16_t texture_palette_banks[6];
    uint16_t arm7_vram_banks[2];
    uint8_t *engine_palette[2];
    uint8_t *engine_oam[2];
    uint16_t changed_banks;
    uint8_t unmapped_2[VRAM_MAP_SIZE - 0x2e72];
} vram_map_t;
void vram_reg_cache_write8(uint8_t *state, uint32_t dir, uint32_t value);
void vram_reg_cache_write16(uint8_t *state, unsigned dir, unsigned value);
void vram_reg_cache_write32(unsigned char *machine, uint32_t dir, uint32_t value);
#endif

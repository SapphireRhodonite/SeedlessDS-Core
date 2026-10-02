#ifndef SEEDLESSDS_BUS_H
#define SEEDLESSDS_BUS_H

#include <stddef.h>
#include <stdint.h>

#define PAGETABLE_PAGE_SHIFT 11
#define BUS_REGION_SHIFT 23
#define BUS_REGION_MASK 0x7fffffu
#define BUS_REGION_SIZE (1u << BUS_REGION_SHIFT)
#define BUS_REGION_PAGE_BITS (BUS_REGION_SIZE >> PAGETABLE_PAGE_SHIFT >> 3)
#define BUS_REGION_CODE_BITS (BUS_REGION_SIZE >> 4)
#define BUS_VRAM_BITMAP_WORDS 0xbu
#define BUS_SHARED_ITCM 0x400000u
#define BUS_SHARED_WRAM 0x408000u
#define BUS_SHARED_DTCM 0x410000u
#define NDS_MAIN_RAM_BASE 0x02000000u
#define NDS_MAIN_RAM_SIZE 0x400000u
#define NDS_SHARED_WRAM_BASE 0x03000000u
#define NDS_SHARED_WRAM_HALF_BYTES 0x4000u
#define NDS_ARM7_WRAM_BASE 0x037f8000u
#define NDS_ARM7_WRAM_WINDOW_BYTES 0xfe00u
#define NDS_ADDRESS_SPACE_BYTES 0x100000000ULL
#define BUS_SHARED_WRAM_SLOT_BYTES 0x10000u
#define BUS_UNMAP_SPAN_BYTES 0x1000000u
#define BUS_ADDRESS_WINDOW_BYTES 0x4000000u
#define BUS_ADDRESS_WINDOW_FIRST_PAGE 0x8000u
#define PAGETABLE_TOP_BLOCK_BYTES 0x10000u
#define NDS_MAIN_RAM_CHIP_ID 0x3ff800u
#define NDS_MAIN_RAM_CHIP_ID_MIRROR 0x3ffc00u
#define NDS_MAIN_RAM_HEADER 0x3ffe00u
#define NDS_VRAM_BASE 0x06000000u
#define BUS_ARM9_REGIONS 32
#define BUS_ARM7_REGIONS 32
#define BUS_REGIONS (BUS_ARM9_REGIONS + BUS_ARM7_REGIONS)

#define BUS_REGION_DIRECT 0
#define BUS_REGION_TRANSLATED 1
#define BUS_REGION_HANDLED 2
#define BUS_REGION_IGNORED 3
#define BUS_SLOT2_REGION 16

typedef struct bus_region {
    uint32_t mask;
    uint32_t unmapped_0;
    union {
        uint8_t *read_memory;
        void *read_translate;
        void *read8;
    };
    void *read16;
    void *read32;
    union {
        uint8_t *write_memory;
        void *write_translate;
        void *write8;
    };
    void *write16;
    void *write32;
    void *page_bits;
    void *code_bits;
    void *bitmap_lookup[2];
    uint8_t read_kind;
    uint8_t write_kind;
    uint8_t unmapped_2[6];
} bus_region_t;

#define PAGETABLE_PAGES 0x200000u
#define PAGETABLE_MISSING UINT64_C(0x4000000000000000)
#define PAGETABLE_GROUP_PAGES 0x400u
#define PAGETABLE_GROUP_BIT_WORDS 0x20u
#define PAGETABLE_GROUPS_BELOW_MAIN_RAM 0x10u
#define PAGETABLE_SIZE 0x1004220u
#define NDS_JIT_ARENA_OFFSET 0x94000u
#define JIT_ARENA_SIZE (0x158f000u - NDS_JIT_ARENA_OFFSET)

typedef struct pagetable {
    uint64_t page[PAGETABLE_PAGES];
    bus_region_t *region;
    struct bus *bus;
    struct arm *cpu;
    uint32_t page_bits[0x1000];
    uint32_t group_bits[0x80];
    uint8_t unmapped_0[PAGETABLE_SIZE - 0x1004218u];
} pagetable_t;
#endif

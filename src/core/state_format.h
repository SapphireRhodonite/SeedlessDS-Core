#ifndef SEEDLESSDS_STATE_FORMAT_H
#define SEEDLESSDS_STATE_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#define STATE_MAGIC "SDS2"
#define STATE_FORMAT_VERSION 1u
#define STATE_STREAM_VERSION 15u

#define STATE_FLAG_COMPRESSED 0x01u
#define STATE_FLAG_SCREENS 0x02u
#define STATE_FLAG_BIOS_ARM7_REPLACEMENT 0x04u
#define STATE_FLAG_BIOS_ARM9_REPLACEMENT 0x08u
#define STATE_FLAG_SPI_MEMORY 0x10u
#define STATE_FLAG_CURRENT 0x20u

enum state_section_id {
    STATE_SECTION_SCREENS = 1,
    STATE_SECTION_SPI_MEMORY = 2,
    STATE_SECTION_ARM9 = 3,
    STATE_SECTION_ARM7 = 4,
    STATE_SECTION_MEMORY = 5,
    STATE_SECTION_DMA = 6,
    STATE_SECTION_IPC_FIFO = 7,
    STATE_SECTION_VRAM_CONTROL = 8,
    STATE_SECTION_GPU2D = 9,
    STATE_SECTION_SPU = 10,
    STATE_SECTION_CART = 11,
    STATE_SECTION_SPI = 12,
    STATE_SECTION_RTC = 13,
    STATE_SECTION_SCHED = 14,
    STATE_SECTION_MACHINE = 15,
    STATE_SECTION_GPU3D = 16,
    STATE_SECTION_COUNT_MAX = 17
};

typedef struct state_file_header {
    char magic[4];
    uint32_t format_version;
    uint32_t flags;
    uint32_t section_count;
    uint32_t stream_version;
    uint32_t machine_field;
    uint64_t time_block;
    uint64_t core_version;
    uint8_t reserved[24];
} state_file_header_t;

typedef struct state_section_header {
    uint32_t id;
    uint32_t size;
    uint32_t crc;
    uint32_t reserved;
} state_section_header_t;

typedef struct state_stream {
    void *file;
    uint64_t framed;
    uint64_t unmapped_0;
    uint8_t *buffer;
    uint8_t *cursor;
} state_stream_t;

#define NDS_ITCM_SIZE 0x8000u
#define NDS_SHARED_WRAM_SIZE 0x8000u
#define NDS_DTCM_SIZE 0x4000u
#define NDS_ARM7_WRAM_SIZE 0x10000u
#define NDS_VRAM_ABCD_SIZE 0x20000u
#define NDS_VRAM_E_SIZE 0x10000u
#define NDS_VRAM_FGI_SIZE 0x4000u
#define NDS_VRAM_H_SIZE 0x8000u
#define NDS_OAM_SIZE 0x800u
#define NDS_PALETTE_SIZE 0x800u
#define NDS_WIFI_RAM_SIZE 0x4000u
#define STATE_WIFI_REGS_SIZE 0x400u
#define STATE_BIOS9_KEY_OFFSET 0x40u
#define NDS_SCREEN_BYTES 0x18000u
typedef struct state_write_work {
    char     path[0x400];
    char     final_name[0x400];
    uint64_t thread;
    uint8_t  stream_header[32];
    uint64_t cursor;
    uint64_t block;
    uint32_t flags;
    uint32_t pending;
} state_write_work_t;
extern state_write_work_t state_write_work;

#endif

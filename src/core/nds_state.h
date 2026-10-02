#ifndef SEEDLESSDS_NDS_STATE_H
#define SEEDLESSDS_NDS_STATE_H

#include <stddef.h>
#include <stdint.h>
#include "../math_unit.h"
#include "../rtc.h"
#include "../cpu/arm.h"
#include "../cpu/jit-arm64/jit.h"
#include "config.h"
#include "../ipc.h"
#include "../irq.h"
#include "../dma.h"
#include "../spi/spi.h"
#include "../cart/cart.h"
#include "../spu/spu.h"
#include "../memory/vram.h"
#include "../gpu/gpu.h"
#include "../gpu/gpu3d/gpu3d.h"
#include "../cpu/cp15.h"
#include "../cpu/bus.h"
#include "sched.h"
#define NDS_MACHINE_SIZE 0x3b34000u

#define IO_MIRROR_SIZE 0x8000u

typedef struct io_mirror {
    uint32_t dispcnt;
    uint16_t dispstat;
    uint16_t vcount;
    uint8_t unmapped_0[0x64 - 8];
    uint32_t dispcapcnt;
    uint8_t unmapped_0b[0xb0 - 0x68];
    dma_regs_t dma[4];
    uint32_t dma_fill[4];
    uint8_t unmapped_4[0x130 - 0xf0];
    uint16_t keyinput;
    uint16_t keycnt;
    uint16_t rcnt;
    uint16_t extkeyin;
    uint8_t unmapped_9[0x180 - 0x138];
    ipc_regs_t ipc;
    uint8_t unmapped_1[0x1a0 - 0x18c];
    cart_regs_t cart;
    uint8_t unmapped_5[0x204 - 0x1b0];
    uint16_t exmemcnt;
    uint8_t unmapped_6[0x208 - 0x206];
    irq_regs_t irq;
    uint8_t unmapped_2[0x240 - 0x218];
    union {
        uint8_t vramcnt[7];
        struct {
            uint8_t vramstat;
            uint8_t wramstat;
        } arm7;
    };
    uint8_t wramcnt;
    uint8_t vramcnt_hi[2];
    uint8_t unmapped_8[0x280 - 0x24a];
    math_regs_t math;
    uint8_t unmapped_3[0x300 - 0x2c0];
    uint16_t postflg;
    uint8_t unmapped_10[2];
    uint16_t powcnt1;
    uint8_t unmapped_11[0x400 - 0x306];
    sound_regs_t sound;
    uint8_t unmapped_7[0x620 - 0x520];
    int32_t pos_result[4];
    int16_t vec_result[3];
    uint8_t unmapped_12[IO_MIRROR_SIZE - 0x636];
} io_mirror_t;

#define BUS_OFFSET 0x35dc930u
#define BUS_SIZE (0x36d9ec0u - BUS_OFFSET)

typedef struct bus {
    uint8_t *main_ram;
    uint8_t *itcm;
    uint8_t *shared_wram;
    uint8_t *dtcm;
    uint8_t arm7_wram[0x10000];
    uint8_t bios9[0x1000];
    uint8_t bios7[0x4000];
    uint8_t *vram_bank[VRAM_BANKS];
    uint8_t *unmapped_page;
    uint8_t oam[0x800];
    uint8_t oam_cache[0x800];
    uint8_t palette[0x800];
    uint8_t palette_cache[0x800];
    uint8_t arm7_io_scratch[0x4000];
    io_mirror_t io_mirror[2];
    uint8_t firmware_image[FIRMWARE_IMAGE_SIZE];
    uint8_t blank_page[0x20000];
    uint8_t scratch_page[0x20000];
    uint8_t discard_page[0x4000];
    uint32_t main_ram_page_bits[0x40];
    uint8_t *slot2_page_bits;
    uint32_t itcm_page_bits;
    uint32_t shared_wram_page_bits;
    uint32_t arm7_wram_page_bits;
    uint32_t vram_page_bits[BUS_VRAM_BITMAP_WORDS];
    uint8_t main_ram_code_bits[0x40000];
    uint8_t *slot2_code_bits;
    uint32_t itcm_code_bits[0x200];
    uint8_t shared_wram_code_bits[0x800];
    uint8_t arm7_wram_code_bits[0x1000];
    uint8_t vram_code_bits[0xa400];
    uint8_t wifi_regs[0xfba38 - 0xfb5b8];
    uint8_t *slot2_rom;
    uint8_t *wram_window[2][2];
    uint8_t unmapped_2[8];
    struct nds *machine;
    struct gpu *gpu;
    struct gpu3d *gpu3d;
    struct spu *spu;
    pagetable_t *arm9_pagetable;
    pagetable_t *arm7_pagetable;
    bus_region_t region[BUS_REGIONS];
    dma_t dma[2];
    ipc_fifo_t ipc_fifo[2];
    cart_t *cart;
    spi_t *spi;
    rtc_t *rtc;
    uint32_t dtcm_start;
    uint32_t dtcm_end;
    uint32_t itcm_size;
    uint32_t host_page_size;
    uint8_t *shared_region;
    int32_t shared_region_fd;
    uint8_t unmapped_4[4];
    uint8_t *address_window;
    uint8_t *vram_region;
    uint8_t *vram_window;
    int32_t vram_region_fd;
    uint8_t unmapped_5[4];
    math_t math;
    uint8_t bios_flags;
    uint8_t active;
    uint8_t unmapped_6[0xfd518 - 0xfd514];
    cp15_t cp15;
} bus_t;

#define BUS_SLOT2_PAGE_BITS_OFF offsetof(bus_t, slot2_page_bits)
#define BUS_SLOT2_CODE_BITS_OFF offsetof(bus_t, slot2_code_bits)

typedef struct nds {
    sched_t sched;
    cart_t cart;
    spi_t spi;
    rtc_t rtc;
    nds_input_record_t input_record;
    nds_config_t config;
    nds_benchmark_t benchmark;
    char rom_path[NDS_PATH_SIZE];
    char cache_dir[NDS_PATH_SIZE];
    char save_dir[NDS_PATH_SIZE];
    char rom_file_name[NDS_PATH_SIZE];
    char rom_name[NDS_PATH_SIZE];
    uint8_t unmapped_1[NDS_JIT_ARENA_OFFSET - 0x90838];
    jit_arena_t jit_arena __attribute__((aligned(0x4000)));
    spu_t spu;
    arm_t arm9;
    arm_t arm7;
    bus_t bus;
    gpu_t gpu;
    gpu3d_t gpu3d;
    gpu_output_t output;
    uint8_t fatal_jump[0x100];
    nds_runtime_t runtime;
} nds_t;

extern nds_t nds_machine;
#endif

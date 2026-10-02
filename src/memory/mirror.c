#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "core/data_paths.h"
#include "ipc.h"
#include "dma.h"
#include <string.h>
#include <stddef.h>
#include "platform/android/audio.h"
#include "core_internals.h"
#include "mem_access.h"

uint64_t mirror_hook_noop(uint64_t x0)
{
    return x0;
}

void mirror_pause_hook_noop(void) {
}

int mirror_query_unsupported(void *machine, const char *path, uint32_t per_game) {
    (void)machine; (void)path; (void)per_game;
    return -1;
}

int mirror_query_zero(void) {
    return 0;
}

int mirror_query_unsupported_clone(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    (void)a4; (void)a5; (void)a6; (void)a7;
    return -1;
}

typedef void   *(*fn_dlopen)(const char *, int);
typedef void   *(*fn_dlsym)(void *, const char *);
typedef int32_t (*fn_dlclose)(void *);
typedef int32_t (*fn_open)(const char *, int32_t, int32_t);
typedef int32_t (*fn_ioctl)(int32_t, unsigned long, unsigned long);
typedef int32_t (*fn_close)(int32_t);
typedef int32_t (*fn_asharedmem_create)(const char *, uint32_t);

int32_t mirror_shm_create(const char *name, uint32_t size, int32_t level_api) {

    static fn_dlopen  p_dlopen;
    static fn_dlsym   p_dlsym;
    static fn_dlclose p_dlclose;
    static fn_open    p_open;
    static fn_ioctl   p_ioctl;
    static fn_close   p_close;
    if (!p_dlopen) {
        p_dlopen  = (fn_dlopen)sym_libc_dlopen;
        p_dlsym   = (fn_dlsym)sym_libc_dlsym;
        p_dlclose = (fn_dlclose)sym_libc_dlclose;
        p_open    = (fn_open)sym_libc_open;
        p_ioctl   = (fn_ioctl)sym_libc_ioctl;
        p_close   = (fn_close)sym_libc_close;
    }

    if (level_api > 0x1c) {

        const char *lib_so  = "libandroid.so";
        const char *symbol = "ASharedMemory_create";

        void *handle = p_dlopen(lib_so, 1);
        fn_asharedmem_create create =
            (fn_asharedmem_create)p_dlsym(handle, symbol);

        int32_t r = create(name, size);
        p_dlclose(handle);
        return r;
    }

    const char *path_dev = "dev/ashmem";
    int32_t fd = p_open(path_dev, 0x42, 0x1ff);
    if (fd < 0) return fd;

    int32_t rc = p_ioctl(fd, 0x41007701ul, (unsigned long)name);
    if (rc < 0) {
        p_close(fd);
        return rc;
    }

    rc = p_ioctl(fd, 0x40087703ul, (unsigned long)size);
    if (rc < 0) {
        p_close(fd);
        return rc;
    }

    return fd;
}

void *mirror_get_fixed_page_ptr(void) {

    return &nds_machine;
}

extern void    input_record_close_write_stream(unsigned char *obj);
extern int32_t mirror_memory_subsystem_destroy_7(uint8_t *param_1) __asm__("mirror_memory_subsystem_destroy");
extern void    gpu2d_worker_shutdown(void);

void mirror_fatal_reset_longjmp(unsigned char *param_1)
{

    if (((nds_t *)param_1)->runtime.jit_enabled == 0) {
        arm_hook_noop();
    }

    (void)mirror_query_zero();

    if (((nds_t *)param_1)->rom_name[0] != 0) {
        (void)nds_close_files(&((nds_t *)param_1)->cart);
    }

    nds_subsystem_shutdown_release_objects();

    input_record_close_write_stream((unsigned char *)(&((nds_t *)param_1)->input_record));

    (void)mirror_memory_subsystem_destroy((uint8_t *)&((nds_t *)param_1)->bus);

    gpu2d_worker_shutdown();

    PLATFORM_AUDIO->restart_pending = 1;

    {
        typedef void (*fn_longjmp)(void *, int);
        fn_longjmp p_longjmp = (fn_longjmp)sym_libc_longjmp;
        p_longjmp(nds_machine.fatal_jump, 0);
    }
}

typedef int   (*fn_munmap)(void *, uint64_t);
typedef void *(*fn_mmap)(void *, uint64_t, int, int, int, int64_t);

int32_t mirror_remap_pages_16k(bus_t *bus, uint8_t *param_2, uint64_t param_3, int32_t param_4) {

    uint32_t n = (uint32_t)param_3 >> 14;
    if (n == 0) return 0;

    int64_t  base_off = (int64_t)(intptr_t)bus->address_window;
    uint8_t *base      = param_2 + base_off;
    fn_munmap unmap = (fn_munmap)sym_libc_munmap;
    fn_mmap   map    = (fn_mmap)sym_libc_mmap;

    uint64_t total = (uint64_t)n << 14;

    for (uint64_t i = 0; i != total; i += 0x4000) {
        void *addr = base + (uint32_t)i;

        unmap(addr, 0x4000);

        int32_t  fd  = bus->shared_region_fd;
        uint32_t off = (uint32_t)param_4 + (uint32_t)i;

        void *r = map(addr, 0x4000, 3, 1, fd, (int64_t)off);
        if (r != addr) return -1;
    }

    return 0;
}

#define BLOCK   0x4000
typedef void *(*fn_mmap_9)(void *, size_t, int, int, int, long);
typedef int   (*fn_munmap_9)(void *, size_t);

int mirror_alias_blocks_16k(bus_t *bus, uint8_t *base, unsigned size,
                       unsigned offset_base) {
    unsigned blocks = size >> 14;
    if (blocks == 0) return 0;

    fn_mmap_9   p_mmap   = (fn_mmap_9)sym_libc_mmap;
    fn_munmap_9 p_munmap = (fn_munmap_9)sym_libc_munmap;

    uint64_t total = (uint64_t)blocks << 14;

    for (uint64_t shift = 0; shift != total; shift += BLOCK) {
        uint8_t *dir = base + shift;

        p_munmap(dir, BLOCK);

        int fd = bus->vram_region_fd;
        uint32_t offset = offset_base + (uint32_t)shift;
        void *r = p_mmap(dir, BLOCK, 3, 1, fd, (long)(uint64_t)offset);

        if (r != (void *)dir) return -1;
    }
    return 0;
}
#undef BLOCK

uint32_t mirror_dirty_bitmap_test(const uint32_t *coarse, const uint32_t *fine,
                            uint32_t address, uint32_t len) {
    uint32_t last = address + len - 1;

    uint32_t block_start = address >> 16;
    uint32_t block_end = last >> 16;
    uint32_t bit_start  = (address >> 11) & 0x1F;
    uint32_t bit_end  = (last >> 11) & 0x1F;
    uint32_t blocks  = block_end - block_start;

    uint32_t accumulated, tail;

    if (blocks != 0) {
        accumulated = coarse[0] & (0xFFFFFFFFu << bit_start);
        tail = coarse[1];
        if (blocks != 1) {
            const uint32_t *p = coarse + 2;
            uint32_t n = blocks - 1;
            do {
                accumulated |= tail;
                tail = *p++;
                n--;
            } while (n != 0);
        }
        goto join;
    }

    {
        uint32_t mask = (0xFFFFFFFFu << bit_start) & ~(0xFFFFFFFEu << bit_end);
        if ((mask & coarse[0]) == 0)
            return 0;
        if (bit_start != bit_end)
            return 1;

        uint32_t gr_start = address >> 6;
        uint32_t gr_end = last >> 6;
        uint32_t fine_start = (address >> 1) & 0x1F;
        uint32_t groups = gr_end - gr_start;
        bit_end = (last >> 1) & 0x1F;

        if (groups == 0)
            return ((0xFFFFFFFFu << fine_start) & ~(0xFFFFFFFEu << bit_end))
                   & fine[0];

        accumulated = fine[0] & (0xFFFFFFFFu << fine_start);
        tail = fine[1];
        if (groups != 1) {
            const uint32_t *p = fine + 2;
            uint32_t n = groups - 1;
            do {
                accumulated |= tail;
                tail = *p++;
                n--;
            } while (n != 0);
        }
    }

join:
    return (tail & ~(0xFFFFFFFEu << bit_end)) | accumulated;
}

uint32_t mirror_write_block_via_regions(unsigned char *table, void *ctx, uint32_t dir,
                            unsigned char *origin, uint32_t remaining) {
    if (remaining == 0)
        return 0;
    uint32_t (*notify)(const uint32_t *, const uint32_t *, uint32_t, uint32_t) = mirror_dirty_bitmap_test;
    void *(*copy)(void *, const void *, uint64_t) =
        (void *(*)(void *, const void *, uint64_t))sym_libc_memcpy;

    uint32_t accumulated = 0;
    void (*perword)(void *, uint32_t, uint32_t) = 0;

    for (;;) {
        uint32_t ireg = dir >> 23;
        unsigned char *e = table + (uint64_t)ireg * 96;
        uint32_t mask = *(uint32_t *)e;
        uint32_t size = mask + 1;

        void *(*before)(void *, void *, uint32_t) =
            *(void *(**)(void *, void *, uint32_t))(e + 72);
        void *mark = before(ctx, e, dir);

        void *(*after)(void *, void *, uint32_t) =
            *(void *(**)(void *, void *, uint32_t))(e + 80);
        void *aux = after(ctx, e, dir);

        unsigned type = e[89];
        unsigned char *dest = 0;

        if (type == 2)
            perword = *(void (**)(void *, uint32_t, uint32_t))(e + 48);
        else if (type == 0)
            dest = *(unsigned char **)(e + 32) + (mask & dir);

        uint32_t chunk;
        if (type == 1) {
            unsigned char *(*resolver)(void *, uint32_t) =
                *(unsigned char *(**)(void *, uint32_t))(e + 32);
            dest = resolver(ctx, dir);
        }
        chunk = size > remaining ? remaining : size;
        remaining -= chunk;

        if (dest != 0) {
            if (mark != 0)
                accumulated |= notify(mark, aux, dir, chunk);
            copy(dest, origin, (uint64_t)chunk);
            dir += chunk;
            origin += chunk;
        } else if (perword == 0) {
            dir += chunk;
            origin += chunk;
        } else if (chunk != 0) {
            do {
                uint32_t v = *(uint32_t *)origin;
                origin += 4;
                perword(ctx, dir & mask, v);
                chunk -= 4;
                dir += 4;
            } while (chunk != 0);
        }

        if (remaining == 0)
            return accumulated;
    }
}

#define OFF_STRUCT_B0     0xfd298UL
#define OFF_STRUCT_B1     0xfd348UL
#define OFF_STRUCT_C0     0xfd3f8UL
#define OFF_STRUCT_C1     0xfd458UL
#define DEST_2C704   0x2c704UL
#define DEST_1A96C   0x1a96cUL
#define DEST_1A978   0x1a978UL
#define DEST_29D28   0x29d28UL
#define DEST_2A064   0x2a064UL
#define DEST_33820   0x33820UL
#define DEST_31B20   0x31b20UL
#define DEST_33C08   0x33c08UL
#define DEST_1B4D4   0x1b4d4UL
#define DEST_2A9FC   0x2a9fcUL

typedef int    (*fn_getpagesize)(void);
typedef void  *(*fn_mmap_12)(void *, size_t, int, int, int, long);
typedef int    (*fn_munmap_12)(void *, size_t);
typedef void   (*fn_perror)(const char *);
typedef void   (*fn_exit)(int);
typedef void  *(*fn_memset)(void *, int, size_t);
static fn_getpagesize p_getpagesize;
static fn_mmap_12        p_mmap;
static fn_munmap_12      p_munmap;
static fn_perror      p_perror;
static fn_exit        p_exit;
static fn_memset      p_memset;

static void ensure_libc(void) {
    if (!p_getpagesize) p_getpagesize = (fn_getpagesize)sym_libc_getpagesize;
    if (!p_mmap)        p_mmap        = (fn_mmap_12)sym_libc_mmap;
    if (!p_munmap)      p_munmap      = (fn_munmap_12)sym_libc_munmap;
    if (!p_perror)      p_perror      = (fn_perror)sym_libc_perror;
    if (!p_exit)        p_exit        = (fn_exit)sym_libc_exit;
    if (!p_memset)      p_memset      = (fn_memset)sym_libc_memset;
}
typedef int32_t (*fn_shm_create)(const char *name, uint32_t size);
typedef int32_t (*fn_close_core)(int32_t fd);
typedef int      (*fn_sprintf_core)(char *, long, const char *, ...);
typedef void      (*fn_init1)(void *);
typedef int        (*fnp_files_read_exact_size)(unsigned char *, const char *, void *, uint32_t);
static const char MSG_ERROR_IS[]        = "Error is";
static const char NAME_REGION_A[]     = "nds_mapped_memory.dat";
static const char NAME_REGION_B[]     = "nds_mapped_memory_vram.dat";
static const char NAME_FW_REAL[]      = "nds_firmware.bin";

static void fatal_perror_exit(void) {
    p_perror(MSG_ERROR_IS);
    p_exit(-1);
}

static const uint64_t OFFSETS_PTAB[10] = {
    0x00000UL, 0x20000UL, 0x40000UL, 0x60000UL, 0x80000UL,
    0x90000UL, 0x94000UL, 0x98000UL, 0xa0000UL, 0xa4000UL
};

static void mirror_window_4mb(uint8_t *ctx, uint8_t *base_window) {
    uint64_t counter = 0;
    do {
        uint64_t off = counter & 0xffffc000ULL;

        uint8_t *dest = base_window + off;
        p_munmap(dest, 0x4000);
        int32_t fd = ((bus_t *)ctx)->shared_region_fd;
        void *obtained = p_mmap(dest, 0x4000, 3 ,
                                 1 , fd, (long)off);
        if (obtained != (void *)dest) {
            p_perror(MSG_ERROR_IS);
            break;
        }
        counter += 0x4000UL;
    } while (counter != 0x400000UL);
}

int32_t mirror_memory_subsystem_create(uint8_t *param_1, uint8_t *param_2) {
    ensure_libc();

    uint8_t *ctx  = param_1;
    uint8_t *pcpu = param_2;

    fn_shm_create   p_shm_create  = input_shared_region_create;
    fn_close_core   p_close_core  = (fn_close_core)sym_libc_close;
    fn_sprintf_core p_sprintf     = str_vsprintf_limit_256;
    fn_init1        init_queue_descriptors       = bus_region_init_queue_descriptors;
    void *(*init_channel_descriptors)(bus_t *) = bus_region_init_channel_descriptors;
    fnp_files_read_exact_size        read_exact_size       = files_read_exact_size;

    nds_t *nds = (nds_t *)pcpu;
    uint8_t *anchor1 = (uint8_t *)&nds->arm9;
    uint8_t *anchor2 = (uint8_t *)&nds->arm7;

    bus_t *bus = (bus_t *)ctx;
    bus->active = 1;

    bus->arm9_pagetable = &nds->arm9.pagetable;
    bus->arm7_pagetable = &nds->arm7.pagetable;
    bus->machine = (nds_t *)pcpu;
    bus->gpu = &((nds_t *)pcpu)->gpu;
    bus->gpu3d = &((nds_t *)pcpu)->gpu3d;
    bus->spu = &((nds_t *)pcpu)->spu;
    bus->spi = &((nds_t *)pcpu)->spi;
    bus->rtc = &((nds_t *)pcpu)->rtc;
    bus->cart = &((nds_t *)pcpu)->cart;

    nds->arm9.pagetable.region = &bus->region[0];
    nds->arm9.pagetable.bus = bus;
    nds->arm9.pagetable.cpu = &nds->arm9;
    nds->arm7.pagetable.region = &bus->region[BUS_ARM9_REGIONS];
    nds->arm7.pagetable.bus = bus;
    nds->arm7.pagetable.cpu = &nds->arm7;

    bus->host_page_size = (uint32_t)p_getpagesize();

    char name_buf[256];

    p_sprintf(name_buf, (long)(uintptr_t)pcpu, NAME_REGION_A);
    int32_t  fd_a  = p_shm_create(name_buf, 0x414000UL
);
    void    *ptr_a = p_mmap(NULL, 0x4000000UL, 3, 1, fd_a, 0);
    if (ptr_a == (void *)-1) {
        p_close_core(fd_a);
        fd_a  = p_shm_create(name_buf, 0x4000000UL);

        ptr_a = p_mmap(NULL, 0x4000000UL, 3, 1, fd_a, 0);
        bus->shared_region_fd = fd_a;
        bus->shared_region = (uint8_t *)ptr_a;
        if (ptr_a == (void *)-1) { fatal_perror_exit(); return -1; }
    } else {
        bus->shared_region_fd = fd_a;
        bus->shared_region = (uint8_t *)ptr_a;
    }

    uint8_t *r1 = (uint8_t *)ptr_a;
    bus->main_ram = r1;
    bus->itcm = r1 + BUS_SHARED_ITCM;
    bus->shared_wram = r1 + BUS_SHARED_WRAM;
    bus->dtcm = r1 + BUS_SHARED_DTCM;

    if (bus->address_window == (uint8_t *)(uintptr_t)(uint64_t)-1) {
        void *ptr_a2 = p_mmap(NULL, 0x4000000UL, 3, 1, fd_a, 0);
        if (ptr_a2 == (void *)-1) { fatal_perror_exit(); return -1; }
        bus->address_window = (uint8_t *)ptr_a2;
    }

    {
        uint64_t off = 0x8000UL;
        do {
            uint8_t *base = bus->address_window;
            uint8_t *dest = base + off;
            p_munmap(dest, 0x4000);
            int32_t fd_now = bus->shared_region_fd;
            void *obtained = p_mmap(dest, 0x4000, 3, 1, fd_now, 0);
            if (obtained != (void *)dest) { p_exit(-1); return -1; }
            off += 0x4000UL;
        } while (off < 0x4000000UL);
    }

    {
        uint8_t *base_ra2 = bus->address_window;
        mirror_window_4mb(ctx, base_ra2 + NDS_MAIN_RAM_BASE);
        mirror_window_4mb(ctx, base_ra2 + NDS_MAIN_RAM_BASE + NDS_MAIN_RAM_SIZE);
        mirror_window_4mb(ctx, base_ra2 + NDS_MAIN_RAM_BASE + 2 * NDS_MAIN_RAM_SIZE);
        mirror_window_4mb(ctx, base_ra2 + NDS_MAIN_RAM_BASE + 3 * NDS_MAIN_RAM_SIZE);
    }

    p_sprintf(name_buf, 0x4000L
, NAME_REGION_B);
    int32_t  fd_b  = p_shm_create(name_buf, 0xa8000UL
);
    void    *ptr_b = p_mmap(NULL, 0x800000UL, 3, 1, fd_b, 0);
    if (ptr_b == (void *)-1) {
        p_close_core(fd_b);
        fd_b  = p_shm_create(name_buf, 0x800000UL);
        ptr_b = p_mmap(NULL, 0x800000UL, 3, 1, fd_b, 0);
        bus->vram_region_fd = fd_b;
        bus->vram_region = (uint8_t *)ptr_b;
        if (ptr_b == (void *)-1) { fatal_perror_exit(); return -1; }
    } else {
        bus->vram_region_fd = fd_b;
        bus->vram_region = (uint8_t *)ptr_b;
    }

    void *ptr_b2 = p_mmap(NULL, 0x800000UL, 3, 1, fd_b, 0);
    bus->vram_window = (uint8_t *)ptr_b2;
    if (ptr_b2 == (void *)-1) { fatal_perror_exit(); return -1; }

    {
        uint8_t *r3 = bus->vram_region;
        for (int k = 0; k < 9; k++)
            bus->vram_bank[k] = r3 + OFFSETS_PTAB[k];
        bus->unmapped_page = r3 + OFFSETS_PTAB[9];
    }

    init_queue_descriptors(ctx);
    init_channel_descriptors((bus_t *)ctx);

    cp15_bind(&((bus_t *)ctx)->cp15, (arm_t *)anchor1);

    nds->arm9.cp15 = &bus->cp15;
    nds->arm9.io_mirror = &bus->io_mirror[0];
    nds->arm7.cp15 = 0;
    nds->arm7.io_mirror = &bus->io_mirror[1];

    dma_channels_schedule_deadlines(&((bus_t *)ctx)->dma[0], bus, &bus->region[0], &((bus_t *)ctx)->io_mirror[0], (arm_t *)anchor1);
    dma_channels_schedule_deadlines(&((bus_t *)ctx)->dma[1], bus, &bus->region[BUS_ARM9_REGIONS], &((bus_t *)ctx)->io_mirror[1], (arm_t *)anchor2);

    ipc_fifo_init(&((bus_t *)ctx)->ipc_fifo[0], (struct arm *)anchor1, &((bus_t *)ctx)->ipc_fifo[1]);
    ipc_fifo_init(&((bus_t *)ctx)->ipc_fifo[1], (struct arm *)anchor2, &((bus_t *)ctx)->ipc_fifo[0]);

    uint8_t *flags = &bus->bios_flags;
    *flags = 0;
    {
        int r = read_exact_size(pcpu, DATA_BIOS9_REAL, bus->bios9, (uint32_t)sizeof bus->bios9);
        if (r < 0) {
            r = read_exact_size(pcpu, DATA_BIOS9_REPLACEMENT, bus->bios9, (uint32_t)sizeof bus->bios9);
            if (r < 0) { return -1; }
            *flags |= 0x2;
        }
    }

    {
        int r = read_exact_size(pcpu, DATA_BIOS7_REAL, bus->bios7, (uint32_t)sizeof bus->bios7);
        if (r < 0) {
            r = read_exact_size(pcpu, DATA_BIOS7_REPLACEMENT, bus->bios7, (uint32_t)sizeof bus->bios7);
            if (r < 0) { return -1; }
            *flags |= 0x1;
        }
    }

    {
        uint8_t *fw = ((bus_t *)ctx)->firmware_image;
        int r = read_exact_size(pcpu, DATA_FIRMWARE_MODIFIED, fw, FIRMWARE_IMAGE_SIZE);
        if (r < 0) {
            r = read_exact_size(pcpu, NAME_FW_REAL, fw, FIRMWARE_IMAGE_SIZE);
            if (r < 0) {
                p_memset(fw, 0, FIRMWARE_IMAGE_SIZE);
                firmware_header_build((firmware_header_t *)fw);
            }
        }
    }

    bus->slot2_rom = 0;
    wr64(ctx + BUS_SLOT2_PAGE_BITS_OFF, 0);
    wr64(ctx + BUS_SLOT2_CODE_BITS_OFF, 0);
    return 0;
}
#undef OFF_STRUCT_B0
#undef OFF_STRUCT_B1
#undef OFF_STRUCT_C0
#undef OFF_STRUCT_C1
#undef DEST_2C704
#undef DEST_1A96C
#undef DEST_1A978
#undef DEST_29D28
#undef DEST_2A064
#undef DEST_33820
#undef DEST_31B20
#undef DEST_33C08
#undef DEST_1B4D4
#undef DEST_2A9FC

typedef int (*fn_munmap_13)(void *, uint64_t);
typedef int (*fn_close_13)(int);

int32_t mirror_memory_subsystem_destroy(uint8_t *param_1) {

    static fn_munmap_13 p_munmap;
    static fn_close_13  p_close;
    if (!p_munmap) {
        p_munmap = (fn_munmap_13)sym_libc_munmap;
        p_close  = (fn_close_13)sym_libc_close;
    }

    bus_t *bus = (bus_t *)param_1;

    int64_t p0 = (int64_t)(intptr_t)bus->address_window;
    p_munmap((void *)(p0 + BUS_ADDRESS_WINDOW_FIRST_PAGE), BUS_ADDRESS_WINDOW_BYTES - BUS_ADDRESS_WINDOW_FIRST_PAGE);

    p_close(bus->shared_region_fd);

    p_munmap(bus->vram_region, 0xa8000);

    p_munmap(bus->vram_window, 0x800000);

    return (int32_t)p_close(bus->vram_region_fd);
}

#define OFF_STRUCT_B0     0xfd298UL
#define OFF_STRUCT_B1     0xfd348UL
#define OFF_STRUCT_C0     0xfd3f8UL
#define OFF_STRUCT_C1     0xfd458UL
#define OFF_COPY_SRC     0x3fe00UL
#define COPY_SIZE         0x70UL
#define OFF_COPY_DST_REL 0x3ffc80UL
#define OFF_FLAG_DST_REL  0x3ffc40UL
#define DEST_21BE0  0x21be0UL
#define DEST_29944  0x29944UL
#define DEST_2A448  0x2a448UL
#define DEST_33838  0x33838UL
#define DEST_31C50  0x31c50UL
#define DEST_33C18  0x33c18UL
#define DEST_2AB44  0x2ab44UL
#define DEST_21DA8  0x21da8UL
typedef void *(*fn_memset_14)(void *, int, size_t);
typedef int    (*fn_munmap_14)(void *, size_t);
typedef void  *(*fn_mmap_14)(void *, size_t, int, int, int, long);
typedef void   (*fn_exit_14)(int);
static fn_memset_14 p_memset_14;
static fn_munmap_14 p_munmap_14;
static fn_mmap_14   p_mmap_14;
static fn_exit_14   p_exit_14;

static void ensure_libc_14(void) {
    if (!p_memset_14) p_memset_14 = (fn_memset_14)sym_libc_memset;
    if (!p_munmap_14) p_munmap_14 = (fn_munmap_14)sym_libc_munmap;
    if (!p_mmap_14)   p_mmap_14   = (fn_mmap_14)sym_libc_mmap;
    if (!p_exit_14)   p_exit_14   = (fn_exit_14)sym_libc_exit;
}

static void clear(void *dst, size_t n) { p_memset_14(dst, 0, n); }

static uint64_t compute_entry(uint8_t *tbl, uint32_t addr) {
    uint64_t lVar13, lVar15, val;

    if ((addr >> 28) == 0) {

        uint32_t idx, mask;
        bus_region_t *desc;
        int8_t flag58, flag59;
        uint64_t base_desc;

        idx = (addr >> 23) & 0x1ffu;
        desc = &((pagetable_t *)tbl)->region[idx];

        mask = desc->mask;
        flag58 = (int8_t)desc->read_kind;
        flag59 = (int8_t)desc->write_kind;
        base_desc = (uint64_t)(uintptr_t)desc->read_memory;

        lVar13 = (flag58 == 0) ? (base_desc + (addr & mask)) : 0;
        lVar15 = (flag59 == 0) ? (base_desc + (addr & mask)) : 0;

        if (lVar13 == 0) return PAGETABLE_MISSING;
    } else if (addr <= 0xfffeffffu) {

        return PAGETABLE_MISSING;
    } else {

        uint32_t mode;

        mode = ((pagetable_t *)tbl)->cpu->is_arm9;
        if (mode != 1) return PAGETABLE_MISSING;

        lVar13 = (uint64_t)(uintptr_t)((pagetable_t *)tbl)->bus->bios9 + (addr & 0x800u);
        lVar15 = 0;
    }

    val = (uint64_t)(((int64_t)lVar13 - (int64_t)addr) >> 2);
    if (lVar13 != lVar15) val |= PAGETABLE_MISSING;
    return val;
}

static void sweep_table_full(uint8_t *tbl) {
    unsigned long i;

    for (i = 0; i < 0x200000UL; i++) {
        uint32_t addr = (uint32_t)(i * 0x800UL);
        uint64_t val = compute_entry(tbl, addr);
        memcpy(tbl + i * 8UL, &val, 8);
    }
}

static void refresh_subset(bus_t *bus) {
    uint8_t *tbl = (uint8_t *)bus->arm9_pagetable;
    uint32_t count = bus->itcm_size, addr;

    addr = 0;
    do {
        uint64_t val = compute_entry(tbl, addr);
        unsigned long idx = (unsigned long)(addr >> 11);
        memcpy(tbl + idx * 8UL, &val, 8);
        addr += 0x800u;
    } while (addr != count);

    bus->itcm_size = 0;
}

void mem_console_reset(uint8_t *param_1) {

    uint8_t *ctx = param_1;
    bus_t *bus = (bus_t *)param_1;
    ensure_libc_14();

    clear((uint8_t *)&bus->io_mirror[0], sizeof bus->io_mirror[0]);
    clear((uint8_t *)&bus->io_mirror[1], sizeof bus->io_mirror[1]);
    {
        uint8_t  b3 = 3;
        uint16_t h3ff = 0x3ff;
        uint32_t w = 0x7f800f;
        uint16_t hff = 0xff;
        uint16_t h1 = 1;

        bus->io_mirror[0].wramcnt = b3;
        bus->io_mirror[0].keyinput = h3ff;
        bus->io_mirror[1].arm7.wramstat = b3;
        bus->io_mirror[1].keyinput = h3ff;
        memcpy(&bus->io_mirror[1].rcnt, &w, 4);
        bus->io_mirror[0].cart.auxspidata = hff;
        bus->io_mirror[0].powcnt1 = h1;

        clear(bus->main_ram, 0x400000);
        clear(bus->itcm, 0x8000);
        clear(bus->shared_wram, 0x8000);
        clear(bus->dtcm, 0x4000);
        clear(bus->arm7_wram, sizeof bus->arm7_wram);

        clear(bus->vram_bank[0], 0x20000);
        clear(bus->vram_bank[1], 0x20000);
        clear(bus->vram_bank[2], 0x20000);
        clear(bus->vram_bank[3], 0x20000);
        clear(bus->vram_bank[4], 0x10000);
        clear(bus->vram_bank[5], 0x4000);
        clear(bus->vram_bank[6], 0x4000);
        clear(bus->vram_bank[7], 0x8000);
        clear(bus->vram_bank[8], 0x4000);
        clear(bus->blank_page, sizeof bus->blank_page);
        clear(bus->unmapped_page, 0x4000);

        clear(bus->palette, sizeof bus->palette);
        clear(bus->palette_cache, sizeof bus->palette_cache);
        clear(bus->oam, sizeof bus->oam);
        clear(bus->oam_cache, sizeof bus->oam_cache);

        ((bus_t *)ctx)->math.div_result_valid = 0;
        ((bus_t *)ctx)->math.sqrt_result_valid = 0;
    }

    {
        wram_apply_wramcnt((bus_t *)(ctx));
        bus_region_reset_and_free((bus_t *)ctx);
        bus_access_tables_init((bus_t *)ctx);
    }

    sweep_table_full((uint8_t *)bus->arm9_pagetable);
    sweep_table_full((uint8_t *)bus->arm7_pagetable);

    {
        cp15_reset(&((bus_t *)ctx)->cp15);
        dma_channels_reset_fields(&((bus_t *)ctx)->dma[0]);
        dma_channels_reset_fields(&((bus_t *)ctx)->dma[1]);
        ipc_fifo_clear(&((bus_t *)ctx)->ipc_fifo[0]);
        ipc_fifo_clear(&((bus_t *)ctx)->ipc_fifo[1]);
    }

    {
        unsigned long x24;
        for (x24 = 0; x24 != 0x800000UL; x24 += 0x4000UL) {
            uint8_t *base;
            uint32_t fd;
            uint8_t *dest, *obtained;

            base = bus->vram_window;
            fd = (uint32_t)bus->vram_region_fd;
            dest = base + x24;

            p_munmap_14(dest, 0x4000);
            obtained = (uint8_t *)p_mmap_14(dest, 0x4000, 3 ,
                                          1 , (int)fd, 0xa4000L);
            if (obtained != dest) {
                p_exit_14(-1);
                return;
            }
        }
    }

    {
        nds_t *pcpu;
        uint32_t gate;

        pcpu = bus->machine;
        gate = pcpu->config.boot_from_firmware;

        if (gate != 0) {
            return;
        }

        {
            uint16_t one = 1;
            uint8_t *p0;
            uint8_t byte1 = 1;
            uint32_t refresh;

            bus->io_mirror[1].postflg = one;
            bus->io_mirror[0].postflg = one;

            {
                firmware_user_settings_build((const uint32_t *)&pcpu->config,
                                                          ((bus_t *)ctx)->firmware_image);
            }

            p0 = bus->main_ram;
            memcpy(p0 + OFF_COPY_DST_REL,
                   bus->firmware_image + OFF_COPY_SRC, COPY_SIZE);

            p0 = bus->main_ram;
            memcpy(p0 + OFF_FLAG_DST_REL, &byte1, 1);

            refresh = bus->itcm_size;
            if (refresh != 0) {
                refresh_subset(bus);
            }

            {

                pagetable_dtcm_remap((bus_t *)((bus_t *)(ctx)), 0, 0);
            }
        }
    }
}
#undef OFF_STRUCT_B0
#undef OFF_STRUCT_B1
#undef OFF_STRUCT_C0
#undef OFF_STRUCT_C1
#undef OFF_COPY_SRC
#undef COPY_SIZE
#undef OFF_COPY_DST_REL
#undef OFF_FLAG_DST_REL
#undef DEST_21BE0
#undef DEST_29944
#undef DEST_2A448
#undef DEST_33838
#undef DEST_31C50
#undef DEST_33C18
#undef DEST_2AB44
#undef DEST_21DA8

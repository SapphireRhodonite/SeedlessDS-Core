#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "core_internals.h"

void wram_apply_wramcnt(bus_t *bus) {
    void (*prune)(unsigned char *, uint32_t, uint32_t) = pagetable_unmap_range;
    int (*unmap)(void *, uint64_t) =
        (int (*)(void *, uint64_t))sym_libc_munmap;
    void *(*map)(void *, uint64_t, int, int, int, long) =
        (void *(*)(void *, uint64_t, int, int, int, long))sym_libc_mmap;

    unsigned bits = bus->io_mirror[0].wramcnt;
    unsigned char *ram = bus->shared_wram;
    unsigned char *alternate = bus->blank_page;
    unsigned char *ram2 = ram + NDS_SHARED_WRAM_HALF_BYTES;

    bus->wram_window[0][0] = (bits & 1) == 0 ? ram  : alternate;
    bus->wram_window[0][1] = (bits & 2) == 0 ? ram2 : alternate;
    bus->wram_window[1][0] = (bits & 1) == 0 ? bus->arm7_wram : ram;
    bus->wram_window[1][1] = (bits & 2) == 0 ? bus->arm7_wram + NDS_SHARED_WRAM_HALF_BYTES : ram2;

    {
        void *o = bus->arm9_pagetable;
        uint32_t start = bus->dtcm_start;
        uint32_t base = 0x3000000, len = 0x1000000;
        if (start < 0x3000001) {
            uint32_t end = bus->dtcm_end;
            if (end >= 0x4000001) {
                prune(o, 0x3000000, start - 0x3000000);
                base = start;
                len = 0x4000000 - end;
            }
        }
        prune(o, base, len);
    }

    {
        void *o = bus->arm7_pagetable;
        uint32_t start = bus->dtcm_start;
        uint32_t base = 0x3000000, len = 0x800000;
        if (start < 0x3000001) {
            uint32_t end = bus->dtcm_end;
            if (end >= 0x3800001) {
                prune(o, 0x3000000, start - 0x3000000);
                base = start;
                len = 0x3800000 - end;
            }
        }
        prune(o, base, len);
    }

    for (uint64_t r = 0; r < 0x200; r++) {
        unsigned char *slot = bus->address_window + (r << 15) + NDS_SHARED_WRAM_BASE;
        uint64_t off = 0;
        for (;;) {
            unsigned char *page = slot + (uint32_t)off;
            unmap(page, 0x4000);
            int fd = bus->shared_region_fd;
            void *given = map(page, 0x4000, 3, 1, fd,
                                (long)(off + BUS_SHARED_WRAM));
            if (off == 0x4000)
                break;
            if (given != page)
                break;
            off += 0x4000;
        }
    }
}

#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include <string.h>
#include "core/nds_state.h"
#include "core/gamedb.h"
#include "core_internals.h"
#include "mem_access.h"

#define CODE(a, b, c) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16))

void *gamedb_apply_fixes(unsigned char *o) {
    nds_runtime_t *f = &((nds_t *)o)->runtime;

    f->cart_cycles_per_word = 1;
    f->dma_cost_low = 0;
    f->dma_cost_scale = 0;
    f->extra_cycles = 0;
    f->force_vcount_192 = 0; f->dma_sets_cycle_mark = 0; f->gxfifo_swap_side_command = 0; f->force_line_finish = 0;

    uint32_t word = *(uint32_t *)(o + 23776);
    int32_t  code = (int32_t)(word & 0xFFFFFFu);

    #define MODE1_PLUS()  do { f->extra_cycles = 1; \
                              f->dma_cost_scale = 1; f->dma_sets_cycle_mark = 1; } while (0)
    #define MODE1()      do { f->extra_cycles = 1; } while (0)
    #define MODE2()      do { f->extra_cycles = 2; } while (0)

    if (code > (int32_t)CODE('B','A','M')) {
        if (code > (int32_t)CODE('B','O','W')) {
            if (code > (int32_t)CODE('A','K','Y')) {
                if (code == (int32_t)CODE('B','K','Y')) { MODE1(); return o; }
                if (code == (int32_t)CODE('B','Y','Y')) { MODE1_PLUS(); return o; }
                return o;
            }
            if (code == (int32_t)CODE('C','O','W')) { MODE1(); return o; }
            if (code == (int32_t)CODE('B','Y','X')) { MODE1_PLUS(); return o; }
            return o;
        }
        if (code > (int32_t)CODE('A','3','P')) {
            if (code == (int32_t)CODE('B','3','P')) { MODE1(); return o; }
            if (code == (int32_t)CODE('Y','P','T')) { MODE2(); return o; }
            return o;
        }
        if (code == (int32_t)CODE('C','A','M')) { f->gxfifo_swap_side_command = 1; return o; }
        if (code == (int32_t)CODE('C','S','N')) { MODE1(); return o; }
        return o;
    }

    if (code > (int32_t)CODE('X','A','J')) {
        if (code > (int32_t)CODE('B','V','J')) {
            if (code == (int32_t)CODE('C','V','J')) { MODE1(); return o; }
            if (code == (int32_t)CODE('B','E','L')) { MODE1(); return o; }
            return o;
        }
        if (code == (int32_t)CODE('Y','A','J')) { f->gxfifo_swap_side_command = 1; return o; }
        if (code == (int32_t)CODE('C','L','J')) {

            uint64_t pair = 0x0000000200000004ULL;
            f->extra_cycles = 2;

            for (int i = 0; i < 8; i++) ((unsigned char *)&f->dma_cost_low)[i] = (unsigned char)(pair >> (i * 8));
            return o;
        }
        return o;
    }

    if (code > (int32_t)CODE('A','2','F')) {
        if (code == (int32_t)CODE('B','2','F')) { f->force_line_finish = 1; return o; }
        if (code == (int32_t)CODE('B','5','J')) {

            if ((word >> 24) == 0x50) { MODE1(); return o; }
            return o;
        }
        return o;
    }

    if (code == (int32_t)CODE('C','Y','8')) { MODE1_PLUS(); return o; }
    if (code == (int32_t)CODE('V','A','A')) { f->force_vcount_192 = 1; return o; }
    return o;
}
#undef CODE


extern void *db_find_cartridge_entry(const game_database_t *catalog, uint32_t key,
                                 const char *name);
typedef int32_t (*fn_read_rom)(const char *path, uint32_t *total_size,
                               void *dest, uint32_t len,
                               uint32_t offset);
typedef void *(*fn_strncpy_chk2)(char *dest, const char *src, size_t n,
                                  size_t destlen, size_t srclen);

void *gamedb_find_by_path(const game_database_t *catalog, const char *path_rom)
{

    uint32_t total_size = 0;
    unsigned char hdr[0x20];

    fn_read_rom read_rom = archive_rom_read_chunk;
    int32_t r = read_rom(path_rom, &total_size, hdr, 0x20, 0);

    if (r < 0)
        return 0;

    fn_strncpy_chk2 strncpy_chk2 =
        (fn_strncpy_chk2)fortify_strncpy;

    char title[13];
    strncpy_chk2(title, (const char *)hdr, 12, 13, 512);

    uint32_t key = rd32(hdr + 12);

    title[12] = 0;

    return db_find_cartridge_entry(catalog, key, title);
}

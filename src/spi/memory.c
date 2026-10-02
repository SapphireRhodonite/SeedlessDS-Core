#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include "spi.h"
#include "seedlessds/platform.h"
#include "core_internals.h"

static uint32_t close(spi_memory_t *m, uint32_t cmd) {
    m->command = (uint16_t)cmd;
    m->byte_count = 0;
    m->address = 0;
    return 0;
}

static void mark(spi_memory_t *m, uint32_t dir) {
    uint64_t o = (uint64_t)(dir >> 12) & 0xffffcull;
    uint32_t b = 1u << (((dir >> 9) & 0x7fffffu) & 31u);
    m->dirty_sectors[o >> 2] |= b;
}

uint32_t spi_memory_transfer_byte(spi_memory_t *m, uint32_t value) {

    uint32_t state = m->state;
    if (state > 11) return value;

    switch (state) {

    case 0:
        if (value == 3) { m->state = 1;  return close(m, value); }
        if (value == 4) { m->status &= (uint8_t)~2u; m->state = 0; return close(m, value); }
        if (value == 5) { m->state = 3;  return close(m, value); }
        if (value == 6) { m->status |= 2; m->state = 0; return close(m, value); }
        if (value == 8) { m->state = 11; return close(m, value); }
        if (value == 0x9f) { m->state = 2; return close(m, value); }

        if (m->type == 1) {
            if ((int32_t)value <= 0xaa) {
                if (value <= 11 && ((1u << value) & 0xc04u))
                    m->state = 1;
                return close(m, value);
            }
            if ((int32_t)value > 0xd7) {
                if (value == 0xdb || value == 0xd8) m->state = 1;
                return close(m, value);
            }
            if (value == 0xab) { m->state = 0;  return close(m, value); }
            if (value == 0xb9) { m->state = 10; return close(m, value); }
            return close(m, value);
        }

        if (value - 10u < 2u) {
            if (m->mask == 0x1ff) { value |= 0x100u; m->state = 1; }
            return close(m, value);
        }
        if (value == 1) { m->state = 4; return close(m, value); }
        if (value == 2) { m->state = 1; return close(m, 0x102); }
        return close(m, value);

    case 1: {
        uint32_t dir = value | (m->address << 8);
        m->address = dir;

        uint32_t c = m->byte_count + 1u;
        uint32_t lim = m->address_bytes;
        m->byte_count = (uint8_t)c;
        if (lim != (c & 0xffu)) return value;

        dir &= m->mask;
        m->address = dir;
        uint32_t cmd = m->command;
        m->byte_count = 0;

        if ((int32_t)cmd > 0xda) {
            if ((int32_t)cmd > 0x109) {
                if (cmd == 0x10a) {
                    m->save_countdown = 0x3c;
                    m->address = dir | 0x100u;
                    m->state = 6;
                } else if (cmd == 0x10b) {
                    m->address = dir | 0x100u;
                    m->state = 5;
                }
                return value;
            }
            if (cmd == 0xdb) {
                uint8_t *p = m->data;
                m->save_countdown = 0x3c;
                memset(p + (dir & 0xffffff00u), 0xff, 256);
                mark(m, m->address);
                return value;
            }
            if (cmd == 0x102) {
                m->save_countdown = 0x3c;
                m->state = 6;
            }
            return value;
        }

        if (cmd - 2u <= 9u) {
            switch (cmd) {
            case 2: case 10:
                m->save_countdown = 0x3c; m->state = 6; break;
            case 3:  m->state = 5; break;
            case 11: m->state = 7; break;
            default: break;
            }
            return value;
        }

        if (cmd == 0xd8) {
            uint8_t *p = m->data;
            uint64_t base = (uint64_t)dir & 0xffff0000ull;
            m->save_countdown = 0x3c;
            memset(p + base, 0xff, 0x10000);
            uint32_t o = (uint32_t)((base >> 14) & 0x3ffffull);
            memset(m->dirty_sectors + o, 0xff, 16);
        }
        return value;
    }

    case 2: {
        uint32_t c = m->byte_count;
        uint32_t r = m->jedec_id[c];
        if (c == 2) m->byte_count = 0;
        else        m->byte_count = (uint8_t)(c + 1);
        return r;
    }

    case 3:
        return m->status | 0xcu;

    case 4:
        m->status = (uint8_t)((value & ~3u) | (m->status & 3u));
        return value;

    case 5: {
        uint8_t *p = m->data;
        uint32_t dir = m->address;
        uint32_t r = p[dir];
        m->address = m->mask & (dir + 1u);
        return r;
    }

    case 6: {
        mark(m, m->address);
        uint8_t *p = m->data;
        p[m->address] = (uint8_t)value;
        m->address = (m->address + 1u) & m->mask;
        return value;
    }

    case 11:
        m->byte_count = 0;
        return 0xaa;

    default:
        return value;
    }
}

void spi_memory_deselect(spi_memory_t *m) {
    m->state = 0;
}

void spi_memory_address_wrap(spi_memory_t *m, uint32_t mask) {
    m->address = m->mask & mask;
}

uint32_t spi_memory_read_word(spi_memory_t *m) {
    const unsigned char *ring = m->data;
    uint32_t i = m->address, mask = m->mask;
    uint32_t v;
    memcpy(&v, ring + i, 4);
    m->address = mask & (i + 4u);
    return v;
}

void spi_memory_write_word(spi_memory_t *m, uint32_t param_2) {

    uint32_t w8 = m->address;

    uint64_t x9 = ((uint64_t)w8 >> 12) & 0xffffcULL;

    uint32_t shift = (w8 >> 9) & 0x7fffffu;

    uint32_t bit = 1u << (shift & 0x1fu);

    m->dirty_sectors[x9 >> 2] |= bit;

    unsigned char *ring = m->data;

    memcpy(ring + m->address, &param_2, 4);

    m->save_countdown = 0x3c;

    m->address = (m->address + 4u) & m->mask;
}

void spi_memory_set_state_word(spi_memory_t *m, uint32_t param_2)
{
    m->dirty_sectors[0] = param_2;
}

static size_t (*core_fwrite)(const void *, size_t, size_t, void *);
static int    (*core_fclose)(void *);
static int    (*core_fseek)(void *, long, int);
static int    (*core_fflush)(void *);
static int    (*core_fileno)(void *);
static int    (*core_ftruncate)(int, long);

long spi_memory_save_file(spi_memory_t *m) {
    if (!core_fwrite) {
        core_fwrite    = (size_t (*)(const void *, size_t, size_t, void *))sym_libc_fwrite;
        core_fclose    = (int (*)(void *))sym_libc_fclose;
        core_fseek     = (int (*)(void *, long, int))sym_libc_fseek;
        core_fflush    = (int (*)(void *))sym_libc_fflush;
        core_fileno    = (int (*)(void *))sym_libc_fileno;
        core_ftruncate = (int (*)(int, long))sym_libc_ftruncate;
    }

    if (m->write_whole_file != 0) {
        void *(*open_fn)(const char *, const char *) = platform_file_open;
        void *f = open_fn(m->path, "wb");
        if (f == 0) return 0;
        uint32_t n = m->mask;
        core_fwrite(m->data, (size_t)(n + 1), 1, f);
        return core_fclose(f);
    }

    void *f = m->file;
    if (f == 0) return 0;

    uint32_t size = m->mask;
    uint32_t regions = (size + SPI_MEMORY_SECTOR_BYTES) >> SPI_MEMORY_SECTOR_SHIFT;

    if (m->truncate_size != 0) {
        core_ftruncate(core_fileno(f), m->truncate_size);
        m->truncate_size = 0;
    }

    for (uint64_t i = 0, base = 0; i < regions; i++, base += 0x4000) {
        uint32_t w = m->dirty_sectors[i];
        if (w == 0) continue;
        uint64_t off = base;
        while (w) {
            if (w & 1) {
                core_fseek(f, (long)off, 0);
                core_fwrite(m->data + off, 512, 1, f);
            }
            w >>= 1;
            off += 0x200;
        }
        m->dirty_sectors[i] = 0;
    }

    if (m->footer_written == 0 && m->has_footer != 0) {
        unsigned char hdr[122];
        static const unsigned char footer_head[80] = {
            0x7c, 0x3c, 0x2d, 0x2d, 0x53, 0x6e, 0x69, 0x70, 0x20, 0x61, 0x62, 0x6f, 0x76, 0x65, 0x20, 0x68,
            0x65, 0x72, 0x65, 0x20, 0x74, 0x6f, 0x20, 0x63, 0x72, 0x65, 0x61, 0x74, 0x65, 0x20, 0x61, 0x20,
            0x72, 0x61, 0x77, 0x20, 0x73, 0x61, 0x76, 0x20, 0x62, 0x79, 0x20, 0x65, 0x78, 0x63, 0x6c, 0x75,
            0x64, 0x69, 0x6e, 0x67, 0x20, 0x74, 0x68, 0x69, 0x73, 0x20, 0x44, 0x65, 0x53, 0x6d, 0x75, 0x4d,
            0x45, 0x20, 0x73, 0x61, 0x76, 0x65, 0x64, 0x61, 0x74, 0x61, 0x20, 0x66, 0x6f, 0x6f, 0x74, 0x65,
        };
        static const unsigned char footer_tail[16] = { 0x7c, 0x2d, 0x44, 0x45, 0x53, 0x4d, 0x55, 0x4d, 0x45, 0x20, 0x53, 0x41, 0x56, 0x45, 0x2d, 0x7c };
        const unsigned char *k1 = footer_head;
        const unsigned char *k2 = footer_tail;
        uint32_t v = size + 1;

        for (int i = 0; i < 80; i++) hdr[i] = k1[i];
        *(uint16_t *)(hdr + 80) = 0x3a72;

        for (int i = 0; i < 4; i++) {
            hdr[82 + i] = (unsigned char)(v >> (8 * i));
            hdr[86 + i] = (unsigned char)(v >> (8 * i));
            hdr[98 + i] = (unsigned char)(v >> (8 * i));
        }
        hdr[90] = hdr[91] = hdr[92] = hdr[93] = 0;
        hdr[94] = m->address_bytes;
        hdr[95] = hdr[96] = hdr[97] = 0;
        hdr[102] = hdr[103] = hdr[104] = hdr[105] = 0;
        for (int i = 0; i < 16; i++) hdr[106 + i] = k2[i];

        core_fseek(f, (long)v, 0);
        core_fwrite(hdr, 122, 1, f);
        m->footer_written = 1;
    }
    return core_fflush(f);
}

long spi_memory_save_tick(spi_memory_t *m) {
    uint32_t left = m->save_countdown;
    if (left == 0) return (long)(intptr_t)m;
    left -= 1;
    m->save_countdown = left;
    if (left != 0) return (long)(intptr_t)m;

    return spi_memory_save_file(m);
}

static size_t (*core_fread)(void *, size_t, size_t, void *);
static int    (*core_fclose_8)(void *);
static int    (*core_fseek_8)(void *, long, int);
static long   (*core_ftell)(void *);
static void  *(*core_memset)(void *, int, size_t);
static void  *(*core_memmem)(const void *, size_t, const void *, size_t);
static char  *(*core_strncpy)(char *, const char *, size_t);

long spi_memory_mount(spi_memory_t *m, uint32_t type, unsigned char *buf,
                        uint32_t size, const char *path, uint32_t fa, uint32_t fb) {
    if (!core_fread) {
        core_fread   = (size_t (*)(void *, size_t, size_t, void *))sym_libc_fread;
        core_fclose_8  = (int (*)(void *))sym_libc_fclose;
        core_fseek_8   = (int (*)(void *, long, int))sym_libc_fseek;
        core_ftell   = (long (*)(void *))sym_libc_ftell;
        core_memset  = (void *(*)(void *, int, size_t))sym_libc_memset;
        core_memmem  = (void *(*)(const void *, size_t, const void *, size_t))sym_libc_memmem;
        core_strncpy = (char *(*)(char *, const char *, size_t))sym_libc_strncpy;
    }
    void *(*open_fn)(const char *, const char *) = platform_file_open;

    m->mask = size - 1;
    m->data = buf;
    m->type = type;
    m->write_whole_file = (unsigned char)fa;
    m->has_footer = (unsigned char)fb;
    m->truncate_size = 0;

    int put = 1, class = 0;
    if (type == 2)      class = (size > 0x10000) ? 3 : (size < 0x201 ? 1 : 2);
    else if (type == 1) class = 3;
    else if (type == 0) class = 0;
    else                put = 0;
    if (put) m->address_bytes = (unsigned char)class;

    void *old = m->file;
    if (old) { core_fclose_8(old); m->file = 0; }

    if (path == 0) {
        m->file = 0;
        m->path[0] = 0;
        m->status = 0;
        return (long)(intptr_t)m;
    }

    void *f = open_fn(path, "rb");
    m->footer_written = 0;

    int fail = (f == 0);
    uint32_t n_read = 0;
    if (!fail && core_fread(buf, size, 1, f) != 1) fail = 1;

    if (!fail) {
        uint32_t expected = (fb == 0) ? size : size + 122;
        long where = core_ftell(f);
        core_fseek_8(f, 0, 2);
        n_read = (uint32_t)core_ftell(f);
        core_fseek_8(f, where, 0);
        core_fclose_8(f);

        if (expected != n_read) m->truncate_size = expected;

        if (n_read >= size) {
            core_memset(m->dirty_sectors, 0, (size + SPI_MEMORY_SECTOR_BYTES - 1u) >> SPI_MEMORY_SECTOR_SHIFT);
        } else {
            int32_t from = (int32_t)(n_read - 0x400);
            from &= ~(from >> 31);
            unsigned char *hit = (unsigned char *)core_memmem(
                buf + from, n_read - (uint32_t)from,
                "|<--Snip above here to create a raw sav by excluding this DeSmuME savedata footer:", 82);
            uint32_t cut = hit ? (uint32_t)(hit - buf) : n_read;

            core_memset(buf + cut, 255, size - cut);
            uint32_t half = cut >> 14;
            uint32_t total = (size + SPI_MEMORY_SECTOR_BYTES - 1u) >> SPI_MEMORY_SECTOR_SHIFT;
            core_memset(m->dirty_sectors, 0, (size_t)half * 4);
            core_memset(m->dirty_sectors + half, 255, (size_t)(total - half) * 4);
        }
    } else {
        core_memset(buf, 255, size);
        core_memset(m->dirty_sectors, 255, size >> 12);
    }

    char *copy = m->path;
    core_strncpy(copy, path, 1023);
    m->path[1023] = 0;

    void *nf = open_fn(copy, "rb+");
    if (nf == 0) {
        void *tmp = open_fn(copy, "wb");
        core_fclose_8(tmp);
        nf = open_fn(copy, "rb+");
    }
    m->file = nf;
    m->status = 0;
    return (long)(intptr_t)nf;
}

static char *(*core_strcpy_chk)(char *, const char *, size_t);

char *spi_memory_set_path(spi_memory_t *m, const char *str) {
    if (!core_strcpy_chk)
        core_strcpy_chk = (char *(*)(char *, const char *, size_t))
                          fortify_strcpy;
    return core_strcpy_chk(m->path, str, 1024);
}

int spi_memory_close_file(spi_memory_t *m) {
    void *f = m->file;
    int r = 0;
    if (f) {
        r = ((int (*)(void *))sym_libc_fclose)(f);
        m->file = 0;
    }
    return r;
}

void spi_memory_reset(spi_memory_t *m) {
    m->save_countdown = 0;
    m->address = 0;
    m->state = 0;
}

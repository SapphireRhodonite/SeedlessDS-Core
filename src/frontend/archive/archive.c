#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include "7z.h"
#include "7zAlloc.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "LzmaDec.h"
#include "Lzma2Dec.h"
#include <zlib.h>
#include <wchar.h>
#include "frontend/archive/archive.h"
#include "cpu/bus.h"
#include "cart/cart.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"


uint32_t archive_zip_open_extracted_rom(uint8_t *state, const char *path,
                                       uint32_t mode, uint32_t flags_path);




void *archive_open_rom(const char *path, const char *cache,
                         uint32_t clip, uint32_t private) {

    int  (*c_close)(int) = (int (*)(int))sym_libc_close;
    void (*c_free)(void *) = (void (*)(void *))sym_libc_free;
    long (*c_lseek)(int, long, int) = (long (*)(int, long, int))sym_libc_lseek;
    void *(*c_fopen)(const char *, const char *) = platform_file_open;
    long (*c_ftell)(void *) = (long (*)(void *))sym_libc_ftell;
    int  (*c_fclose)(void *) = (int (*)(void *))sym_libc_fclose;
    unsigned long (*c_fread)(void *, unsigned long, unsigned long, void *) =
        (unsigned long (*)(void *, unsigned long, unsigned long, void *))sym_libc_fread;
    unsigned long (*c_fwrite)(const void *, unsigned long, unsigned long, void *) =
        (unsigned long (*)(const void *, unsigned long, unsigned long, void *))sym_libc_fwrite;
    int (*arm)(char *, size_t, const char *, ...) = str_vsprintf_caller_limit_5;
    void (*arm2)(char *, long, size_t, const char *, ...) = archive_vsnprintf_limited;

    uint32_t mode = (private == 0) ? 2u : 0x8002u;
    if (path == 0) return 0;

    int fd = nds_platform_default()->files.open_fd(nds_platform_default()->user, path, 0);
    if (fd < 0) return 0;

    unsigned char *b = (unsigned char *)((void *(*)(unsigned long))sym_libc_malloc)(32);
    if (b == 0) { c_close(fd); return 0; }

    char *ext = ((char *(*)(const char *, int))sym_libc_strrchr)(path, '.');
    if (ext == 0) { c_close(fd); c_free(b); return 0; }
    *(uint64_t *)(b + 8) = 0;

    int (*cmpi)(const char *, const char *) =
        (int (*)(const char *, const char *))sym_libc_strcasecmp;

    if (cmpi(ext, ".nds") == 0) {

        *(uint32_t *)(b + 24) = 0;
        *(uint64_t *)b = (uint64_t)(int64_t)fd;
        long size = c_lseek(fd, 0, 2);
        *(uint32_t *)(b + 20) = (uint32_t)size;
        *(uint8_t *)(b + 28) = 1;
        *(uint32_t *)(b + 16) = (uint32_t)size;

        uint32_t skip = (clip == 0) ? 1u : 0u;
        for (;;) {
            if (!(skip & 1)) {
                c_lseek(fd, 0x80, 0);
                if (((long (*)(int, void *, unsigned long))sym_libc_read)(
                        fd, b + 20, 4) >= 1) {
                    uint32_t lo = *(uint32_t *)(b + 16);
                    uint32_t hi = *(uint32_t *)(b + 20);
                    if ((hi - 1) >= lo) *(uint32_t *)(b + 20) = lo;
                }
            }
            c_lseek(fd, 0, 0);
            void *m = ((void *(*)(void *, unsigned long, uint32_t, uint32_t,
                                  int, long))sym_libc_mmap)(
                          0, *(uint32_t *)(b + 20), 1, mode, fd, 0);
            *(void **)(b + 8) = m;
            if (m != (void *)-1) return b;
            if (!(skip & 1)) break;
            skip = 0;
        }
        c_close(fd);
        c_free(b);
        return 0;
    }

    c_close(fd);

    char n1[1024], data[1024], name[1024], meta[1024], stamp[1024];
    unsigned char state[144];
    char *dest = 0;

    if (cache != 0) {
        __builtin_memset(name, 0, 1024);
        __builtin_memset(data, 0, 1024);
        arm(meta, 1024, "%s%cunzipped_rom.nds", cache, '/');
        arm(stamp, 1024, "%s%ccache_info", cache, '/');

        int has = nds_platform_default()->files.stat(nds_platform_default()->user, meta, state);
        int valid = 0;
        if (has == 0) {

            void *f = c_fopen(meta, "rb");
            if (f != 0) {
                ((int (*)(void *, long, int))sym_libc_fseek)(f, 0, 2);
                long data_size = c_ftell(f);
                c_fclose(f);
                void *g = c_fopen(stamp, "rb");
                if (g != 0) {
                    uint64_t mark = 0;
                    uint32_t saved = 0;
                    arm2(name, 0, 1023, "%s", path);
                    if (c_fread(data, 1024, 1, g) != 0
                        && c_fread(&mark, 8, 1, g) != 0
                        && c_fread(&saved, 4, 1, g) != 0
                        && (uint64_t)data_size == saved
                        && mark == *(uint64_t *)(state + 88)
                        && ((int (*)(const char *, const char *))sym_libc_strcmp)(
                               data, name) == 0)
                        valid = 1;
                    c_fclose(g);
                }
            }
        }
        if (valid) {
            if (archive_zip_open_extracted_rom(b, cache, clip, private) == 0)
                return b;
            c_free(b);
            return 0;
        }
        arm2(n1, 0, 1024, "%s%cunzipped_rom.nds", cache, '/');
        dest = n1;
    }

    uint32_t class;
    void *(*extract)(const char *, const char *, uint32_t *, const char *);
    if (cmpi(ext, ".zip") == 0) {
        class = 1; extract = archive_zip_extract_entry;
    } else if (cmpi(ext, ".7z") == 0) {
        class = 2; extract = archive_7z_extract_by_extension;
    } else if (cmpi(ext, ".rar") == 0) {
        class = 3; extract = archive_rar_extract_by_extension;
    } else {
        c_free(b);
        return 0;
    }

    *(uint32_t *)(b + 24) = class;
    void *data_p = extract(path, "nds", (uint32_t *)(b + 16), dest);
    *(void **)(b + 8) = data_p;
    if (data_p == 0) { c_free(b); return 0; }

    uint32_t len = *(uint32_t *)(b + 16);
    if (len == 0) { c_free(b); return 0; }
    *(uint8_t *)(b + 28) = 0;
    *(uint32_t *)(b + 20) = len;

    if (cache == 0) {
        if (clip == 0) return b;
        uint32_t fits = *(uint32_t *)((unsigned char *)data_p + 128);
        *(uint32_t *)(b + 20) = fits;
        if ((fits - 1) >= len) { *(uint32_t *)(b + 20) = len; return b; }
        *(void **)(b + 8) = ((void *(*)(void *, unsigned long))sym_libc_realloc)(
                                data_p, fits);
        return b;
    }

    __builtin_memset(name, 0, 1024);
    arm(stamp, 1024, "%s%cunzipped_rom.nds", cache, '/');
    arm(meta, 1024, "%s%ccache_info", cache, '/');
    if (nds_platform_default()->files.stat(nds_platform_default()->user, stamp, data) == 0) {
        void *f = c_fopen(stamp, "rb");
        if (f != 0) {
            ((int (*)(void *, long, int))sym_libc_fseek)(f, 0, 2);
            long data_size = c_ftell(f);
            c_fclose(f);
            if ((uint32_t)data_size == len) {
                void *g = c_fopen(meta, "wb");
                if (g != 0) {
                    uint64_t mark = *(uint64_t *)((unsigned char *)data + 88);
                    arm2(name, 0, 1023, "%s", path);
                    c_fwrite(name, 1024, 1, g);
                    c_fwrite(&mark, 8, 1, g);
                    c_fwrite(&data_size, 4, 1, g);
                    c_fclose(g);
                }
            }
        }
    }

    if (archive_zip_open_extracted_rom(b, cache, clip, private) != 0) {
        c_free(b);
        return 0;
    }
    return b;
}

typedef int (*fn_open_fd)(const char *, int);
typedef int64_t (*fn_lseek)(int, int64_t, int);
typedef int64_t (*fn_read)(int, void *, size_t);
typedef void *(*fn_mmap)(void *, uint64_t, int, int, int, int64_t);





uint32_t archive_zip_open_extracted_rom(uint8_t *state, const char *path,
                             uint32_t mode, uint32_t flags_path)
{

    static fn_lseek core_lseek;
    static fn_read core_read;
    static fn_mmap core_mmap;
    char name[0x400];
    uint32_t fail = 1;
    int flags = flags_path == 0 ? 2 : 0x8002;

    if (!core_lseek)
        core_lseek = (fn_lseek)sym_libc_lseek;
    if (!core_read)
        core_read = (fn_read)sym_libc_read;
    if (!core_mmap)
        core_mmap = (fn_mmap)sym_libc_mmap;

    str_vsprintf_caller_limit_5(name, sizeof(name),
                               "%s%cunzipped_rom.nds", path, 0x2f);
    int fd = nds_platform_default()->files.open_fd(nds_platform_default()->user, name, 0);
    if (fd < 0)
        return fail;

    wr64(state, (uint64_t)(int64_t)fd);
    int64_t end = core_lseek(fd, 0, 2);
    wr32(state + 20, (uint32_t)end);
    wr32(state + 16, (uint32_t)end);
    wr8(state + 28, 1);
    wr32(state + 24, 0);

    if (mode != 0) {
        core_lseek(fd, 0x80, 0);
        if (core_read(fd, state + 20, 4) >= 1) {
            uint32_t limit = rd32(state + 16);
            uint32_t heading = rd32(state + 20) - 1;
            if (heading >= limit)
                wr32(state + 20, limit);
        }
    }

    core_lseek(fd, 0, 0);
    void *mapping = core_mmap(0, rd32(state + 20), 1, flags, fd, 0);
    uint32_t mapped = mapping != (void *)(intptr_t)-1;
    wr_ptr(state + 8, mapping);

    if (!mapped && mode == 0) {
        core_lseek(fd, 0x80, 0);
        if (core_read(fd, state + 20, 4) >= 1) {
            uint32_t limit = rd32(state + 16);
            uint32_t heading = rd32(state + 20) - 1;
            if (heading >= limit)
                wr32(state + 20, limit);
        }

        core_lseek(fd, 0, 0);
        mapping = core_mmap(0, rd32(state + 20), 1, flags, fd, 0);
        mapped = mapping != (void *)(intptr_t)-1;
        wr_ptr(state + 8, mapping);
    }

    return mapped ^ 1;
}

void archive_vsnprintf_limited(char *dest, long unused, size_t maxlen,
                        const char *format, ...) {
    (void)unused;

    va_list ap;
    va_start(ap, format);
    fortify_vsnprintf(dest, maxlen, 0x400, format, ap);
    va_end(ap);
}



typedef int  (*fn_munmap)(void *, uint64_t);
typedef int  (*fn_close)(int);
typedef void (*fn_free)(void *);
static fn_munmap p_munmap;
static fn_close  p_close;
static fn_free   p_free;

static void resolve_libc(void)
{
    if (!p_munmap) {
        p_munmap = (fn_munmap)sym_libc_munmap;
        p_close  = (fn_close)sym_libc_close;
        p_free   = (fn_free)sym_libc_free;
    }
}

void archive_rom_reader_destroy(uint8_t *param_1)
{

    if (param_1 == NULL) {
        return;
    }

    resolve_libc();

    uint8_t flag = rd8(param_1 + 0x1c);

    void *ptr = rd_ptr(param_1 + 0x8);

    if (flag != 0) {

        uint32_t size = rd32(param_1 + 0x14);

        p_munmap(ptr, (uint64_t)size);

        int32_t fd = (int32_t)rd32(param_1);

        p_close(fd);

        p_free(param_1);
        return;
    }

    p_free(ptr);

    p_free(param_1);
}

#include <stdio.h>

FILE *recon_archive_fopen(const char *path, const char *mode)
{
    extern void *files_fopen_resolved(const char *, const char *);
    return (FILE *)files_fopen_resolved(path, mode);
}

int recon_archive_open(const char *path, int flags)
{
    extern int files_open_translate_flags(const char *, int);
    return files_open_translate_flags(path, flags);
}
extern void *files_fopen_resolved(const char *path, const char *mode);
static char  *(*core_strrchr)(const char *, int);
static int    (*core_strcasecmp)(const char *, const char *);
static int    (*core_fseek)(void *, long, int);
static long   (*core_ftell)(void *);
static size_t (*core_fread)(void *, size_t, size_t, void *);
static int    (*core_fclose)(void *);

static void resolve(void) {
    if (core_strrchr) return;
    core_strcasecmp = (int (*)(const char *, const char *))sym_libc_strcasecmp;
    core_fseek      = (int (*)(void *, long, int))sym_libc_fseek;
    core_ftell      = (long (*)(void *))sym_libc_ftell;
    core_fread      = (size_t (*)(void *, size_t, size_t, void *))sym_libc_fread;
    core_fclose     = (int (*)(void *))sym_libc_fclose;
    core_strrchr    = (char *(*)(const char *, int))sym_libc_strrchr;
}

int archive_rom_read_chunk(const char *path, uint32_t *out_size, void *dest,
                       uint32_t len, uint32_t shift) {
    resolve();

    const char *s_nds  = ".nds";
    const char *s_zip  = ".zip";
    const char *s_7z   = ".7z";
    const char *s_rar  = ".rar";
    const char *s_pat  = "nds";

    const char *ext = core_strrchr(path, 0x2e);
    if (ext == 0) return -1;

    if (core_strcasecmp(ext, s_nds) == 0) {

        if (dest == 0)  return 0;
        if (len == 0) return 0;

        void *f = nds_platform_default()->files.open(nds_platform_default()->user, path, "rb");
        if (f == 0) return -1;

        core_fseek(f, 0, 2);
        long end = core_ftell(f);

        *out_size = (uint32_t)end;

        core_fseek(f, (long)(uint64_t)shift, 0);

        uint32_t size = *out_size;
        uint32_t cap = shift + len;

        if (size < cap) {
            core_fclose(f);
            return -1;
        }

        uint64_t requested = (uint64_t)len;
        size_t n_read = core_fread(dest, 1, (size_t)requested, f);
        core_fclose(f);

        return ((uint64_t)n_read != requested) ? -1 : 0;
    }

    if (core_strcasecmp(ext, s_zip) == 0)
        return archive_zip_open_entry(path, s_pat, out_size, dest, len, shift);

    if (core_strcasecmp(ext, s_7z) == 0)
        return archive_7z_read_range(path, s_pat, out_size, dest, len, shift);

    if (core_strcasecmp(ext, s_rar) != 0) return -1;

    return archive_rar_read_range(path, s_pat, out_size, dest, len, shift);
}


int32_t archive_rom_extension_is_supported(const char *param_1)
{

    uint32_t output;
    int32_t result = archive_rom_read_chunk(param_1, &output, 0, 0, 0);

    return result != 0 ? -1 : 0;
}

typedef int (*fn_reader_rom)(const char *path, uint32_t *out_size, void *dest,
                             uint32_t len, uint32_t shift);


uint32_t archive_rom_classify_header(const char *path)
{

    uint32_t archive_size;
    unsigned char data[128];
    fn_reader_rom reader = archive_rom_read_chunk;

    if (reader(path, &archive_size, data, 128u, 0u) != 0)
        return 0;

    uint32_t class = rd32(data + 12);
    if (class == 0x23232323u)
        return 1;

    uint32_t count = rd32(data + 44);
    if ((count - 1u) >= NDS_ROM_LOAD_LIMIT)
        return 0;

    if (rd32(data + 48) < NDS_ROM_ARM7_MIN_OFFSET)
        return 0;

    uint32_t first = rd32(data + 52);
    uint32_t second = rd32(data + 56);
    uint32_t shift = rd32(data + 32);

    if ((first - NDS_MAIN_RAM_BASE) >= NDS_ROM_LOAD_LIMIT + 1u &&
        (first - NDS_ARM7_WRAM_BASE) > NDS_ARM7_WRAM_WINDOW_BYTES)
        return 0;

    if ((second - NDS_MAIN_RAM_BASE) >= NDS_ROM_LOAD_LIMIT + 1u &&
        (second - NDS_ARM7_WRAM_BASE) > NDS_ARM7_WRAM_WINDOW_BYTES)
        return 0;

    if (reader(path, &archive_size, data, 16u, shift) != 0)
        return 0;

    uint32_t signature0 = rd32(data);
    uint32_t signature1 = rd32(data + 4);

    if (signature0 == 0xe7ffdeffu && signature1 == 0xe7ffdeffu)
        return 3;

    if (class == 0x45355659u && signature0 == 0x014a191au &&
        signature1 == 0xa5c470b9u)
        return 3;

    if (class == 0x50355659u && signature0 == 0xd0d48b67u &&
        signature1 == 0x39392f23u)
        return 3;

    if (signature1 == 0x9968ef44u && signature0 == 0x7829bc8du &&
        class == 0x4a355659u)
        return 3;

    return 2;
}





int archive_rom_read_reordered_block(const char *param_1, void *param_2)
{

    struct {
        uint32_t state;
        uint8_t tmp[0x440];
    } data;
    uint8_t *dest = (uint8_t *)param_2;
    uint32_t i;

    if (archive_rom_read_chunk(param_1, &data.state, data.tmp,
                                  0x80, 0) != 0)
        return -1;

    if (archive_rom_read_chunk(param_1, &data.state, data.tmp,
                                  0x440,
                                  rd32(data.tmp + 104)) != 0)
        return -1;

    memcpy(dest + 512, data.tmp + 544, 32);
    memcpy(dest + 640, data.tmp + 928, 32);
    memcpy(dest + 608, data.tmp + 896, 32);
    memcpy(dest + 704, data.tmp + 992, 32);
    memcpy(dest + 672, data.tmp + 960, 32);
    memcpy(dest + 768, data.tmp + 1056, 32);
    memcpy(dest + 736, data.tmp + 1024, 32);
    memcpy(dest + 576, data.tmp + 864, 32);
    memcpy(dest + 544, data.tmp + 832, 32);

    for (i = 0; i != 32; ++i) {
        uint32_t idx = (i & 7) | ((i >> 3) << 5);
        const uint8_t *origin = data.tmp + 32 + idx * 4;

        wr32(dest, rd32(origin));
        wr32(dest + 4, rd32(origin + 32));
        wr32(dest + 8, rd32(origin + 64));
        wr32(dest + 12, rd32(origin + 96));
        dest += 16;
    }

    wr16((uint8_t *)param_2 + 0x200, 0);
    return 0;
}

uint64_t archive_7z_window_sink_write(void *sink, const void *data, uint64_t size)
{
    archive_7z_window_sink_t *st = sink;
    const uint8_t *src = data;
    uint32_t delta = size;

    uint32_t pos_v    = st->position;
    uint32_t threshold_v = st->threshold;
    uint32_t pos_updated = pos_v + delta;
    st->position = pos_updated;

    if (pos_updated < threshold_v) {

        return 0u;
    }

    uint32_t difference = pos_updated - threshold_v;
    uint32_t cap_a       = st->room;
    uint8_t *base         = st->destination;
    uint32_t cursor_a    = st->cursor;
    uint32_t off_origin  = threshold_v - pos_v;

    uint32_t n = (difference > cap_a) ? cap_a : difference;

    memcpy(base + cursor_a, src + off_origin, n);

    uint32_t cap_b    = st->room;
    uint32_t threshold_b = st->threshold;
    uint32_t cursor_b = st->cursor;

    uint32_t cap_updated    = cap_b - n;
    uint32_t threshold_updated = threshold_b + n;
    uint32_t cursor_updated = cursor_b + n;

    st->room = cap_updated;
    st->threshold = threshold_updated;
    st->cursor = cursor_updated;

    return n;
}

extern ISzAlloc recon_archive_alloc;





void *archive_7z_extract_by_extension(const char *path, const char *suffix, uint32_t *output_size,
                          const char *dest)
{

    uint8_t listing[0x80] = {0};
    uint8_t name[0x30] = {0};
    uint8_t obj[0x80] = {0};
    uint8_t state[0x200] = {0};
    uint8_t entry_b[0x10000];
    uint8_t decoded[0x10000];
    uint8_t ctx[0x4048] = {0};
    uint8_t output[0x70] = {0};
    uint8_t filter[0xa8] = {0};
    CFileInStream *in_stream = (CFileInStream *)(void *)listing;
    CLookToRead *look = (CLookToRead *)(void *)ctx;
    CSzArEx *ar = (CSzArEx *)(void *)state;
    ISzAlloc *alloc_imp = (ISzAlloc *)(void *)(obj + 24);
    ISzAlloc *alloc_temp_imp = (ISzAlloc *)(void *)(obj + 8);
    CBuf *name_buf = (CBuf *)(void *)name;
    ISeqOutStream *out_stream = (ISeqOutStream *)(void *)output;
    void *work = NULL;
    uint64_t capacity = 0;
    uint64_t idx = 0;
    uint64_t offset = 0;
    uint32_t accumulated = 0;
    uint32_t state_final = 0;
    uint64_t reference = 0;
    uint8_t *entries;
    uint8_t *reg;
    uint8_t *descriptor;
    uint8_t *source;
    uint64_t class;
    uint64_t remaining = 0;
    uint64_t consumed;
    uint64_t n_read;
    uint64_t prev;
    uint64_t per_write;
    uint64_t shift_block;
    uint64_t tmp;
    uint64_t counter;
    uint32_t code;
    int found = 0;
    uint32_t map = 0;
    void *memory = NULL;
    void *result = NULL;
    uint64_t dest_len = 0;
    uint64_t src_len = 0;
    uint32_t status = 0;
    UInt64 selection;

    typedef int (*open_t)(void *, void *);
    typedef void (*one_t)(void *);
    typedef void (*two_t)(void *, void *);
    typedef void (*two_u_t)(void *, uint64_t);
    typedef int (*three_t)(void *, void *, void *);
    typedef uint64_t (*index_t)(void *, uint64_t, void *);
    typedef void *(*reserve_t)(void *, uint64_t);
    typedef uint64_t (*reference_t)(void *);
    typedef int (*init_t)(void *, void *, uint32_t, void *);
    typedef int (*read_t)(void *, void *, uint64_t *, uint64_t);
    typedef uint64_t (*write_t)(void *, void *, uint64_t);
    typedef int (*filter_t)(void *, void *, void *, void *, void *, uint32_t,
                            void *);
    typedef void *(*malloc_t)(size_t);
    typedef char *(*strrchr_t)(const char *, int);
    typedef int (*strcasecmp_t)(const char *, const char *);

    wr32(output_size, 0);
    alloc_imp->Alloc = SzAlloc;
    alloc_imp->Free = SzFree;
    alloc_temp_imp->Alloc = SzAllocTemp;
    alloc_temp_imp->Free = SzFreeTemp;
    if (((open_t)((void *)InFile_Open))(listing + 16, (void *)path) != 0)
        goto end;

    ((one_t)((void *)FileInStream_CreateVTable))(listing);
    ((two_u_t)((void *)LookToRead_CreateVTable))(ctx, 0);
    look->realStream = &in_stream->s;
    ((one_t)((void *)LookToRead_Init))(ctx);
    ((void (*)(void))((void *)CrcGenerateTable))();
    ((one_t)((void *)SzArEx_Init))(state);

    if (((int (*)(void *, void *, void *, void *))((void *)SzArEx_Open))
            (state, ctx, obj + 24, obj + 8) != 0) {
        state_final = 1;
        goto clear_listing;
    }

    counter = rd32(state + 48);
    if (counter == 0)
        goto clear_listing;

    entries = (uint8_t *)ar->db.Files;
    for (;;) {
        reg = entries + offset;
        if (reg[25] != 0)
            goto next;

        tmp = ((index_t)((void *)SzArEx_GetFileNameUtf16))(state, idx, NULL);
        if (capacity < tmp) {
            ((two_t)((void *)SzFree))(NULL, work);
            work = ((reserve_t)((void *)SzAlloc))(NULL, tmp << 1);
            capacity = tmp;
            if (work == NULL) {
                state_final = 2;
                goto clear_listing;
            }
        }
        ((index_t)((void *)SzArEx_GetFileNameUtf16))(state, idx, work);
        ((one_t)((void *)Buf_Init))(name);
        if (archive_7z_name_from_utf16(name, work) != 0)
            goto no_matches;

        {
            char *str = (char *)name_buf->data;
            char *extension = ((strrchr_t)sym_libc_strrchr)(str, '.');
            if (extension == NULL ||
                ((strcasecmp_t)sym_libc_strcasecmp)(extension + 1, suffix) != 0)
                goto no_matches;
        }

        ((two_t)((void *)Buf_Free))(name, (void *)&recon_archive_alloc);
        wr32(output_size, (uint32_t)rd64(reg + 8));

        {
            map = rd32((void *)(uintptr_t)
                (rd64(state + 96) + (idx << 2)));
            if (map == UINT32_MAX)
                goto no_matches;
            descriptor = (uint8_t *)ar->db.Folders + (uint64_t)map * 0x38;
            source = (uint8_t *)((CSzFolder *)(void *)descriptor)->Coders;
            reference = ((reference_t)((void *)SzFolder_GetUnpackSize))(descriptor);
            if (rd32(descriptor + 32) != 1)
                goto clear_listing;
            class = rd64(source + 8);
            if (class != 0 && class != 0x30101 && class != 0x21)
                goto clear_listing;
            selection = ((uint64_t (*)(void *, uint32_t, void *))((void *)SzArEx_GetFolderStreamPos))
                    (state, map, NULL);
            ((void (*)(void *, UInt64))((void *)LookInStream_SeekTo))(ctx, selection);
        }

        found = 1;
        break;

no_matches:
        accumulated += rd32(reg + 8);
        goto next;

next:
        idx++;
        offset += 0x20;
        if (idx >= counter)
            goto clear_listing;
    }

    if (!found)
        goto clear_listing;

    if (dest != 0) {
        ((one_t)((void *)FileOutStream_CreateVTable))(output);
        if (((one_t)((void *)File_Construct))(output + 8),
            ((int (*)(void *, const char *))((void *)OutFile_Open))(output + 8, dest) == 0) {
            state_final = 1;
            goto clear_direct;
        }
    } else {
        uint32_t size = rd32(output_size);
        memory = ((malloc_t)sym_libc_malloc)(size);
        out_stream->Write = archive_7z_window_sink_write;
        wr_ptr(output + 8, memory);
        wr64(output + 16, 0);
        wr32(output + 24, size);
        wr32(output + 28, 0);
        if (memory == NULL) {
            state_final = 2;
            goto clear_listing;
        }
    }

    {
        int32_t first = (int32_t)ar->FolderStartFileIndex[map];
        uint32_t last = (uint32_t)idx;
        uint64_t sum = 0;
        if ((uint32_t)first <= last) {
            uint32_t i;
            for (i = (uint32_t)first; i <= last; i++) {
                uint8_t *r = (uint8_t *)ar->db.Files + (uint64_t)i * 0x20;
                sum += rd32(r + 8);
            }
        }
        remaining = sum;
        ARCHIVE_PROGRESS->total = sum;
        ARCHIVE_PROGRESS->done = 0;
    }

    if (class == 0) {
        void *dest = output;
        if (accumulated != 0) {
            look->s.Skip(look, accumulated);
            ARCHIVE_PROGRESS->done = ARCHIVE_PROGRESS->done + accumulated;
            remaining -= accumulated;
        }
        while ((int64_t)remaining >= 1) {
            uint64_t requested = remaining < 0x10000 ? remaining : 0x10000;
            if (((read_t)(void *)look->s.Read)
                    (ctx, entry_b, &requested, requested) != 0) {
                state_final = 1;
                goto clear_direct;
            }
            n_read = ((ISeqOutStream *)dest)->Write(dest, entry_b, requested);
            if (n_read != requested) {
                state_final = 1;
                goto clear_direct;
            }
            ARCHIVE_PROGRESS->done = ARCHIVE_PROGRESS->done + n_read;
            remaining -= n_read;
        }
        goto clear_direct;
    }

    if (class == 0x30101) {
        wr64(filter + 16, 0);
        wr64(filter + 24, 0);
        ((init_t)((void *)LzmaDec_Allocate))(filter, ((CSzCoderInfo *)(void *)source)->Props.data, (uint32_t)((CSzCoderInfo *)(void *)source)->Props.size,
                                    obj + 24);
        ((one_t)((void *)LzmaDec_Init))(filter);
    } else {
        wr64(filter + 16, 0);
        wr64(filter + 24, 0);
        ((int (*)(void *, uint8_t, void *))((void *)Lzma2Dec_Allocate))
            (filter, ((CSzCoderInfo *)(void *)source)->Props.data[0], obj + 24);
        ((one_t)((void *)Lzma2Dec_Init))(filter);
    }

    consumed = accumulated;
    shift_block = 0;
    n_read = 0;
    for (;;) {
        if (shift_block == n_read) {
            n_read = 0x10000;
            if (((read_t)(void *)look->s.Read)
                    (ctx, entry_b, &n_read, n_read) != 0) {
                state_final = 1;
                goto clear_special;
            }
            shift_block = 0;
        }

        tmp = n_read - shift_block;
        dest_len = 0x10000;
        code = 0;
        if (reference != UINT64_MAX && (reference >> 16) == 0) {
            code = 1;
            dest_len = reference;
        }
        src_len = tmp;
        if (class == 0x30101) {
            state_final = ((filter_t)((void *)LzmaDec_DecodeToBuf))
                (filter, decoded, &dest_len, entry_b + shift_block,
                 &src_len, code, &status);
        } else {
            state_final = ((filter_t)((void *)Lzma2Dec_DecodeToBuf))
                (filter, decoded, &dest_len, entry_b + shift_block,
                 &src_len, code, &status);
        }

        prev = dest_len;
        tmp = src_len;
        remaining -= prev;

        if (consumed < 1) {
            per_write = prev;
            counter = 0;
        } else {
            int64_t difference = (int64_t)consumed - (int64_t)prev;
            if (difference >= 0) {
                per_write = 0;
                counter = (uint64_t)difference;
            } else {
                per_write = (uint64_t)(-difference);
                counter = (uint64_t)difference;
            }
        }

        if ((int64_t)remaining >= 0) {
            ARCHIVE_PROGRESS->done = ARCHIVE_PROGRESS->done + prev;
            if (per_write != 0) {
            uint8_t *p = decoded + (consumed < prev ? consumed : 0);
                uint64_t written = out_stream->Write(output, p, per_write);
                if (written != per_write) {
                    state_final = 1;
                    goto clear_special;
                }
            }
        } else {
            per_write += (uint32_t)remaining;
            remaining = 0;
            ARCHIVE_PROGRESS->done = ARCHIVE_PROGRESS->total;
            if (per_write != 0) {
                uint8_t *p = decoded + (consumed < prev ? consumed : 0);
                uint64_t written = out_stream->Write(output, p, per_write);
                if (written != per_write) {
                    state_final = 1;
                    goto clear_special;
                }
            }
        }

        if (state_final != 0 || (reference != UINT64_MAX && remaining == 0))
            goto clear_special;
        shift_block += tmp;
        reference -= prev;
        if (dest_len == 0 && tmp == 0)
            goto clear_special;
    }

clear_special:
    ((two_t)((void *)LzmaDec_Free))(filter, obj + 24);

clear_direct:
    if (dest != 0) {
        ((one_t)((void *)File_Close))(output + 8);
        result = (void *)(uintptr_t)(remaining == 0);
    } else {
        result = memory;
    }

clear_listing:
    ((three_t)((void *)SzArEx_Free))(state, obj + 24, NULL);
    ((two_t)((void *)SzFree))(NULL, work);
    ((one_t)((void *)File_Close))(listing + 16);

end:
    if (state_final != 0 || rd32(output_size) == 0)
        return NULL;
    return result;
}

ISzAlloc recon_archive_alloc = { SzAlloc, SzFree };
typedef uint64_t (*fn_release_buffer)(void *buf, void *allocator);
typedef uint32_t (*fn_reserve)(void *buf, uint64_t capacity, void *allocator);





static const uint8_t utf8_lead_byte[5] = { 0xc0u, 0xe0u, 0xf0u, 0xf8u, 0xfcu };

static uint32_t utf8_prefix(uint32_t cp)
{
    if (cp < 0x800u)
        return 1u;
    if ((cp >> 16) == 0)
        return 2u;
    if (cp < 0x200000u)
        return 3u;
    return 4u + ((cp >> 26) != 0);
}

uint32_t archive_7z_name_from_utf16(void *param_1, const void *param_2)
{

    CBuf *buf = (CBuf *)param_1;
    const uint8_t *entry = (const uint8_t *)param_2;
    uint64_t units = 0;

    for (;;) {
        uint16_t unit = rd16(entry + units * 2u);

        ++units;
        if (unit == 0)
            break;
    }

    uint64_t limit = (uint64_t)((uint32_t)units - 1u);
    uint64_t bytes = 0;

    if (limit != 0) {
        uint64_t idx = 0;

        for (;;) {
            uint32_t cp = rd16(entry + idx * 2u);
            uint64_t next = idx + 1u;

            if (cp <= 0x7fu) {
                ++bytes;
            } else {
                if ((cp >> 11) == 0x1bu) {
                    uint16_t low;

                    if (next == limit || (cp >> 10) > 0x36u)
                        break;
                    low = rd16(entry + next * 2u);
                    if ((low >> 10) != 0x37u)
                        break;
                    next = idx + 2u;
                    cp = (cp << 10) - (UTF16_HIGH_SURROGATE_BASE << 10);
                    cp |= (uint32_t)low - UTF16_LOW_SURROGATE_BASE;
                    cp += UTF16_SUPPLEMENTARY_BASE;
                }
                bytes += (uint64_t)(utf8_prefix(cp) - 1u) + 2u;
            }

            idx = next;
            if (idx == limit)
                break;
        }
    }

    {
        uint64_t needed = bytes + 1u;

        if (buf->size < needed) {
            ((fn_release_buffer)((void *)Buf_Free))(buf, (void *)&recon_archive_alloc);
            if (((fn_reserve)((void *)Buf_Create))(buf, needed,
                                          (void *)&recon_archive_alloc) == 0)
                return 2u;
        }
    }

    {
        uint8_t *output = buf->data;
        uint64_t position = 0;

        if ((uint32_t)units != 1u) {
            uint64_t idx = 0;

            for (;;) {
                uint32_t cp = rd16(entry + idx * 2u);
                uint64_t next = idx + 1u;

                if (cp <= 0x7fu) {
                    if (output != NULL)
                        wr8(output + position, (uint8_t)cp);
                    ++position;
                } else {
                    uint32_t prefix;

                    if ((cp >> 11) == 0x1bu) {
                        uint16_t low;

                        if (next == limit || (cp >> 10) > 0x36u) {
                            output = buf->data;
                            wr8(output + position, 0);
                            return 11u;
                        }
                        low = rd16(entry + next * 2u);
                        if ((low >> 10) != 0x37u) {
                            output = buf->data;
                            wr8(output + position, 0);
                            return 11u;
                        }
                        next = idx + 2u;
                        cp = (cp << 10) - (UTF16_HIGH_SURROGATE_BASE << 10);
                        cp |= (uint32_t)low - UTF16_LOW_SURROGATE_BASE;
                        cp += UTF16_SUPPLEMENTARY_BASE;
                    }

                    prefix = utf8_prefix(cp);
                    if (output != NULL) {
                        uint32_t offset = (prefix * 6u) & 31u;
                        uint8_t first = utf8_lead_byte[prefix - 1u];

                        wr8(output + position,
                             (uint8_t)(first + (cp >> offset)));
                    }
                    ++position;

                    {
                        uint32_t offset = prefix * 6u - 6u;
                        uint32_t remaining = prefix;

                        while (remaining != 0) {
                            if (output != NULL) {
                                uint8_t byte = (uint8_t)(0x80u |
                                    ((cp >> (offset & 31u)) & 0x3fu));

                                wr8(output + position, byte);
                            }
                            ++position;
                            offset -= 6u;
                            --remaining;
                        }
                    }
                }

                idx = next;
                if (idx == limit)
                    break;
            }

            output = buf->data;
        }

        wr8(output + position, 0);
    }

    return 0;
}

extern ISzAlloc recon_archive_alloc;



int archive_7z_read_range(const char *container, const char *extension,
                       uint32_t *found_size, void *dest,
                       uint32_t dest_size, int32_t offset) {

    typedef int (*fn_open_fd)(void *, const char *);
    typedef void (*fn_un_arg)(void *);
    typedef void (*fn_un_zero)(void *, uint32_t);
    typedef void (*fn_no_args)(void);
    typedef int (*fn_parse)(void *, void *, void *, void *);
    typedef uint64_t (*fn_name)(void *, uint64_t, void *);
    typedef void *(*fn_alloc)(void *, uint64_t);
    typedef void (*fn_release)(void *, void *);
    typedef void (*fn_start_name)(void *);
    typedef void (*fn_assign_name)(void *, void *);
    typedef uint64_t (*fn_unpack_size)(void *);
    typedef uint64_t (*fn_resource)(void *, uint32_t, uint32_t);
    typedef void (*fn_prepare)(void *, uint64_t);
    typedef void (*fn_header_a)(void *, void *, uint32_t, void *);
    typedef void (*fn_header_b)(void *, uint32_t, void *);
    typedef void (*fn_start_a)(void *);
    typedef void (*fn_start_b)(void *);
    typedef void (*fn_close_parser)(void *, void *);
    typedef int (*fn_step_a)(void *, void *, void *, void *, void *,
                             uint32_t, void *);
    typedef int (*fn_step_b)(void *, void *, void *, void *, void *,
                             uint32_t, void *);
    typedef int (*fn_read_direct)(void *, void *, void *);
    typedef char *(*fn_strrchr)(const char *, int);
    typedef int (*fn_strcasecmp)(const char *, const char *);

    archive_7z_range_frame_t frame;
    archive_7z_range_frame_t *const p = &frame;
    CSzArEx *const parser = &p->db;
    Byte *const block_entry = p->in_buffer;
    CLookToRead *const block_output = &p->look_stream;
    uint8_t *name = 0;
    uint64_t capacity_name = 0;
    uint64_t idx = 0;
    uint64_t advance_reg = 0;
    uint32_t accumulated = 0;
    uint32_t mark_output = 0;
    uint32_t pending = dest_size;
    int32_t result;

    p->alloc.Alloc = SzAlloc;
    p->alloc.Free = SzFree;
    p->alloc_temp.Alloc = SzAllocTemp;
    p->alloc_temp.Free = SzFreeTemp;
    wr32(found_size, 0);
    result = ((fn_open_fd)((void *)InFile_Open))(&p->archive_stream.file, container);
    if (result != 0)
        return -1;

    p->request.extension = extension;
    ((fn_un_arg)((void *)FileInStream_CreateVTable))(&p->archive_stream);
    ((fn_un_zero)((void *)LookToRead_CreateVTable))(block_output, 0);
    block_output->realStream = &p->archive_stream.s;
    ((fn_un_arg)((void *)LookToRead_Init))(block_output);
    ((fn_no_args)((void *)CrcGenerateTable))();
    ((fn_un_arg)((void *)SzArEx_Init))(parser);
    result = ((fn_parse)((void *)SzArEx_Open))(parser, block_output,
                                         &p->alloc, &p->alloc_temp);
    if (result != 0 || parser->db.NumFiles == 0) {
        mark_output = 0;
        goto output;
    }

    p->request.offset = (uint32_t)offset;
    p->request.destination = dest;
    p->request.file = &p->archive_stream.file;
    p->request.remaining = dest_size;
    idx = 0;
    advance_reg = 0;
    capacity_name = 0;
    name = 0;
    accumulated = 0;

    for (;;) {
        CSzFileItem *regs = parser->db.Files;
        CSzFileItem *reg = (CSzFileItem *)((uint8_t *)regs + advance_reg);
        uint64_t n;
        uint32_t id_resource;
        CSzFolder *description;
        CSzCoderInfo *source;
        uint64_t type;

        if (reg->IsDir != 0)
            goto next;

        n = ((fn_name)((void *)SzArEx_GetFileNameUtf16))(parser, idx, 0);
        if (capacity_name < n) {
            ((fn_release)((void *)SzFree))(0, name);
            name = ((fn_alloc)((void *)SzAlloc))(0, n << 1);
            capacity_name = n;
            if (name == 0)
                break;
        }
        ((fn_name)((void *)SzArEx_GetFileNameUtf16))(parser, idx, name);

        id_resource = parser->FileIndexToFolderIndexMap[idx];
        if (id_resource == UINT32_MAX)
            goto next;
        description = &parser->db.Folders[id_resource];
        if (description->NumCoders != 1)
            goto end_search;
        source = description->Coders;
        type = source->MethodID;
        if (type != ARCHIVE_7Z_METHOD_COPY && type != ARCHIVE_7Z_METHOD_LZMA &&
            type != ARCHIVE_7Z_METHOD_LZMA2)
            goto end_search;

        ((fn_start_name)((void *)Buf_Init))(&p->name);
        if (archive_7z_name_from_utf16(&p->name, name) != 0)
            goto no_matches;
        {
            char *point = ((fn_strrchr)sym_libc_strrchr)(
                (const char *)p->name.data, 0x2e);
            if (point == 0 ||
                ((fn_strcasecmp)sym_libc_strcasecmp)(point + 1, extension) != 0)
                goto no_matches;
        }

        ((fn_assign_name)((void *)Buf_Free))(&p->name, (void *)&recon_archive_alloc);
        {
            uint64_t entry_size = reg->Size;
            uint32_t left;
            uint32_t threshold;
            uint64_t unpack_size;
            uint64_t resource;

            wr32(found_size, (uint32_t)entry_size);
            mark_output = 1;
            pending = 0;
            if (dest == 0 || dest_size == 0)
                goto output;
            if (entry_size < (uint64_t)((uint32_t)offset + dest_size))
                goto error_output;

            threshold = accumulated + (uint32_t)offset;
            unpack_size = ((fn_unpack_size)((void *)SzFolder_GetUnpackSize))(description);
            resource = ((fn_resource)((void *)SzArEx_GetFolderStreamPos))(parser, id_resource, 0);
            ((fn_prepare)((void *)LookInStream_SeekTo))(block_output, resource);
            p->read_size = 0;
            p->sink.write = archive_7z_window_sink_write;
            p->sink.destination = (uint8_t *)dest;
            p->sink.cursor = 0;
            p->sink.position = 0;
            p->sink.room = dest_size;
            p->sink.threshold = threshold;

            type = source->MethodID;
            if (type == ARCHIVE_7Z_METHOD_COPY) {
                ((fn_prepare)((void *)LookInStream_SeekTo))(block_output, resource + threshold);
                p->sink.threshold = 0;
                left = dest_size;
                for (;;) {
                    size_t chunk = left < ARCHIVE_7Z_BLOCK_BYTES ? left : ARCHIVE_7Z_BLOCK_BYTES;
                    uint64_t written;

                    if (((fn_read_direct)block_output->s.Read)(block_output, block_entry, &chunk) != 0)
                        break;
                    written = p->sink.write(&p->sink, block_entry, chunk);
                    if (written != chunk)
                        break;
                    left -= (uint32_t)written;
                    if (left == 0)
                        break;
                }
                pending = left;
                goto output;
            }

            p->decoder.decoder.probs = NULL;
            p->decoder.decoder.dic = NULL;
            if (type == ARCHIVE_7Z_METHOD_LZMA) {
                ((fn_header_a)((void *)LzmaDec_Allocate))(&p->decoder.decoder, source->Props.data,
                                            (uint32_t)source->Props.size, &p->alloc);
                ((fn_start_a)((void *)LzmaDec_Init))(&p->decoder.decoder);
            } else if (type == ARCHIVE_7Z_METHOD_LZMA2) {
                ((fn_header_b)((void *)Lzma2Dec_Allocate))(&p->decoder, source->Props.data[0], &p->alloc);
                ((fn_start_b)((void *)Lzma2Dec_Init))(&p->decoder);
            } else {
                goto error_output;
            }

            {
                uint64_t position = 0;
                uint64_t available = 0;
                uint64_t rest = unpack_size;
                const int uses_a = type == ARCHIVE_7Z_METHOD_LZMA;

                for (;;) {
                    if (position == available) {
                        p->read_size = ARCHIVE_7Z_BLOCK_BYTES;
                        int r = ((fn_read_direct)block_output->s.Read)(
                            block_output, block_entry, &p->read_size);
                        if (r != 0)
                            goto end_compressed;
                        available = p->read_size;
                        position = 0;
                    }

                    {
                        uint32_t flag = 0;
                        uint64_t no_read = available - position;
                        p->out_size = ARCHIVE_7Z_BLOCK_BYTES;
                        p->in_size = no_read;
                        if (rest != UINT64_MAX && (rest >> 16) == 0) {
                            flag = 1;
                            p->out_size = rest;
                        }
                        if (uses_a) {
                            result = ((fn_step_a)((void *)LzmaDec_DecodeToBuf))(
                                &p->decoder.decoder, p->out_buffer, &p->out_size,
                                block_entry + position, &p->in_size, flag,
                                &p->status);
                        } else {
                            result = ((fn_step_b)((void *)Lzma2Dec_DecodeToBuf))(
                                &p->decoder, p->out_buffer, &p->out_size,
                                block_entry + position, &p->in_size, flag,
                                &p->status);
                        }
                        {
                            uint64_t taken = p->out_size;
                            uint64_t advanced = p->in_size;
                            uint64_t delivered = p->sink.write(
                                &p->sink, p->out_buffer, taken);
                            left = p->request.remaining - (uint32_t)delivered;
                            p->request.remaining = left;
                            rest -= taken;
                            if ((int32_t)left < 1 || result != 0 ||
                                (rest == 0 && unpack_size != UINT64_MAX))
                                goto end_compressed;
                            position += advanced;
                            if ((p->out_size | p->in_size) == 0)
                                goto end_compressed;
                        }
                    }
                }
            }

end_compressed:
            LzmaDec_Free(&p->decoder.decoder, &p->alloc);
            pending = p->request.remaining;
            mark_output = 1;
            goto output;
        }

no_matches:
        accumulated += (uint32_t)reg->Size;
next:
        idx++;
        advance_reg += sizeof(CSzFileItem);
        if (idx >= p->db.db.NumFiles)
            break;
    }

end_search:
    mark_output = 0;
output:
    ((fn_close_parser)((void *)SzArEx_Free))(parser, &p->alloc);
    ((fn_release)((void *)SzFree))(0, name);
    ((fn_un_arg)((void *)File_Close))(&p->archive_stream.file);
    return (pending != 0 || mark_output == 0) ? -1 : 0;

error_output:
    pending = p->request.remaining;
    mark_output = 1;
    goto output;
}

typedef int (*fn_fgetc)(void *);

uint32_t archive_zip_read_u16(void *param_1)
{

    static fn_fgetc core_fgetc;
    if (!core_fgetc) core_fgetc = (fn_fgetc)sym_libc_fgetc;

    void *stream = param_1;

    uint32_t b0 = (uint32_t)core_fgetc(stream);

    uint32_t b1 = (uint32_t)core_fgetc(stream);

    return b0 | (b1 << 8);
}

typedef int (*fn_fgetc_13)(void *);

uint32_t archive_zip_read_u32(void *param_1)
{

    static fn_fgetc_13 core_fgetc;
    if (!core_fgetc) core_fgetc = (fn_fgetc_13)sym_libc_fgetc;

    void *stream = param_1;

    uint32_t b0 = (uint32_t)core_fgetc(stream);

    uint32_t b1 = (uint32_t)core_fgetc(stream);

    uint32_t half_low = b0 | (b1 << 8);

    uint32_t b2 = (uint32_t)core_fgetc(stream);

    uint32_t b3 = (uint32_t)core_fgetc(stream);

    uint32_t half_high = b2 | (b3 << 8);

    return (half_low & 0xffffu) | ((half_high & 0xffffu) << 16);
}

typedef long (*fn_read_14)(int, void *, unsigned long);




uint32_t archive_zip_read_sector(void *param_1, uint8_t **param_2)
{

    const uint8_t *base = (const uint8_t *)param_1;

    uint8_t *field8_1 = rd_ptr_u8(base + 8);

    *param_2 = field8_1;

    uint32_t fd = rd32(base + 16);

    void *field8_2 = rd_ptr(base + 8);

    return ((fn_read_14)sym_libc_read)((int)fd, field8_2, 0x200);
}

#define O_DEST  0x00u
#define O_CAP   0x14u
#define O_READY 0x18u
#define O_TOTAL 0x1cu





int32_t archive_zip_window_sink_write(void *sink, uint8_t *src, uint32_t delta)
{
    uint8_t *st = sink;

    uint32_t ready_v = rd32(st + O_READY);
    uint32_t total_v = rd32(st + O_TOTAL);
    uint32_t updated_total = total_v + delta;
    wr32(st + O_TOTAL, updated_total);

    if (updated_total < ready_v) {

        uint32_t cap = rd32(st + O_CAP);
        return (cap == 0u) ? 1u : 0u;
    }

    uint32_t difference = updated_total - ready_v;
    uint32_t cap_a = rd32(st + O_CAP);
    uint8_t *dest = rd_ptr_u8(st + O_DEST);
    uint32_t off_origin = ready_v - total_v;
    const uint8_t *origin = src + off_origin;

    uint32_t n = (difference > cap_a) ? cap_a : difference;

    memcpy(dest, origin, n);

    uint32_t cap_b   = rd32(st + O_CAP);
    uint32_t ready_b = rd32(st + O_READY);
    uint8_t *dest_b  = rd_ptr_u8(st + O_DEST);

    uint32_t cap_updated   = cap_b - n;
    uint32_t ready_updated = ready_b + n;
    uint8_t *dest_updated  = dest_b + n;

    wr32(st + O_CAP, cap_updated);
    wr32(st + O_READY, ready_updated);
    wr_ptr(st + O_DEST, dest_updated);

    return (cap_updated == 0u) ? 1u : 0u;
}
#undef O_DEST
#undef O_CAP
#undef O_READY
#undef O_TOTAL

typedef int64_t (*fn_lseek_16)(int32_t, int64_t, int32_t);
typedef void *(*fn_mmap_16)(void *, uint64_t, int32_t, int32_t, int32_t, int64_t);
typedef size_t (*fn_strlen)(const char *);
typedef int (*fn_memcmp)(const void *, const void *, size_t);
typedef int32_t (*fn_munmap_16)(void *, uint64_t);







int32_t archive_zip_find_entry(int32_t descriptor, const char *name,
                            void *param_3, uint32_t max)
{

    static fn_lseek_16 core_lseek;
    static fn_mmap_16 core_mmap;
    static fn_strlen core_strlen;
    static fn_memcmp core_memcmp;
    static fn_munmap_16 core_munmap;
    int64_t signed_size;
    uint64_t size;
    void *map;
    uintptr_t base;
    uintptr_t end_dir;
    uintptr_t entry;
    uintptr_t end_map;
    uint32_t result = UINT32_MAX;
    uint32_t sought = 0;
    size_t len_name;

    if (descriptor < 1)
        return -1;

    if (!core_lseek)
        core_lseek = (fn_lseek_16)sym_libc_lseek;
    signed_size = core_lseek(descriptor, 0, 2);
    size = (uint64_t)signed_size;
    core_lseek(descriptor, 0, 0);

    if (!core_mmap)
        core_mmap = (fn_mmap_16)sym_libc_mmap;
    map = core_mmap(NULL, size, 1, 1, descriptor, 0);
    if ((uintptr_t)map == UINTPTR_MAX)
        return -1;

    base = (uintptr_t)map;
    if (!core_strlen)
        core_strlen = (fn_strlen)sym_libc_strlen;
    len_name = core_strlen(name);

    if (rd32((const void *)base) != UINT32_C(0x04034b50))
        goto cleanup;

    {
        uintptr_t cursor = base + size - UINT64_C(0x16);
        uint32_t signature_end;

        for (;;) {
            uintptr_t before = cursor;

            signature_end = rd32((const void *)cursor);
            cursor -= 1;
            if (before <= base || signature_end == UINT32_C(0x06054b50))
                break;
        }

        if (signature_end != UINT32_C(0x06054b50))
            goto cleanup;

        {
            uint32_t offset_dir = rd32((const void *)(cursor + 17));

            if (size <= (uint64_t)offset_dir ||
                (int64_t)size <= (int64_t)(uint64_t)offset_dir)
                goto cleanup;

            end_dir = cursor + 1;
            entry = base + (uint64_t)offset_dir;
        }
    }

    end_map = base + size;
    for (;;) {
        uint32_t offset_local;
        uint16_t len_file;

        if (rd32((const void *)entry) != UINT32_C(0x02014b50))
            goto cleanup;

        offset_local = rd32((const void *)(entry + 42));
        if (size <= (uint64_t)offset_local)
            goto cleanup;

        if (max != 0) {
            sought += 1;
            if (sought > max)
                goto cleanup;
        }

        len_file = rd16((const void *)(entry + 28));
        if ((int64_t)(end_dir - (entry + 46)) <
            (int64_t)(uint64_t)len_file)
            goto cleanup;

        if (!core_memcmp)
            core_memcmp = (fn_memcmp)sym_libc_memcmp;
        if (core_memcmp((const void *)(entry + 46 - (uint64_t)len_name +
                        (uint64_t)len_file), name, len_name) == 0) {
            uintptr_t local = base + (uint64_t)offset_local;
            uint32_t end_data;
            uint32_t is_zero;
            uint32_t outside;

            if (rd32((const void *)local) != UINT32_C(0x04034b50))
                goto cleanup;

            wr64(param_3, rd64((const void *)(entry + 20)));
            wr16((uint8_t *)param_3 + 12, rd16((const void *)(entry + 10)));

            end_data = offset_local + (uint32_t)rd16((const void *)(local + 26));
            end_data += (uint32_t)rd16((const void *)(local + 28));
            end_data += 30;
            is_zero = (end_data == 0);
            outside = (size <= (uint64_t)end_data);
            wr32((uint8_t *)param_3 + 8, end_data);
            result = (is_zero | outside) ? UINT32_MAX : 0;
            goto cleanup;
        }

        {
            uint64_t advance = (uint64_t)len_file +
                               (uint64_t)rd16((const void *)(entry + 30));

            advance += (uint64_t)rd16((const void *)(entry + 32));
            advance += 46;
            if (advance > size)
                goto cleanup;

            entry += advance;
            result = UINT32_MAX;
            if (entry < end_map)
                continue;
        }
        goto cleanup;
    }

cleanup:
    if (!core_munmap)
        core_munmap = (fn_munmap_16)sym_libc_munmap;
    core_munmap(map, size);
    return (int32_t)result;
}

typedef int32_t (*fn_close_17)(int32_t);
typedef int64_t (*fn_lseek_17)(int32_t, int64_t, int32_t);
typedef int64_t (*fn_read_17)(int32_t, void *, uint64_t);
typedef int64_t (*fn_read_chk)(int32_t, void *, uint64_t, uint64_t);
typedef void *(*fn_memcpy)(void *, const void *, uint64_t);
typedef void *(*fn_malloc)(uint64_t);
typedef void (*fn_free_17)(void *);
typedef int32_t (*fn_inflate_back_init)(void *, int32_t, void *, const char *, uint32_t);
typedef uint32_t (*fn_entry_zlib)(void *, uint8_t **);
typedef int32_t (*fn_output_zlib)(void *, uint8_t *, uint32_t);
typedef int32_t (*fn_inflate_back)(void *, fn_entry_zlib, void *, fn_output_zlib, void *);
typedef int32_t (*fn_inflate_back_end)(void *);
typedef int32_t (*fn_inflate_init2)(void *, int32_t, const char *, uint32_t);
typedef int32_t (*fn_inflate)(void *, int32_t);
typedef int32_t (*fn_inflate_end)(void *);






int32_t archive_zip_open_entry(const char *path, const char *name,
                            uint32_t *output, void *dest,
                            uint32_t count, uint32_t jump)
{

    uint8_t data[16];
    uint8_t sector[0x200];
    uint8_t stream[0x70];
    uint8_t state[32];
    fn_close_17 close_fn = (fn_close_17)sym_libc_close;
    fn_lseek_17 find = (fn_lseek_17)sym_libc_lseek;
    fn_read_17 read_fn = (fn_read_17)sym_libc_read;
    fn_read_chk read_checked = (fn_read_chk)fortify_read;
    fn_memcpy copy = (fn_memcpy)sym_libc_memcpy;
    int32_t fd = nds_platform_default()->files.open_fd(nds_platform_default()->user, path, 0);

    if (fd == -1 || archive_zip_find_entry(fd, name, data, 6) != 0)
        goto fail;

    if (dest == NULL || count == 0) {
        close_fn(fd);
        wr32(output, rd32(data + 4));
        return 0;
    }

    {
        uint32_t position = rd32(data + 8);
        uint32_t base_sector = position & UINT32_C(0xfffffe00);
        uint32_t inside_sector = position & UINT32_C(0x1ff);
        uint32_t available;
        uint16_t method;
        uint8_t *origin;

        find(fd, (int64_t)(uint64_t)base_sector, 0);
        if (read_fn(fd, sector, 0x200) < 1)
            goto fail;

        origin = sector + inside_sector;
        available = UINT32_C(0x200) - inside_sector;
        method = rd16(data + 12);

        if (method == 8) {
            uint32_t compressed = rd32(data);
            uint32_t entry = available > compressed ? compressed : available;

            wr64(stream + 64, 0);
            wr64(stream + 72, 0);
            wr_ptr(stream + 24, dest);
            wr_ptr(stream, origin);

            if (jump != 0) {
                void *window = ((fn_malloc)sym_libc_malloc)(0x8000);
                int32_t r;

                if (window == NULL)
                    goto fail;

                r = ((fn_inflate_back_init)((void *)inflateBackInit_))(
                    stream, 15, window, "1.2.7", 0x70);
                wr32(stream + 8, entry);
                if (r == 0) {

                    wr_ptr(state, dest);
                    wr_ptr(state + 8, sector);
                    wr32(state + 16, (uint32_t)fd);
                    wr32(state + 20, count);
                    wr32(state + 24, jump);
                    wr32(state + 28, 0);

                    ((fn_inflate_back)((void *)inflateBack))(
                        stream,
                        archive_zip_read_sector,
                        state,
                        archive_zip_window_sink_write,
                        state);
                    ((fn_inflate_back_end)((void *)inflateBackEnd))(stream);
                    close_fn(fd);

                    if (rd32(state + 20) == 0) {
                        ((fn_free_17)sym_libc_free)(window);
                        return 0;
                    }
                }

                ((fn_free_17)sym_libc_free)(window);
                goto end;
            }

            {
                int32_t r;
                uint32_t remaining;

                wr32(stream + 32, count);
                r = ((fn_inflate_init2)((void *)inflateInit2_))(
                    stream, -15, "1.2.7", 0x70);
                wr32(stream + 8, entry);
                if (r != 0)
                    goto end;

                r = ((fn_inflate)((void *)inflate))(stream, 2);
                if (rd32(stream + 32) == 0) {
                    wr32(output, count);
                    ((fn_inflate_end)((void *)inflateEnd))(stream);
                    close_fn(fd);
                    return 0;
                }
                if (r != -5)
                    goto fail;

                remaining = compressed - entry;
                wr32(stream + 8, 0x200);
                wr_ptr(stream, sector);
                if (remaining != 0) {
                    uint32_t request = remaining < 0x200 ? remaining : 0x200;
                    if (read_checked(fd, sector, request, UINT64_MAX) < 1)
                        goto fail;
                }
                ((fn_inflate_end)((void *)inflateEnd))(stream);
                goto end;
            }
        }

        if (method != 0)
            goto end;

        if (jump != 0) {
            uint32_t after_jump = available - jump;
            if (available <= jump) {
                find(fd, (int64_t)(uint64_t)(jump - available), 1);
                available = 0;
            } else {
                origin += jump;
                available = after_jump;
            }
        }

        if (available > count) {
            copy(dest, origin, count);
            goto end;
        }

        if (available != 0)
            copy(dest, origin, available);

        {
            uint32_t missing = count - available;
            uint8_t *write = (uint8_t *)dest + available;

            while (missing != 0) {
                uint32_t request = missing < 0x200 ? missing : 0x200;
                if (read_checked(fd, sector, request, UINT64_MAX) < 1)
                    goto fail;
                copy(write, sector, request);
                missing -= request;
                write += request;
            }
        }
    }

end:
    wr32(output, rd32(data + 4));
    close_fn(fd);
    return 0;

fail:
    close_fn(fd);
    return -1;
}

typedef int64_t (*fn_lseek_18)(int32_t, int64_t, int32_t);
typedef int64_t (*fn_read_chk_18)(int32_t, void *, uint64_t, uint64_t);
typedef int32_t (*fn_close_18)(int32_t);
typedef int32_t (*fn_fclose)(void *);
typedef void *(*fn_malloc_18)(uint64_t);
typedef void (*fn_free_18)(void *);
typedef size_t (*fn_fwrite)(const void *, size_t, size_t, void *);

typedef struct {
    uint8_t *next_in;
    uint32_t avail_in;
    uint32_t pad_in;
    uint64_t total_in;
    uint8_t *next_out;
    uint32_t avail_out;
    uint32_t pad_out;
    uint64_t total_out;
    char *msg;
    void *state;
    void *zalloc;
    void *zfree;
    void *opaque;
    int32_t data_type;
    uint32_t pad_type;
    uint64_t adler;
    uint64_t reserved;
} z_stream_core;
typedef int32_t (*fn_inflate_init2_18)(z_stream_core *, int32_t, const char *, int32_t);
typedef int32_t (*fn_inflate_18)(z_stream_core *, int32_t);
typedef int32_t (*fn_inflate_end_18)(z_stream_core *);




void *archive_zip_extract_entry(const char *path, const char *name,
                         uint32_t *size_output, const char *dest)
{

    static fn_lseek_18 core_lseek;
    static fn_read_chk_18 core_read_chk;
    static fn_close_18 core_close;
    static fn_fclose core_fclose;
    static fn_malloc_18 core_malloc;
    static fn_free_18 core_free;
    static fn_fwrite core_fwrite;
    static fn_inflate_init2_18 core_inflate_init2;
    static fn_inflate_18 core_inflate;
    static fn_inflate_end_18 core_inflate_end;
    uint8_t data_entry[16];
    int32_t descriptor;
    void *stream = NULL;
    void *buffer_work = NULL;
    void *result = NULL;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t position;
    uint16_t method;

    descriptor = nds_platform_default()->files.open_fd(nds_platform_default()->user, path, 0);
    if (dest != NULL) {
        stream = nds_platform_default()->files.open(nds_platform_default()->user, dest,
                                             "wb");
        if (stream == NULL) {
            if (!core_close)
                core_close = (fn_close_18)sym_libc_close;
            core_close(descriptor);
            return NULL;
        }
    }

    if (descriptor == -1 ||
        archive_zip_find_entry(descriptor, name, data_entry, 0) != 0) {
        if (!core_close)
            core_close = (fn_close_18)sym_libc_close;
        core_close(descriptor);
        if (stream != NULL) {
            if (!core_fclose)
                core_fclose = (fn_fclose)sym_libc_fclose;
            core_fclose(stream);
        }
        return NULL;
    }

    compressed_size = rd32(data_entry);
    uncompressed_size = rd32(data_entry + 4);
    position = rd32(data_entry + 8);
    method = rd16(data_entry + 12);

    ARCHIVE_PROGRESS->total = (uint64_t)uncompressed_size;
    ARCHIVE_PROGRESS->done = 0;
    if (!core_lseek)
        core_lseek = (fn_lseek_18)sym_libc_lseek;
    core_lseek(descriptor, (int64_t)(uint64_t)position, 0);

    if (method == 0) {
        if (stream == NULL) {
            if (!core_malloc)
                core_malloc = (fn_malloc_18)sym_libc_malloc;
            result = core_malloc((uint64_t)uncompressed_size);
            if (result == NULL)
                goto error_no_buffer;

            if (!core_read_chk)
                core_read_chk = (fn_read_chk_18)fortify_read;
            if (core_read_chk(descriptor, result, uncompressed_size,
                              UINT64_MAX) < 1)
                goto error_result;

            wr32(size_output, uncompressed_size);
            if (!core_close)
                core_close = (fn_close_18)sym_libc_close;
            core_close(descriptor);
            return result;
        }

        if (!core_malloc)
            core_malloc = (fn_malloc_18)sym_libc_malloc;
        buffer_work = core_malloc(0x20000);
        if (buffer_work == NULL)
            goto cleanup;

        if (uncompressed_size != 0) {
            uint32_t left = uncompressed_size;

            do {
                uint32_t block = left < 0x20000 ? left : 0x20000;

                if (!core_read_chk)
                    core_read_chk = (fn_read_chk_18)fortify_read;
                if (core_read_chk(descriptor, buffer_work, block, 0x20000) < 1) {
                    result = NULL;
                    goto cleanup;
                }
                if (!core_fwrite)
                    core_fwrite = (fn_fwrite)sym_libc_fwrite;
                core_fwrite(buffer_work, block, 1, stream);
                ARCHIVE_PROGRESS->done = ARCHIVE_PROGRESS->done + (uint64_t)block;
                left -= block;
            } while (left != 0);
        }

        wr32(size_output, uncompressed_size);
        if (!core_close)
            core_close = (fn_close_18)sym_libc_close;
        core_close(descriptor);
        if (!core_fclose)
            core_fclose = (fn_fclose)sym_libc_fclose;
        core_fclose(stream);
        return (void *)(uintptr_t)1;
    }

    if (method != 8)
        goto cleanup;

    {
        void *inflated_dest;
        void *buffer_output = NULL;
        z_stream_core state;
        uint32_t left;
        uint32_t capacity_output = 0x20000;
        int32_t state_inflate;

        if (!core_malloc)
            core_malloc = (fn_malloc_18)sym_libc_malloc;
        if (stream != NULL) {
            buffer_output = core_malloc(0x20000);
            inflated_dest = buffer_output;
        } else {
            inflated_dest = core_malloc((uint64_t)uncompressed_size);
        }
        if (inflated_dest == NULL)
            goto cleanup;

        result = NULL;
        buffer_work = core_malloc(0x20000);
        if (buffer_work == NULL) {
            result = inflated_dest;
            goto cleanup;
        }

        state.next_in = buffer_work;
        state.next_out = inflated_dest;
        state.avail_out = stream != NULL ? capacity_output : uncompressed_size;
        state.zalloc = NULL;
        state.zfree = NULL;
        if (!core_inflate_init2)
            core_inflate_init2 = (fn_inflate_init2_18)((void *)inflateInit2_);
        state_inflate = core_inflate_init2(&state, -15,
                                             "1.2.7", 112);

        left = compressed_size < 0x20000 ? compressed_size : 0x20000;
        state.avail_in = left;
        if (!core_read_chk)
            core_read_chk = (fn_read_chk_18)fortify_read;
        if (core_read_chk(descriptor, buffer_work, left, 0x20000) < 1) {
            result = inflated_dest;
            goto cleanup;
        }
        compressed_size -= left;

        if (state_inflate == 0) {
            for (;;) {
                uint32_t written;

                if (!core_inflate)
                    core_inflate = (fn_inflate_18)((void *)inflate);
                state_inflate = core_inflate(&state, 2);

                if (stream != NULL) {
                    written = capacity_output - state.avail_out;
                    if (written != 0) {
                        if (!core_fwrite)
                            core_fwrite = (fn_fwrite)sym_libc_fwrite;
                        ARCHIVE_PROGRESS->done = ARCHIVE_PROGRESS->done + written;
                        if (core_fwrite(buffer_output, written, 1, stream) == 0) {
                            if (!core_free)
                                core_free = (fn_free_18)sym_libc_free;
                            core_free(buffer_output);
                            result = inflated_dest;
                            goto cleanup;
                        }
                    }
                    state.next_out = buffer_output;
                    state.avail_out = capacity_output;
                }

                if (state_inflate == -5) {
                    state.next_in = buffer_work;
                    if (compressed_size != 0) {
                        uint32_t block = compressed_size < 0x20000
                                              ? compressed_size : 0x20000;

                        if (core_read_chk(descriptor, buffer_work, block,
                                          0x20000) < 1) {
                            result = inflated_dest;
                            goto cleanup;
                        }
                        state.avail_in = block;
                        compressed_size -= block;
                    }
                }

                if (state_inflate == 1)
                    break;
            }
        }

        if (buffer_output != NULL) {
            if (!core_free)
                core_free = (fn_free_18)sym_libc_free;
            core_free(buffer_output);
        }
        if (!core_inflate_end)
            core_inflate_end = (fn_inflate_end_18)((void *)inflateEnd);
        core_inflate_end(&state);
        if (!core_free)
            core_free = (fn_free_18)sym_libc_free;
        core_free(buffer_work);
        buffer_work = NULL;

        wr32(size_output, uncompressed_size);
        if (!core_close)
            core_close = (fn_close_18)sym_libc_close;
        core_close(descriptor);
        if (stream == NULL)
            return inflated_dest;
        if (!core_fclose)
            core_fclose = (fn_fclose)sym_libc_fclose;
        core_fclose(stream);
        return (void *)(uintptr_t)1;
    }

error_result:
    if (!core_close)
        core_close = (fn_close_18)sym_libc_close;
    core_close(descriptor);
    if (!core_free)
        core_free = (fn_free_18)sym_libc_free;
    core_free(result);
    return NULL;

error_no_buffer:
    if (!core_close)
        core_close = (fn_close_18)sym_libc_close;
    core_close(descriptor);
    return NULL;

cleanup:
    if (!core_close)
        core_close = (fn_close_18)sym_libc_close;
    core_close(descriptor);
    if (stream != NULL) {
        if (!core_fclose)
            core_fclose = (fn_fclose)sym_libc_fclose;
        core_fclose(stream);
    }
    if (buffer_work != NULL) {
        if (!core_free)
            core_free = (fn_free_18)sym_libc_free;
        core_free(buffer_work);
    }
    if (result != NULL) {
        if (!core_free)
            core_free = (fn_free_18)sym_libc_free;
        core_free(result);
    }
    return NULL;
}




int32_t archive_rar_memory_sink_write(int32_t mode, uint64_t *cursor, const void *src, uint64_t n)
{

    if (mode != 1)
        return 0;

    static void *(*s_memcpy)(void *, const void *, size_t);
    if (!s_memcpy)
        s_memcpy = (void *(*)(void *, const void *, size_t))sym_libc_memcpy;

    ARCHIVE_PROGRESS->done = ARCHIVE_PROGRESS->done + n;

    uint64_t dest = rd64(cursor);
    s_memcpy((void *)dest, src, (size_t)n);

    uint64_t dest2 = rd64(cursor);
    wr64(cursor, dest2 + n);

    return 1;
}

typedef unsigned long (*fn_fwrite_20)(const void *, unsigned long,
                                   unsigned long, void *);

int archive_rar_file_sink_write(int32_t param_1, void *param_2, const void *param_3,
                       uint64_t param_4)
{

    if (param_1 != 1)
        return 0;

    ARCHIVE_PROGRESS->done = ARCHIVE_PROGRESS->done + param_4;

    static fn_fwrite_20 core_fwrite;
    if (!core_fwrite) core_fwrite = (fn_fwrite_20)sym_libc_fwrite;

    core_fwrite(param_3, 1, param_4, param_2);

    return 1;
}

#define _UNIX
#include "dll.hpp"
typedef void *(*fn_create_reader)(void *);
typedef int32_t (*fn_read_entry)(void *, void *);
typedef void (*fn_close_reader)(void *);
typedef int32_t (*fn_process_reader)(void *, uint32_t, void *, void *);
typedef void (*fn_install_output)(void *, void *, void *);
typedef char *(*fn_strrchr_chk)(const char *, int32_t, uint64_t);
typedef int32_t (*fn_strcasecmp)(const char *, const char *);
typedef void *(*fn_malloc_21)(uint64_t);
typedef void (*fn_free_21)(void *);
typedef int32_t (*fn_fclose_21)(void *);


void *archive_rar_extract_by_extension(const char *param_1, const char *extension,
                         uint32_t *output_size, const char *param_4)
{

    struct {
        void *output;
        struct RAROpenArchiveDataEx open_data;
        struct RARHeaderDataEx header;
    } frame;
    struct RAROpenArchiveDataEx *ctx = &frame.open_data;
    struct RARHeaderDataEx *buffer = &frame.header;
    void *reader;
    void *result = NULL;
    int32_t state;

    _Static_assert(sizeof frame == 14584, "rar extract frame");
    wr32(output_size, 0);
    memset(&ctx->ArcNameW, 0, (size_t)((unsigned char *)ctx + sizeof *ctx - (unsigned char *)&ctx->ArcNameW));
    ctx->ArcName = (char *)param_1;
    ctx->OpenMode = 1;

    reader = ((fn_create_reader)((void *)RAROpenArchiveEx))(ctx);
    if (ctx->OpenResult != 0)
        goto cleanup;
    if (((uint16_t)ctx->Flags & UINT16_C(0x185)) != 0)
        goto cleanup;

    buffer->CmtBuf = NULL;
    state = ((fn_read_entry)((void *)RARReadHeaderEx))(reader, buffer);
    if (state != 0)
        goto cleanup;

    for (;;) {
        char *point = ((fn_strrchr_chk)fortify_strrchr)(
            buffer->FileName, 0x2e, sizeof buffer->FileName);

        if (point != NULL &&
            ((fn_strcasecmp)sym_libc_strcasecmp)(point + 1, extension) == 0) {
            uint32_t size = buffer->UnpSize;

            wr32(output_size, size);
            ARCHIVE_PROGRESS->total = size;
            ARCHIVE_PROGRESS->done = 0;

            if (param_4 != NULL) {
                void *stream = nds_platform_default()->files.open(nds_platform_default()->user, 
                    param_4, "wb");

                ((fn_install_output)((void *)RARSetCallback))(
                    reader, (void *)archive_rar_file_sink_write, stream);
                result = ((fn_process_reader)((void *)RARProcessFile))(
                    reader, 1, NULL, NULL) == 0 ? (void *)(uintptr_t)1 : NULL;
                ((fn_fclose_21)sym_libc_fclose)(stream);
                goto cleanup;
            }

            result = ((fn_malloc_21)sym_libc_malloc)(size);
            frame.output = result;
            if (result == NULL)
                goto cleanup;

            ((fn_install_output)((void *)RARSetCallback))(
                reader, (void *)archive_rar_memory_sink_write, &frame.output);
            if (((fn_process_reader)((void *)RARProcessFile))(reader, 1, NULL, NULL) == 0)
                goto cleanup;

            ((fn_free_21)sym_libc_free)(result);
            result = NULL;
            goto cleanup;
        }

        ((fn_process_reader)((void *)RARProcessFile))(reader, 0, NULL, NULL);
        state = ((fn_read_entry)((void *)RARReadHeaderEx))(reader, buffer);
        if (state != 0)
            goto cleanup;
    }

cleanup:
    ((fn_close_reader)((void *)RARCloseArchive))(reader);
    return result;
}
#undef _UNIX

typedef void *(*fn_memcpy_22)(void *, const void *, uint64_t);

int archive_rar_window_sink_write(const unsigned char *param_1, int param_2)
{

    archive_rar_window_sink_t *g = &ARCHIVE_PROGRESS->rar_sink;

    uint32_t pos_orig = g->position;
    uint32_t limit    = g->threshold;

    uint32_t pos_new = pos_orig + (uint32_t)param_2;
    g->position = pos_new;

    uint32_t excess = pos_new - limit;
    uint32_t free_final;

    if (pos_new >= limit) {

        uint32_t avail = g->room;

        uint8_t *ptr = g->destination;

        uint32_t before_threshold = limit - pos_orig;
        uint32_t copy = (excess > avail) ? avail : excess;

        const void *src = (const void *)(param_1 + before_threshold);
        void *dst = ptr;

        ((fn_memcpy_22)sym_libc_memcpy)(dst, src, (uint64_t)copy);

        free_final = avail - copy;
        uint8_t *ptr_updated = ptr + copy;
        uint32_t limit_updated = limit + copy;

        g->room = free_final;
        g->destination = ptr_updated;
        g->threshold = limit_updated;
    } else {

        free_final = g->room;
    }

    return (free_final != 0) ? 1 : 0;
}

#define _UNIX
#include "dll.hpp"
typedef void (*fnp_rar_set_process_data_proc)(unsigned char *obj, int (*v)(const unsigned char *, int));
typedef void *(*fn_rar_open)(void *desc);
typedef int   (*fn_header)(void *arch, void *info);
typedef int   (*fn_process)(void *arch, int op,
                             void *path, void *name);
typedef int   (*fn_rar_close)(void *arch);
extern void rar_set_process_data_proc(unsigned char *obj, uint64_t v);
#define OFF_CALLBACK 0x138ea8


int archive_rar_read_range(const char *name_file, const char *extension,
                       uint32_t *output_size, void *dest,
                       int len, int offset)
{

    static char *(*p_strrchr_chk)(const char *, int, size_t);
    static int   (*p_strcasecmp)(const char *, const char *);

    struct RAROpenArchiveDataEx desc;
    struct RARHeaderDataEx info;
    archive_rar_window_sink_t *g = &ARCHIVE_PROGRESS->rar_sink;
    const char    *name;
    void          *arch;
    int            ret;
    uint32_t       size;

    if (!p_strrchr_chk) {
        p_strrchr_chk = (char *(*)(const char *, int, size_t))
                        fortify_strrchr;
        p_strcasecmp  = (int (*)(const char *, const char *))
                        sym_libc_strcasecmp;
    }

    wr32(output_size, 0);

    memset(&desc, 0, sizeof desc);
    desc.ArcName = (char *)name_file;
    desc.OpenMode = 1;

    arch = ((fn_rar_open)((void *)RAROpenArchiveEx))(&desc);

    if (desc.OpenResult != 0) { ret = -1; goto close_fn; }

    if (((uint16_t)desc.Flags & 0x185u) != 0) { ret = -1; goto close_fn; }

    info.CmtBuf = NULL;

    if (((fn_header)((void *)RARReadHeaderEx))(arch, &info) != 0) { ret = -1; goto close_fn; }

    name = info.FileName;

    for (;;) {

        char *point = p_strrchr_chk(name, 0x2e, 0x400);

        if (point != NULL && p_strcasecmp(point + 1, extension) == 0)
            break;

        ((fn_process)((void *)RARProcessFile))(arch, 0, NULL, NULL);

        if (((fn_header)((void *)RARReadHeaderEx))(arch, &info) != 0) { ret = -1; goto close_fn; }
    }

    size = info.UnpSize;
    ret = 0;
    wr32(output_size, size);

    if (dest == NULL) goto close_fn;
    if (len == 0)   goto close_fn;

    if ((uint32_t)((uint32_t)offset + (uint32_t)len) > size) {
        ret = -1;
        goto close_fn;
    }

    g->destination = (uint8_t *)dest;
    g->room = (uint32_t)len;
    g->threshold = (uint32_t)offset;
    g->position = 0;

    ((fnp_rar_set_process_data_proc)((void *)RARSetProcessDataProc))((unsigned char *)arch,
                       archive_rar_window_sink_write);

    ((fn_process)((void *)RARProcessFile))(arch, 1, NULL, NULL);

    ret = (g->room != 0) ? -1 : 0;

close_fn:

    ((fn_rar_close)((void *)RARCloseArchive))(arch);

    return ret;
}
#undef _UNIX
#undef OFF_CALLBACK

archive_progress_t archive_progress_state;

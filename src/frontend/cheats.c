#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include "mem_access.h"


static void (*core_free)(void *);

static void release(void *p)
{
    if (!core_free) core_free = (void (*)(void *))sym_libc_free;
    core_free(p);
}





void cheats_list_free_all(void *param_1)
{

    unsigned char *obj = (unsigned char *)param_1;

    unsigned char *descriptor = obj + 1040;

    uint32_t count = rd32(obj + 1056);

    if (count != 0) {
        uint64_t shift = 0;
        uint64_t i = 0;
        do {

            unsigned char *base = (unsigned char *)rd_ptr(obj + 1040);

            unsigned char *elem = base + shift;

            release(rd_ptr(elem + 8));
            release(rd_ptr(elem));

            count = rd32(obj + 1056);
            i += 1;
            shift += 0x28;

        } while (i < (uint64_t)count);
    }

    release(rd_ptr(obj + 1040));
    release(rd_ptr(obj + 1048));

    wr64(descriptor, 0);
    wr64(descriptor + 8, 0);

    wr32(descriptor + 16, 0);
}

typedef size_t (*fn_strlen)(const char *);
typedef void  *(*fn_realloc)(void *, size_t);
typedef void  *(*fn_malloc)(size_t);
typedef void  *(*fn_memcpy)(void *, const void *, size_t);
static fn_strlen  p_strlen;
static fn_realloc p_realloc;
static fn_malloc  p_malloc;
static fn_memcpy  p_memcpy;






uint32_t cheats_list_add_entry(unsigned char *context, const char *name,
                            const void *codes, uint32_t count)
{

    unsigned char *ctx = context;
    size_t         len;
    uint32_t       how_many;
    uint32_t       new;
    unsigned char *list;
    unsigned char *flags_prev;
    unsigned char *flags;
    unsigned char *entry;
    unsigned char *copy_name;
    void          *copy_codes;
    uint64_t       len32;
    uint64_t       bytes_codes;

    if (!p_strlen) {
        p_strlen  = (fn_strlen)sym_libc_strlen;
        p_realloc = (fn_realloc)sym_libc_realloc;
        p_malloc  = (fn_malloc)sym_libc_malloc;
        p_memcpy  = (fn_memcpy)sym_libc_memcpy;
    }

    len   = p_strlen(name);
    how_many = rd32(ctx + 1056);
    new  = how_many + 1u;

    {
        uint64_t n = (uint64_t)new;
        void *new_list = p_realloc(rd_ptr_u8(ctx + 1040), (size_t)((n + (n << 2)) << 3));

        flags_prev = rd_ptr_u8(ctx + 1048);
        wr_ptr(ctx + 1040, new_list);
    }

    {
        void *new_flags = p_realloc(flags_prev, (size_t)new);

        list = rd_ptr_u8(ctx + 1040);
        wr_ptr(ctx + 1048, new_flags);
    }

    entry = list + (uint64_t)how_many * 40u;

    copy_name = (unsigned char *)p_malloc((size_t)(uint32_t)((uint32_t)len + 1u));

    len32 = (uint64_t)(uint32_t)len;
    wr_ptr(entry + 8, copy_name);
    p_memcpy(copy_name, name, (size_t)len32);

    bytes_codes = (uint64_t)count << 2;

    {
        unsigned char *saved = rd_ptr_u8(entry + 8);

        saved[len32] = 0;
    }

    copy_codes = p_malloc((size_t)bytes_codes);
    wr_ptr(entry + 0, copy_codes);
    p_memcpy(copy_codes, codes, (size_t)bytes_codes);

    wr32(entry + 24, count);
    flags = rd_ptr_u8(ctx + 1048);
    wr32(entry + 28, 0xffffffffu);
    wr64(entry + 16, 0);
    wr_ptr(entry + 32, flags + how_many);
    flags[how_many] = 0;
    wr32(ctx + 1056, new);

    return 0;
}

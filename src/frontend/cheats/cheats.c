#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "frontend/cheats/cheats.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"

typedef int (*fn_compare_t)(const void *a, const void *b);
typedef void *(*fn_bsearch_t)(const void *key, const void *base, size_t n,
                              size_t size, fn_compare_t compare);

void *cheats_find_entry_by_keys(cheats_t *state, uint32_t key, uint32_t subkey)
{

    fn_bsearch_t bsearch_core = (fn_bsearch_t)sym_libc_bsearch;
    uint32_t key_local = key;
    cheat_index_entry_t *table = state->index;
    uint32_t n_table = state->index_count;
    cheat_index_entry_t *entry = bsearch_core(&key_local, table, n_table, sizeof(cheat_index_entry_t),
                                 str_compare_u32_unsigned);

    if (entry == 0)
        return 0;

    table = state->index;
    ptrdiff_t position = entry - table;
    uint32_t idx = (uint32_t)position;
    int64_t back = (int64_t)(int32_t)position + 1;
    cheat_index_entry_t *cursor = table + back;

    for (;;) {
        back--;
        if (back < 0)
            break;
        if (cursor[-1].game_code != key)
            break;
        entry = cursor - 1;
        cursor = entry;
        if (cursor->crc == subkey)
            return entry;
    }

    uint32_t limit = state->index_count;
    idx++;
    for (;;) {
        if (idx >= limit)
            return 0;
        entry = table + (int32_t)idx;
        if (entry->game_code != key)
            return 0;
        uint32_t secondary = table[(int32_t)idx].crc;
        idx++;
        if (secondary == subkey)
            return entry;
    }
}

void cheats_manager_free_buffers(unsigned char *obj) {
    void (*release)(void *) = (void (*)(void *))sym_libc_free;
    void *a, *b, *c;
    memcpy(&a, obj + 8,  sizeof(a));
    memcpy(&b, obj + 24, sizeof(b));
    memcpy(&c, obj + 32, sizeof(c));
    release(a);
    release(b);
    release(c);
}

#define INITIAL_CAP   32u
#define INITIAL_MALLOC 0x100u
typedef void *(*fn_malloc)(unsigned long);
typedef void  (*fn_free)(void *);
typedef void *(*fn_realloc)(void *, unsigned long);

void *cheats_rebuild_active_list(cheats_t *g) {

    static fn_malloc  core_malloc;
    static fn_free    core_free;
    static fn_realloc core_realloc;
    if (!core_malloc) {
        core_malloc  = (fn_malloc)sym_libc_malloc;
        core_free    = (fn_free)sym_libc_free;
        core_realloc = (fn_realloc)sym_libc_realloc;
    }

    cheat_entry_t **list = (cheat_entry_t **)core_malloc(INITIAL_MALLOC);
    cheat_entry_t **old = g->active;
    if (old != 0)
        core_free(old);

    uint32_t count = 0;
    uint32_t cap    = INITIAL_CAP;

    if (g->database_loaded != 0) {
        uint32_t n = g->entry_count;
        if (n != 0) {
            uint64_t i    = 0;
            for (;;) {

                cheat_entry_t *reg  = &g->entries[i];
                const unsigned char *mark = reg->enabled;
                if (*mark != 0) {
                    if (count >= cap) {
                        cap <<= 1;
                        list = (cheat_entry_t **)core_realloc(list,
                                    (unsigned long)cap * sizeof *list);
                        n = g->entry_count;
                    }

                    list[count] = reg;
                    count++;
                }
                i++;
                if (i >= (uint64_t)n)
                    break;
            }
        }
    }

    {
        uint32_t n = g->user_count;
        if (n != 0) {
            uint64_t i    = 0;
            for (;;) {
                cheat_entry_t *reg  = &g->user_entries[i];
                const unsigned char *mark = reg->enabled;
                if (*mark != 0) {
                    if (count >= cap) {
                        cap <<= 1;
                        list = (cheat_entry_t **)core_realloc(list,
                                    (unsigned long)cap * sizeof *list);
                        n = g->user_count;
                    }
                    list[count] = reg;
                    count++;
                }
                i++;
                if (i >= (uint64_t)n)
                    break;
            }
        }
    }

    list = (cheat_entry_t **)core_realloc(list, (unsigned long)count * sizeof *list);
    g->active = list;
    g->active_count = count;
    return (void *)list;
}
#undef INITIAL_CAP
#undef INITIAL_MALLOC

typedef int (*fn_compare_t_3)(const void *a, const void *b);
typedef void *(*fn_bsearch_t_3)(const void *key, const void *base, size_t n,
                              size_t size, fn_compare_t_3 compare);
typedef void *(*fn_malloc_t)(size_t size);
typedef void (*fn_free_t)(void *p);
typedef int (*fn_fseek_t)(void *stream, long shift, int origin);
typedef size_t (*fn_fread_t)(void *dst, size_t size, size_t n, void *stream);
typedef int (*fn_fclose_t)(void *stream);
typedef size_t (*fn_strlen_t)(const char *s);
typedef void *(*fn_realloc_t)(void *p, size_t size);


int32_t cheats_load_entry_by_key(cheats_t *state, uint32_t key, uint32_t subkey)
{

    fn_bsearch_t_3 bsearch_core = (fn_bsearch_t_3)sym_libc_bsearch;
    fn_free_t free_core = (fn_free_t)sym_libc_free;
    fn_malloc_t malloc_core = (fn_malloc_t)sym_libc_malloc;
    fn_fseek_t fseek_core = (fn_fseek_t)sym_libc_fseek;
    fn_fread_t fread_core = (fn_fread_t)sym_libc_fread;
    fn_fclose_t fclose_core = (fn_fclose_t)sym_libc_fclose;
    fn_strlen_t strlen_core = (fn_strlen_t)sym_libc_strlen;
    fn_realloc_t realloc_core = (fn_realloc_t)sym_libc_realloc;
    cheat_index_entry_t *entry = 0;
    uint32_t key_local = key;
    cheat_index_entry_t *table = state->index;
    uint32_t n_table = state->index_count;
    cheat_index_entry_t *found = bsearch_core(&key_local, table, n_table, sizeof(cheat_index_entry_t),
                                 str_compare_u32_unsigned);

    if (found != 0) {
        table = state->index;
        ptrdiff_t position = found - table;
        uint32_t idx = (uint32_t)position;
        int64_t back = (int64_t)(int32_t)position + 1;
        cheat_index_entry_t *cursor = table + back;
        int was_found = 0;

        for (;;) {
            back--;
            if (back < 0)
                break;
            if (cursor[-1].game_code != key)
                break;
            entry = cursor - 1;
            cursor = entry;
            if (cursor->crc == subkey) {
                was_found = 1;
                break;
            }
        }

        if (!was_found) {
            uint32_t limit = state->index_count;
            idx++;
            for (;;) {
                if (idx >= limit) {
                    entry = 0;
                    break;
                }
                entry = table + (int32_t)idx;
                if (entry->game_code != key) {
                    entry = 0;
                    break;
                }
                uint32_t secondary = table[(int32_t)idx].crc;
                idx++;
                if (secondary == subkey)
                    break;
            }
        }
    }

    if (state->database_loaded != 0) {
        free_core(state->data);
        free_core(state->entries);
        free_core(state->folders);
        state->database_loaded = 0;
    }

    if (entry == 0) {
        cheats_rebuild_active_list(state);
        return -1;
    }

    void *stream = nds_platform_default()->files.open(nds_platform_default()->user, state->database_path, "rb");
    if (stream == 0)
        return -1;

    uint32_t resource_size = entry->size;
    uint8_t *data = (uint8_t *)malloc_core(resource_size);
    if (data == 0) {
        fclose_core(stream);
        return -1;
    }

    fseek_core(stream, (long)entry->offset, 0);
    if (fread_core(data, entry->size, 1, stream) != 1) {
        free_core(data);
        fclose_core(stream);
        return -1;
    }
    fclose_core(stream);

    state->entry = entry;
    state->loaded_data = data;

    size_t long_header = strlen_core((const char *)data);
    uint8_t *header = data + ((long_header + 4) & ~(size_t)3);
    uint32_t number = rd32(header) & CHEAT_DATABASE_COUNT_MASK;
    size_t lists_size = (size_t)number * sizeof(cheat_entry_t);
    cheat_entry_t *list_a = (cheat_entry_t *)malloc_core(lists_size);
    state->entries = list_a;
    cheat_folder_t *list_b = (cheat_folder_t *)malloc_core(lists_size);
    state->folders = list_b;

    uint32_t used_a = 0;
    uint32_t used_b = 0;
    uint32_t str = 0;
    if (number != 0) {
        uint8_t *read_ptr = header + 0x24;
        cheat_entry_t *output_a = list_a;
        cheat_folder_t *output_b = list_b;

        do {
            uint32_t descriptor = rd32(read_ptr);
            uint8_t *text1 = read_ptr + 4;

            if ((descriptor & CHEAT_DATABASE_FOLDER_FLAG) == 0) {
                output_a->enabled = read_ptr + 3;
                output_a->name = (char *)text1;
                uint32_t len1 = (uint32_t)strlen_core((const char *)text1);
                uint32_t prev_b = used_b - 1;
                if (str == 0)
                    prev_b = CHEAT_FOLDER_NONE;
                else
                    str--;
                uint8_t *text2 = text1 + len1 + 1;
                output_a->description = (char *)text2;
                output_a->folder = prev_b;
                size_t len2 = strlen_core((const char *)text2);
                uint8_t *block = text1 + (((size_t)len1 + len2 + 5) & ~(size_t)3);
                uint32_t value = rd32(block);

                read_ptr += ((descriptor & CHEAT_DATABASE_WORDS_MASK) * 4) + 4;
                output_a->codes = (int32_t *)(block + 4);
                output_a->code_words = value;
                output_a += 1;
                used_a++;
            } else {
                output_b->enabled = read_ptr + 2;
                output_b->type = (uint8_t)(descriptor >> 24);
                output_b->cheat_count = descriptor & CHEAT_DATABASE_WORDS_MASK;
                output_b->name = (char *)text1;
                uint32_t len1 = (uint32_t)strlen_core((const char *)text1);
                uint8_t *text2 = text1 + len1 + 1;
                output_b->description = (char *)text2;
                size_t len2 = strlen_core((const char *)text2);
                str = output_b->cheat_count;

                output_b += 1;
                read_ptr = text1 + (((size_t)len1 + len2 + 5) & ~(size_t)3);
                used_b++;
            }
            number--;
        } while (number != 0);
    }

    list_a = state->entries;
    state->entry_count = used_a;
    state->folder_count = used_b;
    list_a = (cheat_entry_t *)realloc_core(list_a, (size_t)used_a * sizeof(cheat_entry_t));
    list_b = state->folders;
    state->entries = list_a;
    list_b = (cheat_folder_t *)realloc_core(list_b, (size_t)used_b * sizeof(cheat_folder_t));
    state->folders = list_b;
    state->data = data;
    state->database_loaded = 1;
    cheats_rebuild_active_list(state);
    return 0;
}

typedef void          *(*fn_malloc_4)(unsigned long);
typedef void            (*fn_free_4)(void *);
typedef void          *(*fn_realloc_4)(void *, unsigned long);
typedef char          *(*fn_fgets)(char *, int, void *);
typedef char          *(*fn_strrchr)(const char *, int);
typedef unsigned long   (*fn_strtoul)(const char *, char **, int);
typedef int             (*fn_isalnum)(int);
typedef int             (*fn_fclose)(void *);
extern void *files_fopen_resolved(const char *path, const char *mode);
extern char *str_skip_whitespace(char *s);
extern int   str_vsprintf_fixed_format_2080(char *dest, long a, long b, ...);

int cheats_load_user_file(cheats_t *p, unsigned char *obj) {

    static fn_malloc_4  core_malloc;
    static fn_free_4    core_free;
    static fn_realloc_4 core_realloc;
    static fn_fgets   core_fgets;
    static fn_strrchr core_strrchr;
    static fn_strtoul core_strtoul;
    static fn_isalnum core_isalnum;
    static fn_fclose  core_fclose;
    if (!core_malloc) {
        core_malloc  = (fn_malloc_4)sym_libc_malloc;
        core_free    = (fn_free_4)sym_libc_free;
        core_realloc = (fn_realloc_4)sym_libc_realloc;
        core_fgets   = (fn_fgets)sym_libc_fgets;
        core_strrchr = (fn_strrchr)sym_libc_strrchr;
        core_strtoul = (fn_strtoul)sym_libc_strtoul;
        core_isalnum = (fn_isalnum)sym_libc_isalnum;
        core_fclose  = (fn_fclose)sym_libc_fclose;
    }

    void *(*after_load)(cheats_t *) = cheats_rebuild_active_list;

    cheat_entry_t *codes0 = p->user_entries;
    if (codes0 != 0) {
        uint32_t count = p->user_count;
        if (count != 0) {
            cheat_entry_t *base = codes0;
            core_free(base[0].name);
            core_free(base[0].codes);
            if (count > 1) {
                uint32_t i;
                for (i = 1; i < count; i++) {
                    core_free(base[i].name);
                    core_free(base[i].codes);
                }
            }
            codes0 = p->user_entries;
        }
        core_free(codes0);
        core_free(p->user_flags);
        p->user_entries = 0;
        p->user_flags = 0;
        p->user_count = 0;
    }

    char path[2080];

    str_vsprintf_fixed_format_2080(path, 0, 0, ((nds_t *)obj)->save_dir, 0x2f, 0x2f,
                       ((nds_t *)obj)->rom_name);

    void *f = nds_platform_default()->files.open(nds_platform_default()->user, path, "rb");
    if (!f)
        return -1;

    cheat_entry_t *regs  = (cheat_entry_t *)core_malloc(32 * sizeof(cheat_entry_t));
    unsigned char *flags = (unsigned char *)core_malloc(0x20);
    char           line[256];

    uint32_t  regcap = 32;
    uint32_t  cnt    = 0;
    uint32_t  codcap = 0;
    cheat_entry_t *current = NULL;

    char *l = core_fgets(line, 0x100, f);
    if (l) {
        for (;;) {
            char *pl = str_skip_whitespace(line);
            if (*pl == '[') {
                char *start = pl + 1;
                char *end = core_strrchr(start, ']');
                if (cnt != 0) {

                    current->codes = (int32_t *)core_realloc(
                        current->codes, (unsigned long)current->code_words * 4);
                }
                if (!end)
                    break;

                unsigned long len    = (unsigned long)(end - start);
                unsigned char active = (end[1] == '+') ? 1 : 0;
                uint32_t      idx    = cnt;
                cnt++;
                if (cnt > regcap) {
                    regcap <<= 1;
                    regs  = (cheat_entry_t *)core_realloc(regs, (unsigned long)regcap * sizeof(cheat_entry_t));
                    flags = (unsigned char *)core_realloc(flags, regcap);
                }
                flags[idx] = active;
                current     = &regs[idx];
                current->name = (char *)core_malloc(len + 1);
                memcpy(current->name, start, len);
                current->name[len] = 0;
                current->description = 0;
                current->code_words = 0;
                current->folder = CHEAT_FOLDER_NONE;
                current->codes = (int32_t *)core_malloc(0x80);
                codcap = 32;
            } else if (core_isalnum((unsigned char)*pl)) {
                if (cnt == 0)
                    break;

                char *end1;
                unsigned long v1 = core_strtoul(pl, &end1, 16);
                char *p2 = str_skip_whitespace(end1);
                char *end2;
                unsigned long v2 = core_strtoul(p2, &end2, 16);
                (void)end2;

                uint32_t c = current->code_words;
                current->codes[c]     = (int32_t)v1;
                current->codes[c + 1] = (int32_t)v2;
                c += 2;
                current->code_words = c;
                if (c >= codcap) {
                    codcap <<= 1;
                    current->codes = (int32_t *)core_realloc(
                        current->codes, (unsigned long)codcap * 4);
                }
            }

            char *l2 = core_fgets(line, 0x100, f);
            if (!l2)
                break;
        }
    }

    if (cnt != 0)
        current->codes = (int32_t *)core_realloc(current->codes,
                                                    (unsigned long)current->code_words * 4);

    regs  = (cheat_entry_t *)core_realloc(regs, (unsigned long)cnt * sizeof(cheat_entry_t));
    flags = (unsigned char *)core_realloc(flags, cnt);

    p->user_entries = regs;
    p->user_count = cnt;
    p->user_flags = flags;

    {
        uint32_t i;
        for (i = 0; i < cnt; i++)
            regs[i].enabled = &flags[i];
    }

    core_fclose(f);
    after_load(p);
    return 0;
}
uint32_t cheats_remove_entry(void *obj, uint32_t idx)
{
    cheats_t *base = (cheats_t *)obj;
    uint32_t count = base->user_count;
    uint32_t new_count;
    uint32_t remaining;
    void *updated;

    if (count <= idx)
        return 0xffffffffu;

    new_count = count - 1u;
    remaining = new_count - idx;
    if (remaining != 0) {
        cheat_entry_t *regs = base->user_entries;

        ((void *(*)(void *, const void *, uint64_t))sym_libc_memmove)(
            regs + idx,
            regs + (idx + 1u),
            (uint64_t)remaining * sizeof(cheat_entry_t));
        {
            unsigned char *bytes = base->user_flags;

            ((void *(*)(void *, const void *, uint64_t))sym_libc_memmove)(
                bytes + idx, bytes + idx + 1u, (uint64_t)remaining);
        }
    }

    updated = ((void *(*)(void *, uint64_t))sym_libc_realloc)(
        base->user_entries, (uint64_t)new_count * sizeof(cheat_entry_t));
    base->user_entries = updated;
    updated = ((void *(*)(void *, uint64_t))sym_libc_realloc)(
        base->user_flags, (uint64_t)new_count);
    base->user_flags = updated;
    base->user_count = new_count;
    return 0;
}

typedef int (*fn_fprintf)(void *, const char *, ...);
typedef int (*fn_fputc)(int, void *);
typedef int (*fn_fclose_6)(void *);

uint32_t cheats_write_user_file(cheats_t *state, nds_t *nds)
{

    char path[2080];
    str_vsprintf_fixed_format_2080(path, 0, 0, nds->save_dir, 0x2f, 0x2f,
                        nds->rom_name);

    void *file = nds_platform_default()->files.open(nds_platform_default()->user, path, "wb");
    if (file == 0)
        return 0xffffffffu;

    fn_fprintf print = (fn_fprintf)sym_libc_fprintf;
    fn_fputc putc = (fn_fputc)sym_libc_fputc;
    fn_fclose_6 close_fn = (fn_fclose_6)sym_libc_fclose;

    uint32_t count = state->user_count;
    if (count != 0) {
        uint64_t idx = 0;

        for (;;) {
            cheat_entry_t *entry = state->user_entries;
            entry += idx;

            print(file, "[%s]",
                    entry->name);
            if (*entry->enabled != 0)
                putc(0x2b, file);
            putc(0x0a, file);

            uint32_t words = entry->code_words;
            if (words != 0) {
                int32_t *codes = entry->codes;
                uint32_t position = 0;

                do {
                    uint32_t next = position + 1u;
                    print(file, "%08X %08X\n",
                            (uint32_t)codes[position],
                            (uint32_t)codes[next]);
                    words = entry->code_words;
                    position += 2u;
                } while (position < words);
            }

            putc(0x0a, file);
            count = state->user_count;
            idx += 1u;
            if (idx >= (uint64_t)count)
                break;
        }
    }

    close_fn(file);
    return 0;
}

unsigned int cheats_find_interned_entry(cheats_t *state, const void *data,
                                unsigned int n_words)
{

    unsigned int count;
    cheat_entry_t *entry;
    unsigned long idx;
    unsigned long width;
    unsigned int key;
    int cmp;

    int (*c_memcmp)(const void *, const void *, unsigned long) =
        (int (*)(const void *, const void *, unsigned long))sym_libc_memcmp;

    count = state->user_count;
    if (count == 0)
        return 0xffffffffu;

    entry = state->user_entries;
    idx = 0;
    width = (unsigned long)n_words << 2;

    key = entry->code_words;
    if (key == n_words)
        goto COMPARE;

SEARCH:
    idx = idx + 1;
    if (idx >= (unsigned long)count)
        return 0xffffffffu;
    entry = entry + 1;
    key = entry->code_words;
    if (key != n_words)
        goto SEARCH;

COMPARE:
    cmp = c_memcmp(entry->codes, data, width);
    if (cmp != 0)
        goto SEARCH;

    return (unsigned int)idx;
}

void cheats_lists_size_scan(cheats_t *param_1) {

    if (param_1->database_loaded != 0 &&
        param_1->entry_count != 0) {
        uint64_t count = param_1->entry_count;
        cheat_entry_t *arr = param_1->entries;

        for (uint64_t i = 0; i < count; i++) {
            uint32_t val = arr[i].code_words;
            if (val != 0) {
                uint32_t acc = 0;
                do { acc += 8; } while (acc < val);
            }
        }
    }

    if (param_1->user_count != 0) {
        uint64_t count = param_1->user_count;
        cheat_entry_t *arr = param_1->user_entries;

        for (uint64_t i = 0; i < count; i++) {
            uint32_t val = arr[i].code_words;
            if (val != 0) {
                uint32_t acc = 0;
                do { acc += 8; } while (acc < val);
            }
        }
    }
}

static int    (*core_fseek)(void *, long, int);
static long   (*core_ftell)(void *);
static unsigned long (*core_fwrite)(const void *, unsigned long, unsigned long, void *);
static int    (*core_fclose)(void *);

int cheats_save_record(cheats_t *param_1) {

    if (!core_fseek) {
        core_fseek  = (int    (*)(void *, long, int))sym_libc_fseek;
        core_ftell  = (long   (*)(void *))sym_libc_ftell;
        core_fwrite = (unsigned long (*)(const void *, unsigned long,
                                          unsigned long, void *))sym_libc_fwrite;
        core_fclose = (int    (*)(void *))sym_libc_fclose;
    }

    cheat_index_entry_t *lVar4 = param_1->entry;

    void *stream = nds_platform_default()->files.open(nds_platform_default()->user, param_1->database_path,
                                       "rb+");
    if (stream == 0) {
        return -1;
    }

    if (core_fseek(stream, (long)lVar4->offset, 0) != 0) {
        core_fclose(stream);
        return -1;
    }

    long pos = core_ftell(stream);
    if ((unsigned long)pos != (unsigned long)lVar4->offset) {

        core_fclose(stream);
        return -1;
    }

    void *buf = param_1->data;
    unsigned long written = core_fwrite(buf, (unsigned long)lVar4->size,
                                          1, stream);
    core_fclose(stream);

    if (written != 1) {
        return -1;
    }

    cheats_rebuild_active_list(param_1);

    return 0;
}

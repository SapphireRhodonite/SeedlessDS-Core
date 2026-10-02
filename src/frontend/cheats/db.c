#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stdarg.h>
#include <stddef.h>
#include "core/gamedb.h"
#include "frontend/cheats/cheats.h"
#include "seedlessds/platform.h"
#include "core_internals.h"
#include <stdio.h>
#include "mem_access.h"




void db_xml_escape_string(uint8_t *output, const uint8_t *entry)
{

    uint8_t c = rd8(entry);

    for (;;) {
        uint32_t idx = (uint32_t)c - 0x22u;
        unsigned size;

        if (idx <= 0x1cu) {
            switch (c) {
            case 0x22:
                wr32(output + 3, 0x003b746fu);
                wr32(output,     0x6f757126u);
                size = 5;
                break;

            case 0x26:
                wr16(output + 4, 0x003bu);
                wr32(output,     0x706d6126u);
                size = 5;
                break;

            case 0x27:
                wr32(output + 3, 0x003b736fu);
                wr32(output,     0x6f706126u);
                size = 6;
                break;

            case 0x3c:
                wr8(output + 4, 0);
                wr32(output,     0x3b746c26u);
                size = 4;
                break;

            case 0x3e:
                wr8(output + 4, 0);
                wr32(output,     0x3b746726u);
                size = 4;
                break;

            default:
                wr8(output, c);
                size = 1;
                break;
            }
        } else {
            if (c == 0) {
                wr8(output, 0);
                return;
            }
            wr8(output, c);
            size = 1;
        }

        output += size;
        entry += 1;
        c = rd8(entry);
    }
}

typedef int (*fn_strncmp)(const char *, const char *, unsigned long);

void db_xml_unescape_string(char *output, const char *entry, unsigned n,
                        unsigned size) {
    if (n == 0) return;

    fn_strncmp cmp = (fn_strncmp)sym_libc_strncmp;

    unsigned i = 0;
    unsigned left = size - 1;
    char *s = output;

    for (;;) {
        unsigned char c = (unsigned char)entry[i];

        if (c != '&') {
            *s = (char)c;
            if (left == 0) return;
        } else {
            const char *p = entry + i + 1;

            if      (cmp(p, "amp;",  4) == 0) { *s = '&';  i += 4; }
            else if (cmp(p, "apos;", 5) == 0) { *s = '\''; i += 5; }
            else if (cmp(p, "quot;", 5) == 0) { *s = '"';  i += 5; }
            else if (cmp(p, "lt;",   3) == 0) { *s = '<';  i += 3; }
            else if (cmp(p, "gt;",   3) == 0) { *s = '>';  i += 3; }
            else                              { *s = '_';           }

            if (i >= n) return;
            if (left == 0) return;
        }

        i += 1;
        s += 1;
        left -= 1;
        if (i >= n) return;
    }
}

#define OFF_SKIP_SPACES 0x7dc58
#define OFF_SKIP_TOKEN   0x7dc84
#define OFF_FORMAT      0x791b0
typedef char *(*fn_fgets)(char *, int, void *);
typedef char *(*fn_strchr)(const char *, int);
typedef int   (*fn_isspace)(int);
typedef void  (*fn_chkcpy)(char *, const char *, uint64_t);
extern char *str_skip_whitespace(char *s);
#define ST_NAME 0x100
#define ST_ATTR_NAME   0x108
#define ST_ATTR_VALUE   0x208
#define ST_ATTR_COUNT   0x308
#define ST_CLASS  0x30c
#define S_NO_LT      0x10efc0
#define S_NO_CLOSE  0x10efdb
#define S_NO_QUOTE 0x10f058



int32_t db_xml_read_tag(void *fp, uint8_t *state, char *err)
{

    char *line = (char *)state;
    char *pcVar3 = ((fn_fgets)sym_libc_fgets)(line, 0x100, fp);

    if (pcVar3 == 0) {

        memcpy(err, "Unexpected end of file.\n", 25);
        return -1;
    }

    char copy[1024];
    ((fn_chkcpy)fortify_strcpy)(copy, line, sizeof(copy));

    char *p = str_skip_whitespace(line);

    if (*p != '<') {
        memcpy(err, "Line doesn't start with <\n", 27);
        return -1;
    }

    unsigned char next = (unsigned char)p[1];

    if (next == '/') {

        char *name = p + 2;
        char *findFrom = p + 3;

        wr32(state + ST_ATTR_COUNT, 0);
        wr_ptr(state + ST_NAME, name);
        state[ST_CLASS] = 1;

        char *closing_bracket = ((fn_strchr)sym_libc_strchr)(findFrom, '>');
        if (closing_bracket == 0) {
            memcpy(err, "Couldn't find closing >", 24);
            return -1;
        }
        *closing_bracket = 0;
        return 0;
    }

    char *nameLabel = p + 1;
    wr_ptr(state + ST_NAME, nameLabel);

    char *s = str_scan_until_tag_separator(nameLabel);
    uint32_t count = 0;
    unsigned char b = (unsigned char)*s;

    if (((fn_isspace)sym_libc_isspace)(b)) {
        *s = 0; s++;
        s = str_skip_whitespace(s);
        b = (unsigned char)*s;
    }

    for (;;) {
        if (b == 0) {
            db_vsprintf_limited(err, -1L,
                "Unexpected end of string (%s).\n", copy, 0);
            return -1;
        }

        if (b == '>') {
            *s = 0;
            wr32(state + ST_ATTR_COUNT, count);
            state[ST_CLASS] = 0;
            return 0;
        }

        if (b == '/') {
            unsigned char after = (unsigned char)s[1];
            if (after != '>') {

                memcpy(err, "Closing / not followed by >.\n", 30);
                return -1;
            }
            *s = 0;
            wr32(state + ST_ATTR_COUNT, count);
            state[ST_CLASS] = 2;
            return 0;
        }

        char *key = s;
        wr_ptr(state + ST_ATTR_NAME + (size_t)count * 8, key);

        char *equal = ((fn_strchr)sym_libc_strchr)(s, '=');
        if (equal == 0) {
            db_vsprintf_limited(err, -1L,
                "Couldn't find = after parameter (%s).\n", key, 0);
            return -1;
        }
        *equal = 0;

        unsigned char afterEqual = (unsigned char)equal[1];
        if (afterEqual != '\'') {
            memcpy(err, "Parameter value doesn't start with opening quote.\n", 48);
            {
                static const unsigned char tail[3] = { 0x2e, 0x0a, 0x00 };
                memcpy(err + 48, tail, 3);
            }
            return -1;
        }

        equal[1] = 0;
        char *value = equal + 2;
        wr_ptr(state + ST_ATTR_VALUE + (size_t)count * 8, value);

        char *cur = value;
        unsigned char c = (unsigned char)*cur;
        for (;;) {
            while (c == '\\') {
                unsigned char pc = (unsigned char)cur[1];
                char *nxt = (pc == '\'') ? (cur + 1) : cur;
                cur = nxt + 1;
                c = (unsigned char)*cur;
            }
            if (c == 0 || c == '\'') break;
            cur++;
            c = (unsigned char)*cur;
        }

        s = cur + 1;
        *cur = 0;
        count++;
        b = (unsigned char)*s;

        if (((fn_isspace)sym_libc_isspace)(b)) {
            *s = 0; s++;
            s = str_skip_whitespace(s);
            b = (unsigned char)*s;
        }

    }
}
#undef OFF_SKIP_SPACES
#undef OFF_SKIP_TOKEN
#undef OFF_FORMAT
#undef ST_NAME
#undef ST_ATTR_NAME
#undef ST_ATTR_VALUE
#undef ST_ATTR_COUNT
#undef ST_CLASS
#undef S_NO_LT
#undef S_NO_CLOSE
#undef S_NO_QUOTE

int db_vsprintf_limited(char *dest, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, size, format, ap);
    va_end(ap);
    return r;
}

typedef int (*fn_strcmp)(const char *, const char *);

uint64_t db_xml_attribute_lookup(uint8_t *param_1, const char *param_2)
{

    static fn_strcmp core_strcmp;
    if (!core_strcmp) core_strcmp = (fn_strcmp)sym_libc_strcmp;

    uint32_t n;
    memcpy(&n, param_1 + 776, 4);

    if (n != 0) {
        uint64_t i = 0;
        do {
            uint8_t *row = param_1 + i * 8;
            char *name;
            memcpy(&name, row + 264, 8);
            if (core_strcmp(name, param_2) == 0) {
                uint64_t value;
                memcpy(&value, row + 520, 8);
                return value;
            }
            i++;
        } while (i < n);
    }
    return 0;
}

#define S_MODE      0x10fbe2
#define S_HEADER  0x10f08b
#define S_BASE      0x10f0b2
#define S_ERR_TAG   0x10f0bb
#define S_CART      0x10f0e6
#define S_TITLE    0x10f0f0
#define S_SLOT1     0x10f0f6
#define S_CRC32     0x10f0fc
#define S_SAVE      0x10f102
#define S_TYPE      0x10f107
#define S_EEPROM    0x10f10c
#define S_ROM       0x10f10f
#define S_FLASH     0x10f113
#define S_NAND      0x10f119
#define S_ERR_SAVE  0x10f11e
#define S_IRPORT    0x10f135
#define S_BT        0x10f13c
#define S_ERR_CLOSE  0x10f146
#define S_SIZE       0x113990
#define S_ID        0x114b1c
#define HEADER_N  0x26
#define LINE       0x100
#define E_NAME    256
#define E_ATTR_NAME      264
#define E_ATTR_VALUE      520
#define E_ATTR_COUNT      776
#define E_CLASS     780
#define E_SIZE       1024
extern void *files_fopen_resolved(const char *path, const char *mode);
extern int files_stat_resolved(const char *path, void *buf);
extern void db_xml_unescape_string_5(char *output, const char *entry, unsigned n,
                               unsigned size) __asm__("db_xml_unescape_string");
extern int32_t db_xml_read_tag_5(void *fp, uint8_t *state, char *err) __asm__("db_xml_read_tag");
extern int db_vsprintf_limited_5(char *dest, size_t size, const char *format, ...) __asm__("db_vsprintf_limited");
extern int str_qsort_compare_u32_at_28(const void *a, const void *b);
typedef void    *(*fn_malloc)(uint64_t);
typedef void    *(*fn_realloc)(void *, uint64_t);
typedef void     (*fn_free)(void *);
typedef char    *(*fn_fgets_5)(char *, int, void *);
typedef int      (*fn_fflush)(void *);
typedef int      (*fn_ncase)(const char *, const char *, uint64_t);
typedef int      (*fn_case)(const char *, const char *);
typedef int      (*fn_cmp)(const char *, const char *);
typedef uint64_t (*fn_strtoul)(const char *, char **, int);
typedef void     (*fn_qsort)(void *, uint64_t, uint64_t, const void *);
typedef void     (*fn_chkcpy_5)(char *, const char *, uint64_t);
typedef uint64_t (*fn_chklen)(const char *, uint64_t);






static const char *attribute(const uint8_t *e, const char *who) {
    uint32_t n = rd32(e + E_ATTR_COUNT);
    const uint8_t *v = e + E_ATTR_VALUE;
    for (uint32_t i = 0; i < n; i++) {
        const char *name = (const char *)rd_ptr(v + (size_t)i * 8 - 256);
        if (((fn_cmp)sym_libc_strcmp)(name, who) == 0)
            return (const char *)rd_ptr(v + (size_t)i * 8);
    }
    return 0;
}

int32_t db_load_cartridge_database(game_database_t *out, const char *path) {

    void *fp = nds_platform_default()->files.open(nds_platform_default()->user, path, "rb");
    if (!fp) return -1;

    uint8_t info[256];

    nds_platform_default()->files.stat(nds_platform_default()->user, path, info);
    out->file_time = (int64_t)rd64(info + 88);

    game_database_entry_t *table = (game_database_entry_t *)((fn_malloc)sym_libc_malloc)(GAME_DATABASE_INITIAL_CAPACITY * sizeof(game_database_entry_t));

    void *output = (void *)stdout;
    uint8_t state[E_SIZE];
    char err[LINE], line[LINE], title[LINE], id[LINE];
    uint32_t n = 0;

    if (!((fn_fgets_5)sym_libc_fgets)(line, LINE, fp)) goto release;
    if (((fn_ncase)sym_libc_strncasecmp)(line, "<?xml version='1.0' encoding='UTF-8'?>",
                                              HEADER_N) != 0) goto release;

    {

        int r = db_xml_read_tag(fp, state, err);
        const char *name = (const char *)rd_ptr(state + E_NAME);
        if (r == -1 || ((fn_case)sym_libc_strcasecmp)(name, "database") != 0
            || state[E_CLASS] != 0) {

            db_vsprintf_limited(err, LINE, "Wrong tag name: expected database, got %s\n", name, 0);
            goto release;
        }
        if (db_xml_read_tag(fp, state, err) == -1) goto release;
    }

    uint32_t cap = GAME_DATABASE_INITIAL_CAPACITY;
    uint64_t sv_id = 0, sv_size = 0;

    for (;;) {
        const char *name = (const char *)rd_ptr(state + E_NAME);
        uint32_t class = 0;

        if (((fn_case)sym_libc_strcasecmp)(name, "database") == 0
            && state[E_CLASS] == 1) break;

        class = state[E_CLASS];
        if (class != 0) goto release_all;
        if (((fn_case)sym_libc_strcasecmp)(name, "cartridge") != 0)
            goto release_all;
        if (rd32(state + E_ATTR_COUNT) == 0) goto release_all;

        {
            const char *v = attribute(state, "title");
            if (v == 0) goto release_all;
            ((fn_chkcpy_5)fortify_strcpy)(title, v, LINE);
        }

        if (db_xml_read_tag(fp, state, err) == -1) goto release_all;
        if (((fn_case)sym_libc_strcasecmp)((const char *)rd_ptr(state + E_NAME),
                                                "slot1") != 0)
            goto release_all;
        if (state[E_CLASS] != 0) goto release_all;

        if (db_xml_read_tag(fp, state, err) == -1) goto release_all;
        if (((fn_case)sym_libc_strcasecmp)((const char *)rd_ptr(state + E_NAME),
                                                "rom") != 0)
            goto release_all;
        if (state[E_CLASS] != 2) goto release_all;
        if (rd32(state + E_ATTR_COUNT) == 0) goto release_all;

        uint64_t rom_size, rom_crc, rom_id;
        {
            const char *v = attribute(state, "size");
            if (v == 0) goto release_all;
            rom_size = ((fn_strtoul)sym_libc_strtoul)(v, 0, 16);
            if (rd32(state + E_ATTR_COUNT) == 0) goto release_all;

            v = attribute(state, "crc32");
            if (v == 0) goto release_all;
            rom_crc = ((fn_strtoul)sym_libc_strtoul)(v, 0, 16);
        }

        rom_id = (uint64_t)(int64_t)-1;
        id[0] = 0;
        if (rd32(state + E_ATTR_COUNT) != 0) {
            const char *v = attribute(state, "id");
            if (v != 0)
                rom_id = ((fn_strtoul)sym_libc_strtoul)(v, 0, 16);
            if (rd32(state + E_ATTR_COUNT) != 0) {
                const char *t = attribute(state, "title");
                if (t != 0)
                    ((fn_chkcpy_5)fortify_strcpy)(id, t, LINE);
            }
        }

        if (db_xml_read_tag(fp, state, err) == -1) goto release_all;
        name = (const char *)rd_ptr(state + E_NAME);

        uint32_t type = 0;
        if (((fn_case)sym_libc_strcasecmp)(name, "save") == 0) {

            if (rd32(state + E_ATTR_COUNT) == 0) goto release_all;
            const char *v = attribute(state, "size");
            if (v == 0) goto release_all;
            sv_size = ((fn_strtoul)sym_libc_strtoul)(v, 0, 16);
            if (rd32(state + E_ATTR_COUNT) == 0) goto release_all;

            const char *t = attribute(state, "type");
            if (t == 0) goto release_all;

            if (((fn_case)sym_libc_strcasecmp)(t, "eeprom") == 0) {
                type = 2;
            } else if (((fn_case)sym_libc_strcasecmp)(t, "flash") == 0) {
                const char *w = attribute(state, "id");
                if (w != 0)
                    sv_id = ((fn_strtoul)sym_libc_strtoul)(w, 0, 16);
                type = 1;
            } else if (((fn_case)sym_libc_strcasecmp)(t, "nand") == 0) {
                type = 3;
            } else {

                db_vsprintf_limited(err, LINE, "Unknown save type %s.\n", t, 0);
                goto release_all;
            }
            if (db_xml_read_tag(fp, state, err) == -1) goto release_all;
            name = (const char *)rd_ptr(state + E_NAME);
        }

        uint32_t features = 0;
        class = state[E_CLASS];
        if (((fn_case)sym_libc_strcasecmp)(name, "irport") == 0
            && class == 2) {
            if (db_xml_read_tag(fp, state, err) == -1) goto release_all;
            name = (const char *)rd_ptr(state + E_NAME);
            class = state[E_CLASS];
            features = 1;
        }
        if (((fn_case)sym_libc_strcasecmp)(name, "bluetooth") == 0
            && class == 2) {
            if (db_xml_read_tag(fp, state, err) == -1) goto release_all;
            name = (const char *)rd_ptr(state + E_NAME);
            class = state[E_CLASS];
            features |= 2;
        }

        if (((fn_case)sym_libc_strcasecmp)(name, "slot1") != 0)
            goto release_all;
        if (class != 1) goto release_all;

        {
            int r = db_xml_read_tag(fp, state, err);
            name = (const char *)rd_ptr(state + E_NAME);
            if (r == -1
                || ((fn_case)sym_libc_strcasecmp)(name, "cartridge") != 0
                || state[E_CLASS] != 1) goto close_bad;
        }

        {
            uint32_t len = (uint32_t)((fn_chklen)fortify_strlen)(title, LINE) + 1u;
            void *str = ((fn_malloc)sym_libc_malloc)(len);
            game_database_entry_t *e = &table[n];
            e->title = (char *)str;
            e->rom_size = (uint32_t)rom_size;
            e->rom_crc = (uint32_t)rom_crc;
            e->save_type = (uint8_t)type;
            e->save_id = (uint32_t)sv_id;
            e->features = features;
            e->save_size = (uint32_t)sv_size;
            e->game_code = (uint32_t)rom_id;

            db_xml_unescape_string(str, title, len, len);

            uint32_t len2 = (uint32_t)((fn_chklen)fortify_strlen)(id, LINE) + 1u;

            db_xml_unescape_string(e->code, id, len2, GAME_DATABASE_CODE_BYTES - 1);
            n++;
            e->code[GAME_DATABASE_CODE_BYTES - 1] = 0;
            if (n == cap) {
                cap *= 2;
                table = (game_database_entry_t *)((fn_realloc)sym_libc_realloc)(
                            table, (uint64_t)cap * sizeof(game_database_entry_t));
            }
        }

        if (db_xml_read_tag(fp, state, err) == -1) goto release_all;
    }

    {
        game_database_entry_t *base = (game_database_entry_t *)((fn_realloc)sym_libc_realloc)(
                            table, (uint64_t)n * sizeof(game_database_entry_t));
        out->entries = base;
        out->count = n;
        game_database_entry_t **i1 = (game_database_entry_t **)((fn_malloc)sym_libc_malloc)((uint64_t)n * sizeof *i1);
        out->by_crc = i1;
        game_database_entry_t **i2 = (game_database_entry_t **)((fn_malloc)sym_libc_malloc)((uint64_t)n * sizeof *i2);
        out->by_code = i2;
        if (n != 0) {
            i1[0] = base;
            out->by_code[0] = base;
            for (uint32_t i = 1; i < n; i++) {
                game_database_entry_t *e = &base[i];
                out->by_crc[i] = e;
                out->by_code[i] = e;
            }
            i1 = out->by_crc;
        }

        ((fn_qsort)sym_libc_qsort)(i1, n, 8, (const void *)str_qsort_compare_u32_at_28);
        ((fn_qsort)sym_libc_qsort)(out->by_code, n, 8, (const void *)str_qsort_compare_u32_at_36);
        return 0;
    }

close_bad:

    db_vsprintf_limited(err, LINE, "Expected closing cartridge and got (%s, %d)\n",
                       (const char *)rd_ptr(state + E_NAME), state[E_CLASS]);

release_all:
    ((fn_fflush)sym_libc_fflush)(output);
    for (uint32_t i = 0; i < n; i++)
        ((fn_free)sym_libc_free)(table[i].title);
    ((fn_free)sym_libc_free)(table);
    return -1;

release:
    ((fn_fflush)sym_libc_fflush)(output);
    ((fn_free)sym_libc_free)(table);
    return -1;
}
#undef S_MODE
#undef S_HEADER
#undef S_BASE
#undef S_ERR_TAG
#undef S_CART
#undef S_TITLE
#undef S_SLOT1
#undef S_CRC32
#undef S_SAVE
#undef S_TYPE
#undef S_EEPROM
#undef S_ROM
#undef S_FLASH
#undef S_NAND
#undef S_ERR_SAVE
#undef S_IRPORT
#undef S_BT
#undef S_ERR_CLOSE
#undef S_SIZE
#undef S_ID
#undef HEADER_N
#undef LINE
#undef E_NAME
#undef E_ATTR_NAME
#undef E_ATTR_VALUE
#undef E_ATTR_COUNT
#undef E_CLASS
#undef E_SIZE




void *db_find_cartridge_entry(const game_database_t *catalog, uint32_t key,
                         const char *name) {
    void *(*find)(const void *, const void *, uint64_t, uint64_t, void *) =
        (void *(*)(const void *, const void *, uint64_t, uint64_t, void *))
        sym_libc_bsearch;
    int (*compare)(const char *, const char *) =
        (int (*)(const char *, const char *))sym_libc_strcmp;

    game_database_key_t mold = {0};
    mold.entry = &mold.fake;
    mold.fake.game_code = key;

    uint32_t count = catalog->count;
    game_database_entry_t **base = catalog->by_code;
    game_database_entry_t **found =
        (game_database_entry_t **)find(&mold, base, count, 8,
                                 (void *)str_qsort_compare_u32_at_36);

    if (found != 0) {
        game_database_entry_t **arr = catalog->by_code;
        uint64_t raw = (uint64_t)((unsigned char *)found - (unsigned char *)arr);
        uint64_t forward = raw >> 3;
        int64_t back = (int32_t)(raw >> 3);

        while (back >= 0) {
            game_database_entry_t *e = arr[back];
            if (e->game_code != key)
                break;
            int d = compare(e->code, name);
            back -= 1;
            if (d == 0)
                return e;
        }

        count = catalog->count;
        forward += 1;
        while ((uint32_t)forward < count) {
            game_database_entry_t *e = arr[(int32_t)forward];
            if (e->game_code != key)
                return *found;
            int d = compare(e->code, name);
            forward += 1;
            if (d == 0)
                return e;
        }
    } else {
        count = catalog->count;
    }

    if (count == 0)
        return 0;
    {
        game_database_entry_t *e = catalog->entries;
        for (uint64_t i = 0; i < count; i++) {
            if (((e->game_code ^ key) & GAME_DATABASE_CODE_MASK) == 0)
                return e;
            e += 1;
        }
    }
    return 0;
}

static size_t (*core_fread)(void *, size_t, size_t, void *);
static int    (*core_fclose)(void *);
static int    (*core_fseek)(void *, long, int);
static long   (*core_ftell)(void *);
static int    (*core_memcmp)(const void *, const void *, size_t);
static void  *(*core_malloc)(size_t);
static char  *(*core_strcpy_chk)(char *, const char *, size_t);
static void   (*core_qsort)(void *, size_t, size_t, int (*)(const void *, const void *));

int db_load_cheat_index(cheats_t *obj, const char *path) {
    if (!core_fread) {
        core_fread  = (size_t (*)(void *, size_t, size_t, void *))sym_libc_fread;
        core_fclose = (int (*)(void *))sym_libc_fclose;
        core_fseek  = (int (*)(void *, long, int))sym_libc_fseek;
        core_ftell  = (long (*)(void *))sym_libc_ftell;
        core_memcmp = (int (*)(const void *, const void *, size_t))sym_libc_memcmp;
        core_malloc = (void *(*)(size_t))sym_libc_malloc;
        core_strcpy_chk = (char *(*)(char *, const char *, size_t))fortify_strcpy;
        core_qsort  = (void (*)(void *, size_t, size_t, int (*)(const void *, const void *)))sym_libc_qsort;
    }
    void *(*open_fn)(const char *, const char *) = platform_file_open;

    void *f = open_fn(path, "rb");

    core_strcpy_chk(obj->database_path, path, sizeof obj->database_path);
    obj->active = 0;
    obj->active_count = 0;
    obj->index_loaded = 0;
    obj->database_loaded = 0;

    if (f == 0) return -1;

    unsigned char signature[12];
    if (core_fread(signature, 12, 1, f) != 1) { core_fclose(f); return -1; }
    if (core_memcmp("R4 CheatCode", signature, 12) != 0) {
        core_fclose(f); return -1;
    }

    uint32_t reg[4], prev[4];
    core_fseek(f, 256, 0);
    if (core_fread(prev, 4, 4, f) != 4) { core_fclose(f); return -1; }

    uint32_t n = 0;
    while (prev[2] != 0) {
        if (core_fread(prev, 4, 4, f) != 4) { core_fclose(f); return -1; }
        n++;
    }

    obj->index_count = n;
    cheat_index_entry_t *arr = (cheat_index_entry_t *)core_malloc((size_t)n * sizeof(cheat_index_entry_t));
    obj->index = arr;

    core_fseek(f, 0, 0);
    core_fread(prev, 4, 4, f);

    for (uint64_t i = 0; i < (uint64_t)n; i++) {
        core_fread(reg, 4, 4, f);
        cheat_index_entry_t *e = &arr[i];
        e->game_code = prev[0];
        e->crc = prev[1];
        e->offset = prev[2];
        e->size = reg[2] - prev[2];
        for (int k = 0; k < 4; k++) prev[k] = reg[k];
    }

    if (n != 0) {

        core_fseek(f, 0, 2);
        long end = core_ftell(f);
        cheat_index_entry_t *last = &arr[n - 1];
        last->size = (uint32_t)(end - last->offset);
        core_qsort(arr, n, sizeof(cheat_index_entry_t), str_compare_u32_unsigned);
    }

    obj->index_loaded = 1;
    core_fclose(f);
    return 0;
}

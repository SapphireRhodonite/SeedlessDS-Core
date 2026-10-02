#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include <string.h>
#include "frontend/frontend.h"
#include "core_internals.h"

static void *(*core_fopen)(const char *, const char *);
static void *(*core_fdopen)(int, const char *);
static long  (*core_ftell)(void *);
static int   (*core_fseek)(void *, long, int);
static size_t(*core_fread)(void *, size_t, size_t, void *);

int files_read_exact_size(unsigned char *obj, const char *name,
                       void *dest, uint32_t expected_size) {
    if (!core_fopen) {
        core_fopen  = (void *(*)(const char *, const char *))sym_libc_fopen;
        core_fdopen = (void *(*)(int, const char *))sym_libc_fdopen;
        core_ftell  = (long (*)(void *))sym_libc_ftell;
        core_fseek  = (int (*)(void *, long, int))sym_libc_fseek;
        core_fread  = (size_t (*)(void *, size_t, size_t, void *))sym_libc_fread;
    }

    const char *format = "%s%csystem%c%s";
    const char *mode    = "rb";
    const char *dir     = (const char *)(obj + 0x8F838);

    char path[0x410];
    str_vsprintf_limit_1024(path, (long)name, format, dir, '/', '/', name);

    unsigned char *r = (unsigned char *)files_jni_call_static_method_build_result(path, mode);
    if (r == 0) return -1;

    void *f;
    int fd = *(int *)(r + 8);
    if (fd < 0) f = core_fopen(*(const char **)r, mode);
    else        f = core_fdopen(fd, mode);
    files_descriptor_free((void **)r);
    if (f == 0) return -1;

    long where = core_ftell(f);
    core_fseek(f, 0, 2);
    long size = core_ftell(f);
    core_fseek(f, where, 0);

    if ((uint32_t)size != expected_size) return -1;

    size_t n = core_fread(dest, (size_t)(size & 0xffffffffL), 1, f);
    return (n == 1) ? 0 : -1;
}

static void *(*core_fopen_5)(const char *, const char *);
static void *(*core_fdopen_5)(int, const char *);

void *files_fopen_resolved(const char *path, const char *mode) {
    if (!core_fopen_5) {
        core_fopen_5  = (void *(*)(const char *, const char *))sym_libc_fopen;
        core_fdopen_5 = (void *(*)(int, const char *))sym_libc_fdopen;
    }

    unsigned char *r = (unsigned char *)files_jni_call_static_method_build_result(path, mode);
    if (r == 0) return 0;

    int fd = *(int *)(r + 8);
    void *f;
    if (fd < 0)
        f = core_fopen_5(*(const char **)r, mode);
    else
        f = core_fdopen_5(fd, mode);

    files_descriptor_free((void **)r);
    return f;
}

static int (*core_open2)(const char *, int);

int files_open_translate_flags(const char *path, int flags) {
    if (!core_open2)
        core_open2 = (int (*)(const char *, int))fortify_open;

    char mode[4];
    if (flags & 2) { mode[0] = 'r'; mode[1] = '+'; mode[2] = 0; }
    else if (flags & 1) { mode[0] = 'w'; mode[1] = 0; }
    else { mode[0] = 'r'; mode[1] = 0; }

    unsigned char *r = (unsigned char *)files_jni_call_static_method_build_result(path, mode);
    if (r == 0) return -1;

    int fd = *(int *)(r + 8);
    if (fd < 0)
        fd = core_open2(*(const char **)r, flags);
    files_descriptor_free((void **)r);
    return fd;
}

static int (*core_fstat)(int, void *);
static int (*core_stat)(const char *, void *);
static int (*core_close)(int);

int files_stat_resolved(const char *path, void *buf) {
    if (!core_fstat) {
        core_fstat = (int (*)(int, void *))sym_libc_fstat;
        core_stat  = (int (*)(const char *, void *))sym_libc_stat;
        core_close = (int (*)(int))sym_libc_close;
    }
    const char *mode = "r";

    unsigned char *r = (unsigned char *)files_jni_call_static_method_build_result(path, mode);
    if (r == 0) return -1;

    int rc;
    int fd = *(int *)(r + 8);
    if (fd < 0) {
        rc = core_stat(*(const char **)r, buf);
    } else {
        rc = core_fstat(fd, buf);
        core_close(*(int *)(r + 8));
    }
    files_descriptor_free((void **)r);
    return rc;
}

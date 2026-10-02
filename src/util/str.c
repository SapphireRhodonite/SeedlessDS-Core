#include <stdarg.h>
#include <stddef.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stdint.h>
#include <string.h>
#include "core_internals.h"

int str_vsprintf_caller_limit_1(char *dest, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, size, format, ap);
    va_end(ap);
    return r;
}

int str_vsprintf_limit_1024(char *dest, long unused, const char *format, ...) {
    (void)unused;
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, 1024, format, ap);
    va_end(ap);
    return r;
}

int str_vsprintf_caller_limit_2(char *dest, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, size, format, ap);
    va_end(ap);
    return r;
}

int str_vsprintf_limit_256(char *dest, long unused, const char *format, ...) {
    (void)unused;

    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, 256, format, ap);
    va_end(ap);
    return r;
}

int str_vsprintf_limit_2080_1(char *dest, long unused, const char *format, ...) {
    (void)unused;
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, 2080, format, ap);
    va_end(ap);
    return r;
}

int str_vsprintf_caller_limit_3(char *dest, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, size, format, ap);
    va_end(ap);
    return r;
}

int str_vsnprintf_limited_swapped_args(char *dest, size_t slen, size_t maxlen,
                       const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsnprintf(dest, maxlen, slen, format, ap);
    va_end(ap);
    return r;
}

char *str_scan_until_quote_or_escape(char *param_1) {

    unsigned char *x0 = (unsigned char *)param_1;
    unsigned char *x8;
    unsigned char w8, w9;

    w8 = *x0;
    if (w8 != 0x5c) goto L_3;

L_1:
    x8 = x0;
    x8 = x8 + 1;
    w9 = *x8;
    if (w9 == 0x27) {
        x0 = x8;
    }

L_2:

    x0 = x0 + 1;
    w8 = *x0;
    if (w8 == 0x5c) goto L_1;

L_3:
    if (w8 == 0) return (char *)x0;
    if (w8 != 0x27) goto L_2;

    return (char *)x0;
}

int str_qsort_compare_u32_at_28(const void *a, const void *b) {
    uint32_t x = *(uint32_t *)(*(unsigned char **)a + 28);
    uint32_t y = *(uint32_t *)(*(unsigned char **)b + 28);
    if (x == y) return 0;
    return (x <= y) ? -1 : 1;
}

int str_qsort_compare_u32_at_36(const void *a, const void *b) {
    uint32_t x = *(uint32_t *)(*(unsigned char **)a + 36);
    uint32_t y = *(uint32_t *)(*(unsigned char **)b + 36);
    if (x == y) return 0;
    return (x <= y) ? -1 : 1;
}

int str_vsprintf_caller_limit_4(char *dest, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, size, format, ap);
    va_end(ap);
    return r;
}

int str_vsprintf_caller_limit_5(char *dest, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, size, format, ap);
    va_end(ap);
    return r;
}

int32_t str_compare_u32_unsigned(const void *a, const void *b) {
    uint32_t x, y;
    memcpy(&x, a, 4);
    memcpy(&y, b, 4);
    if (x == y) return 0;
    return (x <= y) ? -1 : 1;
}

int str_vsprintf_fixed_format_2080(char *dest, long a, long b, ...) {
    (void)a; (void)b;
    const char *format = "%s%ccheats%c%s.cht";
    va_list ap;
    va_start(ap, b);
    int r = fortify_vsprintf(dest, 2080, format, ap);
    va_end(ap);
    return r;
}

char *str_skip_whitespace(char *s) {

    unsigned char *p = (unsigned char *)s - 1;
    int expected;
    do {
        p += 1;
        expected = ((int (*)(int))sym_libc_isspace)(*p);
    } while (expected != 0);
    return (char *)p;
}

char *str_scan_until_tag_separator(char *s) {

    int (*is_space)(int) = (int (*)(int))sym_libc_isspace;

    unsigned char *p = (unsigned char *)s;
    uint32_t c = *p;
    if (is_space((int)c)) return (char *)p;

    const uint64_t set = 0x4000800000000001ull;

    for (;;) {
        if ((c & 0xff) <= 0x3e) {
            if ((1ull << (c & 0xff)) & set) return (char *)p;
        }
        p += 1;
        c = *p;
        if (is_space((int)c)) return (char *)p;
    }
}

void str_trim_trailing_whitespace(char *s) {

    typedef unsigned long (*fn_strlen)(const char *);
    typedef int            (*fn_isspace)(int);
    fn_strlen  f_strlen  = (fn_strlen)sym_libc_strlen;
    fn_isspace f_isspace = (fn_isspace)sym_libc_isspace;

    unsigned char *p = (unsigned char *)s;

    unsigned long len = f_strlen(s);
    int32_t w8 = (int32_t)len - 1;
    if (w8 < 0) return;

    int64_t idx = (int64_t)w8;
    for (;;) {
        int expected = f_isspace((int)p[idx]);
        if (expected == 0) return;
        p[idx] = 0;
        idx -= 1;
        if ((int32_t)idx < 0) return;
    }
}

int str_sprintf_limited(char *dest, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, size, format, ap);
    va_end(ap);
    return r;
}

int str_vsprintf_limit_2080_2(char *dest, long unused, const char *format, ...) {
    (void)unused;
    va_list ap;
    va_start(ap, format);
    int r = fortify_vsprintf(dest, 2080, format, ap);
    va_end(ap);
    return r;
}

#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "core_internals.h"

void fortify_fail(void)
{
    abort();
}

static void fortify_check_access(size_t claim, size_t size)
{
    if (claim > size)
        fortify_fail();
}

int fortify_vsprintf(char *dest, size_t size, const char *format, va_list ap)
{
    int written = vsnprintf(dest, size == SIZE_MAX ? (size_t)SSIZE_MAX : size, format, ap);

    fortify_check_access((size_t)(written + 1), size);
    return written;
}

int fortify_vsnprintf(char *dest, size_t count, size_t size, const char *format, va_list ap)
{
    fortify_check_access(count, size);
    return vsnprintf(dest, count, format, ap);
}

char *fortify_strcpy(char *dest, const char *src, size_t size)
{
    size_t length = strlen(src) + 1;

    fortify_check_access(length, size);
    return strcpy(dest, src);
}

size_t fortify_strlen(const char *s, size_t size)
{
    size_t length = strlen(s);

    if (length >= size)
        fortify_fail();
    return length;
}

char *fortify_strrchr(const char *p, int ch, size_t size)
{
    const char *found = NULL;

    for (;; ++p, size--) {
        if (size == 0)
            fortify_fail();
        if (*p == (char)ch)
            found = p;
        if (!*p)
            return (char *)found;
    }
}

char *fortify_strncpy(char *dest, const char *src, size_t n, size_t dest_size, size_t src_size)
{
    fortify_check_access(n, dest_size);
    if (n != 0) {
        char *d = dest;
        const char *s = src;

        do {
            if ((size_t)(s - src) >= src_size)
                fortify_fail();
            if ((*d++ = *s++) == 0) {
                while (--n != 0)
                    *d++ = 0;
                break;
            }
        } while (--n != 0);
    }
    return dest;
}

void *fortify_memcpy(void *dest, const void *src, size_t count, size_t size)
{
    if (count > size)
        fortify_fail();
    return memcpy(dest, src, count);
}

void *fortify_memset(void *dest, int byte, size_t count, size_t size)
{
    if (count > size)
        fortify_fail();
    return memset(dest, byte, count);
}

ssize_t fortify_read(int fd, void *buf, size_t count, size_t size)
{
    if (count > (size_t)SSIZE_MAX)
        fortify_fail();
    fortify_check_access(count, size);
    return read(fd, buf, count);
}

int fortify_open(const char *path, int flags)
{
    if ((flags & O_CREAT) == O_CREAT || (flags & O_TMPFILE) == O_TMPFILE)
        fortify_fail();
    return open(path, flags, 0);
}

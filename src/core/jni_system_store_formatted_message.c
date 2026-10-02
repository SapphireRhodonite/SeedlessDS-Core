#include <stdarg.h>
#include <stddef.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "frontend/frontend.h"

static int (*core_vsnprintf)(char *, size_t, const char *, va_list);

int jni_system_store_formatted_message(long a0, long a1, long a2, long a3, ...) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    if (!core_vsnprintf)
        core_vsnprintf = (int (*)(char *, size_t, const char *, va_list))
                         sym_libc_vsnprintf;

    char *dest = FRONTEND->rom_path;
    va_list ap;
    va_start(ap, a3);
    int r = core_vsnprintf(dest, 0x400, "%s", ap);
    va_end(ap);
    return r;
}

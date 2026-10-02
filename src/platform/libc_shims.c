#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"

uint64_t platform_wcrtomb_default_state(void *s, uint32_t wc) {
    return ((uint64_t (*)(void *, uint32_t, void *))sym_libc_wcrtomb)(s, wc, 0);
}

uint64_t platform_mbrtowc_default_state(void *pwc, const void *s, uint64_t n) {
    return ((uint64_t (*)(void *, const void *, uint64_t, void *))
            sym_libc_mbrtowc)(pwc, s, n, 0);
}

uint64_t platform_mbrlen_default_state(const void *s, uint64_t n) {
    return ((uint64_t (*)(const void *, uint64_t, void *))
            sym_libc_mbrlen)(s, n, 0);
}

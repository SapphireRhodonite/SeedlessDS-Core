#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include "cart.h"
#include "core_internals.h"

uint32_t cart_header_is_yv5_title(const cart_t *obj)
{

    uint32_t a, b, c;
    a = obj->game_code;
    memcpy(&b, obj->secure_area, 4);
    memcpy(&c, obj->secure_area + 4, 4);

    if (a == 0x45355659u && b == 0x014a191au && c == 0xa5c470b9u)
        return 1u;

    if (a == 0x50355659u && b == 0xd0d48b67u && c == 0x39392f23u)
        return 1u;

    {
        uint32_t e1 = (a == 0x4a355659u) ? 1u : 0u;
        uint32_t e2 = (b == 0x7829bc8du) ? 1u : 0u;
        uint32_t e3 = (c == 0x9968ef44u) ? 1u : 0u;
        return (e1 & e2) & e3;
    }
}

uint32_t cart_key1_feistel_f(const cart_t *obj, uint32_t v) {
    const unsigned char *t = (const unsigned char *)obj->key1_table;
    uint32_t a, b, c, d;
    memcpy(&a, t + (uint64_t)((v >> 24) + 0x12u) * 4, 4);
    memcpy(&b, t + (uint64_t)(((v >> 16) & 0xffu) + 0x112u) * 4, 4);
    memcpy(&c, t + (uint64_t)(((v >> 8) & 0xffu) + 0x212u) * 4, 4);
    memcpy(&d, t + (uint64_t)((v & 0xffu) + 0x312u) * 4, 4);
    uint32_t r = b + a;
    r ^= c;
    return r + d;
}

void cart_key1_encrypt_block(const cart_t *obj, unsigned char *blk)
{

    const unsigned char *p = (const unsigned char *)obj->key1_table;
    const unsigned char *q = (const unsigned char *)&obj->key1_table[16];

    uint32_t left;
    uint32_t right;
    memcpy(&left, blk, 4);
    memcpy(&right, blk + 4, 4);

    uint32_t mixed = 0;
    uint64_t off = 0;
    do {
        memcpy(&mixed, p + off, 4);
        off += 4;
        mixed ^= right;

        uint32_t a, b, c, d;
        memcpy(&a, p + (uint64_t)((mixed >> 24)          + 0x12u)  * 4, 4);
        memcpy(&b, p + (uint64_t)(((mixed >> 16) & 0xffu) + 0x112u) * 4, 4);
        memcpy(&c, p + (uint64_t)(((mixed >>  8) & 0xffu) + 0x212u) * 4, 4);
        memcpy(&d, p + (uint64_t)((mixed & 0xffu)         + 0x312u) * 4, 4);

        right  = b + a;
        right ^= c;
        right += d;
        right ^= left;
        left  = mixed;
    } while (off != 0x40);

    uint32_t p16, p17;
    memcpy(&p16, q, 4);
    p16 ^= right;
    memcpy(blk, &p16, 4);

    memcpy(&p17, q + 4, 4);
    p17 ^= mixed;
    memcpy(blk + 4, &p17, 4);
}

void cart_key1_decrypt_block(const cart_t *obj, unsigned char *blk)
{

    const unsigned char *s = (const unsigned char *)obj->key1_table;
    const unsigned char *p17 = (const unsigned char *)&obj->key1_table[17];

    uint32_t left;
    uint32_t right;
    memcpy(&left, blk, 4);
    memcpy(&right, blk + 4, 4);

    uint32_t mixed = 0;
    int64_t idx = 0;
    uint64_t cnt;
    do {
        memcpy(&mixed, p17 + idx * 4, 4);
        mixed ^= right;

        uint32_t a, b, c, d;
        memcpy(&a, s + (uint64_t)((mixed >> 24)           + 0x12u)  * 4, 4);
        memcpy(&b, s + (uint64_t)(((mixed >> 16) & 0xffu) + 0x112u) * 4, 4);
        memcpy(&c, s + (uint64_t)(((mixed >>  8) & 0xffu) + 0x212u) * 4, 4);
        memcpy(&d, s + (uint64_t)((mixed & 0xffu)         + 0x312u) * 4, 4);

        right  = b + a;
        right ^= c;
        right += d;

        cnt = (uint64_t)(idx + 0x10);
        idx -= 1;

        right ^= left;
        left  = mixed;
    } while (cnt > 1);

    uint32_t t;

    memcpy(&t, s + 4, 4);
    t ^= right;
    memcpy(blk, &t, 4);

    memcpy(&t, s, 4);
    t ^= mixed;
    memcpy(blk + 4, &t, 4);
}

static uint32_t byte_reverse32(uint32_t v)
{
    return ((v >> 24) & 0x000000ffu) | ((v >> 8) & 0x0000ff00u)
         | ((v << 8) & 0x00ff0000u) | ((v << 24) & 0xff000000u);
}

void cart_key1_decrypt_bytes(const cart_t *obj, unsigned char *out,
                        const unsigned char *ent)
{

    uint32_t e0, e1;
    memcpy(&e0, ent, 4);
    memcpy(&e1, ent + 4, 4);

    const unsigned char *s   = (const unsigned char *)obj->key1_table;
    const unsigned char *p17 = (const unsigned char *)&obj->key1_table[17];

    uint32_t right = byte_reverse32(e0);
    uint32_t left = byte_reverse32(e1);

    uint32_t mixed = 0;
    int64_t  idx = 0;
    uint64_t cnt;
    do {
        memcpy(&mixed, p17 + idx * 4, 4);
        mixed ^= right;

        uint32_t a, b, c, d;
        memcpy(&a, s + (uint64_t)((mixed >> 24)           + 0x12u)  * 4, 4);
        memcpy(&b, s + (uint64_t)(((mixed >> 16) & 0xffu) + 0x112u) * 4, 4);
        memcpy(&c, s + (uint64_t)(((mixed >>  8) & 0xffu) + 0x212u) * 4, 4);
        memcpy(&d, s + (uint64_t)((mixed & 0xffu)         + 0x312u) * 4, 4);

        right  = b + a;
        right ^= c;

        cnt = (uint64_t)(idx + 0x10);

        right += d;

        idx -= 1;

        right ^= left;
        left  = mixed;
    } while (cnt > 1);

    uint32_t q0, q1;
    memcpy(&q0, s, 4);
    memcpy(&q1, s + 4, 4);

    q0 ^= mixed;
    q1 ^= right;

    out[0] = (unsigned char)(q0 >> 24);
    out[3] = (unsigned char)(q0);
    out[1] = (unsigned char)(q0 >> 16);
    out[2] = (unsigned char)(q0 >>  8);
    out[4] = (unsigned char)(q1 >> 24);
    out[5] = (unsigned char)(q1 >> 16);
    out[6] = (unsigned char)(q1 >>  8);
    out[7] = (unsigned char)(q1);
}


static uint32_t rev32(uint32_t v)
{
    return ((v >> 24) & 0x000000ffu) | ((v >> 8) & 0x0000ff00u)
         | ((v << 8) & 0x00ff0000u) | ((v << 24) & 0xff000000u);
}

static uint32_t f_mix(const cart_t *obj, uint32_t x)
{
    uint32_t a = obj->key1_table[((x >> 24) & 0xffu) + 0x12u];
    uint32_t b = obj->key1_table[((x >> 16) & 0xffu) + 0x112u];
    uint32_t c = obj->key1_table[((x >> 8) & 0xffu) + 0x212u];
    uint32_t d = obj->key1_table[(x & 0xffu) + 0x312u];
    return ((b + a) ^ c) + d;
}

static void round16(const cart_t *obj, uint32_t *pl, uint32_t *pr)
{
    uint32_t l = *pl, r = *pr, t = 0;
    for (uint32_t i = 0; i < 16; i++) {
        t = obj->key1_table[i] ^ r;
        uint32_t r_updated = f_mix(obj, t) ^ l;
        l = t;
        r = r_updated;
    }
    *pl = l;
    *pr = r;
}

void cart_key1_apply_keycode(cart_t *obj)
{

    uint32_t l1 = obj->keycode[1];
    uint32_t r1 = obj->keycode[2];
    round16(obj, &l1, &r1);

    uint32_t p16 = obj->key1_table[16];
    uint32_t p17 = obj->key1_table[17];

    uint32_t state_c_updated = p17 ^ l1;
    uint32_t state_b_updated = p16 ^ r1;
    obj->keycode[2] = (state_c_updated);
    obj->keycode[1] = (state_b_updated);

    uint32_t l2 = obj->keycode[0];
    uint32_t r2 = state_b_updated;
    round16(obj, &l2, &r2);

    uint32_t state_a_updated = r2 ^ p16;
    uint32_t state_b_new2 = l2 ^ p17;
    obj->keycode[0] = (state_a_updated);
    obj->keycode[1] = (state_b_new2);

    uint32_t rev_a = rev32(state_a_updated);
    uint32_t rev_b = rev32(state_b_new2);

    p16 ^= rev_a;
    p17 ^= rev_b;
    obj->key1_table[16] = (p16);
    obj->key1_table[17] = (p17);

    uint32_t p14 = obj->key1_table[14] ^ rev_a;
    uint32_t p15 = obj->key1_table[15] ^ rev_b;
    obj->key1_table[14] = (p14);
    obj->key1_table[15] = (p15);

    for (uint32_t i = 0; i <= 12; i += 2) {
        uint32_t pi = obj->key1_table[i] ^ rev_a;
        uint32_t pi1 = obj->key1_table[i + 1] ^ rev_b;
        obj->key1_table[i] = (pi);
        obj->key1_table[i + 1] = (pi1);
    }

    uint32_t l3 = 0, r3 = 0;
    uint32_t idx = 0;
    do {
        round16(obj, &l3, &r3);
        uint32_t p16_current = obj->key1_table[16];
        uint32_t p17_current = obj->key1_table[17];
        uint32_t old = idx;
        idx += 2;
        uint32_t in_old = p17_current ^ l3;
        uint32_t in_old1 = p16_current ^ r3;
        obj->key1_table[old] = (in_old);
        obj->key1_table[old + 1] = (in_old1);

        l3 = in_old1;
        r3 = in_old;
    } while (idx < CART_KEY1_TABLE_WORDS);
}

extern void cart_key1_apply_keycode_6(cart_t *obj) __asm__("cart_key1_apply_keycode");

void cart_key1_init_keycode(cart_t *base, void *param_2)
{


    uint32_t uVar1;
    uVar1 = base->game_code;

    uint32_t v0 = uVar1;
    uint32_t v1 = uVar1 >> 1;
    uint32_t v2 = uVar1 << 1;
    base->keycode[0] = v0;
    base->keycode[1] = v1;
    base->keycode[2] = v2;

    memcpy(base->key1_table, param_2, sizeof base->key1_table);

    cart_key1_apply_keycode(base);
    cart_key1_apply_keycode(base);
}

#define MAGIC_ENCRYOBJ 0x6A624F7972636E65ull
#define DECRYPTED_FILL 0xE7FFDEFFE7FFDEFFull
extern void cart_key1_apply_keycode_7(cart_t *obj) __asm__("cart_key1_apply_keycode");
typedef void *(*fn_memcpy)(void *, const void *, size_t);

static uint32_t f_mix_7(const cart_t *obj, uint32_t x)
{
    uint32_t a = obj->key1_table[((x >> 24) & 0xffu) + 0x12u];
    uint32_t b = obj->key1_table[((x >> 16) & 0xffu) + 0x112u];
    uint32_t c = obj->key1_table[((x >>  8) & 0xffu) + 0x212u];
    uint32_t d = obj->key1_table[(x & 0xffu)         + 0x312u];
    return ((b + a) ^ c) + d;
}

static void decrypt64(cart_t *obj, unsigned char *blk,
                       uint32_t *out0, uint32_t *out1)
{
    uint32_t y, x;
    memcpy(&y, blk, 4);
    memcpy(&x, blk + 4, 4);

    uint32_t z = 0;
    for (uint32_t i = 0; i < 16u; i++) {

        z = obj->key1_table[17 - i] ^ x;
        x = f_mix_7(obj, z) ^ y;
        y = z;
    }

    uint32_t p1 = obj->key1_table[1];
    uint32_t s0 = p1 ^ x;
    memcpy(blk, &s0, 4);
    uint32_t p0 = obj->key1_table[0];
    uint32_t s1 = p0 ^ z;
    memcpy(blk + 4, &s1, 4);

    *out0 = s0;
    *out1 = s1;
}

uint32_t cart_key1_decrypt_secure_area(cart_t *ctx, unsigned char *area,
                            uint64_t x2_unused, const void *table)
{

    (void)x2_unused;

    uint32_t sem = ctx->game_code;
    ctx->keycode[0] = (sem);
    ctx->keycode[1] = (sem >> 1);
    ctx->keycode[2] = (sem << 1);

    {
        static fn_memcpy s_memcpy;
        if (!s_memcpy) s_memcpy = (fn_memcpy)sym_libc_memcpy;
        s_memcpy(ctx->key1_table, table, sizeof ctx->key1_table);
    }

    cart_key1_apply_keycode(ctx);
    cart_key1_apply_keycode(ctx);

    uint32_t d0, d1;
    decrypt64(ctx, area, &d0, &d1);

    uint32_t kb = ctx->keycode[1];
    uint32_t kc = ctx->keycode[2];
    ctx->keycode[1] = (kb << 1);
    ctx->keycode[2] = (kc >> 1);

    cart_key1_apply_keycode(ctx);

    decrypt64(ctx, area, &d0, &d1);

    uint64_t decrypted = (uint64_t)d0 | ((uint64_t)d1 << 32);
    if (decrypted != MAGIC_ENCRYOBJ)
        return 0xffffffffu;

    for (uint32_t i = 2u; i < 0x200u; i += 2u)
        decrypt64(ctx, area + (size_t)i * 4u, &d0, &d1);

    uint64_t fill = DECRYPTED_FILL;
    memcpy(area, &fill, 8);

    return 0;
}
#undef MAGIC_ENCRYOBJ
#undef DECRYPTED_FILL

#define MAGIC_ENCRYOBJ 0x6A624F7972636E65ull
extern void cart_key1_apply_keycode_8(cart_t *obj) __asm__("cart_key1_apply_keycode");
typedef void *(*fn_memcpy_8)(void *, const void *, size_t);

static uint32_t f_mix_8(const cart_t *obj, uint32_t x)
{
    uint32_t a = obj->key1_table[((x >> 24) & 0xffu) + 0x12u];
    uint32_t b = obj->key1_table[((x >> 16) & 0xffu) + 0x112u];
    uint32_t c = obj->key1_table[((x >>  8) & 0xffu) + 0x212u];
    uint32_t d = obj->key1_table[(x & 0xffu)         + 0x312u];
    return ((b + a) ^ c) + d;
}

static void encrypt64(cart_t *obj, unsigned char *blk)
{
    uint32_t y, x;
    memcpy(&y, blk, 4);
    memcpy(&x, blk + 4, 4);

    uint32_t z = 0;
    for (uint32_t i = 0; i < 16u; i++) {
        z = obj->key1_table[i] ^ x;
        x = f_mix_8(obj, z) ^ y;
        y = z;
    }

    uint32_t p16 = obj->key1_table[16];
    uint32_t s0 = p16 ^ x;
    memcpy(blk, &s0, 4);
    uint32_t p17 = obj->key1_table[17];
    uint32_t s1 = p17 ^ z;
    memcpy(blk + 4, &s1, 4);
}

static void prepare_key(cart_t *obj, const void *table,
                          int third_pass)
{

    uint32_t sem = obj->game_code;
    obj->keycode[0] = (sem);
    obj->keycode[1] = (sem >> 1);
    obj->keycode[2] = (sem << 1);

    static fn_memcpy_8 s_memcpy;
    if (!s_memcpy) s_memcpy = (fn_memcpy_8)sym_libc_memcpy;
    s_memcpy(obj->key1_table, table, sizeof obj->key1_table);

    cart_key1_apply_keycode(obj);
    cart_key1_apply_keycode(obj);

    uint32_t b = obj->keycode[1];
    uint32_t c = obj->keycode[2];
    obj->keycode[1] = (b << 1);
    obj->keycode[2] = (c >> 1);

    if (third_pass) cart_key1_apply_keycode(obj);
}

uint32_t cart_key1_encrypt_secure_area(cart_t *ctx, unsigned char *area,
                            uint64_t x2_unused, const void *table)
{

    (void)x2_unused;

    uint64_t magic = MAGIC_ENCRYOBJ;
    memcpy(area, &magic, 8);

    prepare_key(ctx, table, 1);

    for (uint32_t i = 0; i < 0x200u; i += 2u)
        encrypt64(ctx, area + (size_t)i * 4u);

    prepare_key(ctx, table, 0);

    encrypt64(ctx, area);

    return 0;
}
#undef MAGIC_ENCRYOBJ

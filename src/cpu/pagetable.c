#include <stdint.h>
#include <string.h>
#include "hires_runtime.h"
#include "core/nds_state.h"
#define PT(p) ((pagetable_t *)(p))
#include "blob_symbols.h"
#include "core_internals.h"
#include "core/nds_state.h"
#include "core/nds_state.h"
#define CPU(p) ((arm_t *)(p))

void pagetable_page_set(unsigned char *table, uint64_t memory, uint32_t dir,
                        uint32_t distinct)
{

    unsigned char *top = (unsigned char *)&PT(table)->group_bits[dir >> 21];
    uint32_t word_top;
    memcpy(&word_top, top, 4);
    word_top |= 1u << ((dir >> 16) & 31);
    memcpy(top, &word_top, 4);

    unsigned char *bottom = (unsigned char *)&PT(table)->page_bits[dir >> 16];
    uint32_t word_bottom;
    memcpy(&word_bottom, bottom, 4);

    int64_t desc = (int64_t)(memory - (uint64_t)dir) >> 2;
    uint64_t with_bit = (uint64_t)desc | 0x4000000000000000ULL;
    uint64_t value = (distinct == 0) ? (uint64_t)desc : with_bit;

    word_bottom |= 1u << ((dir >> 11) & 31);
    memcpy(bottom, &word_bottom, 4);

    PT(table)->page[dir >> 11] = value;
}

void pagetable_range_set(unsigned char *table, uint64_t memory, uint32_t dir,
                        uint32_t len, uint32_t distinct)
{

    if (distinct == 0) {
        do {

            unsigned char *top =
                (unsigned char *)&PT(table)->group_bits[dir >> 21];
            uint32_t word_top;
            memcpy(&word_top, top, 4);
            uint32_t idx_bottom = dir >> 16;
            word_top |= 1u << (idx_bottom & 31);
            unsigned char *bottom =
                (unsigned char *)&PT(table)->page_bits[idx_bottom];
            memcpy(top, &word_top, 4);

            uint32_t word_bottom;
            memcpy(&word_bottom, bottom, 4);

            int64_t desc = (int64_t)(memory - (uint64_t)dir) >> 2;
            uint32_t idx_page = dir >> 11;
            uint64_t value = (uint64_t)desc;

            word_bottom |= 1u << (idx_page & 31);

            memcpy(bottom, &word_bottom, 4);
            PT(table)->page[idx_page] = value;

            memory += 0x800u;
            dir += 0x800u;
            len -= 0x800u;
        } while (len != 0);
    } else {
        do {

            unsigned char *top =
                (unsigned char *)&PT(table)->group_bits[dir >> 21];
            uint32_t word_top;
            memcpy(&word_top, top, 4);
            uint32_t idx_bottom = dir >> 16;
            word_top |= 1u << (idx_bottom & 31);
            unsigned char *bottom =
                (unsigned char *)&PT(table)->page_bits[idx_bottom];
            memcpy(top, &word_top, 4);

            uint32_t word_bottom;
            memcpy(&word_bottom, bottom, 4);

            int64_t desc = (int64_t)(memory - (uint64_t)dir) >> 2;
            uint32_t idx_page = dir >> 11;
            uint64_t value = (uint64_t)desc | 0x4000000000000000ULL;

            word_bottom |= 1u << (idx_page & 31);

            memcpy(bottom, &word_bottom, 4);
            PT(table)->page[idx_page] = value;

            memory += 0x800u;
            dir += 0x800u;
            len -= 0x800u;
        } while (len != 0);
    }
}

void pagetable_page_refresh(unsigned char *table, uint32_t dir)
{

    uint32_t idx_page = dir >> 11;

    uintptr_t read_mask = 0;
    uintptr_t write = 0;
    bus_region_t *base_v = NULL;
    uint32_t idx = 0;
    bus_region_t *entry = 0;
    uint64_t value;

    if ((dir >> 28) != 0)
        goto path_height;

    base_v = PT(table)->region;
    idx = dir >> 23;

    entry = &base_v[idx];

    if (entry->read_kind == 0)
        goto has_read;

    read_mask = 0;
    {

        bus_region_t *e2 = &base_v[idx];
        if (e2->write_kind == 0)
            goto has_write;
    }

no_write:
    write = 0;
    if (read_mask != 0)
        goto end;
    goto no_memory;

path_height:

    if (dir < 0xffff0000u)
        goto no_memory;
    {
        const arm_t *state = PT(table)->cpu;

        uint32_t mark = CPU(state)->is_arm9;
        if (mark != 1)
            goto no_memory;

        uint32_t shift = dir & 0xfffu;
        write = 0;
        read_mask = (uintptr_t)PT(table)->bus->bios9 + shift;
    }
    goto end;

has_read:
    {
        bus_region_t *e2 = &base_v[idx];
        uint32_t mask;
        mask = entry->mask;
        uint8_t *ptr;
        ptr = e2->read_memory;
        uint64_t dir64 = (uint64_t)dir;
        read_mask = (uintptr_t)ptr + ((uint64_t)mask & dir64);

        bus_region_t *e3 = &base_v[idx];
        if (e3->write_kind != 0)
            goto no_write;
    }

has_write:
    {
        bus_region_t *e4 = &base_v[idx];
        uint32_t mask2;
        mask2 = entry->mask;
        uint8_t *ptr2;
        ptr2 = e4->read_memory;
        uint64_t dir64 = (uint64_t)dir;
        write = (uintptr_t)ptr2 + ((uint64_t)mask2 & dir64);
    }
    if (read_mask == 0)
        goto no_memory;

end:
    {

        int64_t desc = (int64_t)(read_mask - (uint64_t)dir) >> 2;
        uint64_t with_bit = (uint64_t)desc | 0x4000000000000000ULL;
        value = (read_mask == write) ? (uint64_t)desc : with_bit;
    }
    PT(table)->page[idx_page] = value;
    return;

no_memory:
    value = 0x4000000000000000ULL;
    PT(table)->page[idx_page] = value;
}

void pagetable_range_refresh(unsigned char *table, uint32_t dir, uint32_t len)
{

    bus_t         *state;
    uint32_t       idx_page;
    uintptr_t      rd;
    uintptr_t      wr;
    bus_region_t  *base;
    uint32_t       idx_reg;
    bus_region_t  *ent;
    unsigned char  flag;
    uint32_t       mask;
    uintptr_t      ptr;
    arm_t         *cfg_p;
    uint32_t       cfg;
    int64_t        desc;
    uint64_t       value;

    state = PT(table)->bus;

    idx_page = dir >> 11;
    if ((dir >> 28) != 0)
        goto L_3;
    goto L_2;

L21070:

    rd = (uintptr_t)state->bios9 + (uint64_t)(dir & 0xfffu);
    wr = 0;
    goto L21080;

L21080:

    desc = (int64_t)(rd - (uint64_t)dir) >> 2;
    value = (rd == wr)
                ? (uint64_t)desc
                : ((uint64_t)desc | 0x4000000000000000ULL);
    len -= 0x800u;
    dir      += 0x800u;
    PT(table)->page[idx_page] = value;

    if (len == 0)
        return;

L_1:
    idx_page = dir >> 11;
    if ((dir >> 28) != 0)
        goto L_3;

L_2:

    base = PT(table)->region;
    idx_reg = dir >> 23;
    ent = &base[idx_reg];

    flag = ent->read_kind;
    if (flag == 0)
        goto L21128;

    rd = 0;
    flag = base[idx_reg].write_kind;
    if (flag == 0)
        goto L_4;

    wr = 0;
    if (rd != 0)
        goto L21080;
    goto L21168;

L_3:

    if (dir < 0xffff0000u)
        goto L21110;
    cfg_p = PT(table)->cpu;
    cfg = cfg_p->is_arm9;

    if (cfg == 1)
        goto L21070;

    value = 0x4000000000000000ULL;
    len -= 0x800u;
    dir      += 0x800u;
    PT(table)->page[idx_page] = value;
    if (len != 0)
        goto L_1;
    return;

L21110:

    value = 0x4000000000000000ULL;
    len -= 0x800u;
    dir      += 0x800u;
    PT(table)->page[idx_page] = value;
    if (len != 0)
        goto L_1;
    return;

L21128:

    mask = ent->mask;
    ptr = (uintptr_t)base[idx_reg].read_memory;
    rd = ptr + ((uint64_t)mask & (uint64_t)dir);

    flag = base[idx_reg].write_kind;
    if (flag != 0)
        goto L_5;

L_4:

    mask = ent->mask;
    ptr = (uintptr_t)base[idx_reg].read_memory;
    wr = ptr + ((uint64_t)mask & (uint64_t)dir);

    if (rd != 0)
        goto L21080;

L21168:

    value = 0x4000000000000000ULL;
    len -= 0x800u;
    dir      += 0x800u;
    PT(table)->page[idx_page] = value;
    if (len != 0)
        goto L_1;
    return;

L_5:
    wr = 0;
    if (rd != 0)
        goto L21080;
    goto L21168;
}

void pagetable_page_set_guarded(unsigned char *table, uint64_t memory, uint32_t dir,
                        uint32_t distinct)
{

    const bus_t *state = PT(table)->bus;

    uint32_t cap = state->itcm_size;
    uint32_t lim = dir + 0x800u;
    if (lim < cap)
        return;

    uint32_t start = state->dtcm_start;
    if (!(start > dir)) {
        uint32_t end = state->dtcm_end;
        if (lim < end)
            return;
    }

    unsigned char *top = (unsigned char *)&PT(table)->group_bits[dir >> 21];
    uint32_t word_top;
    memcpy(&word_top, top, 4);
    word_top |= 1u << ((dir >> 16) & 31);
    memcpy(top, &word_top, 4);

    unsigned char *bottom = (unsigned char *)&PT(table)->page_bits[dir >> 16];
    uint32_t word_bottom;
    memcpy(&word_bottom, bottom, 4);

    int64_t desc = (int64_t)(memory - (uint64_t)dir) >> 2;
    uint64_t with_bit = (uint64_t)desc | 0x4000000000000000ULL;
    uint64_t value = (distinct == 0) ? (uint64_t)desc : with_bit;

    word_bottom |= 1u << ((dir >> 11) & 31);
    memcpy(bottom, &word_bottom, 4);

    PT(table)->page[dir >> 11] = value;
}

static void sweep_until(uint32_t *w, uint32_t bit, uint64_t idx, uint64_t lim,
                        uint64_t *page) {
    for (;;) {
        if (*w & bit) {
            *w &= ~bit;
            page[idx] = PAGETABLE_MISSING;
        }
        if (idx >= lim) break;
        idx++;
        bit <<= 1;
    }
}

static void sweep_skipping(uint32_t *w, uint32_t bit, uint32_t idx, uint32_t lim,
                          uint64_t *page) {
    for (;;) {
        if (*w & bit) {
            *w &= ~bit;
            page[idx] = PAGETABLE_MISSING;
        }
        idx++;
        bit <<= 1;
        if (idx > lim) break;
    }
}

static void sweep_word(uint32_t w, uint64_t *page, uint64_t base) {
    while (w != 0) {
        if (w & 1) page[base] = PAGETABLE_MISSING;
        w >>= 1;
        base++;
    }
}

void pagetable_unmap_range(unsigned char *t, uint32_t dir, uint32_t len) {
    uint64_t *page = (uint64_t *)t;
    uint32_t *b1  = PT(t)->group_bits;
    uint32_t *b2  = PT(t)->page_bits;

    uint32_t end = dir + len - 1;
    uint32_t i1i = dir >> 21, i1f = end >> 21;
    uint32_t c1i = (dir >> 16) & 31, c1f = (end >> 16) & 31;
    uint32_t i2i = dir >> 16;
    uint32_t c2i = (dir >> 11) & 31, c2f = (end >> 11) & 31;
    uint64_t pini = dir >> 11;
    uint32_t m1 = 1u << c1i;

    uint32_t *pw1 = &b1[i1i];
    uint32_t w1 = *pw1;

    if (i1i == i1f && c1i == c1f) {

        if ((w1 & m1) == 0) { *pw1 = w1; return; }
        uint32_t *pw2 = &b2[i2i];
        uint32_t w2 = *pw2;
        sweep_until(&w2, 1u << c2i, pini, pini + c2f, page);
        *pw2 = w2;
        if (w2 != 0) { *pw1 = w1; return; }
        w1 &= ~m1;
        *pw1 = w1;
        return;
    }

    if (w1 & m1) {
        uint32_t *pw2 = &b2[i2i];
        uint32_t w2 = *pw2;
        sweep_until(&w2, 1u << c2i, pini, (uint32_t)(pini + 31), page);
        *pw2 = w2;
        if (i1i == i1f ? (w2 != 0) : (w2 == 0))
            w1 &= ~m1;
    }

    if (i1i == i1f) {

        uint32_t cap = c1f + i2i;
        uint32_t mm = m1 << 1;
        uint64_t k = i2i + 1;
        int has = ((w1 & mm) != 0);

        if (k < cap) {
            for (;;) {
                if (has) {
                    uint32_t v = b2[k];
                    b2[k] = 0;
                    if (v != 0) sweep_word(v, page, k << 5);
                    w1 &= ~mm;
                }
                do {
                    mm <<= 1;
                    k++;
                    has = ((w1 & mm) != 0);
                    if (k == cap) goto close_b;
                } while (!has);
            }
        }
    close_b:
        if (!has) { *pw1 = w1; return; }
        {
            uint32_t *pw2 = &b2[k];
            uint32_t v = *pw2;
            sweep_skipping(&v, 1, (uint32_t)(k << 5), c2f | (uint32_t)(k << 5), page);
            *pw2 = v;
            if (v == 0) w1 &= ~mm;
            *pw1 = w1;
        }
        return;
    }

    {
        uint64_t k = i2i + 1;
        if ((k & 31) != 0) {
            uint64_t base = (uint32_t)((i2i << 5) + 0x20);
            m1 <<= 1;
            int has = ((m1 & w1) != 0);
            for (;;) {
                if (has) {
                    uint32_t v = b2[k];
                    b2[k] = 0;
                    if (v != 0) sweep_word(v, page, base);
                    w1 &= ~m1;
                }
                do {
                    k++;
                    base += 32;
                    if ((k & 31) == 0) goto mid;
                    m1 <<= 1;
                    has = ((m1 & w1) != 0);
                } while (!has);
            }
        }
    }

mid:
    {
        uint32_t k1 = i1i + 1;
        *pw1 = w1;
        if (k1 < i1f) {
            uint64_t pb  = (uint32_t)((i1i << 10) + 0x400);
            uint64_t ib2 = (uint32_t)((i1i << 5) + 0x20);
            uint64_t kk  = k1;
            for (;;) {
                uint32_t v1 = b1[kk];
                b1[kk] = 0;
                if (v1 != 0) {
                    uint64_t p = pb, j = ib2;
                    while (v1 != 0) {
                        if (v1 & 1) {
                            uint32_t v2 = b2[j];
                            b2[j] = 0;
                            if (v2 != 0) sweep_word(v2, page, p);
                        }
                        v1 >>= 1;
                        j++;
                        p += 32;
                    }
                }
                k1++;
                ib2 += 32;
                kk++;
                if (k1 == i1f) break;
                pb += 1024;
            }
            k1 = i1f;
        }

        uint32_t *pw1b = &b1[k1];
        uint32_t w1b = *pw1b;
        uint64_t j    = (uint32_t)(k1 << 5);
        uint64_t jlim = (uint32_t)(c1f | (k1 << 5));
        uint32_t mm2;
        uint64_t last;

        if (j < jlim) {
            mm2 = 1;
            int has = (w1b & 1) != 0;
            for (;;) {
                if (has) {
                    uint32_t v = b2[j];
                    b2[j] = 0;
                    if (v != 0) sweep_word(v, page, j << 5);
                    w1b &= ~mm2;
                }
                do {
                    mm2 <<= 1;
                    j++;
                    has = ((w1b & mm2) != 0);
                    if (j == jlim) goto span_final;
                } while (!has);
            }
        span_final:
            if ((w1b & mm2) == 0) { *pw1b = w1b; return; }
            mm2 = ~mm2;
            last = jlim;
        } else {
            if ((w1b & 1) == 0) { *pw1b = w1b; return; }
            mm2 = 0xfffffffeu;
            last = j;
        }

        {
            uint32_t *pw2 = &b2[last];
            uint32_t v = *pw2;
            sweep_skipping(&v, 1, (uint32_t)(last << 5),
                          c2f | (uint32_t)(last << 5), page);

            w1b &= (v == 0) ? mm2 : 0xffffffffu;
            *pw2 = v;
            *pw1b = w1b;
        }
    }
}

void pagetable_clip_write(unsigned char *machine, uint32_t dir, uint32_t len) {

    void (*write_base)(unsigned char *, uint32_t, uint32_t) = pagetable_unmap_range;

    unsigned char *v = (unsigned char *)&PT(machine)->bus->dtcm_start;
    uint32_t start = *(uint32_t *)v;

    uint32_t pre = start - dir;
    if (start > dir) { write_base(machine, dir, len); return; }

    uint32_t end = *(uint32_t *)(v + 4);
    uint32_t post = (len + dir) - end;
    if ((len + dir) >= end) { write_base(machine, dir, len); return; }

    write_base(machine, dir, pre);
    write_base(machine, start, post);
}

static void record(unsigned char *table, uint32_t p, int64_t descriptor) {
    uint32_t i1 = p >> 21;
    PT(table)->group_bits[i1] |= 1u << ((p >> 16) & 31);
    uint32_t i2 = p >> 16;
    PT(table)->page_bits[i2] |= 1u << ((p >> 11) & 31);
    *(uint64_t *)(table + (uint64_t)(p >> 11) * 8) = (uint64_t)descriptor;
}

void pagetable_dtcm_remap(bus_t *bus, uint32_t base, uint32_t len) {
    int (*unmap)(void *, uint64_t) = (int (*)(void *, uint64_t))sym_libc_munmap;
    void *(*map)(void *, uint64_t, int, int, int, long) =
        (void *(*)(void *, uint64_t, int, int, int, long))sym_libc_mmap;
    void (*redo_cart)(bus_t *, uint32_t) = pagetable_itcm_resize;
    void (*redo_mirror)(bus_t *) = wram_apply_wramcnt;

    uint32_t base_old = bus->dtcm_start;
    uint32_t end_old  = bus->dtcm_end;
    unsigned char *table = (unsigned char *)bus->arm9_pagetable;
    uint32_t len_old = end_old - base_old;

    if (base_old == base && len_old == len)
        return;

    if (len_old != 0) {

        unsigned char *special = (unsigned char *)PT(table)->bus;
        uint32_t p = base_old;
        for (;;) {
            uint32_t page = p >> 11;
            unsigned char *read_base = 0, *write_base = 0;
            int formula = 0;

            if ((p >> 28) != 0) {
                if ((uint64_t)p + PAGETABLE_TOP_BLOCK_BYTES >= NDS_ADDRESS_SPACE_BYTES) {
                    unsigned char *q = (unsigned char *)PT(table)->cpu;
                    if (((arm_t *)q)->is_arm9 == 1) {
                        read_base = ((bus_t *)special)->bios9 + (p & 0xfff);
                        write_base = 0;
                        formula = 1;
                    }
                }
            } else {
                bus_region_t *e = &PT(table)->region[p >> 23];
                if (e->read_kind == 0)
                    read_base = e->read_memory + (e->mask & p);
                if (e->write_kind == 0)
                    write_base = e->read_memory + (e->mask & p);
                formula = (read_base != 0);
            }

            uint64_t v;
            if (formula) {
                int64_t d = (int64_t)((uint64_t)read_base - (uint64_t)p) >> 2;
                v = (read_base == write_base) ? (uint64_t)d
                                     : ((uint64_t)d | 0x4000000000000000ULL);
            } else {
                v = 0x4000000000000000ULL;
            }
            *(uint64_t *)(table + (uint64_t)page * 8) = v;

            p += 0x800;
            if (end_old == p)
                break;
        }

        if ((base_old >> 26) == 0) {
            if ((base_old >> 24) == 2) {
                uint32_t amount = (end_old > 0x4000000u)
                                ? 0x4000000u - base_old : len_old;
                amount >>= 14;
                if (amount != 0) {
                    unsigned char *mirror = bus->address_window;
                    uint64_t cap = ((uint64_t)amount << 14) - 0x4000;
                    unsigned char *start = mirror + base_old;
                    uint32_t masked = base_old & 0x3fffff;
                    uint64_t d = 0;
                    for (;;) {
                        unsigned char *pg = start + (uint32_t)d;
                        unmap(pg, 0x4000);
                        int fd = bus->shared_region_fd;
                        void *r = map(pg, 0x4000, 3, 1, fd,
                                         (long)(uint32_t)(masked + (uint32_t)d));
                        if (cap == d) break;
                        if (r != pg) break;
                        d += 0x4000;
                    }
                }
            } else if ((base_old >> 25) == 0) {
                redo_cart(bus, bus->itcm_size);
            } else {
                redo_mirror(bus);
            }
        }
    }

    bus->dtcm_start = base;
    bus->dtcm_end = len + base;

    if (len > 0x4000) {

        uint32_t remaining = len;
        uint32_t block = base;
        for (;;) {
            unsigned char *mem = bus->dtcm;
            int64_t descriptor = (int64_t)((uint64_t)mem - (uint64_t)block) >> 2;
            for (int k = 0; k < 8; k++)
                record(table, block + (uint32_t)k * 0x800, descriptor);

            if ((block >> 26) == 0 && (remaining >> 14) != 0) {
                unsigned char *mirror = bus->address_window;
                uint64_t cap = (uint64_t)(remaining & 0xffffc000u) - 0x4000;
                unsigned char *start = mirror + block;
                uint64_t d = 0;
                for (;;) {
                    unsigned char *pg = start + (uint32_t)d;
                    unmap(pg, 0x4000);
                    int fd = bus->shared_region_fd;
                    void *r = map(pg, 0x4000, 3, 1, fd,
                                     (long)(uint32_t)((uint32_t)d + BUS_SHARED_DTCM));
                    if (cap == d) break;
                    if (r != pg) break;
                    d += 0x4000;
                }
            }

            block += 0x4000;
            remaining -= 0x4000;
            if (remaining == 0)
                return;
        }
    }

    if (len == 0)
        return;
    {
        unsigned char *mem = bus->dtcm;
        int64_t descriptor = (int64_t)((uint64_t)mem - (uint64_t)base) >> 2;
        uint32_t o = 0;
        do {
            record(table, base + o, descriptor);
            o += 0x800;
        } while (len != o);
    }

    if (((base >> 26) & 0x3f) != 0)
        return;
    if ((len >> 14) == 0)
        return;
    {
        unsigned char *mirror = bus->address_window;
        uint64_t cap = ((uint64_t)(len >> 14) << 14) - 0x4000;
        unsigned char *start = mirror + base;
        uint64_t d = 0;
        for (;;) {
            unsigned char *pg = start + (uint32_t)d;
            unmap(pg, 0x4000);
            int fd = bus->shared_region_fd;
            void *r = map(pg, 0x4000, 3, 1, fd,
                             (long)(uint32_t)((uint32_t)d + BUS_SHARED_DTCM));
            if (cap == d) break;
            if (r != pg) break;
            d += 0x4000;
        }
    }
}

void pagetable_itcm_resize(bus_t *bus, uint32_t updated_size) {
    int (*unmap)(void *, uint64_t) = (int (*)(void *, uint64_t))sym_libc_munmap;
    void *(*map)(void *, uint64_t, int, int, int, long) =
        (void *(*)(void *, uint64_t, int, int, int, long))sym_libc_mmap;

    uint32_t old_size = bus->itcm_size;
    if (old_size == updated_size)
        return;

    unsigned char *table = (unsigned char *)bus->arm9_pagetable;

    if (old_size != 0) {
        unsigned char *special = (unsigned char *)PT(table)->bus;
        uint32_t p = 0;
        for (;;) {
            uint32_t page = p >> 11;
            unsigned char *read_base = 0, *write_base = 0;
            int formula = 0;

            if ((p >> 28) != 0) {

                if ((uint64_t)p + PAGETABLE_TOP_BLOCK_BYTES >= NDS_ADDRESS_SPACE_BYTES) {
                    unsigned char *q = (unsigned char *)PT(table)->cpu;
                    if (((arm_t *)q)->is_arm9 == 1) {
                        read_base = ((bus_t *)special)->bios9 + (p & 0x800);
                        write_base = 0;
                        formula = 1;
                    }
                }
            } else {
                bus_region_t *e = &PT(table)->region[p >> 23];
                if (e->read_kind == 0)
                    read_base = e->read_memory + (e->mask & p);
                if (e->write_kind == 0)
                    write_base = e->read_memory + (e->mask & p);
                formula = (read_base != 0);
            }

            uint64_t v;
            if (formula) {
                int64_t d = (int64_t)((uint64_t)read_base - (uint64_t)p) >> 2;
                v = (read_base == write_base) ? (uint64_t)d
                                     : ((uint64_t)d | 0x4000000000000000ULL);
            } else {
                v = 0x4000000000000000ULL;
            }
            *(uint64_t *)(table + (uint64_t)page * 8) = v;

            p += 0x800;
            if (old_size == p)
                break;
        }
    }

    bus->itcm_size = updated_size;

    if (updated_size <= 0x8000) {
        if (updated_size == 0)
            return;
        unsigned char *rom = bus->itcm;
        uint64_t o = 0;
        do {
            uint32_t i1 = (uint32_t)((o >> 21) & 0x7FF);
            uint32_t *c1 = (uint32_t *)((unsigned char *)&PT(table)->group_bits[i1]);
            uint32_t i2 = (uint32_t)((o >> 16) & 0xFFFF);
            *c1 |= 1u << (i2 & 31);
            uint32_t *c2 = (uint32_t *)((unsigned char *)&PT(table)->page_bits[i2]);
            uint32_t i3 = (uint32_t)((o >> 11) & 0x1FFFFF);
            int64_t v = (int64_t)((uint64_t)(rom + o) - (uint64_t)(uint32_t)o) >> 2;
            o += 0x800;
            *c2 |= 1u << (i3 & 31);
            *(uint64_t *)(table + (uint64_t)i3 * 8) = (uint64_t)v;
        } while (updated_size != (uint32_t)o);
        return;
    }

    {
        uint32_t remaining = updated_size;
        uint32_t offmap   = 0x408000;
        uint64_t basedir  = 0;
        uint32_t shift = 0xffff8000u;

        for (;;) {
            unsigned char *rom = bus->itcm;
            uint32_t low = (uint32_t)basedir;
            uint64_t o = 0;
            uint64_t page = (basedir >> 11) & 0x1FFFFF;
            uint64_t diff = (uint64_t)(-(int64_t)(basedir & 0xffffffffULL));

            do {
                uint32_t i1 = (low + (uint32_t)o) >> 21;
                uint32_t *c1 = (uint32_t *)((unsigned char *)&PT(table)->group_bits[i1]);
                uint32_t i2 = (low + (uint32_t)o) >> 16;
                *c1 |= 1u << (i2 & 31);
                uint32_t *c2 = (uint32_t *)((unsigned char *)&PT(table)->page_bits[i2]);
                int64_t v = (int64_t)(diff + (uint64_t)(rom + o)) >> 2;
                o += 0x800;
                *c2 |= 1u << ((uint32_t)page & 31);
                *(uint64_t *)(table + page * 8) = (uint64_t)v;
                page += 1;
                diff -= 0x800;
            } while ((uint32_t)o != 0x8000);

            if ((basedir >> 26) == 0) {
                uint32_t len = (basedir > 0x3ff8000ULL)
                               ? 0x4000000u - (uint32_t)basedir : 0x8000u;
                if (basedir == 0) {
                    uint32_t gap = 0x8000u - (uint32_t)basedir;
                    if (len > 0x8000 && len != gap) {
                        unsigned char *base = bus->address_window;
                        uint64_t left = ((uint64_t)(uint32_t)(len + shift) >> 14) - 1;
                        unsigned char *start = base + gap;
                        uint32_t d = 0;
                        for (;;) {
                            unsigned char *pg = start + (uint32_t)d;
                            unmap(pg, 0x4000);
                            int fd = bus->shared_region_fd;
                            void *r = map(pg, 0x4000, 3, 1, fd,
                                             (long)(uint32_t)(offmap + d));
                            if (left == 0) break;
                            left -= 1;
                            d += 0x4000;
                            if (r != pg) break;
                        }
                    }
                } else if (len != 0) {
                    unsigned char *base = bus->address_window;
                    uint64_t left = (uint64_t)(len >> 14) - 1;
                    unsigned char *start = base + basedir;
                    uint32_t off2 = 0x400000;
                    for (;;) {
                        unsigned char *pg = start + (uint32_t)(off2 - 0x400000);
                        unmap(pg, 0x4000);
                        int fd = bus->shared_region_fd;
                        void *r = map(pg, 0x4000, 3, 1, fd, (long)off2);
                        if (left == 0) break;
                        left -= 1;
                        off2 += 0x4000;
                        if (r != pg) break;
                    }
                }
            }

            basedir  += 0x8000;
            offmap   -= 0x8000;
            remaining -= 0x8000;
            shift += 0x8000;
            if (remaining == 0)
                return;
        }
    }
}

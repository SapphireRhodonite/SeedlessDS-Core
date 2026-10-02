#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "core/nds_state.h"
#include "core_internals.h"
#include <stdio.h>
#include "mem_access.h"

static uint8_t *base_page(uint64_t entry)
{
    return (uint8_t *)(uintptr_t)(entry << 2);
}
static uint64_t entry_page(const uint8_t *machine, uint32_t address)
{
    return ((const nds_t *)machine)->arm7.pagetable.page[address >> 11];
}









typedef int (*fn_fflush)(void *);

void cheats_run_codes(uint8_t *machine, cheat_entry_t *descriptor, uint32_t mask)
{

    uint32_t count = descriptor->code_words;
    uint32_t accumulated = 0;
    uint32_t value_current = 0;
    uint32_t history = UINT32_MAX;
    uint32_t notices = 0;
    uint32_t repeat = 0;
    uint32_t history_saved = UINT32_MAX;
    uint8_t *pc_saved;
    uint8_t *notifier = (uint8_t *)&((nds_t *)machine)->arm7;
    uint8_t *pages = (uint8_t *)&((nds_t *)machine)->arm7.pagetable;
    uint8_t *jit_block_ptr = (uint8_t *)&((nds_t *)machine)->arm7.jit_block;
    uint8_t *pc;
    uint8_t *end;

    if (count == 0)
        return;

    pc = (uint8_t *)descriptor->codes;
    end = pc + ((uint64_t)count << 2);
    pc_saved = pc;

    while (pc < end) {
        uint32_t op = rd32(pc);
        uint32_t value = rd32(pc + 4);
        uint32_t group;
        uint32_t subgroup;
        uint32_t address;
        uint32_t aligned;
        uint32_t prev_counter;
        uint32_t value_read;
        uint64_t page;
        uint8_t *base;

        pc += 8;
        group = op >> 28;
        address = op & 0x0fffffffu;

        switch (group) {
        case 0:
            address += accumulated;
            page = entry_page(machine, address);
            base = base_page(page);
            aligned = address & ~3u;
            if ((page & (UINT64_C(1) << 62)) == 0) {
                wr32(base + aligned, value);
            } else if ((page & (UINT64_C(1) << 63)) == 0) {
                bus_write32_slow(pages, aligned, value);
            } else if (rd32(base + aligned) != value) {
                if (jit_watch_write32(notifier, aligned) == 0)
                    notices++;
                wr32(base + aligned, value);
            }
            break;

        case 1:
            address += accumulated;
            page = entry_page(machine, address);
            base = base_page(page);
            aligned = address & ~1u;
            if ((page & (UINT64_C(1) << 62)) == 0) {
                wr16(base + aligned, (uint16_t)value);
            } else if ((page & (UINT64_C(1) << 63)) == 0) {
                bus_write16_slow(pages, aligned, value);
            } else if (rd16(base + aligned) != (uint16_t)value) {
                if (jit_watch_write16(notifier, aligned) == 0)
                    notices++;
                wr16(base + aligned, (uint16_t)value);
            }
            break;

        case 2:
            address += accumulated;
            page = entry_page(machine, address);
            base = base_page(page);
            if ((page & (UINT64_C(1) << 62)) == 0) {
                wr8(base + address, (uint8_t)value);
            } else if ((page & (UINT64_C(1) << 63)) == 0) {
                bus_write8_slow(pages, address, value);
            } else if (rd8(base + address) != (uint8_t)value) {
                if (jit_watch_write8(notifier, address) == 0)
                    notices++;
                wr8(base + address, (uint8_t)value);
            }
            break;

        case 3:
            value_read = bus_read32(pages,
                                         address == 0 ? accumulated : address);
            history = (history << 1) | (uint32_t)(value > value_read);
            break;

        case 4:
            value_read = bus_read32(pages,
                                         address == 0 ? accumulated : address);
            history = (history << 1) | (uint32_t)(value < value_read);
            break;

        case 5:
            value_read = bus_read32(pages,
                                         address == 0 ? accumulated : address);
            history = (history << 1) | (uint32_t)(value == value_read);
            break;

        case 6:
            value_read = bus_read32(pages,
                                         address == 0 ? accumulated : address);
            history = (history << 1) | (uint32_t)(value != value_read);
            break;

        case 7:
        case 8:
        case 9:
        case 10:
            value_read = bus_read16(pages,
                                         address == 0 ? accumulated : address);
            value_read &= ~(value >> 16);
            if (group == 7)
                value_read = (uint32_t)((uint16_t)value > (uint16_t)value_read);
            else if (group == 8)
                value_read = (uint32_t)((uint16_t)value < (uint16_t)value_read);
            else if (group == 9)
                value_read = (uint32_t)((uint16_t)value == (uint16_t)value_read);
            else
                value_read = (uint32_t)((uint16_t)value != (uint16_t)value_read);
            history = (history << 1) | value_read;
            break;

        case 11:
            accumulated = bus_read32(pages, address + accumulated);
            break;

        case 12:
            if ((op & 0x00ffffffu) != 0)
                break;
            subgroup = (op >> 24) & 15u;
            if (subgroup == 6) {
                uint32_t index_page = (value >> 8) & 0x00fffff8u;
                page = rd64(pages + index_page);
                base = base_page(page);
                aligned = value & ~3u;
                if ((page & (UINT64_C(1) << 62)) == 0) {
                    wr32(base + aligned, accumulated);
                } else if ((page & (UINT64_C(1) << 63)) == 0) {
                    bus_write32_slow(pages, aligned, accumulated);
                } else if (rd32(base + aligned) != accumulated) {
                    if (jit_watch_write32(notifier, aligned) == 0)
                        notices++;
                    wr32(base + aligned, accumulated);
                }
            } else if (subgroup == 5) {
                value_read = (value & mask) & 0xffffu;
                history = (history << 1) | (uint32_t)(value_read == (value >> 16));
            } else if (subgroup == 0) {
                repeat = value + 1;
                pc_saved = pc;
                history_saved = history;
            }
            break;

        case 13:
            if ((op & 0x00ffffffu) != 0)
                break;
            subgroup = (op >> 24) & 15u;
            if (subgroup > 12)
                break;

            switch (subgroup) {
            case 0:
                accumulated = value;
                break;

            case 1:
                if (value != 0)
                    break;
                repeat--;
                if (repeat == 0)
                    history = history_saved;
                else
                    pc = pc_saved;
                break;

            case 2:
                if (value != 0)
                    break;
                repeat--;
                if (repeat == 0) {
                    accumulated = 0;
                    value_current = 0;
                    history = UINT32_MAX;
                } else {
                    pc = pc_saved;
                }
                break;

            case 3:
                accumulated = value;
                break;

            case 4:
                value_current += value;
                break;

            case 5:
                value_current = value;
                break;

            case 6:
                address = value + accumulated;
                prev_counter = notices;
                page = entry_page(machine, address);
                base = base_page(page);
                aligned = address & ~3u;
                if ((page & (UINT64_C(1) << 62)) == 0) {
                    wr32(base + aligned, value_current);
                } else if ((page & (UINT64_C(1) << 63)) == 0) {
                    bus_write32_slow(pages, aligned, value_current);
                } else if (rd32(base + aligned) != value_current) {
                    if (jit_watch_write32(notifier, aligned) == 0)
                        prev_counter++;
                    notices = prev_counter;
                    wr32(base + aligned, value_current);
                }
                accumulated += 4;
                break;

            case 7:
                address = value + accumulated;
                prev_counter = notices;
                page = entry_page(machine, address);
                base = base_page(page);
                aligned = address & ~1u;
                if ((page & (UINT64_C(1) << 62)) == 0) {
                    wr16(base + aligned, (uint16_t)value_current);
                } else if ((page & (UINT64_C(1) << 63)) == 0) {
                    bus_write16_slow(pages, aligned, value_current);
                } else if (rd16(base + aligned) != (uint16_t)value_current) {
                    if (jit_watch_write16(notifier, aligned) == 0)
                        prev_counter++;
                    notices = prev_counter;
                    wr16(base + aligned, (uint16_t)value_current);
                }
                accumulated += 2;
                break;

            case 8:
                address = value + accumulated;
                page = entry_page(machine, address);
                base = base_page(page);
                if ((page & (UINT64_C(1) << 62)) == 0) {
                    wr8(base + address, (uint8_t)value_current);
                } else if ((page & (UINT64_C(1) << 63)) == 0) {
                    bus_write8_slow(pages, address, value_current);
                } else if (rd8(base + address) != (uint8_t)value_current) {
                    if (jit_watch_write8(notifier, address) == 0)
                        notices++;
                    wr8(base + address, (uint8_t)value_current);
                }
                accumulated++;
                break;

            case 9:
                value_current = bus_read32(pages, value + accumulated);
                break;

            case 10:
                value_current = bus_read16(pages, value + accumulated) &
                                0xffffu;
                break;

            case 11:
                value_current = bus_read8(pages, value + accumulated) &
                                0xffu;
                break;

            case 12:
                accumulated += value;
                break;
            }
            break;

        case 14:
            history >>= (value == 0);
            break;

        case 15:
            if ((history & 1u) != 0 && value != 0) {
                uint32_t dest = address + accumulated;
                uint32_t left = value;
                uint8_t *origin = pc;

                do {
                    uint8_t byte;

                    page = entry_page(machine, dest);
                    byte = rd8(origin);
                    base = base_page(page);
                    if ((page & (UINT64_C(1) << 62)) == 0) {
                        wr8(base + dest, byte);
                    } else if ((page & (UINT64_C(1) << 63)) == 0) {
                        bus_write8_slow(pages, dest, byte);
                    } else if (rd8(base + dest) != byte) {
                        if (jit_watch_write8(notifier, dest) == 0)
                            notices++;
                        wr8(base + dest, byte);
                    }
                    dest++;
                    origin++;
                } while (--left != 0);
            }
            pc += (uint64_t)(((value + 7u) >> 2) & 0x3ffffffeu) << 2;
            break;
        }
    }

    if (notices != 0) {
        ((fn_fflush)sym_libc_fflush)(stdout);
        jit_cache_flush(notifier, 0x02000000u);
        if (rd64(jit_block_ptr) != 0) {
            uint8_t *updated;

            jit_cache_refresh_block_info(notifier);
            updated = jit_cache_lookup_or_compile(notifier, rd32(jit_block_ptr + 292));
            wr_ptr(jit_block_ptr, updated + 8);
        }
    }
}
void cheats_apply_active(void *machine, cheats_t *jit_block_ptr, uint32_t mask)
{

    uint64_t idx;
    uint32_t type_prev;
    uint32_t blocked;

    if (jit_block_ptr->active_count == 0)
        return;

    idx = 0;
    type_prev = UINT32_MAX;
    blocked = 0;

    for (;;) {
        cheat_entry_t **table;
        cheat_entry_t *descriptor;
        uint32_t type;
        int deliver;

        table = jit_block_ptr->active;
        descriptor = table[idx];
        type = descriptor->folder;
        deliver = 0;

        if (type == CHEAT_FOLDER_NONE) {
            blocked = 0;
            deliver = 1;
        } else if (type != type_prev) {
            cheat_folder_t *types;

            types = jit_block_ptr->folders;
            type_prev = type;
            blocked = (uint32_t)(types[type].type == CHEAT_FOLDER_EXCLUSIVE);
            deliver = 1;
        } else if (blocked == 0) {
            deliver = 1;
        }

        if (deliver != 0)
            cheats_run_codes(machine, descriptor, mask);

        idx++;
        if (idx >= (uint64_t)jit_block_ptr->active_count)
            return;
    }
}

#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/data_paths.h"
#include "core/nds_state.h"
#include "dma.h"
#include "cart.h"
#include <string.h>
#include <stddef.h>
#include "seedlessds/platform.h"
#include "core_internals.h"
#include "mem_access.h"


uint32_t cart_romctrl_read(const bus_t *bus) {
    const unsigned char *p = (const unsigned char *)bus->machine;
    const cart_t *q = bus->cart;
    uint64_t base;
    memcpy(&base, p + 8, 8);
    uint32_t shift;
    memcpy(&shift, p + 16, 4);
    int32_t diff32;
    diff32 = ((nds_t *)p)->arm9.cycle_mark;
    int64_t diff = (int64_t)diff32;
    uint32_t ctrl = bus->io_mirror[0].cart.romctrl;
    uint64_t limit = q->transfer_deadline_cycles;
    uint64_t clock = base + (uint64_t)shift;
    clock = clock - (uint64_t)diff;
    uint32_t masked = ctrl & 0xdf7fffffu;
    uint32_t r = (clock < limit) ? masked : ctrl;
    return r | 0x20000000u;
}

uint32_t cart_queue_pop_pair(bus_t *bus) {

    uint32_t r1 = cart_slot2_queue_pop_2bit(&bus->cart->slot2.sensor);

    uint32_t r2 = cart_slot2_queue_pop_2bit(&bus->cart->slot2.sensor);

    uint32_t w20 = r1;
    uint32_t mask = 0xffffffu << 8;
    w20 = (w20 & ~mask) | ((r2 << 8) & mask);
    return w20;
}

static void mov16(unsigned char *d, const unsigned char *o) {
    for (int i = 0; i < 16; i++) d[i] = o[i];
}

void *cart_header_logo_copy(unsigned char *dest, const unsigned char *origin) {
    bus_t *bus = (bus_t *)dest;
    if ((bus->bios_flags & 2) == 0)
        return dest;

    mov16(bus->bios9 + 0x30, origin + 208);
    mov16(bus->bios9 + 0x20, origin + 192);
    mov16(bus->bios9 + 0x70, origin + 272);
    mov16(bus->bios9 + 0x60, origin + 256);
    mov16(bus->bios9 + 0x50, origin + 240);
    mov16(bus->bios9 + 0x40, origin + 224);
    mov16(bus->bios9 + 0xae, origin + 334);
    mov16(bus->bios9 + 0xa0, origin + 320);
    mov16(bus->bios9 + 0x90, origin + 304);
    mov16(bus->bios9 + 0x80, origin + 288);
    return dest;
}

void cart_transfer_irq_raise(void *unused, void *argument) {
    (void)unused;
    cart_t *cart = argument;
    unsigned char *ctx = (unsigned char *)cart->machine;
    unsigned char *sub = (unsigned char *)&((nds_t *)ctx)->arm9.io_mirror;

    cart->irq_pending = 0;

    unsigned char *regs = *(unsigned char **)sub;

    uint32_t requested_n = ((io_mirror_t *)regs)->irq.if_pending;
    requested_n |= 0x80000u;
    ((io_mirror_t *)regs)->irq.if_pending = requested_n;

    uint32_t state = sub[144];
    if ((state & 6u) != 0)
        return;

    uint32_t enabled = ((io_mirror_t *)regs)->irq.ie;
    uint32_t mask     = ((io_mirror_t *)regs)->irq.ime;
    uint32_t r = enabled & requested_n;
    r &= (uint32_t)(-(int32_t)mask);
    *(uint32_t *)(sub + 136) = r;
}


extern int str_vsprintf_caller_limit_3(char *dest, unsigned long size,
                              const char *format, ...);
extern int files_open_translate_flags(const char *path, int flags);
#define RAM_LINE   0x3e0000
#define RAM_ARGV    0x3ffe70
#define FMT_IMG     0x10ed7e
#define MSG_ERR     0x10ed93
#define FMT_FAT     0x10edb4
#define ARGV_HEAD  0x027e00005f617267ULL
#define PATH_N      0x420
typedef int    (*fn_close)(int);
typedef void   (*fn_perror)(const char *);
typedef size_t (*fn_strlen)(const char *);





uint64_t cart_homebrew_setup_argv(cart_t *cart)
{
    static fn_close  s_close;
    static fn_perror s_perror;
    static fn_strlen s_strlen;
    if (!s_close) {
        s_close  = (fn_close) sym_libc_close;
        s_perror = (fn_perror)sym_libc_perror;
        s_strlen = (fn_strlen)sym_libc_strlen;
    }
    nds_t *machine = cart->machine;
    int fd = cart->argv_fd;
    unsigned char *ram = machine->bus.main_ram;
    unsigned char *argv  = ram + RAM_ARGV;
    char          *line = (char *)(ram + RAM_LINE);
    if (fd >= 0)
        s_close(fd);
    {
        char path[PATH_N];
        str_vsprintf_caller_limit_3(path, PATH_N, DATA_DLDI_IMAGE_FMT,
                           (unsigned char *)((nds_t *)machine)->save_dir, '/');
        fd = nds_platform_default()->files.open_fd(nds_platform_default()->user, path, 2);
    }
    cart->argv_fd = fd;
    if (fd < 0)
        s_perror(DATA_DLDI_IMAGE_ERROR);
    str_vsprintf_caller_limit_3(line, (unsigned long)-1,
                       "fat:/%s", (unsigned char *)((nds_t *)machine)->rom_file_name);
    wr64(argv, ARGV_HEAD);
    {
        size_t len = s_strlen(line);
        wr32(argv + 8, (uint32_t)len + 1u);
        return (uint64_t)len;
    }
}
#undef RAM_LINE
#undef RAM_ARGV
#undef FMT_IMG
#undef MSG_ERR
#undef FMT_FAT
#undef ARGV_HEAD
#undef PATH_N

void cart_slot2_gpio_set_bit1(slot2_gpio_t *gpio, uint32_t v) {
    uint32_t mark = gpio->bit1;
    uint32_t bit = (v >> 1) & 1u;
    if (bit != mark) {
        const unsigned char *p = (const unsigned char *)gpio->config;
        uint32_t w;
        memcpy(&w, p + 1216, 4);
        gpio->countdown = (unsigned char)w;
    }
    gpio->bit1 = (unsigned char)bit;
}

int cart_slot2_gpio_countdown_tick(slot2_gpio_t *gpio) {
    uint8_t v = *(volatile uint8_t *)&gpio->countdown;
    if (v == 0) return 0;
    *(volatile uint8_t *)&gpio->countdown = (uint8_t)(v - 1);
    return 1;
}

void cart_slot2_device_attach(slot2_gpio_t *gpio, unsigned long long v) {
    __builtin_memcpy(&gpio->config, &v, sizeof v);
}

void cart_slot2_gpio_reset_flag_and_counter(slot2_gpio_t *gpio)
{
    gpio->bit1 = 0;
    gpio->countdown = 0;
}

uint32_t cart_slot2_queue_pop_2bit(slot2_sensor_t *sensor) {
    uint32_t i = sensor->phase;
    uint32_t v = 0;
    if (i != 0) {
        uint32_t k = i - 1;
        uint32_t by = (&sensor->index)[k >> 2];
        uint32_t sh = 6u & ~(k << 1);
        v = (by >> (sh & 31u)) & 3u;
    }
    uint32_t sig = i + 1;
    sensor->phase = (unsigned char)((sig == 0x11u) ? 0u : sig);
    return v;
}

static uint32_t fcvtzu_f32(float f)
{
    double d = (double)f;
    if (d != d) return 0u;
    if (d < 0.0) return 0u;
    if (d >= 4294967296.0) return 0xFFFFFFFFu;
    return (uint32_t)d;
}

void cart_sensor_write_accel_axes_8bit(float param_1, float param_2, float param_3,
                         slot2_sensor_t *sensor)
{
    union { uint32_t u; float f; } kPos, kNeg, k51, k128;
    kPos.u = 0x3dd0d67fu;
    kNeg.u = 0xbdd0d67fu;
    k51.u  = 0x424c0000u;
    k128.u = 0x43000000u;
    uint32_t threshold = 254u;
    uint32_t w8;

    float s0 = param_1 * kPos.f;
    s0 = s0 * k51.f;
    s0 = s0 + k128.f;
    w8 = fcvtzu_f32(s0);
    if (!(w8 < threshold)) w8 = threshold;
    sensor->accel8_x = (unsigned char)w8;

    float s1 = param_2 * kNeg.f;
    s1 = s1 * k51.f;
    s1 = s1 + k128.f;
    w8 = fcvtzu_f32(s1);
    if (!(w8 < threshold)) w8 = threshold;
    sensor->accel8_y = (unsigned char)w8;

    float s2 = param_3 * kNeg.f;
    s2 = s2 * k51.f;
    s2 = s2 + k128.f;
    w8 = fcvtzu_f32(s2);
    if (!(w8 < threshold)) w8 = threshold;
    sensor->accel8_z = (unsigned char)w8;
}

void cart_slot2_descriptor_invalidate(slot2_sensor_t *sensor) {
    sensor->index = 0xff;
}

void cart_sensor_reset_accel_axes_8bit(slot2_sensor_t *sensor) {
    sensor->phase = 0;
    sensor->accel8_x = 0x80;
    sensor->accel8_y = 0x80;
    sensor->accel8_z = 0x80;
}

uint32_t cart_slot2_descriptor_next_field(slot2_sensor_t *sensor, uint32_t value) {
    uint32_t counter = sensor->phase16;
    uint32_t ret;
    if (counter == 0) {
        sensor->index16 = (unsigned char)value;
        ret = 0xff;
    } else {
        uint32_t b1 = sensor->index16;
        ret = 0xff;
        if ((b1 - 2u) <= 8u && (b1 & 1u) == 0) {
            uint32_t k = (b1 >> 1) - 1u;
            uint16_t h = (&sensor->accel16_x)[k];
            uint32_t count = 16u - (counter << 3);
            ret = (uint32_t)h >> (count & 31u);
        }
    }
    uint32_t sig = counter + 1u;
    sensor->phase16 = (unsigned char)((sig == 3u) ? 0u : counter + 1u);
    return ret;
}

static uint32_t fcvtzu_f32_16(float f)
{
    double d = (double)f;
    if (d != d) return 0u;
    if (d < 0.0) return 0u;
    if (d >= 4294967296.0) return 0xFFFFFFFFu;
    return (uint32_t)d;
}

void cart_sensor_write_accel_axes_16bit(float param_1, float param_2, float param_3,
                         slot2_sensor_t *sensor)
{
    union { uint32_t u; float f; } c1, two_pow_17, ten_thousand, two_pow_15;
    c1.u      = 0x42cbdfa4u;
    two_pow_17.u = 0x48000000u;
    ten_thousand.u  = 0x461c4000u;
    two_pow_15.u  = 0x47000000u;
    uint32_t threshold = 0xffffu;
    uint32_t w;

    float s0 = param_1 * c1.f;
    s0 = s0 * two_pow_17.f;
    s0 = s0 / ten_thousand.f;
    s0 = s0 + two_pow_15.f;
    w = fcvtzu_f32_16(s0);
    if (!(w < threshold)) w = threshold;
    sensor->accel16_x = (uint16_t)w;

    float s1 = param_2 * c1.f;
    s1 = s1 * two_pow_17.f;
    s1 = s1 / ten_thousand.f;
    s1 = s1 + two_pow_15.f;
    w = fcvtzu_f32_16(s1);
    if (!(w < threshold)) w = threshold;
    sensor->accel16_y = (uint16_t)w;

    float s2 = param_3 * c1.f;
    s2 = s2 * two_pow_17.f;
    s2 = s2 / ten_thousand.f;
    s2 = s2 + two_pow_15.f;
    w = fcvtzu_f32_16(s2);
    if (!(w < threshold)) w = threshold;
    sensor->accel16_z = (uint16_t)w;
}

static uint32_t fcvtzu_f32_17(float f)
{
    double d = (double)f;
    if (d != d) return 0u;
    if (d < 0.0) return 0u;
    if (d >= 4294967296.0) return 0xFFFFFFFFu;
    return (uint32_t)d;
}

void cart_sensor_write_gyro_16bit(float param_1, slot2_sensor_t *sensor)
{
    union { uint32_t u; float f; } kA, kB, kC, kD;
    kA.u = 0xc2652ee6u;
    kB.u = 0x464e4000u;
    kC.u = 0x447a0000u;
    kD.u = 0x46d20000u;
    float s0 = param_1;
    s0 = s0 * kA.f;
    s0 = s0 * kB.f;
    s0 = s0 / kC.f;
    s0 = s0 + kD.f;
    uint32_t w8 = fcvtzu_f32_17(s0);
    uint32_t w9 = 0xffffu;
    if (!(w8 < w9)) w8 = w9;
    sensor->gyro16 = (uint16_t)w8;
}

void cart_sensor_write_ident_word(slot2_sensor_t *sensor)
{
    sensor->ident = UINT16_C(0xf00f);
}

void cart_sensor_reset_accel_axes_16bit(slot2_sensor_t *sensor)
{
    sensor->phase16 = 0;
    sensor->index16 = 0;
    sensor->accel16_x = 0x8000;
    sensor->accel16_y = 0x8000;
    sensor->accel16_z = 0x8000;
    sensor->gyro16 = 0x6900;
}

extern uint32_t mirror_write_block_via_regions(unsigned char *table, void *ctx, uint32_t dir,
                                   unsigned char *origin, uint32_t remaining);
extern void    *cart_header_logo_copy_20(unsigned char *dest, const unsigned char *origin) __asm__("cart_header_logo_copy");
extern int32_t  cart_slot2_load_gba(cart_t *p, const char *name_str);
extern int      str_vsnprintf_limited_swapped_args(char *dest, size_t slen, size_t maxlen,
                                   const char *format, ...);
extern uint32_t util_crc32_buffer_padded(const unsigned char *data, uint32_t len);
#define C_TABLE    0x484
#define S_SAV      0x10ede4
#define S_DSV      0x10edf7
#define S_SLOT2    0x10ee0a
#define S_SRAM     0x10ee18
#define PASS       0x53534150u
#define EMPTY      0xe7ffdeffu
#define HEADER        0x200
#define PATH       0x820
typedef void    *(*fn_malloc)(uint64_t);
typedef void     (*fn_chk)(char *, const char *, uint64_t, uint64_t, uint64_t);
typedef int      (*fn_cmp)(const void *, const void *, uint64_t);






uint64_t cart_boot_from_header(cart_t *ctx) {
    uint8_t *mod = (uint8_t *)ctx->machine;
    rom_image_t *cfg = ctx->rom;
    uint8_t *pram = (uint8_t *)&ctx->machine->bus;
    const uint8_t *rom = cfg->data;
    uint32_t size = cfg->size;
    uint32_t type = ((nds_t *)mod)->config.slot2_type;
    nds_config_t *marks = &((nds_t *)mod)->config;
    uint8_t h[HEADER];
    char title[16];
    char path[PATH];

    memcpy(h, rom, HEADER);
    ((fn_chk)fortify_strncpy)(title, (const char *)h, 0xc, 0xd, HEADER);
    title[12] = 0;

    uint32_t code = rd32(h + 12);
    ctx->game_code = code;
    uint32_t pot = 0x20000u << (h[20] & 0x1fu);
    uint32_t m = pot - 1u;
    ctx->rom_mask = m;
    if (pot < size) {
        do { m = (m << 1) | 1u; } while ((m + 1u) < size);
        ctx->rom_mask = m;
    }
    if (marks->ignore_gamecard_limit != 0) {
        uint32_t v = size + 1u;
        uint32_t c = 0;
        while (c < 32 && !(v & (0x80000000u >> c))) c++;
        uint32_t s = (uint32_t)(0u - c) & 31u;
        ctx->rom_mask = ~(0xffffffffu << s);
    }
    if (code == 0x23232323u) cart_homebrew_setup_argv(ctx);
    else ctx->argv_fd = -1;
    ctx->write_words_remaining = 0;
    uint32_t crc = util_crc32_buffer_padded(h, HEADER);

    if (marks->boot_from_firmware != 0) {

        ((nds_t *)mod)->arm9.pc = 0xffff0000u;
        ((nds_t *)mod)->arm7.pc = 0;
        memcpy(ctx->secure_area, cfg->data + NDS_ROM_SECURE_AREA_OFFSET, CART_SECURE_AREA_SIZE);
        if (rd32(ctx->secure_area) == EMPTY && rd32(ctx->secure_area + 4) == EMPTY) {
            if (((nds_t *)mod)->bus.bios_flags & 1) ctx->secure_area_pending = 1;
            else cart_key1_encrypt_secure_area(ctx, ctx->secure_area, 0, ((nds_t *)mod)->bus.bios7 + 0x30);
        }
    } else {

        uint32_t a9_off = rd32(h + 32), a9_ent = rd32(h + 36);
        uint32_t a9_ram = rd32(h + 40), a9_size = rd32(h + 44);
        uint32_t a7_off = rd32(h + 48), a7_ent = rd32(h + 52);
        uint32_t a7_ram = rd32(h + 56), a7_size = rd32(h + 60);

        mirror_write_block_via_regions((unsigned char *)((nds_t *)mod)->bus.arm9_pagetable->region,
                            pram, a9_ram, (unsigned char *)(rom + a9_off), a9_size);
        mirror_write_block_via_regions((unsigned char *)((nds_t *)mod)->bus.arm7_pagetable->region,
                            pram, a7_ram, (unsigned char *)(rom + a7_off), a7_size);

        uint8_t *ram = (uint8_t *)rd64(pram);
        wr16(ram + NDS_MAIN_RAM_CHIP_ID + 8, rd16(h + 0x15e));
        memcpy(ram + NDS_MAIN_RAM_HEADER, h, 0x170);
        ((nds_t *)mod)->arm9.pc = a9_ent;
        ((nds_t *)mod)->arm7.pc = a7_ent;
        ctx->secure_area_pending = 0;
        if (a9_off == 0x4000u && (a9_ram & 0xff000000u) == 0x2000000u) {
            const uint8_t *q = ram + (a9_ram & 0x3fffffu);
            if (rd32(q) != EMPTY || rd32(q + 4) != EMPTY) {
                uint32_t b = ctx->game_code;
                uint32_t p = rd32(ctx->secure_area);
                uint32_t s = rd32(ctx->secure_area + 4);
                int exc =
                    (b == 0x45355659u && p == 0x0014a191au && s == 0xa5c470b9u) ||
                    (b == 0x50355659u && p == 0xd0d48b67u  && s == 0x39392f23u) ||
                    (b == 0x4a355659u && p == 0x7829bc8du  && s == 0x9968ef44u);
                if (!exc) {
                    if (((nds_t *)mod)->bus.bios_flags & 1) ctx->secure_area_pending = 1;
                    else cart_key1_decrypt_secure_area(ctx, 0, 0, ((nds_t *)mod)->bus.bios7 + 0x30);
                }
            }
        }
    }

    uint32_t dsv = (marks->raw_sav_format == 0);

    str_vsnprintf_limited_swapped_args(path, PATH, PATH,
                        dsv ? "%s%cbackup%c%s.dsv" : "%s%cbackup%c%s.sav",
                        ((nds_t *)mod)->save_dir, '/', '/', ((nds_t *)mod)->rom_name);

    spi_memory_t *stream = &ctx->backup;
    game_database_entry_t *ent = 0;
    if (ctx->use_user_database != 0) ent = (game_database_entry_t *)db_find_cartridge_entry(&ctx->database[1], code, title);
    if (ent == 0)     ent = (game_database_entry_t *)db_find_cartridge_entry(&ctx->database[0], code, title);
    if (ent == 0) {
        uint32_t k = util_crc32_buffer_padded(rom, cfg->size);
        ent = (game_database_entry_t *)util_bsearch_key_by_id(&ctx->database[0], k);
        if (ent == 0) {
            void *b = ((fn_malloc)sym_libc_malloc)(0x80000);
            ctx->backup_data = (uint8_t *)b;
            spi_memory_mount(stream, 1, (unsigned char *)b, 0x80000, path, 0, dsv);
            wr32(ctx->backup.jedec_id, 0x204013);
            goto follows;
        }
    }
    {
        uint32_t class = ent->save_type;
        if (class > 3) goto follows;
        uint32_t n = ent->save_size;
        void *b;
        switch (class) {
        case 1:
            b = ((fn_malloc)sym_libc_malloc)(n);
            ctx->backup_data = (uint8_t *)b;
            spi_memory_mount(stream, 1, (unsigned char *)b, n, path, 0, dsv);
            wr32(ctx->backup.jedec_id, ent->save_id);
            break;
        case 2:
            b = ((fn_malloc)sym_libc_malloc)(n);
            ctx->backup_data = (uint8_t *)b;
            spi_memory_mount(stream, 2, (unsigned char *)b, n, path, 0, dsv);
            break;
        case 3:
            ctx->backup_write_mode = 0;
            ctx->backup_write_enabled = 0;
            ctx->backup_base = (uint32_t)rd16(h + 0x96) << 17;
            b = ((fn_malloc)sym_libc_malloc)(n);
            ctx->backup_data = (uint8_t *)b;
            spi_memory_mount(stream, 3, (unsigned char *)b, n, path, 0, dsv);
            break;
        default:
            b = ((fn_malloc)sym_libc_malloc)(0x80000);
            ctx->backup_data = (uint8_t *)b;
            spi_memory_mount(stream, 1, (unsigned char *)b, 0x80000, path, 0, dsv);
            wr32(ctx->backup.jedec_id, 0x204013);
            break;
        }
    }

follows:
    cheats_load_user_file(&ctx->cheats, mod);
    if (rd32((uint8_t *)ctx + C_TABLE) != 0)
        cheats_load_entry_by_key(&ctx->cheats, code, ~crc);
    {
        uint32_t v = ctx->rom_mask + 1u;
        v = (v >> 12) & CART_CHIP_ID_SIZE_FIELD;
        v = (v + CART_CHIP_ID_SIZE_FIELD) & CART_CHIP_ID_SIZE_FIELD;
        v |= 0xc2u;
        ctx->chip_id = v;
        if (marks->boot_from_firmware == 0) {
            wr32((uint8_t *)rd64(pram) + NDS_MAIN_RAM_CHIP_ID, v);
            wr32((uint8_t *)rd64(pram) + NDS_MAIN_RAM_CHIP_ID + 4, ctx->chip_id);
            wr32((uint8_t *)rd64(pram) + NDS_MAIN_RAM_CHIP_ID_MIRROR, ctx->chip_id);
            wr16((uint8_t *)rd64(pram) + NDS_MAIN_RAM_CHIP_ID + 8, rd16(h + 0x1fe));
            wr16(&((nds_t *)mod)->bus.io_mirror[1].postflg, 1);
            wr16(&((nds_t *)mod)->bus.io_mirror[0].postflg, 1);
            cart_header_logo_copy(pram, rom);
        }
    }
    uint32_t r = type;
    if (r <= 1) {
        if (cart_slot2_load_gba(ctx, ((nds_t *)mod)->rom_name) == -1) {
            r = (cart_slot2_load_gba(ctx, "slot2_gamepak") != -1);
            if (ctx->slot2.loaded != 0) goto finish;
        } else {
            r = 1;
            if (ctx->slot2.loaded != 0) goto finish;
        }
    } else {
        if (ctx->slot2.loaded != 0) goto finish;
    }
    {
        int signature = 0;
        if (((fn_cmp)sym_libc_memcmp)(h + 0xa0, "SRAM_V110", 9) == 0)
            signature = (rd32(h + 0xac) == PASS);
        if (r != 2 && !(signature && r == 0)) goto finish;
    }

    if (size > 0x2000000u) { r = 0; goto end; }
    if (bus_regions_alloc(pram) == -1) { r = 0; goto end; }
    memcpy(((nds_t *)mod)->bus.slot2_rom, rom, size);
    r = 2;
    goto end;

finish:
    if (r == 5) bus_regions_init(pram);
    else if (r == 4) cart_slot2_map_region_both_cpus(pram);
    else if (r == 3) cart_slot2_map_gpio_region_arm9(pram);

end:
    marks->slot2_type = r;
    return 0;
}
#undef C_TABLE
#undef S_SAV
#undef S_DSV
#undef S_SLOT2
#undef S_SRAM
#undef PASS
#undef EMPTY
#undef HEADER
#undef PATH

int cart_open_rom(cart_t *cart, char *path) {

    char *(*last)(const char *, int) = (char *(*)(const char *, int))sym_libc_strrchr;
    char *(*last_chk)(const char *, int, uint64_t) =
        (char *(*)(const char *, int, uint64_t))fortify_strrchr;
    char *(*copies)(char *, const char *, uint64_t) =
        (char *(*)(char *, const char *, uint64_t))sym_libc_strncpy;
    void *(*copy)(void *, const void *, uint64_t) =
        (void *(*)(void *, const void *, uint64_t))sym_libc_memcpy;
    char *(*getcwd_fn)(char *, uint64_t) = (char *(*)(char *, uint64_t))sym_libc_getcwd;
    void (*notify)(const char *) = script_invoke_on_load;

    unsigned char *base = (unsigned char *)cart->machine;
    char *cfg_txt = ((nds_t *)base)->cache_dir;
    nds_config_t *cfg = &((nds_t *)base)->config;

    char buf[0x820];
    char name_str[0x420];

    str_vsprintf_caller_limit_3(buf, 2080, "%s%cunzip_cache", cfg_txt, '/');

    char *first = (cfg->rom_in_cache_dir == 0) ? 0 : buf;
    if (cart->rom != 0)
        nds_close_files(cart);

    void *f = archive_open_rom(path, first, cfg->auto_trim,
                    cfg->preload_roms);
    if (f == 0) {
        if (cfg->rom_in_cache_dir != 0)
            return -1;
        f = archive_open_rom(path, buf, cfg->auto_trim,
                  cfg->preload_roms);
        if (f == 0)
            return -1;
    }

    cart->rom = (rom_image_t *)f;
    if (cart->rom->size < 0x200)
        return -1;

    {
        char *bar = last(path, '/');
        copies(name_str, bar == 0 ? path : bar + 1, 1024);
        char *point = last_chk(name_str, '.', 0x420);
        if (point != 0) *point = 0;
    }

    copies(((nds_t *)base)->rom_path, path, 1024);
    ((nds_t *)base)->rom_path[NDS_PATH_SIZE - 1] = 0;

    char *name = ((nds_t *)base)->rom_name;
    {
        char *bar = last(path, '/');
        copies(name, bar == 0 ? path : bar + 1, 1024);
    }
    ((nds_t *)base)->rom_name[NDS_PATH_SIZE - 1] = 0;
    copy(((nds_t *)base)->rom_file_name, name, 1024);
    {
        char *point = last_chk(name, '.', 1024);
        if (point != 0) *point = 0;
    }

    if (getcwd_fn(((nds_t *)base)->config.working_dir, 1024) == 0)
        return -1;

    if (cfg->lua_enabled == 0)
        return 0;

    str_vsprintf_caller_limit_3(buf, 2080, "%s%c%s%c%s.%s", cfg_txt, '/',
             "scripts", '/', name,
             "lua");

    if (script_run_file(buf) != 0) {
        str_vsprintf_caller_limit_3(buf, 2080, "%s%c%s%cdefault.%s", cfg_txt, '/',
                 "scripts", '/', "lua");
        if (script_run_file(buf) != 0)
            return 0;
    }

    notify(name);
    return 0;
}

#define COMMANDS    0xd7
typedef long (*fn_lseek)(int, long, int);
extern void cart_key1_apply_keycode(cart_t *obj);




static uint32_t bswap32(uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0xff00u) | ((v << 8) & 0xff0000u) | (v << 24);
}

static void notify(uint8_t *machine, uint32_t bit) {
    uint8_t *r = (uint8_t *)rd64(machine);
    uint32_t v = ((io_mirror_t *)r)->irq.if_pending | bit;
    ((io_mirror_t *)r)->irq.if_pending = (v);
    uint8_t *pend = machine + 136;
    if (!(machine[144] & 6)) {
        uint32_t n = (((io_mirror_t *)r)->irq.ie & v) & (0u - ((io_mirror_t *)r)->irq.ime);
        wr32(pend, n);
        if (n == 0) return;
    } else {
        if (rd32(pend) == 0) return;
    }
    wr32(pend + 416, rd32(pend + 416) | 2u);
}

void cart_command_execute(cart_t *ctx, uint32_t arg) {
    io_mirror_t *cart = ctx->mirror;
    dma_t *sched = ctx->dma9;
    uint32_t n = (arg >> 24) & 7u;
    uint32_t block, base;
    if (n == 0) {
        block = 0;
        base = arg & 0x7fffffffu;
    } else {
        block = (n == 7) ? 1u : (0x40u << n);
        base = arg | 0x80000000u;
    }
    ctx->words_remaining = block;
    uint32_t class;
    if (cart->exmemcnt & 0x800) {
        sched = ctx->dma7;
        class = 2;
    } else {
        class = 5;
    }
    uint8_t stack[8];
    const uint8_t *order;
    if (ctx->key1_active == 0) {
        order = cart->cart.command;
    } else {
        const uint32_t *k = ctx->key1_table;
        uint32_t l = bswap32(rd32(cart->cart.command));
        uint32_t r = bswap32(rd32(cart->cart.command + 4));
        uint32_t x = 0;
        for (int i = 0; i < 16; i++) {
            x = k[17 - i] ^ l;
            uint32_t f = k[(x >> 24) + 0x12]
                       + k[((x >> 16) & 0xff) + 0x112];
            f ^= k[((x >> 8) & 0xff) + 0x212];
            f += k[(x & 0xff) + 0x312];
            l = f ^ r;
            r = x;
        }
        uint32_t a = k[0] ^ x;
        uint32_t b = k[1] ^ l;
        stack[0] = (uint8_t)(a >> 24); stack[1] = (uint8_t)(a >> 16);
        stack[2] = (uint8_t)(a >> 8);  stack[3] = (uint8_t)a;
        stack[4] = (uint8_t)(b >> 24); stack[5] = (uint8_t)(b >> 16);
        stack[6] = (uint8_t)(b >> 8);  stack[7] = (uint8_t)b;
        order = stack;
    }
    uint32_t mark = base | 0x800000u;
    uint32_t op = order[0];
    if (op < COMMANDS) switch (op) {
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15:
    case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b:
    case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x90: case 0xb8:
        ctx->transfer_mode = 1;
        if (ctx->argv_fd < 0) ctx->data_word = ctx->chip_id;
        else                  ctx->data_word = 0xfc2;
        break;
    case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5:
    case 0xa6: case 0xa7: case 0xa8: case 0xa9: case 0xaa: case 0xab:
    case 0xac: case 0xad: case 0xae: case 0xaf:
        ctx->transfer_mode = 0;
        ctx->data_word = 0xffffffffu;
        ctx->key1_active = 0;
        break;
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25:
    case 0x26: case 0x27: case 0x28: case 0x29: case 0x2a: case 0x2b:
    case 0x2c: case 0x2d: case 0x2e: case 0x2f:
        ctx->words_remaining = block;
        ctx->read_address = (uint32_t)(order[2] & 0xf0u) << 8;
        ctx->transfer_mode = 8;
        break;
    case 0xb9: case 0xba: case 0xbb: case 0xbc: {
        int32_t fd = ctx->argv_fd;
        if (fd < 0) break;
        long off = ((long)order[1] << 24) | ((long)order[2] << 16)
                 | ((long)order[3] << 8) | (long)order[4];
        ((fn_lseek)sym_libc_lseek)(fd, off, 0);
        if (order[0] == 0xba) {
            ctx->transfer_mode = 4;
            ctx->words_remaining = 0x80;
        } else {
            ctx->data_word = 0;
            ctx->transfer_mode = 5;
            ctx->words_remaining = 1;
            if (order[0] == 0xbb) ctx->write_words_remaining = 0x80;
        }
        break;
    }
    case 0x8b:  ctx->backup_write_mode = 0; mark &= 0x7fffffffu; break;
    case 0x84:  ctx->backup_write_enabled = 0; mark &= 0x7fffffffu; break;
    case 0x85:  ctx->backup_write_enabled = 1; mark &= 0x7fffffffu; break;
    case 0x82:  mark &= 0x7fffffffu; break;
    case 0x94:
        ctx->data_word = 1;
        ctx->transfer_mode = 7;
        ctx->words_remaining = 1;
        break;
    case 0x9f:
        ctx->transfer_mode = 0;
        ctx->data_word = 0xffffffffu;
        break;
    case 0xb2:
        if (ctx->backup.type != 3) break;
        {
            uint32_t v = bswap32(rd32(order + 1));
            ctx->backup_address = v;
            spi_memory_address_wrap(&ctx->backup, v - ctx->backup_base);
            ctx->backup_write_mode = 1;
            mark &= 0x7fffffffu;
        }
        break;
    case 0xb7: {
        uint32_t v = bswap32(rd32(order + 1));
        if (ctx->backup_write_mode == 0) {
            uint32_t w = ctx->rom_mask & v;
            uint32_t r = (0x8000u & ~0x1ffu) | (w & 0x1ffu);
            ctx->words_remaining = block;
            if (w >= 0x8000u) r = w;
            ctx->read_address = r;
            ctx->transfer_mode = 2;
            if (r >= ctx->rom->secure_limit) {
                ctx->transfer_mode = 3;
                ctx->data_word = 0xff;
            }
        } else {
            ctx->transfer_mode = 6;
            ctx->words_remaining = 0x80;
            spi_memory_address_wrap(&ctx->backup, v - ctx->backup_base);
        }
        break;
    }
    case 0x81:
        if (ctx->backup.type != 3) break;
        {
            uint32_t v = bswap32(rd32(order + 1)) - ctx->backup_base;
            if (v == ctx->backup_address) { mark &= 0x7fffffffu; break; }
            ctx->backup_address = v;
            spi_memory_address_wrap(&ctx->backup, v);
            ctx->backup_write_mode = 1;
            mark &= 0x7fffffffu;
        }
        break;
    case 0xb0:
        if (ctx->argv_fd < 0) break;
        ctx->data_word = 0x1f4;
        ctx->words_remaining = 1;
        ctx->transfer_mode = 1;
        break;
    case 0xd6: {
        if (ctx->backup.type != 3) break;
        uint32_t v = (ctx->backup_write_mode == 0) ? 0x60606060u : 0x20202020u;
        ctx->data_word = v;
        if (ctx->backup_write_enabled != 0) ctx->data_word = v | 0x10101010u;
        ctx->transfer_mode = 7;
        ctx->words_remaining = 1;
        break;
    }
    case 0x00:
        ctx->read_address = 0;
        ctx->transfer_mode = 2;
        break;
    case 0x3c: {
        ctx->key1_active = 1;
        uint8_t *mod = (uint8_t *)ctx->machine;
        ctx->key1_id1 = ((uint32_t)order[1] << 16)
                      | ((uint32_t)order[2] << 8) | order[3];
        ctx->key1_id2 = (((uint32_t)order[4] & 0xfu) << 16)
                      | ((uint32_t)order[5] << 8) | order[6];
        ctx->transfer_mode = 0;
        ctx->data_word = 0xffffffffu;
        if (((nds_t *)mod)->bus.bios_flags & 1) { ctx->secure_area_pending = 1; break; }
        {
            uint32_t s = ctx->game_code;
            ctx->keycode[0] = s;
            ctx->keycode[1] = s >> 1;
            ctx->keycode[2] = s << 1;
            memcpy(ctx->key1_table, ((nds_t *)mod)->bus.bios7 + 0x30, sizeof ctx->key1_table);
            cart_key1_apply_keycode(ctx);
            cart_key1_apply_keycode(ctx);
            ctx->keycode[1] = ctx->keycode[1] << 1;
            ctx->keycode[2] = ctx->keycode[2] >> 1;
        }
        break;
    }
    default:
        break;
    }
    cart->cart.romctrl = mark;
    uint8_t *mod = (uint8_t *)ctx->machine;
    uint32_t pair = ((nds_t *)mod)->runtime.cart_cycles_per_word;
    if (pair == 0) {
        ctx->transfer_deadline_cycles = 0;
    } else {
        uint64_t t = rd64(mod + 8) + rd32(mod + 16);
        int32_t d = (int32_t)((nds_t *)mod)->arm9.cycle_mark;
        uint32_t k = pair * ctx->words_remaining;
        ctx->transfer_deadline_cycles = (t - (uint64_t)(int64_t)d) + ((k + k * 4u) << 3);
    }
    for (uint32_t i = 0; i < 4; i++) {
        dma_channel_t *channel = &sched->channels[i];
        if ((int32_t)channel->cnt >= 0) continue;
        if (class != channel->start_mode) continue;
        dma_channel_finish_transfer((void *const *)(sched), channel, ctx);
    }
    if (block != 0) return;
    if (!(cart->cart.auxspicnt & 0x4000)) return;
    notify((uint8_t *)&ctx->machine->arm9.io_mirror, 0x80000);
    notify((uint8_t *)&ctx->machine->arm7.io_mirror, 0x80000);
}
#undef COMMANDS

uint32_t cart_data_in_next_word(cart_t *obj) {
    if (obj->words_remaining == 0) return 0;
    uint32_t mode = obj->transfer_mode;
    uint32_t pal;
    uint32_t left;
    switch (mode) {
    case 2: {
        uint32_t i = obj->read_address;
        pal = *(const uint32_t *)(obj->rom->data + i);
        left = obj->words_remaining - 1;
        obj->read_address = i + 4;
        obj->words_remaining = left;
        break;
    }
    case 4:
        ((long (*)(int, void *, unsigned long))sym_libc_read)(
            obj->argv_fd, &pal, 4);
        left = obj->words_remaining - 1;
        obj->words_remaining = left;
        break;
    case 6:
        pal = spi_memory_read_word(&obj->backup);
        left = obj->words_remaining - 1;
        obj->words_remaining = left;
        break;
    case 8: {
        uint32_t i = obj->read_address;
        pal = *(uint32_t *)(obj->secure_area + (uint32_t)(i - 0x4000));
        obj->read_address = i + 4;
        left = obj->words_remaining - 1;
        obj->words_remaining = left;
        break;
    }
    default:
        pal = obj->data_word;
        left = obj->words_remaining - 1;
        obj->words_remaining = left;
        break;
    }
    if (left != 0) return pal;
    io_mirror_t *c = obj->mirror;
    c->cart.romctrl &= 0x7fffffffu;
    if (obj->transfer_mode != 7 && obj->backup_write_mode == 0) {
        c->cart.romctrl &= 0xff7fffffu;
    }
    if (!(c->cart.auxspicnt & 0x4000)) return pal;
    unsigned char *m = (unsigned char *)obj->machine;
    arm_t *arm9 = &((nds_t *)m)->arm9;
    arm_t *arm7 = &((nds_t *)m)->arm7;
    unsigned char *reg0 = (unsigned char *)arm9->io_mirror;
    uint32_t pend0 = ((io_mirror_t *)reg0)->irq.if_pending | 0x80000u;
    ((io_mirror_t *)reg0)->irq.if_pending = pend0;
    uint32_t tap0 = (uint8_t)arm9->halt_flags;
    if (!(tap0 & 6)) {
        uint32_t v = (((io_mirror_t *)reg0)->irq.ie & pend0)
                   & (uint32_t)(-(int32_t)((io_mirror_t *)reg0)->irq.ime);
        arm9->irq_pending = v;
        if (v != 0) arm9->wake_flags |= 2u;
    } else if (arm9->irq_pending != 0) {
        arm9->wake_flags |= 2u;
    }
    unsigned char *reg1 = (unsigned char *)arm7->io_mirror;
    uint32_t pend1 = ((io_mirror_t *)reg1)->irq.if_pending | 0x80000u;
    ((io_mirror_t *)reg1)->irq.if_pending = pend1;
    uint32_t tap1 = (uint8_t)arm7->halt_flags;
    if (!(tap1 & 6)) {
        uint32_t v = (((io_mirror_t *)reg1)->irq.ie & pend1)
                   & (uint32_t)(-(int32_t)((io_mirror_t *)reg1)->irq.ime);
        arm7->irq_pending = v;
        if (v != 0) arm7->wake_flags |= 2u;
    } else if (arm7->irq_pending != 0) {
        arm7->wake_flags |= 2u;
    }
    return pal;
}




void cart_data_transfer_step(cart_t *obj, uint32_t word)
{
    if (obj->backup_write_mode != 0 && obj->backup_write_enabled != 0) {
        spi_memory_write_word(&obj->backup, word);
        return;
    }
    if (obj->write_words_remaining == 0)
        return;
    ((long (*)(int, const void *, unsigned long))sym_libc_write)(
        obj->argv_fd, &word, 4);
    uint32_t left = obj->write_words_remaining - 1u;
    obj->write_words_remaining = left;
    if (left != 0)
        return;
    io_mirror_t *state = obj->mirror;
    state->cart.romctrl &= 0x7f7fffffu;
    if ((state->cart.auxspicnt & 0x4000) == 0)
        return;
    unsigned char *machine = (unsigned char *)obj->machine;
    arm_t *arm9 = &((nds_t *)machine)->arm9;
    arm_t *arm7 = &((nds_t *)machine)->arm7;
    unsigned char *reg0 = (unsigned char *)arm9->io_mirror;
    uint32_t pend0 = ((io_mirror_t *)reg0)->irq.if_pending | 0x80000u;
    ((io_mirror_t *)reg0)->irq.if_pending = (pend0);
    uint8_t tap0 = (uint8_t)arm9->halt_flags;
    if ((tap0 & 6u) == 0) {
        uint32_t enable0 = ((io_mirror_t *)reg0)->irq.ie;
        uint32_t covered0 = ((io_mirror_t *)reg0)->irq.ime;
        uint32_t mask0 = (enable0 & pend0) & (0u - covered0);
        arm9->irq_pending = mask0;
    }
    if (arm9->irq_pending != 0)
        arm9->wake_flags = arm9->wake_flags | 2u;
    unsigned char *reg1 = (unsigned char *)arm7->io_mirror;
    uint32_t pend1 = ((io_mirror_t *)reg1)->irq.if_pending | 0x80000u;
    ((io_mirror_t *)reg1)->irq.if_pending = (pend1);
    uint8_t tap1 = (uint8_t)arm7->halt_flags;
    if ((tap1 & 6u) == 0) {
        uint32_t enable1 = ((io_mirror_t *)reg1)->irq.ie;
        uint32_t covered1 = ((io_mirror_t *)reg1)->irq.ime;
        uint32_t mask1 = (enable1 & pend1) & (0u - covered1);
        arm7->irq_pending = mask1;
    }
    if (arm7->irq_pending != 0)
        arm7->wake_flags = arm7->wake_flags | 2u;
}

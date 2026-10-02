#include <stdint.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "core/nds_state.h"
#include "core_internals.h"
#include <string.h>

void cart_backup_init_paths(cart_t *cart, unsigned char *ctx) {
    unsigned char *dir  = (unsigned char *)((nds_t *)ctx)->cache_dir;
    unsigned char *dir2 = (unsigned char *)((nds_t *)ctx)->save_dir;
    char buf[0x428];

    cart->use_user_database = 0;
    str_vsprintf_caller_limit_3(buf, 1056, "%s%cgame_database.xml", dir, '/');
    db_load_cartridge_database(&cart->database[0], buf);
    str_vsprintf_caller_limit_3(buf, 1056, "%s%cgame_database_user.xml", dir, '/');
    int32_t rc = db_load_cartridge_database(&cart->database[1], buf);
    if (rc >= 0)
        cart->use_user_database = 1;
    str_vsprintf_caller_limit_3(buf, 1056, "%s%cusrcheat.dat", dir2, '/');
    db_load_cheat_index(&cart->cheats, buf);

    cart->mirror = &((nds_t *)ctx)->bus.io_mirror[0];
    cart->dma7 = &((nds_t *)ctx)->bus.dma[1];
    cart->dma9 = &((nds_t *)ctx)->bus.dma[0];
    cart->rom = 0;
    cart->machine = (nds_t *)ctx;
    cart->argv_fd = -1;
    cart->slot2.sensor.index = 255;
    cart->slot2.sensor.ident = 0xf00f;
    cart->slot2.gpio.config = (uint8_t *)&((nds_t *)ctx)->config;
}

void *cart_backup_reset_zone_defaults(cart_t *cart) {
    cart->chip_id = 0;
    cart->read_address = 0;
    cart->words_remaining = 0;
    cart->transfer_deadline_cycles = 0;
    cart->irq_pending = 0;
    cart->key1_active = 0;
    cart->slot2.sensor.phase = 0;
    cart->slot2.sensor.accel8_x = 0x80;
    cart->slot2.sensor.accel8_y = 0x80;
    cart->slot2.sensor.accel8_z = 0x80;
    cart->slot2.sensor.phase16 = 0;
    cart->slot2.sensor.index16 = 0;
    cart->slot2.sensor.accel16_x = 0x8000;
    cart->slot2.sensor.accel16_y = 0x8000;
    cart->slot2.sensor.accel16_z = 0x8000;
    cart->slot2.sensor.gyro16 = 0x6900;
    cart->slot2.gpio.bit1 = 0;
    cart->slot2.gpio.countdown = 0;
    return cart;
}

void cart_backup_clear_flag_byte(void *param_1)
{
    const unsigned char zero = 0;
    memcpy((unsigned char *)param_1 + 17, &zero, sizeof(zero));
}

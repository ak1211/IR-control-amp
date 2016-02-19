/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * i2c_display.h
 *
 * Created: 2015/12/13 21:28:16
 *  Author: akihiro
 */

#include <stdint-gcc.h>

#ifndef I2C_DISPLAY_H_
#define I2C_DISPLAY_H_

enum {
    CHAR_VOL_BACKGROUND     = 0x00,
    CHAR_VOL_LEFT_BAR       = 0x01,
    CHAR_VOL_RIGHT_BAR      = 0x02,
    CHAR_LEFT_BAR           = 0x03,
    CHAR_LEFT_RIGHT_BAR     = 0x04,
    CHAR_LEFT_PEAK_LEVEL    = 0x05,
    CHAR_RIGHT_PEAK_LEVEL   = 0x06,
    CHAR_PEAK_LEVEL_OVER_DB = 0x07,
};

extern void i2c_display_init();
extern void i2c_display_power_on();
extern void i2c_display_power_off();
extern void i2c_display_send_data( uint8_t data );
extern void i2c_display_send_command( uint8_t command );

#endif /* I2C_DISPLAY_H_ */

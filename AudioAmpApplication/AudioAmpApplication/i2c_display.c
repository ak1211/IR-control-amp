/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * i2c_display.c
 * i2cディスプレイのi2cバス制御部分を担当します
 *
 * Created: 2015/03/05 21:36:00
 *  Author: akihiro
 */

#include <stdbool.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/twi.h>
#include <util/delay.h>

#include "ApplicationIncl.h"
#include "i2c_display.h"

#define I2C_DISPLAY_ADDR    (0x3C <<1)

//
//i2c状態取得
//
static uint8_t i2c_get_stat_block()
{
    do{}while(!(TWCR & _BV(TWINT)));
    
    return TW_STATUS;
}

//
//i2cスタートコンディション発行のち
//i2cスレーブへデータ送信
//
static bool i2c_start( uint8_t i2c_slave_addr, uint8_t send_byte )
{
    do {
        //スタートコンディション発行
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
        //結果待ちと再試行
        switch (i2c_get_stat_block()) {
            case TW_REP_START:
            case TW_START:
                                    //成功
            break;
            case TW_MT_ARB_LOST:
                continue;           //再試行
            break;
            default:
                return false;       //失敗
            break;
        }
        //i2cスレーブアドレス送信
        TWDR = i2c_slave_addr | TW_WRITE;
        TWCR = _BV(TWINT) | _BV(TWEN);
        //結果待ちとACK値の検査
        switch (i2c_get_stat_block()) {
            case TW_MT_SLA_ACK:
                                    //スレーブより返答:成功
            break;
            case TW_MT_SLA_NACK:
            case TW_MT_ARB_LOST:
                continue;           //再試行
            break;
            default:
                return false;       //失敗
            break;
        }
        //i2cスレーブへデータ送信
        TWDR = send_byte;
        TWCR = _BV(TWINT) | _BV(TWEN);
        //結果待ちとACK値の検査
        switch (i2c_get_stat_block()) {
            case TW_MT_DATA_ACK:
                                    //スレーブより返答:成功
            break;
            case TW_MT_DATA_NACK:
            case TW_MT_ARB_LOST:
                continue;           //再試行
            break;
            default:
                return false;       //失敗
            break;
        }
    } while(false); //continueで先頭に戻るためで、ループではない

    return true;    //成功
}

//
//i2cストップコンディション発行
//
static void i2c_stop()
{
    TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN);
    do{}while(TWCR & _BV(TWSTO));
}

//
//i2cディスプレイへデータ送信
//
static bool i2c_display_send( uint8_t i2c_slave_addr, uint8_t first_byte, uint8_t second_byte )
{
    bool is_success;

    //失敗フラグで開始する
    is_success = false;
    do {
        //
        if (i2c_start( i2c_slave_addr, first_byte ) != true) {
            break;
        }
        //
        TWDR = second_byte;
        TWCR = _BV(TWINT) | _BV(TWEN);
        //結果待ちとACK値の検査
        switch (i2c_get_stat_block()){
            case TW_MT_DATA_ACK:
                is_success = true;  //スレーブより返答:成功
            break;
            case TW_MT_ARB_LOST:
                continue;           //再試行
            break;
            case TW_MT_DATA_NACK:
            default:
                                    //失敗
            break;
        }
    } while(false); //continueで先頭に戻るためで、ループではない

    i2c_stop();

    return is_success;
}

//
//i2cディスプレイにコマンド送信
//
void i2c_display_send_command( uint8_t command )
{
    if (IOread_from_oled_power_on()) {  //電源が入っていないと無限ループに陥るので
        i2c_display_send( I2C_DISPLAY_ADDR, 0x00, command );
    }
}

//
//i2cディスプレイにデータ送信
//
void i2c_display_send_data( uint8_t data )
{
    if (IOread_from_oled_power_on()) {  //電源が入っていないと無限ループに陥るので
        i2c_display_send( I2C_DISPLAY_ADDR, 0x40, data );
    }
}

/*
 * 初期化
 */
void i2c_display_init()
{
    // TWBR = {(CLOCK(8MHz) / I2C_CLK) - 16} / 2;
    // I2C_CLK = 100kHz, CLOCK = 8MHz, TWBR = 32
    #if F_CPU ==  8000000
    TWBR = 32;
    #else
    #error "F_CPU is not valid"
    #endif
    TWSR = 0;
}

/*
 * ディスプレイ電源ON
 */
void i2c_display_power_on()
{
    static const uint8_t register_characters[] PROGMEM = {
        //
        //左右・
        //
        0b00000000,     //line 1
        0b00000000,     //line 2
        0b00000000,     //line 3
        0b00000000,     //line 4
        0b00000000,     //line 5
        0b00001001,     //line 6
        0b00001001,     //line 7
        0b00000000,     //line 8
        //
        //左１本棒
        //
        0b00001000,     //line 1
        0b00001000,     //line 2
        0b00001000,     //line 3
        0b00001000,     //line 4
        0b00001000,     //line 5
        0b00001001,     //line 6
        0b00001001,     //line 7
        0b00000000,     //line 8
        //
        //右１本棒
        //
        0b00000001,     //line 1
        0b00000001,     //line 2
        0b00000001,     //line 3
        0b00000001,     //line 4
        0b00000001,     //line 5
        0b00001001,     //line 6
        0b00001001,     //line 7
        0b00000000,     //line 8
        //
        //レベルメータ左１本棒
        //
        0b00001000,     //line 1
        0b00001000,     //line 2
        0b00001000,     //line 3
        0b00001000,     //line 4
        0b00000000,     //line 5
        0b00000000,     //line 6
        0b00000000,     //line 7
        0b00000000,     //line 8
        //
        //レベルメータ左右２本棒
        //
        0b00001001,     //line 1
        0b00001001,     //line 2
        0b00001001,     //line 3
        0b00001001,     //line 4
        0b00000000,     //line 5
        0b00000000,     //line 6
        0b00000000,     //line 7
        0b00000000,     //line 8
        //
        //左ピーク表示
        //
        0b00000000,     //line 1
        0b00000000,     //line 2
        0b00000000,     //line 3
        0b00000000,     //line 4
        0b00011100,     //line 5
        0b00011100,     //line 6
        0b00011100,     //line 7
        0b00000000,     //line 8
        //
        //右ピーク表示
        //
        0b00000000,     //line 1
        0b00000000,     //line 2
        0b00000000,     //line 3
        0b00000000,     //line 4
        0b00000111,     //line 5
        0b00000111,     //line 6
        0b00000111,     //line 7
        0b00000000,     //line 8
        //
        //peak over dB表示
        //
        0b00000000,     //line 1
        0b00000000,     //line 2
        0b00000000,     //line 3
        0b00000000,     //line 4
        0b00000100,     //line 5
        0b00000110,     //line 6
        0b00000111,     //line 7
        0b00000000,     //line 8
    };
    static const uint8_t register_characters_count = (sizeof(register_characters)/sizeof(register_characters[0])) / 8;

    //ディスプレイ電源ON
    IOwrite_to_oled_power_on();
    //コマンドが送信できるようになるまでしばらく待つ
    _delay_ms(100);

    //
    //外字登録
    //
    for (uint8_t code=0x00 ; code<register_characters_count ; code++) {
        //set cgram
        i2c_display_send_command (0x40 | (code<<3) | 0);    //char code 'code' and top line
        _delay_ms(1);
        //write data
        for (uint8_t n=0 ; n<8 ; n++) {
            i2c_display_send_data (pgm_read_byte(&register_characters[8*code + n]));
            _delay_ms(1);
        }
    }

    //Clear Display
    i2c_display_send_command(0x01);
    _delay_ms(20);
    //Return Home
    i2c_display_send_command(0x02);
    _delay_ms(2);
    //Send Display on command
    i2c_display_send_command(0x0C);
    _delay_ms(2);
    //Clear Display
    i2c_display_send_command(0x01);
    _delay_ms(20);

    //RE=1
    i2c_display_send_command(0x2a);
    //SD=1
    i2c_display_send_command(0x79);
    //contrast set
    i2c_display_send_command(0x81);
    //輝度
    i2c_display_send_command(0xFF);
    //SDを０に戻す
    i2c_display_send_command(0x78);
    //2C=高文字 28=ノーマル
    i2c_display_send_command(0x28);
}

/*
 * ディスプレイ電源OFF
 */
void i2c_display_power_off()
{
    if (IOread_from_oled_power_on()) {
        //Clear Display
        i2c_display_send_command(0x01);
        _delay_ms(20);

        IOwrite_to_oled_power_off();
    }
}

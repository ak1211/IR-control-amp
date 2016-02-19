/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * electric_volume.c
 * 電子ボリュームＩＣを操作する部分を担当します
 *
 * Created: 2015/04/22 20:32:13
 *  Author: akihiro
 */

#include <stdbool.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "ApplicationIncl.h"
#include "electric_volume.h"

//
//LM1972に1ビット書き込む
//
static void LM1972_write_data( uint8_t bitHigh )
{
    if (bitHigh) {
        IOwrite_to_LM1972_DATA_H();
    } else {
        IOwrite_to_LM1972_DATA_L();
    }
    IOwrite_to_LM1972_CLOCK_H();    // CLOCKをHにします。
    _delay_us(2);                   // 余裕を見て2usウェイトします。
    IOwrite_to_LM1972_CLOCK_L();    // CLOCKをLにします。
    _delay_us(2);                   // 余裕を見て2usウェイトします。
}

//
//LM1972に設定値を書き込む
//
static void LM1972_write( uint8_t ch, uint8_t data )
{
    IOwrite_to_LM1972_CLOCK_L();        // CLOCKをLにします。
    IOwrite_to_LM1972_LOADSHIFT_L();    // LOAD/SHIFTをLにします。Enable
    _delay_us(2);                       // 余裕を見て2usウェイトします。

    LM1972_write_data (0);          //bit7
    LM1972_write_data (0);          //bit6
    LM1972_write_data (0);          //bit5
    LM1972_write_data (0);          //bit4
    LM1972_write_data (0);          //bit3
    LM1972_write_data (0);          //bit2
    LM1972_write_data (ch &  0x2);  //bit1
    LM1972_write_data (ch &  0x1);  //bit0
    //
    //
    //
    LM1972_write_data (data & 0x80);    //bit7
    LM1972_write_data (data & 0x40);    //bit6
    LM1972_write_data (data & 0x20);    //bit5
    LM1972_write_data (data & 0x10);    //bit4
    LM1972_write_data (data &  0x8);    //bit3
    LM1972_write_data (data &  0x4);    //bit2
    LM1972_write_data (data &  0x2);    //bit1
    LM1972_write_data (data &  0x1);    //bit0
    //
    //
    //
    _delay_us(2);                       // 余裕を見て2usウェイトします。
    IOwrite_to_LM1972_LOADSHIFT_H();    // LOAD/SHIFTをHにします。Disable
    IOwrite_to_LM1972_DATA_pwrsave();   // 省電力のため
    IOwrite_to_LM1972_CLOCK_pwrsave();  // 省電力のため
}

//
//電子ボリュームの使用開始
//
void elevol_prepare()
{
    elevol_set_mute();
}

//
//電子ボリュームを省電力状態に
//
void elevol_power_save()
{
    elevol_set_mute();
    elevol_setvolume(0);

    //Vdd+0.2V以上の電圧を入力すると絶対最大定格を超えるので
    //出力ピンはLレベルにする
    IOwrite_to_LM1972_CLOCK_L();
    IOwrite_to_LM1972_LOADSHIFT_L();
    IOwrite_to_LM1972_DATA_L();
}

//
//
//
elevol_t elevol_validate_volume( elevol_t volume )
{
#if (ELEVOL_MIN != 0) || (ELEVOL_MAX != 31)
#error "0 <= ELEVOL <= 31"
#endif
    if (volume < ELEVOL_MIN) {
        volume = ELEVOL_MIN;
    } else if (ELEVOL_MAX < volume) {
        volume = ELEVOL_MAX;
    }

    return volume;
}

//
//電子ボリュームをsetvolにする
//
elevol_t elevol_setvolume( elevol_t setvol )
{
    static const uint8_t tVolume[32] PROGMEM = {
        // lm1972_param
        127, 105,  99,  95,  90,  86,  83,  80,  78,  76,  74,  72,  71,  69,  68,  67,  60,  53,  48,  44,  41,  38,  36,  34,  27,  20,  15,  11,   8,   5,   3,   0,
    };
    elevol_t idx = elevol_validate_volume(setvol);
    uint8_t v = pgm_read_byte(&tVolume[idx]);
    //
    //IN1 : left
    //channel selection : 0
    //
    LM1972_write( 0, v );
    //
    //IN2 : right
    //channel selection : 1
    //
    LM1972_write( 1, v );

    return idx;
}

//
//電子ボリュームをミュート状態に
//
void elevol_set_mute()
{
    //LM1972にはミュートピンがないのでボリューム0で代用する
    elevol_setvolume(0);
}

//
//電子ボリュームをミュート解除状態に
//
elevol_t elevol_clear_mute( elevol_t vol )
{
    return elevol_setvolume(vol);
}

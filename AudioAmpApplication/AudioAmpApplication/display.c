/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * https://github.com/ak1211/IR-control-amp
 *
 * Copyright (c) 2015 Akihiro Yamamoto http://ak1211.com
 *
 * This software is released under the MIT License.
 * https://github.com/ak1211/IR-control-amp/blob/master/LICENSE
 */

/*
 * display.c
 * ディスプレイに表示する部分を担当します。
 *
 * Created: 2015/04/22 20:43:15
 *  Author: akihiro
 */

#include <stdio.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "ApplicationIncl.h"
#include "electric_volume.h"
#include "audio_level_meter.h"
#include "i2c_display.h"
#include "display.h"

//ミュートメッセージのウエイト
#define MUTE_MESSAGE_SCROLL_WAIT    200

//今のディスプレイバッファ
static display_buffer_t nowDisplayBuffer = {};
//次のディスプレイバッファ
static display_buffer_t nextDisplayBuffer = {};

enum {
    LEVEL_METER_TABLE_SIZE = 30,
    LEVEL_METER_SCALE_MIN = 0,
    LEVEL_METER_SCALE_MAX = LEVEL_METER_TABLE_SIZE-1,
};
static int8_t num_of_bar_level( uint16_t level, int8_t level_over_return )
{
    static const uint16_t level_meter_tbl[LEVEL_METER_TABLE_SIZE] PROGMEM = {
        //最大値 +3vu
          1,   2,   3,   6,   9,  13,  20,  30,  38,  47,  60,  76,  85,  95, 107,
        120, 135, 151, 170, 191, 214, 240, 269, 302, 320, 339, 359, 381, 452, 538,
    };

    for (uint8_t idx=0 ; idx<LEVEL_METER_TABLE_SIZE ; idx++) {
        if (level < pgm_read_word(&level_meter_tbl[idx])) {
            return idx;
        }
    }
    return level_over_return;   //レベルオーバー
}

//
//現在のボリュームをメーターの形で表現
//
static void render_volume_meter( display_line_buf_t line, elevol_t volume )
{
    volume = elevol_validate_volume( volume );
    //バックグラウンド
    memset (line, CHAR_VOL_BACKGROUND, DISPLAY_WIDTH );
    //指針を書き込む
    uint8_t idx = volume >> 1;
    line[idx] = (volume & 1) ? CHAR_VOL_RIGHT_BAR : CHAR_VOL_LEFT_BAR;
}

//
//現在の入力とボリューム
//
static void render_input_source_and_volume( display_line_buf_t line, uint8_t src_num,  elevol_t volume )
{
    char buf[DISPLAY_WIDTH + 1];
    snprintf_P (buf, sizeof(buf), PSTR("SOURCE:%-3dVOL:%2d"),  src_num, volume);
    strncpy (line, buf, DISPLAY_WIDTH );
}

//
//オーディオレベルメータ表示
//
static void render_audio_level_meter( audio_level_t* audiolevel )
{
    const char msg[] = "LR";

    for (uint8_t linenum=0 ; linenum<DISPLAY_LINES ; linenum++) {
        //
        union UU16LeftRight al, pl;
        get_audio_level( &al, &pl, audiolevel );
        //オーディオレベルからメーターの位置を得る
        int8_t level = num_of_bar_level( al.array[linenum], LEVEL_METER_SCALE_MAX );
        //ピークレベルからメーターの位置を得る
        int8_t peak = num_of_bar_level( pl.array[linenum], LEVEL_METER_SCALE_MAX );
        //
        //メータの位置を画面に表示する
        //
        int8_t idx = DISPLAY_WIDTH - 1;
        //
        memset (nextDisplayBuffer[linenum].line, ' ', DISPLAY_WIDTH);
        //'L'または'R'を表示する
        nextDisplayBuffer[linenum].line[0] = msg[linenum];
        if (level > 0) {
            //レベル位置のレベルメーターキャラクタ書き込み
            idx = 1 + (level >> 1); //0の位置はLまたはR文字がいるため1から開始
            if ((level & 1) == 0) {
                //２の倍数なら左右表示
                nextDisplayBuffer[linenum].line[idx] = CHAR_LEFT_RIGHT_BAR;
                idx--;
            } else {
                //２の倍数でないなら左表示
                nextDisplayBuffer[linenum].line[idx] = CHAR_LEFT_BAR;
                idx--;
            }
            //レベル位置のレベルメーターキャラクタより下は左右表示
            for (; idx>=1 ; idx--) {
                //左右表示
                nextDisplayBuffer[linenum].line[idx] = CHAR_LEFT_RIGHT_BAR;
            }
        }
        //ピークレベル位置キャラクタ書き込み
        idx = 1 + (peak >> 1);  //0の位置はLまたはR文字がいるために1から開始
        if (peak > 0) {
            idx = idx + 1;      //ピークキャラクタはレベルメーターの右側
        }
        if (idx >= DISPLAY_WIDTH) {
            //over dB
            nextDisplayBuffer[linenum].line[DISPLAY_WIDTH-1] = CHAR_PEAK_LEVEL_OVER_DB;
        } else {
            if ((peak & 1) == 0) {
                //２の倍数なら左表示
                nextDisplayBuffer[linenum].line[idx] = CHAR_LEFT_PEAK_LEVEL;
            } else {
                //２の倍数でないなら右表示
                nextDisplayBuffer[linenum].line[idx] = CHAR_RIGHT_PEAK_LEVEL;
            }
        }
    }
}

//
//全ての文字を更新する
//
static void show_display_force()
{
    for (int8_t linenum=0 ; linenum<DISPLAY_LINES ; linenum++) {
        for (int8_t column=0 ; column<DISPLAY_WIDTH ; column++) {
            //カーソルを移動
            i2c_display_send_command (0x80 + 0x20*linenum + column);
            //更新
            nowDisplayBuffer[linenum].line[column] = nextDisplayBuffer[linenum].line[column];
            i2c_display_send_data (nowDisplayBuffer[linenum].line[column]);
        }
    }
}

//
//バッファと比較して、変化のあった部分だけ更新する
//
static void update_display()
{
    for (int8_t linenum=0 ; linenum<DISPLAY_LINES ; linenum++) {
        for (int8_t column=0 ; column<DISPLAY_WIDTH ; column++) {
            if (nowDisplayBuffer[linenum].line[column] != nextDisplayBuffer[linenum].line[column]) {
                //カーソルを移動
                i2c_display_send_command (0x80 + 0x20*linenum + column);
                //更新
                nowDisplayBuffer[linenum].line[column] = nextDisplayBuffer[linenum].line[column];
                i2c_display_send_data (nowDisplayBuffer[linenum].line[column]);
            }
        }
    }
}

//
//ディスプレイに表示する。
//
void display_message( const display_buffer_t buffer )
{
    for (uint8_t linenum=0 ; linenum<DISPLAY_LINES ; linenum++) {
        //行
        int8_t column;
        for (column=0 ; (column<DISPLAY_WIDTH)&&(buffer[linenum].line[column] != '\0') ; column++) {
            //桁
            nextDisplayBuffer[linenum].line[column] = buffer[linenum].line[column];
        }
        //'\0'以降は空白で表示する
        for (; column<DISPLAY_WIDTH ; column++) {
            nextDisplayBuffer[linenum].line[column] = ' ';
        }
    }

    show_display_force();
}

//
//情報を表示する。
//
void display_information( elevol_t volume )
{
    //１行目
    render_volume_meter( nextDisplayBuffer[0].line, volume );
    //２行目
    render_input_source_and_volume( nextDisplayBuffer[1].line, IOread_from_select_source(), volume );

    update_display ();
}

//
//ミュート状態の情報を表示する。
//
uint16_t display_mute_information( uint16_t counter, elevol_t volume )
{
    static const char message[] PROGMEM = {
    //   1234567890123456
        "   M  u  t  e   "
        "   m  U  t  e   "
        "   m  u  T  e   "
        "   m  u  t  E   "
    };
    #if MUTE_MESSAGE_SCROLL_WAIT <= 0
    #error "MUTE_MESSAGE_SCROLL_WAIT must be natural number"
    #endif
    //１行目
    uint8_t idx = (counter / (MUTE_MESSAGE_SCROLL_WAIT)) % 4;
    strncpy_P (nextDisplayBuffer[0].line, &message[16*idx], DISPLAY_WIDTH );
    //２行目
    render_input_source_and_volume( nextDisplayBuffer[1].line, IOread_from_select_source(), volume );

    update_display ();

    return (counter + 1) % (MUTE_MESSAGE_SCROLL_WAIT*4);
}

//
//オーディオレベルメーターを表示する。
//
void display_audio_level_meter( audio_level_t* audiolevel )
{
    render_audio_level_meter (audiolevel);

    update_display ();
}

//
//全ての文字を更新する
//
void display_refresh()
{
    show_display_force();
}

//
//画面消去
//
void display_clear()
{
    memset (nowDisplayBuffer, ' ', sizeof(nowDisplayBuffer));
    memset (nextDisplayBuffer, ' ', sizeof(nextDisplayBuffer));
    display_refresh ();
}

//
//ディスプレイの使用開始
//
void display_prepare()
{
    i2c_display_power_on ();            //i2cディスプレイモジュールON
    display_clear ();
}

//
//ディスプレイを省電力状態に
//
void display_power_save()
{
    i2c_display_power_off ();
}

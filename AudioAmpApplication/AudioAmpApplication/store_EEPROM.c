/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * store_EEPROM.c
 * EEPROM読み込み/書き込み部分を担当します
 *
 * Created: 2015/12/21 18:58:16
 *  Author: akihiro
 */

#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "ApplicationIncl.h"
#include "IR_receiver.h"
#include "electric_volume.h"
#include "store_EEPROM.h"

//
//EEPROMから読み出しに失敗した時の初期値
//一例です。実運用は設定メニューから好きなボタンを登録お願いします
//
static const saved_eeprom_data_t Inital_EEPROM_data PROGMEM = {
    1,  //初期音量
    {
        //
        //電源ONOFFリモコンコード
        [IRCODE_onoff]          = {
            .ir.custom_code     = 0xbf40,           //リモコンカスタムコード    （0xbf40 == TOSHIBA）
            .ir.data_code       = 0x12,             //リモコンデーターコード    （0x12 == 電源）
            .ir.inv_data_code   = 0xed,             //リモコンデーター反転コード（0xed == ~0x12）
        },
        //
        //入力切替リモコンコード
        [IRCODE_switch_source]  = {
            .ir.custom_code     = 0xbf40,           //リモコンカスタムコード     (0xbf40 == TOSHIBA)
            .ir.data_code       = 0x0f,             //リモコンデーターコード    （0x0f == 入力切替)
            .ir.inv_data_code   = 0xf0,             //リモコンデーター反転コード (0xf0 == ~0x0f)
        },
        //音量＋リモコンコード
        [IRCODE_volume_up]      = {
            .ir.custom_code     = 0xbf40,           //リモコンカスタムコード     (0xbf40 == TOSHIBA)
            .ir.data_code       = 0x1a,             //リモコンデーターコード     (0x1a == 音量＋)
            .ir.inv_data_code   = 0xe5,             //リモコンデーター反転コード (0xe5 == ~0x1a)
        },
        //音量ーリモコンコード
        [IRCODE_volume_down]    = {
            .ir.custom_code     = 0xbf40,           //リモコンカスタムコード     (0xbf40 == TOSHIBA)
            .ir.data_code       = 0x1e,             //リモコンデーターコード     (0x1e == 音量ー)
            .ir.inv_data_code   = 0xe1,             //リモコンデーター反転コード (0xe1 == ~0x1e)
        },
        //ミュートリモコンコード
        [IRCODE_mute]   = {
            .ir.custom_code     = 0xbc45,           //リモコンカスタムコード     (0xbc45 ==)
            .ir.data_code       = 0x46,             //リモコンデーターコード     (0x46 == ミュート)
            .ir.inv_data_code   = 0xb9,             //リモコンデーター反転コード (0xb9 == ~0x46)
        },
        //表示省略リモコンコード
        [IRCODE_omit_display]   = {
            .ir.custom_code     = 0xbc45,           //リモコンカスタムコード     (0xbc45 ==)
            .ir.data_code       = 0xde,             //リモコンデーターコード     (0xde == 表示省略)
            .ir.inv_data_code   = 0x21,             //リモコンデーター反転コード (0x21 == ~0xde)
        },
    }
};

typedef uint16_t checksum_t;

//
//EEPROMに保存/読み込みするデーター
//リセット/電源断後はEEPROMから読み込む
//
saved_eeprom_data_t Saved_EEPROM_data;


//
//チェックサム計算
//
static checksum_t get_checksum( const void *first_addr, size_t size )
{
    const uint8_t *data = first_addr;   
    checksum_t sum;
    
    sum = 0x0000;
    for (uint8_t idx=0 ; idx<size ; idx++ ) {
        sum = sum ^ data[idx];      //排他的論理和
    }

    return sum;
}

//
//EEPROMアドレス
//
enum {
    EEPROM_ADDR_START               = 0x00,
    //
    EEPROM_ADDR_BO_RESET_COUNTER    = EEPROM_ADDR_START,
    //
    EEPROM_ADDR_WDT_RESET_COUNTER   = EEPROM_ADDR_BO_RESET_COUNTER + sizeof(uint16_t),
    //
    EEPROM_ADDR_STORE_THE_CHECKSUM  = EEPROM_ADDR_WDT_RESET_COUNTER + sizeof(uint16_t),
    EEPROM_ADDR_STORE_THE_DATA      = EEPROM_ADDR_STORE_THE_CHECKSUM + sizeof(checksum_t),
};

//
//カウンタをクリアする
//
static void EEPROM_clear_the_counter( void* addr )
{
    //準備完了待ち
    eeprom_busy_wait ();
    //書き込む
    eeprom_write_word( addr, 0 );
}

//
//電圧低下リセットカウンタ読み込み
//
int16_t EEPROM_read_brownout_reset_counter()
{
    //準備完了待ち
    eeprom_busy_wait ();
    //読み込む
    uint16_t word = eeprom_read_word((void*)EEPROM_ADDR_BO_RESET_COUNTER);
    return word;
}

//
//電圧低下リセットカウンタ加算
//
void EEPROM_acc_brownout_reset_counter()
{
    //準備完了待ち
    eeprom_busy_wait ();
    //読み込む
    uint16_t word = eeprom_read_word((void*)EEPROM_ADDR_BO_RESET_COUNTER);
    //加算
    if (word < 0xffff) {
        word = word + 1;
    }
    //準備完了待ち
    eeprom_busy_wait ();
    //書き込む
    eeprom_write_word( (void*)EEPROM_ADDR_BO_RESET_COUNTER, word );
}

//
//ウォッチドッグ・タイムアウトリセットカウンタ読み込み
//
int16_t EEPROM_read_watchdog_timeout_counter()
{
    //準備完了待ち
    eeprom_busy_wait ();
    //読み込む
    uint16_t word = eeprom_read_word((void*)EEPROM_ADDR_WDT_RESET_COUNTER);
    //フラグを取り除く
    return word & ~0x8000;
}

//
//ソフトウエアリセットフラグを得る
//
bool EEPROM_get_software_reset_flag()
{
    //準備完了待ち
    eeprom_busy_wait ();
    //読み込む
    uint16_t word = eeprom_read_word((void*)EEPROM_ADDR_WDT_RESET_COUNTER);
    //フラグを取り出す
    return (word & 0x8000) ? true : false;

}

//
//ウォッチドッグ・タイムアウトリセットカウンタを加算する
//
void EEPROM_acc_watchdog_timeout_counter()
{
    //準備完了待ち
    eeprom_busy_wait ();
    //読み込む
    uint16_t word = eeprom_read_word((void*)EEPROM_ADDR_WDT_RESET_COUNTER);
    //カウンタのみ加算
    if ((word & 0x7fff) < 0x7fff) {
        word = word +1; 
    }
    //準備完了待ち
    eeprom_busy_wait ();
    //書き込む
    eeprom_write_word( (void*)EEPROM_ADDR_WDT_RESET_COUNTER, word );
}

//
//ソフトウエアリセットフラグをセットする
//
void EEPROM_set_software_reset_flag()
{
    //準備完了待ち
    eeprom_busy_wait ();
    //読み込む
    uint16_t word = eeprom_read_word((void*)EEPROM_ADDR_WDT_RESET_COUNTER);
    //準備完了待ち
    eeprom_busy_wait ();
    //フラグをセットして書き込む
    eeprom_write_word( (void*)EEPROM_ADDR_WDT_RESET_COUNTER, word|0x8000 );
}

//
//ソフトウエアリセットフラグをクリアする
//
void EEPROM_clear_software_reset_flag()
{
    //準備完了待ち
    eeprom_busy_wait ();
    //読み込む
    uint16_t word = eeprom_read_word((void*)EEPROM_ADDR_WDT_RESET_COUNTER);
    //準備完了待ち
    eeprom_busy_wait ();
    //フラグをクリアして書き込む
    eeprom_write_word( (void*)EEPROM_ADDR_WDT_RESET_COUNTER, word&~0x8000 );
}

//
//EEPROM読み込み
//
int8_t EEPROM_read( saved_eeprom_data_t *data )
{
    checksum_t checksum;

    //準備完了待ち
    eeprom_busy_wait ();
    //チェックサムを読み込む
    eeprom_read_block( &checksum, (void*)EEPROM_ADDR_STORE_THE_CHECKSUM, sizeof(checksum_t) );
    //準備完了待ち
    eeprom_busy_wait ();
    //データーを読み込む
    eeprom_read_block( data, (void*)EEPROM_ADDR_STORE_THE_DATA, sizeof(saved_eeprom_data_t));
    //チェックサム計算
    if (checksum != get_checksum( data, sizeof(saved_eeprom_data_t) )) {
        //
        //チェックサム不一致のために初期値を使う
        //
        //カウンタを初期値に
        EEPROM_clear_the_counter( (void*)EEPROM_ADDR_BO_RESET_COUNTER );
        EEPROM_clear_the_counter( (void*)EEPROM_ADDR_WDT_RESET_COUNTER );
        //初期値を書き込む
        memcpy_P (data, &Inital_EEPROM_data, sizeof(saved_eeprom_data_t));
        EEPROM_write(data);
        return -1;
    }
    return 0;
}

//
//EEPROM書き込み
//
void EEPROM_write( const saved_eeprom_data_t *data )
{
    checksum_t checksum;

    //チェックサム計算
    checksum = get_checksum( data, sizeof(saved_eeprom_data_t) );
    //準備完了待ち
    eeprom_busy_wait ();
    //チェックサムを書き込む
    eeprom_write_block( &checksum, (void*)EEPROM_ADDR_STORE_THE_CHECKSUM, sizeof(checksum_t) );
    //準備完了待ち
    eeprom_busy_wait ();
    //データーを書き込む
    eeprom_write_block( data, (void*)EEPROM_ADDR_STORE_THE_DATA, sizeof(saved_eeprom_data_t));
}

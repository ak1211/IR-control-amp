/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * store_EEPROM.h
 * EEPROM読み込み/書き込み部分を担当します
 *
 * Created: 2015/12/21 18:58:16
 *  Author: akihiro
 */

#include <stdint-gcc.h>

#ifndef STORE_EEPROM_H_
#define STORE_EEPROM_H_

typedef enum {
    IRCODE_onoff            = 0,    //電源ONOFFリモコンコード
    IRCODE_switch_source    = 1,    //入力切替リモコンコード
    IRCODE_volume_up        = 2,    //音量＋リモコンコード
    IRCODE_volume_down      = 3,    //音量ーリモコンコード
    IRCODE_mute             = 4,    //ミュートリモコンコード
    IRCODE_omit_display     = 5,    //表示省略リモコンコード
    //
    IRCODE_total_num        = 6,    //全リモコンコード数
    //
    IRCODE_not_found        = -1,   //見つからないリモコンコード
} IRCODE_t;

//
//EEPROMに保存するデーター
//
typedef struct __attribute__((__packed__)) {
            elevol_t                    init_volume;                //電子ボリュームの初期値
    union   irr_necformat_or_uint32_t   ircodes[IRCODE_total_num];  //リモコンコード
} saved_eeprom_data_t;

extern int16_t EEPROM_read_brownout_reset_counter();
extern void EEPROM_acc_brownout_reset_counter();
//
extern int16_t EEPROM_read_watchdog_timeout_counter();
extern void EEPROM_acc_watchdog_timeout_counter();
extern void EEPROM_set_software_reset_flag();
extern void EEPROM_clear_software_reset_flag();
extern bool EEPROM_get_software_reset_flag();

extern saved_eeprom_data_t Saved_EEPROM_data;

extern int8_t EEPROM_read( saved_eeprom_data_t *data );
extern void EEPROM_write( const saved_eeprom_data_t *data );


//
//リモコンコードの対応を得る
//
inline IRCODE_t get_ircode_mapping( const union irr_necformat_or_uint32_t *ircode )
{
    for (uint8_t idx=0 ; idx<IRCODE_total_num ; idx++) {
        if (ircode->codebit32 == Saved_EEPROM_data.ircodes[idx].codebit32) {
            return idx;
        }
    }

    //その他
    return IRCODE_not_found;
}


#endif /* STORE_EEPROM_H_ */

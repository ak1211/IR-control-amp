/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * setup_menu.c
 * セットアップメニューのユーザーインターフェース部分を担当します
 *
 * Created: 2015/12/21 18:48:16
 *  Author: akihiro
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "ApplicationIncl.h"
#include "IR_receiver.h"
#include "electric_volume.h"
#include "audio_level_meter.h"
#include "display.h"
#include "store_EEPROM.h"
#include "setup_menu.h"

#if DISPLAY_WIDTH == 16
//空白行
//                                          1234567890123456
static const char WhiteSpaceFilledLine[] = "                ";
#else
#error "DISPLAY_WIDTH must be 16 characters"
#endif

//
//ABOUTメニューのメッセージ
//プログラムメモリへ配置する
//
static const char about_menu_message[] PROGMEM = {
//   1234567890123456
    "firmware Ver1.00 for IR remote control headphone amplifier main board"
    " "
    "Rev:A2.0 or above."
//   1234567890123456
    "                "
};

//
//LICENSEメニューのメッセージ
//プログラムメモリへ配置する
//
static const char license_menu_message[] PROGMEM = {
//   1234567890123456
    " The MIT License (MIT)"
//   12345678
    "        "
    "Copyright (c) 2015 Akihiro Yamamoto."
//   12345678
    "        "
    "Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, "
    "including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, "
    "subject to the following conditions:"
//   12345678
    "        "
    "The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software."
//   12345678
    "        "
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. "
    "IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE."
//   1234567890123456
    "                "
};
//メニューのスクロール時間
enum { MESSAGE_SCROLL_WAIT = 6, };

//
//受信コードをlineに書き込む
//
static void sprint_IR_code( display_line_buf_t line, bool show_action, const IRR_frame_t *frame )
{
    //リモコンコードそれぞれの対応
    static const char *message_map[IRCODE_total_num] = {
        [IRCODE_onoff]          = "ON/OFF",
        [IRCODE_switch_source]  = "SwSrc",
        [IRCODE_volume_up]      = "VOL +",
        [IRCODE_volume_down]    = "VOL -",
        [IRCODE_mute]           = "Mute",
        [IRCODE_omit_display]   = "OmitDisp",
    };
    char buf[DISPLAY_WIDTH + 1];
    const char *p = "";

    if (frame->type != FRAME) {
        return;
    }

    if (show_action) {
        //対応を表示する
        IRCODE_t code = get_ircode_mapping(&frame->u);
        if (code == IRCODE_not_found) {
            p = "UNUSED";
        } else {
            p = message_map[code];
        }
    }

    //受信コードを表示する
    snprintf_P (buf, sizeof(buf), PSTR("%8s %02x %02x"),  p, frame->u.ir.custom_code, frame->u.ir.data_code);
    strncpy (line, buf, DISPLAY_WIDTH );
}

//何もしないフック関数
static void nop_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    //nothing to do
}
//何もせずに現在の状態を継続するフック関数
static bool nop_and_continue_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    //nothing to do
    return true;    //現在の状態を継続する
}
//何もしないが現在の状態から抜けるフック関数
static bool nop_and_break_reset_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    //nothing to do
    return false;   //現在の状態から抜ける
}

//
//赤外線信号登録メニュー
//
//フック関数に渡すワークスペース
struct workspace_for_IR_hook_t {
            IRR_frame_t                     frame;
    union   irr_necformat_or_uint32_t*      register_target_ircode_ptr; //登録対象ポインタ
};
static struct workspace_for_IR_hook_t workspace_for_IR_hook[1];
//
static void learn_IR_sub_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static bool learn_IR_sub_menu_OK_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
//コンストラクタ
static void learn_IR_sub_menu_constructor( void *initializer )
{
    //ワークスペースを初期化
    workspace_for_IR_hook->frame.type                   = ERROR;
    workspace_for_IR_hook->frame.u.codebit32            = 0;
    workspace_for_IR_hook->register_target_ircode_ptr   = (union irr_necformat_or_uint32_t*)initializer;
}
//
static const setup_menu_t learn_IR_sub_menu = {
    learn_IR_sub_menu_constructor,  //コンストラクタ関数
    {
        { WhiteSpaceFilledLine, NULL, nop_onFocus_hook, nop_and_continue_onKeyUp_hook },                //ガード
        // 1234567890123456
        {           "Cancel", workspace_for_IR_hook, learn_IR_sub_menu_onFocus_hook, nop_and_break_reset_onKeyUp_hook },
        {               "OK", workspace_for_IR_hook, learn_IR_sub_menu_onFocus_hook, learn_IR_sub_menu_OK_onKeyUp_hook },
        // 1234567890123456
        { WhiteSpaceFilledLine, NULL, nop_onFocus_hook, nop_and_continue_onKeyUp_hook },                //ガード
    }
};



//
//赤外線信号登録メニューonFocusフック関数
//
static void learn_IR_sub_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    struct workspace_for_IR_hook_t *workspace = detail;

    //                                                 1234567890123456
    strncpy_P (display_buf[target_linenum].line, PSTR("push remocon btn"), DISPLAY_WIDTH );

    if (workspace == NULL) {
        return;
    }
    //
    //赤外線リモコン信号を確認した
    //
    if (IRR_getCaptureCondition () == IRR_CAPTURE_DONE) {
        IRR_receive_block (&workspace->frame, true);
    }
    //
    //リモコンコードの確認
    //
    if (workspace->frame.type == FRAME) {
        //リモコンコードを表示する
        sprint_IR_code( display_buf[target_linenum].line, true, &workspace->frame );
    }
}

//
//赤外線信号登録OKメニューonKeyUpフック関数
//
static bool learn_IR_sub_menu_OK_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    //更新する
    struct workspace_for_IR_hook_t *workspace = detail;

    if (workspace == NULL) {
        return false;   //現在の状態から抜ける
    }
    
    //登録対象ポインタに書き込む
    if (workspace->register_target_ircode_ptr != NULL) {
        *workspace->register_target_ircode_ptr = workspace->frame.u;
    }

    return false;   //現在の状態から抜ける
}





//
//ボリューム選択メニュー
//
static bool volume_set_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
//コンストラクタ
static void volume_setup_menu_constructor( void *initializer )
{
    //nothing to do
}
//
static const setup_menu_t volume_setup_menu = {
    volume_setup_menu_constructor,  //コンストラクタ
    {
        { WhiteSpaceFilledLine,  NULL, nop_onFocus_hook, nop_and_continue_onKeyUp_hook },               //ガード
        // 1234567890123456
        {                "0", "\x00", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "1", "\x01", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "2", "\x02", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "3", "\x03", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "4", "\x04", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "5", "\x05", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "6", "\x06", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "7", "\x07", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "8", "\x08", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {                "9", "\x09", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "10", "\x0a", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "11", "\x0b", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "12", "\x0c", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "13", "\x0d", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "14", "\x0e", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "15", "\x0f", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "16", "\x10", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "17", "\x11", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "18", "\x12", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "19", "\x13", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "20", "\x14", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "21", "\x15", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "22", "\x16", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "23", "\x17", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "24", "\x18", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "25", "\x19", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "26", "\x1a", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "27", "\x1b", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "28", "\x1c", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "29", "\x1d", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "30", "\x1e", nop_onFocus_hook, volume_set_onKeyUp_hook },
        {               "31", "\x1f", nop_onFocus_hook, volume_set_onKeyUp_hook },
        // 1234567890123456
        { WhiteSpaceFilledLine,  NULL, nop_onFocus_hook, nop_and_continue_onKeyUp_hook },               //ガード
    }
};

//
//ボリュームの値から対応するメニュー番号を得る
//
static uint8_t get_volume_menu_cursor_pos( elevol_t volume )
{
    volume = elevol_validate_volume (volume);
    int16_t number = volume + 1;        //ガードの分ずらす
    return number;
}

//
//ボリューム選択メニューonKeyUpフック関数
//
static bool volume_set_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    char *codestr = detail;
    //設定する値を文字コードで入れておいたので、文字コードをそのまま値として取り出す
    uint8_t vol = codestr[0];
    //設定する
    Saved_EEPROM_data.init_volume = elevol_validate_volume (vol);
    return false;   //現在の状態から抜ける
}



//
//最上位セットアップメニュー
//
//フック関数に渡すワークスペース
struct workspace_for_messages_menu_hook_t {
    const char* PROGMEM message;    //表示するメッセージ(プログラムメモリーに配置されているもの)
    uint16_t            counter;    //文字位置カウンタ
    int16_t             remain;     //残りカウント
};
static struct workspace_for_messages_menu_hook_t workspace_for_about_menu_hook   = {about_menu_message, 0, MESSAGE_SCROLL_WAIT};
static struct workspace_for_messages_menu_hook_t workspace_for_license_menu_hook = {license_menu_message, 0, MESSAGE_SCROLL_WAIT};
//
static void messages_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static bool messages_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static void bor_count_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static void wdt_count_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static void init_vol_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static bool init_vol_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static bool check_IR_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static void learn_IR_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static bool learn_IR_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static void save_and_exit_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static bool save_and_exit_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
static void revert_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
//コンストラクタ
static void top_setup_menu_constructor( void *initializer )
{
    //IRメニュー用ワークスペースを初期化
    workspace_for_IR_hook->frame.type                   = ERROR;
    workspace_for_IR_hook->frame.u.codebit32            = 0;
    workspace_for_IR_hook->register_target_ircode_ptr   = NULL;
}
//
const setup_menu_t top_setup_menu = {
    top_setup_menu_constructor,     //コンストラクタ,
    {
        { WhiteSpaceFilledLine, NULL, nop_onFocus_hook, nop_and_continue_onKeyUp_hook },                //ガード
        // 1234567890123456
        {            "ABOUT", &workspace_for_about_menu_hook,                   messages_menu_onFocus_hook,         messages_menu_onKeyUp_hook },
        {          "license", &workspace_for_license_menu_hook,                 messages_menu_onFocus_hook,         messages_menu_onKeyUp_hook },
        {         "PWR fail", NULL,                                             bor_count_menu_onFocus_hook,        nop_and_continue_onKeyUp_hook },
        {      "system down", NULL,                                             wdt_count_menu_onFocus_hook,        nop_and_continue_onKeyUp_hook },
        {         "Init vol", NULL,                                             init_vol_menu_onFocus_hook,         init_vol_menu_onKeyUp_hook },
        {         "check IR", workspace_for_IR_hook,                            learn_IR_sub_menu_onFocus_hook,     check_IR_menu_onKeyUp_hook },
        {           "ON/OFF", &Saved_EEPROM_data.ircodes[IRCODE_onoff],         learn_IR_menu_onFocus_hook,         learn_IR_menu_onKeyUp_hook },
        {            "SwSrc", &Saved_EEPROM_data.ircodes[IRCODE_switch_source], learn_IR_menu_onFocus_hook,         learn_IR_menu_onKeyUp_hook },
        {            "Vol +", &Saved_EEPROM_data.ircodes[IRCODE_volume_up],     learn_IR_menu_onFocus_hook,         learn_IR_menu_onKeyUp_hook },
        {            "Vol -", &Saved_EEPROM_data.ircodes[IRCODE_volume_down],   learn_IR_menu_onFocus_hook,         learn_IR_menu_onKeyUp_hook },
        {             "MUTE", &Saved_EEPROM_data.ircodes[IRCODE_mute],          learn_IR_menu_onFocus_hook,         learn_IR_menu_onKeyUp_hook },
        {         "OmitDisp", &Saved_EEPROM_data.ircodes[IRCODE_omit_display],  learn_IR_menu_onFocus_hook,         learn_IR_menu_onKeyUp_hook },
        {           "Revert", NULL,                                             revert_menu_onFocus_hook,           nop_and_break_reset_onKeyUp_hook },
        {        "Save&Exit", NULL,                                             save_and_exit_menu_onFocus_hook,    save_and_exit_menu_onKeyUp_hook },
        // 1234567890123456
        { WhiteSpaceFilledLine, NULL, nop_onFocus_hook, nop_and_continue_onKeyUp_hook },                //ガード
    },
};

//
//ABOUT,licenseメニューonFocusフック関数
//
static void messages_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    struct workspace_for_messages_menu_hook_t *workspace = (struct workspace_for_messages_menu_hook_t *)detail;
    int16_t delta;

    if (workspace == NULL) {
        return;
    }
    //１文字目
    delta = workspace->counter - DISPLAY_WIDTH;
    if (delta < 0) {
        //最初は表示行幅一杯になるまで表示する
        delta = 0;  
    }
    //スクロール表示
    strncpy_P( display_buf[target_linenum].line, &workspace->message[delta], DISPLAY_WIDTH );
    if (pgm_read_byte (&workspace->message[workspace->counter]) == '\0') {
        //始めに戻る
        workspace->counter = 0;
        workspace->remain  = MESSAGE_SCROLL_WAIT;
    }
    ///
    if (--workspace->remain < 0) {
        workspace->counter++;
        workspace->remain = MESSAGE_SCROLL_WAIT;
    }

}
//
//ABOUT,licenceメニューonKeyUpフック関数
//
static bool messages_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    struct workspace_for_messages_menu_hook_t *workspace = (struct workspace_for_messages_menu_hook_t *)detail;

    if (workspace != NULL) {
        workspace->counter = 0;
        workspace->remain = MESSAGE_SCROLL_WAIT;
    }

    return true;    //現在の状態を継続する
}

//
//wdt countメニューonFocusフック関数
//
static void wdt_count_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    char buf[DISPLAY_WIDTH + 1];

    //                                  1234567890123456
    snprintf_P (buf, sizeof(buf), PSTR("    %5d times."),  EEPROM_read_watchdog_timeout_counter() );
    strncpy (display_buf[target_linenum].line, buf, DISPLAY_WIDTH );
}

//
//bor countメニューonFocusフック関数
//
static void bor_count_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    char buf[DISPLAY_WIDTH + 1];

    //                                  1234567890123456
    snprintf_P (buf, sizeof(buf), PSTR("    %5d times."),  EEPROM_read_brownout_reset_counter() );
    strncpy (display_buf[target_linenum].line, buf, DISPLAY_WIDTH );
}

//
//init volメニューonFocusフック関数
//
static void init_vol_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    char buf[DISPLAY_WIDTH + 1];

    //                                  1234567890123456
    snprintf_P (buf, sizeof(buf), PSTR("   volume : %2d "), Saved_EEPROM_data.init_volume);
    strncpy (display_buf[target_linenum].line, buf, DISPLAY_WIDTH );
}

//
//init volメニューonKeyUpフック関数
//
static bool init_vol_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    //再帰的に呼びだす
    system_setup( display_buf, target_linenum, &volume_setup_menu, detail, get_volume_menu_cursor_pos(Saved_EEPROM_data.init_volume) );
    return true;    //現在の状態を継続する
}

//
//赤外線信号チェックメニューonKeyUpフック関数
//
static bool check_IR_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    struct workspace_for_IR_hook_t *workspace = detail;

    //読み取ったデータを消す
    if (workspace != NULL) {
        workspace->frame.type           = ERROR;
        workspace->frame.u.codebit32    = 0;
    }
    return true;    //現在の状態を継続する
}

//
//赤外線信号登録メニューonFocusフック関数
//
static void learn_IR_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    IRR_frame_t fr;
    fr.type = FRAME;
    fr.u = *((union irr_necformat_or_uint32_t *)detail);

    //リモコンコードを表示する
    sprint_IR_code( display_buf[target_linenum].line, false, &fr );
}

//
//赤外線信号登録メニューonKeyUpフック関数
//
static bool learn_IR_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    //再帰的に呼びだす
    system_setup( display_buf, target_linenum, &learn_IR_sub_menu, detail, FIRST_MENU_ITEM );
    return true;    //現在の状態を継続する
}




//
//Save&ExitメニューonFocusフック関数
//
static void save_and_exit_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    //                                                 1234567890123456
    strncpy_P (display_buf[target_linenum].line, PSTR("   save settings"), DISPLAY_WIDTH );
}
//
//Save&ExitメニューonKeyUpフック関数
//
static bool save_and_exit_menu_onKeyUp_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    EEPROM_write(&Saved_EEPROM_data);
    return false;   //現在の状態から抜ける
}


//
//RevertメニューonFocusフック関数
//
static void revert_menu_onFocus_hook( display_buffer_t display_buf, uint8_t target_linenum, void* detail )
{
    //                                                 1234567890123456
    strncpy_P (display_buf[target_linenum].line, PSTR(" cancel settings"), DISPLAY_WIDTH );
}


//
//メニュー番号menu_numの有効性の検査
//
uint8_t validate_menu_item_number ( const setup_menu_t *menu, int8_t menu_num )
{
    uint8_t valid_num;

    valid_num = menu_num;
    
    if (menu_num <= FIRST_MENU_ITEM) {
        valid_num = FIRST_MENU_ITEM;
    } else {
        for (uint8_t idx=FIRST_MENU_ITEM+1 ; idx<=menu_num ; idx++) {
            if (menu->menu_items[idx].caption == WhiteSpaceFilledLine) {
                //ガード
                valid_num = idx - 1;
            }
        }
    }
    return valid_num;
}

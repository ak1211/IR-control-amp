/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * AudioAmpApplication.c
 * メインソースファイルです。
 *
 * Created: 2014/12/30 12:34:16
 *  Author: akihiro
 */

#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "ApplicationIncl.h"
#include "IR_receiver.h"
#include "electric_volume.h"
#include "i2c_display.h"
#include "audio_level_meter.h"
#include "display.h"
#include "store_EEPROM.h"
#include "setup_menu.h"

//
// システム状態
//
typedef enum {
    DEEP_SLEEPING_STATE = 0,
    NORMAL_STATE,
    PENDING_SYSTEM_POWER_ON_STATE,
    PENDING_SYSTEM_POWER_OFF_STATE,
    INFO_DISP_STATE,
    MUTE_STATE,
    OMIT_DISPLAY_STATE,
    DEBUG_DISP_STATE,
} system_state_t;

//自動状態移行の間隔
#define STATE_SHIFT_PENDING_TIME    800


//
//システム変数
//
static struct {
    volatile int8_t         renc_rotation_travel;       //ロータリーエンコーダの移動量
    volatile uint64_t       input_switch_conditions;    //入力スイッチの状態検出用
             system_state_t system_state;               //システム状態
             int16_t        system_state_remain;        //システム状態移行残りカウンタ
             uint16_t       system_state_workspace;     //システム状態ワークスペース(各状態で好きなように)
             uint8_t        selected_source_number;     //選択中の入力源番号
             elevol_t       volume;                     //電子ボリュームの設定値
             audio_level_t  audio_level;                //オーディオ信号入力レベル
} SysVal = {
    .renc_rotation_travel = 0,
    .input_switch_conditions = 0,
    .system_state = NORMAL_STATE,
    .system_state_remain = 0,
    .system_state_workspace = 0,
    .selected_source_number = 0,
    .volume = 1,
    .audio_level = {},
};

//
//状態移行
//状態移行が発生したらtrueを返す
//
static bool switch_to_system_state( system_state_t next_state )
{
    SysVal.system_state_remain      = -1;                       //ダウンカウント停止
    SysVal.system_state_workspace   =  0;

    switch (next_state) {
        case MUTE_STATE: {
            if (SysVal.system_state == MUTE_STATE) {
                //ミュート状態はトグル動作です
                next_state = NORMAL_STATE;
            }
        } break;
        case OMIT_DISPLAY_STATE: {
            if (SysVal.system_state == OMIT_DISPLAY_STATE) {
                //表示省略状態はトグル動作です
                next_state = NORMAL_STATE;
            }
        } break;
        case INFO_DISP_STATE:
        case DEBUG_DISP_STATE:
            SysVal.system_state_remain = STATE_SHIFT_PENDING_TIME;  //ダウンカウント開始
        break;
        case DEEP_SLEEPING_STATE:
        case PENDING_SYSTEM_POWER_ON_STATE:
        case PENDING_SYSTEM_POWER_OFF_STATE:
        case NORMAL_STATE:
        break;
    }

    if (SysVal.system_state != next_state) {
        SysVal.system_state = next_state;
        return true;
    } else {
        return false;
    }
}

//
//状態管理用周期関数
//状態変化が発生したらtrueを返す
//
static bool periodic_func_of_system_state()
{
    bool retval = false;
    if (SysVal.system_state_remain == 0) {
        //カウント停止
        SysVal.system_state_remain = -1;
        //状態推移用ダウンカウント満了にて状態推移
        retval = switch_to_system_state (NORMAL_STATE);
    } else if (SysVal.system_state_remain > 0) {
        //正数の時カウントダウン
        SysVal.system_state_remain--;
    } else {
        //負数の時はカウント停止なので何もしない
    }

    return retval;
}

//
//デバッグ用情報表示
//即時表示してから状態移行で
//しばらく表示したままにする。
//
static inline bool DEBUG_MSG (const display_buffer_t buf)
{
    display_message(buf);
    return switch_to_system_state (DEBUG_DISP_STATE);
}

//
//ウォッチドッグタイマのリセットループ除けの設定
//
uint8_t mcusr_mirror __attribute__ ((section (".noinit")));
void get_mcusr(void) __attribute__((naked)) __attribute__((section(".init3")));
void get_mcusr(void)
{
    mcusr_mirror = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

//
//ウォッチドッグタイマ利用のソフトウエアリセット
//
static void software_reset()
{
    display_clear();
    //ソフトウエアリセットフラグを立てておく
    EEPROM_set_software_reset_flag();
    wdt_enable(WDTO_15MS);
    do{}while(true);        //リセット待ちループ
}



//
//TIMER1割り込み開始
//このソフトウェアのステートマシンはこのタイマ割り込みに同期して動きます
//
static inline void timer1_start()
{
#if F_CPU == 8000000UL
    //
    //CTCモード (8分周 * (1+3599)) / 8MHz= 3.6ms
    //
    TCCR1A  = 0b00000000;
    TCCR1B  = 0b00001010;
    TIMSK1  = _BV(OCIE1A);  //比較Ａ割り込みを設定
    OCR1A   = 3599;
#else
#error "F_CPU is not valid"
#endif
}

//
// TIMER1割り込み終了
//
static inline void timer1_stop()
{
    TCCR1B = 0;
}

//
//ロータリーエンコーダ読み込み
//
static void capture_rotary_encoder()
{
    static uint8_t enc = 0;         //以前に呼ばれたときの値を保存する変数
    bool clockwise, anticlockwise;

    enc <<= 2;
    enc  |= (IOread_from_renc_A() <<1) | IOread_from_renc_B();

        clockwise = (enc >>2 &1) & (enc &1)  &   (enc >>3 &1) & (~enc >>1 &1);
    anticlockwise = (enc >>2 &1) & (enc &1)  &  (~enc >>3 &1) &  (enc >>1 &1);

    SysVal.renc_rotation_travel +=     clockwise;   //  時計回りで増加
    SysVal.renc_rotation_travel -= anticlockwise;   //反時計回りで減少
}

//
//num番の入力ソースへ切り替え
//
static uint8_t shift_to_num_audio_source( uint8_t num )
{
    if (num == 2) {
        SysVal.selected_source_number = 2;
        IOwrite_to_select_source_2();
        //LED表示処理
        IOwrite_to_red_green_blue_LED (0b010);  //入力２＝緑
    } else {
        SysVal.selected_source_number = 1;
        IOwrite_to_select_source_1();
        //LED表示処理
        IOwrite_to_red_green_blue_LED (0b001);  //入力１＝青
    }
    return SysVal.selected_source_number;
}

//
//次の入力ソースへ切り替え
//
static inline uint8_t shift_to_next_audio_source()
{
    return shift_to_num_audio_source( (SysVal.selected_source_number == 1) ? 2 : 1 );
}


//
//起動時アニメーションと突入電流抑制待ち
//
static void do_opening_animation_and_wait( bool isSetupMode )
{
    struct _message_text {
        char    ch;
        uint8_t pos;
    };
    static const struct _message_text default_message[] PROGMEM = {
        { 'H',  3 },
        { 'e',  5 },
        { 'l',  7 },
        { 'l',  9 },
        { 'o', 11 },
    };
    static const struct _message_text setup_mode_message[] PROGMEM = {
        { 'S',  1 },
        { 'e',  2 },
        { 't',  3 },
        { 'u',  4 },
        { 'p',  5 },
        { 'M',  7 },
        { 'o',  8 },
        { 'd',  9 },
        { 'e', 10 },
    };
    const struct _message_text  *message;
                        size_t  message_count;
    display_buffer_t buf = {};

    if (isSetupMode) {
        message         = setup_mode_message;
        message_count   = sizeof(setup_mode_message)/sizeof(setup_mode_message[0]);
    } else {
        message         = default_message;
        message_count   = sizeof(default_message)/sizeof(default_message[0]);
    }

    memset (buf[0].line, ' ', DISPLAY_WIDTH);
    memset (buf[1].line, ' ', DISPLAY_WIDTH);

    for (uint8_t idx=0 ; idx<message_count ; idx++) {
        char     ch = pgm_read_byte(&message[idx].ch);
        uint8_t pos = pgm_read_byte(&message[idx].pos);
        for (int8_t x=DISPLAY_WIDTH-2 ; x>=pos ; x--) {
            buf[0].line[x+0] = ch;
            buf[0].line[x+1] = ' ';
            display_message( buf );
        }
    }
    
    for (uint8_t idx=0 ; idx<60; idx++) {
        buf[0].line[12+0] =
        buf[0].line[12+1] =
        buf[0].line[12+2] =
        buf[0].line[12+3] = ' ';
        buf[0].line[12+(idx&3)] = '.';
        display_message( buf );
        _delay_ms(18);
    }
    buf[0].line[12+0] =
    buf[0].line[12+1] =
    buf[0].line[12+2] =
    buf[0].line[12+3] = ' ';

    for (uint8_t idx=0 ; idx<message_count ; idx++) {
        char     ch = pgm_read_byte(&message[idx].ch);
        uint8_t pos = pgm_read_byte(&message[idx].pos);
        //
        buf[0].line[pos] = ' ';
        for (int8_t x=pos ; x>=0 ; x--) {
            buf[1].line[x+0] = ch;
            buf[1].line[x+1] = ' ';
            display_message( buf );
        }
        buf[1].line[0] = ' ';
        display_message( buf );
    }
}

//
//終了時アニメーション
//
static void do_ending_animation_and_wait()
{
    //                                     1234567890123456
    static const char message[] PROGMEM = "    Goodbye.    ";
    display_buffer_t buf = {};
    int8_t x;

    memset (buf[0].line, ' ', DISPLAY_WIDTH);
    memset (buf[1].line, ' ', DISPLAY_WIDTH);

    display_clear();
    //
    //
    //
    for (x=0 ; x<=DISPLAY_WIDTH-2 ; x++) {
        buf[0].line[x+0] = pgm_read_byte(&message[x+0]);
        buf[0].line[x+1] = CHAR_LEFT_PEAK_LEVEL;
        display_message( buf );
    }
    for (uint8_t cnt=0 ; cnt<10 ; cnt++) {
        buf[0].line[x+0] = CHAR_LEFT_PEAK_LEVEL;
        display_message( buf );
        _delay_ms(120);
        buf[0].line[x+0] = ' ';
        display_message( buf );
        _delay_ms(120);
    }

    buf[0].line[x+0] = message[x+0];
    buf[1].line[x+0] = CHAR_LEFT_PEAK_LEVEL;
    display_message( buf );

    //
    //
    //
    for (x=DISPLAY_WIDTH-2 ; x>0 ; x--) {
        buf[1].line[x+0] = CHAR_LEFT_PEAK_LEVEL;
        buf[1].line[x+1] = ' ';
        display_message( buf );
    }
    buf[1].line[x+1] = ' ';

    for (int8_t period=1 ; period<7 ; period++) {
        //
        //
        //
        for (x=0+period ; x<=DISPLAY_WIDTH-2-period ; x++) {
            buf[0].line[x+0] = ' ';
            buf[0].line[x+1] = CHAR_LEFT_PEAK_LEVEL;
            display_message( buf );
        }
        buf[0].line[x+0] = ' ';
        buf[1].line[x+0] = CHAR_LEFT_PEAK_LEVEL;
        display_message( buf );
        //
        //
        //
        for (x=DISPLAY_WIDTH-2-period ; x>0+period ; x--) {
            buf[1].line[x+0] = CHAR_LEFT_PEAK_LEVEL;
            buf[1].line[x+1] = ' ';
            display_message( buf );
        }
        buf[1].line[x+1] = ' ';
    }
    display_message( buf );
}

//
// システム初期化
//
static void initialize()
{
    initialize_iopin();
    //突入電流抑制回路を有効にする。
    IOwrite_to_inrush_current_limitter_enable();
    //アナログ電源OFF
    IOwrite_to_analog_section_power_off();

    //オーディオレベルメーター初期化
    SysVal.audio_level = (audio_level_t){};

    i2c_display_init();

    //ADC
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1);
    ADMUX = 0x00;

    //
    //EEPROMより読み込む
    //
    EEPROM_read(&Saved_EEPROM_data);
    //EEPROMのデータを使う
    SysVal.volume = Saved_EEPROM_data.init_volume;

    //入力セレクタを設定
    shift_to_num_audio_source (SysVal.selected_source_number);

    //電子ボリュームを設定
    elevol_prepare ();
    SysVal.volume = elevol_setvolume (SysVal.volume);

    SysVal.system_state = NORMAL_STATE;
    switch_to_system_state(NORMAL_STATE);

    SysVal.renc_rotation_travel = 0;

    IRR_prepare();

    //
    // LレベルINT0割り込み
    //
    EICRA &= ~((1<<ISC01) | (1<<ISC00));

    //INT1の割り込み許可
    EIMSK |= (1<<INT1);
    //INT0の割り込み禁止
    EIMSK &= ~(1<<INT0);
}

//
// システム＆アンプ電源ON
//
static void system_power_on( bool isSetupMode )
{
    //INT0, INT1の割り込み禁止
    EIMSK &= ~((1<<INT1)|(1<<INT0));

    //突入電流抑制回路を有効にする。
    IOwrite_to_inrush_current_limitter_enable();
    //電源ON前にメインアンプをmute/standby状態にする
    //ポップノイズ防止のため
    IOwrite_to_main_amp_mute_mode_enable ();
    IOwrite_to_main_amp_stanby_mode_enable ();

    switch_to_system_state (INFO_DISP_STATE);

    display_prepare();
    //アナログ回路電源ON
    IOwrite_to_analog_section_power_on();
    _delay_ms(200);
    //突入電流抑制待ち時間中のアニメーション再生
    do_opening_animation_and_wait (isSetupMode);
    //突入電流抑制回路を無効にする。
    IOwrite_to_inrush_current_limitter_disable();

    elevol_prepare();

    //以前の状況に復帰
    SysVal.selected_source_number =
        shift_to_num_audio_source (SysVal.selected_source_number);
    SysVal.volume = elevol_setvolume(SysVal.volume);

    //メインアンプのmute/standby状態を解除する
    IOwrite_to_main_amp_stanby_mode_disable ();
    IOwrite_to_main_amp_mute_mode_disable ();

    //TIMER1起動
    timer1_start();

    //INT1の割り込み許可
    EIMSK |= (1<<INT1);
    //INT0の割り込み禁止
    EIMSK &= ~(1<<INT0);
}

//
// システム＆アンプ電源OFF
//
static void system_power_off()
{
    //TIMER1停止
    timer1_stop();

    //INT0, INT1の割り込み禁止
    EIMSK &= ~((1<<INT1)|(1<<INT0));

    //電源OFF前にアンプをmute/standby状態にする
    //ポップノイズ防止のため
    IOwrite_to_main_amp_mute_mode_enable ();
    IOwrite_to_main_amp_stanby_mode_enable ();

    do_ending_animation_and_wait();

    //アナログ回路電源OFF
    IOwrite_to_analog_section_power_off();

    elevol_power_save();
    display_power_save ();

    //アンプソースを強制的にチャネル１へ(リレーコイルの励磁電流を切る)
    IOwrite_to_select_source_1();

    //復帰時に余計な変数をクリア
    SysVal.renc_rotation_travel = 0;
    SysVal.input_switch_conditions = 0;
    //
    //リモコンでONするためにここで初期化する
    //
    IRR_prepare();

    switch_to_system_state (DEEP_SLEEPING_STATE);

    //LEDも省エネのためにOFF
    IOwrite_to_red_green_blue_LED(0b000);
    //フォトカプラも省エネのためにOFF
    IOwrite_to_main_amp_stanby_mode_pwrsave();
    IOwrite_to_main_amp_mute_mode_pwrsave();

    //
    //割り込みに使用するピンがHレベルになるまで待つ
    //
    do{}while (IOread_from_input_switch() | IOread_from_IR_receiver());

    //割り込みでスリープ解除のため
    //INT0の割り込み許可
    EIMSK |= (1<<INT0);
    //INT1の割り込み許可
    EIMSK |= (1<<INT1);
    //
    //スリープ
    //スタンバイモード(クロック発振を止めないモード)を設定
    //
    set_sleep_mode(SLEEP_MODE_STANDBY);
    sleep_mode();
}

//
//タイマ1割り込みベクタ
//
ISR(TIMER1_COMPA_vect)
{
    static uint8_t division_counter = 0;    //分周用
    
    //
    //オーディオレベル取り込み
    //
    periodic_capture_audio_level (&SysVal.audio_level);

    //ロータリーエンコーダ読み込み
    capture_rotary_encoder();
    //分周カウンタのカウント
    division_counter = (division_counter + 1) & 15; // 1/16分周
    //分周カウンタのカウント満了毎で読み込む
    if (division_counter == 0) {
        //入力スイッチ読み込み
        SysVal.input_switch_conditions = (SysVal.input_switch_conditions << 1) | (IOread_from_input_switch() & 1);
    }
}

//
//INT0割り込みベクタ
//入力スイッチ押下げ割り込みはスリープ復帰のトリガー
//
ISR(INT0_vect)
{
    IOwrite_to_red_green_blue_LED(0b100);
    //スイッチが離されるまで待つ
    do{}while (IOread_from_input_switch());
    switch_to_system_state (PENDING_SYSTEM_POWER_ON_STATE);

    //INT0の割り込み禁止
    EIMSK &= ~(1<<INT0);
}

//
//リモコン入力に反応する
//状態移行が発生したらtrueを返す
//
static bool react_to_remocon_input( IRR_frame_t *frame )
{
    bool retval = false;
    
    if (frame->type == ERROR) {
        return false;
    }

    //リモコンコードの確認
    switch (get_ircode_mapping(&frame->u))
    {
        //電源ON/OFF
        case IRCODE_onoff: {
            retval = switch_to_system_state (PENDING_SYSTEM_POWER_OFF_STATE);
            //repeat無効にする
            frame->u.ir.data_code = 0x00;
        } break;
        //入力切替
        case IRCODE_switch_source: {
            shift_to_next_audio_source();
            //repeat無効にする
            frame->u.ir.data_code = 0x00;
            //変化を表示する
            retval = switch_to_system_state (INFO_DISP_STATE);
        } break;
        //音量＋
        case IRCODE_volume_up: {
            SysVal.volume = elevol_setvolume(SysVal.volume+1);
            //変化を表示する
            retval = switch_to_system_state (INFO_DISP_STATE);
        } break;
        //音量ー
        case IRCODE_volume_down: {
            SysVal.volume = elevol_setvolume(SysVal.volume-1);
            //変化を表示する
            retval = switch_to_system_state (INFO_DISP_STATE);
        } break;
        //ミュート
        case IRCODE_mute: {
            retval = switch_to_system_state (MUTE_STATE);
            //repeat無効にする
            frame->u.ir.data_code = 0x00;
        } break;
        //表示省略
        case IRCODE_omit_display: {
            retval = switch_to_system_state (OMIT_DISPLAY_STATE);
            //repeat無効にする
            frame->u.ir.data_code = 0x00;
        } break;
        //その他
        case IRCODE_total_num :
        case IRCODE_not_found : break;
    }

    return retval;
}


//
//エンコーダ移動＆入力スイッチに反応する
//状態移行が発生したらtrueを返す
//
static bool react_to_rotenc_sw_input()
{
    bool retval = false;

    //エンコーダ入力の処理  
    if (SysVal.renc_rotation_travel != 0) {
        //ボリュームの変更
        SysVal.volume = elevol_setvolume(SysVal.volume + SysVal.renc_rotation_travel);
        //入力を受け付けたのでクリアする
        SysVal.renc_rotation_travel = 0;
        //変化を表示する
        retval = switch_to_system_state (INFO_DISP_STATE);
    }

    if (0 == SysVal.input_switch_conditions) {
        //
        //入力スイッチが押されていない、つまり何もしない
        //
    } else if (0x100 <= SysVal.input_switch_conditions) {
        //
        //入力スイッチの長押しを検出した
        //
        IOwrite_to_red_green_blue_LED(0b100);
        //
        if (0x1000000000000000 <= SysVal.input_switch_conditions) {
            //
            //長押し検出 -> システムリセット
            //
            software_reset();
        } else if ((SysVal.input_switch_conditions & 1) == 0) {
            //
            //長押し＆戻し検出 -> 電源を切る
            //
            retval = switch_to_system_state (PENDING_SYSTEM_POWER_OFF_STATE);
        }
    } else if ((SysVal.input_switch_conditions & 1) == 0) {
        //
        //入力スイッチの押し下げ後、離されたのを検出したので、入力切替する
        //
        //入力を受け付けたのでクリアする
        SysVal.input_switch_conditions = 0;
        //入力切替する
        shift_to_next_audio_source();
        //変化を表示する
        retval = switch_to_system_state (INFO_DISP_STATE);
    }

    return retval;
}

//
//このリモコン入力は電源ボタンか？
//ブロッキング関数です
//
static inline bool is_power_onoff_remocon_frame_block( IRR_frame_t *frame )
{
    //ブロッキングしてリモコン信号を得る
    IRR_receive_block (frame, false);
    //比較用電源リモコンフレーム
    IRR_frame_t power_onoff = {
        .type           = FRAME,
        .u.codebit32    = Saved_EEPROM_data.ircodes[IRCODE_onoff].codebit32,
    };
#define IRR_frame_compare(_x,_y) ((_x).type == (_y).type && (_x).u.codebit32 == (_y).u.codebit32)
    //電源ボタンか？
    return (IRR_frame_compare (*frame, power_onoff)) ? true : false;
#undef IRR_frame_compare
}




//
//セットアップメニュー
//
static const setup_menu_item_t *render_setup_menu( display_line_buf_t line_buf, int8_t cursor_pos, const setup_menu_t *menu )
{
    const char *caption;
    uint8_t length;
    int8_t start, end;

    //領域検査
    cursor_pos = validate_menu_item_number( menu, cursor_pos );
    //
    //ディスプレイ表示処理
    //
    const setup_menu_item_t *menu_cursor_prev = &menu->menu_items[cursor_pos-1];    //直前のメニュー
    const setup_menu_item_t *menu_cursor_pos  = &menu->menu_items[cursor_pos];      //現在選択中のメニュー
    const setup_menu_item_t *menu_cursor_next = &menu->menu_items[cursor_pos+1];    //直後のメニュー
    memset (line_buf, ' ', DISPLAY_WIDTH);

    //
    //直前のメニューを表示する
    caption = menu_cursor_prev->caption;
    length = strlen (caption);
    //      ディスプレイ中心   空白 左カーソル           選択中の見出し             右カーソル  空白    左見出しの幅
    start = DISPLAY_WIDTH/2 - (1 +    1     + strlen(menu_cursor_pos->caption) +    1     +  1)/2 - length;
    end = start + length;
    for (int8_t idx=start ; idx<=end ; idx++) {
        if ((0 <= idx)&&(idx < DISPLAY_WIDTH)) {    //表示範囲外を無視する
            line_buf[idx] = *caption;
        }
        caption++;
    }
    //メニュー間の空白
    if ((0 <= end)&&(end < DISPLAY_WIDTH)) {    //表示範囲外を無視する
        line_buf[end++] = ' ';
    }
    //選択中のメニューに左カーソルを表示する
    if ((0 <= end)&&(end < DISPLAY_WIDTH)) {    //表示範囲外を無視する
        line_buf[end++] = '<';
    }
    //
    //カーソルに選択されたメニューを中心に表示する
    caption = menu_cursor_pos->caption;
    length = strlen (caption);
    start = end;
    end = start + length;
    //
    for (int8_t idx=start ; idx<=end ; idx++) {
        if ((0 <= idx)&&(idx < DISPLAY_WIDTH)) {    //表示範囲外を無視する
            line_buf[idx] = *caption;
        }
        caption++;
    }
    //選択中のメニューに右カーソルを表示する
    if ((0 <= end)&&(end < DISPLAY_WIDTH)) {    //表示範囲外を無視する
        line_buf[end++] = '>';
    }
    //メニュー間の空白
    if ((0 <= end)&&(end < DISPLAY_WIDTH)) {    //表示範囲外を無視する
        line_buf[end++] = ' ';
    }
    //
    //直後のメニューを表示する
    caption = menu_cursor_next->caption;
    length = strlen (caption);
    start = end;
    end = start + length;
    for (int8_t idx=start ; idx<=end ; idx++) {
        if ((0 <= idx)&&(idx < DISPLAY_WIDTH)) {    //表示範囲外を無視する
            line_buf[idx] = *caption;
        }
        caption++;
    }

    //現在選択中のメニューを返す
    return &menu->menu_items[cursor_pos];
}

//
//システム設定ユーザインターフェース
//外部より再帰的に呼ばれる関数です
//
void system_setup( display_buffer_t display_buf, uint8_t target_linenum, const setup_menu_t *menu, void *initializer, int8_t cursor_pos )
{
#if DISPLAY_LINES == 2
    const uint8_t next_target_linenum = (target_linenum + 1) & 1;
#else
#error "DISPLAY_LINES must be 2 lines"
#endif
    bool isContinue;    //再帰呼び出し状態の継続または脱出フラグ

    //コンストラクタ呼び出し
    menu->constructor(initializer);

    isContinue = true;
    do {
        //メニュー処理
        const setup_menu_item_t *menu_cursor_pos = render_setup_menu( display_buf[target_linenum].line, cursor_pos, menu );
        //選択中のメニューが返ってきているので
        //onFocusフック関数を呼び出す
        //選択中なら繰り返し呼び出される
        menu_cursor_pos->onFocus_hook( display_buf, next_target_linenum, menu_cursor_pos->detail );
        //
        //エンコーダ入力に反応する
        //
        if (SysVal.renc_rotation_travel != 0) {
            //領域検査
            cursor_pos = validate_menu_item_number( menu, cursor_pos+SysVal.renc_rotation_travel );
            //入力を受け付けたので０クリア
            SysVal.renc_rotation_travel = 0;
        }
        //入力スイッチの押し下げ、戻し検出
        if((SysVal.input_switch_conditions != 0) && ((SysVal.input_switch_conditions & 1) == 0) ) {
            //入力を受け付けたので
            SysVal.input_switch_conditions = 0;
            //選択中のメニューのonKeyUpフック関数を呼び出す
            //trueが返って来たらループを継続、falseならループから抜ける
            isContinue = menu_cursor_pos->onKeyUp_hook( display_buf, next_target_linenum, menu_cursor_pos->detail );
        }
        //表示
        display_message( display_buf );
        //
        //アイドルモードでスリープ
        //赤外線リモコン割り込み, タイマ割り込み等があるまでスリープ
        //
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_mode();
    } while (isContinue);   //継続フラグがtrueなら現在の状態を継続する
}

//
//外部入力に反応する
//状態移行が発生したらtrueを返す
//
static bool react_to_external_input()
{
    static IRR_frame_t frame = {0};
    bool retval = false;

    //
    //エンコーダ入力に反応する
    //
    retval = react_to_rotenc_sw_input ();
    //
    //赤外線リモコン信号があれば反応する
    //
    if (IRR_getCaptureCondition () == IRR_CAPTURE_DONE) {
        IRR_receive_nonblock (&frame, true);
        retval = react_to_remocon_input (&frame);
    }

    return retval;
}

//
//エントリポイント
//
int main()
{
    bool isSetupMode = false;

    //初期化
    initialize ();

    //
    //リセット要因の確認をする
    //
    if (bit_is_set(mcusr_mirror, BORF)) {
        //ブラウンアウトリセットであった
        EEPROM_acc_brownout_reset_counter();
    }
    if (bit_is_set(mcusr_mirror, WDRF)) {
        //ウォッチドッグ・システム・リセットであった
        if (!EEPROM_get_software_reset_flag()) {
            //ウォッチドッグ・タイムアウトリセットカウンタを加算する
            EEPROM_acc_watchdog_timeout_counter();
        }
    }
    //ソフトウエアリセットフラグをクリアしておく
    EEPROM_clear_software_reset_flag();
    //
    //ここで(リセット直後)入力スイッチが押されたままであったら、
    //システム設定に移行する
    //
    if (IOread_from_input_switch()) {
        isSetupMode = true;
    }

    //電源投入時は初期化後にON状態に移行
    system_power_on(isSetupMode);

    sei();  //割り込み開始

    if (isSetupMode) {
        //
        //システム設定
        //
        display_buffer_t display_buf;
        memset (display_buf, ' ', sizeof(display_buffer_t));
        //システム設定へ
        system_setup( display_buf, 0, &top_setup_menu, NULL, FIRST_MENU_ITEM );
        //ここでリセット
        software_reset();
    }

    //
    //ウォッチドッグタイマーを有効にする
    //
#define WDT_ENABLE()    wdt_enable(WDTO_30MS)
    WDT_ENABLE();
    //
    //メインループ
    //
    for(;;) {
        //状態推移用周期関数呼び出し
        periodic_func_of_system_state();
        //
        //ステートマシンの状態によって
        //それぞれの処理をする
        //
        switch (SysVal.system_state)
        {
            //
            case DEEP_SLEEPING_STATE: {
                //
                //システムOFF中の呼び出し
                //入力スイッチ、赤外線リモコンのLレベル割り込みでスリープからの復帰をするが、
                //入力スイッチ割り込みはその割り込みハンドラ内でPOWER ON待機状態に移行するために
                //赤外線リモコン割り込み発生時のみここに来る。
                //
                IRR_frame_t frame = {0};
                IOwrite_to_red_green_blue_LED(0b100);
                if (is_power_onoff_remocon_frame_block (&frame)) {
                    switch_to_system_state (PENDING_SYSTEM_POWER_ON_STATE);
                } else {
                    IOwrite_to_red_green_blue_LED(0b000);
                    _delay_ms(1);
                    if (IRR_getCaptureCondition() == IRR_CAPTURE_IDLE) {
                        switch_to_system_state (PENDING_SYSTEM_POWER_OFF_STATE);
                    }
                }
            } continue; //ループの先頭へ
            //
            case PENDING_SYSTEM_POWER_ON_STATE: {
                //
                //system power ON待機状態のためにここでPOWER ON
                //
                system_power_on(false);
                //
                //スリープ復帰後ウォッチドッグタイマーを有効にする
                //
                WDT_ENABLE();
            } continue; //ループの先頭へ
            //
            case PENDING_SYSTEM_POWER_OFF_STATE: {
                //
                //スリープ中はウォッチドッグタイマーを無効にする
                //
                wdt_disable();
                //
                //system power OFF待機状態のためにここでPOWER OFF
                //
                system_power_off();
            } continue; //ループの先頭へ
            //
            case MUTE_STATE: {
                //
                //ミュート状態
                //
                elevol_set_mute();
                //外部入力に反応する
                bool isShiftState = react_to_external_input();
                //情報を表示する
                SysVal.system_state_workspace = display_mute_information (SysVal.system_state_workspace, SysVal.volume);
                if (isShiftState) {
                    //状態変化前にミュート解除
                    elevol_clear_mute(SysVal.volume);
                }
            } break;
            //
            case OMIT_DISPLAY_STATE: {
                //
                //表示省略状態
                //
                //ディスプレイをOFFする
                display_power_save();
                //外部入力に反応する
                bool isShiftState = react_to_external_input();
                if (isShiftState) {
                    //
                    //時間のかかる処理なので、一旦ウォッチドッグタイマーを無効にする
                    //
                    wdt_disable();
                    //状態変化前にディスプレイをONする
                    display_prepare();
                    //
                    //ウォッチドッグタイマーを有効にする
                    //
                    WDT_ENABLE();
                }
            } break;
            //
            case DEBUG_DISP_STATE: {
                //
                //デバッグ情報表示状態
                //情報表示はすでに行われているので
                //ここでは何もしない
                //
                //外部入力に反応する
                react_to_external_input();
            } break;
            //
            case INFO_DISP_STATE: {
                //
                //情報表示状態
                //
                //外部入力に反応する
                react_to_external_input();
                //情報を表示する
                display_information (SysVal.volume);
            } break;
            //
            case NORMAL_STATE: {
                //
                //通常状態
                //
                //レベルメーターを表示する
                display_audio_level_meter (&SysVal.audio_level);
                //外部入力に反応する
                react_to_external_input();
            } break;
        }
        //
        //タスク処理終了後
        //アイドルモードでスリープ
        //赤外線リモコン割り込み、タイマ割り込み等があるまで
        //ここで停止する
        //
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_mode();
        //
        //ウォッチドッグタイマーをリセットする
        //
        wdt_reset();
    }
}

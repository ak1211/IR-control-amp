/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * setup_menu.h
 * セットアップメニューのユーザーインターフェース部分を担当します
 *
 * Created: 2015/12/21 18:48:16
 *  Author: akihiro
 */
#ifndef SETUP_MENU_H_
#define SETUP_MENU_H_

//
//セットアップメニューアイテム
//
typedef struct {
    //メニューの見出し
    const char * PROGMEM caption;
    //フック関数に渡す情報
    void    *detail;
    //onFocusイベントフック関数
    void    (*onFocus_hook)( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
    //keyUpイベントフック関数
    bool    (*onKeyUp_hook)( display_buffer_t display_buf, uint8_t target_linenum, void* detail );
} setup_menu_item_t;

//
//セットアップメニュー
//
typedef struct _setup_menu_t {
    void (*constructor)(void *initializer);     //コンストラクタ関数
    setup_menu_item_t   menu_items[];           //メニューアイテムの配列
} setup_menu_t;

//メニューアイテムの最初
#define FIRST_MENU_ITEM     1
extern const setup_menu_t top_setup_menu;

//
extern uint8_t validate_menu_item_number ( const setup_menu_t *menu, int8_t menu_num );

//
extern void system_setup( display_buffer_t display_buf, uint8_t target_linenum, const setup_menu_t *menu, void *initializer, int8_t cursor_pos );


#endif /* SETUP_MENU_H_ */

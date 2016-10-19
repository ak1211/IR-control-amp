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
 * display.h
 *
 * Created: 2015/12/13 21:05:16
 *  Author: akihiro
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#define DISPLAY_WIDTH   16
#define DISPLAY_LINES   2

typedef char display_line_buf_t[DISPLAY_WIDTH];
typedef struct {
    display_line_buf_t line;
} display_buffer_t[DISPLAY_LINES];

extern void display_message( const display_buffer_t buffer );

extern void display_information( elevol_t volume );

extern uint16_t display_mute_information( uint16_t counter, elevol_t volume );

extern void display_audio_level_meter( audio_level_t* audiolevel );

extern void display_refresh();

extern void display_clear();

extern void display_prepare();

extern void display_power_save();

#endif /* DISPLAY_H_ */

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
 * ApplicationIncl.h
 * プロジェクト定義ヘッダファイル
 *
 * Created: 2015/01/04 19:57:16
 *  Author: akihiro
 */

#include <stdint-gcc.h>

#ifndef APPLICATIONINCL_H_
#define APPLICATIONINCL_H_

/*
 * PB0          -- pin 14 -- out    -- ~INRUSH_CURRENT_LIMITTER
 * PB1          -- pin 15 -- out    -- ANALOG_PWR_GATE
 * PB2          -- pin 16 -- out    -- SELECT_SOURCE
 * PB3(MOSI)    -- pin 17 -- out    -- AVRISP mkII and EVOL_SDI EVOL_MUTE
 * PB4(MISO)    -- pin 18 -- out    -- AVRISP mkII and EVOL_SCLK
 * PB5(SCK)     -- pin 19 -- out    -- AVRISP mkII and ~EVOL_CS
 * PB6(XTAL1)   -- pin  9 -- out    --
 * PB7(XTAL2)   -- pin 10 -- out    --
 *
 * 0b1111 1111 == 0xff
 *
 *
 * PC0(ADC0)    -- pin 23 -- in     -- L ch LEVEL INPUT
 * PC1(ADC1)    -- pin 24 -- in     -- R ch LEVEL INPUT
 * PC2          -- pin 25 -- out    -- ~MAIN_AMP_STANDBY
 * PC3          -- pin 26 -- out    -- ~MAIN_AMP_MUTE
 * PC4(SDA)     -- pin 27 -- out    -- I2C_SDA
 * PC5(SCL)     -- pin 28 -- out    -- I2C_SCL
 * PC6(~RESET)  -- pin  1 -- out    -- ~RESET
 *
 * 0b1111 1100 == 0xfc
 *
 *
 * PD0          -- pin  2 -- in     -- Rotary Encoder B
 * PD1          -- pin  3 -- in     -- Rotary Encoder A
 * PD2(INT0)    -- pin  4 -- in     -- ~INPUT SWITCH
 * PD3(INT1)    -- pin  5 -- in     -- ~IR_RECEIVER
 * PD4          -- pin  6 -- out    -- ~DISPLAY_PWR
 * PD5          -- pin 11 -- out    -- LED_B
 * PD6          -- pin 12 -- out    -- LED_G
 * PD7          -- pin 13 -- out    -- LED_R
 *
 * 0b1111 0000 == 0xf0
 *
 */

inline void initialize_iopin() {
    //ポートＢ初期化
    //すべて出力
    DDRB  = 0xff;
    //すべて出力なので、内蔵プルアップ無効
    PORTB = 0x00;

    //ポートＣ初期化
    //(PC0:ADC0), (PC1:ADC1)を入力に
    DDRC  = 0xfc;
    //ADCなので内蔵プルアップ無効
    PORTC = 0x00;

    //ポートＤ初期化
    DDRD  = 0xf0;
    //(PD2:~INPUT_SWITCH),
    //(PD1:Rotary Encoder A),
    //(PD0:Rotary Encoder B)
    //内蔵プルアップを有効
    PORTD = 0x07;
}

//
// OLED power pin
//

inline bool IOread_from_oled_power_on() {
    return bit_is_clear(PORTD, PD4);    //負論理
}
inline void IOwrite_to_oled_power_on() {
    PORTD &= ~_BV(PD4); //負論理
}
inline void IOwrite_to_oled_power_off() {
    PORTD |= _BV(PD4);  //負論理
}

//
// electric volume SDI pin
//
inline void IOwrite_to_LM1972_DATA_H() {
    PORTB |= _BV(PB3);
}
inline void IOwrite_to_LM1972_DATA_L() {
    PORTB &= ~_BV(PB3);
}
inline void IOwrite_to_LM1972_DATA_pwrsave() {
    IOwrite_to_LM1972_DATA_L();         // DATAをLに、省電力のため
}

//
// electric volume clock pin
//
inline void IOwrite_to_LM1972_CLOCK_H() {
    PORTB |= _BV(PB4);
}
inline void IOwrite_to_LM1972_CLOCK_L() {
    PORTB &= ~_BV(PB4);
}
inline void IOwrite_to_LM1972_CLOCK_pwrsave() {
    IOwrite_to_LM1972_CLOCK_L();            // CLOCKをLに、省電力のため
}

//
// electric volume ~EVOL_CS pin
//
inline void IOwrite_to_LM1972_LOADSHIFT_H() {
    PORTB |= _BV(PB5);
}
inline void IOwrite_to_LM1972_LOADSHIFT_L() {
    PORTB &= ~_BV(PB5);
}
inline void IOwrite_to_LM1972_LOADSHIFT_pwrsave() {
    IOwrite_to_LM1972_LOADSHIFT_L();        // LOADSHIFTをLに、省電力のため
}


//
// main amp standby pin
//
inline void IOwrite_to_main_amp_stanby_mode_enable() {
    PORTC &= ~_BV(PC2); //負論理
}
inline void IOwrite_to_main_amp_stanby_mode_disable() {
    PORTC |= _BV(PC2);  //負論理
}
inline void IOwrite_to_main_amp_stanby_mode_pwrsave() {
    PORTC &= ~_BV(PC2); //フォトカプラをoffに、省電力のため
}
//
// main amp mute pin
//
inline void IOwrite_to_main_amp_mute_mode_enable() {
    PORTC &= ~_BV(PC3); //負論理
}
inline void IOwrite_to_main_amp_mute_mode_disable() {
    PORTC |= _BV(PC3);  //負論理
}
inline void IOwrite_to_main_amp_mute_mode_pwrsave() {
    PORTC &= ~_BV(PC3); //フォトカプラをoffに、省電力のため
}

//
// rotary encoder
//
inline bool IOread_from_renc_A() {
    return bit_is_set(PIND, PD1);
}
inline bool IOread_from_renc_B() {
    return bit_is_set(PIND, PD0);
}

//
// input switch pin
//
inline bool IOread_from_input_switch() {
    return bit_is_clear(PIND, PD2); //負論理
}

//
// infra red receiver pin
//
inline bool IOread_from_IR_receiver() {
    return bit_is_clear(PIND, PD3); //負論理
}

//
// select source pin
//
inline void IOwrite_to_select_source_1() {
    PORTB &= ~_BV(PB2);
}
inline void IOwrite_to_select_source_2() {
    PORTB |= _BV(PB2);
}
inline uint8_t IOread_from_select_source() {
    return (bit_is_clear(PORTB, PB2)) ? 1 : 2;
}

//
// analog section power pin
//
inline void IOwrite_to_analog_section_power_on() {
    PORTB |= _BV(PB1);
}
inline void IOwrite_to_analog_section_power_off() {
    PORTB &= ~_BV(PB1);
}

//
// inrush current limitter pin
//
inline void IOwrite_to_inrush_current_limitter_enable() {
    PORTB &= ~_BV(PB0); //負論理
}
inline void IOwrite_to_inrush_current_limitter_disable() {
    PORTB |= _BV(PB0);  //負論理
}

//
// blue LED pin
//
inline void IOwrite_to_blue_LED_on() {
    PORTD |= _BV(PD5);
}
inline void IOwrite_to_blue_LED_off() {
    PORTD &= ~_BV(PD5);
}
inline bool IOread_from_blue_LED() {
    return bit_is_set(PIND, PD5);
}

//
// green LED pin
//
inline void IOwrite_to_green_LED_on() {
    PORTD |= _BV(PD6);
}
inline void IOwrite_to_green_LED_off() {
    PORTD &= ~_BV(PD6);
}
inline bool IOread_from_green_LED() {
    return bit_is_set(PIND, PD6);
}

//
// red LED pin
//
inline void IOwrite_to_red_LED_on() {
    PORTD |= _BV(PD7);
}
inline void IOwrite_to_red_LED_off() {
    PORTD &= ~_BV(PD7);
}
inline bool IOread_from_red_LED() {
    return bit_is_set(PIND, PD7);
}

//
// R/G/B LED pins control
//
inline void IOwrite_to_red_green_blue_LED(uint8_t rgb) {
    if (rgb & 0x4) {
        IOwrite_to_red_LED_on();
    } else {
        IOwrite_to_red_LED_off();
    }
    if (rgb & 0x2) {
        IOwrite_to_green_LED_on();
    } else {
        IOwrite_to_green_LED_off();
    }
    if (rgb & 0x1) {
        IOwrite_to_blue_LED_on();
    } else {
        IOwrite_to_blue_LED_off();
    }
}
inline uint8_t IOread_from_red_green_blue_LED() {
    return
        IOread_from_red_LED() << 2 |
        IOread_from_green_LED() << 1 |
        IOread_from_blue_LED();
}


#endif /* APPLICATIONINCL_H_ */

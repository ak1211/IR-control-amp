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
 * audio_level_meter.h
 *
 * Created: 2015/12/14 19:18:16
 *  Author: akihiro
 */

#include <stdint-gcc.h>

#ifndef AUDIO_LEVEL_METER_H_
#define AUDIO_LEVEL_METER_H_

//
struct U16LeftRight {
    uint16_t left;
    uint16_t right;
} __attribute__((__packed__));
//
union UU16LeftRight {
            uint16_t        array[2];
    struct  U16LeftRight    u16;
};
//
struct U32LeftRight {
    uint32_t left;
    uint32_t right;
} __attribute__((__packed__));
//
union UU32LeftRight {
            uint32_t        array[2];
    struct  U32LeftRight    u32;
};

enum {
    AudioLevelQueueShiftbits        = 5,
    AudioLevelQueueSampleSize       = 32,   //2^AudioLevelQueueShiftbits
    AudioLevelDecrementInitialVal   = 16,   //デクリメントカウンタの初期値（ピーク表示を止めている時間）
};

typedef struct {
    union   UU16LeftRight   al_queue_samples[AudioLevelQueueSampleSize];    //オーディオレベルのキュー
            uint16_t        al_queue_idx;                                   //キューの次回書き込み位置
    union   UU32LeftRight   al_acc;                                         //オーディオレベルの蓄積器
    //
    union   UU32LeftRight   al_peak;                                        //ピークレベル(周期関数によって徐々に減る)
    //
            uint8_t         decrement_counter;                              //デクリメントカウンタ(周期関数が使う)
} audio_level_t;

extern void periodic_capture_audio_level( audio_level_t *al );
extern void get_audio_level( union UU16LeftRight *audio_level, union UU16LeftRight *peak_level, audio_level_t *al );

#endif /* AUDIO_LEVEL_METER_H_ */

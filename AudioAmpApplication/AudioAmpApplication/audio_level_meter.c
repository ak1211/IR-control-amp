/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * audio_level_meter.c
 * オーディオレベルメータ部分のソース
 *
 * Created: 2015/12/14 19:13:16
 *  Author: akihiro
 */ 

#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <avr/io.h>

#include "ApplicationIncl.h"
#include "audio_level_meter.h"

//
//オーディオレベル取り込み周期関数
//定期的に呼んでください
//
void periodic_capture_audio_level( audio_level_t *al )
{
    union UU16LeftRight sampled;

    //
    //オーディオレベル取り込み
    //左、右の順で取り込む
    //可能なら同時サンプリングしたいんですけどね
    //
    for (uint8_t idx=0 ; idx<2 ; idx++) {
        ADMUX = 0x40 | (idx &1);
        ADCSRA |= _BV(ADEN) | _BV(ADSC);
        ADCSRA |= _BV(ADIF);
        loop_until_bit_is_set(ADCSRA, ADIF);
        uint32_t value = ADC;
        //二乗
        sampled.array[idx] = value * value;
    }


    //指針をしばらく置いておくだけの
    //ピークメータの周期減衰処理
    union UU16LeftRight peak_level_descent = {
        .u16.left  = 0,     //左チャネルの移動量
        .u16.right = 0,     //右チャネルの移動量
    };
    if (al->decrement_counter > 0) {
        //指針停止カウント中
        al->decrement_counter--;
    } else if (al->decrement_counter == 0) {
        //カウント終了にて移動する
        al->decrement_counter = AudioLevelDecrementInitialVal;              //デクリメントカウンタを更新する
        peak_level_descent.array[0] = (al->al_peak.array[0] == 0) ? 0 : 1;  //左チャネルの移動量(0になるまで)
        peak_level_descent.array[1] = (al->al_peak.array[1] == 0) ? 0 : 1;  //右チャネルの移動量(0になるまで)
    }

    //
    //レベルメータ演算
    //
    for (uint8_t idx=0 ; idx<2 ; idx++) {
        //ピークメータの周期減衰処理
        al->al_peak.array[idx] -= peak_level_descent.array[idx];
        //取り除くべき古い値
        union UU16LeftRight stale_sample = al->al_queue_samples[al->al_queue_idx];
        //
        //レベルメータ
        //
        al->al_acc.array[idx] -= stale_sample.array[idx];                       //アキュムレータから古い値を取り除く
        al->al_acc.array[idx] += sampled.array[idx];                            //アキュムレータに蓄積する
        al->al_queue_samples[al->al_queue_idx].array[idx] = sampled.array[idx]; //リングバッファに新しい値を入れる
    }

    //リングバッファのインデクスを進める
    al->al_queue_idx = (al->al_queue_idx + 1) & (AudioLevelQueueSampleSize-1);
}

//
//レベルメータの値を得る
//
void get_audio_level( union UU16LeftRight *audio_level, union UU16LeftRight *peak_level, audio_level_t *al )
{
    for (uint8_t idx=0 ; idx<2 ; idx++) {
        //レベルメータの値はアキュムレータから実効値を得る
        uint32_t vvmean = al->al_acc.array[idx] >> AudioLevelQueueShiftbits;    //２乗平均値
        audio_level->array[idx] = floor (sqrt (vvmean));                        //二乗平均平方根
        //ピークメータの値は現在値より大きい場合に更新する
        uint32_t peak = audio_level->array[idx];
        if (al->al_peak.array[idx] < peak) {
            al->al_peak.array[idx] = peak;                          //ピークメータの値を更新する
            al->decrement_counter = AudioLevelDecrementInitialVal;  //デクリメントカウンタを更新する

        }
        peak_level->array[idx] = al->al_peak.array[idx];
    }
}

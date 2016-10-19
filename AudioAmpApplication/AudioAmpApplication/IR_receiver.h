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
 * IR_receiver.h
 *
 * Created: 2015/01/04 19:57:16
 *  Author: akihiro
 */

#ifndef IR_RECEIVER_H_
#define IR_RECEIVER_H_

//
//NECフォーマットリモコンコード型
//
struct irr_necformat_t {
    uint16_t custom_code;       //16bitカスタムコード
    uint8_t  data_code;         //8bitデータ
    uint8_t  inv_data_code;     //8bit反転データ
} __attribute__((__packed__));
//
union irr_necformat_or_uint32_t {
            uint8_t         array[4];       //配列アクセス(ＡＶＲはリトルエンディアン)
            uint32_t        codebit32;      //32ビットアクセス
    struct  irr_necformat_t ir;             //リモコンコード型アクセス
};

typedef struct {
    enum {
        ERROR,
        FRAME,
        REPEAT,
    } type;
    union irr_necformat_or_uint32_t u;
} IRR_frame_t;

typedef enum {
    IRR_CAPTURE_IDLE    = 0,
    IRR_CAPTURE_BUSY    = 1,
    IRR_CAPTURE_DONE    = 2,
} IRR_capture_condition_t;

extern void IRR_prepare();

extern IRR_capture_condition_t IRR_getCaptureCondition ();
extern void IRR_receive_nonblock( IRR_frame_t *frame, bool strict_checked );
extern void IRR_receive_block( IRR_frame_t *frame, bool strict_checked );


#endif //IR_RECEIVER_H_

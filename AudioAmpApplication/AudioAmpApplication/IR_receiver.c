/*
 * This software for IR control headphone amplifier main board Rev:A2.0 or above.
 * http://ak1211.com
 * Copyright (c) 2015 Akihiro Yamamoto

 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
/*
 * IR_receiver.c
 * 赤外線リモコンの受信、解析部分を担当します
 *
 * Created: 2015/03/24 20:06:50
 *  Author: akihiro
 */

#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "ApplicationIncl.h"
#include "IR_receiver.h"

//
//ビットベクタ, ビットの列
//
enum {
    //NECフォーマット
    //データ0 = 2T
    //データ1 = 4T
    //リーダーコード    = 16T+8T
    //カスタムコード    = 4T*16
    //データーコード    = (2T+4T)*8
    //ストップビット    = 16T
    //= 152カウント
    //タイムアウトカウント
    Timeout_CaptureCount    = 155,
    //データ領域はさらに余裕を見て 200T = 25byte 
    BitVector_Size          = 25,
};

volatile static enum {
    BitVector_Success   = 0,
    BitVector_Error,
} condition_BitVector = BitVector_Success;

volatile static uint8_t     array_BitVector[BitVector_Size] = {};
volatile static uint16_t    count_BitVector = 0;

//
//
//
volatile static IRR_capture_condition_t state_CaptureIR = IRR_CAPTURE_IDLE;
volatile static uint16_t                tickCounter_CaptureIR = 0;

//
//ビットベクタを０クリアする
//
static void BitVector_Clear()
{
    for (int8_t k=sizeof(array_BitVector)-1 ; k>=0 ; k--) {
        array_BitVector[k] = 0;
    }
    count_BitVector = 0;
    condition_BitVector = BitVector_Success;
}

//
//ビットベクタの先頭から書き込めるようにする
//
static inline void BitVector_Rewind()
{
    count_BitVector = 0;
}

//
//ビットベクタの最後に１ビットいれる
//
static void BitVector_StoreBit( uint8_t bit )
{
    uint8_t idx, bitnum, mask;

    bit = bit & 1;

    //代入対象インデックス
    idx     = count_BitVector >> 3;     //count_BitVector / 8;
    if (idx >= BitVector_Size) {
        //オーバーフロー
        condition_BitVector = BitVector_Error;
        return;
    }

    //代入対象ビット番号(MSB first)
    bitnum  = 7 - (count_BitVector & 7);
    //対象ビットのビットマスク
    mask    = ~(1 << bitnum);

    //対象ビットに代入する
    array_BitVector[idx] = (array_BitVector[idx] & mask) | (bit << bitnum);

    //カウンタを進める
    count_BitVector++;
}

//
//ビットベクタからcount位置の１ビットを取り出す
//
static uint8_t BitVector_ExtractBit( uint16_t count )
{
    uint8_t octet, bit, bitnum;
    int8_t idx;

    //抽出対象インデックス
    idx = count >> 3;       //count / 8;
    if ( idx >= BitVector_Size ) {
        //オーバーフロー
        condition_BitVector = BitVector_Error;
        return 0;
    }

    //抽出対象バイト
    octet   = array_BitVector[idx];
    //抽出対象ビット番号(MSB first)
    bitnum  = 7 - (count & 7);

    //ビット抽出
    bit = (octet >> bitnum) & 1;

    return bit;
}

//
//ビットベクタからstart_bit_count位置から始まる８ビットを取り出す
//
static uint8_t BitVector_ExtractOctet( uint16_t start_bit_count )
{
    uint8_t idx, shift_bits;
    uint16_t word, header, trailer;

    if (count_BitVector == 0) {
        //何も入っていない
        condition_BitVector = BitVector_Error;
        return 0;
    }

    //抽出対象前方インデックス
    idx = start_bit_count >> 3;     //start_bit_count / 8;
    if ( (idx+8) >= BitVector_Size ) {
        //オーバーフロー
        condition_BitVector = BitVector_Error;
        return 0;
    }

    shift_bits = start_bit_count & 7;

    header  = array_BitVector[idx+0] << (8+shift_bits);
    trailer = array_BitVector[idx+1] << shift_bits;
    word = (header | trailer) >> 8;

    //８ビット抽出
    return (word & 0x00ff);
}

//
//ビットベクタの最終１６ビットを取り出す
//
static uint16_t BitVector_ExtractLatestWord()
{
    if (count_BitVector < 16) {
        //足りていない
        condition_BitVector = BitVector_Error;
        return 0;
    }

    //抽出対象後方インデックス
    uint8_t idx = count_BitVector >> 3;     //count_BitVector / 8;

    uint8_t shift_bits = count_BitVector & 7;

    uint32_t header     = array_BitVector[idx-2];
    uint32_t body       = array_BitVector[idx-1];
    uint32_t trailer    = array_BitVector[idx+0];
    uint32_t dword = (header << 16) | (body << 8) | trailer;
    dword = dword >> shift_bits;

    //16ビット抽出
    return (dword & 0xffff);
}

//
// TIMER0割り込み開始
//
static inline void timer0_start()
{
//
// TIMER0割り込み
// CTCモード (64分周 * (1+69)) / 8MHz= 0.560ms
//
#if F_CPU ==  8000000
    TCCR0A  = 0b00000010;   //CTC mode,
    TCCR0B  = 0b00000011;   //64分周
    OCR0A   = 69;           //0.560msで割り込み
    TIMSK0  = 0b0000010;    //比較A一致割り込み有効
#else
#error "F_CPU is not valid"
#endif
}

//
// TIMER0割り込み停止
//
static inline void timer0_stop()
{
    TCCR0B = 0;
}

//
//(INT1:receive IR)割り込みベクタ
//
ISR(INT1_vect)
{
    timer0_stop();
    TCNT0 = 20;         //ビット幅560usの中間付近を読むために最初は加速
    timer0_start();

    //
    //リーダー部検出開始
    //
    //変数の初期化
    tickCounter_CaptureIR = 0;
    //
    BitVector_Rewind();
    state_CaptureIR = IRR_CAPTURE_BUSY;

    //INT1の割り込み禁止
    EIMSK &= ~(_BV(INT1));
}

//
//失敗によって受光終了
//
static inline void receive_stop_fail()
{
    timer0_stop();
    //
    BitVector_Clear();
    state_CaptureIR = IRR_CAPTURE_IDLE;
    //INT1の割り込み許可
    EIMSK |= _BV(INT1);
}

//
//成功によって受光終了
//
static inline void receive_stop_success()
{
    timer0_stop();
    //
    state_CaptureIR = IRR_CAPTURE_DONE;
}

//
//TIMER0割り込みベクタ
//
ISR(TIMER0_COMPA_vect)
{
    //ストップビットの検出
    if (count_BitVector >= 20) {    //(9ms+2.25ms)/0.556 = 20
        if (BitVector_ExtractLatestWord() == 0x8000) {
            //1ビットHレベル, 15ビットLレベル検出
            receive_stop_success();
            return;
        }
    }
    //タイムアウトの検出
    if (tickCounter_CaptureIR > Timeout_CaptureCount ) {
        receive_stop_fail();
        return;
    }

    //
    //赤外線信号を取り込む
    //
    if ((tickCounter_CaptureIR < 18) ||         //リーダーコードの長さに満たない場合
       ((array_BitVector[0]         == 0xff)    // 0から 7カウント(0.0 ->  4.5ms)はHHHH_HHHHであること
    && ((array_BitVector[1] & 0xfc) == 0xfc)    // 8から15カウント(4.5 ->  9.0ms)はHHHH_HHxxであること(x==不定)
    && ((array_BitVector[2] & 0xc0) == 0x00)))  //16から23カウント(9.0 -> 13.5ms)はLLxx_xxxxであること(x==不定)
    {
        tickCounter_CaptureIR++;
        //入力赤外線信号
        if (IOread_from_IR_receiver()) {
            BitVector_StoreBit (1);
        } else {
            BitVector_StoreBit (0);
        }
    } else {
        //
        //リーダーコード異常の検出
        //
        receive_stop_fail();
        return;
    }
}

//
//IRリモコン受信準備
//
void IRR_prepare()
{
    state_CaptureIR = IRR_CAPTURE_IDLE;
    BitVector_Clear();

    //
    // LレベルINT1割り込み
    //
    EICRA &= ~((1<<ISC11) | (1<<ISC10));

    //INT1の割り込み許可
    EIMSK |= (1<<INT1);
}

//
//IRリモコン受信状態を返す
//
IRR_capture_condition_t IRR_getCaptureCondition ()
{
    return state_CaptureIR;
}

//
//受信したビット列をデコードしたコードを返す
//
static int16_t decode_remocon_code( uint16_t *count )
{
    uint8_t data = 0x00;

    for (uint8_t shiftbits=0 ; shiftbits<8 ; shiftbits++) {
        uint8_t bs = 0;
        bs = bs | BitVector_ExtractBit ((*count)+0) << 2;
        bs = bs | BitVector_ExtractBit ((*count)+1) << 1;
        bs = bs | BitVector_ExtractBit ((*count)+2) << 0;
        if (bs == 0x4) {                //1 0 0, data bit 1
            data = data | (1 << shiftbits);
            *count = *count + 4;
        } else if (bs == 0x5) {         //1 0 1, data bit 0
            *count = *count + 2;
        } else {
            //ERROR
             return -1;
        }
    }

    return data;
}

//
//受信したビット列をデコードする
//
static void receive_remocon_sub( IRR_frame_t *frame, bool strict_checked )
{
    uint16_t bitvect_count;

    //
    //リーダー部のHレベルを数える
    //
    for (bitvect_count=0 ; bitvect_count<=16 ; bitvect_count++ ) {
        if (BitVector_ExtractBit(bitvect_count)) {
            //No operation
        } else {
            break;
        }
    }
    uint8_t high_level_count = bitvect_count;
    uint8_t low_level = 0;

    if (strict_checked) {
        //
        //厳格な検査をするので、
        //リーダー部のHレベルの数が14,15,16であることを確認する
        //
        if ((high_level_count == 14)
        ||  (high_level_count == 15)
        ||  (high_level_count == 16))
        {
            //No operation
        } else {
            goto error_handling;
        }
        //次の8ビット読み込み
        low_level = BitVector_ExtractOctet (bitvect_count);bitvect_count += 8;
    } else {
        //残るリーダー部のLレベルを読み飛ばす
        for (uint8_t k=0 ; k<8 ; k++) {
            if (BitVector_ExtractBit (bitvect_count) == 0) {
                bitvect_count++;
            } else {
                break;
            }
        }
        //無条件でリーダーチェックを通過させる
        low_level = 0x00;
    }
    //
    //リーダーチェック開始
    //
    switch (low_level & 0xf8) {
        //
        //8Lレベル
        //
        case 0x00: {
            frame->type = FRAME;
            int16_t code;
            //カスタマーコードの下位８ビット
            if ((code = decode_remocon_code (&bitvect_count)) < 0) {
                goto error_handling;
            } else {
                frame->u.ir.custom_code = (uint8_t)code;
            }
            //次にカスタマーコードの上位８ビット
            if ((code = decode_remocon_code (&bitvect_count)) < 0) {
                goto error_handling;
            } else {
                frame->u.ir.custom_code |= (uint8_t)code << 8;
            }
            //次にリモコンデータ
            if ((code = decode_remocon_code (&bitvect_count)) < 0) {
                goto error_handling;
            } else {
                frame->u.ir.data_code = (uint8_t)code;
            }
            //次にリモコンデータの反転
            if ((code = decode_remocon_code (&bitvect_count)) < 0) {
                goto error_handling;
            } else {
                frame->u.ir.inv_data_code = (uint8_t)code;
            }
            //リモコンデータのビット反転を確認する
            if ((frame->u.ir.data_code ^ frame->u.ir.inv_data_code) != 0xff) {
                goto error_handling;
            }
        } break;
        //
        //4Lレベル
        //
        case 0x08: {
            //
            //リピート
            //
            frame->type = REPEAT;
        } break;
        //
        //エラー
        //
        default: {
            goto error_handling;
        }
    }

    if (condition_BitVector != BitVector_Success) {
        goto error_handling;
    }

    //
    //デコード成功
    //
    return; 

    //
    //作業中にエラーが発生していたら, エラーを返す
    //
    error_handling:
    frame->type = ERROR;
    frame->u.codebit32 = 0;
}

//
//割り込み処理で受信したリモコンコードを得る
//ノンブロッキング関数
//
void IRR_receive_nonblock( IRR_frame_t *frame, bool strict_checked )
{
    if (state_CaptureIR != IRR_CAPTURE_DONE) {
        frame->type = ERROR;
    } else {
        //INT1の割り込み禁止
        EIMSK &= ~(_BV(INT1));

        receive_remocon_sub (frame, strict_checked);

        BitVector_Clear();
        state_CaptureIR = IRR_CAPTURE_IDLE;

        //INT1の割り込み復旧
        EIMSK |= _BV(INT1);
    }
}

//
//割り込み処理で受信したリモコンコードを得る
//ブロッキング関数
//
void IRR_receive_block( IRR_frame_t *frame, bool strict_checked )
{
    do {
        IRR_receive_nonblock (frame, strict_checked);
    } while (state_CaptureIR == IRR_CAPTURE_BUSY);
}

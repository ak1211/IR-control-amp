% 赤外線リモコンコードの可視化
% https://ak1211.com
% Copyright (c) 2016 Akihiro Yamamoto
%
%
% This software is released under the MIT License.
% http://opensource.org/licenses/mit-license.php
%

clear *;

% サンプリング周期 0.56ms
T = sampling_period = 0.56e-3

% リーダーがHiである時間 9.0ms
leader_H_duration = 9.0e-3;
% リーダーがHiであるサンプル数
leader_H_count = floor(leader_H_duration / sampling_period)

% リーダーがLoである時間 4.5ms
leader_L_duration = 4.5e-3;
% リーダーがLoであるサンプル数
leader_L_count = floor(leader_L_duration / sampling_period)

%            1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
leader_H =  [1 1 1 1 1 1 1 1 1  1  1  1  1  1  1  1];
%            1 2 3 4 5 6 7 8
leader_L =  [0 0 0 0 0 0 0 0];

% リピートがLoである時間 2.25ms
repeat_L_duration = 2.25e-3;
% リピートがLoであるサンプル数
repeat_L_count = floor(repeat_L_duration / sampling_period)

%            1 2 3 4
repeat_L =  [0 0 0 0];

%
% NECリモコンフォーマットによる定義
%
% ストップビット
STOPBIT =   [1 0 0 0    0 0 0 0];
% リピート
REPEAT =    [leader_H repeat_L STOPBIT];
% リーダー
LEADER =    [leader_H leader_L];
% bit Lo(0) / Hi(1)
L = [1 0];
H = [1 0 0 0];

% custom code 0xbf40
xBF40 = [H L H H    H H H H    L H L L    L L L L];

% data code
% 0x12
% 0xed = ~0x12
x12 = [L L L H    L L H L];
xED = [H H H L    H H L H];


%
% NECフォーマットのフレーム
%
frame = [LEADER xBF40 x12 xED STOPBIT];

%
% 出力
%
stem( REPEAT, "filled")
print -dpng "-S4800, 600" "rawrepeatIR.png"

stem( frame, "filled")
print -dpng "-S4800, 600" "rawframeIR.png"



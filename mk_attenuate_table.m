%
% 電子ボリュームの減衰値を計算する
% http://ak1211.com
% Copyright (c) 2015 Akihiro Yamamoto
%
%
% This software is released under the MIT License.
% http://opensource.org/licenses/mit-license.php
%

%
% 減衰率－設定値表を作る
%

% LM1972の設定値 - 減衰量
global lm1972_table = [ 0.0 -0.5 -1.0 -1.5 -2.0 -2.5 -3.0 -3.5 -4.0 -4.5 -5.0 -5.5 -6.0 -6.5 -7.0 -7.5 -8.0 -8.5 -9.0 -9.5 -10.0 -10.5 -11.0 -11.5 -12.0 -12.5 -13.0 -13.5 -14.0 -14.5 -15.0 -15.5 -16.0 -16.5 -17.0 -17.5 -18.0 -18.5 -19.0 -19.5 -20.0 -20.5 -21.0 -21.5 -22.0 -22.5 -23.0 -23.5 -24.0 -24.5 -25.0 -25.5 -26.0 -26.5 -27.0 -27.5 -28.0 -28.5 -29.0 -29.5 -30.0 -30.5 -31.0 -31.5 -32.0 -32.5 -33.0 -33.5 -34.0 -34.5 -35.0 -35.5 -36.0 -36.5 -37.0 -37.5 -38.0 -38.5 -39.0 -39.5 -40.0 -40.5 -41.0 -41.5 -42.0 -42.5 -43.0 -43.5 -44.0 -44.5 -45.0 -45.5 -46.0 -46.5 -47.0 -47.5 -48.0 -49.0 -50.0 -51.0 -52.0 -53.0 -54.0 -55.0 -56.0 -57.0 -58.0 -59.0 -60.0 -61.0 -62.0 -63.0 -64.0 -65.0 -66.0 -67.0 -68.0 -69.0 -70.0 -71.0 -72.0 -73.0 -74.0 -75.0 -76.0 -77.0 -78.0 ];

%
% LM1972の設定値に対応したdBを返す
% 0 <= param <= 126
%
function retval = LM1972_decibel_of_set_values (param)
    global lm1972_table;
    retval = lm1972_table (param+1);
endfunction

%
% dBに対応したLM1972の設定値を返す
%
function retval = LM1972_set_values_of_decibel (dB)
    global lm1972_table;
    dB05 = 0.5*floor(dB/0.5);
    dB10 = floor(dB);
    if any (lm1972_table == dB05)
        retval = find (lm1972_table == dB05) - 1;
    elseif any (lm1972_table == dB10)
        retval = find (lm1972_table == dB10) - 1;
    else
        error ("out of bounds");
    end
endfunction

%
% PGA2311の設定値
%

%
% PGA2311の設定値に対応したdBを返す
% 1 <= N <= 255
%
function retval = PGA2311_decibel_of_set_values (N)
    retval = 31.5 - (0.5 * (255-N));
endfunction

%
% dBに対応したLM1972の設定値を返す
%
function retval = PGA2311_set_values_of_decibel (dB)
    if (-95.5 <= dB && dB <= 31.5)
        retval = floor(255 - (31.5-dB)/0.5);
    else
        error ("out of bounds");
    end
endfunction

%
% 作成する電子ボリュームマップベクタ
%
function retval = make_electric_volume_vect (rotation_travel, ideal_a_curve)
    % 電子ボリュームは3つの角度の関数で構成

    %
    % 1つめの関数
    %
    a = ideal_a_curve(1) / 0.5;
    p = 0;
    q = 0;
    f1 = a*(rotation_travel-p)+q;

    %
    % 2つめの関数
    %
    a = 1/2;    % 1 : 2
    p = 0.65;   % 65%
    q = 0.10;   % 10%
    f2 = a*(rotation_travel-p)+q;

    %
    % 3つめの関数
    %
    a = (1.0 - ideal_a_curve(30)) / (1.0 - 30/32);
    p = 1.0-(1/a);
    q = 0;
    f3 = a*(rotation_travel-p)+q;

    % 電子ボリュームカーブ
    evol_curve = max(f1, max(f2, f3));

    retval = evol_curve;
endfunction

%
%
%
format short g;
clf;

% ボリュームの段階数
volume_steps = 0:31;

% ボリューム段階[steps] - 回転位置[%]
rotation_travel = linspace (0.0, 1.0, length(volume_steps));

%
% 理想Aカーブボリューム
% y = exp(a*x) / exp(a)
% y = exp(a*x-a)
% y = exp(a(x-1))
%
% exp(a(x-1)) = y
% a(x-1) = log(y)
% a = log(y) / (x-1)
%
% 回転位置 50% は 出力電圧 15%
%
times_of_A = log(0.15) / (0.5-1);

% 理想Aカーブボリューム
ideal_a_curve = exp(times_of_A*(rotation_travel-1));

% 理想Aカーブボリュームのプロット
subplot (2,1,1);
hold on;
%plot (rotation_travel, ideal_a_curve, 'cm-@*')
plot (volume_steps, ideal_a_curve, 'cm-@*')

evol_curve = make_electric_volume_vect (rotation_travel, ideal_a_curve);

% 電子ボリュームカーブのプロット
%plot (rotation_travel, evol_curve, 'cb-@o')
plot (volume_steps, evol_curve, 'cb-@o')

% 目盛とか
axis ([ [min(volume_steps) max(volume_steps)], [0.0 1.0] ]);
grid minor;
xlabel ('volume steps');
ylabel ('Vout/Vin');
legend ('ideal A curved volume', 'electric volume curve', "location", 'north');
hold off;

% Aカーブボリュームのプロット
subplot (2,1,2);
hold on;
plot (volume_steps, 20*log10(ideal_a_curve), 'cm-@*')

%
% LM1972電子ボリュームのデシベル
%
evol_curve(1) = 10^(-78.0 / 20);  % ボリューム０は取りあえず -78dB
attenuetor_dB = 20 * log10 (evol_curve);
%
% LM1972の設定値
lm1972_param = arrayfun (@LM1972_set_values_of_decibel, attenuetor_dB);
% LM1972の減衰量
lm1972_att = arrayfun (@LM1972_decibel_of_set_values, lm1972_param);
%
printf ("// LM1972_attenuetor_dB\n")
printf ("%3.1f, ", lm1972_att)
printf("\n\n")
%

% LM1972電子ボリュームのプロット
plot (volume_steps, lm1972_att, "cb-@o;simulate LM1972 volume at decibel;")

%
% PGA2311電子ボリュームのデシベル
%
evol_curve(1) = 10^(-95.5 / 20);  % ボリューム０は取りあえず -95.5dB
attenuetor_dB = 20 * log10 (evol_curve);
%
% PGA2311の設定値
PGA2311_param = arrayfun (@PGA2311_set_values_of_decibel, attenuetor_dB);
% PGA2311の減衰量
PGA2311_att = arrayfun (@PGA2311_decibel_of_set_values, PGA2311_param);
%
printf ("// PGA2311_attenuetor_dB\n")
printf ("%3.1f, ", PGA2311_att)
printf("\n\n")
%


% PGA2311電子ボリュームのプロット
plot (volume_steps, PGA2311_att, "cg-@o;simulate PGA2311 volume at decibel;")

%
% 目盛とか
%
axis ([ [min(volume_steps) max(volume_steps)], [min(PGA2311_att) max(PGA2311_att)] ]);
grid minor;
xlabel ('volume steps');
ylabel ('attenuation [dB]');
legend ( 'ideal A curved volume','LM1972 volume steps of decibel', 'PGA2311 volume steps of decibel', "location", 'southeast');
hold off;

%
% 計算結果を出力
%

printf ("// lm1972_param\n")
printf ("%3d, ", lm1972_param)
printf("\n\n")
%
printf ("// PGA2311_param\n")
printf ("%3d, ", PGA2311_param)
printf("\n\n")

%print -Ggswin32c.exe foo.png


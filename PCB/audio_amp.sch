EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:analog_switches
LIBS:texas
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:mylib
LIBS:toragi_201305
LIBS:audio_amp-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 4
Title "IR control headphone amplifier"
Date "2015-10-21"
Rev "A2.1"
Comp "Copyright 2015 ak1211.com (CC BY-SA 4.0) "
Comment1 "main sheet"
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Sheet
S 7050 750  2150 600 
U 55C72EED
F0 "power_supply_section" 100
F1 "power_supply_section.sch" 100
F2 "ANALOG_PWR" I L 7050 900 60 
F3 "~INRUSH_CURRENT_LIMITTER" I L 7050 1100 60 
$EndSheet
$Sheet
S 1050 900  1900 1400
U 5546C578
F0 "analog_section" 100
F1 "analog_section.sch" 100
F2 "EVOL_~CS" I R 2950 1550 60 
F3 "EVOL_SDI" I R 2950 1700 60 
F4 "EVOL_SCLK" I R 2950 1850 60 
F5 "|input.L|" O R 2950 1250 60 
F6 "|input.R|" O R 2950 1400 60 
F7 "SELECT_SOURCE" I R 2950 2000 60 
$EndSheet
Wire Wire Line
	4100 1250 2950 1250
Wire Wire Line
	4100 1400 2950 1400
Wire Wire Line
	4100 1550 2950 1550
Wire Wire Line
	4100 1700 2950 1700
Wire Wire Line
	4100 1850 2950 1850
Wire Wire Line
	4100 2000 2950 2000
Wire Wire Line
	7050 900  6100 900 
Wire Wire Line
	7050 1100 6100 1100
$Sheet
S 4100 750  2000 1550
U 554228E2
F0 "controller_section" 100
F1 "controller_section.sch" 100
F2 "EVOL_SDI" O L 4100 1700 60 
F3 "EVOL_~CS" O L 4100 1550 60 
F4 "EVOL_SCLK" O L 4100 1850 60 
F5 "ANALOG_PWR" O R 6100 900 60 
F6 "~INRUSH_CURRENT_LIMITTER" O R 6100 1100 60 
F7 "|input.L|" I L 4100 1250 60 
F8 "|input.R|" I L 4100 1400 60 
F9 "SELECT_SOURCE" O L 4100 2000 60 
$EndSheet
$EndSCHEMATC

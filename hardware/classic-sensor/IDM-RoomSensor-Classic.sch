EESchema Schematic File Version 4
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "IDM-RoomSensor-Classic"
Date "2026-07-15"
Rev "A"
Comp "IDM AERO ALM prototype"
Comment1 "Assumption: 0-10V = 0-100% RH"
$EndDescr
$Comp
L Device:R R1
U 1 1 3E8
P 1800 1400
F 0 "J1" H 2000 1500 50 0000 C CNN
F 1 "IDM connector" H 2100 1300 50 0000 C CNN
	1    1800 1400
	0    -1 -1 0
$EndComp
Text Notes 1300 1750 0 50 ~ 0
43/42/41/40
$Comp
L Device:R R2
U 1 1 3E9
P 4000 1400
F 0 "U1" H 4200 1500 50 0000 C CNN
F 1 "24V→5V" H 4300 1300 50 0000 C CNN
	1    4000 1400
	0    -1 -1 0
$EndComp
Text Notes 3500 1750 0 50 ~ 0
Versorgung aus IDM
$Comp
L Device:R R3
U 1 1 3EA
P 6200 1400
F 0 "U2" H 6400 1500 50 0000 C CNN
F 1 "ATtiny1616" H 6500 1300 50 0000 C CNN
	1    6200 1400
	0    -1 -1 0
$EndComp
Text Notes 5700 1750 0 50 ~ 0
Kein WLAN
$Comp
L Device:R R4
U 1 1 3EB
P 1800 3000
F 0 "U3" H 2000 3100 50 0000 C CNN
F 1 "SHT45" H 2100 2900 50 0000 C CNN
	1    1800 3000
	0    -1 -1 0
$EndComp
Text Notes 1300 3350 0 50 ~ 0
Feuchte
$Comp
L Device:R R5
U 1 1 3EC
P 4000 3000
F 0 "U4" H 4200 3100 50 0000 C CNN
F 1 "DAC + OPV" H 4300 2900 50 0000 C CNN
	1    4000 3000
	0    -1 -1 0
$EndComp
Text Notes 3500 3350 0 50 ~ 0
0–10V
$Comp
L Device:R R6
U 1 1 3ED
P 6200 3000
F 0 "RTH1" H 6400 3100 50 0000 C CNN
F 1 "KTY81-210" H 6500 2900 50 0000 C CNN
	1    6200 3000
	0    -1 -1 0
$EndComp
Text Notes 5700 3350 0 50 ~ 0
IDM Temperatur

Text Notes 900 700 0 100 ~ 20
IDM-RoomSensor-Classic
Text Notes 900 7600 0 60 ~ 0
IDM: 43=KTY, 42=+14-32V, 41=GND/KTY return, 40=RH 0-10V
$EndSCHEMATC

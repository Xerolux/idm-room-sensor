EESchema Schematic File Version 4
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "IDM Fake Sensor ESP"
Date "2026-07-15"
Rev "A"
Comp "Prototype"
Comment1 "HA values -> IDM 0-10V RH + emulated KTY81-210"
Comment2 "Assumption: 0V=0%RH, 10V=100%RH"
$EndDescr
Text Notes 700 500 0 110 ~ 22
IDM Fake Sensor ESP
Text Notes 700 800 0 60 ~ 0
J1: 43=KTY, 42=+14-32V, 41=GND/KTY return, 40=RH 0-10V

$Comp
L Regulator_Switching:LMR16006 U1
U 1 1 1
P 2200 1800
F 0 "U1" H 2200 2167 50 0000 C CNN
F 1 "24V_TO_5V" H 2200 2076 50 0000 C CNN
	1    2200 1800
	1 0 0 -1
$EndComp
Text Notes 1700 2250 0 50 ~ 0
Protected 24V input, TVS, reverse-polarity protection, LC filter

$Comp
L MCU_Espressif:ESP32-C3-MINI-1 U2
U 1 1 2
P 4700 1900
F 0 "U2" H 4700 2567 50 0000 C CNN
F 1 "ESP32-C3-MINI-1" H 4700 2476 50 0000 C CNN
	1    4700 1900
	1 0 0 -1
$EndComp
Text Notes 4000 2700 0 50 ~ 0
ESPHome API / MQTT, watchdog, local fallback values

$Comp
L DAC:MCP4725 U3
U 1 1 3
P 7200 1700
F 0 "U3" H 7200 2067 50 0000 C CNN
F 1 "MCP4725" H 7200 1976 50 0000 C CNN
	1    7200 1700
	1 0 0 -1
$EndComp

$Comp
L Amplifier_Operational:OPA197 U4
U 1 1 4
P 9000 1700
F 0 "U4" H 9344 1746 50 0000 L CNN
F 1 "OPA197" H 9344 1655 50 0000 L CNN
	1    9000 1700
	1 0 0 -1
$EndComp
Text Notes 8200 2300 0 50 ~ 0
Gain stage: DAC 0..3.3V -> calibrated 0..10V RH output

$Comp
L Digital_Potentiometer:AD5242 U5
U 1 1 5
P 6000 4300
F 0 "U5" H 6000 4967 50 0000 C CNN
F 1 "AD5242BRUZ10" H 6000 4876 50 0000 C CNN
	1    6000 4300
	1 0 0 -1
$EndComp
Text Notes 4850 5150 0 50 ~ 0
Temperature emulation: calibrated resistance table approximates KTY81-210.
Text Notes 4850 5400 0 50 ~ 0
Validate IDM excitation voltage/current before use.

$Comp
L Connector_Generic:Conn_01x05 J1
U 1 1 6
P 1800 4300
F 0 "J1" H 1718 4717 50 0000 C CNN
F 1 "IDM" H 1718 4626 50 0000 C CNN
	1    1800 4300
	-1 0 0 -1
$EndComp
Text Notes 900 5000 0 50 ~ 0
Pins: KTY / +14-32V / GND+KTY return / RH 0-10V

Text Notes 700 7000 0 70 ~ 12
FAIL-SAFE: On HA/API timeout output configurable safe values, default 80%RH and 28C-equivalent resistance.
$EndSCHEMATC

# Hardwareübersicht

## ESP Sensor

- ESP32-C3-MINI-1
- SHT45
- 24-V-Eingang mit Schutzbeschaltung
- 24 V → 5 V → 3,3 V
- MCP4725-DAC
- OPA197-Verstärker für 0–10 V
- echter KTY81-210
- ESPHome, WLAN und OTA

## Classic Sensor

- ATtiny1616
- SHT45
- MCP4725-DAC
- OPA197-Verstärker
- echter KTY81-210
- keine Netzwerkabhängigkeit
- kleineres Gehäuse

## Fake Sensor

- ESP32-C3-MINI-1
- MCP4725 und OPA197 für 0–10 V
- programmierbares Widerstandsnetzwerk für die Temperatur
- Home-Assistant-Werte als Quelle
- konfigurierbare Fallback-Werte
- Watchdog und Kommunikations-Timeout

> Die KTY-Emulation des Fake Sensors ist noch nicht elektrisch validiert.


## Bestätigte Originalbelegung

| IDM | Sensor | Funktion |
|---:|---:|---|
| 43 | 2 | KTY-Temperatursignal |
| 42 | 7 | +14–32 V DC |
| 41 | 3 und 6 | KTY-Rückleiter und GND |
| 40 | 5 | Feuchte 0–10 V |

Der Anschluss benötigt vier Adern; die beiden Rückleiter werden am Sensor bzw. auf der Platine gemeinsam auf Klemme 41 geführt.

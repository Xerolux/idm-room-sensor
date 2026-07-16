# IDM Fake Sensor ESP – Revision A

Diese Platine empfängt Temperatur- und Feuchtewerte aus Home Assistant und simuliert gegenüber der IDM AERO ALM den originalen Temperatur-/Feuchtesensor.

## Ausgänge

| IDM-Klemme | Signal |
|---|---|
| 43 | emuliertes KTY-Temperatursignal |
| 42 | +14–32 V DC Versorgung |
| 41 | gemeinsamer GND/KTY-Rückleiter |
| 40 | simulierte Feuchte 0–10 V |

Der ESP32 wird vollständig aus den 24 V der IDM versorgt.

## Home Assistant

Home Assistant setzt zwei ESPHome-Number-Entitäten:

- simulierte relative Feuchte
- simulierte Raumtemperatur

Damit können Mittelwerte, Maximalwerte oder der ungünstigste Raum an die Wärmepumpe weitergegeben werden. Für die Kühlung sollte eher der **kritischste Raum** verwendet werden als ein Durchschnitt.

## Fail-safe

Bei fehlenden Aktualisierungen werden standardmäßig ausgegeben:

- 80 % rF
- 28 °C

Die Werte sind absichtlich konservativ. Zusätzlich sollte die vorhandene Taupunktüberwachung der Anlage aktiv bleiben.

## Wichtige technische Einschränkung

Die Feuchtesimulation ist klar: DAC plus Operationsverstärker erzeugen 0–10 V.

Die Temperatursimulation muss noch elektrisch validiert werden. Der Entwurf verwendet einen Digitalpotentiometer plus Festwiderstandsnetzwerk zur Nachbildung eines KTY81-210. Vor Anschluss müssen an der IDM gemessen werden:

1. Leerlaufspannung an 43 gegen 41,
2. Messstrom durch einen echten KTY81-210,
3. zulässige Spannung an den Anschlüssen des Digitalpotentiometers,
4. tatsächliche Widerstandskennlinie, die die IDM erwartet.

Falls Spannung oder Strom für einen Digitalpotentiometer ungeeignet sind, muss die Temperaturstufe durch eine galvanisch isolierte Widerstandsmatrix oder PhotoMOS-Lösung ersetzt werden.

## Status des Layouts

Das KiCad-PCB enthält Boardkontur und Bauteilplatzierung. Es ist eine Rev.-A-Entwicklungsgrundlage und **nicht produktionsfreigegeben**. Vor Bestellung:

- Footprints kontrollieren,
- Schaltplan in modernes KiCad-Format übernehmen,
- vollständig routen,
- ERC/DRC ausführen,
- 0–10-V-Verstärkung dimensionieren,
- Temperaturstufe vermessen und kalibrieren,
- EMV- und Schutzbeschaltung prüfen.

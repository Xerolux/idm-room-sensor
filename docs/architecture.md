# IDM Smart Climate Platform

## Ziel

Temperatur- und Feuchtewerte können aus KNX, ESPHome, Zigbee, MQTT, Home Assistant oder lokalen Sensoren stammen. Die Plattform berechnet je Raum Taupunkt und Taupunktabstand und übergibt den kritischsten gültigen Wert an die IDM.

## Übertragungswege

1. **GLT/Modbus über `idm-heatpump-hass`** – bevorzugter Weg.
2. **Fake-Sensor-Platine** – 0–10 V Feuchte und KTY-Nachbildung.
3. **Lokaler Sensor** – autonomer Fallback bei Netzwerk- oder HA-Ausfall.

## Relevante Register

| Funktion | Register | Typ |
|---|---:|---|
| Externe Raumtemperatur HK A | 1650–1651 | FLOAT |
| HK B | 1652–1653 | FLOAT |
| HK C | 1654–1655 | FLOAT |
| HK D | 1656–1657 | FLOAT |
| HK E | 1658–1659 | FLOAT |
| HK F | 1660–1661 | FLOAT |
| HK G | 1662–1663 | FLOAT |
| Externe Feuchte GLT | 1692–1693 | FLOAT |

## Auswahlstrategie

Für die Kühlung sollte nicht pauschal ein Mittelwert verwendet werden. Bevorzugt wird der Raum mit dem kleinsten Abstand zwischen Raumtemperatur und Taupunkt. Optional können Maximalfeuchte, fester Referenzraum oder ein eigenes Template verwendet werden.

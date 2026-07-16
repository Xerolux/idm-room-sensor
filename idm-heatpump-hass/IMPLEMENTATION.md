# Native Erweiterung in idm-heatpump-hass main

## Aktion: `set_external_climate`

Die Aktion wurde am 2026-07-16 in den Upstream-Main-Branch aufgenommen. Ziel
ist, die bekannten Register nicht über rohe Adressen, sondern über aufgelöste,
schreibbare Registerdefinitionen zu bedienen.

```yaml
action: idm_heatpump.set_external_climate
data:
  heating_circuit: A
  room_temperature: 23.1
  humidity: 58.4
```

## Registerabbildung

- HK A–G: 1650, 1652, 1654, 1656, 1658, 1660, 1662
- Feuchte GLT: 1692
- Datentyp jeweils FLOAT

## Validierung

- Effektiver Temperaturbereich laut aktuellem Register-Metadatenvertrag:
  15 bis 30 °C
- Feuchtebereich 0 bis 100 %
- nur vorhandene, schreibbare Register verwenden
- NaN und unendliche Werte ablehnen
- alle angeforderten Register vor dem ersten Schreibzugriff validieren

Bis die native Aktion in einer getaggten Version veröffentlicht ist,
funktioniert das beiliegende HA-Paket mit
`idm_heatpump.write_register`.

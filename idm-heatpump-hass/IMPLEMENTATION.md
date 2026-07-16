# Vorgesehene Erweiterung für idm-heatpump-hass

## Neue Aktion: `set_external_climate`

Ziel: Die bekannten Register nicht über rohe Adressen bedienen, sondern über eine sichere, verständliche Aktion.

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

- Temperaturbereich zunächst -20 bis 60 °C
- Feuchtebereich 0 bis 100 %
- nur auf unterstützten Navigator-Modellen anbieten
- Schreibzugriffe protokollieren
- optional zyklische Aktualisierung und Timeout-Diagnose

Bis die native Aktion implementiert ist, funktioniert das beiliegende HA-Paket mit dem vorhandenen Dienst `idm_heatpump.write_register`.

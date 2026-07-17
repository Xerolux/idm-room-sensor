# Contributing

Beiträge sind willkommen, besonders:

- vermessene IDM-Eingangskennlinien,
- Daten des originalen Sensors,
- Verbesserungen an Schaltplan und PCB,
- getestete Bauteilalternativen,
- Gehäuseverbesserungen,
- ESPHome- und Home-Assistant-Beispiele.

## Pull Requests

1. Änderungen in einem eigenen Branch erstellen.
2. KiCad ERC/DRC lokal ausführen.
3. Keine generierten Backup-Dateien committen.
4. Messwerte und Annahmen dokumentieren.
5. Änderungen an Hardware deutlich als getestet oder ungetestet kennzeichnen.

## Software-Prüfungen

```bash
python3 -m pip install jinja2 jsonschema pytest pyyaml
make check
```

Der Check validiert YAML/JSON, die Home-Assistant-Vertragsbestandteile,
Taupunktberechnung und die `set_external_climate`-Referenztests. Die
C++-Core-Tests werden mit `g++ -std=c++17` kompiliert; unter Ubuntu ist
`build-essential` dafür ausreichend.

## Sicherheit

Keine Änderung als produktionsreif bezeichnen, solange sie nicht auf echter Hardware getestet und dokumentiert wurde.

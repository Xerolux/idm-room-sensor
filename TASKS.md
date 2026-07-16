# Zentrale Aufgabenliste

Stand: 2026-07-16

Diese Datei ist die priorisierte und deduplizierte Arbeitswarteschlange des
Projekts. `PROJECT_STATUS.md`, die Work Packages sowie die Checklisten in
`docs/`, `hardware/` und `manufacturing/` liefern die Details, erzeugen aber
keine zusätzlichen unabhängigen Aufgaben.

## Legende

| Kennzeichen | Bedeutung |
|---|---|
| **Codex** | Kann im Repository autonom umgesetzt und getestet werden |
| **Maintainer** | Benötigt eine Projektentscheidung, Freigabe oder Veröffentlichung |
| **Labor** | Benötigt reale Hardware, Messgeräte oder die IDM-Anlage |
| **Review** | Benötigt eine fachkundige externe Gegenprüfung |
| **Upstream** | Hängt von einem anderen Repository oder dessen Release ab |

## Nächste Ausführungsreihenfolge

Diese Reihenfolge minimiert Arbeit an noch nicht charakterisierter Hardware.

1. **SW-01 bis SW-05** – Software- und Home-Assistant-Lücken schließen.
2. **FW-01 bis FW-08** – Firmware kompilierbar, testbar und ausfallsicher machen.
3. **SAFE-01 bis SAFE-05** – Originalsensor und IDM-Eingänge real vermessen.
4. **HW-01 bis HW-10** – Schaltung freigeben, routen und Fertigungsdaten prüfen.
5. **LAB-01 bis LAB-07** – Prototyp fertigen und am Prüfstand validieren.
6. **SITE-01 bis SITE-04** – Kontrollierte Tests an einer realen IDM-Anlage.
7. **REL-01 bis REL-05** – Nachweise, Bauanleitung und erste verifizierte Version.

## P0 – Repository und Software

- [ ] **REPO-01 · Maintainer** – Den aktuellen Arbeitsstand prüfen und als
  getrennte, nachvollziehbare Commits übernehmen. Die reinen
  Zeilenende-Änderungen in Fabrication- und Validation-CSV-Dateien dabei nicht
  mit funktionalen Änderungen vermischen.
  **Fertig, wenn:** Tests grün sind, der Diff bewusst freigegeben ist und keine
  unbeabsichtigten CSV-Änderungen im Commit verbleiben.

- [x] **SW-01 · Codex** – Das Blueprint `critical_room.yaml` von einem
  gekennzeichneten Platzhalter zu einer ausführbaren Automation ausbauen.
  **Fertig, wenn:** Eingaben, Bedingungen und Aktionen vollständig sind und
  der YAML-/Repository-Check die Automation validiert.

- [x] **SW-02 · Codex** – Das Blueprint `stale_guard.yaml` ausführbar machen.
  **Fertig, wenn:** veraltete oder ungültige Raumwerte zuverlässig den
  konfigurierten sicheren Zustand auslösen und Rückkehrbedingungen getestet
  sind.

- [x] **SW-03 · Codex** – Diagnose-Entitäten und ein kompaktes
  Home-Assistant-Dashboard für Datenalter, Auswahlgrund, Fallbackstatus,
  Schreibfehler und Taupunktreserve ergänzen.
  **Fertig, wenn:** ein Betreiber ohne Logsuche erkennt, warum welcher Wert an
  IDM gesendet wird.

- [x] **SW-04 · Codex** – Regressionstests für Blueprint-Aktionen,
  Grenzwert-Clamping, nicht verfügbare Sensoren, gleiche Taupunkte und
  Wiederanlauf nach Home-Assistant-Neustart ergänzen.
  **Fertig, wenn:** die sicherheitsrelevanten Pfade automatisiert abgedeckt
  sind und `make check` erfolgreich läuft.

- [ ] **SW-05 · Upstream/Maintainer** – Ein getaggtes
  `idm-heatpump-hass`-Release mit dem bereits auf `main` vorhandenen
  `set_external_climate`-Dienst veröffentlichen und danach die lokale
  Raw-Register-Fallback-Dokumentation neu bewerten.
  **Fertig, wenn:** eine installierbare Release-Version den Dienst enthält und
  die Kompatibilitätsmatrix aktualisiert ist.

## P0 – Sicherheitsrelevante Charakterisierung

Diese Aufgaben blockieren die endgültige Analogschaltung und jeden Kühltest an
einer realen Anlage.

- [ ] **SAFE-01 · Maintainer/Labor** – Exaktes IDM-Modell,
  Navigator-Version, Klemmenbelegung und vorgesehene GLT-/Raumsensor-
  Konfiguration dokumentieren.
  **Fertig, wenn:** Typenschild, Softwarestand, Schaltplanausschnitt und
  verwendete Klemmen eindeutig festgehalten sind.

- [ ] **SAFE-02 · Labor** – Den 0–10-V-Ausgang eines originalen LCN-FTW04 bei
  mehreren stabilen Feuchtepunkten messen.
  **Fertig, wenn:** Rohdaten, Messunsicherheit, Versorgungsspannung, Last,
  Temperatur und eine belastbare Übertragungsfunktion vorliegen.

- [ ] **SAFE-03 · Labor** – Den Navigator-Temperatureingang mit einer
  Widerstandsdekade charakterisieren.
  **Fertig, wenn:** Messstrom/-spannung, akzeptierter Widerstandsbereich,
  Temperaturzuordnung und Plausibilitätsgrenzen bekannt sind.

- [ ] **SAFE-04 · Labor** – Offener Eingang, Kurzschluss, Ausfall der
  Versorgung und unplausible Analogwerte am Navigator prüfen.
  **Fertig, wenn:** das reale Fehlerverhalten dokumentiert ist und daraus
  sichere Firmware-Fallbackwerte abgeleitet wurden.

- [ ] **SAFE-05 · Labor** – Nachweisen, dass die ausgewählte
  IDM-Konfiguration die GLT-Werte tatsächlich für Heiz- und Kühlregelung
  verwendet.
  **Fertig, wenn:** Registeränderung, angezeigter Wert und Regelreaktion
  gemeinsam protokolliert sind.

## P1 – Firmware

- [x] **FW-01 · Codex/Maintainer** – Eine unterstützte ESPHome-Version
  auswählen, fest angeben und alle ESPHome-Konfigurationen dagegen
  kompilieren.
  **Fertig, wenn:** reproduzierbare lokale und CI-Builds ohne Warnungen über
  veraltete Konfigurationen laufen.

- [x] **FW-02 · Codex** – Die leere `idm_bridge`-Komponente implementieren:
  Lebenszyklus, Eingangsvalidierung, Ausgangsaktualisierung, Fehlerstatus und
  sichere Initialwerte.
  **Abhängigkeit:** Schnittstellen dürfen vorgezogen werden; finale
  Analogkennlinien benötigen SAFE-02 und SAFE-03.

- [x] **FW-03 · Codex** – Die Platzhalter in
  `fake-sensor-esphome.yaml` und `fake-sensor-bridge.yaml` durch reale
  DAC-/KTY-Ausgabe und Statusrückmeldung ersetzen.
  **Fertig, wenn:** Sollwerte nicht nur geloggt, sondern über klar definierte
  Treiber ausgegeben werden.

- [x] **FW-04 · Codex** – Kalibrierwerte versioniert und
  stromausfallsicher speichern, einschließlich Werkswerten, Wertebereich und
  Migrationsverhalten.
  **Fertig, wenn:** ungültige Daten erkannt werden und ein Reset auf sichere
  Standardwerte möglich ist.

- [x] **FW-05 · Codex** – Unit-Tests für Taupunkt, KTY-Tabelle,
  Qualitätsbewertung, Fallback-Zustandsmaschine, Clamping und
  Kalibriertransformation ergänzen.
  **Fertig, wenn:** Randwerte und Fehlerzustände ohne Zielhardware getestet
  werden.

- [x] **FW-06 · Codex** – MQTT-Topics, Discovery, Availability und
  Diagnose-Payloads vollständig spezifizieren und implementieren.
  **Fertig, wenn:** Entitäten nach Neustart reproduzierbar erscheinen und
  Verbindungsverlust eindeutig sichtbar ist.

- [x] **FW-07 · Codex** – Das geplante lokale Web-UI um Status,
  Kalibrierung, sichere Grenzen, Diagnoseexport und Reset ergänzen.
  **Fertig, wenn:** Schema und Implementierung übereinstimmen und gefährliche
  Änderungen eine bewusste Bestätigung verlangen.

- [x] **FW-08 · Codex** – Die native ESP-IDF-Variante auf Funktionsparität
  bringen: I/O, Netzwerk, Watchdog, Fallback, Persistenz, OTA und Diagnose.
  **Fertig, wenn:** die Variante mehr als die aktuelle
  Taupunkt-Demonstration ausführt und ihre Funktionen automatisiert prüfbar
  sind.

## P1 – Hardwarefreigabe und Fertigungsdaten

- [ ] **HW-01 · Review** – Externe Architektur- und Sicherheitsprüfung für
  GLT-Pfad, Analog-Fallback und unabhängige Kondensationsabschaltung
  durchführen.

- [ ] **HW-02 · Review** – Schaltpläne elektrisch prüfen: Versorgung,
  Verpolung, Überspannung, Schutzbeschaltung, Pegel, Op-Amp-Stabilität,
  Pull-ups und Testpunkte.

- [ ] **HW-03 · Maintainer/Review** – Exakte Bauteile,
  Herstellerteilenummern, Datenblattrevisionen und zulässige Alternativen
  festlegen.

- [ ] **HW-04 · Review** – Jedes Symbol und Footprint gegen Hersteller-
  Landpattern, Pin-1-Markierung und Bestückungsorientierung prüfen.

- [ ] **HW-05 · Review** – Schaltregler-Layout gegen das Referenzlayout des
  verwendeten Reglers prüfen, einschließlich Hot Loop, Induktivität,
  Rückkopplung und thermischer Kupferflächen.

- [ ] **HW-06 · Review** – Analoge Masse-, Versorgungs- und Rückstrompfade
  für 0–10 V, KTY-Emulation und Sensorik festlegen und simulieren oder am
  Steckbrett vorvalidieren.

- [ ] **HW-07 · Codex/Maintainer** – Zuerst Fake-Sensor und Pipe-Safety als
  verifizierbare MVP-Platinen vollständig routen; weitere Varianten erst nach
  erfolgreichem Prüfstandstest priorisieren.

- [ ] **HW-08 · Codex/Review** – KiCad-9-ERC und -DRC ohne ungeklärte Fehler
  ausführen; bewusste Ausnahmen schriftlich begründen.

- [ ] **HW-09 · Codex/Maintainer** – Gehäuse, Steckverbinder, Montage,
  Luftführung, Isolation, Programmierzugang und Testpunkte auf mechanische
  Kollisionen prüfen.

- [ ] **HW-10 · Codex/Review** – Gerber, Bohrdaten, Pick-and-Place und BOM
  erzeugen und mit Viewer sowie Hersteller-Preview gegenprüfen.
  **Fertig, wenn:** ein eingefrorener, versionierter Engineering-Sample-Satz
  vorliegt.

## P2 – Prototyp und Prüfstand

- [ ] **LAB-01 · Maintainer** – Klar als Engineering Sample markierte
  Prototypen von Fake-Sensor und Pipe-Safety bestellen.

- [ ] **LAB-02 · Labor** – Eingangsschutz und Versorgung bei 14 V, 24 V und
  30 V sowie bei Grenz- und Fehlerbedingungen messen.

- [ ] **LAB-03 · Labor** – 0–10-V-Ausgang über den gesamten Bereich auf
  Linearität, Offset, Lasttreiberfähigkeit, Rauschen und Startverhalten
  prüfen.

- [ ] **LAB-04 · Labor** – KTY-Emulation über Temperaturbereich,
  Navigator-Messstrom, Toleranzen, Ausfallzustände und Kalibrierung prüfen.

- [ ] **LAB-05 · Labor/Codex** – Hardware-in-the-Loop-Test für Sensorwerte,
  MQTT/HA-Ausfall, Neustart, Brownout und Rückkehr aus dem Fallback
  automatisieren.

- [ ] **LAB-06 · Labor** – OTA-Abbruch, beschädigte Konfiguration,
  Watchdog, Netzwerkverlust und Langzeitbetrieb mit Fault Injection testen.

- [ ] **LAB-07 · Labor/Review** – Thermik, ESD und EMC-Pre-Compliance prüfen
  und Abweichungen vor einer realen Installation beheben.

## P3 – Kontrollierte reale Installation

- [ ] **SITE-01 · Labor** – Zuerst einen reinen Beobachtungstest im
  Heizbetrieb ohne aktive Beeinflussung durchführen.

- [ ] **SITE-02 · Labor** – GLT-/Modbus-Schreibvorgänge auf der realen Anlage
  verifizieren, einschließlich Grenzen, Rücklesen, Neustart und Fehlercodes.

- [ ] **SITE-03 · Labor/Review** – Einen kontrollierten Kühltest mit
  unabhängigem, hart verdrahtetem Taupunktwächter und sofortiger
  Abschaltmöglichkeit durchführen.

- [ ] **SITE-04 · Labor** – Mehrwöchiges Logging mit Sensor-, Netzwerk-,
  Home-Assistant- und Bridge-Fehlern durchführen und sichere Reaktionen
  nachweisen.

## P4 – Dokumentation und Release

- [ ] **REL-01 · Labor** – Reale Fotos, Platinenaufnahmen,
  Verdrahtungsbilder und Oszilloskopkurven mit Testbedingungen veröffentlichen.

- [ ] **REL-02 · Codex/Labor** – Eine erstmals praktisch verifizierte
  Bau-, Flash-, Kalibrier- und Inbetriebnahmeanleitung erstellen.

- [x] **REL-03 · Codex** – Nach jedem abgeschlossenen Block die duplizierten
  Checklisten in `README.md`, `PROJECT_STATUS.md`, Work Packages, Docs und
  Wiki synchronisieren.

- [ ] **REL-04 · Maintainer** – Versionierte Firmware-Binaries,
  Konfigurationsbeispiele, Schaltpläne und Fertigungsdaten mit eindeutiger
  Hardware-Revision veröffentlichen.

- [ ] **REL-05 · Maintainer/Review** – Vor Entfernen der Warnung
  „not production-ready“ die vollständige Sicherheits-, Validierungs- und
  Release-Checkliste extern gegenprüfen.

## Release-Gate

Eine Version darf erst als an realer IDM-Hardware verifiziert bezeichnet
werden, wenn mindestens **SAFE-01 bis SAFE-05**, **HW-01 bis HW-10**,
**LAB-01 bis LAB-07** und **SITE-01 bis SITE-03** abgeschlossen und durch
Rohdaten belegt sind. Ein Softwaretest allein ersetzt keinen
Kondensationsschutz.

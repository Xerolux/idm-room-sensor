# Design: Breite Härtung und Aufräumarbeiten (P1–P4)

**Datum:** 2026-07-17
**Branch:** `optimization/hardening-cleanup`
**Status:** Entworfen, wartet auf Freigabe nach `TASKS.md`-Verfahren (`REPO-01 · Maintainer`)

## Hintergrund und Ziel

Auf Basis eines zweigleisigen Audits (HA/Firmware-Tiefenanalyse + CI/Tools/Doku-Konsistenz)
wurden ~37 konkrete Punkte identifiziert, davon mehrere mit echtem Sicherheits- oder
Korrektheitsbezug im Kontrollpfad zur IDM-Wärmepumpe. Ziel ist ein geordneter Durchlauf
aller technisch ohne Labor-Hardware umsetzbaren Punkte in vier Phasen. Labor-/Review-/
Upstream-Aufgaben (SAFE-*, LAB-*, SITE-*, HW-Review, SW-05 Upstream-Release) bleiben
ausdrücklich out of scope, da sie reale Messungen, Freigaben oder Fremd-Repos benötigen.

Entschieden wurde (siehe Brainstorming-Protokoll):

1. **Streng fail-closed** für Sicherheits-Gates (MQTT-Auth, Web-Passwort, API-Token,
   OTA-Allowlist). Bau/Start schlagen fehl, solange Platzhalter konfiguriert sind.
2. **Vollumfang P1–P4** mit je einem Commit pro Phase; `make check` muss nach jeder Phase
   grün sein.
3. **Dead Code wird gelöscht**, driftende Manifeste werden bereinigt oder automatisch
   generiert.

Jede Phase ist so abgegrenzt, dass sie unabhängig reviewbar ist und bei Bedarf einzeln
revertiert werden kann. Phase P1 ist die einzige mit verändertem Laufzeitverhalten.

## Non-Goals

- Keine Hardware-Messungen, keine ERC/DRC-Durchläufe, keine Real-Device-Tests.
- Keine Änderung der analog hardwareseitigen Auslegung (wartet auf SAFE-02/03).
- Kein Wechsel auf `set_external_climate` (blockiert durch SW-05 / fehlendes Upstream-Release).
- Keine Architekturrewrites; nur gezielte Verbesserungen am Bestehenden.

## Phase P1 — Safety- und Security-Härtung (verhaltensändernd)

### P1.1 MQTT-Kommandopfad absichern (`firmware/esphome/packages/mqtt.yaml`)

- `username`/`password` als Substitutions einführen, Default = Platzhalter
  `idm-mqtt-CHANGE-ME`.
- Fail-closed: ein Pre-Compile-Check (in `tools/check_esphome.py`) weist die Konfiguration
  zurück, wenn die Substitution noch den Platzhalter enthält.
- TLS-Hinweis: Port-Substitution `${mqtt_port}` auf `8883` Default stellen und
  Dokumentation in `firmware/mqtt/topics.md` um einen „Threat Model"-Abschnitt ergänzen
  (mTLS via `certificate_authority` + `client_certificate` optional,LAN-Betrieb nur mit Auth).
- Tippfehler `discover_ip` → `discovery_ip` korrigieren (wird aktuell still ignoriert).

### P1.2 Web-UI fail-closed (`firmware/fake-sensor-bridge.yaml`, `fake-sensor-esphome.yaml`)

- Substitution `web_password` Default bleibt `CHANGE-ME-BEFORE-INSTALLATION`, aber der
  Web-Server darf nicht starten, solange der Platzhalter aktiv ist. Umsetzung: der
  Pre-Compile-Check in `tools/check_esphome.py` lehnt die Konfiguration ab, wenn
  `web_password` noch den Platzhalter enthält (primäres fail-closed Gate). Zusätzlich in
  `fake-sensor-webui.yaml` ein `on_boot`-Lambda, das `web_server` per
  `id(...).set_enabled(false)` deaktiviert, falls der Platzhalter zur Laufzeit erkannt wird
  (Verteidigung in depth für manuell geflashte Images).
- Dokumentation im `firmware/BUILDING.md`: wie das Passwort zu setzen ist und dass der Build
  ohne echte Setzung fehlschlägt.

### P1.3 Native Diagnose-Endpoint härten (`firmware/esp-idf/main/main.cpp`)

- `diagnostics_get_handler` (`main.cpp:493`) bekommt eine Auth-Ebene mit zwei Stufen:
  (a) Ist `IDM_API_TOKEN` gesetzt → vollständiges Bearer-Gate wie bei Mutationen
  (`authorize_mutation`), fehlender/falscher Token → 401. (b) Ist kein Token gesetzt → der
  Endpoint antwortet weiterhin, aber mit **redactierten** Feldern: `command_source` →
  `"redacted"`, `effective_humidity/temperature/dew_point_c` → `null`,
  `mutating_api_enabled` bleibt sichtbar (operativ nötig), `output_attempts/failures`,
  `bridge_state`, `safe_active/stale/fault`, `uptime`, `free_heap` bleiben sichtbar
  (Betriebsdiagnose ohne Sensordaten-Leak). Umsetzung: Hilfsfunktion
  `redact_diagnostics(root)` in `main.cpp`, gesteuert durch Token-Präsenz.
- Vertragstest in `tests/test_esp_idf_native.py`: Presence-Check der Redact-Logik und des
  Bearer-Gates im Quelltext (String-Fragmente `redact_diagnostics`, Token-Pfad-Zweig).

### P1.4 OTA sichern (`firmware/esp-idf/main/main.cpp` + `Kconfig.projbuild`)

- `confirm_pending_ota_image()` (`main.cpp:996`) markiert ein neues Image **nicht mehr auf
  jedem Boot** automatisch als gültig. Stattdessen wird `esp_ota_mark_app_valid_cancel_rollback()`
  erst aufgerufen, wenn **beide** Bedingungen erfüllt sind:
  (1) `s_output_hardware.ready == true` (I2C-Ausgang initialisiert) **oder** die
  dokumentierte I2C-Retry-Zeit `I2C_RETRY_MS` seit Boot abgelaufen ist, sodass ein
  dauerhaft kaputter Bus das Gerät nicht im Pending-Zustand festhält (dann Markierung mit
  Warnlog, weil die Analog-Ausgabe bewusst nicht verfügbar ist, aber Steuerung/Stromausfall-
  Schutz weiterlaufen); **und**
  (2) seit Boot sind mindestens `CONFIG_IDM_OTA_HEALTH_SECONDS` (Default 30) Sekunden
  vergangen und der Watchdog wurde in dieser Zeit regelmäßig zurückgesetzt (implizit durch
  den laufenden Main-Loop).
  Konkret: neue Funktion `try_confirm_ota_image(uint32_t boot_ms)` im Main-Loop, die das
  einmalig machbare `s_ota_confirmed`-Flag setzt und nur dann markiert. Bei einem Image, das
  den Main-Loop nicht erreicht (Crash-Loop), greift der ESP-IDF-Rollback-Mechanismus
  automatisch nach dem nächsten Reset.
- OTA-URL-Allowlist: neue Kconfig `CONFIG_IDM_OTA_ALLOWED_HOST` (Default leer = beliebige
  HTTPS, rückwärtskompatibel). Wenn gesetzt, `ota_post_handler` lehnt URLs ab, deren Host
  nicht passt. `firmware/esp-idf/README.md` dokumentiert die Empfehlung (Pin auf Vendor-CDN).
- Hinweis auf Secure-Boot-v2 + signiertes Image in `sdkconfig.defaults`-Kommentar (aktivieren
  bleibt Maintainer-Entscheid im Flash-Prozess; wir dokumentieren es nur).

### P1.5 `cooling_inhibit` mit Stale-Interlock (`homeassistant/blueprints/automation/cooling_inhibit.yaml`)

- Zustands-Trigger auf `unavailable`/`unknown` des Margin-Sensors ergänzen.
- Bei nicht-numerischem Margin → sicherer Default „Inhibit aktiv" (Fail-safe).
- `assert`-Guard, dass `clear_above > inhibit_below` (vor Aktionen).
- `id:` an allen Triggern konsistent setzen.

### P1.6 `fake-sensor-automation.yaml` auf Taupunkt umstellen

- Auswahl nach maximalem Taupunkt (gleiche Magnus-Formel wie `room_*.yaml`), nicht nach RH.
- Moderne `triggers:`/`action:`-Syntax, parametrisierte Entity-IDs.
- Fail-closed: fehlende/`unavailable`-Quellen → publish überspringen und Status-Sensor setzen.

**Vertragstests:** Für jedes P1-Item wird ein Test ergänzt oder ausgeweitet
(`tests/test_mqtt_contract.py`, `test_homeassistant_blueprints.py`,
`test_esp_idf_native.py`, C++ wo passend), der das fail-closed-Verhalten sichert.

## Phase P2 — Korrektheit und Robustheit

### P2.1 Command-Quality-Gating im ESPHome-Bridge angleichen
- `IdmBridgeCore::set_values` (`idm_bridge_core.h:57`) um `minimum_command_quality`-Gate
  erweitern, analog zu `native_runtime.h:105`. Konfigurierbar, Default 0 (rückwärtskompatibel,
  keine Überraschung) — der Native-Default 50 bleibt unangetastet, dokumentiert aber die
  Divergenz explizit, falls unterschiedliches Verhalten gewünscht ist. Entschieden: Default 0
  im ESPHome-Pfad + klarer Kommentar; Native behält 50. Test ergänzt.

### P2.2 Auswahllogik auf „kleinster Taupunktabstand" + Äquivalenz-Test
- Auswahl weiterhin `argmax(dew_point)` belassen ( Performanz, getestet), aber
  Äquivalenzannahme (shared pipe) in einen **expliziten Test** gießen
  (`tests/test_climate_template.py`): für konstantes `cooling_flow_temperature` gilt
  `argmax(dew_point) == argmin(margin)`. Code-Kommentar im Package erklärt die Annahme.
- Tie-Verhalten dokumentieren (erstes Raum in Iterationsreihenfolge gewinnt).

### P2.3 `failsafe.h` Dead Code entfernen + State-Machine-Doku korrigieren
- `firmware/common/failsafe.h` löschen (kein Consumer, siehe Audit).
- `docs/firmware/state-machine.md` und `wiki/State-Machine.md` auf die echte
  `BridgeState`-Maschine (`idm_bridge_core.h:8`) umstellen.
- `check_repo.py` ggf. anpassen, falls es `failsafe.h` referenziert (tut es aktuell nicht).

### P2.4 Room-Templates deduplizieren + Metadata
- 20 nahezu identische `homeassistant/examples/rooms/room_*.yaml` werden durch einen
  Generator ersetzt: `tools/generate_room_examples.py` (statt des aktuellen Einzeiler-Stubs)
  erzeugt sie aus einer Vorlage. `make`-Target `generate-rooms` ergänzt.
- Jedes Template erhält `device_class: temperature`, `state_class: measurement`, und eine
  `availability`-Template, die auf numerische + frische Quellen prüft.

### P2.5 Kleinere Firmware-Korrekturen (Bündel)
- `idm_analog_output_core.h`: `set_humidity_code_range` erzwingt `min < max`, sonst Fault.
- `idm_bridge.cpp` Legacy-Float-Pfad: bei nicht-ready Output Fault setzen statt still zu
  akzeptieren.
- Doppelten `tick()`-Aufruf im Native-Loop bereinigen (`main.cpp:1089` + `apply_outputs_once`).
- Wi-Fi-Reconnect mit Backoff (`main.cpp:911`): exponentiell bis 30 s.
- `mqtt.reboot_timeout`-Prüfung auf `api`/`wifi` konsistent halten (Kommentar, wo ESPHome-
  Defaults greifen).

**Tests:** je ein Unit-Test pro Korrektur, der den alten Pfad abdeckt.

## Phase P3 — CI, Tools, Qualitätsgate

### P3.1 Workflow-Härtung (`.github/workflows/`)
- `permissions: { contents: read }` auf `esphome.yml`, `docs.yml`, `kicad-check.yml`,
  `yaml-check.yml` (workflow-level).
- `concurrency: { group: <name>-${{ github.ref }}, cancel-in-progress: true }` auf `docs.yml`.
- Aktionen an SHA pinnen (Kommentar mit Version). `ibiqlik/action-yamllint@v3` → offiziell
  gepflegte Variante oder SHA-Pin.
- `docs.yml`: expliziten Step `command -v g++` zum frühen Fail, falls Runner-Image wechselt.

### P3.2 CI-Anforderungen bündeln + cachen
- `requirements-ci.txt` neu (jinja2, jsonschema, pytest, pyyaml); `docs.yml` und
  `yaml-check.yml` nutzen `setup-python` mit `cache: pip` dagegen.
- Pfad-Filter in `esphome.yml` um `firmware/components/**`, `firmware/common/**`,
  `firmware/webui/**`, `homeassistant/**` erweitern.

### P3.3 `make check-esphome` Semantik dokumentieren
- Umbenennung ist riskant (Konvention); stattdessen klare Hilfe in `Makefile`-Kommentar und
  `firmware/BUILDING.md`: „`check-esphome` validiert Konfigurationssyntax, `build-esphome`
  kompiliert."

### P3.4 `tools/check_repo.py` robuster machen
- Wo immer möglich strukturelle Prüfungen statt reiner Substring-Suche
  (YAML/JSON parsen, Felder prüfen). Substring-Prüfungen auf Fälle beschränken, die sich
  nicht strukturieren lassen, mit Kommentar.
- README-Warnungs-Check auf stabilen Sentinel umstellen
  (`<!-- SAFETY:NOT_PRODUCTION -->` in README ergänzen, Check sucht danach).
- `make check` darf weiterhin grün bleiben; es werden **keine** bestehenden Vertragsstrings
  entfernt, nur robuster geprüft.

### P3.5 Test-Abdeckung MQTT-Schemas + Blueprint-Aktionen
- `tests/test_mqtt_contract.py`: `Draft202012Validator.check_schema` für jedes Schema +
  Validierung eines Beispiel-Payloads.
- `tests/test_homeassistant_blueprints.py`: strukturelle Assertions auf `actions`-Bäume und
  `!input`-Defaults.

## Phase P4 — Dokumentation und Hygiene

### P4.1 README-Links reparieren
- `README.md:129`: `docs/#/safety` und `docs/#/validation` sind kaputt (Site liegt in
  `pages/`). Korrekte Ziele: `pages/#/cooling-safety` bzw. `pages/#/validation` (oder
  statische Markdown-Pfade). Prüfen, ob `pages/validation.md` existiert, sonst anlegen bzw.
  auf `docs/validation.md` zeigen.

### P4.2 CHANGELOG + Version
- `## 0.2.0`-Sektion ergänzen (Manifest deklariert 0.2.0, CHANGELOG springt noch von
  Unreleased auf 0.1.0). Inhalt: die bereits committeten SW-/FW-Backlog-Items plus die
  yamllint-Konfigurationskorrektur (`50242a1`).
- Unter „Unreleased" die P1–P4-Änderungen dieses Branches eintragen.

### P4.3 CONTRIBUTING + Doku-Kleinigkeiten
- `CONTRIBUTING.md`: `jsonschema` in die `pip install`-Zeile aufnehmen.
- ADR-Status-Datum-Hinweis (optional); `docs/repository-settings.md` Wortlaut
  „IDM Navigator" vereinheitlichen.
- `dew_point.py` um `t <= -243.12`-Guard ergänzen (Konsistenz zu C++ `dew_point.h`).

### P4.4 Manifeste automatisch generieren, Dead Code entfernen
- `FILE_INVENTORY.md` und `DEVELOPER_EDITION_MANIFEST.txt`: eins löschen (Redundanz), das
  andere über ein `tools/generate_file_inventory.py` aus `git ls-files` erzeugen.
  `check_repo.py` erhält einen Check, dass die committierte Datei aktuell ist
  (Drift-Schutz).
- `firmware/common/failsafe.h` (bereits in P2.3), `generate_room_examples.py`-Stub (wird in
  P2.4 zur echten Funktion), `hardware/fake-sensor/firmware/idm_fake_sensor_output.h`
  (Orphan) löschen.
- SPDX-Header (`// SPDX-License-Identifier: MIT`) in alle Firmware-`.cpp`/`.h` aufnehmen,
  da `firmware/LICENSE` MIT ist.

### P4.5 Checklisten-Sync (`REL-03`)
- `work-packages/WP04-documentation/README.md`: die dort noch ungeprüften, aber laut
  `README.md` erledigten Punkte abhaken oder WP04 zum reinen Verweis auf `TASKS.md` machen.

## Build-Sequenz und Verifikation

Jede Phase endet obligatorisch mit:

```bash
make check           # check_repo.py + dew_point.py + pytest (alle Tests grün)
make check-esphome   # ESPHome-Konfig-Syntax OK
```

`make build-esp-idf` und `make build-esphome` (volle Kompilierung) werden am Ende von P1
und P2 einmal ausgeführt, sofern die Toolchains lokal vorhanden sind; fehlen sie, wird das
im Commit-Vermerk dokumentiert und bleibt der CI überlassen (die CI-Jobs decken es ab).

Commit-Struktur: ein Commit pro Phase (`P1-safety-hardening`,
`P2-correctness-robustness`, `P3-ci-tools`, `P4-docs-hygiene`), innerhalb einer Phase
logisch zusammenhängende Änderungen. KEIN Mix funktionaler Änderungen mit CSV- oder
Zeilenende-Änderungen (`REPO-01`-Vorgabe).

## Risiko und Rollback

- **P1** ändert Laufzeitverhalten (fail-closed). Wer heute auf Platzhalter baut, muss danach
  echte Credentials setzen. Das ist beabsichtigt und wird in `CHANGELOG.md` + `BUILDING.md`
  prominent dokumentiert. Rollback = Revert des P1-Commits.
- **P2–P4** sind additiv bzw. Dokumentation; niedriges Risiko.
- Alle Änderungen bleiben unter der bestehenden `make check`-Decke. Neue Tests sichern die
  neuen Verträge ab, bevor der jeweilige Commit entsteht.

## Offen / nicht in diesem Branch

- `SW-05` (Upstream-Release `set_external_climate`) — fremdes Repo.
- `SAFE-01..05`, `HW-01..10`, `LAB-*`, `SITE-*` — Labor/Freigabe.
- Aktivierung von Secure-Boot-v2 im Flash-Prozess — Maintainer-Entscheid vor Auslieferung.

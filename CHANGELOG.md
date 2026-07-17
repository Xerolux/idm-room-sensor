# Changelog

## Unreleased
### Security and safety hardening (fail-closed)
- Made MQTT credentials fail-closed: device entry points ship sentinel placeholders and `tools/check_esphome.py` refuses to validate while they remain; documented the MQTT threat model and TLS/mTLS options.
- Made the local web UI fail-closed: the web server disables itself at runtime if `web_password` is still the shipped placeholder.
- Native diagnostics endpoint now requires a bearer token when `IDM_API_TOKEN` is set, and redacts sensor/command-provenance fields when no token is configured.
- OTA is now health-gated: a pending image is confirmed only after a configurable health window, never blindly at boot; added an optional `IDM_OTA_ALLOWED_HOST` allowlist.
- Added a stale/availability interlock and a run-time hysteresis guard to the cooling-inhibit blueprint.
- Rewrote the fake-sensor automation to select by dew point (not relative humidity) and to fail closed when no room has usable data.
### Correctness and robustness
- Added an optional command-quality gate to the ESPHome bridge (default 0, backward compatible) plus an `INVALID_QUALITY` error.
- Enforced `min < max` on the humidity DAC code range; the legacy float-output path now respects output readiness.
- Removed the duplicate `tick()` call in the native main loop and added Wi-Fi reconnect exponential backoff.
- Deleted the dead `failsafe.h` and rewrote the state-machine documentation to match the real `BridgeState` machine.
- Regenerated the 20 room templates via `tools/generate_room_examples.py` with `unique_id`, `device_class`, `state_class` and an availability template.
### CI, tooling and quality gate
- Added `permissions: contents: read`, concurrency cancellation, a g++ presence guard, and extended path filters to the workflows.
- Pinned every GitHub Action to a commit SHA; added a pinned `.github/requirements-ci.txt` with pip caching.
- Pinned README safety warnings to stable HTML-comment sentinels instead of brittle substring matching.
- Aligned `dew_point.py` with the C++ input domain (`t <= -243.12` guard).
- Added JSON-Schema validation (`check_schema` + representative payloads) for every MQTT schema.
### Documentation and hygiene
- Repaired the broken README Safety/Validation links.
- Documented the ESP-IDF OTA allowlist/health behaviour and diagnostics redaction.
- Documented the `check_esphome.py` syntax-only vs `--compile` distinction.

## 0.2.0 — Developer Edition
- Fixed critical-room selection to use the highest calculated dew point instead of humidity alone.
- Added `last_reported`-based stale-input detection and conservative fallback values.
- Kept the Home Assistant package compatible with released `idm-heatpump-hass` v0.8.2 via `write_register`.
- Aligned the `set_external_climate` reference with the native service now present on upstream main.
- Made the cooling-inhibit blueprint executable with configurable hysteresis actions.
- Made the critical-room and standalone stale-data blueprints executable.
- Added transition, fallback and blueprint-output regression tests.
- Added climate selection, source-age, fallback and publish diagnostics.
- Added restart, tie, unavailable-input and output-clamping regression tests.
- Pinned ESPHome 2026.6.5 and added manifest-based configuration and firmware builds.
- Migrated device names, OTA configuration and fallback captive portals to the pinned ESPHome release.
- Implemented the `idm_bridge` lifecycle, validation, timeout fallback, latched output faults and diagnostics.
- Replaced fake-sensor output placeholders with compiled MCP4725 and AD5242 drivers, atomic fallback handling and output diagnostics.
- Added versioned, CRC-protected calibration persistence with range validation, V1 migration, read-back verification and factory reset.
- Expanded native unit coverage for dew point, KTY interpolation, value quality, fallback transitions, clamping and calibration transforms.
- Rejected invalid embedded dew-point humidity inputs instead of silently clamping them to a potentially unsafe value.
- Added deterministic MQTT discovery, retained availability/state, versioned diagnostics, atomic command validation and broker-loss contract tests.
- Added an authenticated, fully local ESPHome web UI for status, command visibility, bounded calibration, confirmation-gated apply/reset actions and versioned diagnostic export.
- Replaced the native ESP-IDF dew-point demonstration with a build-tested bridge runtime providing real MCP4725/AD5242 I/O, quality and stale fallback, CRC-protected NVS calibration, Wi-Fi REST diagnostics, task watchdog, brownout protection and confirmation-gated HTTPS OTA.
- Synchronized firmware status and remaining physical validation gates across the README, project summary, work package, Docsify pages and Wiki.
- Added executable service-contract, dew-point, YAML and repository checks to CI.
- Fixed repository YAML lint configuration.

## Developer Edition 0.1.0 — WIP
- Added Docsify Pages and Wiki source.
- Added validation and community test plans.
- Added climate-engine examples.
- Added proposed `set_external_climate` implementation and test skeleton.
- Added manufacturing templates explicitly marked not released.

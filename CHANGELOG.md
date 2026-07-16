# Changelog

## Unreleased
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

## Developer Edition 0.1.0 — WIP
- Added Docsify Pages and Wiki source.
- Added validation and community test plans.
- Added climate-engine examples.
- Added proposed `set_external_climate` implementation and test skeleton.
- Added manufacturing templates explicitly marked not released.

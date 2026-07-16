# ESPHome

ESPHome exposes source selection, calibration, fallback and diagnostics over
the native API and, when explicitly enabled, MQTT. Reusable MQTT packages
define retained discovery/state, deterministic availability and versioned
diagnostic payloads. The authoritative topic contract is
[`firmware/mqtt/topics.md`](../../firmware/mqtt/topics.md).

The fake-sensor targets also expose a local ESPHome web-server v3 interface
with embedded assets, authentication, status and command grouping, bounded
calibration editing, confirmation-gated apply/reset actions and a versioned
diagnostic export. Its contract and commissioning notes are in
[`firmware/webui/README.md`](../../firmware/webui/README.md).

## Status

- [x] Configuration reviewed
- [x] All supported manifests validated
- [x] Representative firmware images compile tested
- [x] Local web UI contract and diagnostic export tested
- [ ] Hardware bench tested
- [ ] Real-device tested

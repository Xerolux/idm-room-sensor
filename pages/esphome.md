# ESPHome

The ESPHome targets expose source selection, quality, fallback and diagnostics
through the native API and optionally MQTT. The fake-sensor variants drive the
MCP4725 and AD5242 output stages, persist versioned calibration and provide a
local authenticated web UI with confirmation-gated calibration and reset.

All supported manifests and representative firmware images compile against
the pinned ESPHome release. Hardware calibration, broker-loss fault injection
and real-device validation remain open.

The separate [native ESP-IDF target](native-esp-idf.md) implements the same
bridge core without ESPHome.

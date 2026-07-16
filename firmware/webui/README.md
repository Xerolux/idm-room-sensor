# Local web UI

The fake-sensor firmware exposes ESPHome web-server version 3 on port 80.
The page assets are embedded in the firmware, browser OTA is disabled for this
web server, and HTTP basic authentication is required.

Before flashing, override the deliberately conspicuous example password
`CHANGE-ME-BEFORE-INSTALLATION` in the device substitutions. The web UI must
only be reachable from a trusted commissioning network; HTTP basic
authentication does not provide transport encryption.

## Sections

- **Status:** effective humidity and temperature, bridge state, fallback,
  staleness, output readiness, faults, RSSI and uptime.
- **Commands:** the current humidity and temperature command plus source and
  quality.
- **Calibration:** editable DAC endpoints, KTY resistance endpoints,
  inversion, stored calibration state and a reload action.
- **Safety:** explicit confirmation plus apply and factory-reset actions.
- **Diagnostics:** raw output values and a versioned JSON export.

The machine-readable UI contract is
[`manifest.json`](manifest.json), validated by [`schema.json`](schema.json).
The implementation is
[`../esphome/packages/fake-sensor-webui.yaml`](../esphome/packages/fake-sensor-webui.yaml).

## Confirmation flow

Calibration changes are only editor values until **IDM Web Apply Calibration**
is pressed. Applying calibration and resetting to factory calibration both
require **IDM Web Confirm Dangerous Action** to be enabled first. Confirmation
expires after 60 seconds and is consumed by the next apply or reset attempt.
Invalid endpoint ordering is rejected by the firmware and the editor is
reloaded from the active calibration.

## Diagnostic export

The text-sensor endpoint is:

`/text_sensor/idm_webui_diagnostic_export`

It returns the standard ESPHome entity response whose `value` contains a JSON
document. That nested document follows
[`diagnostic-export.schema.json`](diagnostic-export.schema.json). The export
updates every 30 seconds and can also be refreshed from the web UI.

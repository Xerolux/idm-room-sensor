# MQTT contract

The ESPHome images use one deterministic topic prefix per device:

```text
idm/<device-name>
```

MQTT support is compiled into the supported images but disabled by default.
This prevents a placeholder broker from becoming part of the safety path. To
enable it, override the substitutions in the device entry point:

```yaml
substitutions:
  mqtt_enabled: "true"
  mqtt_broker: 192.0.2.10
  mqtt_port: "8883"
  mqtt_username: !secret mqtt_username
  mqtt_password: !secret mqtt_password
  mqtt_topic_prefix: idm/plant-room-bridge
```

Use a unique prefix for every physical device. Do not put secrets in a tracked
configuration file.

## Threat model (security)

The MQTT command topics (`<prefix>/command/...`) drive the analog bridge that
emulates the IDM heat-pump room sensor input, so anyone who can publish to the
broker can push humidity and temperature values to the heat pump. The default
configuration therefore **refuses to validate** while credentials are still the
shipped placeholders (`idm-mqtt-CHANGE-ME`); see the fail-closed gate in
`tools/check_esphome.py`.

Two deployment tiers are supported:

- **Trusted LAN** — set `mqtt_username` and `mqtt_password` to strong unique
  values and keep `mqtt_port: "1883"` only if the broker is on an isolated,
  trusted network segment. Plaintext MQTT without authentication is never
  acceptable on the command path.
- **Anything reachable beyond a trusted LAN** — use TLS on port `8883` (the
  shipped default). For mutual TLS, extend the `mqtt:` block with
  `certificate_authority`, `client_certificate` and `client_key`, and point
  `mqtt_broker` at the broker hostname that matches the CA.

The shared `mqtt:` block lives in
[`firmware/esphome/packages/mqtt.yaml`](../esphome/packages/mqtt.yaml). The
connection cannot reboot the controller (`reboot_timeout: 0s`), so a broker or
network outage never interrupts the local fail-safe state machine.

## Delivery rules

- Discovery, availability, state and aggregate diagnostics are retained.
- Commands are QoS 1 and **must never be retained**.
- MQTT uses a clean session. Commands queued while a device is offline are
  intentionally discarded; current state is recreated from retained state
  publishes after reconnect.
- The MQTT connection cannot reboot the controller (`reboot_timeout: 0s`).
  Loss of the broker therefore does not interrupt the local fail-safe state
  machine.
- ESPHome log messages are not published to MQTT.

Before using an existing broker, clear any retained message below
`<prefix>/command/#`. A retained command from a third-party publisher cannot
be distinguished from a new command by ESPHome.

## Core topics

| Topic below `<prefix>` | Direction | QoS | Retained | Payload |
| --- | --- | ---: | :---: | --- |
| `availability` | device → broker | 1 | yes | `online` or `offline` |
| `diagnostics/state` | device → broker | 1 | yes | Versioned JSON diagnostics |
| `command/climate/set` | controller → bridge | 1 | **no** | Atomic humidity/temperature command |
| `command/humidity/set` | controller → bridge | 1 | **no** | ESPHome number value |
| `command/humidity/state` | bridge → broker | 1 | yes | Last number value |
| `command/temperature/set` | controller → bridge | 1 | **no** | ESPHome number value |
| `command/temperature/state` | bridge → broker | 1 | yes | Last number value |

The atomic bridge command is preferred because humidity and temperature are
validated and applied together:

```json
{
  "humidity": 62.5,
  "temperature": 22.4,
  "source": "home_assistant",
  "quality": 95
}
```

Invalid JSON values, out-of-range values, an invalid quality or an invalid
source activate the configured safe fallback. The bridge then reports
`command_source: "mqtt_invalid"` and `command_quality: 0`.

The payload contract is
[`climate-command.schema.json`](climate-command.schema.json).

## Availability and restart behaviour

The retained birth message sets `<prefix>/availability` to `online`. Graceful
shutdown and the broker Last Will set it to `offline`. Every discovered
entity uses this global availability contract.

Home Assistant discovery is retained below `homeassistant/`, with MAC-derived
unique IDs and device-name-derived object IDs. On every MQTT reconnect the
device republishes aggregate diagnostics. Regular entity states are also
retained, so a Home Assistant restart reconstructs the same entities and
their most recent state without waiting for the next sensor interval.

## Diagnostics

Bridge diagnostics follow
[`bridge-diagnostics.schema.json`](bridge-diagnostics.schema.json). They
include the fail-safe state, stale/fault flags, effective output values,
command source/quality, driver status and calibration provenance.

Room-sensor diagnostics follow
[`room-diagnostics.schema.json`](room-diagnostics.schema.json). They include
temperature, humidity, device status, Wi-Fi signal and uptime.

Diagnostics are published on connect, every 30 seconds and after relevant
bridge commands or calibration changes.

## Broker-loss acceptance test

1. Subscribe to `<prefix>/#` with retained-message display enabled.
2. Start the device and verify retained `availability=online` plus
   `diagnostics/state`.
3. Restart Home Assistant or a clean test subscriber. Discovery and state
   must reappear without changing a sensor value.
4. Disconnect the device from the network without a graceful shutdown.
5. After the broker keepalive timeout, verify retained
   `availability=offline`.
6. Restore the connection. Verify `availability=online`, one fresh
   diagnostics payload and no replay of commands sent while the device was
   offline.

# MQTT

The firmware contract is specified in
[`firmware/mqtt/topics.md`](../../firmware/mqtt/topics.md) and implemented by
the reusable ESPHome MQTT packages.

The safety-relevant rules are:

- commands use QoS 1 but are never retained;
- a clean session prevents offline command queues from being replayed;
- retained discovery and state restore entities after a broker or Home
  Assistant restart;
- retained birth, shutdown and Last-Will messages expose connection loss via
  one deterministic `availability` topic;
- broker loss never reboots the device and does not bypass the local stale
  timeout or analog fallback;
- invalid atomic commands force the bridge into its safe fallback and are
  visible as source `mqtt_invalid`, quality `0`.

Payload schemas:

- [`climate-command.schema.json`](../../firmware/mqtt/climate-command.schema.json)
- [`bridge-diagnostics.schema.json`](../../firmware/mqtt/bridge-diagnostics.schema.json)
- [`room-diagnostics.schema.json`](../../firmware/mqtt/room-diagnostics.schema.json)

## Status

- [x] Configuration and payload contract reviewed
- [x] ESPHome configuration and compile tested
- [ ] Broker-loss bench tested
- [ ] Real-device tested

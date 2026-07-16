# MQTT loss and restart test

Status: NOT RUN

This test verifies discovery replay, retained availability, command safety and
the independence of the local fail-safe state machine from the broker.

## Equipment and setup

- Device flashed from one of the supported ESPHome MQTT manifests.
- MQTT enabled with a unique topic prefix and a dedicated test account.
- MQTT broker with retained-message inspection and connection logs.
- Clean MQTT subscriber that can show QoS and retained flags.
- Home Assistant test instance for discovery checks.
- Means to interrupt Wi-Fi without gracefully shutting down the device.
- For the analog bridge: isolated load or measurement fixture; do not connect
  unverified outputs to a heat pump.

Record firmware revision, broker/version, Home Assistant version, device MAC,
topic prefix, configured stale timeout and broker keepalive behaviour.

## Procedure

1. Clear retained messages below `<prefix>/command/#`.
2. Start the broker, subscriber and device.
3. Verify retained `<prefix>/availability=online`, retained entity state and a
   retained `<prefix>/diagnostics/state` payload matching the applicable JSON
   schema.
4. Record every Home Assistant entity unique ID and object ID.
5. Restart Home Assistant with the device still online. Verify that the same
   entities and current retained states reappear.
6. Restart the device. Verify that the same entities reappear and availability
   changes back to `online`.
7. Interrupt the device network without a graceful shutdown. Keep the device
   powered.
8. While offline, publish a QoS 1, non-retained command and confirm that the
   local bridge reaches its configured stale fallback independently.
9. Verify that the broker publishes the retained Last-Will state
   `<prefix>/availability=offline`.
10. Restore the network. Verify `online`, a fresh diagnostic payload and that
    the command published while offline is not applied.
11. Publish an invalid atomic climate command and verify safe fallback plus
    `command_source=mqtt_invalid` and `command_quality=0`.
12. Inspect the broker and confirm there are no retained command messages.

## Acceptance criteria

- Discovery payloads and state payloads are retained; command payloads are not.
- Entity unique IDs and object IDs are identical across Home Assistant and
  device restarts.
- Abrupt connection loss becomes visible as retained `offline` no later than
  the broker's documented keepalive timeout (normally at most 45 seconds for
  the configured 30-second keepalive).
- Reconnect publishes `online` and fresh diagnostics within 10 seconds after
  MQTT connection establishment.
- A command published while the clean-session client is offline is not
  replayed after reconnect.
- Broker loss does not reboot the device or suppress the local stale timeout,
  output-fault handling or safe fallback.
- Invalid commands never reach the normal active state.

Save broker logs, Home Assistant screenshots, subscriber output and all timing
observations. Enter observations in `validation/raw-data/mqtt-loss.csv`; keep
the result status `NOT RUN` until real hardware and a real broker were used.

# Native ESP-IDF

The native firmware now implements the analog bridge independently of
ESPHome: MCP4725/AD5242 I/O, validated and stale-safe commands, CRC-protected
NVS calibration, Wi-Fi REST diagnostics, watchdog/brownout protection and
confirmation-gated HTTPS OTA with two rollback slots.

See
[`firmware/esp-idf/README.md`](https://github.com/Xerolux/IDM-Smart-Climate-Platform/blob/main/firmware/esp-idf/README.md)
for the pinned build, configuration and API examples.

- [x] Host runtime and static safety contracts tested
- [x] ESP32-C3 firmware and dual-OTA image compile tested
- [ ] Prototype flashed and analog outputs calibrated
- [ ] Watchdog, brownout and OTA recovery fault-injection tested
- [ ] Real-device tested
- [ ] Community reviewed

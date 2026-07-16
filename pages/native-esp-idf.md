# Native ESP-IDF

The native ESP-IDF target is no longer a dew-point demonstration. It provides:

- MCP4725 humidity and AD5242 temperature output;
- quality, range and stale-command validation with conservative fallback;
- CRC-protected NVS calibration and V1-to-V2 migration;
- Wi-Fi reconnect and a bearer-authenticated REST API;
- versioned JSON diagnostics;
- task watchdog and brownout configuration;
- confirmation-gated HTTPS OTA with dual-slot rollback support.

The pinned PlatformIO/ESP-IDF build and 70 repository tests pass. The firmware
image occupies about 13% of either 1,984 KiB OTA slot. Flashing prototype
hardware, analog calibration, watchdog fault injection and OTA interruption
testing remain required before installation.

Configuration and API examples are maintained in
[`firmware/esp-idf/README.md`](https://github.com/Xerolux/IDM-Smart-Climate-Platform/blob/main/firmware/esp-idf/README.md).

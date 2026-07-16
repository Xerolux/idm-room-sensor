# Native ESP-IDF firmware

This target implements the fake-sensor bridge directly on ESP-IDF. It shares
the host-tested dew-point, quality, bridge-state, calibration and analog-output
cores with the ESPHome variant.

> [!CAUTION]
> This remains experimental firmware for unverified prototype hardware. It
> does not replace an independent, hard-wired condensation guard.

## Implemented behavior

- boots into configured fail-safe humidity and temperature values before
  networking starts;
- drives an MCP4725 humidity DAC and AD5242 temperature digipot over I2C;
- validates humidity, temperature, source length and command quality;
- returns to fail-safe output after the command stale timeout;
- latches output failures, retries the I2C path and reports recovery;
- stores CRC-protected calibration V2 records in NVS, verifies writes and
  migrates valid V1 records;
- connects as a Wi-Fi station when an SSID is configured and otherwise remains
  local-only;
- exposes versioned diagnostics and authenticated mutation endpoints;
- uses the ESP task watchdog, brownout detection and OTA rollback support;
- accepts only HTTPS OTA URLs using the ESP certificate bundle.

## Build

PlatformIO 6.1.19 and the pinned pioarduino ESP32 platform are used for the
reproducible build:

```bash
python3 -m pip install platformio==6.1.19
make build-esp-idf
```

The equivalent direct command is:

```bash
platformio run --project-dir firmware/esp-idf
```

The generated `sdkconfig.idm-native` and `.pio/` directory are local build
artifacts and are ignored.

## Configuration

Run PlatformIO's ESP-IDF menu configuration and open **IDM native firmware**:

```bash
platformio run --project-dir firmware/esp-idf --target menuconfig
```

At minimum, review:

- Wi-Fi SSID/password and hostname;
- a random HTTP bearer token of at least 16 characters;
- I2C pins and device addresses;
- fail-safe values, stale timeout and minimum command quality;
- factory analog calibration.

An empty Wi-Fi SSID keeps the target local-only. An empty API token disables
every mutating endpoint. The diagnostic endpoint remains read-only and does
not require authentication.

The HTTP API itself is not TLS-protected. Operate it only on a trusted,
isolated commissioning network; the bearer token must not be exposed across
an untrusted network.

## HTTP API

Replace `DEVICE`, `TOKEN` and values below as appropriate.

Read the payload defined by
[`diagnostics.schema.json`](diagnostics.schema.json):

```bash
curl http://DEVICE/api/v1/diagnostics
```

Submit an atomic climate command:

```bash
curl -X POST http://DEVICE/api/v1/climate \
  -H "Authorization: Bearer TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"humidity":58.0,"temperature":23.5,"quality":100,"source":"commissioning"}'
```

Immediately request the configured safe fallback:

```bash
curl -X POST http://DEVICE/api/v1/fallback \
  -H "Authorization: Bearer TOKEN"
```

Calibration writes are bounded by the same core validation as the ESPHome
target and require an explicit confirmation header:

```bash
curl -X POST http://DEVICE/api/v1/calibration \
  -H "Authorization: Bearer TOKEN" \
  -H "X-IDM-Confirm: apply-calibration" \
  -H "Content-Type: application/json" \
  -d '{"humidity_code_min":0,"humidity_code_max":4095,"temperature_resistance_min":650.0,"temperature_resistance_max":3000.0,"temperature_code_inverted":false}'
```

Reset calibration to configured factory values:

```bash
curl -X POST http://DEVICE/api/v1/calibration/reset \
  -H "Authorization: Bearer TOKEN" \
  -H "X-IDM-Confirm: reset-calibration"
```

Queue a firmware image from a trusted HTTPS host:

```bash
curl -X POST http://DEVICE/api/v1/ota \
  -H "Authorization: Bearer TOKEN" \
  -H "X-IDM-Confirm: firmware-update" \
  -H "Content-Type: application/json" \
  -d '{"url":"https://example.invalid/idm-native.bin"}'
```

The device validates a pending OTA image on successful startup. Interrupted or
unbootable OTA images are handled by the configured dual-OTA partition table
and bootloader rollback support. Recovery behavior still requires a physical
fault-injection test before real installation.

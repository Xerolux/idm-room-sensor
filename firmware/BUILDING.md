# Reproducible ESPHome builds

The supported ESPHome version is pinned in `firmware/requirements.txt`.
Pre-release versions are intentionally excluded from the supported build.

## Local validation

Create a virtual environment, install the exact dependency and validate every
configuration in the firmware manifest:

```bash
python3 -m venv .venv-esphome
.venv-esphome/bin/python -m pip install -r firmware/requirements.txt
.venv-esphome/bin/python tools/check_esphome.py
```

Compile every target and the reusable-package fixture:

```bash
.venv-esphome/bin/python tools/check_esphome.py --compile
```

Build output is generated below the repository-level `.esphome/` directory and
is not committed. On WSL, placing temporary build files on the Linux
filesystem is substantially faster:

```bash
ESPHOME_DATA_DIR=/tmp/idm-esphome-build \
  .venv-esphome/bin/python tools/check_esphome.py --compile
```

GPIO8 and GPIO9 are currently used for I²C on the ESP32-C3 prototypes. ESPHome
correctly warns that they are strapping pins. This warning is retained until
the hardware review confirms the boot-state circuitry and external pull-ups;
it must not be suppressed merely to produce a quiet build.

# Reproducible native ESP-IDF build

The native target pins PlatformIO 6.1.19, pioarduino platform 55.03.39 and
ESP-IDF 5.5.4. Install PlatformIO and compile the target with:

```bash
python3 -m pip install platformio==6.1.19
make build-esp-idf
```

Configuration, REST endpoints, safety behavior and commissioning examples are
documented in [`esp-idf/README.md`](esp-idf/README.md). The native target is
also compiled in CI. Its generated `.pio/` directory and environment-specific
`sdkconfig.*` files are not committed; `sdkconfig.defaults`, Kconfig and the
partition table form the reproducible source configuration.

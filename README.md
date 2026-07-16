<div align="center">

# IDM Smart Climate Platform

**Open-source climate bridge, room sensing and condensation protection for IDM heat pumps**

![Status](https://img.shields.io/badge/status-WORK%20IN%20PROGRESS-orange)
![Hardware](https://img.shields.io/badge/hardware-UNTESTED-red)
![ESPHome](https://img.shields.io/badge/ESPHome-prototype-blue)
![Home Assistant](https://img.shields.io/badge/Home%20Assistant-integration-18BCF2)
![KiCad](https://img.shields.io/badge/KiCad-9-314CB0)

</div>

> [!CAUTION]
> **Experimental developer edition. Not production-ready and not validated on a real IDM installation.**  
> Do not connect unverified hardware to a heat pump. Independent condensation protection remains mandatory for cooling tests.

## What this project contains

The platform combines three hardware approaches and a software-first GLT bridge:

| Component | Purpose | Status |
|---|---|---|
| ESP Room Sensor | Local SHT45 sensing, ESPHome, 0–10 V humidity and real KTY81-210 | Design prototype |
| Classic Sensor | Compact local sensor without Wi-Fi | Design prototype |
| Fake Sensor / Bridge | Receives HA/KNX/MQTT values and emulates the original IDM interface | Design prototype |
| Climate Engine | Selects the most critical room by dew-point margin | Functional HA example |
| `set_external_climate` patch | Native service proposal for `idm-heatpump-hass` | Ready for review, untested |
| Pipe Dew-Point Guard | Independent pipe-temperature/condensation safety concept | Documentation only |

## Confirmed original sensor information

The available LCN-FTW04 documentation identifies a combined room humidity/temperature sensor, a 0–10 V humidity output, KTY temperature element and 14–30 V DC supply. Only one room humidity sensor is intended for the Navigator; individual room temperatures may come from room units.

Current project terminal mapping must be verified against the exact IDM wiring diagram before connection:

| Function | Project signal |
|---|---|
| Supply | 14–30 V DC |
| Humidity | 0–10 V, working assumption 0–100 % RH |
| Temperature | KTY81-210 equivalent |
| Ground | Common reference |

## Preferred architecture

```text
KNX / ESPHome / Zigbee / MQTT room sensors
                    │
                    ▼
        Home Assistant Climate Engine
        - dew point per room
        - minimum dew-point margin
        - stale sensor detection
        - configurable fallback
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
 GLT / Modbus registers   Analog fallback bridge
 1650…1663 temperature    0–10 V humidity
 1692 humidity            KTY emulation
          │                   │
          └─────────┬─────────┘
                    ▼
               IDM Navigator

Independent pipe dew-point switch / cooling inhibit
```

## Project checklist

### Documentation and repository
- [x] Professional README
- [x] GitHub Pages / Docsify structure
- [x] Wiki source pages
- [x] Issue and pull-request templates
- [x] Security, contributing and code-of-conduct files
- [x] Community test forms
- [x] Architecture and safety documentation

### Home Assistant and IDM integration
- [x] Multi-room dew-point climate-engine package
- [x] Example automation for GLT register writes
- [x] Proposed `set_external_climate` implementation patch
- [x] Service schema and documentation draft
- [ ] Review against current `idm-heatpump-api` public interfaces
- [ ] Unit tests executed in upstream repository CI
- [ ] Real-device Modbus verification
- [ ] Confirm that the selected IDM configuration uses GLT values for cooling control

### ESPHome and firmware
- [x] ESPHome bridge configuration
- [x] Home Assistant-controlled temperature/humidity inputs
- [x] Stale-data timeout and conservative fallback concept
- [x] Dew-point calculation example
- [x] MCP4725 output component source
- [x] KTY lookup and calibration framework
- [ ] Compile against the selected ESPHome release
- [ ] Hardware-in-the-loop test
- [ ] OTA recovery test

### Hardware
- [x] Block-level schematics and KiCad project structure
- [x] Board outlines and component placement concepts
- [x] Preliminary BOM and JLCPCB templates
- [x] Enclosure STL/OpenSCAD prototypes
- [ ] Replace every prototype symbol/footprint with verified manufacturer footprint
- [ ] Complete ERC/DRC with KiCad 9
- [ ] Review switch-mode power-supply layout against regulator datasheet
- [ ] Route and validate analog ground/current return paths
- [ ] Manufacture prototype
- [ ] Validate 0–10 V linearity and load drive
- [ ] Validate KTY emulation excitation voltage/current
- [ ] EMC, ESD, thermal and long-term tests

### Real installation
- [ ] Measure the original sensor at multiple humidity points
- [ ] Measure IDM KTY input using a resistance decade
- [ ] Confirm safe open-circuit and short-circuit behavior
- [ ] Heating-only observation
- [ ] Cooling test with independent dew-point switch
- [ ] Multi-week logging and fault injection

## Quick start for contributors

1. Read [Safety](docs/#/safety) and [Validation Plan](docs/#/validation).
2. Choose a contribution from [Help Wanted](docs/#/help-wanted).
3. Do not mark measurements as verified without raw data and test conditions.
4. Open an issue using the measurement or hardware-test template.

## Repository layout

```text
hardware/                 KiCad prototypes, BOM, CPL templates, enclosures
firmware/                 ESPHome and custom component sources
homeassistant/            Climate engine, automations and dashboard examples
idm-heatpump-hass/        Upstream service patch and tests
pages/                    GitHub Pages / Docsify site
wiki/                     GitHub Wiki source pages
docs/                     Technical documentation and validation plans
manufacturing/            Shared fabrication notes and release checklist
.github/                  Workflows, issue templates and contribution automation
```

## Safety

This project is unaffiliated with IDM Energiesysteme. It is experimental open hardware. Incorrect humidity or temperature values can defeat condensation protection and cause water damage. Use an independent, hard-wired pipe dew-point switch during development and commissioning.

## Community Work Packages

- `work-packages/WP01-hardware`
- `work-packages/WP02-firmware`
- `work-packages/WP03-home-assistant`
- `work-packages/WP04-documentation`
- `work-packages/WP05-validation`

See [the master checklist](PROJECT_STATUS.md).

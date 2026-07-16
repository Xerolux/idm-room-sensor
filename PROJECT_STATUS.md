# Project status

> 🚧 Work in progress · experimental · not production validated

## Architecture
- [x] Local sensor, bridge and hybrid modes
- [x] GLT/Modbus-first strategy
- [x] Analog fallback strategy
- [x] Fail-safe behavior
- [ ] External architecture review

## Original sensor research
- [x] LCN-FTW04 documentation
- [x] One-sensor limitation documented
- [x] 14–30 V DC supply documented
- [x] 0–10 V humidity signal documented
- [x] KTY element documented
- [ ] Original output measured at several humidity points
- [ ] Navigator temperature input measured

## Hardware
- [x] ESP, Classic, Fake, Pipe and Gateway concepts
- [x] Test-point plan
- [x] Fabrication templates
- [ ] Electrical peer review
- [ ] Footprint peer review
- [ ] Complete routed PCB revision
- [ ] Prototype order and bench test
- [ ] Real installation test
- [ ] EMC pre-compliance test

## Firmware
- [x] ESPHome structure
- [x] MQTT model
- [x] Climate algorithms
- [x] Fail-safe state machine
- [ ] Build and flash target hardware
- [ ] Calibrate analog output
- [ ] Validate KTY emulation
- [ ] Long-term watchdog test

## Home Assistant
- [x] Sensor fusion examples
- [x] Critical-room selection
- [x] Dew-point templates
- [x] Dashboard examples
- [x] `set_external_climate` design
- [ ] Merge service into idm-heatpump-hass
- [ ] Real-device Modbus write test

## Documentation
- [x] Pages and Wiki sources
- [x] Ordering, validation and community guides
- [ ] Real photographs and oscilloscope captures
- [ ] First verified build guide

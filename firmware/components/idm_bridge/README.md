# IDM bridge component

`idm_bridge` owns the command lifecycle between Home Assistant/ESPHome and the
analog sensor-emulation outputs.

## Safety behaviour

- Startup immediately selects the configured fallback values.
- Temperature and humidity are accepted atomically.
- Non-finite or out-of-range values are rejected and replace both outputs with
  the fallback pair.
- The last valid command expires after `stale_timeout`.
- A reported hardware-output fault is latched and selects the fallback pair.
- Clearing an output fault does not restore an old command. A new valid command
  is required.
- The timeout calculation is safe across the 32-bit `millis()` wraparound.
- Stored calibration is accepted only after version, CRC and range checks.

The component exposes normalized `FloatOutput` ports:

- humidity: `0.0` for 0 % and `1.0` for 100 %
- temperature: `0.0` for -20 °C and `1.0` for 60 °C

The concrete DAC and KTY-emulation drivers and their calibrated transfer
functions are implemented by the optional atomic `analog_output` driver. It
writes the MCP4725 DAC register and AD5242 RDAC1 in one bridge update. If either
I²C transaction fails, the bridge latches an output fault and retries the safe
fallback pair.

The committed KTY table and resistance-to-code limits are prototype values, not
validated installation calibration. Enabling the driver therefore requires the
explicit `accept_unverified_kty_calibration: true` acknowledgement and remains
limited to isolated bench testing until SAFE-02 and SAFE-03 are complete.

## Actions

```yaml
- idm_bridge.set_values:
    id: climate_bridge
    humidity: 60
    temperature: 23

- idm_bridge.apply_fallback: climate_bridge

- idm_bridge.set_output_fault:
    id: climate_bridge
    active: true

- idm_bridge.set_calibration:
    id: idm_analog_outputs
    humidity_dac_min_code: 100
    humidity_dac_max_code: 3900
    temperature_resistance_min: 700
    temperature_resistance_max: 3200
    temperature_code_inverted: false

- idm_bridge.reset_calibration: idm_analog_outputs
```

`set_output_fault` is intended for an output driver that detects an I/O or
calibration failure.

## Analog protocol

- MCP4725: DAC-register write command `0x40` followed by the 12-bit code.
- AD5242: instruction byte `0x00` selects RDAC1 without reset or shutdown,
  followed by the 8-bit wiper code.

The humidity code range and temperature resistance endpoints are configurable
so measured calibration can replace the prototype defaults without changing
driver code.

## Calibration persistence

The YAML values are immutable factory defaults. Runtime updates are stored as a
version-2 ESPHome preference record with magic, size, flags and CRC-32. The
component reads the record back before activating it. Invalid data selects the
factory values and publishes `invalid_using_factory`; it never restores a
legacy record when a current record is present but corrupt.

Version-1 gain/offset records are migrated once and rewritten as version 2.
`calibration_preference_id` must remain stable across YAML ID renames. See
[`docs/firmware/calibration-storage.md`](../../../docs/firmware/calibration-storage.md)
for the exact format, ranges and status values.

Protocol implementation references:

- [Microchip MCP4725 data sheet](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP4725-Data-Sheet-20002039E.pdf)
- [Analog Devices AD5241/AD5242 data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ad5241_ad5242.pdf)

## States

| State | Meaning |
|---|---|
| `startup_safe` | Booted with fallback values; no valid command received |
| `active` | A valid, non-expired command is active |
| `manual_safe` | Fallback was explicitly requested |
| `stale_safe` | The command timed out |
| `invalid_safe` | An invalid command was rejected |
| `output_fault_safe` | The output driver reported a fault |

# Calibration Storage

The ESPHome analog bridge stores its active transfer-function calibration in
the platform preference backend (ESP32 NVS). Configuration values remain the
factory defaults and are never overwritten.

## Stored values

The current version-2 record contains:

| Field | Accepted range |
|---|---:|
| Humidity DAC minimum code | 0…4094 |
| Humidity DAC maximum code | 1…4095 and greater than the minimum |
| Temperature-network minimum resistance | 100…50,000 Ω |
| Temperature-network maximum resistance | 100…50,000 Ω and greater than the minimum |
| Temperature code inversion | true/false |

Each record also carries a fixed magic value, format version, record size,
flags and an IEEE CRC-32 over every preceding byte. The ESPHome preference ID
is configured separately from the record version so the component can read a
legacy record and write the migrated current record without ambiguity.

## Boot and failure behaviour

1. Load and validate a version-2 record.
2. If no version-2 record exists, attempt the documented version-1 migration.
3. Reject unknown versions, sizes, flags, non-finite values, invalid ranges or
   CRC mismatches.
4. On rejection, use the compiled factory values and publish
   `invalid_using_factory`.
5. Never fall back from a present but invalid current record to an older
   record, because that could silently resurrect stale calibration.

The first boot without stored data uses factory values and publishes
`factory_defaults`. A successful migration is immediately rewritten as a
version-2 record. A failed migration write remains visible as
`migration_save_failed`.

## Updates and reset

`idm_bridge.set_calibration` validates the complete calibration atomically,
writes it to NVS, reads it back and only then activates it. Invalid or
unconfirmed writes therefore cannot partially change the active transfer
function.

`idm_bridge.reset_calibration` persists the values from the YAML configuration
and activates them. It does not erase unrelated ESPHome preferences.

The following diagnostics are available under `analog_output`:

- `calibration_status`
- `calibration_version`
- `calibration_using_factory`

Keep `calibration_preference_id` stable when renaming the component ID. A new
preference ID intentionally starts with factory defaults.

## Migration contract

Version 1 stored humidity as `DAC code = offset + gain × RH[%]`, plus the two
temperature-resistance endpoints and the inversion flag. Migration converts
that representation to the version-2 minimum and maximum DAC codes, validates
the result, then writes a new version-2 record.

## Verification status

- [x] Format, CRC, range and migration unit tests
- [x] Compiled against the pinned ESPHome/ESP32 target
- [ ] Power-interruption bench test
- [ ] Real-device calibration test

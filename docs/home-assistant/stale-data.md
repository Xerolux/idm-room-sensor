# Stale Data

The example package checks the temperature and humidity state objects via
Home Assistant's `last_reported` timestamp. A room is considered stale when
either source exceeds `input_number.idm_sensor_stale_after_minutes`.

Stale or unavailable pairs use the configurable conservative fallback
temperature and humidity. The package exposes
`binary_sensor.idm_climate_input_stale`; do not silently suppress that warning.

For installations that need the same guard independently of the complete
climate-engine package, use
`homeassistant/blueprints/automation/stale_guard.yaml`. It runs the configured
safe-state actions once when the input becomes stale or invalid, and the
recovery actions once after fresh data returns.

## Status

- [x] Implemented in the example package
- [x] Standalone transition-based blueprint
- [x] Covered by repository contract checks
- [ ] Bench tested
- [ ] Real-device tested

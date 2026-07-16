# Home Assistant

The climate-engine package calculates every room's dew point and selects the
highest value, because humidity alone is not enough to identify the critical
room. Inputs older than the configured `last_reported` limit are replaced with
conservative fallback values.

The package exposes diagnostic entities for the selection reason, selected
source age, fallback use, last successful publish time and incomplete/failed
publish attempts. `homeassistant/dashboard-example.yaml` groups these signals
so the active value path can be understood without searching logs.

Replace all example entity IDs before use. The included automation uses
`idm_heatpump.write_register` so it works with released
`idm-heatpump-hass` v0.8.2. Current upstream main also provides the safer
`idm_heatpump.set_external_climate` action; switch to it after a tagged release
contains the service.

The package still requires bench and real-device validation. Keep an
independent condensation guard active during cooling tests.

# Proposed upstream changes

The current `idm-heatpump-hass` main branch contains
`idm_heatpump.set_external_climate` (commit `bd41999`, 2026-07-16). This
directory now preserves a dependency-light contract reference and standalone
tests for review from the climate-platform side.

The service has not yet shipped in a tagged release and has not been validated
against a real heat pump. The Home Assistant package therefore uses
`write_register` for v0.8.2 compatibility.

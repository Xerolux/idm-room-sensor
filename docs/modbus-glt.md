# GLT / Modbus path

Preferred register targets from the current register catalog:

- Heating circuits A–G external room temperatures: 1650, 1652, 1654, 1656, 1658, 1660, 1662 (`FLOAT`).
- External humidity: 1692 (`FLOAT`).

Only one writer should own a register. The service proposal validates ranges and resolves known register definitions rather than encouraging raw writes. Actual use by the Navigator control strategy remains installation- and configuration-dependent and must be tested.

As of 2026-07-16, `idm-heatpump-hass` main contains the native
`set_external_climate` service, while the latest tagged v0.8.2 release does not.
The example package therefore retains a raw-write compatibility path until the
native service ships in a tagged release.

# GLT / Modbus path

Preferred register targets from the current register catalog:

- Heating circuits A–G external room temperatures: 1650, 1652, 1654, 1656, 1658, 1660, 1662 (`FLOAT`).
- External humidity: 1692 (`FLOAT`).

Only one writer should own a register. The service proposal validates ranges and resolves known register definitions rather than encouraging raw writes. Actual use by the Navigator control strategy remains installation- and configuration-dependent and must be tested.

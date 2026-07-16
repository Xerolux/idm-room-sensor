"""Reviewable reference for the upstream ``set_external_climate`` handler.

The service is implemented on the current ``idm-heatpump-hass`` main branch.
This dependency-light copy keeps the validation contract executable in this
repository without constructing ad-hoc register definitions.
"""

from __future__ import annotations

import math
from collections.abc import Mapping
from typing import Any

try:
    from homeassistant.exceptions import ServiceValidationError
except ModuleNotFoundError:
    # Home Assistant is intentionally not a development dependency of this
    # hardware repository. The fallback preserves the validation semantics for
    # the standalone contract tests below.
    class ServiceValidationError(ValueError):
        """Standalone substitute used outside Home Assistant."""


EXT_ROOM_TEMP = {
    "A": 1650,
    "B": 1652,
    "C": 1654,
    "D": 1656,
    "E": 1658,
    "F": 1660,
    "G": 1662,
}
EXT_HUMIDITY = 1692
ROOM_TEMPERATURE_RANGE = (15.0, 30.0)
HUMIDITY_RANGE = (0.0, 100.0)


def _coerce_finite_float(data: Mapping[str, Any], field: str) -> float:
    raw_value = data.get(field)
    try:
        value = float(raw_value)
    except (TypeError, ValueError, OverflowError) as err:
        raise ServiceValidationError(f"{field} must be numeric") from err
    if not math.isfinite(value):
        raise ServiceValidationError(f"{field} must be finite")
    return value


def _known_writable_register(coordinator: Any, register_name: str) -> Any:
    get_register = getattr(coordinator, "get_register", None)
    register = get_register(register_name) if callable(get_register) else None
    if register is None or not getattr(register, "writable", False):
        raise ServiceValidationError(f"register is not writable: {register_name}")
    return register


def _validate_range(value: float, field: str, register: Any, fallback: tuple[float, float]) -> None:
    minimum = getattr(register, "min_val", None)
    maximum = getattr(register, "max_val", None)
    minimum = fallback[0] if minimum is None else float(minimum)
    maximum = fallback[1] if maximum is None else float(maximum)
    if not minimum <= value <= maximum:
        raise ServiceValidationError(f"{field} must be between {minimum:g} and {maximum:g}")


async def handle_set_external_climate(coordinator: Any, call: Any) -> None:
    """Validate and write one circuit temperature plus optional humidity."""
    data = call.data if isinstance(call.data, Mapping) else {}
    circuit = str(data.get("heating_circuit", "")).strip().upper()
    if circuit not in EXT_ROOM_TEMP:
        raise ServiceValidationError("invalid heating circuit")

    room_temperature = _coerce_finite_float(data, "room_temperature")
    temperature_register = _known_writable_register(
        coordinator,
        f"hc_{circuit.lower()}_ext_room_temp",
    )
    _validate_range(
        room_temperature,
        "room_temperature",
        temperature_register,
        ROOM_TEMPERATURE_RANGE,
    )

    writes = [(temperature_register, room_temperature)]
    if "humidity" in data and data.get("humidity") is not None:
        humidity = _coerce_finite_float(data, "humidity")
        humidity_register = _known_writable_register(coordinator, "ext_humidity")
        _validate_range(humidity, "humidity", humidity_register, HUMIDITY_RANGE)
        writes.append((humidity_register, humidity))

    # Resolve and validate every requested register before the first write so
    # unsupported humidity cannot leave a partial update behind.
    for register, value in writes:
        await coordinator.async_write_register(register, value)

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from services_implementation import (
    EXT_HUMIDITY,
    EXT_ROOM_TEMP,
    ServiceValidationError,
    handle_set_external_climate,
)


@dataclass
class Register:
    name: str
    address: int
    writable: bool = True
    min_val: float | None = None
    max_val: float | None = None


def make_coordinator(*, missing: set[str] | None = None, read_only: set[str] | None = None):
    missing = missing or set()
    read_only = read_only or set()
    registers = {
        f"hc_{circuit.lower()}_ext_room_temp": Register(
            name=f"hc_{circuit.lower()}_ext_room_temp",
            address=address,
            writable=f"hc_{circuit.lower()}_ext_room_temp" not in read_only,
            min_val=15,
            max_val=30,
        )
        for circuit, address in EXT_ROOM_TEMP.items()
    }
    registers["ext_humidity"] = Register(
        name="ext_humidity",
        address=EXT_HUMIDITY,
        writable="ext_humidity" not in read_only,
        min_val=0,
        max_val=100,
    )
    for name in missing:
        registers.pop(name, None)

    coordinator = SimpleNamespace()
    coordinator.get_register = registers.get
    coordinator.async_write_register = AsyncMock()
    return coordinator


def run_handler(coordinator, data):
    asyncio.run(
        handle_set_external_climate(
            coordinator,
            SimpleNamespace(data=data),
        )
    )


@pytest.mark.parametrize(
    ("circuit", "address"),
    [
        ("A", 1650),
        ("B", 1652),
        ("C", 1654),
        ("D", 1656),
        ("E", 1658),
        ("F", 1660),
        ("G", 1662),
    ],
)
def test_external_temperature_address_map(circuit, address):
    assert EXT_ROOM_TEMP[circuit] == address


def test_external_humidity_address():
    assert EXT_HUMIDITY == 1692


def test_writes_temperature_and_humidity_from_known_registers():
    coordinator = make_coordinator()

    run_handler(
        coordinator,
        {"heating_circuit": "A", "room_temperature": 23.1, "humidity": 58.4},
    )

    assert coordinator.async_write_register.await_count == 2
    temperature_call, humidity_call = coordinator.async_write_register.await_args_list
    assert temperature_call.args[0].name == "hc_a_ext_room_temp"
    assert temperature_call.args[1] == 23.1
    assert humidity_call.args[0].name == "ext_humidity"
    assert humidity_call.args[1] == 58.4


def test_humidity_is_optional_and_circuit_is_case_insensitive():
    coordinator = make_coordinator()

    run_handler(
        coordinator,
        {"heating_circuit": " b ", "room_temperature": "21.5"},
    )

    coordinator.async_write_register.assert_awaited_once()
    register, value = coordinator.async_write_register.await_args.args
    assert register.name == "hc_b_ext_room_temp"
    assert value == 21.5


@pytest.mark.parametrize(
    "data",
    [
        {},
        {"heating_circuit": "Z", "room_temperature": 20},
        {"heating_circuit": "A"},
        {"heating_circuit": "A", "room_temperature": 14.9},
        {"heating_circuit": "A", "room_temperature": 30.1},
        {"heating_circuit": "A", "room_temperature": "nan"},
        {"heating_circuit": "A", "room_temperature": "inf"},
        {"heating_circuit": "A", "room_temperature": 20, "humidity": -0.1},
        {"heating_circuit": "A", "room_temperature": 20, "humidity": 100.1},
    ],
)
def test_invalid_inputs_do_not_write(data):
    coordinator = make_coordinator()

    with pytest.raises(ServiceValidationError):
        run_handler(coordinator, data)

    coordinator.async_write_register.assert_not_awaited()


@pytest.mark.parametrize(
    ("missing", "read_only"),
    [
        ({"hc_a_ext_room_temp"}, set()),
        ({"ext_humidity"}, set()),
        (set(), {"hc_a_ext_room_temp"}),
        (set(), {"ext_humidity"}),
    ],
)
def test_missing_or_read_only_registers_do_not_partially_write(missing, read_only):
    coordinator = make_coordinator(missing=missing, read_only=read_only)

    with pytest.raises(ServiceValidationError):
        run_handler(
            coordinator,
            {"heating_circuit": "A", "room_temperature": 23, "humidity": 50},
        )

    coordinator.async_write_register.assert_not_awaited()

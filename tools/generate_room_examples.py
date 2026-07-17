#!/usr/bin/env python3
"""Generate the per-room dew-point template sensors.

The 20 files under homeassistant/examples/rooms/room_01..20.yaml are
near-identical. Generating them from a single template keeps them in sync and
ensures every room carries device_class/state_class metadata and an
availability template that rejects non-numeric or missing source sensors.

Run manually after adding or renaming rooms:

    python3 tools/generate_room_examples.py

The generated files are committed; this script is the source of truth.
"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROOM_COUNT = 20
ROOM_DIRECTORY = ROOT / "homeassistant/examples/rooms"

TEMPLATE = """\
template:
  - sensor:
      - name: "Room {index:02d} Dew Point"
        unique_id: idm_room_{index:02d}_dew_point
        unit_of_measurement: "°C"
        device_class: temperature
        state_class: measurement
        icon: mdi:water-thermometer
        availability: >
          {{% set t = states('sensor.room_{index:02d}_temperature') | float(none) %}}
          {{% set rh = states('sensor.room_{index:02d}_humidity') | float(none) %}}
          {{{{ t is not none and rh is not none and rh > 0 and rh <= 100 }}}}
        state: >
          {{% set t = states('sensor.room_{index:02d}_temperature') | float(none) %}}
          {{% set rh = states('sensor.room_{index:02d}_humidity') | float(none) %}}
          {{% if t is not none and rh is not none and rh > 0 %}}
            {{% set a = 17.62 %}}{{% set b = 243.12 %}}
            {{% set g = log(rh / 100) + (a * t) / (b + t) %}}
            {{{{ (b * g / (a - g)) | round(2) }}}}
          {{% else %}} unavailable {{% endif %}}
"""


def generate() -> None:
    ROOM_DIRECTORY.mkdir(parents=True, exist_ok=True)
    for index in range(1, ROOM_COUNT + 1):
        path = ROOM_DIRECTORY / f"room_{index:02d}.yaml"
        path.write_text(TEMPLATE.format(index=index), encoding="utf-8")
    print(f"Generated {ROOM_COUNT} room templates in {ROOM_DIRECTORY}")


if __name__ == "__main__":
    generate()

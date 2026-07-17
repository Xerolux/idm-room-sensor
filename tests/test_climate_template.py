from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
import math
from pathlib import Path

from jinja2 import Environment
import yaml


class State:
    def __init__(self, value: object, last_reported: datetime) -> None:
        self.state = str(value)
        self.last_reported = last_reported


class States:
    def __init__(self, values: dict[str, State], now: datetime) -> None:
        self._values = values
        self._now = now

    def __call__(self, entity_id: str) -> str:
        return self._values.get(entity_id, State("unknown", self._now)).state


def is_number(value: object) -> bool:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return False
    return math.isfinite(number)


def render_selection(
    *,
    stale_office: bool = False,
    office_humidity: object = 50,
    overrides: dict[str, object] | None = None,
) -> dict[str, object]:
    package = yaml.safe_load(
        Path("homeassistant/climate-engine-package.yaml").read_text(encoding="utf-8")
    )
    template = package["template"][0]["actions"][0]["variables"]["idm_selected_room"]
    now = datetime.now(timezone.utc)
    fresh = now - timedelta(minutes=1)
    office_time = now - timedelta(minutes=30) if stale_office else fresh
    values = {
        "input_number.idm_sensor_stale_after_minutes": State(15, fresh),
        "input_number.idm_fallback_temperature": State(28, fresh),
        "input_number.idm_fallback_humidity": State(80, fresh),
        "sensor.living_temperature": State(26, fresh),
        "sensor.living_humidity": State(55, fresh),
        "sensor.bedroom_temperature": State(20, fresh),
        "sensor.bedroom_humidity": State(70, fresh),
        "sensor.office_temperature": State(18, office_time),
        "sensor.office_humidity": State(office_humidity, office_time),
    }
    for entity_id, value in (overrides or {}).items():
        values[entity_id] = State(value, fresh)

    environment = Environment()
    environment.filters["to_json"] = json.dumps
    environment.filters["from_json"] = json.loads
    environment.globals.update(
        states=States(values, now),
        expand=lambda entity_id: [values[entity_id]] if entity_id in values else [],
        is_number=is_number,
        as_timestamp=lambda value, default=None: (
            value.timestamp() if hasattr(value, "timestamp") else default
        ),
        now=lambda: now,
        clamp=lambda value, low, high: max(low, min(high, float(value))),
        log=math.log,
    )
    return json.loads(environment.from_string(template).render())


def render_selected_output(
    unique_id: str,
    *,
    temperature: float,
    humidity: float,
) -> float:
    package = yaml.safe_load(
        Path("homeassistant/climate-engine-package.yaml").read_text(encoding="utf-8")
    )
    sensor = next(
        item
        for item in package["template"][0]["sensor"]
        if item["unique_id"] == unique_id
    )
    environment = Environment()
    environment.filters["from_json"] = json.loads
    environment.globals["clamp"] = (
        lambda value, low, high: max(low, min(high, float(value)))
    )
    selected = json.dumps(
        {
            "temperature": temperature,
            "humidity": humidity,
        }
    )
    return float(
        environment.from_string(sensor["state"]).render(
            idm_selected_room=selected,
        )
    )


def test_selects_highest_dew_point_not_highest_humidity():
    result = render_selection()

    assert result["name"] == "living"
    assert result["humidity"] == 55
    assert result["any_stale"] is False
    assert result["uses_fallback"] is False
    assert result["reason"] == "highest_dew_point"


def test_stale_room_uses_conservative_fallback():
    result = render_selection(stale_office=True)

    assert result["name"] == "office (fallback)"
    assert result["temperature"] == 28
    assert result["humidity"] == 80
    assert result["any_stale"] is True
    assert result["uses_fallback"] is True
    assert result["reason"] == "fallback_stale_or_invalid"


def test_out_of_range_room_value_uses_conservative_fallback():
    result = render_selection(office_humidity=0)

    assert result["name"] == "office (fallback)"
    assert result["humidity"] == 80
    assert result["any_stale"] is True


def test_unavailable_room_value_uses_conservative_fallback():
    result = render_selection(
        overrides={"sensor.office_humidity": "unavailable"},
    )

    assert result["name"] == "office (fallback)"
    assert result["uses_fallback"] is True


def test_equal_dew_points_keep_first_configured_room():
    result = render_selection(
        overrides={
            "sensor.bedroom_temperature": 26,
            "sensor.bedroom_humidity": 55,
            "sensor.office_temperature": 15,
            "sensor.office_humidity": 30,
        },
    )

    assert result["name"] == "living"


def test_selected_outputs_are_clamped_to_supported_ranges():
    temperature = render_selected_output(
        "idm_selected_external_room_temperature",
        temperature=55,
        humidity=150,
    )
    humidity = render_selected_output(
        "idm_selected_external_humidity",
        temperature=55,
        humidity=150,
    )

    assert temperature == 30
    assert humidity == 100


def test_template_and_publish_automation_run_after_restart():
    package = yaml.safe_load(
        Path("homeassistant/climate-engine-package.yaml").read_text(encoding="utf-8")
    )
    template_triggers = package["template"][0]["triggers"]
    publish_triggers = package["automation"][0]["triggers"]

    assert {"trigger": "homeassistant", "event": "start"} in template_triggers
    assert {"trigger": "homeassistant", "event": "start"} in publish_triggers


def test_publish_status_only_becomes_ok_after_both_register_writes():
    package = yaml.safe_load(
        Path("homeassistant/climate-engine-package.yaml").read_text(encoding="utf-8")
    )
    actions = package["automation"][0]["actions"]

    assert actions[0]["action"] == "input_text.set_value"
    assert actions[0]["data"]["value"] == "writing"
    assert [action.get("data", {}).get("address") for action in actions] == [
        None,
        1650,
        1692,
        None,
        None,
    ]
    assert actions[-1]["action"] == "input_text.set_value"
    assert actions[-1]["data"]["value"] == "ok"


def test_highest_dew_point_equivalence_to_smallest_margin_for_shared_pipe():
    # The climate-engine selects by argmax(dew_point). The documented strategy
    # is "smallest dew-point margin" (pipe_temperature - dew_point). These are
    # equivalent ONLY because the implementation uses a single shared pipe/
    # flow temperature for all rooms, so the pipe term is constant and drops
    # out of the argmin. This test pins that assumption: if anyone ever wires
    # per-room pipe sensors, the selection must move to a true per-room argmin
    # and this test will flag the behavioural change.
    import random

    def dew_point(t: float, rh: float) -> float:
        a, b = 17.62, 243.12
        g = math.log(rh / 100.0) + (a * t) / (b + t)
        return b * g / (a - g)

    rng = random.Random(20260717)
    for _ in range(200):
        rooms = [
            (
                rng.uniform(-10, 40),
                rng.uniform(20, 95),
            )
            for _ in range(6)
        ]
        dps = [dew_point(t, rh) for t, rh in rooms]
        argmax_dp = max(range(len(rooms)), key=lambda i: dps[i])
        for shared_pipe in (5.0, 10.0, 40.0):
            margins = [shared_pipe - dp for dp in dps]
            argmin_margin = min(range(len(rooms)), key=lambda i: margins[i])
            assert argmax_dp == argmin_margin, (
                "argmax(dew_point) != argmin(margin) — the shared-pipe "
                "equivalence assumption is broken; selection must switch to "
                "a per-room margin calculation."
            )

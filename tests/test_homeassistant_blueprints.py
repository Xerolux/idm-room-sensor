from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
import math
from pathlib import Path

from jinja2 import Environment
import yaml


class HomeAssistantLoader(yaml.SafeLoader):
    pass


HomeAssistantLoader.add_constructor(
    "!input",
    lambda loader, node: {"!input": loader.construct_scalar(node)},
)


class State:
    def __init__(self, value: object, last_reported: datetime) -> None:
        self.state = str(value)
        self.last_reported = last_reported


def is_number(value: object) -> bool:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return False
    return math.isfinite(number)


def load_blueprint(name: str) -> dict[str, object]:
    path = Path("homeassistant/blueprints/automation") / name
    return yaml.load(path.read_text(encoding="utf-8"), Loader=HomeAssistantLoader)


def template_environment(
    values: dict[str, State],
    now: datetime,
) -> Environment:
    environment = Environment()
    environment.filters["to_json"] = json.dumps
    environment.globals.update(
        expand=lambda entity_id: (
            [values[entity_id]] if entity_id in values else []
        ),
        is_number=is_number,
        as_timestamp=lambda value, default=None: (
            value.timestamp() if hasattr(value, "timestamp") else default
        ),
        now=lambda: now,
        clamp=lambda value, low, high: max(low, min(high, float(value))),
        log=math.log,
    )
    return environment


def render_critical_room(
    *,
    stale_office: bool = False,
) -> dict[str, object]:
    blueprint = load_blueprint("critical_room.yaml")
    template = blueprint["actions"][0]["variables"]["selected_result"]
    now = datetime.now(timezone.utc)
    fresh = now - timedelta(minutes=1)
    office_time = now - timedelta(minutes=30) if stale_office else fresh
    values = {
        "sensor.living_temperature": State(26, fresh),
        "sensor.living_humidity": State(55, fresh),
        "sensor.bedroom_temperature": State(20, fresh),
        "sensor.bedroom_humidity": State(70, fresh),
        "sensor.office_temperature": State(18, office_time),
        "sensor.office_humidity": State(50, office_time),
    }
    rooms = [
        {
            "name": "living",
            "temperature": "sensor.living_temperature",
            "humidity": "sensor.living_humidity",
        },
        {
            "name": "bedroom",
            "temperature": "sensor.bedroom_temperature",
            "humidity": "sensor.bedroom_humidity",
        },
        {
            "name": "office",
            "temperature": "sensor.office_temperature",
            "humidity": "sensor.office_humidity",
        },
    ]
    rendered = template_environment(values, now).from_string(template).render(
        configured_rooms=rooms,
        stale_after_minutes=15,
        fallback_temperature=28,
        fallback_humidity=80,
    )
    return json.loads(rendered)


def render_stale_trigger(value: object, age_minutes: int) -> bool:
    blueprint = load_blueprint("stale_guard.yaml")
    template = blueprint["triggers"][0]["value_template"]
    now = datetime.now(timezone.utc)
    values = {
        "sensor.source": State(value, now - timedelta(minutes=age_minutes)),
    }
    rendered = template_environment(values, now).from_string(template).render(
        source_sensor="sensor.source",
        stale_after_minutes=15,
        require_numeric=True,
    )
    return rendered.strip().lower() == "true"


def test_all_blueprints_have_executable_actions():
    for name in ("cooling_inhibit.yaml", "critical_room.yaml", "stale_guard.yaml"):
        blueprint = load_blueprint(name)

        assert blueprint["actions"]
        assert "placeholder" not in blueprint["blueprint"]["description"].lower()


def test_cooling_inhibit_has_stale_interlock_and_hysteresis_guard():
    blueprint = load_blueprint("cooling_inhibit.yaml")
    trigger_ids = [trigger["id"] for trigger in blueprint["triggers"]]

    # The margin sensor must be monitored for unavailable/unknown transitions
    # so a stuck sensor cannot leave the inhibit/clear state frozen. Both
    # unavailable transitions share the id "unavailable" so they route to the
    # inhibit (safe) branch.
    assert "inhibit" in trigger_ids
    assert "clear" in trigger_ids
    assert trigger_ids.count("unavailable") == 2
    unavailable_targets = {
        trigger["to"] for trigger in blueprint["triggers"]
        if trigger["id"] == "unavailable"
    }
    assert unavailable_targets == {"unavailable", "unknown"}

    # The hysteresis guard must refuse a configuration where the recovery
    # threshold is not above the inhibit threshold, rather than letting both
    # numeric_state triggers flap.
    actions_yaml = json.dumps(blueprint["actions"])
    assert "z_clear_above <= z_inhibit_below" in actions_yaml
    assert "Recovery threshold must be above the inhibit threshold" in actions_yaml


def test_cooling_inhibit_default_thresholds_are_safe_and_ordered():
    blueprint = load_blueprint("cooling_inhibit.yaml")
    inputs = blueprint["blueprint"]["input"]

    inhibit_default = inputs["inhibit_below"]["default"]
    clear_default = inputs["clear_above"]["default"]
    # Defaults must provide hysteresis: clear strictly above inhibit.
    assert clear_default > inhibit_default
    # The inhibit default must be a small positive margin (cooling unsafe when
    # the dew-point margin is only a couple of kelvin).
    assert 0 < inhibit_default <= 5


def test_fake_sensor_automation_selects_by_dew_point_and_fails_closed():
    import yaml

    source = Path(
        "homeassistant/fake-sensor-automation.yaml"
    ).read_text(encoding="utf-8")
    automation = yaml.safe_load(source)

    # Modern triggers/actions syntax (not the deprecated trigger:/service:).
    assert "triggers" in automation
    assert "actions" in automation or "action" in automation

    # Selection must compute a dew point (Magnus constants), not sort by raw
    # relative humidity. The previous implementation sorted by attribute 0
    # (humidity), which contradicts the documented critical-margin strategy.
    assert "sort(attribute='dp'" in source or 'sort(attribute="dp"' in source
    assert "17.62" in source and "243.12" in source
    # Fail-closed: when no room has usable numeric data, the automation must
    # NOT publish synthetic fallback values to the bridge.
    assert "worst is not none" in source


def test_critical_room_selects_highest_dew_point():
    result = render_critical_room()

    assert result["name"] == "living"
    assert result["humidity"] == 55
    assert result["uses_fallback"] is False
    assert result["any_stale"] is False


def test_critical_room_uses_fallback_for_stale_pair():
    result = render_critical_room(stale_office=True)

    assert result["name"] == "office (fallback)"
    assert result["temperature"] == 28
    assert result["humidity"] == 80
    assert result["uses_fallback"] is True
    assert result["any_stale"] is True


def test_critical_room_exposes_output_actions_and_variables():
    blueprint = load_blueprint("critical_room.yaml")
    output_inputs = blueprint["blueprint"]["input"]["output_actions"]["input"]
    variable_names = set(blueprint["actions"][1]["variables"])

    assert {"selected_actions", "stale_actions"} <= set(output_inputs)
    assert {
        "selected_room_name",
        "selected_temperature",
        "selected_humidity",
        "selected_dew_point",
        "selected_uses_fallback",
        "any_room_stale",
        "configured_room_count",
    } <= variable_names


def test_stale_guard_detects_age_and_invalid_value():
    assert render_stale_trigger(22.5, age_minutes=1) is False
    assert render_stale_trigger(22.5, age_minutes=30) is True
    assert render_stale_trigger("unavailable", age_minutes=1) is True


def test_stale_guard_has_transition_triggers_and_recovery_actions():
    blueprint = load_blueprint("stale_guard.yaml")
    trigger_ids = {trigger["id"] for trigger in blueprint["triggers"]}
    output_inputs = blueprint["blueprint"]["input"]["output_actions"]["input"]

    assert trigger_ids == {"stale", "recovered", "startup"}
    assert {"stale_actions", "recovery_actions"} <= set(output_inputs)

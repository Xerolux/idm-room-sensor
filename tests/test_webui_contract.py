"""Static contract tests for the local ESPHome web UI."""

from __future__ import annotations

import json
from pathlib import Path
import re

from jsonschema import Draft202012Validator
import yaml


ROOT = Path(__file__).resolve().parents[1]
WEBUI_DIRECTORY = ROOT / "firmware/webui"
WEBUI_PACKAGE = (
    ROOT / "firmware/esphome/packages/fake-sensor-webui.yaml"
)


class EsphomeLoader(yaml.SafeLoader):
    """YAML loader that preserves ESPHome tagged scalar values."""


for yaml_tag in ("!include", "!lambda", "!secret"):
    EsphomeLoader.add_constructor(
        yaml_tag,
        lambda loader, node, tag=yaml_tag: {
            tag: loader.construct_scalar(node),
        },
    )


def load_json(filename: str) -> dict:
    return json.loads(
        (WEBUI_DIRECTORY / filename).read_text(encoding="utf-8")
    )


def load_package() -> dict:
    return yaml.load(
        WEBUI_PACKAGE.read_text(encoding="utf-8"),
        Loader=EsphomeLoader,
    )


def by_id(items: list[dict]) -> dict[str, dict]:
    return {item["id"]: item for item in items}


def test_webui_manifest_validates_against_its_schema() -> None:
    schema = load_json("schema.json")
    manifest = load_json("manifest.json")
    Draft202012Validator.check_schema(schema)
    Draft202012Validator(schema).validate(manifest)


def test_web_server_runtime_matches_manifest() -> None:
    manifest = load_json("manifest.json")
    package = load_package()
    web_server = package["web_server"]

    assert manifest["implementation"] == "esphome_web_server_v3"
    assert web_server["version"] == 3
    assert web_server["local"] is manifest["local_assets"]
    assert ("auth" in web_server) is manifest["authentication_required"]
    assert web_server["ota"] is manifest["web_ota_enabled"]

    group_ids = [group["id"] for group in web_server["sorting_groups"]]
    assert group_ids == [
        "idm_web_status_group",
        "idm_web_commands_group",
        "idm_web_calibration_group",
        "idm_web_safety_group",
        "idm_web_diagnostics_group",
    ]
    assert manifest["sections"] == [
        "status",
        "commands",
        "calibration",
        "safety",
        "diagnostics",
    ]


def test_calibration_editor_bounds_match_manifest_and_core() -> None:
    manifest = load_json("manifest.json")
    numbers = by_id(load_package()["number"])
    calibration = manifest["calibration"]

    mappings = {
        "humidity_code_min": "idm_web_humidity_code_min",
        "humidity_code_max": "idm_web_humidity_code_max",
        "temperature_resistance_min": (
            "idm_web_temperature_resistance_min"
        ),
        "temperature_resistance_max": (
            "idm_web_temperature_resistance_max"
        ),
    }
    for contract_name, entity_id in mappings.items():
        contract = calibration[contract_name]
        entity = numbers[entity_id]
        assert entity["min_value"] == contract["minimum"]
        assert entity["max_value"] == contract["maximum"]
        assert entity["step"] == contract["step"]
        assert entity["optimistic"] is True
        assert entity["restore_value"] is False

    core = (
        ROOT
        / "firmware/components/idm_bridge/idm_calibration_storage_core.h"
    ).read_text(encoding="utf-8")
    for fragment in (
        "value.humidity_code_min < value.humidity_code_max",
        "value.temperature_resistance_min <",
        "value.temperature_resistance_max",
        "CALIBRATION_HUMIDITY_CODE_MAX = 4095",
        "CALIBRATION_RESISTANCE_MIN = 100.0f",
        "CALIBRATION_RESISTANCE_MAX = 50000.0f",
    ):
        assert fragment in core


def test_dangerous_actions_require_expiring_confirmation() -> None:
    manifest = load_json("manifest.json")
    package = load_package()
    scripts = by_id(package["script"])
    switches = by_id(package["switch"])
    buttons = by_id(package["button"])
    package_text = WEBUI_PACKAGE.read_text(encoding="utf-8")

    confirmation = switches["idm_web_dangerous_action_confirmation"]
    assert confirmation["optimistic"] is False
    assert confirmation["restore_mode"] == "ALWAYS_OFF"
    assert scripts["idm_web_expire_confirmation"]["then"][0]["delay"] == (
        f"{manifest['confirmation_timeout_s']}s"
    )

    action_ids = {
        "apply_calibration": "idm_web_apply_calibration",
        "factory_reset_calibration": (
            "idm_web_factory_reset_calibration"
        ),
    }
    for action_name in manifest["dangerous_actions"]:
        assert action_ids[action_name] in buttons

    assert package_text.count(
        "switch.is_on: idm_web_dangerous_action_confirmation"
    ) == len(manifest["dangerous_actions"])
    assert package_text.count(
        "switch.turn_off: idm_web_dangerous_action_confirmation"
    ) == len(manifest["dangerous_actions"])
    assert 'state: "confirmation_required"' in package_text


def test_diagnostic_export_fields_match_schema_and_endpoint() -> None:
    manifest = load_json("manifest.json")
    schema = load_json("diagnostic-export.schema.json")
    package = load_package()
    export = by_id(package["text_sensor"])[
        "idm_webui_diagnostic_export"
    ]
    package_text = WEBUI_PACKAGE.read_text(encoding="utf-8")

    payload_fields = set(re.findall(r'root\["([^"]+)"\]', package_text))
    assert payload_fields == set(schema["required"])
    assert set(schema["required"]) == set(schema["properties"])
    assert schema["additionalProperties"] is False
    assert export["update_interval"] == (
        f"{manifest['diagnostic_export']['refresh_interval_s']}s"
    )

    object_id = re.sub(r"[^a-z0-9]+", "_", export["name"].lower()).strip(
        "_"
    )
    assert manifest["diagnostic_export"]["path"] == (
        f"/text_sensor/{object_id}"
    )


def test_supported_fake_sensor_entry_points_enable_the_webui_contract() -> None:
    readme = (WEBUI_DIRECTORY / "README.md").read_text(encoding="utf-8")
    for entry_point in (
        ROOT / "firmware/fake-sensor-bridge.yaml",
        ROOT / "firmware/fake-sensor-esphome.yaml",
    ):
        config = entry_point.read_text(encoding="utf-8")
        assert "local_web_ui:" in config
        assert "fake-sensor-webui.yaml" in config
        assert "web_username:" in config
        assert "web_password: CHANGE-ME-BEFORE-INSTALLATION" in config
        assert "idm_web_status_group" in config
        assert "idm_web_commands_group" in config

    assert "trusted commissioning network" in readme
    assert "CHANGE-ME-BEFORE-INSTALLATION" in readme

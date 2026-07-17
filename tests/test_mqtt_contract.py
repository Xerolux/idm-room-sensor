"""Static contract tests for MQTT discovery, safety and diagnostics."""

from __future__ import annotations

import json
from pathlib import Path
import re

import pytest
import yaml
from jsonschema import Draft202012Validator


ROOT = Path(__file__).resolve().parents[1]
MQTT_PACKAGE = ROOT / "firmware/esphome/packages/mqtt.yaml"
BRIDGE_PACKAGE = ROOT / "firmware/esphome/packages/fake-sensor-mqtt.yaml"
ROOM_PACKAGE = ROOT / "firmware/esphome/packages/room-sensor-mqtt.yaml"
SCHEMA_DIRECTORY = ROOT / "firmware/mqtt"


def load_schema(filename: str) -> dict:
    schema = json.loads(
        (SCHEMA_DIRECTORY / filename).read_text(encoding="utf-8")
    )
    # Every shipped MQTT schema must itself be a valid JSON Schema document;
    # a malformed schema file used to pass silently because the tests only
    # inspected keys by hand.
    Draft202012Validator.check_schema(schema)
    return schema


def payload_fields(package: Path) -> set[str]:
    text = package.read_text(encoding="utf-8")
    return set(re.findall(r'root\["([^"]+)"\]', text))


def test_common_mqtt_package_has_replay_and_availability_contract() -> None:
    config = yaml.safe_load(MQTT_PACKAGE.read_text(encoding="utf-8"))["mqtt"]

    assert config["enable_on_boot"] == "${mqtt_enabled}"
    assert config["clean_session"] is True
    assert config["discovery"] is True
    assert config["discovery_retain"] is True
    assert config["discovery_unique_id_generator"] == "mac"
    assert config["discovery_object_id_generator"] == "device_name"
    assert config["reboot_timeout"] == "0s"
    assert config["log_topic"] is None

    for message_name in (
        "birth_message",
        "will_message",
        "shutdown_message",
    ):
        message = config[message_name]
        assert message["topic"] == "${mqtt_topic_prefix}/availability"
        assert message["qos"] == 1
        assert message["retain"] is True

    assert config["birth_message"]["payload"] == "online"
    assert config["will_message"]["payload"] == "offline"
    assert config["shutdown_message"]["payload"] == "offline"


def test_diagnostic_payloads_match_their_json_schemas() -> None:
    contracts = (
        (BRIDGE_PACKAGE, "bridge-diagnostics.schema.json"),
        (ROOM_PACKAGE, "room-diagnostics.schema.json"),
    )
    for package, schema_name in contracts:
        schema = load_schema(schema_name)
        assert schema["type"] == "object"
        assert schema["additionalProperties"] is False
        assert set(schema["required"]) == set(schema["properties"])
        assert payload_fields(package) == set(schema["required"])


def test_atomic_command_schema_and_firmware_ranges_match() -> None:
    schema = load_schema("climate-command.schema.json")
    properties = schema["properties"]
    bridge_package = BRIDGE_PACKAGE.read_text(encoding="utf-8")

    assert schema["required"] == ["humidity", "temperature"]
    assert schema["additionalProperties"] is False
    assert properties["humidity"]["minimum"] == 0
    assert properties["humidity"]["maximum"] == 100
    assert properties["temperature"]["minimum"] == -20
    assert properties["temperature"]["maximum"] == 60
    assert properties["quality"]["minimum"] == 0
    assert properties["quality"]["maximum"] == 100
    assert properties["source"]["maxLength"] == 64

    for fragment in (
        "humidity < 0.0f",
        "humidity > 100.0f",
        "temperature < -20.0f",
        "temperature > 60.0f",
        "quality < 0",
        "quality > 100",
        "source.size() > 64",
        'set_command_metadata("mqtt_invalid", 0)',
        "bridge->apply_fallback()",
    ):
        assert fragment in bridge_package


def test_supported_entry_points_compile_mqtt_but_disable_it_by_default() -> None:
    entry_points = (
        ROOT / "firmware/esp-sensor-esphome.yaml",
        ROOT / "firmware/fake-sensor-bridge.yaml",
        ROOT / "firmware/fake-sensor-esphome.yaml",
        ROOT / "firmware/esphome/package-test.yaml",
    )
    for entry_point in entry_points:
        config = entry_point.read_text(encoding="utf-8")
        assert 'mqtt_enabled: "false"' in config
        assert "mqtt_topic_prefix: idm/" in config
        assert "mqtt_transport:" in config
        assert "mqtt_diagnostics:" in config


def test_mqtt_transport_requires_credentials() -> None:
    # The shared MQTT component must carry username/password wiring so the
    # command path to the heat pump is never open. The actual fail-closed gate
    # for placeholder values lives in tools/check_esphome.py.
    config = yaml.safe_load(MQTT_PACKAGE.read_text(encoding="utf-8"))["mqtt"]
    assert config["username"] == "${mqtt_username}"
    assert config["password"] == "${mqtt_password}"
    # discover_ip (not the misspelled discover_ip) is the documented ESPHome
    # option; the typo was silently ignored before this contract existed.
    assert "discover_ip" not in config
    assert config["discovery_ip"] is True


def test_device_entry_points_ship_credential_placeholders() -> None:
    # Production entry points must ship the sentinel placeholder values so the
    # repository credential check refuses to validate them until a real secret
    # is configured. The compile fixture package-test.yaml deliberately uses
    # non-sentinel test values and is excluded here.
    device_entry_points = (
        ROOT / "firmware/esp-sensor-esphome.yaml",
        ROOT / "firmware/fake-sensor-bridge.yaml",
        ROOT / "firmware/fake-sensor-esphome.yaml",
    )
    for entry_point in device_entry_points:
        config = entry_point.read_text(encoding="utf-8")
        assert "mqtt_username: idm-mqtt-CHANGE-ME" in config
        assert "mqtt_password: idm-mqtt-CHANGE-ME" in config
        # The bridge/fake-sensor devices expose the local web UI; the pure
        # room sensor (esp-sensor-esphome.yaml) does not, so it has no
        # web_password substitution to gate.
        if entry_point.name != "esp-sensor-esphome.yaml":
            assert "web_password: CHANGE-ME-BEFORE-INSTALLATION" in config


def test_credential_placeholder_gate_rejects_sentinels() -> None:
    # tools/check_esphome.py must refuse any configuration that still contains
    # a shipped credential placeholder. This is the fail-closed gate. The check
    # reads ROOT/configuration, so we write a temporary fixture inside the repo
    # tree and remove it afterwards.
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "check_esphome", ROOT / "tools/check_esphome.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    fixture_rel = "tests/.fixture-credential-gate.yaml"
    fixture_abs = ROOT / fixture_rel
    try:
        for placeholder in module.CREDENTIAL_PLACEHOLDERS:
            fixture_abs.write_text(
                f"web_password: {placeholder}\n", encoding="utf-8"
            )
            try:
                module.check_credential_placeholders(fixture_rel)
            except SystemExit:
                continue
            raise AssertionError(
                f"placeholder {placeholder} was not rejected by the gate"
            )
    finally:
        if fixture_abs.exists():
            fixture_abs.unlink()


def test_ci_compile_override_substitutes_credentials() -> None:
    # The CI compile proof (--ci) must produce a throwaway copy of every device
    # entry point with synthetic non-secret credentials and leave NO sentinel
    # behind, while the committed file keeps its fail-closed placeholders.
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "check_esphome", ROOT / "tools/check_esphome.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    for entry_point in module.DEVICE_ENTRY_POINTS:
        override = module.materialize_ci_override(entry_point)
        try:
            text = override.read_text(encoding="utf-8")
            for placeholder in module.CREDENTIAL_PLACEHOLDERS:
                assert placeholder not in text, (
                    f"CI override for {entry_point} still contains {placeholder}"
                )
            assert module.CI_TEST_USERNAME in text
            # Only the bridge/fake-sensor devices expose the local web UI;
            # the pure room sensor (esp-sensor-esphome.yaml) has no
            # web_password substitution, so its override has none either.
            if not entry_point.endswith("esp-sensor-esphome.yaml"):
                assert module.CI_TEST_WEB_PASSWORD in text
        finally:
            if override.exists():
                override.unlink()

    # The committed entry points must still carry the fail-closed placeholders.
    for entry_point in module.DEVICE_ENTRY_POINTS:
        committed = (ROOT / entry_point).read_text(encoding="utf-8")
        assert "idm-mqtt-CHANGE-ME" in committed
        # The pure room sensor (esp-sensor-esphome.yaml) has no web UI and
        # therefore no web_password substitution to gate.
        if not entry_point.endswith("esp-sensor-esphome.yaml"):
            assert "CHANGE-ME-BEFORE-INSTALLATION" in committed


def test_representative_payloads_validate_against_their_schemas() -> None:
    # Validate at least one well-formed sample payload against each MQTT JSON
    # schema, and confirm a malformed payload is rejected. This catches schema
    # drift that hand-written structural assertions miss.
    command_schema = load_schema("climate-command.schema.json")
    valid_command = {
        "humidity": 62.5,
        "temperature": 22.4,
        "source": "home_assistant",
        "quality": 95,
    }
    Draft202012Validator(command_schema).validate(valid_command)
    invalid_command = dict(valid_command, humidity=150.0)
    with pytest.raises(Exception):
        Draft202012Validator(command_schema).validate(invalid_command)

    # The diagnostics schemas describe the device-authored payloads; validate a
    # minimal-but-complete sample against each.
    for schema_name, sample in (
        (
            "bridge-diagnostics.schema.json",
            {
                "schema_version": 1,
                "device": "idm-fake-sensor",
                "mode": "analog_bridge",
                "bridge_state": "active",
                "bridge_error": "none",
                "safe_active": False,
                "stale": False,
                "fault": False,
                "output_ready": True,
                "effective_humidity": 55.0,
                "effective_temperature": 23.0,
                "command_source": "home_assistant",
                "command_quality": 95,
                "output_fault": False,
                "output_error": "none",
                "humidity_dac_code": 2253,
                "temperature_digipot_code": 128,
                "target_resistance_ohm": 1420.0,
                "calibration_status": "stored_v2",
                "calibration_version": 2,
                "calibration_using_factory": False,
                "uptime_s": 3600,
            },
        ),
        (
            "room-diagnostics.schema.json",
            {
                "schema_version": 1,
                "device": "idm-room-sensor",
                "mode": "room_sensor",
                "online": True,
                "temperature": 21.5,
                "humidity": 57.0,
                "wifi_rssi": -58,
                "uptime_s": 3600,
            },
        ),
    ):
        schema = load_schema(schema_name)
        Draft202012Validator(schema).validate(sample)


def test_bridge_command_topics_are_deterministic_and_not_retained() -> None:
    expected_topics = (
        "${mqtt_topic_prefix}/command/climate/set",
        "${mqtt_topic_prefix}/command/humidity/set",
        "${mqtt_topic_prefix}/command/humidity/state",
        "${mqtt_topic_prefix}/command/temperature/set",
        "${mqtt_topic_prefix}/command/temperature/state",
    )
    combined_config = "\n".join(
        (
            BRIDGE_PACKAGE.read_text(encoding="utf-8"),
            (ROOT / "firmware/fake-sensor-bridge.yaml").read_text(
                encoding="utf-8"
            ),
            (ROOT / "firmware/fake-sensor-esphome.yaml").read_text(
                encoding="utf-8"
            ),
        )
    )
    for topic in expected_topics:
        assert topic in combined_config

    for entry_point in (
        ROOT / "firmware/fake-sensor-bridge.yaml",
        ROOT / "firmware/fake-sensor-esphome.yaml",
    ):
        config = entry_point.read_text(encoding="utf-8")
        assert config.count("command_retain: false") == 2
        assert config.count("subscribe_qos: 1") == 2
        assert "command_source:" in config
        assert "command_quality:" in config


def test_diagnostics_are_republished_after_connect_and_periodically() -> None:
    common_package = MQTT_PACKAGE.read_text(encoding="utf-8")
    bridge_package = BRIDGE_PACKAGE.read_text(encoding="utf-8")
    room_package = ROOM_PACKAGE.read_text(encoding="utf-8")

    assert "on_connect:" in common_package
    assert "interval: 30s" in common_package
    assert common_package.count(
        "script.execute: publish_idm_mqtt_diagnostics"
    ) == 2
    for package in (bridge_package, room_package):
        assert "${mqtt_topic_prefix}/diagnostics/state" in package
        assert "qos: 1" in package
        assert "retain: true" in package

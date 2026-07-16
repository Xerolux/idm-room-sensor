"""Static contract tests for MQTT discovery, safety and diagnostics."""

from __future__ import annotations

import json
from pathlib import Path
import re

import yaml


ROOT = Path(__file__).resolve().parents[1]
MQTT_PACKAGE = ROOT / "firmware/esphome/packages/mqtt.yaml"
BRIDGE_PACKAGE = ROOT / "firmware/esphome/packages/fake-sensor-mqtt.yaml"
ROOM_PACKAGE = ROOT / "firmware/esphome/packages/room-sensor-mqtt.yaml"
SCHEMA_DIRECTORY = ROOT / "firmware/mqtt"


def load_schema(filename: str) -> dict:
    return json.loads(
        (SCHEMA_DIRECTORY / filename).read_text(encoding="utf-8")
    )


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

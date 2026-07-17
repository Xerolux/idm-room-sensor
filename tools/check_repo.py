#!/usr/bin/env python3
"""Fast repository integrity checks used locally and in CI."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import yaml


class HomeAssistantLoader(yaml.SafeLoader):
    """YAML loader that preserves Home Assistant's ``!input`` values."""


HomeAssistantLoader.add_constructor(
    "!input",
    lambda loader, node: {"!input": loader.construct_scalar(node)},
)
for yaml_tag in ("!include", "!lambda", "!secret"):
    HomeAssistantLoader.add_constructor(
        yaml_tag,
        lambda loader, node, tag=yaml_tag: {
            tag: loader.construct_scalar(node),
        },
    )

REQUIRED = [
    "README.md",
    "PROJECT_STATUS.md",
    "TASKS.md",
    ".github/workflows/esphome.yml",
    "docs",
    "hardware",
    "firmware",
    "firmware/esp-idf/diagnostics.schema.json",
    "firmware/esp-idf/platformio.ini",
    "firmware/esp-idf/README.md",
    "firmware/requirements.txt",
    "homeassistant",
    "tools/check_esphome.py",
    "validation",
]


def main() -> None:
    errors: list[str] = []
    missing = [path for path in REQUIRED if not Path(path).exists()]
    if missing:
        errors.append("Missing required paths: " + ", ".join(missing))

    readme = Path("README.md").read_text(encoding="utf-8")
    # Safety warnings are pinned to stable HTML comment sentinels rather than
    # natural-language substrings, so a prose rewrite that preserves the safety
    # meaning does not break CI. The visible CAUTION block carries the human
    # wording; these sentinels are the machine-readable contract.
    warning_checks = {
        "work in progress": "<!-- SAFETY:WORK_IN_PROGRESS -->" in readme,
        "not production": "<!-- SAFETY:NOT_PRODUCTION -->" in readme,
    }
    for warning, present in warning_checks.items():
        if not present:
            errors.append(f"README.md is missing required warning: {warning}")

    ignored_directories = {
        ".git",
        ".esphome",
        ".pio",
        "__pycache__",
        ".pytest_cache",
    }
    for root, directories, filenames in os.walk("."):
        directories[:] = [
            name for name in directories if name not in ignored_directories
        ]
        for filename in filenames:
            path = Path(root, filename)
            try:
                if path.suffix in {".yaml", ".yml"}:
                    with path.open(encoding="utf-8") as handle:
                        yaml.load(handle, Loader=HomeAssistantLoader)
                elif path.suffix == ".json":
                    with path.open(encoding="utf-8") as handle:
                        json.load(handle)
            except Exception as err:  # pragma: no cover - parser messages vary
                errors.append(f"{path}: {err}")

    climate_package = Path("homeassistant/climate-engine-package.yaml").read_text(encoding="utf-8")
    required_fragments = (
        "last_reported",
        "sensor.idm_selected_external_room_temperature",
        "sensor.idm_selected_external_humidity",
        "idm_heatpump.write_register",
        "address: 1650",
        "address: 1692",
        "idm_climate_selection_reason",
        "idm_selected_source_age",
        "idm_external_climate_publish_status",
        "idm_external_climate_publish_problem",
    )
    for fragment in required_fragments:
        if fragment not in climate_package:
            errors.append(f"climate-engine-package.yaml is missing contract fragment: {fragment}")

    bridge_component = Path(
        "firmware/components/idm_bridge/idm_bridge_core.h"
    ).read_text(encoding="utf-8")
    bridge_contract_fragments = (
        "BridgeState::STALE_SAFE",
        "BridgeState::INVALID_SAFE",
        "BridgeState::OUTPUT_FAULT_SAFE",
        "output_fault_latched_",
        "normalized_humidity",
        "normalized_temperature",
    )
    for fragment in bridge_contract_fragments:
        if fragment not in bridge_component:
            errors.append(
                f"idm_bridge_core.h is missing safety-contract fragment: {fragment}"
            )

    bridge_config = Path("firmware/fake-sensor-bridge.yaml").read_text(
        encoding="utf-8"
    )
    for fragment in (
        "idm_bridge:",
        "idm_bridge.set_values:",
        "analog_output:",
        "output_ready:",
        "accept_unverified_kty_calibration: true",
    ):
        if fragment not in bridge_config:
            errors.append(
                f"fake-sensor-bridge.yaml is missing bridge integration: {fragment}"
            )

    analog_driver = Path(
        "firmware/components/idm_bridge/idm_analog_output.cpp"
    ).read_text(encoding="utf-8")
    for fragment in (
        "0x40",
        "const uint8_t data[] = {0x00, code}",
        "humidity_dac_write_failed",
        "temperature_digipot_write_failed",
    ):
        if fragment not in analog_driver:
            errors.append(
                f"idm_analog_output.cpp is missing driver contract: {fragment}"
            )

    calibration_storage = Path(
        "firmware/components/idm_bridge/idm_calibration_storage_core.h"
    ).read_text(encoding="utf-8")
    for fragment in (
        "CalibrationRecordV2",
        "CalibrationRecordV1",
        "INVALID_USING_FACTORY",
        "crc32",
        "Never downgrade",
    ):
        if fragment not in calibration_storage:
            errors.append(
                "idm_calibration_storage_core.h is missing storage contract: "
                f"{fragment}"
            )

    quality_core = Path("firmware/common/value_quality.h").read_text(
        encoding="utf-8"
    )
    for fragment in (
        "assess_climate_value",
        "ValueQualityStatus::STALE",
        "ValueQualityStatus::NON_FINITE",
        "BELOW_MINIMUM_QUALITY",
    ):
        if fragment not in quality_core:
            errors.append(
                f"value_quality.h is missing quality contract: {fragment}"
            )

    mqtt_package = Path("firmware/esphome/packages/mqtt.yaml").read_text(
        encoding="utf-8"
    )
    for fragment in (
        "clean_session: true",
        "discovery_retain: true",
        "${mqtt_topic_prefix}/availability",
        "reboot_timeout: 0s",
        "publish_idm_mqtt_diagnostics",
    ):
        if fragment not in mqtt_package:
            errors.append(
                f"mqtt.yaml is missing MQTT safety contract: {fragment}"
            )

    bridge_mqtt = Path(
        "firmware/esphome/packages/fake-sensor-mqtt.yaml"
    ).read_text(encoding="utf-8")
    for fragment in (
        "${mqtt_topic_prefix}/command/climate/set",
        "${mqtt_topic_prefix}/diagnostics/state",
        'set_command_metadata("mqtt_invalid", 0)',
        "bridge->apply_fallback()",
    ):
        if fragment not in bridge_mqtt:
            errors.append(
                "fake-sensor-mqtt.yaml is missing MQTT bridge contract: "
                f"{fragment}"
            )

    webui_package = Path(
        "firmware/esphome/packages/fake-sensor-webui.yaml"
    ).read_text(encoding="utf-8")
    for fragment in (
        "version: 3",
        "local: true",
        "ota: false",
        "idm_web_dangerous_action_confirmation",
        "delay: 60s",
        "confirmation_required",
        "idm_webui_diagnostic_export",
    ):
        if fragment not in webui_package:
            errors.append(
                "fake-sensor-webui.yaml is missing local UI contract: "
                f"{fragment}"
            )

    native_main = Path("firmware/esp-idf/main/main.cpp").read_text(
        encoding="utf-8"
    )
    for fragment in (
        "i2c_master_transmit",
        "CONFIG_IDM_WIFI_SSID",
        "esp_task_wdt_add",
        "CalibrationStorageCore::load",
        "esp_https_ota",
        "/api/v1/diagnostics",
        "/api/v1/climate",
        "/api/v1/fallback",
        "/api/v1/calibration",
        "/api/v1/ota",
        "X-IDM-Confirm",
    ):
        if fragment not in native_main:
            errors.append(
                f"esp-idf/main.cpp is missing native firmware contract: {fragment}"
            )

    fake_sensor_config = Path("firmware/fake-sensor-esphome.yaml").read_text(
        encoding="utf-8"
    )
    for fragment in (
        "analog_output:",
        "idm_bridge.set_values:",
        "idm_bridge.set_calibration:",
        "idm_bridge.reset_calibration:",
        "calibration_status:",
    ):
        if fragment not in fake_sensor_config:
            errors.append(
                f"fake-sensor-esphome.yaml is missing real output integration: {fragment}"
            )

    blueprint_names = (
        "cooling_inhibit.yaml",
        "critical_room.yaml",
        "stale_guard.yaml",
    )
    for blueprint_name in blueprint_names:
        blueprint_path = Path("homeassistant/blueprints/automation") / blueprint_name
        with blueprint_path.open(encoding="utf-8") as handle:
            blueprint = yaml.load(handle, Loader=HomeAssistantLoader)
        if not blueprint.get("actions"):
            errors.append(f"{blueprint_name} must contain executable actions")
        description = blueprint.get("blueprint", {}).get("description", "")
        if "placeholder" in description.lower():
            errors.append(f"{blueprint_name} must not remain a placeholder")

    if errors:
        raise SystemExit("\n".join(errors))

    # FILE_INVENTORY.md must match the generator output exactly. The inventory
    # used to be hand-maintained and drifted; it is now generated from
    # `git ls-files` by tools/generate_file_inventory.py. Fail CI if someone
    # adds/removes/renames a tracked file without rerunning the generator.
    inventory_result = subprocess.run(
        [sys.executable, "tools/generate_file_inventory.py", "--check"],
        capture_output=True,
        text=True,
    )
    if inventory_result.returncode:
        errors.append(
            "FILE_INVENTORY.md is out of sync with git ls-files. Regenerate "
            "with: python3 tools/generate_file_inventory.py"
        )

    if errors:
        raise SystemExit("\n".join(errors))

    print("Repository checks passed.")


if __name__ == "__main__":
    main()

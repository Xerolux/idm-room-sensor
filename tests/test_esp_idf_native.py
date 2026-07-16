"""Static and schema contracts for the native ESP-IDF firmware."""

from __future__ import annotations

import csv
import json
from pathlib import Path
import re

from jsonschema import Draft202012Validator


ROOT = Path(__file__).resolve().parents[1]
NATIVE_DIRECTORY = ROOT / "firmware/esp-idf"
MAIN = NATIVE_DIRECTORY / "main/main.cpp"


def read_native_source() -> str:
    return MAIN.read_text(encoding="utf-8")


def test_native_diagnostic_fields_match_schema() -> None:
    schema = json.loads(
        (NATIVE_DIRECTORY / "diagnostics.schema.json").read_text(
            encoding="utf-8"
        )
    )
    Draft202012Validator.check_schema(schema)
    source = read_native_source()
    diagnostics_handler = source[
        source.index("esp_err_t diagnostics_get_handler"):
        source.index("esp_err_t climate_post_handler")
    ]
    payload_fields = set(
        re.findall(
            r'(?:cJSON_Add\w+ToObject|add_number_or_null)'
            r'\(\s*root,\s*"([^"]+)"',
            diagnostics_handler,
        )
    )

    assert set(schema["required"]) == set(schema["properties"])
    assert payload_fields == set(schema["required"])
    assert schema["additionalProperties"] is False


def test_native_hardware_and_fail_safe_contract() -> None:
    source = read_native_source()
    runtime = (
        NATIVE_DIRECTORY / "main/native_runtime.h"
    ).read_text(encoding="utf-8")

    for fragment in (
        "i2c_new_master_bus",
        "CONFIG_IDM_HUMIDITY_I2C_ADDRESS",
        "CONFIG_IDM_TEMPERATURE_I2C_ADDRESS",
        "const uint8_t humidity_data[] = {",
        "0x40",
        "const uint8_t temperature_data[] = {",
        "s_runtime.boot",
        "apply_outputs_once(startup_ms)",
        "s_runtime.tick(current_ms)",
        "s_runtime.record_output_result(success)",
        "esp_task_wdt_add",
        "esp_task_wdt_reset",
    ):
        assert fragment in source

    for fragment in (
        "assess_climate_value",
        "minimum_command_quality",
        "apply_manual_fallback",
        "set_output_fault(true)",
        "set_output_fault(false)",
        "output_failures_",
    ):
        assert fragment in runtime


def test_native_persistence_network_and_ota_contract() -> None:
    source = read_native_source()
    defaults = (
        NATIVE_DIRECTORY / "sdkconfig.defaults"
    ).read_text(encoding="utf-8")

    for fragment in (
        "CalibrationRecordV2",
        "CalibrationRecordV1",
        "CalibrationStorageCore::make_record",
        "CalibrationStorageCore::decode_record",
        "CalibrationStorageCore::load",
        "nvs_commit",
        "calibration_equal",
        "esp_wifi_connect",
        "WIFI_EVENT_STA_DISCONNECTED",
        "IP_EVENT_STA_GOT_IP",
        "esp_https_ota",
        "esp_crt_bundle_attach",
        'std::strncmp(url->valuestring, "https://", 8)',
        "esp_ota_mark_app_valid_cancel_rollback",
    ):
        assert fragment in source

    for fragment in (
        'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"',
        "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y",
        "CONFIG_ESP_TASK_WDT_INIT=y",
        "CONFIG_ESP_TASK_WDT_PANIC=y",
        "CONFIG_ESP_BROWNOUT_DET=y",
        "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y",
    ):
        assert fragment in defaults


def test_mutating_routes_require_auth_and_dangerous_confirmation() -> None:
    source = read_native_source()

    expected_routes = {
        "/api/v1/diagnostics": "HTTP_GET",
        "/api/v1/climate": "HTTP_POST",
        "/api/v1/fallback": "HTTP_POST",
        "/api/v1/calibration": "HTTP_POST",
        "/api/v1/calibration/reset": "HTTP_POST",
        "/api/v1/ota": "HTTP_POST",
    }
    for route, method in expected_routes.items():
        route_block = source[source.index(f'.uri = "{route}"'):]
        assert f".method = {method}" in route_block[:220]

    assert "mutating API disabled until IDM_API_TOKEN is configured" in source
    assert '"Authorization"' in source
    assert '"Bearer " + token' in source
    assert "constant_time_equal" in source
    assert '"X-IDM-Confirm"' in source
    assert '"apply-calibration"' in source
    assert '"reset-calibration"' in source
    assert '"firmware-update"' in source

    for handler in (
        "climate_post_handler",
        "fallback_post_handler",
        "calibration_post_handler",
        "calibration_reset_post_handler",
        "ota_post_handler",
    ):
        body = source[source.index(f"esp_err_t {handler}"):]
        assert "authorize_mutation(request)" in body[:700]


def test_partition_table_has_two_equal_ota_slots() -> None:
    with (NATIVE_DIRECTORY / "partitions.csv").open(
        encoding="utf-8", newline=""
    ) as handle:
        rows = [
            row
            for row in csv.reader(handle)
            if row and not row[0].lstrip().startswith("#")
        ]

    partitions = {
        row[0].strip(): {
            "type": row[1].strip(),
            "subtype": row[2].strip(),
            "offset": int(row[3].strip(), 0),
            "size": int(row[4].strip(), 0),
        }
        for row in rows
    }
    assert partitions["ota_0"]["type"] == "app"
    assert partitions["ota_0"]["subtype"] == "ota_0"
    assert partitions["ota_1"]["subtype"] == "ota_1"
    assert partitions["ota_0"]["size"] == partitions["ota_1"]["size"]
    assert partitions["ota_0"]["offset"] == 0x20000
    assert (
        partitions["ota_1"]["offset"] + partitions["ota_1"]["size"]
        <= 0x400000
    )

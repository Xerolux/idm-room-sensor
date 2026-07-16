import csv
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def test_idm_analog_output_calibration_math(tmp_path):
    executable = tmp_path / "idm_analog_output_core_test"
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(ROOT),
            str(ROOT / "tests/cpp/idm_analog_output_core_test.cpp"),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)


def test_committed_kty_csv_matches_firmware_table():
    with (ROOT / "firmware/common/kty_table.csv").open(
        encoding="utf-8", newline=""
    ) as handle:
        csv_points = [
            (float(row["temperature_c"]), float(row["resistance_ohm"]))
            for row in csv.DictReader(handle)
        ]

    header = (
        ROOT
        / "firmware/components/idm_bridge/idm_analog_output_core.h"
    ).read_text(encoding="utf-8")
    table_block = header.split(
        "KTY81_210_PROTOTYPE_TABLE[] = {", maxsplit=1
    )[1].split("};", maxsplit=1)[0]
    firmware_points = [
        (float(temperature), float(resistance))
        for temperature, resistance in re.findall(
            r"\{(-?\d+(?:\.\d+)?)f,\s*(\d+(?:\.\d+)?)f\}",
            table_block,
        )
    ]

    assert firmware_points == csv_points

from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def test_idm_calibration_storage_format_and_migration(tmp_path):
    executable = tmp_path / "idm_calibration_storage_core_test"
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(ROOT),
            str(
                ROOT
                / "tests/cpp/idm_calibration_storage_core_test.cpp"
            ),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)

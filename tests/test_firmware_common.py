from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def test_firmware_common_math_and_quality(tmp_path):
    executable = tmp_path / "firmware_common_test"
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(ROOT),
            str(ROOT / "tests/cpp/firmware_common_test.cpp"),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)

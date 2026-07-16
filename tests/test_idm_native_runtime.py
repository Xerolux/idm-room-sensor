from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def test_idm_native_runtime_state_machine(tmp_path):
    executable = tmp_path / "idm_native_runtime_test"
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(ROOT),
            str(ROOT / "tests/cpp/idm_native_runtime_test.cpp"),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)

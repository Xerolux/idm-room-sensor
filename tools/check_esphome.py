#!/usr/bin/env python3
"""Validate or compile every supported ESPHome configuration."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
REQUIREMENTS = ROOT / "firmware" / "requirements.txt"
BUILD_DATA_DIR = ROOT / ".esphome"
CONFIGURATIONS = (
    "firmware/esp-sensor-esphome.yaml",
    "firmware/fake-sensor-bridge.yaml",
    "firmware/fake-sensor-esphome.yaml",
    "firmware/esphome/package-test.yaml",
    "hardware/esp-sensor/firmware/esphome.yaml",
    "hardware/fake-sensor/firmware/esphome.yaml",
)


def expected_version() -> str:
    match = re.search(
        r"^esphome==([0-9.]+)$",
        REQUIREMENTS.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    if match is None:
        raise SystemExit("firmware/requirements.txt must pin esphome with ==")
    return match.group(1)


def check_version() -> None:
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "version"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    actual = result.stdout.strip().removeprefix("Version: ")
    expected = expected_version()
    if actual != expected:
        raise SystemExit(
            f"ESPHome {expected} is required, but {actual or 'unknown'} is installed"
        )
    print(f"ESPHome version: {actual}")


def validate(configuration: str) -> None:
    environment = os.environ.copy()
    environment.setdefault("ESPHOME_DATA_DIR", str(BUILD_DATA_DIR))
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "config", configuration],
        cwd=ROOT,
        env=environment,
        capture_output=True,
        text=True,
    )
    if result.returncode:
        sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(
            f"ESPHome configuration validation failed: {configuration}"
        )
    print(f"Validated: {configuration}")


def compile_configuration(configuration: str) -> None:
    print(f"Compiling: {configuration}", flush=True)
    environment = os.environ.copy()
    environment.setdefault("ESPHOME_DATA_DIR", str(BUILD_DATA_DIR))
    subprocess.run(
        [sys.executable, "-m", "esphome", "compile", configuration],
        cwd=ROOT,
        env=environment,
        check=True,
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--compile",
        action="store_true",
        help="Compile all targets after validating them.",
    )
    args = parser.parse_args()

    check_version()
    for configuration in CONFIGURATIONS:
        validate(configuration)
    if args.compile:
        for configuration in CONFIGURATIONS:
            compile_configuration(configuration)


if __name__ == "__main__":
    main()

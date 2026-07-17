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
# Sentinel credential placeholders. A configuration that resolves to one of
# these values is rejected before validation so an accidental production build
# without real credentials cannot happen. The compile fixture package-test.yaml
# deliberately uses non-sentinel test values to stay compilable.
CREDENTIAL_PLACEHOLDERS = (
    "idm-mqtt-CHANGE-ME",
    "CHANGE-ME-BEFORE-INSTALLATION",
)
# Device entry points that ship credential placeholders and therefore need a
# synthetic override in CI to prove they still compile. package-test.yaml is
# excluded because it already uses non-sentinel test values.
DEVICE_ENTRY_POINTS = (
    "firmware/esp-sensor-esphome.yaml",
    "firmware/fake-sensor-bridge.yaml",
    "firmware/fake-sensor-esphome.yaml",
)
# Non-secret synthetic values used only by the CI compile proof. These never
# reach a real device: they live in a throwaway working copy that is removed
# after the run, and the committed files keep their fail-closed placeholders.
CI_TEST_USERNAME = "idm-ci-compile-user"
CI_TEST_PASSWORD = "idm-ci-compile-password"
CI_TEST_WEB_PASSWORD = "idm-ci-compile-web-password"


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


def check_credential_placeholders(configuration: str) -> None:
    """Reject configurations that still carry credential placeholders.

    A device entry point must override the placeholder substitution values
    (mqtt_username, mqtt_password, web_password) before it may be validated.
    This is the fail-closed gate for the MQTT and web UI control paths; it
    complements the runtime checks documented in firmware/BUILDING.md.
    """
    source = (ROOT / configuration).read_text(encoding="utf-8")
    offenders = [
        placeholder
        for placeholder in CREDENTIAL_PLACEHOLDERS
        if placeholder in source
    ]
    if offenders:
        raise SystemExit(
            f"{configuration} still carries placeholder credential(s): "
            f"{', '.join(offenders)}. Override the substitution values "
            f"(mqtt_username, mqtt_password, web_password) with real secrets "
            f"before validation."
        )


def validate(configuration: str) -> None:
    check_credential_placeholders(configuration)
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


def ci_override_path(configuration: str) -> Path:
    """Location of the throwaway CI working copy for a device entry point.

    The copy lives next to the original so ESPHome resolves relative
    `!include` paths exactly as in production. Files are removed after use.
    """
    original = ROOT / configuration
    return original.with_name(original.stem + ".ci-compile.yaml")


def materialize_ci_override(configuration: str) -> Path:
    """Write a throwaway copy of a device entry point with synthetic CI creds.

    Only used when --ci is passed (CI compile proof). The committed file keeps
    its fail-closed placeholders; this copy substitutes non-secret test values
    so ESPHome can compile the target without weakening the production gate.
    """
    source = (ROOT / configuration).read_text(encoding="utf-8")
    source = source.replace("idm-mqtt-CHANGE-ME", CI_TEST_USERNAME)
    source = source.replace(
        "CHANGE-ME-BEFORE-INSTALLATION", CI_TEST_WEB_PASSWORD
    )
    # Guard: the copy must contain no remaining sentinel afterwards.
    for placeholder in CREDENTIAL_PLACEHOLDERS:
        if placeholder in source:
            raise SystemExit(
                f"CI override for {configuration} still contains {placeholder}; "
                f"this is a generator bug."
            )
    override = ci_override_path(configuration)
    override.write_text(source, encoding="utf-8")
    return override


def compile_configuration(configuration: str, *, ci: bool) -> None:
    target = configuration
    cleanup: Path | None = None
    if ci and configuration in DEVICE_ENTRY_POINTS:
        # The committed device entry point carries fail-closed placeholders
        # by design. To prove it still compiles, build a throwaway copy with
        # non-secret synthetic credentials that never reach a real device.
        cleanup = materialize_ci_override(configuration)
        target = str(cleanup.relative_to(ROOT))
    print(f"Compiling: {target}", flush=True)
    environment = os.environ.copy()
    environment.setdefault("ESPHOME_DATA_DIR", str(BUILD_DATA_DIR))
    try:
        subprocess.run(
            [sys.executable, "-m", "esphome", "compile", target],
            cwd=ROOT,
            env=environment,
            check=True,
        )
    finally:
        if cleanup is not None and cleanup.exists():
            cleanup.unlink()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--compile",
        action="store_true",
        help="Compile all targets after validating them.",
    )
    parser.add_argument(
        "--ci",
        action="store_true",
        help=(
            "CI compile proof: substitute non-secret synthetic credentials "
            "into device entry points so they can be compiled without "
            "weakening the production fail-closed gate."
        ),
    )
    args = parser.parse_args()

    if args.ci and not args.compile:
        raise SystemExit("--ci only makes sense together with --compile")

    check_version()
    for configuration in CONFIGURATIONS:
        validate(configuration)
    if args.compile:
        for configuration in CONFIGURATIONS:
            compile_configuration(configuration, ci=args.ci)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Calculate dew point with the Magnus approximation."""

from __future__ import annotations

import argparse
import math


def dew_point_c(temperature: float, humidity: float) -> float:
    """Return dew point in °C for temperature in °C and relative humidity."""
    if not math.isfinite(temperature) or not math.isfinite(humidity):
        raise ValueError("temperature and humidity must be finite")
    if not 0 < humidity <= 100:
        raise ValueError("humidity must be greater than 0 and at most 100")
    gamma = math.log(humidity / 100) + 17.62 * temperature / (243.12 + temperature)
    return 243.12 * gamma / (17.62 - gamma)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("temperature", type=float)
    parser.add_argument("humidity", type=float)
    args = parser.parse_args()
    print(round(dew_point_c(args.temperature, args.humidity), 3))


if __name__ == "__main__":
    main()

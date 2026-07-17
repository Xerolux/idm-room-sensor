#!/usr/bin/env python3
"""Calculate dew point with the Magnus approximation."""

from __future__ import annotations

import argparse
import math


def dew_point_c(temperature: float, humidity: float) -> float:
    """Return dew point in °C for temperature in °C and relative humidity.

    Mirrors firmware/common/dew_point.h, including the temperature domain guard
    (t <= -b would make the Magnus denominator singular) so the Python
    reference and the C++ implementation accept exactly the same inputs.
    """
    b = 243.12
    if not math.isfinite(temperature) or not math.isfinite(humidity):
        raise ValueError("temperature and humidity must be finite")
    if temperature <= -b:
        raise ValueError("temperature must be greater than -243.12 °C")
    if not 0 < humidity <= 100:
        raise ValueError("humidity must be greater than 0 and at most 100")
    gamma = math.log(humidity / 100) + 17.62 * temperature / (b + temperature)
    return b * gamma / (17.62 - gamma)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("temperature", type=float)
    parser.add_argument("humidity", type=float)
    args = parser.parse_args()
    print(round(dew_point_c(args.temperature, args.humidity), 3))


if __name__ == "__main__":
    main()

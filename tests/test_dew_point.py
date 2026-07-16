from __future__ import annotations

import math

import pytest

from tools.dew_point import dew_point_c


def test_known_dew_point_example():
    assert dew_point_c(23, 60) == pytest.approx(14.815, abs=0.001)


def test_higher_temperature_can_be_more_critical_than_higher_humidity():
    warmer_room = dew_point_c(26, 55)
    more_humid_room = dew_point_c(20, 70)

    assert warmer_room > more_humid_room


def test_saturated_air_has_dew_point_equal_to_temperature():
    assert dew_point_c(20, 100) == pytest.approx(20, abs=0.001)


def test_dew_point_does_not_exceed_air_temperature():
    assert dew_point_c(10, 40) <= 10


@pytest.mark.parametrize("humidity", [0, -1, 100.1])
def test_invalid_humidity_is_rejected(humidity):
    with pytest.raises(ValueError):
        dew_point_c(23, humidity)


@pytest.mark.parametrize("value", [math.nan, math.inf, -math.inf])
def test_non_finite_inputs_are_rejected(value):
    with pytest.raises(ValueError):
        dew_point_c(value, 60)
    with pytest.raises(ValueError):
        dew_point_c(23, value)

// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Xerolux

#pragma once

#include <cmath>
#include <cstdint>

struct ClimateValue {
  float value;
  uint32_t timestamp_ms;
  uint8_t quality;
  bool valid;
};

enum class ValueQualityStatus : uint8_t {
  GOOD = 0,
  INVALID_FLAG,
  NON_FINITE,
  OUT_OF_RANGE,
  INVALID_QUALITY,
  STALE,
  BELOW_MINIMUM_QUALITY,
};

struct ValueQualityAssessment {
  ValueQualityStatus status;
  uint32_t age_ms;
  bool usable;
};

inline ValueQualityAssessment assess_climate_value(
    const ClimateValue &value, uint32_t now_ms, uint32_t stale_after_ms,
    float minimum, float maximum, uint8_t minimum_quality) {
  const uint32_t age_ms =
      static_cast<uint32_t>(now_ms - value.timestamp_ms);
  if (!value.valid)
    return {ValueQualityStatus::INVALID_FLAG, age_ms, false};
  if (!std::isfinite(value.value))
    return {ValueQualityStatus::NON_FINITE, age_ms, false};
  if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
      minimum > maximum || value.value < minimum || value.value > maximum)
    return {ValueQualityStatus::OUT_OF_RANGE, age_ms, false};
  if (value.quality > 100)
    return {ValueQualityStatus::INVALID_QUALITY, age_ms, false};
  if (stale_after_ms != 0 && age_ms >= stale_after_ms)
    return {ValueQualityStatus::STALE, age_ms, false};
  if (value.quality < minimum_quality)
    return {ValueQualityStatus::BELOW_MINIMUM_QUALITY, age_ms, false};
  return {ValueQualityStatus::GOOD, age_ms, true};
}

inline const char *value_quality_status_name(ValueQualityStatus status) {
  switch (status) {
    case ValueQualityStatus::GOOD:
      return "good";
    case ValueQualityStatus::INVALID_FLAG:
      return "invalid_flag";
    case ValueQualityStatus::NON_FINITE:
      return "non_finite";
    case ValueQualityStatus::OUT_OF_RANGE:
      return "out_of_range";
    case ValueQualityStatus::INVALID_QUALITY:
      return "invalid_quality";
    case ValueQualityStatus::STALE:
      return "stale";
    case ValueQualityStatus::BELOW_MINIMUM_QUALITY:
      return "below_minimum_quality";
  }
  return "unknown";
}

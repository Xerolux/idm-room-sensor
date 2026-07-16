#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "firmware/common/dew_point.h"
#include "firmware/common/value_quality.h"

static bool close_to(float actual, float expected, float tolerance = 0.001f) {
  return std::fabs(actual - expected) < tolerance;
}

int main() {
  assert(close_to(dew_point_c(23.0f, 60.0f), 14.815f));
  assert(close_to(dew_point_c(20.0f, 100.0f), 20.0f));
  assert(std::isnan(dew_point_c(20.0f, 0.0f)));
  assert(std::isnan(dew_point_c(20.0f, -1.0f)));
  assert(std::isnan(dew_point_c(20.0f, 100.01f)));
  assert(dew_point_c(10.0f, 40.0f) <= 10.0f);
  assert(std::isnan(dew_point_c(
      std::numeric_limits<float>::quiet_NaN(), 50.0f)));
  assert(std::isnan(dew_point_c(
      20.0f, std::numeric_limits<float>::infinity())));
  assert(std::isnan(dew_point_c(-243.12f, 50.0f)));

  const ClimateValue good{23.0f, 1000, 80, true};
  auto assessment =
      assess_climate_value(good, 1999, 1000, -20.0f, 60.0f, 50);
  assert(assessment.status == ValueQualityStatus::GOOD);
  assert(assessment.usable);
  assert(assessment.age_ms == 999);

  const ClimateValue minimum_boundary{-20.0f, 1000, 50, true};
  assert(assess_climate_value(minimum_boundary, 1000, 1000,
                              -20.0f, 60.0f, 50)
             .usable);
  const ClimateValue maximum_boundary{60.0f, 1000, 100, true};
  assert(assess_climate_value(maximum_boundary, 1000, 1000,
                              -20.0f, 60.0f, 50)
             .usable);

  assessment =
      assess_climate_value(good, 2000, 1000, -20.0f, 60.0f, 50);
  assert(assessment.status == ValueQualityStatus::STALE);
  assert(!assessment.usable);

  assessment =
      assess_climate_value(good, 500000, 0, -20.0f, 60.0f, 50);
  assert(assessment.status == ValueQualityStatus::GOOD);

  const ClimateValue invalid_flag{23.0f, 1000, 80, false};
  assert(assess_climate_value(invalid_flag, 1100, 1000, -20.0f,
                              60.0f, 50)
             .status == ValueQualityStatus::INVALID_FLAG);

  const ClimateValue non_finite{
      std::numeric_limits<float>::infinity(), 1000, 80, true};
  assert(assess_climate_value(non_finite, 1100, 1000, -20.0f,
                              60.0f, 50)
             .status == ValueQualityStatus::NON_FINITE);

  const ClimateValue out_of_range{60.01f, 1000, 80, true};
  assert(assess_climate_value(out_of_range, 1100, 1000, -20.0f,
                              60.0f, 50)
             .status == ValueQualityStatus::OUT_OF_RANGE);

  const ClimateValue invalid_quality{23.0f, 1000, 101, true};
  assert(assess_climate_value(invalid_quality, 1100, 1000,
                              -20.0f, 60.0f, 50)
             .status == ValueQualityStatus::INVALID_QUALITY);

  const ClimateValue low_quality{23.0f, 1000, 49, true};
  assert(assess_climate_value(low_quality, 1100, 1000, -20.0f,
                              60.0f, 50)
             .status ==
         ValueQualityStatus::BELOW_MINIMUM_QUALITY);
  assert(assess_climate_value(low_quality, 3000, 1000, -20.0f,
                              60.0f, 50)
             .status == ValueQualityStatus::STALE);
  assert(assess_climate_value(good, 1100, 1000, 60.0f, -20.0f,
                              50)
             .status == ValueQualityStatus::OUT_OF_RANGE);

  const ClimateValue wrapped{23.0f, UINT32_MAX - 49, 80, true};
  assessment =
      assess_climate_value(wrapped, 49, 101, -20.0f, 60.0f, 50);
  assert(assessment.status == ValueQualityStatus::GOOD);
  assert(assessment.age_ms == 99);
  assessment =
      assess_climate_value(wrapped, 50, 100, -20.0f, 60.0f, 50);
  assert(assessment.status == ValueQualityStatus::STALE);
  assert(assessment.age_ms == 100);

  assert(std::string(value_quality_status_name(
             ValueQualityStatus::BELOW_MINIMUM_QUALITY)) ==
         "below_minimum_quality");

  return 0;
}

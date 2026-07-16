#pragma once

#include <cmath>

inline float dew_point_c(float t, float rh) {
  constexpr float a = 17.62f;
  constexpr float b = 243.12f;
  if (!std::isfinite(t) || !std::isfinite(rh) || t <= -b ||
      rh <= 0.0f || rh > 100.0f)
    return NAN;

  const float gamma = std::log(rh / 100.0f) + a * t / (b + t);
  const float denominator = a - gamma;
  if (!std::isfinite(gamma) || !std::isfinite(denominator) ||
      std::fabs(denominator) < 1.0e-6f)
    return NAN;
  const float result = b * gamma / denominator;
  return std::isfinite(result) ? result : NAN;
}

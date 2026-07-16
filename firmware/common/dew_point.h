#pragma once
#include <cmath>
inline float dew_point_c(float t, float rh) {
  const float a=17.62f,b=243.12f;
  rh=std::fmax(1.0f,std::fmin(100.0f,rh));
  const float g=std::log(rh/100.0f)+a*t/(b+t);
  return b*g/(a-g);
}

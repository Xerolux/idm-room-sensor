#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "idm_calibration_storage_core.h"

namespace esphome::idm_bridge {

struct KtyPoint {
  float temperature;
  float resistance;
};

inline constexpr KtyPoint KTY81_210_PROTOTYPE_TABLE[] = {
    {-20.0f, 684.0f}, {-10.0f, 815.0f}, {0.0f, 980.0f},
    {10.0f, 1180.0f}, {20.0f, 1420.0f}, {25.0f, 1560.0f},
    {30.0f, 1710.0f}, {40.0f, 2020.0f}, {50.0f, 2370.0f},
    {60.0f, 2760.0f},
};

class IdmAnalogOutputCore {
 public:
  void set_humidity_code_range(uint16_t minimum, uint16_t maximum) {
    this->calibration_.humidity_code_min =
        std::min<uint16_t>(minimum, CALIBRATION_HUMIDITY_CODE_MAX);
    this->calibration_.humidity_code_max =
        std::min<uint16_t>(maximum, CALIBRATION_HUMIDITY_CODE_MAX);
  }

  void set_temperature_resistance_range(float minimum, float maximum) {
    this->calibration_.temperature_resistance_min = minimum;
    this->calibration_.temperature_resistance_max = maximum;
  }

  void set_temperature_code_inverted(bool inverted) {
    this->calibration_.temperature_code_inverted = inverted;
  }

  bool set_calibration(const IdmAnalogCalibration &calibration) {
    if (!valid_calibration(calibration))
      return false;
    this->calibration_ = calibration;
    return true;
  }

  const IdmAnalogCalibration &calibration() const {
    return this->calibration_;
  }

  uint16_t humidity_code(float humidity) const {
    const float normalized =
        std::clamp(humidity / 100.0f, 0.0f, 1.0f);
    const float code =
        this->calibration_.humidity_code_min +
        normalized * (this->calibration_.humidity_code_max -
                      this->calibration_.humidity_code_min);
    return static_cast<uint16_t>(std::lround(code));
  }

  float resistance_for_temperature(float temperature) const {
    const auto *table = KTY81_210_PROTOTYPE_TABLE;
    constexpr size_t count =
        sizeof(KTY81_210_PROTOTYPE_TABLE) /
        sizeof(KTY81_210_PROTOTYPE_TABLE[0]);
    if (temperature <= table[0].temperature)
      return table[0].resistance;
    for (size_t index = 1; index < count; index++) {
      if (temperature <= table[index].temperature) {
        const float fraction =
            (temperature - table[index - 1].temperature) /
            (table[index].temperature - table[index - 1].temperature);
        return table[index - 1].resistance +
               fraction *
                   (table[index].resistance -
                    table[index - 1].resistance);
      }
    }
    return table[count - 1].resistance;
  }

  uint8_t temperature_code(float temperature) const {
    const float resistance = this->resistance_for_temperature(temperature);
    const float span =
        this->calibration_.temperature_resistance_max -
        this->calibration_.temperature_resistance_min;
    float normalized = 0.0f;
    if (std::isfinite(span) && span > 0.0f) {
      normalized =
          std::clamp(
              (resistance -
               this->calibration_.temperature_resistance_min) /
                  span,
              0.0f, 1.0f);
    }
    if (this->calibration_.temperature_code_inverted)
      normalized = 1.0f - normalized;
    return static_cast<uint8_t>(std::lround(normalized * 255.0f));
  }

 protected:
  IdmAnalogCalibration calibration_{};
};

}  // namespace esphome::idm_bridge

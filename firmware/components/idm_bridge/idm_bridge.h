#pragma once
#include "esphome.h"
namespace esphome { namespace idm_bridge {
class IdmBridge : public Component {
 public:
  void setup() override {}
  void loop() override {}
  void set_humidity(float value) { humidity_ = clamp(value, 0.0f, 100.0f); }
  void set_temperature(float value) { temperature_ = clamp(value, -20.0f, 60.0f); }
 protected:
  float humidity_{80.0f};
  float temperature_{28.0f};
};
}}
#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include "idm_analog_output_core.h"
#include "idm_calibration_storage_core.h"
#include "idm_bridge.h"

namespace esphome::idm_bridge {

class IdmAnalogOutput : public Component, public IdmBridgeOutput {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_i2c_bus(i2c::I2CBus *bus) {
    this->humidity_device_.set_i2c_bus(bus);
    this->temperature_device_.set_i2c_bus(bus);
  }
  void set_addresses(uint8_t humidity_address, uint8_t temperature_address) {
    this->humidity_device_.set_i2c_address(humidity_address);
    this->temperature_device_.set_i2c_address(temperature_address);
  }
  void set_humidity_code_range(uint16_t minimum, uint16_t maximum) {
    this->factory_calibration_.humidity_code_min = minimum;
    this->factory_calibration_.humidity_code_max = maximum;
    this->calibration_.set_humidity_code_range(minimum, maximum);
  }
  void set_temperature_resistance_range(float minimum, float maximum) {
    this->factory_calibration_.temperature_resistance_min = minimum;
    this->factory_calibration_.temperature_resistance_max = maximum;
    this->calibration_.set_temperature_resistance_range(minimum, maximum);
  }
  void set_temperature_code_inverted(bool inverted) {
    this->factory_calibration_.temperature_code_inverted = inverted;
    this->calibration_.set_temperature_code_inverted(inverted);
  }
  void set_preference_keys(uint32_t current, uint32_t legacy) {
    this->current_preference_key_ = current;
    this->legacy_preference_key_ = legacy;
  }
  void set_unverified_calibration_accepted(bool accepted) {
    this->unverified_calibration_accepted_ = accepted;
  }

  void set_fault_sensor(binary_sensor::BinarySensor *value) {
    this->fault_sensor_ = value;
  }
  void set_error_sensor(text_sensor::TextSensor *value) {
    this->error_sensor_ = value;
  }
  void set_humidity_code_sensor(sensor::Sensor *value) {
    this->humidity_code_sensor_ = value;
  }
  void set_temperature_code_sensor(sensor::Sensor *value) {
    this->temperature_code_sensor_ = value;
  }
  void set_target_resistance_sensor(sensor::Sensor *value) {
    this->target_resistance_sensor_ = value;
  }
  void set_calibration_status_sensor(text_sensor::TextSensor *value) {
    this->calibration_status_sensor_ = value;
  }
  void set_calibration_version_sensor(sensor::Sensor *value) {
    this->calibration_version_sensor_ = value;
  }
  void set_calibration_using_factory_sensor(
      binary_sensor::BinarySensor *value) {
    this->calibration_using_factory_sensor_ = value;
  }

  bool apply(float humidity, float temperature) override;
  bool ready() const override { return this->ready_; }
  bool set_calibration(uint16_t humidity_code_min,
                       uint16_t humidity_code_max,
                       float temperature_resistance_min,
                       float temperature_resistance_max,
                       bool temperature_code_inverted);
  bool reset_calibration();
  const IdmAnalogCalibration &calibration() const {
    return this->calibration_.calibration();
  }

 protected:
  void load_calibration_();
  bool persist_calibration_(const IdmAnalogCalibration &calibration);
  void publish_calibration_status_(const char *status, uint8_t version,
                                   bool using_factory, bool warning);
  void reapply_last_values_();
  bool write_humidity_(uint16_t code);
  bool write_temperature_(uint8_t code);
  void publish_status_(bool fault, const char *error, uint16_t humidity_code,
                       uint8_t temperature_code, float target_resistance);
  void update_component_warning_();

  i2c::I2CDevice humidity_device_;
  i2c::I2CDevice temperature_device_;
  IdmAnalogOutputCore calibration_;
  IdmAnalogCalibration factory_calibration_{};
  ESPPreferenceObject current_preference_;
  ESPPreferenceObject legacy_preference_;
  uint32_t current_preference_key_{0x49444D02};
  uint32_t legacy_preference_key_{0x49444D01};
  binary_sensor::BinarySensor *fault_sensor_{nullptr};
  text_sensor::TextSensor *error_sensor_{nullptr};
  sensor::Sensor *humidity_code_sensor_{nullptr};
  sensor::Sensor *temperature_code_sensor_{nullptr};
  sensor::Sensor *target_resistance_sensor_{nullptr};
  text_sensor::TextSensor *calibration_status_sensor_{nullptr};
  sensor::Sensor *calibration_version_sensor_{nullptr};
  binary_sensor::BinarySensor *calibration_using_factory_sensor_{nullptr};
  bool unverified_calibration_accepted_{false};
  bool ready_{false};
  bool has_last_values_{false};
  float last_humidity_{NAN};
  float last_temperature_{NAN};
  const char *calibration_status_{"not_loaded"};
  uint8_t calibration_version_{0};
  bool calibration_using_factory_{true};
  bool calibration_warning_{false};
  bool output_fault_{true};
  const char *output_error_{"not_initialized"};
  bool status_published_{false};
  bool published_fault_{true};
  uint16_t published_humidity_code_{0};
  uint8_t published_temperature_code_{0};
  float published_target_resistance_{NAN};
  const char *published_error_{"not_initialized"};
};

template<typename... Ts>
class SetCalibrationAction : public Action<Ts...>,
                             public Parented<IdmAnalogOutput> {
 public:
  TEMPLATABLE_VALUE(uint16_t, humidity_code_min)
  TEMPLATABLE_VALUE(uint16_t, humidity_code_max)
  TEMPLATABLE_VALUE(float, temperature_resistance_min)
  TEMPLATABLE_VALUE(float, temperature_resistance_max)
  TEMPLATABLE_VALUE(bool, temperature_code_inverted)

  void play(const Ts &...x) override {
    this->parent_->set_calibration(
        this->humidity_code_min_.value(x...),
        this->humidity_code_max_.value(x...),
        this->temperature_resistance_min_.value(x...),
        this->temperature_resistance_max_.value(x...),
        this->temperature_code_inverted_.value(x...));
  }
};

template<typename... Ts>
class ResetCalibrationAction : public Action<Ts...>,
                               public Parented<IdmAnalogOutput> {
 public:
  void play(const Ts &...x) override {
    this->parent_->reset_calibration();
  }
};

}  // namespace esphome::idm_bridge

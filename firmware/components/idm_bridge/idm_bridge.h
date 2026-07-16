#pragma once

#include <string>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include "idm_bridge_core.h"

namespace esphome::idm_bridge {

class IdmBridgeOutput {
 public:
  virtual ~IdmBridgeOutput() = default;
  virtual bool apply(float humidity, float temperature) = 0;
  virtual bool ready() const = 0;
};

class IdmBridge : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_stale_timeout(uint32_t stale_timeout_ms) {
    this->stale_timeout_ms_ = stale_timeout_ms;
  }
  void set_fallback_values(float humidity, float temperature) {
    this->fallback_humidity_ = humidity;
    this->fallback_temperature_ = temperature;
  }
  void set_humidity_output(output::FloatOutput *value) {
    this->humidity_output_ = value;
  }
  void set_temperature_output(output::FloatOutput *value) {
    this->temperature_output_ = value;
  }
  void set_output_driver(IdmBridgeOutput *value) {
    this->output_driver_ = value;
  }

  void set_effective_humidity_sensor(sensor::Sensor *value) {
    this->effective_humidity_sensor_ = value;
  }
  void set_effective_temperature_sensor(sensor::Sensor *value) {
    this->effective_temperature_sensor_ = value;
  }
  void set_safe_active_sensor(binary_sensor::BinarySensor *value) {
    this->safe_active_sensor_ = value;
  }
  void set_stale_sensor(binary_sensor::BinarySensor *value) {
    this->stale_sensor_ = value;
  }
  void set_fault_sensor(binary_sensor::BinarySensor *value) {
    this->fault_sensor_ = value;
  }
  void set_output_ready_sensor(binary_sensor::BinarySensor *value) {
    this->output_ready_sensor_ = value;
  }
  void set_state_sensor(text_sensor::TextSensor *value) {
    this->state_sensor_ = value;
  }
  void set_error_sensor(text_sensor::TextSensor *value) {
    this->error_sensor_ = value;
  }
  void set_command_source_sensor(text_sensor::TextSensor *value) {
    this->command_source_sensor_ = value;
  }
  void set_command_quality_sensor(sensor::Sensor *value) {
    this->command_quality_sensor_ = value;
  }

  bool set_values(float humidity, float temperature);
  bool set_humidity(float humidity);
  bool set_temperature(float temperature);
  void apply_fallback();
  void set_output_fault(bool active);
  void set_command_metadata(const std::string &source, uint8_t quality);

  const IdmBridgeCore &core() const { return this->core_; }

 protected:
  bool outputs_ready_() const {
    if (this->output_driver_ != nullptr)
      return this->output_driver_->ready();
    return this->humidity_output_ != nullptr &&
           this->temperature_output_ != nullptr;
  }
  void apply_outputs_();
  void publish_diagnostics_(bool force = false);
  void update_component_status_();

  IdmBridgeCore core_;
  uint32_t stale_timeout_ms_{120000};
  float fallback_humidity_{80.0f};
  float fallback_temperature_{28.0f};

  output::FloatOutput *humidity_output_{nullptr};
  output::FloatOutput *temperature_output_{nullptr};
  IdmBridgeOutput *output_driver_{nullptr};
  sensor::Sensor *effective_humidity_sensor_{nullptr};
  sensor::Sensor *effective_temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *safe_active_sensor_{nullptr};
  binary_sensor::BinarySensor *stale_sensor_{nullptr};
  binary_sensor::BinarySensor *fault_sensor_{nullptr};
  binary_sensor::BinarySensor *output_ready_sensor_{nullptr};
  text_sensor::TextSensor *state_sensor_{nullptr};
  text_sensor::TextSensor *error_sensor_{nullptr};
  text_sensor::TextSensor *command_source_sensor_{nullptr};
  sensor::Sensor *command_quality_sensor_{nullptr};

  BridgeState published_state_{BridgeState::ACTIVE};
  BridgeError published_error_{BridgeError::OUTPUT_FAILURE};
  float published_humidity_{NAN};
  float published_temperature_{NAN};
  bool published_output_ready_{false};
  bool diagnostics_published_{false};
  std::string command_source_{"startup"};
  std::string published_command_source_;
  uint8_t command_quality_{0};
  uint8_t published_command_quality_{0};
  uint32_t next_output_retry_ms_{0};
  bool driver_fault_active_{false};
};

template<typename... Ts>
class SetValuesAction : public Action<Ts...>, public Parented<IdmBridge> {
 public:
  TEMPLATABLE_VALUE(float, humidity)
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(std::string, source)
  TEMPLATABLE_VALUE(uint8_t, quality)

  void play(const Ts &...x) override {
    this->parent_->set_command_metadata(this->source_.value(x...),
                                        this->quality_.value(x...));
    this->parent_->set_values(this->humidity_.value(x...),
                              this->temperature_.value(x...));
  }
};

template<typename... Ts>
class ApplyFallbackAction : public Action<Ts...>, public Parented<IdmBridge> {
 public:
  void play(const Ts &...x) override {
    this->parent_->set_command_metadata("manual_fallback", 0);
    this->parent_->apply_fallback();
  }
};

template<typename... Ts>
class SetOutputFaultAction : public Action<Ts...>, public Parented<IdmBridge> {
 public:
  TEMPLATABLE_VALUE(bool, active)

  void play(const Ts &...x) override {
    this->parent_->set_output_fault(this->active_.value(x...));
  }
};

}  // namespace esphome::idm_bridge

// ESPHome's external-component code generator includes the main component
// header. Re-export the nested analog driver type for generated main.cpp.
#include "idm_analog_output.h"

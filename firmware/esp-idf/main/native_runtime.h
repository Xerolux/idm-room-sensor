#pragma once

#include <cstdint>
#include <string>

#include "../../common/dew_point.h"
#include "../../common/value_quality.h"
#include "../../components/idm_bridge/idm_analog_output_core.h"
#include "../../components/idm_bridge/idm_bridge_core.h"

namespace idm::native {

using esphome::idm_bridge::BridgeError;
using esphome::idm_bridge::BridgeState;
using esphome::idm_bridge::IdmAnalogCalibration;
using esphome::idm_bridge::IdmAnalogOutputCore;
using esphome::idm_bridge::IdmBridgeCore;
using esphome::idm_bridge::valid_calibration;

struct NativeRuntimeConfig {
  float fallback_humidity{80.0f};
  float fallback_temperature{28.0f};
  uint32_t stale_timeout_ms{120000};
  uint8_t minimum_command_quality{50};
};

struct NativeOutput {
  uint16_t humidity_dac_code{0};
  uint8_t temperature_digipot_code{0};
  float temperature_resistance_ohm{0.0f};
};

struct NativeDiagnostics {
  const char *bridge_state;
  const char *bridge_error;
  bool safe_active;
  bool stale;
  bool fault;
  float effective_humidity;
  float effective_temperature;
  float dew_point;
  std::string command_source;
  std::string command_status;
  uint8_t command_quality;
  uint32_t command_age_ms;
  bool output_fault;
  uint32_t output_attempts;
  uint32_t output_failures;
  NativeOutput output;
  IdmAnalogCalibration calibration;
};

class NativeRuntime {
 public:
  void configure(const NativeRuntimeConfig &config) {
    this->config_ = config;
    if (this->config_.minimum_command_quality > 100)
      this->config_.minimum_command_quality = 100;
    this->bridge_.configure(
        config.fallback_humidity, config.fallback_temperature,
        config.stale_timeout_ms);
  }

  void boot(uint32_t now_ms, const IdmAnalogCalibration &factory_calibration,
            const IdmAnalogCalibration &loaded_calibration) {
    this->factory_calibration_ =
        valid_calibration(factory_calibration)
            ? factory_calibration
            : IdmAnalogCalibration{};
    const IdmAnalogCalibration active_calibration =
        valid_calibration(loaded_calibration)
            ? loaded_calibration
            : this->factory_calibration_;
    this->output_transform_.set_calibration(active_calibration);
    this->bridge_.configure(
        this->config_.fallback_humidity,
        this->config_.fallback_temperature,
        this->config_.stale_timeout_ms);
    this->bridge_.reset(now_ms);
    this->command_source_ = "startup";
    this->command_status_ = "startup_safe";
    this->command_quality_ = 0;
    this->last_command_ms_ = now_ms;
    this->has_command_attempt_ = false;
    this->output_fault_ = false;
    this->calibration_dirty_ = true;
    this->output_attempts_ = 0;
    this->output_failures_ = 0;
  }

  bool accept_command(float humidity, float temperature, uint8_t quality,
                      const std::string &source, uint32_t now_ms) {
    this->last_command_ms_ = now_ms;
    this->has_command_attempt_ = true;
    this->command_quality_ = quality;
    this->command_source_ =
        source.empty() || source.size() > 64 ? "invalid_source" : source;

    const ClimateValue humidity_value{
        humidity, now_ms, quality, true,
    };
    const ClimateValue temperature_value{
        temperature, now_ms, quality, true,
    };
    const auto humidity_assessment = assess_climate_value(
        humidity_value, now_ms, this->config_.stale_timeout_ms,
        IdmBridgeCore::MIN_HUMIDITY, IdmBridgeCore::MAX_HUMIDITY,
        this->config_.minimum_command_quality);
    const auto temperature_assessment = assess_climate_value(
        temperature_value, now_ms, this->config_.stale_timeout_ms,
        IdmBridgeCore::MIN_TEMPERATURE, IdmBridgeCore::MAX_TEMPERATURE,
        this->config_.minimum_command_quality);

    if (this->command_source_ == "invalid_source") {
      this->command_status_ = "invalid_source";
      this->bridge_.apply_manual_fallback();
      return false;
    }
    if (!humidity_assessment.usable) {
      this->command_status_ =
          value_quality_status_name(humidity_assessment.status);
      this->bridge_.apply_manual_fallback();
      return false;
    }
    if (!temperature_assessment.usable) {
      this->command_status_ =
          value_quality_status_name(temperature_assessment.status);
      this->bridge_.apply_manual_fallback();
      return false;
    }

    const bool accepted =
        this->bridge_.set_values(humidity, temperature, now_ms);
    this->command_status_ =
        accepted ? "accepted"
                 : IdmBridgeCore::error_name(this->bridge_.error());
    return accepted;
  }

  void force_fallback(const std::string &source = "manual_fallback") {
    this->command_source_ =
        source.empty() || source.size() > 64 ? "manual_fallback" : source;
    this->command_status_ = "fallback_requested";
    this->command_quality_ = 0;
    this->bridge_.apply_manual_fallback();
  }

  bool tick(uint32_t now_ms) {
    const bool changed = this->bridge_.tick(now_ms);
    if (changed) {
      this->command_status_ = "stale";
      this->command_quality_ = 0;
    }
    return changed;
  }

  bool apply_calibration(const IdmAnalogCalibration &calibration) {
    if (!this->output_transform_.set_calibration(calibration))
      return false;
    this->calibration_dirty_ = true;
    return true;
  }

  void reset_calibration() {
    this->output_transform_.set_calibration(this->factory_calibration_);
    this->calibration_dirty_ = true;
  }

  const IdmAnalogCalibration &calibration() const {
    return this->output_transform_.calibration();
  }

  const IdmAnalogCalibration &factory_calibration() const {
    return this->factory_calibration_;
  }

  bool output_dirty() const {
    return this->bridge_.output_dirty() || this->calibration_dirty_;
  }

  NativeOutput desired_output() const {
    const auto &values = this->bridge_.values();
    return {
        this->output_transform_.humidity_code(values.humidity),
        this->output_transform_.temperature_code(values.temperature),
        this->output_transform_.resistance_for_temperature(
            values.temperature),
    };
  }

  void record_output_result(bool success) {
    this->output_attempts_++;
    if (!success) {
      this->output_failures_++;
      this->output_fault_ = true;
      this->bridge_.set_output_fault(true);
      return;
    }

    if (this->output_fault_) {
      this->output_fault_ = false;
      this->bridge_.set_output_fault(false);
      return;
    }

    this->bridge_.clear_output_dirty();
    this->calibration_dirty_ = false;
  }

  NativeDiagnostics diagnostics(uint32_t now_ms) const {
    const auto &values = this->bridge_.values();
    return {
        IdmBridgeCore::state_name(this->bridge_.state()),
        IdmBridgeCore::error_name(this->bridge_.error()),
        this->bridge_.safe_active(),
        this->bridge_.stale(),
        this->bridge_.fault(),
        values.humidity,
        values.temperature,
        dew_point_c(values.temperature, values.humidity),
        this->command_source_,
        this->command_status_,
        this->command_quality_,
        this->has_command_attempt_
            ? static_cast<uint32_t>(now_ms - this->last_command_ms_)
            : 0,
        this->output_fault_,
        this->output_attempts_,
        this->output_failures_,
        this->desired_output(),
        this->output_transform_.calibration(),
    };
  }

  BridgeState bridge_state() const {
    return this->bridge_.state();
  }

  BridgeError bridge_error() const {
    return this->bridge_.error();
  }

 private:
  NativeRuntimeConfig config_{};
  IdmBridgeCore bridge_{};
  IdmAnalogOutputCore output_transform_{};
  IdmAnalogCalibration factory_calibration_{};
  std::string command_source_{"startup"};
  std::string command_status_{"not_started"};
  uint8_t command_quality_{0};
  uint32_t last_command_ms_{0};
  bool has_command_attempt_{false};
  bool output_fault_{false};
  bool calibration_dirty_{true};
  uint32_t output_attempts_{0};
  uint32_t output_failures_{0};
};

}  // namespace idm::native

// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Xerolux

#pragma once

#include <cmath>
#include <cstdint>

namespace esphome::idm_bridge {

enum class BridgeState : uint8_t {
  STARTUP_SAFE = 0,
  ACTIVE,
  MANUAL_SAFE,
  STALE_SAFE,
  INVALID_SAFE,
  OUTPUT_FAULT_SAFE,
};

enum class BridgeError : uint8_t {
  NONE = 0,
  STALE_INPUT,
  INVALID_HUMIDITY,
  INVALID_TEMPERATURE,
  INVALID_VALUES,
  INVALID_QUALITY,
  OUTPUT_FAILURE,
};

struct BridgeValues {
  float humidity;
  float temperature;
};

class IdmBridgeCore {
 public:
  static constexpr float MIN_HUMIDITY = 0.0f;
  static constexpr float MAX_HUMIDITY = 100.0f;
  static constexpr float MIN_TEMPERATURE = -20.0f;
  static constexpr float MAX_TEMPERATURE = 60.0f;

  void configure(float fallback_humidity, float fallback_temperature,
                 uint32_t stale_timeout_ms) {
    if (valid_humidity(fallback_humidity))
      this->fallback_.humidity = fallback_humidity;
    if (valid_temperature(fallback_temperature))
      this->fallback_.temperature = fallback_temperature;
    this->stale_timeout_ms_ = stale_timeout_ms;
  }

  // Optional command-quality gate mirroring the native runtime. Default 0
  // preserves the historical ESPHome behaviour (accept any in-range command
  // regardless of reported quality) so existing deployments are unaffected;
  // raising it lets an operator reject low-confidence sources the same way
  // the native firmware does. Clamped to 0..100 because quality is uint8_t.
  void set_minimum_command_quality(uint8_t minimum) {
    minimum_command_quality_ =
        minimum > 100 ? 100 : minimum;
  }

  void reset(uint32_t now_ms) {
    this->effective_ = this->fallback_;
    this->last_command_ms_ = now_ms;
    this->has_command_ = false;
    this->output_fault_latched_ = false;
    this->state_ = BridgeState::STARTUP_SAFE;
    this->error_ = BridgeError::NONE;
    this->output_dirty_ = true;
  }

  bool set_values(float humidity, float temperature, uint32_t now_ms) {
    return set_values(humidity, temperature, 100, now_ms);
  }

  bool set_values(float humidity, float temperature,
                  uint8_t quality, uint32_t now_ms) {
    if (this->output_fault_latched_) {
      this->apply_safe_(BridgeState::OUTPUT_FAULT_SAFE,
                        BridgeError::OUTPUT_FAILURE);
      return false;
    }

    const bool humidity_valid = valid_humidity(humidity);
    const bool temperature_valid = valid_temperature(temperature);
    if (!humidity_valid || !temperature_valid) {
      BridgeError error = BridgeError::INVALID_VALUES;
      if (!humidity_valid && temperature_valid)
        error = BridgeError::INVALID_HUMIDITY;
      if (humidity_valid && !temperature_valid)
        error = BridgeError::INVALID_TEMPERATURE;
      this->apply_safe_(BridgeState::INVALID_SAFE, error);
      return false;
    }

    // Quality gate. quality > 100 is treated as invalid (mirrors
    // value_quality.h). When the gate is 0 (default), every in-range command
    // is accepted — historical ESPHome behaviour, documented divergence from
    // the native firmware's default of 50.
    if (quality > 100 || quality < minimum_command_quality_) {
      this->apply_safe_(BridgeState::INVALID_SAFE,
                        BridgeError::INVALID_QUALITY);
      return false;
    }

    this->effective_ = {humidity, temperature};
    this->last_command_ms_ = now_ms;
    this->has_command_ = true;
    this->state_ = BridgeState::ACTIVE;
    this->error_ = BridgeError::NONE;
    this->output_dirty_ = true;
    return true;
  }

  bool set_humidity(float humidity, uint32_t now_ms) {
    return this->set_values(humidity, this->effective_.temperature, now_ms);
  }

  bool set_temperature(float temperature, uint32_t now_ms) {
    return this->set_values(this->effective_.humidity, temperature, now_ms);
  }

  bool tick(uint32_t now_ms) {
    if (this->state_ != BridgeState::ACTIVE || !this->has_command_ ||
        this->stale_timeout_ms_ == 0)
      return false;
    if (static_cast<uint32_t>(now_ms - this->last_command_ms_) <
        this->stale_timeout_ms_)
      return false;
    this->apply_safe_(BridgeState::STALE_SAFE, BridgeError::STALE_INPUT);
    return true;
  }

  void apply_manual_fallback() {
    if (this->output_fault_latched_) {
      this->apply_safe_(BridgeState::OUTPUT_FAULT_SAFE,
                        BridgeError::OUTPUT_FAILURE);
      return;
    }
    this->apply_safe_(BridgeState::MANUAL_SAFE, BridgeError::NONE);
  }

  void set_output_fault(bool active) {
    if (active) {
      this->output_fault_latched_ = true;
      this->apply_safe_(BridgeState::OUTPUT_FAULT_SAFE,
                        BridgeError::OUTPUT_FAILURE);
      return;
    }
    this->output_fault_latched_ = false;
    if (this->state_ == BridgeState::OUTPUT_FAULT_SAFE)
      this->apply_safe_(BridgeState::STARTUP_SAFE, BridgeError::NONE);
  }

  static bool valid_humidity(float value) {
    return std::isfinite(value) && value >= MIN_HUMIDITY &&
           value <= MAX_HUMIDITY;
  }

  static bool valid_temperature(float value) {
    return std::isfinite(value) && value >= MIN_TEMPERATURE &&
           value <= MAX_TEMPERATURE;
  }

  const BridgeValues &values() const { return this->effective_; }
  const BridgeValues &fallback_values() const { return this->fallback_; }
  BridgeState state() const { return this->state_; }
  BridgeError error() const { return this->error_; }
  bool has_command() const { return this->has_command_; }
  bool output_dirty() const { return this->output_dirty_; }
  void clear_output_dirty() { this->output_dirty_ = false; }

  bool safe_active() const { return this->state_ != BridgeState::ACTIVE; }
  bool stale() const { return this->state_ == BridgeState::STALE_SAFE; }
  bool fault() const {
    return this->state_ == BridgeState::INVALID_SAFE ||
           this->state_ == BridgeState::OUTPUT_FAULT_SAFE;
  }

  float normalized_humidity() const {
    return this->effective_.humidity / MAX_HUMIDITY;
  }

  float normalized_temperature() const {
    return (this->effective_.temperature - MIN_TEMPERATURE) /
           (MAX_TEMPERATURE - MIN_TEMPERATURE);
  }

  static const char *state_name(BridgeState state) {
    switch (state) {
      case BridgeState::STARTUP_SAFE:
        return "startup_safe";
      case BridgeState::ACTIVE:
        return "active";
      case BridgeState::MANUAL_SAFE:
        return "manual_safe";
      case BridgeState::STALE_SAFE:
        return "stale_safe";
      case BridgeState::INVALID_SAFE:
        return "invalid_safe";
      case BridgeState::OUTPUT_FAULT_SAFE:
        return "output_fault_safe";
    }
    return "unknown";
  }

  static const char *error_name(BridgeError error) {
    switch (error) {
      case BridgeError::NONE:
        return "none";
      case BridgeError::STALE_INPUT:
        return "stale_input";
      case BridgeError::INVALID_HUMIDITY:
        return "invalid_humidity";
      case BridgeError::INVALID_TEMPERATURE:
        return "invalid_temperature";
      case BridgeError::INVALID_VALUES:
        return "invalid_values";
      case BridgeError::INVALID_QUALITY:
        return "invalid_quality";
      case BridgeError::OUTPUT_FAILURE:
        return "output_failure";
    }
    return "unknown";
  }

 protected:
  void apply_safe_(BridgeState state, BridgeError error) {
    this->effective_ = this->fallback_;
    this->has_command_ = false;
    this->state_ = state;
    this->error_ = error;
    this->output_dirty_ = true;
  }

  BridgeValues fallback_{80.0f, 28.0f};
  BridgeValues effective_{80.0f, 28.0f};
  uint32_t stale_timeout_ms_{120000};
  uint32_t last_command_ms_{0};
  BridgeState state_{BridgeState::STARTUP_SAFE};
  BridgeError error_{BridgeError::NONE};
  bool has_command_{false};
  bool output_fault_latched_{false};
  bool output_dirty_{true};
  uint8_t minimum_command_quality_{0};
};

}  // namespace esphome::idm_bridge

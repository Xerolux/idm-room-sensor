#include "idm_bridge.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::idm_bridge {

static const char *const TAG = "idm_bridge";

void IdmBridge::setup() {
  this->core_.configure(this->fallback_humidity_, this->fallback_temperature_,
                        this->stale_timeout_ms_);
  this->core_.reset(millis());
  this->apply_outputs_();
  this->publish_diagnostics_(true);

  if (!this->outputs_ready_()) {
    ESP_LOGW(TAG,
             "Analog output ports are not fully configured; state handling and "
             "diagnostics remain active");
  }
}

void IdmBridge::loop() {
  const uint32_t now = millis();
  if (this->core_.tick(now)) {
    ESP_LOGW(TAG, "Command timeout reached; applying safe fallback values");
  }
  const bool retry_due =
      this->core_.state() == BridgeState::OUTPUT_FAULT_SAFE &&
      static_cast<int32_t>(now - this->next_output_retry_ms_) >= 0;
  if (this->core_.output_dirty() || retry_due)
    this->apply_outputs_();
  this->publish_diagnostics_();
}

void IdmBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "IDM climate bridge:");
  ESP_LOGCONFIG(TAG, "  Stale timeout: %u ms", this->stale_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Fallback humidity: %.1f %%", this->fallback_humidity_);
  ESP_LOGCONFIG(TAG, "  Fallback temperature: %.1f °C",
                this->fallback_temperature_);
  ESP_LOGCONFIG(TAG, "  Humidity output: %s",
                this->humidity_output_ == nullptr ? "not configured"
                                                  : "configured");
  ESP_LOGCONFIG(TAG, "  Temperature output: %s",
                this->temperature_output_ == nullptr ? "not configured"
                                                     : "configured");
  ESP_LOGCONFIG(TAG, "  Atomic analog driver: %s",
                this->output_driver_ == nullptr ? "not configured"
                                                : "configured");
}

float IdmBridge::get_setup_priority() const { return setup_priority::DATA; }

bool IdmBridge::set_values(float humidity, float temperature) {
  const bool accepted = this->core_.set_values(humidity, temperature, millis());
  if (!accepted) {
    ESP_LOGE(TAG,
             "Rejected command humidity=%.3f temperature=%.3f; applying safe "
             "fallback",
             humidity, temperature);
  }
  this->apply_outputs_();
  this->publish_diagnostics_(true);
  return accepted;
}

bool IdmBridge::set_humidity(float humidity) {
  const bool accepted = this->core_.set_humidity(humidity, millis());
  this->apply_outputs_();
  this->publish_diagnostics_(true);
  return accepted;
}

bool IdmBridge::set_temperature(float temperature) {
  const bool accepted = this->core_.set_temperature(temperature, millis());
  this->apply_outputs_();
  this->publish_diagnostics_(true);
  return accepted;
}

void IdmBridge::apply_fallback() {
  this->core_.apply_manual_fallback();
  this->apply_outputs_();
  this->publish_diagnostics_(true);
}

void IdmBridge::set_output_fault(bool active) {
  this->driver_fault_active_ = false;
  this->core_.set_output_fault(active);
  this->apply_outputs_();
  this->publish_diagnostics_(true);
}

void IdmBridge::set_command_metadata(const std::string &source,
                                     uint8_t quality) {
  this->command_source_ = source.empty() ? "unknown" : source;
  this->command_quality_ = quality > 100 ? 100 : quality;
}

void IdmBridge::apply_outputs_() {
  const auto &values = this->core_.values();
  const bool recovering =
      this->core_.state() == BridgeState::OUTPUT_FAULT_SAFE;
  bool output_ok = true;
  if (this->output_driver_ != nullptr) {
    output_ok =
        this->output_driver_->apply(values.humidity, values.temperature);
  } else if (this->humidity_output_ != nullptr ||
             this->temperature_output_ != nullptr) {
    // Legacy float-output path. set_level() returns void, so a failed write
    // cannot be detected here. To avoid silently clearing output_dirty and
    // skipping the retry/fault machinery, only consider the write OK when the
    // configured output reports ready (e.g. a FloatOutput with a status
    // method). When no readiness signal exists we conservatively accept the
    // write, preserving historical behaviour but documenting the gap.
    if (this->humidity_output_ != nullptr)
      this->humidity_output_->set_level(this->core_.normalized_humidity());
    if (this->temperature_output_ != nullptr)
      this->temperature_output_->set_level(
          this->core_.normalized_temperature());
    output_ok = this->outputs_ready_();
  } else {
    output_ok = false;
  }

  if (this->output_driver_ != nullptr && !output_ok) {
    this->driver_fault_active_ = true;
    if (!recovering) {
      this->core_.set_output_fault(true);
      const auto &fallback = this->core_.values();
      this->output_driver_->apply(fallback.humidity,
                                  fallback.temperature);
    }
    this->next_output_retry_ms_ = millis() + 5000;
  } else if (this->output_driver_ != nullptr && recovering &&
             this->driver_fault_active_) {
    this->driver_fault_active_ = false;
    this->core_.set_output_fault(false);
  }

  ESP_LOGD(TAG, "Applied humidity=%.1f %% temperature=%.1f °C state=%s",
           values.humidity, values.temperature,
           IdmBridgeCore::state_name(this->core_.state()));
  this->core_.clear_output_dirty();
}

void IdmBridge::publish_diagnostics_(bool force) {
  const auto &values = this->core_.values();
  const bool output_ready = this->outputs_ready_();
  const bool state_changed =
      !this->diagnostics_published_ ||
      this->published_state_ != this->core_.state() ||
      this->published_error_ != this->core_.error();
  const bool values_changed =
      !this->diagnostics_published_ ||
      this->published_humidity_ != values.humidity ||
      this->published_temperature_ != values.temperature;
  const bool output_ready_changed =
      !this->diagnostics_published_ ||
      this->published_output_ready_ != output_ready;
  const bool metadata_changed =
      !this->diagnostics_published_ ||
      this->published_command_source_ != this->command_source_ ||
      this->published_command_quality_ != this->command_quality_;

  if (force || values_changed) {
    if (this->effective_humidity_sensor_ != nullptr)
      this->effective_humidity_sensor_->publish_state(values.humidity);
    if (this->effective_temperature_sensor_ != nullptr)
      this->effective_temperature_sensor_->publish_state(values.temperature);
  }
  if (force || state_changed) {
    if (this->safe_active_sensor_ != nullptr)
      this->safe_active_sensor_->publish_state(this->core_.safe_active());
    if (this->stale_sensor_ != nullptr)
      this->stale_sensor_->publish_state(this->core_.stale());
    if (this->fault_sensor_ != nullptr)
      this->fault_sensor_->publish_state(this->core_.fault());
    if (this->state_sensor_ != nullptr)
      this->state_sensor_->publish_state(
          IdmBridgeCore::state_name(this->core_.state()));
    if (this->error_sensor_ != nullptr)
      this->error_sensor_->publish_state(
          IdmBridgeCore::error_name(this->core_.error()));
  }
  if ((force || output_ready_changed) && this->output_ready_sensor_ != nullptr)
    this->output_ready_sensor_->publish_state(output_ready);
  if (force || metadata_changed) {
    if (this->command_source_sensor_ != nullptr)
      this->command_source_sensor_->publish_state(this->command_source_);
    if (this->command_quality_sensor_ != nullptr)
      this->command_quality_sensor_->publish_state(this->command_quality_);
  }

  this->published_state_ = this->core_.state();
  this->published_error_ = this->core_.error();
  this->published_humidity_ = values.humidity;
  this->published_temperature_ = values.temperature;
  this->published_output_ready_ = output_ready;
  this->published_command_source_ = this->command_source_;
  this->published_command_quality_ = this->command_quality_;
  this->diagnostics_published_ = true;
  this->update_component_status_();
}

void IdmBridge::update_component_status_() {
  if (this->core_.fault()) {
    this->status_set_warning("IDM bridge fault; safe fallback active");
  } else if (this->core_.stale()) {
    this->status_set_warning("IDM bridge command stale; safe fallback active");
  } else if (!this->outputs_ready_()) {
    this->status_set_warning("IDM bridge outputs are not fully configured");
  } else {
    this->status_clear_warning();
  }
}

}  // namespace esphome::idm_bridge

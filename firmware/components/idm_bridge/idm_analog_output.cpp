#include "idm_analog_output.h"

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::idm_bridge {

static const char *const TAG = "idm_bridge.output";

void IdmAnalogOutput::setup() {
  this->load_calibration_();
  const bool humidity_present =
      this->humidity_device_.write(nullptr, 0) == i2c::ERROR_OK;
  const bool temperature_present =
      this->temperature_device_.write(nullptr, 0) == i2c::ERROR_OK;
  this->ready_ = humidity_present && temperature_present &&
                 this->unverified_calibration_accepted_;

  const char *error = "none";
  if (!this->unverified_calibration_accepted_)
    error = "unverified_kty_calibration_not_accepted";
  else if (!humidity_present && !temperature_present)
    error = "humidity_and_temperature_devices_missing";
  else if (!humidity_present)
    error = "humidity_dac_missing";
  else if (!temperature_present)
    error = "temperature_digipot_missing";
  this->publish_status_(!this->ready_, error, 0, 0, NAN);
}

void IdmAnalogOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "IDM analog output:");
  ESP_LOGCONFIG(TAG, "  Humidity DAC address: 0x%02X",
                this->humidity_device_.get_i2c_address());
  ESP_LOGCONFIG(TAG, "  Temperature digipot address: 0x%02X",
                this->temperature_device_.get_i2c_address());
  const auto &calibration = this->calibration_.calibration();
  ESP_LOGCONFIG(TAG, "  Calibration status: %s (version %u)",
                this->calibration_status_, this->calibration_version_);
  ESP_LOGCONFIG(TAG, "  Humidity DAC range: %u..%u",
                calibration.humidity_code_min,
                calibration.humidity_code_max);
  ESP_LOGCONFIG(TAG, "  Temperature resistance range: %.1f..%.1f Ω",
                calibration.temperature_resistance_min,
                calibration.temperature_resistance_max);
  ESP_LOGCONFIG(TAG, "  Temperature code inverted: %s",
                calibration.temperature_code_inverted ? "yes" : "no");
  ESP_LOGW(TAG,
           "KTY calibration is a prototype approximation and must be verified "
           "against the real IDM input before connection");
}

float IdmAnalogOutput::get_setup_priority() const {
  return setup_priority::HARDWARE;
}

bool IdmAnalogOutput::apply(float humidity, float temperature) {
  if (!IdmBridgeCore::valid_humidity(humidity) ||
      !IdmBridgeCore::valid_temperature(temperature)) {
    this->ready_ = false;
    this->publish_status_(true, "invalid_bridge_values", 0, 0, NAN);
    return false;
  }
  if (!this->unverified_calibration_accepted_) {
    this->ready_ = false;
    this->publish_status_(
        true, "unverified_kty_calibration_not_accepted", 0, 0, NAN);
    return false;
  }

  this->last_humidity_ = humidity;
  this->last_temperature_ = temperature;
  this->has_last_values_ = true;
  const uint16_t humidity_code =
      this->calibration_.humidity_code(humidity);
  const float target_resistance =
      this->calibration_.resistance_for_temperature(temperature);
  const uint8_t temperature_code =
      this->calibration_.temperature_code(temperature);

  const bool humidity_ok = this->write_humidity_(humidity_code);
  const bool temperature_ok =
      this->write_temperature_(temperature_code);
  this->ready_ = humidity_ok && temperature_ok;

  const char *error = "none";
  if (!humidity_ok && !temperature_ok)
    error = "humidity_and_temperature_write_failed";
  else if (!humidity_ok)
    error = "humidity_dac_write_failed";
  else if (!temperature_ok)
    error = "temperature_digipot_write_failed";

  this->publish_status_(!this->ready_, error, humidity_code,
                        temperature_code, target_resistance);
  return this->ready_;
}

bool IdmAnalogOutput::set_calibration(
    uint16_t humidity_code_min, uint16_t humidity_code_max,
    float temperature_resistance_min, float temperature_resistance_max,
    bool temperature_code_inverted) {
  const IdmAnalogCalibration requested{
      humidity_code_min,
      humidity_code_max,
      temperature_resistance_min,
      temperature_resistance_max,
      temperature_code_inverted,
  };
  if (!valid_calibration(requested)) {
    ESP_LOGE(TAG, "Rejected invalid calibration update");
    this->publish_calibration_status_(
        "invalid_update_rejected", this->calibration_version_,
        this->calibration_using_factory_, true);
    return false;
  }
  if (!this->persist_calibration_(requested)) {
    ESP_LOGE(TAG, "Failed to persist calibration update");
    this->publish_calibration_status_(
        "save_failed", this->calibration_version_,
        this->calibration_using_factory_, true);
    return false;
  }

  this->calibration_.set_calibration(requested);
  this->publish_calibration_status_(
      "stored_v2", CalibrationStorageCore::CURRENT_VERSION,
      calibration_equal(requested, this->factory_calibration_), false);
  this->reapply_last_values_();
  return true;
}

bool IdmAnalogOutput::reset_calibration() {
  if (!this->persist_calibration_(this->factory_calibration_)) {
    ESP_LOGE(TAG, "Failed to persist factory calibration reset");
    this->publish_calibration_status_(
        "reset_save_failed", this->calibration_version_,
        this->calibration_using_factory_, true);
    return false;
  }

  this->calibration_.set_calibration(this->factory_calibration_);
  this->publish_calibration_status_(
      "reset_to_factory", CalibrationStorageCore::CURRENT_VERSION, true,
      false);
  this->reapply_last_values_();
  return true;
}

void IdmAnalogOutput::load_calibration_() {
  if (!valid_calibration(this->factory_calibration_)) {
    ESP_LOGE(TAG,
             "Configured factory calibration is invalid; using built-in safe "
             "defaults");
    this->factory_calibration_ = IdmAnalogCalibration{};
  }
  this->current_preference_ =
      global_preferences->make_preference<CalibrationRecordV2>(
          this->current_preference_key_, true);
  this->legacy_preference_ =
      global_preferences->make_preference<CalibrationRecordV1>(
          this->legacy_preference_key_, true);

  CalibrationRecordV2 current{};
  const bool has_current = this->current_preference_.load(&current);
  CalibrationRecordV1 legacy{};
  const bool has_legacy =
      !has_current && this->legacy_preference_.load(&legacy);
  const auto loaded = CalibrationStorageCore::load(
      this->factory_calibration_, has_current, current, has_legacy, legacy);
  this->calibration_.set_calibration(loaded.calibration);

  bool warning =
      loaded.status == CalibrationLoadStatus::INVALID_USING_FACTORY;
  uint8_t version = 0;
  bool using_factory =
      loaded.status == CalibrationLoadStatus::FACTORY_DEFAULTS ||
      loaded.status == CalibrationLoadStatus::INVALID_USING_FACTORY;
  const char *status = CalibrationStorageCore::status_name(loaded.status);

  if (loaded.status == CalibrationLoadStatus::STORED_V2) {
    version = CalibrationStorageCore::CURRENT_VERSION;
    using_factory =
        calibration_equal(loaded.calibration, this->factory_calibration_);
  } else if (loaded.status == CalibrationLoadStatus::MIGRATED_V1) {
    version = CalibrationStorageCore::LEGACY_VERSION;
    using_factory =
        calibration_equal(loaded.calibration, this->factory_calibration_);
    if (loaded.migration_required) {
      if (this->persist_calibration_(loaded.calibration)) {
        version = CalibrationStorageCore::CURRENT_VERSION;
      } else {
        status = "migration_save_failed";
        warning = true;
      }
    }
  }
  this->publish_calibration_status_(status, version, using_factory, warning);
}

bool IdmAnalogOutput::persist_calibration_(
    const IdmAnalogCalibration &calibration) {
  if (!valid_calibration(calibration))
    return false;
  const CalibrationRecordV2 record =
      CalibrationStorageCore::make_record(calibration);
  if (!this->current_preference_.save(&record))
    return false;
  global_preferences->sync();

  CalibrationRecordV2 persisted{};
  IdmAnalogCalibration verified{};
  return this->current_preference_.load(&persisted) &&
         CalibrationStorageCore::decode_record(persisted, &verified) &&
         calibration_equal(verified, calibration);
}

void IdmAnalogOutput::publish_calibration_status_(
    const char *status, uint8_t version, bool using_factory, bool warning) {
  this->calibration_status_ = status;
  this->calibration_version_ = version;
  this->calibration_using_factory_ = using_factory;
  this->calibration_warning_ = warning;
  if (this->calibration_status_sensor_ != nullptr)
    this->calibration_status_sensor_->publish_state(status);
  if (this->calibration_version_sensor_ != nullptr)
    this->calibration_version_sensor_->publish_state(version);
  if (this->calibration_using_factory_sensor_ != nullptr)
    this->calibration_using_factory_sensor_->publish_state(using_factory);
  this->update_component_warning_();
}

void IdmAnalogOutput::reapply_last_values_() {
  if (this->has_last_values_)
    this->apply(this->last_humidity_, this->last_temperature_);
}

bool IdmAnalogOutput::write_humidity_(uint16_t code) {
  code &= 0x0FFF;
  const uint8_t data[] = {
      0x40,
      static_cast<uint8_t>(code >> 4),
      static_cast<uint8_t>((code & 0x0F) << 4),
  };
  return this->humidity_device_.write(data, sizeof(data)) == i2c::ERROR_OK;
}

bool IdmAnalogOutput::write_temperature_(uint8_t code) {
  // AD5242 instruction byte: A/B=0 selects RDAC1; reset, shutdown and
  // auxiliary outputs remain inactive.
  const uint8_t data[] = {0x00, code};
  return this->temperature_device_.write(data, sizeof(data)) ==
         i2c::ERROR_OK;
}

void IdmAnalogOutput::publish_status_(
    bool fault, const char *error, uint16_t humidity_code,
    uint8_t temperature_code, float target_resistance) {
  const bool error_changed =
      !this->status_published_ ||
      std::strcmp(this->published_error_, error) != 0;
  const bool values_changed =
      !this->status_published_ ||
      this->published_humidity_code_ != humidity_code ||
      this->published_temperature_code_ != temperature_code ||
      this->published_target_resistance_ != target_resistance;

  if ((!this->status_published_ || this->published_fault_ != fault) &&
      this->fault_sensor_ != nullptr)
    this->fault_sensor_->publish_state(fault);
  if (error_changed && this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(error);
  if (values_changed) {
    if (this->humidity_code_sensor_ != nullptr)
      this->humidity_code_sensor_->publish_state(humidity_code);
    if (this->temperature_code_sensor_ != nullptr)
      this->temperature_code_sensor_->publish_state(temperature_code);
    if (this->target_resistance_sensor_ != nullptr &&
        std::isfinite(target_resistance))
      this->target_resistance_sensor_->publish_state(target_resistance);
  }

  if (fault) {
    ESP_LOGE(TAG, "Analog output fault: %s", error);
  }

  this->output_fault_ = fault;
  this->output_error_ = error;
  this->update_component_warning_();
  this->published_fault_ = fault;
  this->published_error_ = error;
  this->published_humidity_code_ = humidity_code;
  this->published_temperature_code_ = temperature_code;
  this->published_target_resistance_ = target_resistance;
  this->status_published_ = true;
}

void IdmAnalogOutput::update_component_warning_() {
  if (this->output_fault_) {
    this->status_set_warning(this->output_error_);
  } else if (this->calibration_warning_) {
    this->status_set_warning(this->calibration_status_);
  } else {
    this->status_clear_warning();
  }
}

}  // namespace esphome::idm_bridge

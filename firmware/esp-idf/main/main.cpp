#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cJSON.h"
#include "driver/i2c_master.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "idm_calibration_storage_core.h"
#include "native_runtime.h"

namespace {

using esphome::idm_bridge::CalibrationLoadStatus;
using esphome::idm_bridge::CalibrationRecordV1;
using esphome::idm_bridge::CalibrationRecordV2;
using esphome::idm_bridge::CalibrationStorageCore;
using esphome::idm_bridge::IdmAnalogCalibration;
using esphome::idm_bridge::calibration_equal;
using esphome::idm_bridge::valid_calibration;
using idm::native::NativeDiagnostics;
using idm::native::NativeOutput;
using idm::native::NativeRuntime;
using idm::native::NativeRuntimeConfig;

constexpr char TAG[] = "idm_native";
constexpr char NVS_NAMESPACE[] = "idm_native";
constexpr char NVS_CALIBRATION_V2_KEY[] = "cal_v2";
constexpr char NVS_CALIBRATION_V1_KEY[] = "cal_v1";
constexpr size_t HTTP_BODY_LIMIT = 768;
constexpr uint32_t OUTPUT_RETRY_MS = 1000;
constexpr uint32_t I2C_RETRY_MS = 5000;
#ifdef CONFIG_IDM_FACTORY_TEMPERATURE_CODE_INVERTED
constexpr bool FACTORY_TEMPERATURE_CODE_INVERTED = true;
#else
constexpr bool FACTORY_TEMPERATURE_CODE_INVERTED = false;
#endif

struct OutputHardware {
  i2c_master_bus_handle_t bus{nullptr};
  i2c_master_dev_handle_t humidity{nullptr};
  i2c_master_dev_handle_t temperature{nullptr};
  bool ready{false};
};

NativeRuntime s_runtime;
SemaphoreHandle_t s_runtime_mutex{nullptr};
OutputHardware s_output_hardware;
std::atomic<bool> s_wifi_connected{false};
std::atomic<bool> s_ota_running{false};
httpd_handle_t s_http_server{nullptr};
bool s_nvs_ready{false};
char s_output_error[64]{"not_initialized"};
char s_ota_status[32]{"idle"};
uint32_t s_next_output_attempt_ms{0};
uint32_t s_next_i2c_attempt_ms{0};

class RuntimeGuard {
 public:
  RuntimeGuard() {
    if (s_runtime_mutex != nullptr)
      xSemaphoreTake(s_runtime_mutex, portMAX_DELAY);
  }

  ~RuntimeGuard() {
    if (s_runtime_mutex != nullptr)
      xSemaphoreGive(s_runtime_mutex);
  }

  RuntimeGuard(const RuntimeGuard &) = delete;
  RuntimeGuard &operator=(const RuntimeGuard &) = delete;
};

uint32_t now_ms() {
  return static_cast<uint32_t>(
      static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL);
}

float configured_float(const char *value, float fallback) {
  if (value == nullptr)
    return fallback;
  char *end = nullptr;
  const float parsed = std::strtof(value, &end);
  if (end == value || *end != '\0' || !std::isfinite(parsed))
    return fallback;
  return parsed;
}

IdmAnalogCalibration factory_calibration() {
  return {
      static_cast<uint16_t>(CONFIG_IDM_FACTORY_HUMIDITY_CODE_MIN),
      static_cast<uint16_t>(CONFIG_IDM_FACTORY_HUMIDITY_CODE_MAX),
      configured_float(
          CONFIG_IDM_FACTORY_TEMPERATURE_RESISTANCE_MIN, 650.0f),
      configured_float(
          CONFIG_IDM_FACTORY_TEMPERATURE_RESISTANCE_MAX, 3000.0f),
      FACTORY_TEMPERATURE_CODE_INVERTED,
  };
}

void copy_status(char *destination, size_t size, const char *value) {
  if (destination == nullptr || size == 0)
    return;
  std::snprintf(destination, size, "%s", value != nullptr ? value : "unknown");
}

void set_output_error(const char *value) {
  RuntimeGuard guard;
  copy_status(s_output_error, sizeof(s_output_error), value);
}

void set_ota_status(const char *value) {
  RuntimeGuard guard;
  copy_status(s_ota_status, sizeof(s_ota_status), value);
}

bool initialize_nvs() {
  esp_err_t result = nvs_flash_init();
  if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
      result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS requires recovery; erasing partition");
    result = nvs_flash_erase();
    if (result == ESP_OK)
      result = nvs_flash_init();
  }
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(result));
    return false;
  }
  return true;
}

template<typename T>
bool read_nvs_record(nvs_handle_t handle, const char *key, T *value) {
  size_t size = sizeof(T);
  return nvs_get_blob(handle, key, value, &size) == ESP_OK &&
         size == sizeof(T);
}

bool persist_calibration(const IdmAnalogCalibration &calibration) {
  if (!s_nvs_ready || !valid_calibration(calibration))
    return false;

  nvs_handle_t handle;
  esp_err_t result =
      nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (result != ESP_OK)
    return false;

  const CalibrationRecordV2 record =
      CalibrationStorageCore::make_record(calibration);
  result = nvs_set_blob(
      handle, NVS_CALIBRATION_V2_KEY, &record, sizeof(record));
  if (result == ESP_OK)
    result = nvs_commit(handle);

  CalibrationRecordV2 read_back{};
  size_t read_back_size = sizeof(read_back);
  if (result == ESP_OK) {
    result = nvs_get_blob(
        handle, NVS_CALIBRATION_V2_KEY, &read_back, &read_back_size);
  }
  nvs_close(handle);

  IdmAnalogCalibration decoded{};
  return result == ESP_OK && read_back_size == sizeof(read_back) &&
         CalibrationStorageCore::decode_record(read_back, &decoded) &&
         calibration_equal(decoded, calibration);
}

IdmAnalogCalibration load_calibration(
    const IdmAnalogCalibration &factory) {
  if (!s_nvs_ready)
    return factory;

  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
    return factory;

  CalibrationRecordV2 current{};
  CalibrationRecordV1 legacy{};
  const bool has_current = read_nvs_record(
      handle, NVS_CALIBRATION_V2_KEY, &current);
  const bool has_legacy =
      !has_current &&
      read_nvs_record(handle, NVS_CALIBRATION_V1_KEY, &legacy);
  nvs_close(handle);

  const auto loaded = CalibrationStorageCore::load(
      factory, has_current, current, has_legacy, legacy);
  ESP_LOGI(
      TAG, "Calibration load status: %s",
      CalibrationStorageCore::status_name(loaded.status));

  if (loaded.migration_required) {
    if (persist_calibration(loaded.calibration)) {
      ESP_LOGI(TAG, "Migrated V1 calibration to V2");
    } else {
      ESP_LOGE(TAG, "Failed to persist migrated calibration");
    }
  } else if (
      loaded.status == CalibrationLoadStatus::INVALID_USING_FACTORY) {
    ESP_LOGW(TAG, "Invalid stored calibration; using factory values");
  }
  return loaded.calibration;
}

void release_output_hardware() {
  if (s_output_hardware.humidity != nullptr) {
    i2c_master_bus_rm_device(s_output_hardware.humidity);
    s_output_hardware.humidity = nullptr;
  }
  if (s_output_hardware.temperature != nullptr) {
    i2c_master_bus_rm_device(s_output_hardware.temperature);
    s_output_hardware.temperature = nullptr;
  }
  if (s_output_hardware.bus != nullptr) {
    i2c_del_master_bus(s_output_hardware.bus);
    s_output_hardware.bus = nullptr;
  }
  s_output_hardware.ready = false;
}

bool initialize_output_hardware() {
  if (s_output_hardware.ready)
    return true;
  release_output_hardware();

  i2c_master_bus_config_t bus_config{};
  bus_config.i2c_port = I2C_NUM_0;
  bus_config.sda_io_num =
      static_cast<gpio_num_t>(CONFIG_IDM_I2C_SDA_GPIO);
  bus_config.scl_io_num =
      static_cast<gpio_num_t>(CONFIG_IDM_I2C_SCL_GPIO);
  bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_config.glitch_ignore_cnt = 7;
  bus_config.flags.enable_internal_pullup = true;

  esp_err_t result =
      i2c_new_master_bus(&bus_config, &s_output_hardware.bus);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "I2C bus initialization failed: %s",
             esp_err_to_name(result));
    set_output_error("i2c_bus_init_failed");
    release_output_hardware();
    return false;
  }

  i2c_device_config_t humidity_config{};
  humidity_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  humidity_config.device_address = CONFIG_IDM_HUMIDITY_I2C_ADDRESS;
  humidity_config.scl_speed_hz = 100000;
  result = i2c_master_bus_add_device(
      s_output_hardware.bus, &humidity_config,
      &s_output_hardware.humidity);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "MCP4725 registration failed: %s",
             esp_err_to_name(result));
    set_output_error("humidity_dac_init_failed");
    release_output_hardware();
    return false;
  }

  i2c_device_config_t temperature_config{};
  temperature_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  temperature_config.device_address =
      CONFIG_IDM_TEMPERATURE_I2C_ADDRESS;
  temperature_config.scl_speed_hz = 100000;
  result = i2c_master_bus_add_device(
      s_output_hardware.bus, &temperature_config,
      &s_output_hardware.temperature);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "AD5242 registration failed: %s",
             esp_err_to_name(result));
    set_output_error("temperature_digipot_init_failed");
    release_output_hardware();
    return false;
  }

  s_output_hardware.ready = true;
  set_output_error("none");
  ESP_LOGI(TAG, "I2C analog outputs initialized");
  return true;
}

bool write_output(const NativeOutput &output) {
  if (!s_output_hardware.ready)
    return false;

  const uint16_t humidity_code = output.humidity_dac_code & 0x0FFF;
  const uint8_t humidity_data[] = {
      0x40,
      static_cast<uint8_t>(humidity_code >> 4),
      static_cast<uint8_t>((humidity_code & 0x0F) << 4),
  };
  const uint8_t temperature_data[] = {
      0x00,
      output.temperature_digipot_code,
  };

  const esp_err_t humidity_result = i2c_master_transmit(
      s_output_hardware.humidity, humidity_data, sizeof(humidity_data),
      100);
  const esp_err_t temperature_result = i2c_master_transmit(
      s_output_hardware.temperature, temperature_data,
      sizeof(temperature_data), 100);

  if (humidity_result != ESP_OK && temperature_result != ESP_OK) {
    set_output_error("humidity_and_temperature_write_failed");
  } else if (humidity_result != ESP_OK) {
    set_output_error("humidity_dac_write_failed");
  } else if (temperature_result != ESP_OK) {
    set_output_error("temperature_digipot_write_failed");
  } else {
    set_output_error("none");
    return true;
  }

  ESP_LOGE(
      TAG, "Analog output write failed: humidity=%s temperature=%s",
      esp_err_to_name(humidity_result),
      esp_err_to_name(temperature_result));
  return false;
}

void record_output_result(bool success) {
  RuntimeGuard guard;
  s_runtime.record_output_result(success);
}

bool apply_outputs_once(uint32_t current_ms) {
  NativeOutput output{};
  bool dirty = false;
  {
    RuntimeGuard guard;
    s_runtime.tick(current_ms);
    dirty = s_runtime.output_dirty();
    if (dirty)
      output = s_runtime.desired_output();
  }
  if (!dirty)
    return true;

  const bool success =
      initialize_output_hardware() && write_output(output);
  record_output_result(success);
  return success;
}

void add_number_or_null(cJSON *root, const char *name, float value) {
  if (std::isfinite(value))
    cJSON_AddNumberToObject(root, name, value);
  else
    cJSON_AddNullToObject(root, name);
}

esp_err_t send_json(httpd_req_t *request, cJSON *root) {
  char *payload = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (payload == nullptr)
    return httpd_resp_send_err(
        request, HTTPD_500_INTERNAL_SERVER_ERROR,
        "JSON serialization failed");
  httpd_resp_set_type(request, "application/json");
  const esp_err_t result =
      httpd_resp_send(request, payload, HTTPD_RESP_USE_STRLEN);
  std::free(payload);
  return result;
}

esp_err_t send_error(
    httpd_req_t *request, const char *status, const char *error) {
  httpd_resp_set_status(request, status);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "error", error);
  return send_json(request, root);
}

bool constant_time_equal(
    const std::string &left, const std::string &right) {
  const size_t maximum =
      left.size() > right.size() ? left.size() : right.size();
  unsigned int difference =
      static_cast<unsigned int>(left.size() ^ right.size());
  for (size_t index = 0; index < maximum; index++) {
    const unsigned char left_value =
        index < left.size()
            ? static_cast<unsigned char>(left[index])
            : 0;
    const unsigned char right_value =
        index < right.size()
            ? static_cast<unsigned char>(right[index])
            : 0;
    difference |= left_value ^ right_value;
  }
  return difference == 0;
}

bool authorize_mutation(httpd_req_t *request) {
  const std::string token = CONFIG_IDM_API_TOKEN;
  if (token.empty()) {
    send_error(
        request, "503 Service Unavailable",
        "mutating API disabled until IDM_API_TOKEN is configured");
    return false;
  }

  char header[384]{};
  if (httpd_req_get_hdr_value_str(
          request, "Authorization", header, sizeof(header)) != ESP_OK) {
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Bearer");
    send_error(request, "401 Unauthorized", "missing bearer token");
    return false;
  }

  const std::string expected = "Bearer " + token;
  if (!constant_time_equal(header, expected)) {
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Bearer");
    send_error(request, "401 Unauthorized", "invalid bearer token");
    return false;
  }
  return true;
}

// Outcome of a diagnostics authorization check.
enum class DiagnosticsAuth {
  kFull,         // token configured and request authorized: publish everything
  kRedacted,     // token unset: publish operational fields, redact sensor data
  kBlocked,      // token configured but request unauthorized: 401 already sent
};

// When IDM_API_TOKEN is configured, the diagnostics endpoint is gated by the
// same bearer check as mutating endpoints. When the token is empty, the
// endpoint still answers (operational diagnostics) but sensor values and the
// command provenance are redacted so a network observer cannot read live
// climate state or learn whether the mutating API is protected.
DiagnosticsAuth authorize_diagnostics(httpd_req_t *request) {
  const std::string token = CONFIG_IDM_API_TOKEN;
  if (token.empty())
    return DiagnosticsAuth::kRedacted;
  char header[384]{};
  if (httpd_req_get_hdr_value_str(
          request, "Authorization", header, sizeof(header)) != ESP_OK) {
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Bearer");
    send_error(request, "401 Unauthorized", "missing bearer token");
    return DiagnosticsAuth::kBlocked;
  }
  const std::string expected = "Bearer " + token;
  if (!constant_time_equal(header, expected)) {
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Bearer");
    send_error(request, "401 Unauthorized", "invalid bearer token");
    return DiagnosticsAuth::kBlocked;
  }
  return DiagnosticsAuth::kFull;
}

// Replace sensor and command-provenance fields with non-sensitive surrogates.
// Called when the diagnostics request is unauthenticated and IDM_API_TOKEN is
// unset. Operational fields (bridge_state, safe_active/stale/fault, uptime,
// free_heap, output_attempts/failures, network_connected, mutating_api_enabled)
// remain visible for commissioning diagnostics.
void redact_diagnostics(cJSON *root) {
  cJSON_ReplaceItemInObjectCaseSensitive(
      root, "effective_humidity", cJSON_CreateNull());
  cJSON_ReplaceItemInObjectCaseSensitive(
      root, "effective_temperature", cJSON_CreateNull());
  cJSON_ReplaceItemInObjectCaseSensitive(
      root, "dew_point_c", cJSON_CreateNull());
  cJSON_ReplaceItemInObjectCaseSensitive(
      root, "command_source", cJSON_CreateString("redacted"));
  cJSON_ReplaceItemInObjectCaseSensitive(
      root, "humidity_dac_code", cJSON_CreateNull());
  cJSON_ReplaceItemInObjectCaseSensitive(
      root, "temperature_digipot_code", cJSON_CreateNull());
  cJSON_ReplaceItemInObjectCaseSensitive(
      root, "temperature_resistance_ohm", cJSON_CreateNull());
}

bool require_confirmation(
    httpd_req_t *request, const char *expected_value) {
  char header[96]{};
  if (httpd_req_get_hdr_value_str(
          request, "X-IDM-Confirm", header, sizeof(header)) != ESP_OK ||
      std::strcmp(header, expected_value) != 0) {
    send_error(
        request, "409 Conflict",
        "required X-IDM-Confirm header is missing or invalid");
    return false;
  }
  return true;
}

cJSON *read_json_body(httpd_req_t *request) {
  if (request->content_len <= 0 ||
      request->content_len >= static_cast<int>(HTTP_BODY_LIMIT))
    return nullptr;

  char buffer[HTTP_BODY_LIMIT]{};
  int received_total = 0;
  while (received_total < request->content_len) {
    const int received = httpd_req_recv(
        request, buffer + received_total,
        request->content_len - received_total);
    if (received == HTTPD_SOCK_ERR_TIMEOUT)
      continue;
    if (received <= 0)
      return nullptr;
    received_total += received;
  }
  buffer[received_total] = '\0';
  return cJSON_Parse(buffer);
}

bool json_integer_in_range(
    const cJSON *value, int minimum, int maximum, int *result) {
  if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) ||
      std::floor(value->valuedouble) != value->valuedouble ||
      value->valuedouble < minimum || value->valuedouble > maximum)
    return false;
  *result = static_cast<int>(value->valuedouble);
  return true;
}

esp_err_t diagnostics_get_handler(httpd_req_t *request) {
  const DiagnosticsAuth auth = authorize_diagnostics(request);
  if (auth == DiagnosticsAuth::kBlocked)
    return ESP_OK;

  NativeDiagnostics diagnostics{};
  char output_error[sizeof(s_output_error)]{};
  char ota_status[sizeof(s_ota_status)]{};
  {
    RuntimeGuard guard;
    diagnostics = s_runtime.diagnostics(now_ms());
    copy_status(output_error, sizeof(output_error), s_output_error);
    copy_status(ota_status, sizeof(ota_status), s_ota_status);
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "schema_version", 1);
  cJSON_AddStringToObject(root, "mode", "native_esp_idf");
  cJSON_AddStringToObject(
      root, "project", esp_app_get_description()->project_name);
  cJSON_AddStringToObject(
      root, "firmware_version", esp_app_get_description()->version);
  const esp_partition_t *running = esp_ota_get_running_partition();
  cJSON_AddStringToObject(
      root, "running_partition",
      running != nullptr ? running->label : "unknown");
  cJSON_AddStringToObject(
      root, "bridge_state", diagnostics.bridge_state);
  cJSON_AddStringToObject(
      root, "bridge_error", diagnostics.bridge_error);
  cJSON_AddBoolToObject(root, "safe_active", diagnostics.safe_active);
  cJSON_AddBoolToObject(root, "stale", diagnostics.stale);
  cJSON_AddBoolToObject(root, "fault", diagnostics.fault);
  add_number_or_null(
      root, "effective_humidity", diagnostics.effective_humidity);
  add_number_or_null(
      root, "effective_temperature", diagnostics.effective_temperature);
  add_number_or_null(root, "dew_point_c", diagnostics.dew_point);
  cJSON_AddStringToObject(
      root, "command_source", diagnostics.command_source.c_str());
  cJSON_AddStringToObject(
      root, "command_status", diagnostics.command_status.c_str());
  cJSON_AddNumberToObject(
      root, "command_quality", diagnostics.command_quality);
  cJSON_AddNumberToObject(
      root, "command_age_ms", diagnostics.command_age_ms);
  cJSON_AddBoolToObject(
      root, "output_fault", diagnostics.output_fault);
  cJSON_AddStringToObject(root, "output_error", output_error);
  cJSON_AddNumberToObject(
      root, "output_attempts", diagnostics.output_attempts);
  cJSON_AddNumberToObject(
      root, "output_failures", diagnostics.output_failures);
  cJSON_AddNumberToObject(
      root, "humidity_dac_code",
      diagnostics.output.humidity_dac_code);
  cJSON_AddNumberToObject(
      root, "temperature_digipot_code",
      diagnostics.output.temperature_digipot_code);
  add_number_or_null(
      root, "temperature_resistance_ohm",
      diagnostics.output.temperature_resistance_ohm);
  cJSON_AddNumberToObject(
      root, "calibration_humidity_code_min",
      diagnostics.calibration.humidity_code_min);
  cJSON_AddNumberToObject(
      root, "calibration_humidity_code_max",
      diagnostics.calibration.humidity_code_max);
  cJSON_AddNumberToObject(
      root, "calibration_temperature_resistance_min",
      diagnostics.calibration.temperature_resistance_min);
  cJSON_AddNumberToObject(
      root, "calibration_temperature_resistance_max",
      diagnostics.calibration.temperature_resistance_max);
  cJSON_AddBoolToObject(
      root, "calibration_temperature_code_inverted",
      diagnostics.calibration.temperature_code_inverted);
  cJSON_AddBoolToObject(
      root, "network_connected", s_wifi_connected.load());
  cJSON_AddBoolToObject(
      root, "mutating_api_enabled",
      std::strlen(CONFIG_IDM_API_TOKEN) > 0);
  cJSON_AddStringToObject(root, "ota_status", ota_status);
  cJSON_AddNumberToObject(
      root, "uptime_s",
      static_cast<double>(esp_timer_get_time()) / 1000000.0);
  cJSON_AddNumberToObject(root, "free_heap_bytes", esp_get_free_heap_size());
  if (auth == DiagnosticsAuth::kRedacted) {
    redact_diagnostics(root);
    cJSON_AddBoolToObject(root, "redacted", true);
  }
  return send_json(request, root);
}

esp_err_t climate_post_handler(httpd_req_t *request) {
  if (!authorize_mutation(request))
    return ESP_OK;

  cJSON *root = read_json_body(request);
  if (root == nullptr)
    return send_error(request, "400 Bad Request", "invalid JSON body");

  const cJSON *humidity = cJSON_GetObjectItemCaseSensitive(root, "humidity");
  const cJSON *temperature =
      cJSON_GetObjectItemCaseSensitive(root, "temperature");
  const cJSON *quality = cJSON_GetObjectItemCaseSensitive(root, "quality");
  const cJSON *source = cJSON_GetObjectItemCaseSensitive(root, "source");
  int quality_value = 100;
  const bool valid =
      cJSON_IsNumber(humidity) && cJSON_IsNumber(temperature) &&
      std::isfinite(humidity->valuedouble) &&
      std::isfinite(temperature->valuedouble) &&
      (quality == nullptr ||
       json_integer_in_range(quality, 0, 100, &quality_value)) &&
      (source == nullptr || cJSON_IsString(source));
  if (!valid) {
    cJSON_Delete(root);
    return send_error(
        request, "422 Unprocessable Entity",
        "humidity, temperature, quality or source is invalid");
  }

  std::string source_value =
      source != nullptr ? source->valuestring : "http";
  bool accepted = false;
  std::string command_status;
  {
    RuntimeGuard guard;
    accepted = s_runtime.accept_command(
        static_cast<float>(humidity->valuedouble),
        static_cast<float>(temperature->valuedouble),
        static_cast<uint8_t>(quality_value), source_value, now_ms());
    command_status = s_runtime.diagnostics(now_ms()).command_status;
  }
  cJSON_Delete(root);

  cJSON *response = cJSON_CreateObject();
  cJSON_AddBoolToObject(response, "accepted", accepted);
  cJSON_AddStringToObject(
      response, "command_status", command_status.c_str());
  if (!accepted)
    httpd_resp_set_status(request, "422 Unprocessable Entity");
  return send_json(request, response);
}

esp_err_t fallback_post_handler(httpd_req_t *request) {
  if (!authorize_mutation(request))
    return ESP_OK;
  {
    RuntimeGuard guard;
    s_runtime.force_fallback("http_manual_fallback");
  }
  cJSON *response = cJSON_CreateObject();
  cJSON_AddBoolToObject(response, "accepted", true);
  cJSON_AddStringToObject(response, "bridge_state", "manual_safe");
  return send_json(request, response);
}

esp_err_t calibration_post_handler(httpd_req_t *request) {
  if (!authorize_mutation(request) ||
      !require_confirmation(request, "apply-calibration"))
    return ESP_OK;

  cJSON *root = read_json_body(request);
  if (root == nullptr)
    return send_error(request, "400 Bad Request", "invalid JSON body");

  const cJSON *humidity_min =
      cJSON_GetObjectItemCaseSensitive(root, "humidity_code_min");
  const cJSON *humidity_max =
      cJSON_GetObjectItemCaseSensitive(root, "humidity_code_max");
  const cJSON *resistance_min = cJSON_GetObjectItemCaseSensitive(
      root, "temperature_resistance_min");
  const cJSON *resistance_max = cJSON_GetObjectItemCaseSensitive(
      root, "temperature_resistance_max");
  const cJSON *inverted = cJSON_GetObjectItemCaseSensitive(
      root, "temperature_code_inverted");
  int humidity_min_value = 0;
  int humidity_max_value = 0;
  const bool valid_fields =
      json_integer_in_range(humidity_min, 0, 4095, &humidity_min_value) &&
      json_integer_in_range(humidity_max, 0, 4095, &humidity_max_value) &&
      cJSON_IsNumber(resistance_min) &&
      cJSON_IsNumber(resistance_max) &&
      std::isfinite(resistance_min->valuedouble) &&
      std::isfinite(resistance_max->valuedouble) &&
      cJSON_IsBool(inverted);
  if (!valid_fields) {
    cJSON_Delete(root);
    return send_error(
        request, "422 Unprocessable Entity",
        "calibration fields are missing or invalid");
  }

  const IdmAnalogCalibration calibration{
      static_cast<uint16_t>(humidity_min_value),
      static_cast<uint16_t>(humidity_max_value),
      static_cast<float>(resistance_min->valuedouble),
      static_cast<float>(resistance_max->valuedouble),
      cJSON_IsTrue(inverted) != 0,
  };
  cJSON_Delete(root);
  if (!valid_calibration(calibration))
    return send_error(
        request, "422 Unprocessable Entity",
        "calibration violates safe ranges or endpoint ordering");
  if (!persist_calibration(calibration))
    return send_error(
        request, "500 Internal Server Error",
        "calibration persistence failed");

  bool applied = false;
  {
    RuntimeGuard guard;
    applied = s_runtime.apply_calibration(calibration);
  }
  if (!applied)
    return send_error(
        request, "500 Internal Server Error",
        "persisted calibration could not be activated");

  cJSON *response = cJSON_CreateObject();
  cJSON_AddBoolToObject(response, "accepted", true);
  cJSON_AddStringToObject(response, "calibration_status", "stored_v2");
  return send_json(request, response);
}

esp_err_t calibration_reset_post_handler(httpd_req_t *request) {
  if (!authorize_mutation(request) ||
      !require_confirmation(request, "reset-calibration"))
    return ESP_OK;

  IdmAnalogCalibration factory{};
  {
    RuntimeGuard guard;
    factory = s_runtime.factory_calibration();
  }
  if (!persist_calibration(factory))
    return send_error(
        request, "500 Internal Server Error",
        "factory calibration persistence failed");
  {
    RuntimeGuard guard;
    s_runtime.reset_calibration();
  }

  cJSON *response = cJSON_CreateObject();
  cJSON_AddBoolToObject(response, "accepted", true);
  cJSON_AddStringToObject(
      response, "calibration_status", "reset_to_factory");
  return send_json(request, response);
}

void ota_task(void *argument) {
  char *url = static_cast<char *>(argument);
  set_ota_status("downloading");
  ESP_LOGI(TAG, "Starting HTTPS OTA from %s", url);

  esp_http_client_config_t http_config{};
  http_config.url = url;
  http_config.crt_bundle_attach = esp_crt_bundle_attach;
  http_config.timeout_ms = 30000;
  http_config.keep_alive_enable = true;

  esp_https_ota_config_t ota_config{};
  ota_config.http_config = &http_config;
  const esp_err_t result = esp_https_ota(&ota_config);
  std::free(url);

  if (result == ESP_OK) {
    set_ota_status("rebooting");
    ESP_LOGI(TAG, "HTTPS OTA succeeded; rebooting");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
  }

  ESP_LOGE(TAG, "HTTPS OTA failed: %s", esp_err_to_name(result));
  set_ota_status("failed");
  s_ota_running.store(false);
  vTaskDelete(nullptr);
}

esp_err_t ota_post_handler(httpd_req_t *request) {
  if (!authorize_mutation(request) ||
      !require_confirmation(request, "firmware-update"))
    return ESP_OK;
  if (!s_wifi_connected.load())
    return send_error(
        request, "503 Service Unavailable",
        "network is not connected");

  cJSON *root = read_json_body(request);
  if (root == nullptr)
    return send_error(request, "400 Bad Request", "invalid JSON body");
  const cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");
  const bool valid_url =
      cJSON_IsString(url) && url->valuestring != nullptr &&
      std::strncmp(url->valuestring, "https://", 8) == 0 &&
      std::strlen(url->valuestring) < 512;
  if (!valid_url) {
    cJSON_Delete(root);
    return send_error(
        request, "422 Unprocessable Entity",
        "OTA URL must be an HTTPS URL shorter than 512 characters");
  }

  // Optional host allowlist. When IDM_OTA_ALLOWED_HOST is configured, the URL
  // host (between "https://" and the next '/', ':', or end of string) must
  // match exactly. Empty config preserves the legacy any-HTTPS behaviour for
  // isolated commissioning networks, but pinning is strongly recommended.
  const std::string allowed_host = CONFIG_IDM_OTA_ALLOWED_HOST;
  if (!allowed_host.empty()) {
    const char *host_start = url->valuestring + 8;  // past "https://"
    const char *host_end = host_start;
    while (*host_end != '\0' && *host_end != '/' && *host_end != ':')
      ++host_end;
    const std::string url_host(host_start, host_end - host_start);
    if (url_host != allowed_host) {
      cJSON_Delete(root);
      return send_error(
          request, "422 Unprocessable Entity",
          "OTA URL host is not on the configured allowlist");
    }
  }

  bool expected = false;
  if (!s_ota_running.compare_exchange_strong(expected, true)) {
    cJSON_Delete(root);
    return send_error(request, "409 Conflict", "OTA already in progress");
  }

  char *url_copy = ::strdup(url->valuestring);
  cJSON_Delete(root);
  if (url_copy == nullptr) {
    s_ota_running.store(false);
    return send_error(
        request, "500 Internal Server Error", "out of memory");
  }

  set_ota_status("queued");
  const BaseType_t task_result = xTaskCreate(
      ota_task, "idm_https_ota", CONFIG_IDM_OTA_TASK_STACK_SIZE,
      url_copy, 5, nullptr);
  if (task_result != pdPASS) {
    std::free(url_copy);
    s_ota_running.store(false);
    set_ota_status("failed");
    return send_error(
        request, "500 Internal Server Error",
        "could not create OTA task");
  }

  httpd_resp_set_status(request, "202 Accepted");
  cJSON *response = cJSON_CreateObject();
  cJSON_AddBoolToObject(response, "accepted", true);
  cJSON_AddStringToObject(response, "ota_status", "queued");
  return send_json(request, response);
}

esp_err_t start_http_server() {
  if (s_http_server != nullptr)
    return ESP_OK;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = CONFIG_IDM_HTTP_PORT;
  config.max_uri_handlers = 8;
  config.stack_size = 8192;
  esp_err_t result = httpd_start(&s_http_server, &config);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(result));
    s_http_server = nullptr;
    return result;
  }

  static const httpd_uri_t diagnostics_uri{
      .uri = "/api/v1/diagnostics",
      .method = HTTP_GET,
      .handler = diagnostics_get_handler,
      .user_ctx = nullptr,
  };
  static const httpd_uri_t climate_uri{
      .uri = "/api/v1/climate",
      .method = HTTP_POST,
      .handler = climate_post_handler,
      .user_ctx = nullptr,
  };
  static const httpd_uri_t fallback_uri{
      .uri = "/api/v1/fallback",
      .method = HTTP_POST,
      .handler = fallback_post_handler,
      .user_ctx = nullptr,
  };
  static const httpd_uri_t calibration_uri{
      .uri = "/api/v1/calibration",
      .method = HTTP_POST,
      .handler = calibration_post_handler,
      .user_ctx = nullptr,
  };
  static const httpd_uri_t calibration_reset_uri{
      .uri = "/api/v1/calibration/reset",
      .method = HTTP_POST,
      .handler = calibration_reset_post_handler,
      .user_ctx = nullptr,
  };
  static const httpd_uri_t ota_uri{
      .uri = "/api/v1/ota",
      .method = HTTP_POST,
      .handler = ota_post_handler,
      .user_ctx = nullptr,
  };

  for (const httpd_uri_t *uri : {
           &diagnostics_uri,
           &climate_uri,
           &fallback_uri,
           &calibration_uri,
           &calibration_reset_uri,
           &ota_uri,
       }) {
    result = httpd_register_uri_handler(s_http_server, uri);
    if (result != ESP_OK) {
      ESP_LOGE(TAG, "HTTP URI registration failed for %s: %s",
               uri->uri, esp_err_to_name(result));
      return result;
    }
  }

  ESP_LOGI(
      TAG, "HTTP diagnostics/API listening on port %d",
      CONFIG_IDM_HTTP_PORT);
  return ESP_OK;
}

void wifi_event_handler(
    void *argument, esp_event_base_t event_base, int32_t event_id,
    void *event_data) {
  (void) argument;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    return;
  }
  if (event_base == WIFI_EVENT &&
      event_id == WIFI_EVENT_STA_DISCONNECTED) {
    s_wifi_connected.store(false);
    ESP_LOGW(TAG, "Wi-Fi disconnected; retaining local fail-safe control");
    esp_wifi_connect();
    return;
  }
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    auto *event = static_cast<ip_event_got_ip_t *>(event_data);
    s_wifi_connected.store(true);
    ESP_LOGI(TAG, "Wi-Fi connected, address " IPSTR,
             IP2STR(&event->ip_info.ip));
    start_http_server();
  }
}

bool initialize_wifi() {
  if (std::strlen(CONFIG_IDM_WIFI_SSID) == 0) {
    ESP_LOGW(
        TAG,
        "IDM_WIFI_SSID is empty; native firmware remains local-only");
    return false;
  }

  esp_err_t result = esp_netif_init();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
    return false;
  result = esp_event_loop_create_default();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
    return false;

  esp_netif_t *station = esp_netif_create_default_wifi_sta();
  if (station == nullptr)
    return false;
  esp_netif_set_hostname(station, CONFIG_IDM_HOSTNAME);

  wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&initialization) != ESP_OK)
    return false;
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, nullptr));

  wifi_config_t configuration{};
  std::snprintf(
      reinterpret_cast<char *>(configuration.sta.ssid),
      sizeof(configuration.sta.ssid), "%s", CONFIG_IDM_WIFI_SSID);
  std::snprintf(
      reinterpret_cast<char *>(configuration.sta.password),
      sizeof(configuration.sta.password), "%s",
      CONFIG_IDM_WIFI_PASSWORD);
  configuration.sta.threshold.authmode =
      std::strlen(CONFIG_IDM_WIFI_PASSWORD) == 0
          ? WIFI_AUTH_OPEN
          : WIFI_AUTH_WPA2_PSK;

  if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
      esp_wifi_set_config(WIFI_IF_STA, &configuration) != ESP_OK ||
      esp_wifi_start() != ESP_OK) {
    ESP_LOGE(TAG, "Wi-Fi initialization failed");
    return false;
  }
  ESP_LOGI(TAG, "Wi-Fi station started for SSID %s", CONFIG_IDM_WIFI_SSID);
  return true;
}

void initialize_watchdog() {
  esp_err_t result = esp_task_wdt_add(nullptr);
  if (result == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_config_t config{};
    config.timeout_ms = 10000;
    config.idle_core_mask =
        (1U << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1U;
    config.trigger_panic = true;
    result = esp_task_wdt_init(&config);
    if (result == ESP_OK)
      result = esp_task_wdt_add(nullptr);
  }
  if (result == ESP_OK) {
    ESP_LOGI(TAG, "Task watchdog subscribed");
  } else {
    ESP_LOGE(TAG, "Task watchdog subscription failed: %s",
             esp_err_to_name(result));
  }
}

bool s_ota_confirmed = false;
uint32_t s_boot_ms = 0;

// Health-gated OTA confirmation. The previous implementation marked a pending
// image valid on every boot unconditionally, which defeated the ESP-IDF
// anti-rollback mechanism: a broken image was accepted before any safety check
// could run. Now confirmation requires BOTH:
//   1. The main loop is alive and the watchdog was reset for at least
//      CONFIG_IDM_OTA_HEALTH_SECONDS since boot, AND
//   2. Either the I2C analog output initialized successfully, or the I2C
//      retry window has elapsed (so a persistently broken bus does not pin
//      the device in PENDING forever — the bridge still runs fail-safe).
// If the new image crash-loops before the window elapses, the watchdog panic
// triggers a reset and ESP-IDF rolls back to the previous image automatically.
void try_confirm_ota_image(uint32_t current_ms) {
  if (s_ota_confirmed)
    return;
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running == nullptr)
    return;
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) != ESP_OK ||
      state != ESP_OTA_IMG_PENDING_VERIFY)
    return;

  const uint32_t health_window_ms =
      static_cast<uint32_t>(CONFIG_IDM_OTA_HEALTH_SECONDS) * 1000U;
  const bool health_window_elapsed =
      static_cast<uint32_t>(current_ms - s_boot_ms) >= health_window_ms;
  const bool i2c_ready_or_retried =
      s_output_hardware.ready ||
      static_cast<int32_t>(current_ms - s_next_i2c_attempt_ms) >= 0;
  if (!health_window_elapsed || !i2c_ready_or_retried)
    return;

  const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
  s_ota_confirmed = true;
  if (result == ESP_OK) {
    ESP_LOGI(TAG, "Pending OTA image marked valid after health window");
  } else {
    ESP_LOGE(TAG, "Could not validate OTA image: %s",
             esp_err_to_name(result));
  }
}

void log_diagnostics() {
  NativeDiagnostics diagnostics{};
  char output_error[sizeof(s_output_error)]{};
  char ota_status[sizeof(s_ota_status)]{};
  {
    RuntimeGuard guard;
    diagnostics = s_runtime.diagnostics(now_ms());
    copy_status(output_error, sizeof(output_error), s_output_error);
    copy_status(ota_status, sizeof(ota_status), s_ota_status);
  }
  ESP_LOGI(
      TAG,
      "state=%s error=%s safe=%d humidity=%.1f temperature=%.1f "
      "source=%s quality=%u output_fault=%d output_error=%s "
      "wifi=%d ota=%s",
      diagnostics.bridge_state, diagnostics.bridge_error,
      diagnostics.safe_active, diagnostics.effective_humidity,
      diagnostics.effective_temperature,
      diagnostics.command_source.c_str(), diagnostics.command_quality,
      diagnostics.output_fault, output_error, s_wifi_connected.load(),
      ota_status);
}

}  // namespace

extern "C" void app_main(void) {
  s_runtime_mutex = xSemaphoreCreateMutex();
  if (s_runtime_mutex == nullptr) {
    ESP_LOGE(TAG, "Runtime mutex allocation failed");
    return;
  }

  s_nvs_ready = initialize_nvs();
  const IdmAnalogCalibration factory = factory_calibration();
  if (!valid_calibration(factory)) {
    ESP_LOGE(
        TAG,
        "Configured factory calibration is invalid; using safe defaults");
  }
  const IdmAnalogCalibration safe_factory =
      valid_calibration(factory) ? factory : IdmAnalogCalibration{};
  const IdmAnalogCalibration loaded = load_calibration(safe_factory);

  NativeRuntimeConfig runtime_config{};
  runtime_config.fallback_humidity =
      configured_float(CONFIG_IDM_FALLBACK_HUMIDITY, 80.0f);
  runtime_config.fallback_temperature =
      configured_float(CONFIG_IDM_FALLBACK_TEMPERATURE, 28.0f);
  runtime_config.stale_timeout_ms =
      static_cast<uint32_t>(CONFIG_IDM_STALE_TIMEOUT_SECONDS) * 1000U;
  runtime_config.minimum_command_quality =
      static_cast<uint8_t>(CONFIG_IDM_MINIMUM_COMMAND_QUALITY);
  {
    RuntimeGuard guard;
    s_runtime.configure(runtime_config);
    s_runtime.boot(now_ms(), safe_factory, loaded);
  }

  initialize_watchdog();

  // Hardware outputs are attempted before any network service starts. A
  // missing or failed device latches the runtime into output-fault fallback
  // and is retried without blocking diagnostics or the watchdog.
  const uint32_t startup_ms = now_ms();
  s_boot_ms = startup_ms;
  apply_outputs_once(startup_ms);
  s_next_output_attempt_ms = startup_ms + OUTPUT_RETRY_MS;
  s_next_i2c_attempt_ms = startup_ms + I2C_RETRY_MS;

  // A pending OTA image is no longer confirmed blindly at boot. The health
  // window is evaluated inside the main loop (try_confirm_ota_image), so a
  // crash-looping new image rolls back instead of being accepted.
  initialize_wifi();

  uint32_t next_diagnostic_log_ms = now_ms();
  while (true) {
    const uint32_t current_ms = now_ms();
    {
      RuntimeGuard guard;
      s_runtime.tick(current_ms);
    }

    if (!s_output_hardware.ready &&
        static_cast<int32_t>(current_ms - s_next_i2c_attempt_ms) >= 0) {
      initialize_output_hardware();
      s_next_i2c_attempt_ms = current_ms + I2C_RETRY_MS;
    }
    if (static_cast<int32_t>(
            current_ms - s_next_output_attempt_ms) >= 0) {
      apply_outputs_once(current_ms);
      s_next_output_attempt_ms = current_ms + OUTPUT_RETRY_MS;
    }
    if (static_cast<int32_t>(
            current_ms - next_diagnostic_log_ms) >= 0) {
      log_diagnostics();
      next_diagnostic_log_ms =
          current_ms +
          static_cast<uint32_t>(
              CONFIG_IDM_DIAGNOSTIC_LOG_INTERVAL_SECONDS) *
              1000U;
    }

    try_confirm_ota_image(current_ms);
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

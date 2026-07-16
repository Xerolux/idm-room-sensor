#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace esphome::idm_bridge {

inline constexpr uint16_t CALIBRATION_HUMIDITY_CODE_MIN = 0;
inline constexpr uint16_t CALIBRATION_HUMIDITY_CODE_MAX = 4095;
inline constexpr float CALIBRATION_RESISTANCE_MIN = 100.0f;
inline constexpr float CALIBRATION_RESISTANCE_MAX = 50000.0f;

struct IdmAnalogCalibration {
  uint16_t humidity_code_min{0};
  uint16_t humidity_code_max{4095};
  float temperature_resistance_min{650.0f};
  float temperature_resistance_max{3000.0f};
  bool temperature_code_inverted{false};
};

inline bool valid_calibration(const IdmAnalogCalibration &value) {
  return value.humidity_code_max <= CALIBRATION_HUMIDITY_CODE_MAX &&
         value.humidity_code_min < value.humidity_code_max &&
         std::isfinite(value.temperature_resistance_min) &&
         std::isfinite(value.temperature_resistance_max) &&
         value.temperature_resistance_min >= CALIBRATION_RESISTANCE_MIN &&
         value.temperature_resistance_max <= CALIBRATION_RESISTANCE_MAX &&
         value.temperature_resistance_min <
             value.temperature_resistance_max;
}

inline bool calibration_equal(const IdmAnalogCalibration &left,
                              const IdmAnalogCalibration &right) {
  return left.humidity_code_min == right.humidity_code_min &&
         left.humidity_code_max == right.humidity_code_max &&
         left.temperature_resistance_min ==
             right.temperature_resistance_min &&
         left.temperature_resistance_max ==
             right.temperature_resistance_max &&
         left.temperature_code_inverted ==
             right.temperature_code_inverted;
}

struct CalibrationRecordV2 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  float temperature_resistance_min;
  float temperature_resistance_max;
  uint16_t humidity_code_min;
  uint16_t humidity_code_max;
  uint32_t flags;
  uint32_t crc32;
};

// Historical format retained solely for deterministic migration testing.
// humidity_gain is measured in DAC codes per percentage point.
struct CalibrationRecordV1 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  float humidity_gain;
  float humidity_offset;
  float temperature_resistance_min;
  float temperature_resistance_max;
  uint32_t flags;
  uint32_t crc32;
};

static_assert(sizeof(CalibrationRecordV2) == 28);
static_assert(sizeof(CalibrationRecordV1) == 32);
static_assert(std::is_trivially_copyable_v<CalibrationRecordV2>);
static_assert(std::is_trivially_copyable_v<CalibrationRecordV1>);

enum class CalibrationLoadStatus : uint8_t {
  FACTORY_DEFAULTS = 0,
  STORED_V2,
  MIGRATED_V1,
  INVALID_USING_FACTORY,
};

struct CalibrationLoadResult {
  IdmAnalogCalibration calibration;
  CalibrationLoadStatus status;
  bool migration_required;
};

class CalibrationStorageCore {
 public:
  static constexpr uint32_t MAGIC = 0x49444D43;
  static constexpr uint16_t CURRENT_VERSION = 2;
  static constexpr uint16_t LEGACY_VERSION = 1;
  static constexpr uint32_t FLAG_TEMPERATURE_CODE_INVERTED = 1U;

  static uint32_t crc32(const void *data, size_t length) {
    const auto *bytes = static_cast<const uint8_t *>(data);
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t index = 0; index < length; index++) {
      crc ^= bytes[index];
      for (uint8_t bit = 0; bit < 8; bit++) {
        const uint32_t mask =
            static_cast<uint32_t>(-static_cast<int32_t>(crc & 1U));
        crc = (crc >> 1U) ^ (0xEDB88320U & mask);
      }
    }
    return ~crc;
  }

  static CalibrationRecordV2 make_record(
      const IdmAnalogCalibration &calibration) {
    CalibrationRecordV2 record{};
    record.magic = MAGIC;
    record.version = CURRENT_VERSION;
    record.size = sizeof(record);
    record.temperature_resistance_min =
        calibration.temperature_resistance_min;
    record.temperature_resistance_max =
        calibration.temperature_resistance_max;
    record.humidity_code_min = calibration.humidity_code_min;
    record.humidity_code_max = calibration.humidity_code_max;
    record.flags = calibration.temperature_code_inverted
                       ? FLAG_TEMPERATURE_CODE_INVERTED
                       : 0U;
    record.crc32 = record_crc(record);
    return record;
  }

  static CalibrationRecordV1 make_legacy_record(
      float humidity_gain, float humidity_offset,
      float temperature_resistance_min, float temperature_resistance_max,
      bool temperature_code_inverted) {
    CalibrationRecordV1 record{};
    record.magic = MAGIC;
    record.version = LEGACY_VERSION;
    record.size = sizeof(record);
    record.humidity_gain = humidity_gain;
    record.humidity_offset = humidity_offset;
    record.temperature_resistance_min = temperature_resistance_min;
    record.temperature_resistance_max = temperature_resistance_max;
    record.flags = temperature_code_inverted
                       ? FLAG_TEMPERATURE_CODE_INVERTED
                       : 0U;
    record.crc32 = record_crc(record);
    return record;
  }

  static bool decode_record(const CalibrationRecordV2 &record,
                            IdmAnalogCalibration *calibration) {
    if (record.magic != MAGIC || record.version != CURRENT_VERSION ||
        record.size != sizeof(record) ||
        (record.flags & ~FLAG_TEMPERATURE_CODE_INVERTED) != 0U ||
        record.crc32 != record_crc(record))
      return false;

    IdmAnalogCalibration decoded{
        record.humidity_code_min,
        record.humidity_code_max,
        record.temperature_resistance_min,
        record.temperature_resistance_max,
        (record.flags & FLAG_TEMPERATURE_CODE_INVERTED) != 0U,
    };
    if (!valid_calibration(decoded))
      return false;
    *calibration = decoded;
    return true;
  }

  static bool migrate_record(const CalibrationRecordV1 &record,
                             IdmAnalogCalibration *calibration) {
    if (record.magic != MAGIC || record.version != LEGACY_VERSION ||
        record.size != sizeof(record) ||
        (record.flags & ~FLAG_TEMPERATURE_CODE_INVERTED) != 0U ||
        record.crc32 != record_crc(record) ||
        !std::isfinite(record.humidity_gain) ||
        !std::isfinite(record.humidity_offset) ||
        record.humidity_gain <= 0.0f)
      return false;

    const float minimum = record.humidity_offset;
    const float maximum =
        record.humidity_offset + record.humidity_gain * 100.0f;
    if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
        minimum < CALIBRATION_HUMIDITY_CODE_MIN ||
        maximum > CALIBRATION_HUMIDITY_CODE_MAX)
      return false;

    IdmAnalogCalibration migrated{
        static_cast<uint16_t>(std::lround(minimum)),
        static_cast<uint16_t>(std::lround(maximum)),
        record.temperature_resistance_min,
        record.temperature_resistance_max,
        (record.flags & FLAG_TEMPERATURE_CODE_INVERTED) != 0U,
    };
    if (!valid_calibration(migrated))
      return false;
    *calibration = migrated;
    return true;
  }

  static CalibrationLoadResult load(
      const IdmAnalogCalibration &factory, bool has_current,
      const CalibrationRecordV2 &current, bool has_legacy,
      const CalibrationRecordV1 &legacy) {
    const IdmAnalogCalibration safe_factory =
        valid_calibration(factory) ? factory : IdmAnalogCalibration{};
    IdmAnalogCalibration calibration{};

    if (has_current) {
      if (decode_record(current, &calibration)) {
        return {calibration, CalibrationLoadStatus::STORED_V2, false};
      }
      // Never downgrade from a present but invalid current record to an older
      // record. That could resurrect stale calibration unexpectedly.
      return {safe_factory,
              CalibrationLoadStatus::INVALID_USING_FACTORY, false};
    }
    if (has_legacy) {
      if (migrate_record(legacy, &calibration)) {
        return {calibration, CalibrationLoadStatus::MIGRATED_V1, true};
      }
      return {safe_factory,
              CalibrationLoadStatus::INVALID_USING_FACTORY, false};
    }
    return {safe_factory, CalibrationLoadStatus::FACTORY_DEFAULTS, false};
  }

  static const char *status_name(CalibrationLoadStatus status) {
    switch (status) {
      case CalibrationLoadStatus::FACTORY_DEFAULTS:
        return "factory_defaults";
      case CalibrationLoadStatus::STORED_V2:
        return "stored_v2";
      case CalibrationLoadStatus::MIGRATED_V1:
        return "migrated_v1";
      case CalibrationLoadStatus::INVALID_USING_FACTORY:
        return "invalid_using_factory";
    }
    return "unknown";
  }

 private:
  template<typename T> static uint32_t record_crc(const T &record) {
    return crc32(&record, offsetof(T, crc32));
  }
};

}  // namespace esphome::idm_bridge

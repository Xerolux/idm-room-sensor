#include <cassert>
#include <cstddef>

#include "firmware/components/idm_bridge/idm_calibration_storage_core.h"

using esphome::idm_bridge::CalibrationLoadStatus;
using esphome::idm_bridge::CalibrationRecordV1;
using esphome::idm_bridge::CalibrationRecordV2;
using esphome::idm_bridge::CalibrationStorageCore;
using esphome::idm_bridge::IdmAnalogCalibration;
using esphome::idm_bridge::calibration_equal;

template<typename T> static void refresh_crc(T *record) {
  record->crc32 =
      CalibrationStorageCore::crc32(record, offsetof(T, crc32));
}

int main() {
  static constexpr char CRC_REFERENCE[] = "123456789";
  assert(CalibrationStorageCore::crc32(CRC_REFERENCE, 9) == 0xCBF43926U);

  const IdmAnalogCalibration factory{};
  const CalibrationRecordV2 empty_current{};
  const CalibrationRecordV1 empty_legacy{};

  const auto first_boot = CalibrationStorageCore::load(
      factory, false, empty_current, false, empty_legacy);
  assert(first_boot.status == CalibrationLoadStatus::FACTORY_DEFAULTS);
  assert(calibration_equal(first_boot.calibration, factory));
  assert(!first_boot.migration_required);

  const IdmAnalogCalibration custom{
      100, 3900, 700.0f, 3200.0f, true,
  };
  const auto current_record = CalibrationStorageCore::make_record(custom);
  IdmAnalogCalibration decoded{};
  assert(CalibrationStorageCore::decode_record(current_record, &decoded));
  assert(calibration_equal(decoded, custom));

  const auto stored = CalibrationStorageCore::load(
      factory, true, current_record, false, empty_legacy);
  assert(stored.status == CalibrationLoadStatus::STORED_V2);
  assert(calibration_equal(stored.calibration, custom));

  auto corrupted = current_record;
  corrupted.crc32 ^= 0x00010000U;
  assert(!CalibrationStorageCore::decode_record(corrupted, &decoded));

  auto out_of_range = current_record;
  out_of_range.humidity_code_max = 5000;
  refresh_crc(&out_of_range);
  assert(!CalibrationStorageCore::decode_record(out_of_range, &decoded));

  auto unknown_version = current_record;
  unknown_version.version = 3;
  refresh_crc(&unknown_version);
  assert(!CalibrationStorageCore::decode_record(unknown_version, &decoded));

  auto unknown_flags = current_record;
  unknown_flags.flags = 0x2U;
  refresh_crc(&unknown_flags);
  assert(!CalibrationStorageCore::decode_record(unknown_flags, &decoded));

  const auto legacy = CalibrationStorageCore::make_legacy_record(
      38.0f, 100.0f, 700.0f, 3200.0f, true);
  IdmAnalogCalibration migrated{};
  assert(CalibrationStorageCore::migrate_record(legacy, &migrated));
  assert(migrated.humidity_code_min == 100);
  assert(migrated.humidity_code_max == 3900);
  assert(migrated.temperature_resistance_min == 700.0f);
  assert(migrated.temperature_resistance_max == 3200.0f);
  assert(migrated.temperature_code_inverted);

  const auto migration = CalibrationStorageCore::load(
      factory, false, empty_current, true, legacy);
  assert(migration.status == CalibrationLoadStatus::MIGRATED_V1);
  assert(migration.migration_required);
  assert(calibration_equal(migration.calibration, migrated));

  const auto invalid_legacy = CalibrationStorageCore::make_legacy_record(
      -1.0f, 100.0f, 700.0f, 3200.0f, false);
  const auto rejected_legacy = CalibrationStorageCore::load(
      factory, false, empty_current, true, invalid_legacy);
  assert(rejected_legacy.status ==
         CalibrationLoadStatus::INVALID_USING_FACTORY);
  assert(calibration_equal(rejected_legacy.calibration, factory));

  // A corrupt current record must not resurrect an older, otherwise valid
  // calibration.
  const auto no_downgrade = CalibrationStorageCore::load(
      factory, true, corrupted, true, legacy);
  assert(no_downgrade.status ==
         CalibrationLoadStatus::INVALID_USING_FACTORY);
  assert(calibration_equal(no_downgrade.calibration, factory));

  IdmAnalogCalibration invalid_factory = factory;
  invalid_factory.temperature_resistance_max = 50.0f;
  const auto safe_factory = CalibrationStorageCore::load(
      invalid_factory, false, empty_current, false, empty_legacy);
  assert(calibration_equal(safe_factory.calibration, factory));

  return 0;
}

#include <cassert>
#include <cmath>
#include <string>

#include "firmware/esp-idf/main/native_runtime.h"

using esphome::idm_bridge::BridgeState;
using esphome::idm_bridge::IdmAnalogCalibration;
using idm::native::NativeRuntime;
using idm::native::NativeRuntimeConfig;

static bool close_to(float actual, float expected,
                     float tolerance = 0.001f) {
  return std::fabs(actual - expected) < tolerance;
}

int main() {
  NativeRuntime runtime;
  runtime.configure(NativeRuntimeConfig{
      80.0f,
      28.0f,
      1000,
      50,
  });
  const IdmAnalogCalibration factory{};
  runtime.boot(100, factory, factory);

  auto diagnostics = runtime.diagnostics(100);
  assert(std::string(diagnostics.bridge_state) == "startup_safe");
  assert(diagnostics.safe_active);
  assert(runtime.output_dirty());
  assert(diagnostics.output.humidity_dac_code == 3276);
  assert(diagnostics.output.temperature_digipot_code == 109);

  runtime.record_output_result(true);
  assert(!runtime.output_dirty());

  assert(runtime.accept_command(55.0f, 23.0f, 90, "http", 200));
  diagnostics = runtime.diagnostics(250);
  assert(std::string(diagnostics.bridge_state) == "active");
  assert(!diagnostics.safe_active);
  assert(diagnostics.command_source == "http");
  assert(diagnostics.command_status == "accepted");
  assert(diagnostics.command_quality == 90);
  assert(diagnostics.command_age_ms == 50);
  assert(close_to(diagnostics.dew_point, dew_point_c(23.0f, 55.0f)));

  runtime.record_output_result(false);
  diagnostics = runtime.diagnostics(300);
  assert(diagnostics.output_fault);
  assert(diagnostics.output_failures == 1);
  assert(std::string(diagnostics.bridge_state) ==
         "output_fault_safe");

  runtime.record_output_result(true);
  assert(runtime.bridge_state() == BridgeState::STARTUP_SAFE);
  assert(runtime.output_dirty());
  runtime.record_output_result(true);
  assert(!runtime.output_dirty());

  assert(!runtime.accept_command(50.0f, 20.0f, 49, "http", 400));
  diagnostics = runtime.diagnostics(400);
  assert(diagnostics.command_status == "below_minimum_quality");
  assert(std::string(diagnostics.bridge_state) == "manual_safe");
  assert(diagnostics.safe_active);

  assert(!runtime.accept_command(101.0f, 20.0f, 100, "http", 500));
  assert(runtime.diagnostics(500).command_status == "out_of_range");
  assert(!runtime.accept_command(50.0f, 20.0f, 100, "", 600));
  assert(runtime.diagnostics(600).command_status == "invalid_source");

  assert(runtime.accept_command(50.0f, 20.0f, 100, "http", 700));
  assert(!runtime.tick(1699));
  assert(runtime.tick(1700));
  diagnostics = runtime.diagnostics(1700);
  assert(diagnostics.stale);
  assert(diagnostics.command_status == "stale");

  const IdmAnalogCalibration calibrated{
      100,
      3900,
      700.0f,
      3200.0f,
      true,
  };
  assert(runtime.apply_calibration(calibrated));
  assert(runtime.output_dirty());
  assert(runtime.calibration().temperature_code_inverted);
  runtime.reset_calibration();
  assert(!runtime.calibration().temperature_code_inverted);

  const IdmAnalogCalibration invalid{
      3900,
      100,
      700.0f,
      3200.0f,
      false,
  };
  assert(!runtime.apply_calibration(invalid));

  runtime.force_fallback("operator");
  diagnostics = runtime.diagnostics(1800);
  assert(diagnostics.command_source == "operator");
  assert(diagnostics.command_status == "fallback_requested");
  assert(diagnostics.command_quality == 0);

  return 0;
}

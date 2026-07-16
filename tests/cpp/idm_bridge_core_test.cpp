#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "firmware/components/idm_bridge/idm_bridge_core.h"

using esphome::idm_bridge::BridgeError;
using esphome::idm_bridge::BridgeState;
using esphome::idm_bridge::IdmBridgeCore;

static bool close_to(float actual, float expected) {
  return std::fabs(actual - expected) < 0.0001f;
}

int main() {
  IdmBridgeCore bridge;
  bridge.configure(80.0f, 28.0f, 120000);
  bridge.reset(1000);

  assert(bridge.state() == BridgeState::STARTUP_SAFE);
  assert(bridge.safe_active());
  assert(!bridge.stale());
  assert(!bridge.fault());
  assert(close_to(bridge.values().humidity, 80.0f));
  assert(close_to(bridge.values().temperature, 28.0f));

  assert(bridge.set_values(55.0f, 23.0f, 2000));
  assert(bridge.state() == BridgeState::ACTIVE);
  assert(!bridge.safe_active());
  assert(close_to(bridge.normalized_humidity(), 0.55f));
  assert(close_to(bridge.normalized_temperature(), 0.5375f));
  assert(!bridge.tick(121999));
  assert(bridge.tick(122000));
  assert(bridge.state() == BridgeState::STALE_SAFE);
  assert(bridge.error() == BridgeError::STALE_INPUT);
  assert(close_to(bridge.values().humidity, 80.0f));

  assert(!bridge.set_values(std::numeric_limits<float>::quiet_NaN(), 20.0f,
                            130000));
  assert(bridge.state() == BridgeState::INVALID_SAFE);
  assert(bridge.error() == BridgeError::INVALID_HUMIDITY);
  assert(bridge.fault());

  assert(!bridge.set_values(50.0f, 61.0f, 140000));
  assert(bridge.error() == BridgeError::INVALID_TEMPERATURE);

  assert(bridge.set_values(50.0f, 20.0f, 150000));
  bridge.set_output_fault(true);
  assert(bridge.state() == BridgeState::OUTPUT_FAULT_SAFE);
  assert(bridge.error() == BridgeError::OUTPUT_FAILURE);
  assert(!bridge.set_values(45.0f, 19.0f, 151000));
  assert(bridge.state() == BridgeState::OUTPUT_FAULT_SAFE);
  bridge.apply_manual_fallback();
  assert(bridge.state() == BridgeState::OUTPUT_FAULT_SAFE);
  bridge.set_output_fault(false);
  assert(bridge.state() == BridgeState::STARTUP_SAFE);
  assert(!bridge.has_command());

  bridge.apply_manual_fallback();
  assert(bridge.state() == BridgeState::MANUAL_SAFE);
  assert(std::string(IdmBridgeCore::state_name(bridge.state())) ==
         "manual_safe");

  bridge.configure(80.0f, 28.0f, 100);
  bridge.reset(UINT32_MAX - 49);
  assert(bridge.set_values(40.0f, 10.0f, UINT32_MAX - 49));
  assert(!bridge.tick(49));
  assert(bridge.tick(50));
  assert(bridge.state() == BridgeState::STALE_SAFE);

  IdmBridgeCore boundaries;
  boundaries.configure(80.0f, 28.0f, 1000);
  boundaries.reset(0);
  assert(boundaries.output_dirty());
  boundaries.clear_output_dirty();
  assert(!boundaries.output_dirty());
  assert(boundaries.set_values(0.0f, -20.0f, 1));
  assert(close_to(boundaries.normalized_humidity(), 0.0f));
  assert(close_to(boundaries.normalized_temperature(), 0.0f));
  assert(boundaries.output_dirty());
  boundaries.clear_output_dirty();
  assert(boundaries.set_values(100.0f, 60.0f, 2));
  assert(close_to(boundaries.normalized_humidity(), 1.0f));
  assert(close_to(boundaries.normalized_temperature(), 1.0f));
  assert(!boundaries.set_values(-0.01f, 60.01f, 3));
  assert(boundaries.error() == BridgeError::INVALID_VALUES);
  assert(!boundaries.set_values(
      std::numeric_limits<float>::infinity(), 20.0f, 4));
  assert(boundaries.error() == BridgeError::INVALID_HUMIDITY);
  assert(!boundaries.set_values(
      50.0f, -std::numeric_limits<float>::infinity(), 5));
  assert(boundaries.error() == BridgeError::INVALID_TEMPERATURE);

  IdmBridgeCore no_timeout;
  no_timeout.configure(80.0f, 28.0f, 0);
  no_timeout.reset(10);
  assert(no_timeout.set_values(50.0f, 20.0f, 10));
  assert(!no_timeout.tick(UINT32_MAX));
  assert(no_timeout.state() == BridgeState::ACTIVE);

  IdmBridgeCore fallback_validation;
  fallback_validation.configure(-1.0f, 100.0f, 1000);
  fallback_validation.reset(0);
  assert(close_to(fallback_validation.values().humidity, 80.0f));
  assert(close_to(fallback_validation.values().temperature, 28.0f));
  fallback_validation.configure(0.0f, -20.0f, 1000);
  fallback_validation.reset(0);
  assert(close_to(fallback_validation.values().humidity, 0.0f));
  assert(close_to(fallback_validation.values().temperature, -20.0f));

  assert(std::string(IdmBridgeCore::state_name(
             BridgeState::OUTPUT_FAULT_SAFE)) == "output_fault_safe");
  assert(std::string(IdmBridgeCore::error_name(
             BridgeError::INVALID_VALUES)) == "invalid_values");

  return 0;
}

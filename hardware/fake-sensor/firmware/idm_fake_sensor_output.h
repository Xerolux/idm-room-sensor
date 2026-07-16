#pragma once

// Compatibility alias for early sketches. The maintained implementation is
// the ESPHome external component configured through `idm_bridge.analog_output`.
#include "esphome/components/idm_bridge/idm_analog_output.h"

using IdmFakeSensorOutput = esphome::idm_bridge::IdmAnalogOutput;

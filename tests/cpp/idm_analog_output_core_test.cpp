#include <cassert>
#include <cmath>

#include "firmware/components/idm_bridge/idm_analog_output_core.h"

using esphome::idm_bridge::IdmAnalogCalibration;
using esphome::idm_bridge::IdmAnalogOutputCore;
using esphome::idm_bridge::KTY81_210_PROTOTYPE_TABLE;

static bool close_to(float actual, float expected) {
  return std::fabs(actual - expected) < 0.001f;
}

int main() {
  IdmAnalogOutputCore output;

  assert(output.humidity_code(-10.0f) == 0);
  assert(output.humidity_code(0.0f) == 0);
  assert(output.humidity_code(50.0f) == 2048);
  assert(output.humidity_code(100.0f) == 4095);
  assert(output.humidity_code(110.0f) == 4095);

  output.set_humidity_code_range(100, 3900);
  assert(output.humidity_code(0.0f) == 100);
  assert(output.humidity_code(50.0f) == 2000);
  assert(output.humidity_code(100.0f) == 3900);
  assert(output.humidity_code(-1.0f) == 100);
  assert(output.humidity_code(101.0f) == 3900);

  output.set_humidity_code_range(101, 3900);
  assert(output.humidity_code(50.0f) == 2001);
  output.set_humidity_code_range(100, 3900);

  constexpr size_t kty_count =
      sizeof(KTY81_210_PROTOTYPE_TABLE) /
      sizeof(KTY81_210_PROTOTYPE_TABLE[0]);
  for (size_t index = 0; index < kty_count; index++) {
    assert(close_to(
        output.resistance_for_temperature(
            KTY81_210_PROTOTYPE_TABLE[index].temperature),
        KTY81_210_PROTOTYPE_TABLE[index].resistance));
    if (index > 0) {
      assert(KTY81_210_PROTOTYPE_TABLE[index].temperature >
             KTY81_210_PROTOTYPE_TABLE[index - 1].temperature);
      assert(KTY81_210_PROTOTYPE_TABLE[index].resistance >
             KTY81_210_PROTOTYPE_TABLE[index - 1].resistance);
      const float midpoint_temperature =
          (KTY81_210_PROTOTYPE_TABLE[index - 1].temperature +
           KTY81_210_PROTOTYPE_TABLE[index].temperature) /
          2.0f;
      const float midpoint_resistance =
          (KTY81_210_PROTOTYPE_TABLE[index - 1].resistance +
           KTY81_210_PROTOTYPE_TABLE[index].resistance) /
          2.0f;
      assert(close_to(
          output.resistance_for_temperature(midpoint_temperature),
          midpoint_resistance));
    }
  }

  assert(close_to(output.resistance_for_temperature(-30.0f), 684.0f));
  assert(close_to(output.resistance_for_temperature(15.0f), 1300.0f));
  assert(close_to(output.resistance_for_temperature(25.0f), 1560.0f));
  assert(close_to(output.resistance_for_temperature(70.0f), 2760.0f));

  output.set_temperature_resistance_range(650.0f, 3000.0f);
  assert(output.temperature_code(-20.0f) == 4);
  assert(output.temperature_code(25.0f) == 99);
  assert(output.temperature_code(60.0f) == 229);

  output.set_temperature_code_inverted(true);
  assert(output.temperature_code(25.0f) == 156);

  output.set_temperature_code_inverted(false);
  output.set_temperature_resistance_range(1000.0f, 2000.0f);
  assert(output.temperature_code(-20.0f) == 0);
  assert(output.temperature_code(60.0f) == 255);
  output.set_temperature_code_inverted(true);
  assert(output.temperature_code(-20.0f) == 255);
  assert(output.temperature_code(60.0f) == 0);

  const IdmAnalogCalibration previous = output.calibration();
  const IdmAnalogCalibration invalid{
      3900, 100, 700.0f, 3200.0f, false,
  };
  assert(!output.set_calibration(invalid));
  assert(output.calibration().humidity_code_min ==
         previous.humidity_code_min);
  assert(output.calibration().humidity_code_max ==
         previous.humidity_code_max);

  const IdmAnalogCalibration calibrated{
      200, 3800, 700.0f, 3200.0f, false,
  };
  assert(output.set_calibration(calibrated));
  assert(output.humidity_code(0.0f) == 200);
  assert(output.humidity_code(50.0f) == 2000);
  assert(output.humidity_code(100.0f) == 3800);

  return 0;
}

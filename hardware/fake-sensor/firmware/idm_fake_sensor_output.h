#pragma once
#include "esphome.h"
#include <Wire.h>

class IdmFakeSensorOutput : public Component {
 public:
  float humidity = 80.0f;
  float temperature = 28.0f;

  static constexpr uint8_t MCP4725_ADDR = 0x60;
  static constexpr uint8_t AD5242_ADDR  = 0x2C;

  // Approximate KTY81-210 table. Replace with values from the exact datasheet
  // and calibrate against the actual IDM input.
  struct Point { float t_c; float r_ohm; };
  static constexpr Point kty_table[] = {
    {-20,  684}, {-10,  815}, {0,  980}, {10, 1180},
    {20, 1420}, {25, 1560}, {30, 1710}, {40, 2020},
    {50, 2370}, {60, 2760}
  };

  void setup() override {
    Wire.begin();
    apply_outputs();
  }

  void set_values(float rh, float temp) {
    humidity = clamp(rh, 0.0f, 100.0f);
    temperature = clamp(temp, -20.0f, 60.0f);
    apply_outputs();
  }

  float resistance_for_temp(float t) {
    if (t <= kty_table[0].t_c) return kty_table[0].r_ohm;
    const size_t n = sizeof(kty_table) / sizeof(kty_table[0]);
    for (size_t i = 1; i < n; i++) {
      if (t <= kty_table[i].t_c) {
        const float f = (t - kty_table[i-1].t_c) /
                        (kty_table[i].t_c - kty_table[i-1].t_c);
        return kty_table[i-1].r_ohm +
               f * (kty_table[i].r_ohm - kty_table[i-1].r_ohm);
      }
    }
    return kty_table[n-1].r_ohm;
  }

  void write_mcp4725(uint16_t code) {
    code &= 0x0FFF;
    Wire.beginTransmission(MCP4725_ADDR);
    Wire.write(0x40);
    Wire.write(code >> 4);
    Wire.write((code & 0x0F) << 4);
    Wire.endTransmission();
  }

  void write_ad5242(uint8_t code) {
    Wire.beginTransmission(AD5242_ADDR);
    Wire.write(0x00);
    Wire.write(code);
    Wire.endTransmission();
  }

  void apply_outputs() {
    const uint16_t dac = (uint16_t) lroundf(humidity / 100.0f * 4095.0f);
    write_mcp4725(dac);

    const float target_r = resistance_for_temp(temperature);

    // Prototype mapping for a 10 kΩ digipot plus fixed network.
    // Replace with calibration fit from measured resistance vs code.
    const float r_min = 650.0f;
    const float r_max = 3000.0f;
    const float norm = clamp((target_r - r_min) / (r_max - r_min), 0.0f, 1.0f);
    write_ad5242((uint8_t) lroundf(norm * 255.0f));
  }
};

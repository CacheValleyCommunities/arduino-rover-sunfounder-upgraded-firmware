#include "hardware_spy.h"

#include <cstring>

HardwareSpy hw;

void HardwareSpy::reset() {
  millis_ms = 0;
  pulse_us = 0;
  pwmWrites = 0;
  servoAngle = 0;
  servoPin = 255;
  lampLevel = -1;
  sent.clear();
  std::memset(digital, 0, sizeof(digital));
  std::memset(analog, 0, sizeof(analog));
  std::memset(pwm, 0, sizeof(pwm));
  for (int i = 0; i < 32; i++) {
    digital[i] = 1; // HIGH: IR idle = not detected
  }
}

void hwReset() { hw.reset(); }

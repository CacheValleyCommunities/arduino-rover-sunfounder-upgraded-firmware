#ifndef HARDWARE_SPY_H
#define HARDWARE_SPY_H

#include <stdint.h>
#include <vector>

struct HardwareSpy {
  unsigned long millis_ms = 0;
  int digital[32] = {};
  int analog[32] = {};
  unsigned long pulse_us = 0;
  int pwm[32] = {};
  uint32_t pwmWrites = 0;
  uint8_t servoAngle = 0;
  uint8_t servoPin = 255;
  int lampLevel = -1;
  std::vector<uint8_t> sent;

  void reset();
};

extern HardwareSpy hw;

void hwReset();

#endif

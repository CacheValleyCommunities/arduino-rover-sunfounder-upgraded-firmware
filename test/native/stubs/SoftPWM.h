#ifndef SOFTPWM_H
#define SOFTPWM_H

#include "Arduino.h"
#include "hardware_spy.h"

inline void SoftPWMBegin() {}

inline void SoftPWMSet(uint8_t pin, uint8_t value) {
  analogWrite(pin, value);
}

inline void SoftPWMSetFadeTime(uint8_t, uint8_t, uint8_t) {}

#endif

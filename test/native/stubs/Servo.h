#ifndef Servo_h
#define Servo_h

#include "Arduino.h"
#include "hardware_spy.h"

class Servo {
public:
  void attach(uint8_t pin) { hw.servoPin = pin; }
  void write(uint8_t angle) { hw.servoAngle = angle; }
};

#endif

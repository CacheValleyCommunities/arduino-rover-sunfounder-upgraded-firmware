#pragma once

#include <Arduino.h>
#include "car_control.h"
#include "rgb.h"
#include "sensors.h"

struct RoverDrive {
  void setMotors(int8_t left, int8_t right) { carSetMotors(left, right); }
  void forward(int8_t power) { carForward(power); }
  void backward(int8_t power) { carBackward(power); }
  void turnLeft(int8_t power) { carTurnLeft(power); }
  void turnRight(int8_t power) { carTurnRight(power); }
  void backLeft(int8_t power) { carBackLeft(power); }
  void backRight(int8_t power) { carBackRight(power); }
  void stop() { carStop(); }
};

struct RoverLights {
  void set(uint32_t color) { rgbWrite(color); }
  void set(uint8_t r, uint8_t g, uint8_t b) { rgbWrite(r, g, b); }
  void off() { rgbOff(); }
};

struct RoverSensors {
  float distanceCm() { return ultrasonicRead(); }
  bool wallAhead() { return ultrasonicIsObstacle(); }
  bool obstacleLeft() { return irObstacleIsLeftDetected(); }
  bool obstacleRight() { return irObstacleIsRightDetected(); }
  uint8_t irBits() { return irObstacleRead(); }
  float batteryVolts() { return batteryGetVoltage(); }
  uint8_t batteryPercent() { return batteryGetPercentage(); }
};

class Rover {
public:
  RoverDrive drive;
  RoverLights lights;
  RoverSensors sensors;

  void begin();
  void look(uint8_t angle);
  void cameraLamp(bool on);
  void follow();
  void avoid();
};

extern Rover rover;

#ifdef UNIT_TEST
void roverResetObstacleForTest();
#endif

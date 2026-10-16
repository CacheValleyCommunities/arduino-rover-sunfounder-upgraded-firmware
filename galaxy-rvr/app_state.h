#pragma once

#include <Arduino.h>
#include "rgb.h"

enum AppState : uint8_t {
  STATE_IDLE = 0,
  STATE_APP = 1,
};

enum AppMode : uint8_t {
  MODE_NONE = 0,
  MODE_OBSTACLE_FOLLOWING = 1,
  MODE_OBSTACLE_AVOIDANCE = 2,
  MODE_APP_CONTROL = 3,
};

#define COLOR_STATE_IDLE GREEN
#define COLOR_STATE_APP MAGENTA

extern uint8_t currentState;
extern uint8_t currentMode;
extern int8_t leftMotorPower;
extern int8_t rightMotorPower;
extern uint8_t servoAngle;
extern uint8_t obstaclePower;

void appStateUpdate(bool wsConnected);
void modeHandler();

#ifdef UNIT_TEST
void appStateResetForTest();
#endif

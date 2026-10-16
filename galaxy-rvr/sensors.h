#pragma once

#include <Arduino.h>
#include "defaults.h"
#include "pins.h"

void ultrasonicUpdate();
float ultrasonicRead();
bool ultrasonicIsObstacle();
bool ultrasonicIsClear();

void irObstacleBegin();
uint8_t irObstacleRead();
bool irObstacleIsLeftDetected();
bool irObstacleIsRightDetected();

void batteryBegin();
float batteryGetVoltage();
uint8_t batteryGetPercentage();

#ifdef UNIT_TEST
void ultrasonicSetDistance(float cm);
void ultrasonicResetForTest();
#endif

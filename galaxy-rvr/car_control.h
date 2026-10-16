#pragma once

#include <Arduino.h>
#include "pins.h"

void carBegin();
void carSetMotors(int8_t power0, int8_t power1);
void carForward(int8_t power);
void carBackward(int8_t power);
void carTurnLeft(int8_t power);
void carTurnRight(int8_t power);
void carBackLeft(int8_t power);
void carBackRight(int8_t power);
void carStop();

#ifdef UNIT_TEST
void carGetLastPowers(int8_t *left, int8_t *right);
void carResetForTest();
#endif

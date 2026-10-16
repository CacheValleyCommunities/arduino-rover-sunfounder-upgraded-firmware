#pragma once

#include <Arduino.h>

void c2RuntimeLink();
void c2RuntimeMotors(int left, int right);
void c2RuntimeRgb(int r, int g, int b);
void c2RuntimeLamp(bool on);
void c2RuntimeServo(int angle);
void c2RuntimeSetPower(int power);
float c2RuntimeDistance();
bool c2RuntimeIrLeft();
bool c2RuntimeIrRight();
float c2RuntimeBattery();
int c2RuntimeSetting(const char *key);

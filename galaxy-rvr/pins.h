#pragma once

#include <Arduino.h>

#define MOTOR_PINS (uint8_t[4]){2, 3, 4, 5}
#define MOTOR_DIRECTIONS (uint8_t[2]){0, 1}

#define SERVO_PIN 6

#define IR_RIGHT_PIN 7
#define IR_LEFT_PIN 8

#define ULTRASONIC_TRIG_PIN 10
#define ULTRASONIC_ECHO_PIN 10

#define RGB_PINS (uint8_t[3]){12, 13, 11}

#define BATTERY_PIN A3

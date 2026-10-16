#pragma once

#include <Arduino.h>
#include "defaults.h"

#define WS_DATA_START_BIT 0xA0
#define WS_DATA_END_BIT 0xA1

#define ENTITY_MOTORS 0x01
#define ENTITY_RGB 0x02
#define ENTITY_SERVO 0x03
#define ENTITY_LAMP 0x04
#define ENTITY_OBSTACLE 0x05
#define ENTITY_CONFIG 0x07

#define TELEMETRY_ULTRASONIC 0x81
#define TELEMETRY_IR 0x82
#define TELEMETRY_BATTERY 0x83
#define TELEMETRY_CONFIG 0x85

#define CFG_FOLLOW_STOP_CM 1
#define CFG_FOLLOW_SLOW_CM 2
#define CFG_FOLLOW_DISTANCE_CM 3
#define CFG_FOLLOW_SLOW_POWER 4
#define CFG_AVOID_THRESHOLD_CM 5
#define CFG_REVERSE_CM 6
#define CFG_MOVE_POWER 7
#define CFG_MS_PER_FOOT 8
#define CFG_MS_PER_DEG 9

#define CFG_ID_COUNT 9
#define CFG_TELEMETRY_BYTES (CFG_ID_COUNT * 2)

extern int16_t followStopCm;
extern int16_t followSlowCm;
extern int16_t followDistanceCm;
extern int16_t followSlowPower;
extern int16_t avoidThresholdCm;
extern int16_t reverseCm;
extern int16_t movePower;
extern int16_t msPerFoot;
extern int16_t msPerDeg;

void roverConfigReset();
bool roverConfigSet(uint8_t id, int16_t value);
int16_t roverConfigGet(uint8_t id);
void roverConfigWriteTelemetry(uint8_t *dest);

void onReceive();
void handleSensorData();

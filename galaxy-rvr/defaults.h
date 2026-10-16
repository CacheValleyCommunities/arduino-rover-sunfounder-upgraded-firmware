#pragma once

#include <Arduino.h>

#define WATCH_DOG 0
#if WATCH_DOG
#include <avr/wdt.h>
#endif

#define MEM 0
#if MEM
#include <MemoryFree.h>
#include <pgmStrToRAM.h>
#endif

#define DEBUG_WS 0

#ifndef AP_SSID
#define AP_SSID "GalaxyRVR"
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "12345678"
#endif
#define NAME "GalaxyRVR"
#define TYPE "GalaxyRVR"
#define PORT "30102"

#define DEFAULT_OBSTACLE_POWER 65
#define FOLLOW_STOP_CM 12
#define FOLLOW_SLOW_CM 25
#define FOLLOW_DISTANCE 50
#define FOLLOW_SLOW_POWER 30

#define SERVO_CENTER_ANGLE 90
#define SERVO_MAX_ANGLE 140

#define ULTRASONIC_AVOIDANCE_THRESHOLD 35
#define ULTRASONIC_REVERSE_CM 15
#define MAX_DISTANCE 300
#define ULTRASONIC_READ_TIMEOUT 18000

#define ANALOG_REFERENCE_VOLTAGE 5.246

#define DEFAULT_MOVE_POWER 70
#define DEFAULT_MS_PER_FOOT 1800
#define DEFAULT_MS_PER_DEG 8

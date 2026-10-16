#pragma once

#include <Arduino.h>
#include "pins.h"

#define COMMON_ANODE 0

#define WHITE 0xFFFFFF
#define RED 0xFF0202
#define ORANGE 0xFFA500
#define YELLOW 0xFFFF0A
#define YELLOW_GREEN 0xA5FF0A
#define GREEN 0x0AFF0A
#define GREEN_CYAN 0x0AFFA5
#define CYAN 0x0AFFFF
#define CYAN_BLUE 0x0AA5FF
#define BLUE 0x0A0AFF
#define PURPLE 0xA50AFF
#define VOILET 0xFF0AFF
#define MAGENTA 0xFF0AA5

#define R_OFFSET 1.0
#define G_OFFSET 0.25
#define B_OFFSET 0.45

void rgbBegin();
void rgbWrite(uint32_t color);
void rgbWrite(uint8_t r, uint8_t g, uint8_t b);
void rgbOff();

#pragma once

#include <Arduino.h>

#define WS_BIN_START_BYTE 0xA0
#define WS_BIN_END_BYTE 0xA1

void serialBridgeForwardToUno(const uint8_t *data, int len);
size_t serialBridgeRead(uint8_t *buf, size_t cap);
void serialBridgeFanoutBinary(uint8_t *payload, size_t length);

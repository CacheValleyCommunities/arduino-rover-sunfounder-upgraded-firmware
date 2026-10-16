#pragma once

#include <Arduino.h>
#include <WebServer.h>

void c2Begin(WebServer &server);
void c2Loop();
void c2OnUnoTelemetry(const uint8_t *data, size_t len);
void c2ResetForTest();

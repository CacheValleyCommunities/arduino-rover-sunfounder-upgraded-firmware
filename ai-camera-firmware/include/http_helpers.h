#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

void httpBind(WebServer &server);
void httpSetCors();
void httpSendGzip(const char *mime, const uint8_t *data, size_t len);
void httpSendJsonOk();
void httpSendJson(const String &body);
void httpSendText(int code, const char *mime, const String &body);
void httpSendNoContent();
void httpSendCloseHeader();

#ifndef CAM_FIRMWARE_TEST_H
#define CAM_FIRMWARE_TEST_H

#include "Arduino.h"
#include "WebServer.h"
#include "WiFiUdp.h"
#include "app_session.h"
#include "c2_program.h"
#include "c2_server.h"
#include "http_helpers.h"
#include "identity.h"
#include "led_status.h"
#include "serial_bridge.h"
#include "udp_server.h"
#include "gtest/gtest.h"

#include <ArduinoJson.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

extern WebServer camHttp;
extern UDP_Server udpServer;
extern String videoUrl;
void resetCamFirmware();

inline std::vector<std::vector<uint8_t>> unoPayloads() {
  const auto &tx = Serial.tx;
  std::vector<std::vector<uint8_t>> out;
  for (size_t i = 0; i + 7 < tx.size(); i++) {
    if (tx[i] == 'W' && tx[i + 1] == 'S' && tx[i + 2] == 'B' &&
        tx[i + 3] == '+' && tx[i + 4] == WS_BIN_START_BYTE) {
      uint8_t len = tx[i + 5];
      size_t payload = i + 7;
      if (payload + len < tx.size() &&
          tx[payload + len] == WS_BIN_END_BYTE) {
        out.emplace_back(tx.begin() + (long)payload,
                         tx.begin() + (long)payload + len);
        i = payload + len;
      }
    }
  }
  return out;
}

inline bool frameMatches(const std::vector<uint8_t> &f, uint8_t id,
                         std::initializer_list<uint8_t> rest) {
  if (f.empty() || f[0] != id || f.size() != 1 + rest.size()) {
    return false;
  }
  size_t i = 1;
  for (uint8_t b : rest) {
    if (f[i++] != b) {
      return false;
    }
  }
  return true;
}

inline bool lastEntityIs(uint8_t id, std::initializer_list<uint8_t> rest) {
  auto frames = unoPayloads();
  return !frames.empty() && frameMatches(frames.back(), id, rest);
}

inline bool hasEntity(uint8_t id, std::initializer_list<uint8_t> rest) {
  for (const auto &f : unoPayloads()) {
    if (frameMatches(f, id, rest)) {
      return true;
    }
  }
  return false;
}

inline std::vector<uint8_t> makeTelemetry(float cm, bool irLeft, bool irRight,
                                          float volts,
                                          const int16_t cfg[9] = nullptr) {
  int16_t defaults[9] = {12, 25, 50, 30, 35, 15, 70, 1800, 8};
  const int16_t *src = cfg ? cfg : defaults;
  std::vector<uint8_t> p(30, 0);
  p[0] = 0xA0;
  p[1] = 26;
  p[3] = 0x81;
  uint16_t mm = (uint16_t)std::lround(cm * 10.0f);
  p[4] = (uint8_t)(mm >> 8);
  p[5] = (uint8_t)(mm & 0xFF);
  p[6] = 0x82;
  p[7] = (uint8_t)((irLeft ? 0b10 : 0) | (irRight ? 0b01 : 0));
  p[8] = 0x83;
  p[9] = (uint8_t)std::lround((volts - 6.0f) * 100.0f);
  p[10] = 0x85;
  for (int i = 0; i < 9; i++) {
    p[11 + i * 2] = (uint8_t)((src[i] >> 8) & 0xFF);
    p[12 + i * 2] = (uint8_t)(src[i] & 0xFF);
  }
  p[29] = 0xA1;
  uint8_t cs = 0;
  for (size_t i = 0; i + 1 < p.size(); i++) {
    cs ^= p[i];
  }
  p[2] = cs;
  return p;
}

inline bool postJson(const char *uri, const char *body) {
  camHttp.setPlain(body);
  return camHttp.dispatch(uri, HTTP_POST);
}

inline bool getUri(const char *uri) { return camHttp.dispatch(uri, HTTP_GET); }

inline JsonDocument stateJson() {
  getUri("/c2/api/state");
  JsonDocument doc;
  deserializeJson(doc, camHttp.lastBodyString().c_str());
  return doc;
}

class CamTest : public ::testing::Test {
protected:
  void SetUp() override { resetCamFirmware(); }
};

#endif

#include "serial_bridge.h"

#include "c2_server.h"
#include "defaults.h"
#include "udp_server.h"
#include "ws_server.h"

#include <string.h>

extern UDP_Server udpServer;
extern WS_Server wsServer;

static bool isFramedBinaryPayload(const uint8_t *data, int len) {
  return len >= 5 && data[0] == WS_BIN_START_BYTE &&
         data[len - 1] == WS_BIN_END_BYTE;
}

void serialBridgeForwardToUno(const uint8_t *data, int len) {
  Serial.print("WSB+");
  if (data == nullptr || len <= 0) {
    Serial.println();
    return;
  }
  if (isFramedBinaryPayload(data, len) || len > 255) {
    Serial.write(data, len);
  } else {
    uint8_t checksum = 0;
    for (int i = 0; i < len; i++) {
      checksum ^= data[i];
    }
    Serial.write((uint8_t)WS_BIN_START_BYTE);
    Serial.write((uint8_t)len);
    Serial.write(checksum);
    Serial.write(data, len);
    Serial.write((uint8_t)WS_BIN_END_BYTE);
  }
  Serial.println();
}

void serialBridgeFanoutBinary(uint8_t *payload, size_t length) {
  wsServer.sendBIN(payload, length);
  udpServer.sendBIN(payload, length);
  c2OnUnoTelemetry(payload, length);
}

size_t serialBridgeRead(uint8_t *buf, size_t cap) {
  if (buf == nullptr || cap == 0 || !Serial.available()) {
    return 0;
  }

  const uint8_t LegacyStart = 0x0C;
  size_t n = 0;
  bool started = false;
  bool legacyBin = false;
  uint8_t legacyCount = 0;
  uint8_t legacyDataLen = 3;
  bool wsbFramed = false;
  size_t wsbNeed = 0;

  uint32_t charTime = millis();
  while (n < cap && (millis() - charTime) < CHAR_TIMEOUT) {
    if (!Serial.available()) {
      delay(1);
      continue;
    }
    uint8_t b = (uint8_t)Serial.read();

    if (!started) {
      if (b == 0xff || b == '\r' || b == '\n') {
        continue;
      }
      started = true;
      if (b == LegacyStart) {
        legacyBin = true;
      }
    }

    if (legacyBin) {
      legacyCount++;
      if (legacyCount == 2) {
        legacyDataLen = b;
      }
      buf[n++] = b;
      charTime = millis();
      if (legacyCount >= legacyDataLen) {
        break;
      }
      continue;
    }

    if (!wsbFramed && n >= 4 && memcmp(buf, "WSB+", 4) == 0) {
      if (b == WS_BIN_START_BYTE) {
        wsbFramed = true;
        buf[n++] = b;
        charTime = millis();
        continue;
      }
    }

    if (wsbFramed) {
      buf[n++] = b;
      charTime = millis();
      if (n == 6) {
        wsbNeed = 8u + (size_t)buf[5];
        if (wsbNeed > cap) {
          wsbNeed = cap;
        }
      }
      if (wsbNeed > 0 && n >= wsbNeed) {
        break;
      }
      continue;
    }

    if (b == '\n') {
      break;
    }
    if (b == '\r') {
      charTime = millis();
      continue;
    }
    buf[n++] = b;
    charTime = millis();
  }

  return n;
}

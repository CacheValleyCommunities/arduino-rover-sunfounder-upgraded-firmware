#ifndef SUNFOUNDER_AI_CAMERA_H
#define SUNFOUNDER_AI_CAMERA_H

#include "Arduino.h"
#include "hardware_spy.h"

#include <string.h>

#ifndef AI_CAM_RECV_MAX
#define AI_CAM_RECV_MAX 256
#endif

class AiCamera {
public:
  uint8_t recvBuffer[AI_CAM_RECV_MAX] = {};
  int recvBufferLength = 0;
  bool ws_connected = false;

  AiCamera(const char *, const char *) {}

  void begin(const char *, const char *, const char *, bool) {}
  void loop() {}
  void setOnReceivedBinary(void (*)()) {}

  void sendBinaryData(uint8_t *data, size_t len) {
    hw.sent.assign(data, data + len);
  }

  void lamp_on(uint8_t level) { hw.lampLevel = (int)level; }
  void lamp_off() { hw.lampLevel = -1; }

  void loadRecv(const uint8_t *data, int length) {
    recvBufferLength = length;
    if (length > AI_CAM_RECV_MAX) {
      recvBufferLength = AI_CAM_RECV_MAX;
    }
    memcpy(recvBuffer, data, (size_t)recvBufferLength);
  }
};

#endif

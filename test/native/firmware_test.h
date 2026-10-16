#ifndef FIRMWARE_TEST_FIXTURE_H
#define FIRMWARE_TEST_FIXTURE_H

#include "app_state.h"
#include "car_control.h"
#include "defaults.h"
#include "hardware_spy.h"
#include "pins.h"
#include "protocol.h"
#include "rover.h"
#include "sensors.h"
#include "gtest/gtest.h"

#include <Servo.h>
#include <SunFounder_AI_Camera.h>

#include <initializer_list>
#include <stdint.h>

extern AiCamera aiCam;
extern Servo servo;
void resetFirmware();

class FirmwareTest : public ::testing::Test {
protected:
  void SetUp() override { resetFirmware(); }

  void loadRecv(std::initializer_list<uint8_t> bytes) {
    uint8_t buf[AI_CAM_RECV_MAX];
    int n = 0;
    for (uint8_t b : bytes) {
      buf[n++] = b;
    }
    aiCam.loadRecv(buf, n);
  }

  void setIr(bool leftDetected, bool rightDetected) {
    hw.digital[IR_LEFT_PIN] = leftDetected ? LOW : HIGH;
    hw.digital[IR_RIGHT_PIN] = rightDetected ? LOW : HIGH;
  }
};

#endif

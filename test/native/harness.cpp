#include "app_state.h"
#include "car_control.h"
#include "defaults.h"
#include "hardware_spy.h"
#include "protocol.h"
#include "rover.h"
#include "sensors.h"

#include <Servo.h>
#include <SunFounder_AI_Camera.h>
#include <string.h>

AiCamera aiCam = AiCamera(NAME, TYPE);
Servo servo;

void resetFirmware() {
  hwReset();
  aiCam.recvBufferLength = 0;
  aiCam.ws_connected = false;
  memset(aiCam.recvBuffer, 0, sizeof(aiCam.recvBuffer));
  ultrasonicResetForTest();
  carResetForTest();
  roverResetObstacleForTest();
  appStateResetForTest();
}

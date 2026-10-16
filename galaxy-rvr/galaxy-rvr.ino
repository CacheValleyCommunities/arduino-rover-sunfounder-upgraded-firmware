/*******************************************************************
  The control program of the Arduino GalaxyRVR.

  Development test environment:
    - Arduino IDE 2.x
  Board tools:
    - Arduino AVR Boards 1.8.3
  Libraries:
    - SoftPWM
    - Servo
    - Sunfounder_AI_Camera

  Version: 2.0.0
    -- https://github.com/sunfounder/galaxy-rvr.git

  Author: Sunfounder
  Website: https://www.sunfounder.com
           https://docs.sunfounder.com

********************************************************************/
#define VERSION "2.0.0"

#include "defaults.h"
#include "pins.h"
#include "rover.h"
#include "app_state.h"
#include "protocol.h"

#include <Servo.h>
#include <SunFounder_AI_Camera.h>

AiCamera aiCam = AiCamera(NAME, TYPE);
Servo servo;

void setup() {
  uint32_t startMs = millis();
  Serial.begin(115200);
  Serial.print(F("GalaxyRVR version "));
  Serial.println(VERSION);

  Serial.println(F("Initializing..."));
  rover.begin();

  aiCam.begin(AP_SSID, AP_PASSWORD, PORT, false);
  aiCam.setOnReceivedBinary(onReceive);

  while (millis() - startMs < 500) {
    delay(1);
  }

#if WATCH_DOG
  wdt_disable();
  delay(3000);
  wdt_enable(WDTO_2S);
#endif

  Serial.println(F("Okie!"));
  rover.lights.set(GREEN);
}

void loop() {
  appStateUpdate(aiCam.ws_connected);
  if (currentState == STATE_APP) {
    ultrasonicUpdate();
    handleSensorData();
    modeHandler();
  }

  aiCam.loop();

#if WATCH_DOG
  wdt_reset();
#endif

#if MEM
  Serial.print(F("Free RAM = "));
  Serial.println(freeMemory());
#endif
}

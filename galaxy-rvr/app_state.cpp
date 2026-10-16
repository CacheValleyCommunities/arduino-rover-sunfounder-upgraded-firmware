#include "app_state.h"
#include "defaults.h"
#include "protocol.h"
#include "rover.h"

uint8_t currentState = STATE_IDLE;
uint8_t currentMode = MODE_NONE;
int8_t leftMotorPower = 0;
int8_t rightMotorPower = 0;
uint8_t servoAngle = SERVO_CENTER_ANGLE;
uint8_t obstaclePower = DEFAULT_OBSTACLE_POWER;

static uint8_t lastState = 255;
static uint8_t lastMode = 255;

static const uint32_t RGB_BLINK_INTERVAL_MS = 500;
static uint32_t rgbBlinkStartTime = 0;
static bool rgbBlinkOn = false;

static void handleStateEnter() {
  if (lastState == currentState) {
    return;
  }

  lastState = currentState;
  Serial.print(F("State changed to "));

  switch (currentState) {
  case STATE_IDLE:
    Serial.println(F("IDLE"));
    rover.lights.set(COLOR_STATE_IDLE);
    rover.drive.stop();
    rover.look(SERVO_CENTER_ANGLE);
    break;
  case STATE_APP:
    Serial.println(F("APP"));
    currentMode = MODE_APP_CONTROL;
    rover.lights.set(COLOR_STATE_APP);
    break;
  default:
    Serial.println(F("UNKNOWN"));
    break;
  }
}

static void runIdle(bool wsConnected) {
  if (millis() - rgbBlinkStartTime > RGB_BLINK_INTERVAL_MS) {
    rgbBlinkOn = !rgbBlinkOn;
    rgbBlinkStartTime = millis();
  }
  if (rgbBlinkOn) {
    rover.lights.set(COLOR_STATE_IDLE);
  } else {
    rover.lights.off();
  }
  if (wsConnected) {
    currentState = STATE_APP;
  }
}

void appStateUpdate(bool wsConnected) {
  handleStateEnter();

  switch (currentState) {
  case STATE_IDLE:
    runIdle(wsConnected);
    break;
  case STATE_APP:
    if (!wsConnected) {
      currentState = STATE_IDLE;
    }
    break;
  default:
    break;
  }

  handleStateEnter();
}

void modeHandler() {
  if (lastMode != currentMode) {
    Serial.print(F("Mode Change: "));
    Serial.print(lastMode);
    Serial.print(F(" -> "));
    Serial.println(currentMode);
    lastMode = currentMode;
  }

  switch (currentMode) {
  case MODE_OBSTACLE_FOLLOWING:
    rover.follow();
    break;
  case MODE_OBSTACLE_AVOIDANCE:
    rover.avoid();
    break;
  case MODE_APP_CONTROL:
    rover.drive.setMotors(leftMotorPower, rightMotorPower);
    break;
  default:
    break;
  }
}

#ifdef UNIT_TEST
void appStateResetForTest() {
  currentState = STATE_IDLE;
  currentMode = MODE_NONE;
  leftMotorPower = 0;
  rightMotorPower = 0;
  servoAngle = SERVO_CENTER_ANGLE;
  obstaclePower = DEFAULT_OBSTACLE_POWER;
  lastState = 255;
  lastMode = 255;
  rgbBlinkStartTime = 0;
  rgbBlinkOn = false;
  roverConfigReset();
}
#endif

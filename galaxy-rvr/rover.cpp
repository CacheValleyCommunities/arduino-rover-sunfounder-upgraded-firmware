#include "rover.h"
#include "app_state.h"
#include "defaults.h"
#include "pins.h"
#include "protocol.h"

#include <Servo.h>
#include <SunFounder_AI_Camera.h>
#if defined(ARDUINO_AVR_UNO)
#include <SoftPWM.h>
#endif

extern Servo servo;
extern AiCamera aiCam;

Rover rover;

static const uint8_t CAM_LAMP_LEVEL = 5;

enum ClearSide : int8_t {
  CLEAR_RIGHT = -1,
  CLEAR_LEFT = 1,
};

static int8_t lastClear = CLEAR_RIGHT;

static void servoNod() {
  servo.write(SERVO_CENTER_ANGLE);
  delay(200);
  servo.write(60);
  delay(200);
  servo.write(120);
  delay(200);
  servo.write(SERVO_CENTER_ANGLE);
}

void Rover::begin() {
#if defined(ARDUINO_AVR_UNO)
  SoftPWMBegin();
#endif
  rgbBegin();
  lights.set(ORANGE);
  carBegin();
  irObstacleBegin();
  batteryBegin();
  servo.attach(SERVO_PIN);
  servoNod();
  servoAngle = SERVO_CENTER_ANGLE;
}

void Rover::look(uint8_t angle) {
  servoAngle = constrain(angle, 0, SERVO_MAX_ANGLE);
  servo.write(servoAngle);
}

void Rover::cameraLamp(bool on) {
  if (on) {
    aiCam.lamp_on(CAM_LAMP_LEVEL);
  } else {
    aiCam.lamp_off();
  }
}

void Rover::follow() {
  float usDistance = sensors.distanceCm();
  bool left = sensors.obstacleLeft();
  bool right = sensors.obstacleRight();
  bool inRange = usDistance > 0 && usDistance < followDistanceCm;
  int8_t power = (int8_t)obstaclePower;
  int8_t arcInner = power / 3;

  if (left && !right) {
    if (inRange) {
      drive.setMotors(arcInner, power);
    } else {
      drive.turnLeft(power);
    }
    return;
  }
  if (right && !left) {
    if (inRange) {
      drive.setMotors(power, arcInner);
    } else {
      drive.turnRight(power);
    }
    return;
  }

  if (usDistance > 0 && usDistance < followStopCm) {
    drive.stop();
  } else if (usDistance > 0 && usDistance < followSlowCm) {
    drive.forward((int8_t)followSlowPower);
  } else if (usDistance > 0 && usDistance < followDistanceCm) {
    drive.forward(obstaclePower);
  } else {
    drive.stop();
  }
}

void Rover::avoid() {
  bool leftBlocked = sensors.obstacleLeft();
  bool rightBlocked = sensors.obstacleRight();
  float usDistance = sensors.distanceCm();
  bool usValid = usDistance > 0;
  bool tooClose = usValid && usDistance < reverseCm;
  bool middleClear = usValid && usDistance >= avoidThresholdCm;
  int8_t power = (int8_t)obstaclePower;

  if (leftBlocked && !rightBlocked) {
    drive.backRight(power);
    lastClear = CLEAR_RIGHT;
    return;
  }
  if (rightBlocked && !leftBlocked) {
    drive.backLeft(power);
    lastClear = CLEAR_LEFT;
    return;
  }
  if (leftBlocked && rightBlocked) {
    drive.backward(power);
    return;
  }

  if (tooClose) {
    drive.backward(power);
    return;
  }

  if (!middleClear) {
    if (lastClear == CLEAR_LEFT) {
      drive.turnLeft(power);
    } else {
      drive.turnRight(power);
    }
    return;
  }

  drive.forward(power);
}

#ifdef UNIT_TEST
void roverResetObstacleForTest() { lastClear = CLEAR_RIGHT; }
#endif

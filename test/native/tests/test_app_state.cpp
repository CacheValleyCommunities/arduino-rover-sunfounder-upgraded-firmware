#include "firmware_test.h"

TEST_F(FirmwareTest, ConnectMovesIdleToAppControl) {
  EXPECT_EQ(currentState, STATE_IDLE);
  appStateUpdate(true);

  EXPECT_EQ(currentState, STATE_APP);
  EXPECT_EQ(currentMode, MODE_APP_CONTROL);
}

TEST_F(FirmwareTest, DisconnectReturnsToIdleStopsAndCentersServo) {
  appStateUpdate(true);
  servoAngle = 40;
  hw.servoAngle = 40;
  leftMotorPower = 50;
  carSetMotors(50, 50);

  appStateUpdate(false);

  EXPECT_EQ(currentState, STATE_IDLE);
  int8_t left = 0;
  int8_t right = 0;
  carGetLastPowers(&left, &right);
  EXPECT_EQ(left, 0);
  EXPECT_EQ(right, 0);
  EXPECT_EQ(servoAngle, SERVO_CENTER_ANGLE);
  EXPECT_EQ(hw.servoAngle, SERVO_CENTER_ANGLE);
}

TEST_F(FirmwareTest, IdleRgbBlinksEvery500Ms) {
  hw.millis_ms = 0;
  appStateUpdate(false);
  int firstGreen = hw.pwm[12];

  hw.millis_ms = 501;
  appStateUpdate(false);
  int afterToggle = hw.pwm[12];

  EXPECT_NE(firstGreen, afterToggle);
}

TEST_F(FirmwareTest, ModeHandlerDrivesAppControlMotors) {
  currentMode = MODE_APP_CONTROL;
  leftMotorPower = 30;
  rightMotorPower = -40;
  modeHandler();

  int8_t left = 0;
  int8_t right = 0;
  carGetLastPowers(&left, &right);
  EXPECT_EQ(left, 30);
  EXPECT_EQ(right, -40);
}

TEST_F(FirmwareTest, ModeHandlerDispatchesFollow) {
  currentMode = MODE_OBSTACLE_FOLLOWING;
  obstaclePower = 65;
  setIr(false, false);
  ultrasonicSetDistance(FOLLOW_DISTANCE - 1);
  modeHandler();

  int8_t left = 0;
  int8_t right = 0;
  carGetLastPowers(&left, &right);
  EXPECT_EQ(left, 65);
  EXPECT_EQ(right, 65);
}

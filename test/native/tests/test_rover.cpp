#include "firmware_test.h"

static void expectMotors(int8_t left, int8_t right) {
  int8_t gotL = 0;
  int8_t gotR = 0;
  carGetLastPowers(&gotL, &gotR);
  EXPECT_EQ(gotL, left);
  EXPECT_EQ(gotR, right);
}

TEST_F(FirmwareTest, RoverDriveMatchesCarControl) {
  rover.drive.forward(50);
  expectMotors(50, 50);
  rover.drive.backward(40);
  expectMotors(-40, -40);
  rover.drive.turnLeft(30);
  expectMotors(-30, 30);
  rover.drive.turnRight(20);
  expectMotors(20, -20);
  rover.drive.stop();
  expectMotors(0, 0);
  rover.drive.setMotors(40, -20);
  expectMotors(40, -20);
}

TEST_F(FirmwareTest, RoverLightsMatchRgbWrites) {
  rover.lights.set(WHITE);
  EXPECT_EQ(hw.pwm[12], (int)(0xFF * R_OFFSET));
  EXPECT_EQ(hw.pwm[13], (int)(0xFF * G_OFFSET));
  EXPECT_EQ(hw.pwm[11], (int)(0xFF * B_OFFSET));

  rover.lights.set(10, 20, 30);
  EXPECT_EQ(hw.pwm[12], (int)(10 * R_OFFSET));
  EXPECT_EQ(hw.pwm[13], (int)(20 * G_OFFSET));
  EXPECT_EQ(hw.pwm[11], (int)(30 * B_OFFSET));

  rover.lights.off();
  EXPECT_EQ(hw.pwm[12], 0);
  EXPECT_EQ(hw.pwm[13], 0);
  EXPECT_EQ(hw.pwm[11], 0);
}

TEST_F(FirmwareTest, RoverSensorsReadDistanceAndIr) {
  ultrasonicSetDistance(42.0f);
  EXPECT_FLOAT_EQ(rover.sensors.distanceCm(), 42.0f);

  setIr(true, false);
  EXPECT_TRUE(rover.sensors.obstacleLeft());
  EXPECT_FALSE(rover.sensors.obstacleRight());
  EXPECT_EQ(rover.sensors.irBits(), 0b10);

  hw.analog[A3] = 700;
  EXPECT_FLOAT_EQ(rover.sensors.batteryVolts(), batteryGetVoltage());
  EXPECT_EQ(rover.sensors.batteryPercent(), batteryGetPercentage());
}

TEST_F(FirmwareTest, RoverLookClampsAndWritesServo) {
  rover.look(90);
  EXPECT_EQ(hw.servoAngle, 90);
  EXPECT_EQ(servoAngle, 90);

  rover.look(200);
  EXPECT_EQ(hw.servoAngle, SERVO_MAX_ANGLE);
  EXPECT_EQ(servoAngle, SERVO_MAX_ANGLE);
}

TEST_F(FirmwareTest, RoverBeginAttachesServoAtCenter) {
  rover.begin();
  EXPECT_EQ(hw.servoPin, SERVO_PIN);
  EXPECT_EQ(hw.servoAngle, SERVO_CENTER_ANGLE);
  EXPECT_EQ(servoAngle, SERVO_CENTER_ANGLE);
}

TEST_F(FirmwareTest, RoverCameraLampTogglesAiCam) {
  rover.cameraLamp(true);
  EXPECT_EQ(hw.lampLevel, 5);
  rover.cameraLamp(false);
  EXPECT_EQ(hw.lampLevel, -1);
}

TEST_F(FirmwareTest, FollowStopsWhenCloserThanStopDistance) {
  obstaclePower = 65;
  setIr(false, false);
  ultrasonicSetDistance(FOLLOW_STOP_CM - 1);
  rover.follow();
  expectMotors(0, 0);
}

TEST_F(FirmwareTest, FollowCreepsWhenInsideSlowBand) {
  obstaclePower = 65;
  setIr(false, false);
  ultrasonicSetDistance(FOLLOW_SLOW_CM - 1);
  rover.follow();
  expectMotors(FOLLOW_SLOW_POWER, FOLLOW_SLOW_POWER);
}

TEST_F(FirmwareTest, FollowDrivesWhenInsideFollowDistance) {
  obstaclePower = 65;
  setIr(false, false);
  ultrasonicSetDistance(FOLLOW_DISTANCE - 1);
  rover.follow();
  expectMotors(65, 65);
}

TEST_F(FirmwareTest, FollowStopsWhenOutOfRangeOrInvalidPing) {
  obstaclePower = 65;
  setIr(false, false);
  ultrasonicSetDistance(FOLLOW_DISTANCE + 1);
  rover.follow();
  expectMotors(0, 0);

  carResetForTest();
  ultrasonicSetDistance(-1);
  rover.follow();
  expectMotors(0, 0);
}

TEST_F(FirmwareTest, FollowArcsTowardLeftIrWhenInRange) {
  obstaclePower = 65;
  setIr(true, false);
  ultrasonicSetDistance(30);
  rover.follow();
  expectMotors(65 / 3, 65);
}

TEST_F(FirmwareTest, FollowTurnsLeftToSeekWhenLeftIrOutOfRange) {
  obstaclePower = 65;
  setIr(true, false);
  ultrasonicSetDistance(FOLLOW_DISTANCE + 5);
  rover.follow();
  expectMotors(-65, 65);
}

TEST_F(FirmwareTest, FollowArcsTowardRightIrWhenInRange) {
  obstaclePower = 65;
  setIr(false, true);
  ultrasonicSetDistance(30);
  rover.follow();
  expectMotors(65, 65 / 3);
}

TEST_F(FirmwareTest, AvoidBackRightWhenOnlyLeftIrBlocked) {
  obstaclePower = 65;
  setIr(true, false);
  ultrasonicSetDistance(80);
  rover.avoid();
  expectMotors(-65, 0);
}

TEST_F(FirmwareTest, AvoidBackLeftWhenOnlyRightIrBlocked) {
  obstaclePower = 65;
  setIr(false, true);
  ultrasonicSetDistance(80);
  rover.avoid();
  expectMotors(0, -65);
}

TEST_F(FirmwareTest, AvoidReversesWhenBothIrBlocked) {
  obstaclePower = 65;
  setIr(true, true);
  ultrasonicSetDistance(80);
  rover.avoid();
  expectMotors(-65, -65);
}

TEST_F(FirmwareTest, AvoidReversesWhenUltrasonicTooClose) {
  obstaclePower = 65;
  setIr(false, false);
  ultrasonicSetDistance(ULTRASONIC_REVERSE_CM - 1);
  rover.avoid();
  expectMotors(-65, -65);
}

TEST_F(FirmwareTest, AvoidGoesForwardWhenPathIsClear) {
  obstaclePower = 65;
  setIr(false, false);
  ultrasonicSetDistance(ULTRASONIC_AVOIDANCE_THRESHOLD + 1);
  rover.avoid();
  expectMotors(65, 65);
}

TEST_F(FirmwareTest, AvoidTurnsUsingLastClearHysteresis) {
  obstaclePower = 65;
  setIr(false, false);
  ultrasonicSetDistance(ULTRASONIC_AVOIDANCE_THRESHOLD - 1);

  roverResetObstacleForTest();
  rover.avoid();
  expectMotors(65, -65);

  setIr(true, false);
  ultrasonicSetDistance(80);
  rover.avoid();
  expectMotors(-65, 0);

  setIr(false, false);
  ultrasonicSetDistance(ULTRASONIC_AVOIDANCE_THRESHOLD - 1);
  rover.avoid();
  expectMotors(65, -65);
}

TEST_F(FirmwareTest, FollowUsesRuntimeStopDistance) {
  obstaclePower = 65;
  followStopCm = 20;
  setIr(false, false);
  ultrasonicSetDistance(18);
  rover.follow();
  expectMotors(0, 0);
}

TEST_F(FirmwareTest, AvoidUsesRuntimeReverseDistance) {
  obstaclePower = 65;
  reverseCm = 40;
  setIr(false, false);
  ultrasonicSetDistance(30);
  rover.avoid();
  expectMotors(-65, -65);
}

#include "firmware_test.h"

TEST_F(FirmwareTest, MotorsFrameSetsSignedPowersWithoutChangingMode) {
  currentMode = MODE_OBSTACLE_FOLLOWING;
  loadRecv({ENTITY_MOTORS, 40, 0x9C}); // 40, -100
  onReceive();

  EXPECT_EQ(leftMotorPower, 40);
  EXPECT_EQ(rightMotorPower, (int8_t)-100);
  EXPECT_EQ(currentMode, MODE_OBSTACLE_FOLLOWING);
}

TEST_F(FirmwareTest, RgbFrameWritesCalibratedColor) {
  loadRecv({ENTITY_RGB, 255, 255, 255});
  onReceive();

  EXPECT_EQ(hw.pwm[12], 255);
  EXPECT_EQ(hw.pwm[13], (int)(255 * G_OFFSET));
  EXPECT_EQ(hw.pwm[11], (int)(255 * B_OFFSET));
}

TEST_F(FirmwareTest, ServoFrameIsClampedToMaxAngle) {
  loadRecv({ENTITY_SERVO, 200});
  onReceive();

  EXPECT_EQ(servoAngle, SERVO_MAX_ANGLE);
  EXPECT_EQ(hw.servoAngle, SERVO_MAX_ANGLE);
}

TEST_F(FirmwareTest, LampOnAndOff) {
  loadRecv({ENTITY_LAMP, 1});
  onReceive();
  EXPECT_EQ(hw.lampLevel, 5);

  loadRecv({ENTITY_LAMP, 0});
  onReceive();
  EXPECT_EQ(hw.lampLevel, -1);
}

TEST_F(FirmwareTest, ObstacleAvoidWhenStateSetAndModeZero) {
  loadRecv({ENTITY_OBSTACLE, 1, 0, 80});
  onReceive();

  EXPECT_EQ(currentMode, MODE_OBSTACLE_AVOIDANCE);
  EXPECT_EQ(obstaclePower, 80);
}

TEST_F(FirmwareTest, ObstacleFollowWhenStateSetAndModeNonzero) {
  loadRecv({ENTITY_OBSTACLE, 1, 1, 70});
  onReceive();

  EXPECT_EQ(currentMode, MODE_OBSTACLE_FOLLOWING);
  EXPECT_EQ(obstaclePower, 70);
}

TEST_F(FirmwareTest, ObstacleOffReturnsToAppControlAndStopsMotors) {
  leftMotorPower = 40;
  rightMotorPower = -20;
  currentMode = MODE_OBSTACLE_AVOIDANCE;
  loadRecv({ENTITY_OBSTACLE, 0, 0, 65});
  onReceive();

  EXPECT_EQ(currentMode, MODE_APP_CONTROL);
  EXPECT_EQ(leftMotorPower, 0);
  EXPECT_EQ(rightMotorPower, 0);
}

TEST_F(FirmwareTest, TruncatedMotorFrameDoesNotConsumePayload) {
  leftMotorPower = 7;
  rightMotorPower = 9;
  loadRecv({ENTITY_MOTORS, 40});
  onReceive();

  EXPECT_EQ(leftMotorPower, 7);
  EXPECT_EQ(rightMotorPower, 9);
}

TEST_F(FirmwareTest, TelemetryPacketLayoutChecksumAndEncodings) {
  ultrasonicSetDistance(25.0f);
  setIr(true, false);
  hw.analog[A3] = 700;

  handleSensorData();

  ASSERT_EQ(hw.sent.size(), (size_t)(12 + CFG_TELEMETRY_BYTES));
  EXPECT_EQ(hw.sent[0], WS_DATA_START_BIT);
  EXPECT_EQ(hw.sent[1], 8 + CFG_TELEMETRY_BYTES);
  EXPECT_EQ(hw.sent[11 + CFG_TELEMETRY_BYTES], WS_DATA_END_BIT);

  EXPECT_EQ(hw.sent[3], TELEMETRY_ULTRASONIC);
  uint16_t mm = (uint16_t)(hw.sent[4] << 8) | hw.sent[5];
  EXPECT_EQ(mm, 250);

  EXPECT_EQ(hw.sent[6], TELEMETRY_IR);
  EXPECT_EQ(hw.sent[7], 0b10);

  EXPECT_EQ(hw.sent[8], TELEMETRY_BATTERY);
  float volts = batteryGetVoltage();
  uint8_t expectedBat = (uint8_t)round((volts - 6.0f) * 100);
  EXPECT_EQ(hw.sent[9], expectedBat);

  EXPECT_EQ(hw.sent[10], TELEMETRY_CONFIG);

  uint8_t checksum = 0;
  for (size_t i = 0; i + 1 < hw.sent.size(); i++) {
    checksum ^= hw.sent[i];
  }
  EXPECT_EQ(checksum, 0);
}

TEST_F(FirmwareTest, ConfigFrameUpdatesClampedRuntimeThresholds) {
  loadRecv({ENTITY_CONFIG, 3, CFG_FOLLOW_STOP_CM, 0, 20, CFG_MOVE_POWER, 0, 80,
            CFG_MS_PER_FOOT, 0x08, 0x34});
  onReceive();

  EXPECT_EQ(followStopCm, 20);
  EXPECT_EQ(movePower, 80);
  EXPECT_EQ(msPerFoot, 2100);
}

TEST_F(FirmwareTest, ConfigValuesAreClamped) {
  loadRecv({ENTITY_CONFIG, 2, CFG_FOLLOW_STOP_CM, 0x7F, 0xFF, CFG_MOVE_POWER, 0,
            200});
  onReceive();

  EXPECT_EQ(followStopCm, 100);
  EXPECT_EQ(movePower, 100);
}

TEST_F(FirmwareTest, TruncatedConfigFrameIsIgnored) {
  followStopCm = 12;
  loadRecv({ENTITY_CONFIG, 1, CFG_FOLLOW_STOP_CM, 0});
  onReceive();
  EXPECT_EQ(followStopCm, 12);
}

TEST_F(FirmwareTest, UnknownConfigIdIsIgnored) {
  followStopCm = 12;
  loadRecv({ENTITY_CONFIG, 1, 0x7B, 0, 40});
  onReceive();
  EXPECT_EQ(followStopCm, 12);
}

TEST_F(FirmwareTest, TelemetryPacketIncludesConfigEcho) {
  followStopCm = 14;
  followSlowCm = 22;
  handleSensorData();

  ASSERT_EQ(hw.sent.size(), (size_t)(12 + CFG_TELEMETRY_BYTES));
  EXPECT_EQ(hw.sent[0], WS_DATA_START_BIT);
  EXPECT_EQ(hw.sent[hw.sent.size() - 1], WS_DATA_END_BIT);
  EXPECT_EQ(hw.sent[10], TELEMETRY_CONFIG);

  int16_t stop = (int16_t)(((uint16_t)hw.sent[11] << 8) | hw.sent[12]);
  int16_t slow = (int16_t)(((uint16_t)hw.sent[13] << 8) | hw.sent[14]);
  EXPECT_EQ(stop, 14);
  EXPECT_EQ(slow, 22);

  uint8_t checksum = 0;
  for (size_t i = 0; i + 1 < hw.sent.size(); i++) {
    checksum ^= hw.sent[i];
  }
  EXPECT_EQ(checksum, 0);
}

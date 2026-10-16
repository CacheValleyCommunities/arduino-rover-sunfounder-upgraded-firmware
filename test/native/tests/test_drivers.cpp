#include "firmware_test.h"

static void expectMotors(int8_t left, int8_t right) {
  int8_t gotL = 0;
  int8_t gotR = 0;
  carGetLastPowers(&gotL, &gotR);
  EXPECT_EQ(gotL, left);
  EXPECT_EQ(gotR, right);
}

TEST_F(FirmwareTest, SimpleMovesMapToMotorPowers) {
  carForward(80);
  expectMotors(80, 80);
  carBackward(80);
  expectMotors(-80, -80);
  carTurnLeft(80);
  expectMotors(-80, 80);
  carTurnRight(80);
  expectMotors(80, -80);
  carBackLeft(80);
  expectMotors(0, -80);
  carBackRight(80);
  expectMotors(-80, 0);
  carStop();
  expectMotors(0, 0);
}

TEST_F(FirmwareTest, UnchangedPowersSkipASecondWrite) {
  carForward(50);
  uint32_t afterFirst = hw.pwmWrites;
  EXPECT_GT(afterFirst, 0u);

  carForward(50);
  EXPECT_EQ(hw.pwmWrites, afterFirst);
}

TEST_F(FirmwareTest, ZeroPowerClearsPwmPins) {
  carForward(50);
  carStop();
  EXPECT_EQ(hw.pwm[2], 0);
  EXPECT_EQ(hw.pwm[3], 0);
  EXPECT_EQ(hw.pwm[4], 0);
  EXPECT_EQ(hw.pwm[5], 0);
}

TEST_F(FirmwareTest, UpdateCachesPingConvertedFromPulseWidth) {
  hw.pulse_us = (unsigned long)(50.0f / 0.017f);
  ultrasonicUpdate();
  EXPECT_NEAR(ultrasonicRead(), 50.0f, 0.5f);
}

TEST_F(FirmwareTest, TimeoutAndOutOfRangeBecomeMinusOne) {
  hw.pulse_us = 0;
  ultrasonicUpdate();
  EXPECT_FLOAT_EQ(ultrasonicRead(), -1.0f);

  hw.pulse_us = (unsigned long)((MAX_DISTANCE + 10) / 0.017f);
  ultrasonicUpdate();
  EXPECT_FLOAT_EQ(ultrasonicRead(), -1.0f);
}

TEST_F(FirmwareTest, ObstacleAndClearAreComplementary) {
  const float samples[] = {-1.0f, 0.0f, 14.0f, 34.0f, 35.0f, 36.0f, 300.0f};
  for (float d : samples) {
    ultrasonicSetDistance(d);
    EXPECT_EQ(ultrasonicIsClear(), !ultrasonicIsObstacle()) << "distance=" << d;
  }
}

TEST_F(FirmwareTest, FailedPingIsObstacleNotClear) {
  ultrasonicSetDistance(-1.0f);
  EXPECT_TRUE(ultrasonicIsObstacle());
  EXPECT_FALSE(ultrasonicIsClear());
}

TEST_F(FirmwareTest, DistanceAtThresholdIsClear) {
  ultrasonicSetDistance((float)ULTRASONIC_AVOIDANCE_THRESHOLD);
  EXPECT_FALSE(ultrasonicIsObstacle());
  EXPECT_TRUE(ultrasonicIsClear());
}

TEST_F(FirmwareTest, DistanceJustBelowThresholdIsObstacle) {
  ultrasonicSetDistance((float)ULTRASONIC_AVOIDANCE_THRESHOLD - 1.0f);
  EXPECT_TRUE(ultrasonicIsObstacle());
  EXPECT_FALSE(ultrasonicIsClear());
}

TEST_F(FirmwareTest, ObstacleCheckUsesRuntimeAvoidThreshold) {
  avoidThresholdCm = 20;
  ultrasonicSetDistance(21.0f);
  EXPECT_TRUE(ultrasonicIsClear());
  ultrasonicSetDistance(19.0f);
  EXPECT_TRUE(ultrasonicIsObstacle());
}

TEST_F(FirmwareTest, IrDetectedWhenPinIsLow) {
  setIr(true, false);
  EXPECT_TRUE(irObstacleIsLeftDetected());
  EXPECT_FALSE(irObstacleIsRightDetected());

  setIr(false, true);
  EXPECT_FALSE(irObstacleIsLeftDetected());
  EXPECT_TRUE(irObstacleIsRightDetected());
}

TEST_F(FirmwareTest, IrPackedByteMatchesIosBitLayout) {
  setIr(false, false);
  EXPECT_EQ(irObstacleRead(), 0);

  setIr(true, false);
  EXPECT_EQ(irObstacleRead(), 0b10);

  setIr(false, true);
  EXPECT_EQ(irObstacleRead(), 0b01);

  setIr(true, true);
  EXPECT_EQ(irObstacleRead(), 0b11);
}

TEST_F(FirmwareTest, BatteryVoltageUsesAdcScaleAndTruncatesToTwoDecimals) {
  hw.analog[A3] = 700;
  float volts = batteryGetVoltage();
  float raw = (700.0f / 1024.0f) * ANALOG_REFERENCE_VOLTAGE * 2.0f;
  float expected = int(raw * 100) / 100.0f;
  EXPECT_FLOAT_EQ(volts, expected);
}

TEST_F(FirmwareTest, RgbHexSplitsAndAppliesOffsets) {
  rgbWrite(WHITE);
  EXPECT_EQ(hw.pwm[12], (int)(0xFF * R_OFFSET));
  EXPECT_EQ(hw.pwm[13], (int)(0xFF * G_OFFSET));
  EXPECT_EQ(hw.pwm[11], (int)(0xFF * B_OFFSET));
}

TEST_F(FirmwareTest, RgbOffWritesZeros) {
  rgbWrite(255, 128, 64);
  rgbOff();
  EXPECT_EQ(hw.pwm[12], 0);
  EXPECT_EQ(hw.pwm[13], 0);
  EXPECT_EQ(hw.pwm[11], 0);
}

#include "firmware_test.h"

TEST_F(CamTest, TelemetryPacketUpdatesStateSensors) {
  auto pkt = makeTelemetry(25.0f, true, false, 7.4f);
  c2OnUnoTelemetry(pkt.data(), pkt.size());
  auto doc = stateJson();
  EXPECT_FLOAT_EQ(doc["sensors"]["distance_cm"].as<float>(), 25.0f);
  EXPECT_TRUE(doc["sensors"]["ir_left"].as<bool>());
  EXPECT_FALSE(doc["sensors"]["ir_right"].as<bool>());
  EXPECT_NEAR(doc["sensors"]["battery_v"].as<float>(), 7.4f, 0.011f);
}

TEST_F(CamTest, TelemetryBothIrFlags) {
  auto pkt = makeTelemetry(10.0f, true, true, 6.6f);
  c2OnUnoTelemetry(pkt.data(), pkt.size());
  auto doc = stateJson();
  EXPECT_TRUE(doc["sensors"]["ir_left"].as<bool>());
  EXPECT_TRUE(doc["sensors"]["ir_right"].as<bool>());
}

TEST_F(CamTest, ZeroOrHugeUltrasonicIsNoReading) {
  auto zero = makeTelemetry(0.0f, false, false, 7.0f);
  c2OnUnoTelemetry(zero.data(), zero.size());
  EXPECT_EQ(stateJson()["sensors"]["distance_cm"].as<int>(), -1);

  auto huge = makeTelemetry(4000.0f, false, false, 7.0f);
  c2OnUnoTelemetry(huge.data(), huge.size());
  EXPECT_EQ(stateJson()["sensors"]["distance_cm"].as<int>(), -1);
}

TEST_F(CamTest, DistanceOver300CmIsHiddenInState) {
  auto pkt = makeTelemetry(301.0f, false, false, 7.0f);
  c2OnUnoTelemetry(pkt.data(), pkt.size());
  EXPECT_EQ(stateJson()["sensors"]["distance_cm"].as<int>(), -1);
}

TEST_F(CamTest, ConfigTelemetryUpdatesSettingsCache) {
  int16_t cfg[9] = {14, 22, 50, 30, 35, 15, 80, 2100, 9};
  auto pkt = makeTelemetry(12.0f, false, false, 7.2f, cfg);
  c2OnUnoTelemetry(pkt.data(), pkt.size());
  auto doc = stateJson();
  EXPECT_EQ(doc["settings"]["follow_stop_cm"].as<int>(), 14);
  EXPECT_EQ(doc["settings"]["follow_slow_cm"].as<int>(), 22);
  EXPECT_EQ(doc["settings"]["move_power"].as<int>(), 80);
  EXPECT_EQ(doc["settings"]["ms_per_foot"].as<int>(), 2100);
  EXPECT_EQ(doc["settings"]["ms_per_deg"].as<int>(), 9);
}

TEST_F(CamTest, TruncatedOrUnframedTelemetryIsIgnored) {
  uint8_t shortPkt[] = {0xA0, 1, 0, 0x81};
  c2OnUnoTelemetry(shortPkt, sizeof(shortPkt));
  EXPECT_EQ(stateJson()["sensors"]["distance_cm"].as<int>(), -1);

  uint8_t badStart[] = {0x00, 26, 0, 0x81, 0, 0, 0x82, 0, 0x83, 0, 0xA1};
  c2OnUnoTelemetry(badStart, sizeof(badStart));
  EXPECT_EQ(stateJson()["sensors"]["distance_cm"].as<int>(), -1);

  auto good = makeTelemetry(18.0f, false, true, 7.1f);
  good.back() = 0x00;
  c2OnUnoTelemetry(good.data(), good.size());
  EXPECT_EQ(stateJson()["sensors"]["distance_cm"].as<int>(), -1);
}

TEST_F(CamTest, NullTelemetryIsIgnored) {
  c2OnUnoTelemetry(nullptr, 30);
  EXPECT_EQ(stateJson()["sensors"]["distance_cm"].as<int>(), -1);
}

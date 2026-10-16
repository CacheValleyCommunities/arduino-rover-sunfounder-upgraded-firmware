#include "firmware_test.h"

TEST_F(CamTest, ForwardWrapsRawEntityAsWsbFrame) {
  uint8_t motors[3] = {0x01, 40, 0x9C};
  serialBridgeForwardToUno(motors, 3);
  auto frames = unoPayloads();
  ASSERT_EQ(frames.size(), 1u);
  ASSERT_EQ(frames[0].size(), 3u);
  EXPECT_EQ(frames[0][0], 0x01);
  EXPECT_EQ(frames[0][1], 40);
  EXPECT_EQ(frames[0][2], 0x9C);

  const auto &tx = Serial.tx;
  ASSERT_GE(tx.size(), 10u);
  EXPECT_EQ(tx[0], 'W');
  EXPECT_EQ(tx[4], WS_BIN_START_BYTE);
  EXPECT_EQ(tx[5], 3);
  uint8_t checksum = 0x01 ^ 40 ^ 0x9C;
  EXPECT_EQ(tx[6], checksum);
  EXPECT_EQ(tx[10], WS_BIN_END_BYTE);
  EXPECT_EQ(tx.back(), '\n');
}

TEST_F(CamTest, ForwardPassesAlreadyFramedPayloadThrough) {
  uint8_t framed[5] = {0xA0, 1, 0, 0x01, 0xA1};
  serialBridgeForwardToUno(framed, 5);
  std::string tx(Serial.tx.begin(), Serial.tx.end());
  EXPECT_EQ(tx.compare(0, 4, "WSB+"), 0);
  EXPECT_EQ(Serial.tx[4], 0xA0);
  EXPECT_EQ(Serial.tx[8], 0xA1);
}

TEST_F(CamTest, ForwardEmptyStillWritesWsbPrefix) {
  serialBridgeForwardToUno(nullptr, 0);
  std::string tx(Serial.tx.begin(), Serial.tx.end());
  EXPECT_EQ(tx, "WSB+\n");
}

TEST_F(CamTest, ReadAsciiLineStopsAtNewline) {
  const char *line = "SET+START\n";
  Serial.loadRx((const uint8_t *)line, std::strlen(line));
  uint8_t buf[64] = {};
  size_t n = serialBridgeRead(buf, sizeof(buf));
  EXPECT_EQ(n, 9u);
  EXPECT_EQ(std::string((char *)buf, n), "SET+START");
}

TEST_F(CamTest, ReadSkipsLeadingFfAndBlankLines) {
  uint8_t raw[] = {0xff, '\n', '\r', 'O', 'K', '\n'};
  Serial.loadRx(raw, sizeof(raw));
  uint8_t buf[16] = {};
  size_t n = serialBridgeRead(buf, sizeof(buf));
  EXPECT_EQ(std::string((char *)buf, n), "OK");
}

TEST_F(CamTest, ReadWsbFramedPacketUsesDeclaredLength) {
  uint8_t payload[3] = {0x81, 0x00, 0xFA};
  uint8_t checksum = payload[0] ^ payload[1] ^ payload[2];
  std::vector<uint8_t> line = {'W', 'S', 'B', '+', 0xA0, 3, checksum,
                               payload[0], payload[1], payload[2], 0xA1};
  Serial.loadRx(line.data(), line.size());
  uint8_t buf[32] = {};
  size_t n = serialBridgeRead(buf, sizeof(buf));
  EXPECT_EQ(n, line.size());
  EXPECT_EQ(std::memcmp(buf, line.data(), n), 0);
}

TEST_F(CamTest, ReadLegacyBinaryUsesSecondByteAsLength) {
  uint8_t raw[] = {0x0C, 0x03, 0x01};
  Serial.loadRx(raw, sizeof(raw));
  uint8_t buf[16] = {};
  size_t n = serialBridgeRead(buf, sizeof(buf));
  EXPECT_EQ(n, 3u);
  EXPECT_EQ(buf[0], 0x0C);
  EXPECT_EQ(buf[2], 0x01);
}

TEST_F(CamTest, ReadEmptyReturnsZero) {
  uint8_t buf[8] = {};
  EXPECT_EQ(serialBridgeRead(buf, sizeof(buf)), 0u);
}

TEST_F(CamTest, FanoutFeedsC2TelemetryParser) {
  auto pkt = makeTelemetry(25.0f, true, false, 7.4f);
  serialBridgeFanoutBinary(pkt.data(), pkt.size());
  auto doc = stateJson();
  EXPECT_FLOAT_EQ(doc["sensors"]["distance_cm"].as<float>(), 25.0f);
  EXPECT_TRUE(doc["sensors"]["ir_left"].as<bool>());
  EXPECT_FALSE(doc["sensors"]["ir_right"].as<bool>());
  EXPECT_NEAR(doc["sensors"]["battery_v"].as<float>(), 7.4f, 0.01f);
}

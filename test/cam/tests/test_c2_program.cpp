#include "firmware_test.h"

TEST_F(CamTest, RejectsEmptyMissingStepsAndTooLong) {
  EXPECT_FALSE(c2ProgramStoreAndStart(""));
  EXPECT_FALSE(c2ProgramStoreAndStart("{}"));
  EXPECT_FALSE(c2ProgramStoreAndStart("{\"version\":1}"));
  EXPECT_FALSE(c2ProgramStoreAndStart("not json"));
  String huge = "{\"version\":1,\"steps\":[";
  while (huge.length() < 3100) {
    huge += "{\"wait\":0},";
  }
  huge += "]}";
  EXPECT_FALSE(c2ProgramStoreAndStart(huge));
}

TEST_F(CamTest, PostProgram400OnBadJson) {
  ASSERT_TRUE(postJson("/c2/api/program", "{\"nope\":true}"));
  EXPECT_EQ(camHttp.lastCode, 400);
}

TEST_F(CamTest, StartHatRunsBody) {
  ASSERT_TRUE(postJson("/c2/api/program",
                       "{\"version\":1,\"steps\":[{\"when\":\"start\",\"do\":[{"
                       "\"lamp\":\"on\"}]}]}"));
  EXPECT_EQ(camHttp.lastCode, 200);
  EXPECT_TRUE(hasEntity(0x04, {1}));
}

TEST_F(CamTest, OnStartAliasRunsBody) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"on_start\":[{\"look\":45}]}]}"));
  EXPECT_TRUE(hasEntity(0x03, {45}));
}

TEST_F(CamTest, ColorLampLookAndStop) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"set_color\":[10,20,30]},{\"lamp\":\"off\"},"
      "{\"look\":0},{\"stop\":\"motors\"}]}"));
  auto frames = unoPayloads();
  ASSERT_GE(frames.size(), 3u);
  EXPECT_EQ(frames[0], (std::vector<uint8_t>{0x02, 10, 20, 30}));
  EXPECT_EQ(frames[1], (std::vector<uint8_t>{0x04, 0}));
  EXPECT_EQ(frames[2], (std::vector<uint8_t>{0x03, 0}));
  EXPECT_EQ(frames.back(), (std::vector<uint8_t>{0x01, 0, 0}));
}

TEST_F(CamTest, RepeatRunsBodyNTimes) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"repeat\":2,\"steps\":[{\"lamp\":\"on\"}]}]"
      "}"));
  auto frames = unoPayloads();
  int lamps = 0;
  for (const auto &f : frames) {
    if (f.size() == 2 && f[0] == 0x04 && f[1] == 1) {
      lamps++;
    }
  }
  EXPECT_EQ(lamps, 2);
}

TEST_F(CamTest, IfElseTakesTrueBranch) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"if\":{\"op\":\"gt\",\"a\":2,\"b\":1},"
      "\"do\":[{\"lamp\":\"on\"}],\"else\":[{\"lamp\":\"off\"}]}]}"));
  EXPECT_TRUE(hasEntity(0x04, {1}));
}

TEST_F(CamTest, IfElseTakesFalseBranch) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"if\":{\"op\":\"lt\",\"a\":2,\"b\":1},"
      "\"do\":[{\"lamp\":\"on\"}],\"else\":[{\"lamp\":\"off\"}]}]}"));
  EXPECT_TRUE(hasEntity(0x04, {0}));
  EXPECT_FALSE(hasEntity(0x04, {1}));
}

TEST_F(CamTest, WaitUntilUsesTelemetrySensor) {
  auto pkt = makeTelemetry(40.0f, false, false, 7.0f);
  c2OnUnoTelemetry(pkt.data(), pkt.size());
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"wait_until\":{\"op\":\"gt\",\"a\":"
      "\"distance_cm\",\"b\":10}},{\"lamp\":\"on\"}]}"));
  EXPECT_TRUE(hasEntity(0x04, {1}));
}

TEST_F(CamTest, SensorKeysInComparisons) {
  auto pkt = makeTelemetry(12.0f, true, false, 7.2f);
  c2OnUnoTelemetry(pkt.data(), pkt.size());
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"if\":{\"op\":\"eq\",\"a\":\"ir_left\","
      "\"b\":1},\"do\":[{\"look\":30}]}]}"));
  EXPECT_TRUE(hasEntity(0x03, {30}));
}

TEST_F(CamTest, SetPowerWritesConfigEntity) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"set_power\":50}]}"));
  auto frames = unoPayloads();
  ASSERT_FALSE(frames.empty());
  const auto &f = frames.front();
  ASSERT_EQ(f.size(), 5u);
  EXPECT_EQ(f[0], 0x07);
  EXPECT_EQ(f[1], 1);
  EXPECT_EQ(f[2], 7);
  EXPECT_EQ(f[3], 0);
  EXPECT_EQ(f[4], 50);
}

TEST_F(CamTest, MoveForwardSendsDriveThenStop) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"move_forward\":\"0 sec\"}]}"));
  auto frames = unoPayloads();
  ASSERT_GE(frames.size(), 2u);
  EXPECT_EQ(frames[0], (std::vector<uint8_t>{0x01, 70, 70}));
  EXPECT_EQ(frames.back(), (std::vector<uint8_t>{0x01, 0, 0}));
}

TEST_F(CamTest, TurnLeftUsesOppositePowers) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"turn\":\"left 0\"}]}"));
  auto frames = unoPayloads();
  ASSERT_GE(frames.size(), 2u);
  EXPECT_EQ(frames[0], (std::vector<uint8_t>{0x01, (uint8_t)-70, 70}));
}

TEST_F(CamTest, RequestStopDoesNotWaitForTask) {
  c2ProgramRequestStop();
  EXPECT_FALSE(c2ProgramIsRunning());
}

TEST_F(CamTest, StopAllHaltsLaterSteps) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"stop\":\"all\"},{\"lamp\":\"on\"}]}"));
  auto frames = unoPayloads();
  for (const auto &f : frames) {
    EXPECT_FALSE(f.size() == 2 && f[0] == 0x04);
  }
}

TEST_F(CamTest, GetProgramReturnsStoredJson) {
  ASSERT_TRUE(postJson("/c2/api/program",
                       "{\"version\":1,\"steps\":[{\"lamp\":\"on\"}]}"));
  ASSERT_TRUE(getUri("/c2/api/program"));
  JsonDocument doc;
  deserializeJson(doc, camHttp.lastBodyString().c_str());
  EXPECT_TRUE(doc["ok"].as<bool>());
  EXPECT_TRUE(doc["deployed"].as<bool>());
  EXPECT_FALSE(doc["onboard"].as<bool>());
  EXPECT_EQ(doc["program"]["steps"][0]["lamp"].as<const char *>(),
            std::string("on"));
}

TEST_F(CamTest, DeleteClearsProgram) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"lamp\":\"on\"}]}"));
  ASSERT_TRUE(camHttp.dispatch("/c2/api/program", HTTP_DELETE));
  EXPECT_FALSE(c2ProgramIsStored());
  auto doc = stateJson();
  EXPECT_FALSE(doc["deployed"].as<bool>());
}

TEST_F(CamTest, PersistAndAutorunOnBegin) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"lamp\":\"on\"}]}"));
  c2ProgramResetForTest();
  EXPECT_FALSE(c2ProgramIsStored());
  Serial.reset();
  c2ProgramBegin();
  EXPECT_TRUE(c2ProgramIsStored());
  EXPECT_TRUE(hasEntity(0x04, {1}));
}

TEST_F(CamTest, StateDoesNotEmbedProgram) {
  ASSERT_TRUE(c2ProgramStoreAndStart(
      "{\"version\":1,\"steps\":[{\"lamp\":\"on\"}]}"));
  auto doc = stateJson();
  EXPECT_TRUE(doc["program"].isNull());
  EXPECT_TRUE(doc["deployed"].as<bool>());
  EXPECT_FALSE(doc["onboard"].as<bool>());
}

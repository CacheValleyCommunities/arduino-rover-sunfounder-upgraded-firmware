#include "firmware_test.h"

TEST_F(CamTest, IndexIsGzipHtml) {
  ASSERT_TRUE(getUri("/c2"));
  EXPECT_EQ(camHttp.lastCode, 200);
  EXPECT_EQ(camHttp.lastType, "text/html");
  EXPECT_TRUE(camHttp.hasHeader("Content-Encoding", "gzip"));
  ASSERT_EQ(camHttp.lastBody.size(), 2u);
  EXPECT_EQ(camHttp.lastBody[0], 0x1f);
}

TEST_F(CamTest, AssetsAreGzip) {
  ASSERT_TRUE(getUri("/c2/c2.js"));
  EXPECT_EQ(camHttp.lastType, "text/javascript");
  ASSERT_TRUE(getUri("/c2/c2.css"));
  EXPECT_EQ(camHttp.lastType, "text/css");
  ASSERT_TRUE(getUri("/c2/blockly.js"));
  EXPECT_EQ(camHttp.lastType, "text/javascript");
}

TEST_F(CamTest, OptionsReturnsNoContentWithCors) {
  ASSERT_TRUE(camHttp.dispatch("/c2/api/state", HTTP_OPTIONS));
  EXPECT_EQ(camHttp.lastCode, 204);
  EXPECT_TRUE(camHttp.hasHeader("Access-Control-Allow-Origin", "*"));
}

TEST_F(CamTest, StateDefaultsWithoutTelemetry) {
  auto doc = stateJson();
  EXPECT_EQ(camHttp.lastCode, 200);
  EXPECT_EQ(doc["sensors"]["distance_cm"].as<int>(), -1);
  EXPECT_FALSE(doc["sensors"]["ir_left"].as<bool>());
  EXPECT_FALSE(doc["sensors"]["ir_right"].as<bool>());
  EXPECT_FLOAT_EQ(doc["sensors"]["battery_v"].as<float>(), 0);
  EXPECT_FALSE(doc["running"].as<bool>());
  EXPECT_FALSE(doc["deployed"].as<bool>());
  EXPECT_FALSE(doc["onboard"].as<bool>());
  EXPECT_EQ(doc["settings"]["move_power"].as<int>(), 70);
  EXPECT_EQ(doc["settings"]["follow_stop_cm"].as<int>(), 12);
  EXPECT_TRUE(doc["preview"].isNull());
}

TEST_F(CamTest, MotorsPostFramesSignedPowers) {
  ASSERT_TRUE(postJson("/c2/api/motors", "{\"left\":40,\"right\":-100}"));
  EXPECT_EQ(camHttp.lastCode, 200);
  EXPECT_TRUE(lastEntityIs(0x01, {40, 0x9C}));
  EXPECT_TRUE(appSessionHas(APP_PEER_C2));
}

TEST_F(CamTest, MotorsClampToPlusMinus100) {
  ASSERT_TRUE(postJson("/c2/api/motors", "{\"left\":150,\"right\":-200}"));
  EXPECT_TRUE(lastEntityIs(0x01, {100, (uint8_t)-100}));
}

TEST_F(CamTest, MotorsDefaultToZero) {
  ASSERT_TRUE(postJson("/c2/api/motors", "{}"));
  EXPECT_TRUE(lastEntityIs(0x01, {0, 0}));
}

TEST_F(CamTest, RgbIsClamped) {
  ASSERT_TRUE(postJson("/c2/api/rgb", "{\"r\":300,\"g\":-1,\"b\":9}"));
  EXPECT_TRUE(lastEntityIs(0x02, {255, 0, 9}));
}

TEST_F(CamTest, LampOnAndOff) {
  ASSERT_TRUE(postJson("/c2/api/lamp", "{\"on\":true}"));
  EXPECT_TRUE(lastEntityIs(0x04, {1}));
  ASSERT_TRUE(postJson("/c2/api/lamp", "{\"on\":false}"));
  EXPECT_TRUE(lastEntityIs(0x04, {0}));
}

TEST_F(CamTest, ServoClampedTo140) {
  ASSERT_TRUE(postJson("/c2/api/servo", "{\"angle\":200}"));
  EXPECT_TRUE(lastEntityIs(0x03, {140}));
  ASSERT_TRUE(postJson("/c2/api/servo", "{\"angle\":-5}"));
  EXPECT_TRUE(lastEntityIs(0x03, {0}));
}

TEST_F(CamTest, ServoDefaultsTo90) {
  ASSERT_TRUE(postJson("/c2/api/servo", "{}"));
  EXPECT_TRUE(lastEntityIs(0x03, {90}));
}

TEST_F(CamTest, SettingsSendsConfigIdsBigEndian) {
  ASSERT_TRUE(postJson("/c2/api/settings",
                       "{\"move_power\":80,\"ms_per_foot\":2100}"));
  auto frames = unoPayloads();
  ASSERT_FALSE(frames.empty());
  const auto &f = frames.back();
  ASSERT_GE(f.size(), 8u);
  EXPECT_EQ(f[0], 0x07);
  EXPECT_EQ(f[1], 2);
  EXPECT_EQ(f[2], 7);
  EXPECT_EQ(f[3], 0);
  EXPECT_EQ(f[4], 80);
  EXPECT_EQ(f[5], 8);
  EXPECT_EQ(f[6], 0x08);
  EXPECT_EQ(f[7], 0x34);
  auto doc = stateJson();
  EXPECT_EQ(doc["settings"]["move_power"].as<int>(), 80);
  EXPECT_EQ(doc["settings"]["ms_per_foot"].as<int>(), 2100);
}

TEST_F(CamTest, SettingsIgnoresUnknownKeys) {
  Serial.reset();
  ASSERT_TRUE(postJson("/c2/api/settings", "{\"nope\":1}"));
  EXPECT_TRUE(unoPayloads().empty());
}

TEST_F(CamTest, HeartbeatMarksRunningThenTimeoutStopsMotors) {
  ASSERT_TRUE(postJson("/c2/api/heartbeat", "{}"));
  EXPECT_TRUE(appSessionHas(APP_PEER_C2));
  auto doc = stateJson();
  EXPECT_TRUE(doc["running"].as<bool>());

  Serial.reset();
  camMillisAdd(401);
  c2Loop();
  EXPECT_FALSE(appSessionHas(APP_PEER_C2));
  EXPECT_TRUE(lastEntityIs(0x01, {0, 0}));
  std::string tx(Serial.tx.begin(), Serial.tx.end());
  EXPECT_NE(tx.find("[APPSTOP]"), std::string::npos);
}

TEST_F(CamTest, HeartbeatTimeoutDoesNotStopMotorsIfUdpPeerRemains) {
  appSessionNote(APP_PEER_UDP, "UDP");
  ASSERT_TRUE(postJson("/c2/api/heartbeat", "{}"));
  Serial.reset();
  camMillisAdd(401);
  c2Loop();
  EXPECT_FALSE(appSessionHas(APP_PEER_C2));
  EXPECT_TRUE(appSessionHas(APP_PEER_UDP));
  EXPECT_TRUE(unoPayloads().empty());
}

TEST_F(CamTest, StopClearsMotorsAndRunning) {
  ASSERT_TRUE(postJson("/c2/api/motors", "{\"left\":20,\"right\":20}"));
  ASSERT_TRUE(postJson("/c2/api/stop", "{}"));
  EXPECT_TRUE(lastEntityIs(0x01, {0, 0}));
  auto doc = stateJson();
  EXPECT_FALSE(doc["running"].as<bool>());
}

TEST_F(CamTest, UnknownRouteIs404) {
  EXPECT_FALSE(getUri("/c2/api/nope"));
  EXPECT_EQ(camHttp.lastCode, 404);
}

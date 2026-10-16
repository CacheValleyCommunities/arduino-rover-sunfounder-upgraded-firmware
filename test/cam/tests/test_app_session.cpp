#include "firmware_test.h"

TEST_F(CamTest, FirstPeerNotesConnectedAndTurnsLedOn) {
  appSessionNote(APP_PEER_C2, "C2");
  EXPECT_TRUE(appSessionHas(APP_PEER_C2));
  EXPECT_FALSE(appSessionHas(APP_PEER_UDP));
  ledStatusHandler();
  EXPECT_EQ(camPin(LED_PIN), LOW);
  std::string tx(Serial.tx.begin(), Serial.tx.end());
  EXPECT_NE(tx.find("[CONNECTED] Connected C2"), std::string::npos);
}

TEST_F(CamTest, SecondPeerDoesNotRetriggerConnectedLog) {
  appSessionNote(APP_PEER_C2, "C2");
  Serial.reset();
  appSessionNote(APP_PEER_UDP, "UDP");
  EXPECT_TRUE(appSessionHas(APP_PEER_C2));
  EXPECT_TRUE(appSessionHas(APP_PEER_UDP));
  EXPECT_TRUE(Serial.tx.empty());
}

TEST_F(CamTest, DropLastPeerSendsAppStopAndDisconnectsLed) {
  appSessionNote(APP_PEER_WS, "WS");
  Serial.reset();
  appSessionDrop(APP_PEER_WS, true, "gone");
  EXPECT_FALSE(appSessionHas(APP_PEER_WS));
  std::string tx(Serial.tx.begin(), Serial.tx.end());
  EXPECT_NE(tx.find("[APPSTOP] gone"), std::string::npos);
}

TEST_F(CamTest, DropOneOfTwoPeersKeepsSession) {
  appSessionNote(APP_PEER_C2, "C2");
  appSessionNote(APP_PEER_UDP, "UDP");
  Serial.reset();
  appSessionDrop(APP_PEER_C2, true, "c2 left");
  EXPECT_FALSE(appSessionHas(APP_PEER_C2));
  EXPECT_TRUE(appSessionHas(APP_PEER_UDP));
  EXPECT_TRUE(Serial.tx.empty());
}

TEST_F(CamTest, PauseWithoutOthersSendsAppStopOnce) {
  appSessionNote(APP_PEER_C2, "C2");
  Serial.reset();
  appSessionPause(APP_PEER_C2, "idle");
  appSessionPause(APP_PEER_C2, "idle");
  std::string tx(Serial.tx.begin(), Serial.tx.end());
  EXPECT_EQ(tx.find("[APPSTOP] idle"), tx.rfind("[APPSTOP] idle"));
}

TEST_F(CamTest, PauseIsSkippedWhenAnotherPeerRemains) {
  appSessionNote(APP_PEER_C2, "C2");
  appSessionNote(APP_PEER_UDP, "UDP");
  Serial.reset();
  appSessionPause(APP_PEER_C2, "idle");
  EXPECT_TRUE(Serial.tx.empty());
}

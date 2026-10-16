#include "firmware_test.h"

#include "WiFi.h"

static const IPAddress kPeer(192, 168, 4, 2);

TEST_F(CamTest, ProbeFromClientGetsIdentityJson) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  WiFiUDP::inject(kPeer, 40123, "GRVR?");
  udpServer.loop();

  ASSERT_FALSE(WiFiUDP::sent().empty());
  const auto &pkt = WiFiUDP::sent().back();
  EXPECT_TRUE(pkt.ip == kPeer);
  EXPECT_EQ(pkt.port, 40123);
  std::string body(pkt.data.begin(), pkt.data.end());
  EXPECT_NE(body.find("\"Name\":\"GalaxyRVR\""), std::string::npos);
  EXPECT_NE(body.find("\"udp\":30103"), std::string::npos);
  EXPECT_NE(body.find("\"ap\":\"192.168.4.1\""), std::string::npos);
  EXPECT_EQ(body.find("\"sta\""), std::string::npos);
}

TEST_F(CamTest, ProbeFromOwnApIpIsIgnored) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  WiFiUDP::inject(WiFi.softAPIP(), 40123, "GRVR?");
  udpServer.loop();
  EXPECT_TRUE(WiFiUDP::sent().empty());
}

TEST_F(CamTest, JsonEchoIsIgnored) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  WiFiUDP::inject(kPeer, 40123, "{\"Name\":\"x\"}");
  udpServer.loop();
  EXPECT_TRUE(WiFiUDP::sent().empty());
  EXPECT_FALSE(appSessionHas(APP_PEER_UDP));
}

TEST_F(CamTest, PingMarksPeerWithoutForwarding) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  WiFiUDP::inject(kPeer, 40123, "ping");
  udpServer.loop();
  EXPECT_TRUE(appSessionHas(APP_PEER_UDP));
  EXPECT_TRUE(unoPayloads().empty());
}

TEST_F(CamTest, NullKeepaliveMarksPeer) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  uint8_t zero = 0;
  WiFiUDP::inject(kPeer, 40123, &zero, 1);
  udpServer.loop();
  EXPECT_TRUE(appSessionHas(APP_PEER_UDP));
}

TEST_F(CamTest, ControlBytesAreForwardedAsWsb) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  uint8_t motors[3] = {0x01, 40, 40};
  WiFiUDP::inject(kPeer, 40123, motors, 3);
  udpServer.loop();
  EXPECT_TRUE(lastEntityIs(0x01, {40, 40}));
  EXPECT_TRUE(appSessionHas(APP_PEER_UDP));
}

TEST_F(CamTest, ProbeDoesNotMarkPeer) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  WiFiUDP::inject(kPeer, 40123, "GRVR?\n");
  udpServer.loop();
  EXPECT_FALSE(appSessionHas(APP_PEER_UDP));
}

TEST_F(CamTest, PeerTimeoutDropsSession) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  WiFiUDP::inject(kPeer, 40123, "ping");
  udpServer.loop();
  EXPECT_TRUE(appSessionHas(APP_PEER_UDP));
  Serial.reset();
  camMillisAdd(UDP_DATA_TIMEOUT_MS + 1);
  udpServer.loop();
  EXPECT_FALSE(appSessionHas(APP_PEER_UDP));
  std::string tx(Serial.tx.begin(), Serial.tx.end());
  EXPECT_NE(tx.find("[APPSTOP]"), std::string::npos);
}

TEST_F(CamTest, BeaconBroadcastsIdentity) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  camMillisAdd(UDP_BEACON_INTERVAL_MS);
  udpServer.loop();
  bool sawBroadcast = false;
  for (const auto &pkt : WiFiUDP::sent()) {
    if (pkt.ip == IPAddress(255, 255, 255, 255) &&
        pkt.port == UDP_CONTROL_PORT) {
      std::string body(pkt.data.begin(), pkt.data.end());
      if (body.find("\"Name\":\"GalaxyRVR\"") != std::string::npos) {
        sawBroadcast = true;
      }
    }
  }
  EXPECT_TRUE(sawBroadcast);
}

TEST_F(CamTest, SendBinOnlyAfterPeer) {
  udpServer.begin("GalaxyRVR", "GalaxyRVR");
  uint8_t payload[] = {0xA0, 0xA1};
  udpServer.sendBIN(payload, sizeof(payload));
  EXPECT_TRUE(WiFiUDP::sent().empty());

  WiFiUDP::inject(kPeer, 40123, "ping");
  udpServer.loop();
  WiFiUDP::reset();
  udpServer.sendBIN(payload, sizeof(payload));
  ASSERT_EQ(WiFiUDP::sent().size(), 1u);
  EXPECT_TRUE(WiFiUDP::sent()[0].ip == kPeer);
  EXPECT_EQ(WiFiUDP::sent()[0].data.size(), 2u);
}

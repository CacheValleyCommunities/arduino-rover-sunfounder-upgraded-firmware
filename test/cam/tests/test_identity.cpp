#include "firmware_test.h"

TEST_F(CamTest, DiscoveryJsonOmitsEmptySta) {
  String json = identityDiscoveryJson("GalaxyRVR", "GalaxyRVR", 30103,
                                      "http://192.168.4.1:9000/mjpg",
                                      "192.168.4.1", "");
  EXPECT_NE(json.s.find("\"Name\":\"GalaxyRVR\""), std::string::npos);
  EXPECT_NE(json.s.find("\"Type\":\"GalaxyRVR\""), std::string::npos);
  EXPECT_NE(json.s.find("\"udp\":30103"), std::string::npos);
  EXPECT_NE(json.s.find("\"video\":\"http://192.168.4.1:9000/mjpg\""),
            std::string::npos);
  EXPECT_NE(json.s.find("\"ap\":\"192.168.4.1\""), std::string::npos);
  EXPECT_EQ(json.s.find("\"sta\""), std::string::npos);
}

TEST_F(CamTest, DiscoveryJsonOmitsZeroSta) {
  String json = identityDiscoveryJson("N", "T", 30103, "http://x/mjpg",
                                      "192.168.4.1", "0.0.0.0");
  EXPECT_EQ(json.s.find("\"sta\""), std::string::npos);
}

TEST_F(CamTest, DiscoveryJsonIncludesStaWhenSet) {
  String json = identityDiscoveryJson("N", "T", 30103, "http://x/mjpg",
                                      "192.168.4.1", "10.0.0.8");
  EXPECT_NE(json.s.find("\"sta\":\"10.0.0.8\""), std::string::npos);
}

TEST_F(CamTest, WsHelloJsonIncludesCheckAndTemplate) {
  String json =
      identityWsHelloJson("GalaxyRVR", "GalaxyRVR", "SC",
                          "http://192.168.4.1:9000/mjpg", "10.0.0.8",
                          "http://{ip}:9000/mjpg");
  EXPECT_NE(json.s.find("\"Check\":\"SC\""), std::string::npos);
  EXPECT_NE(json.s.find("\"StaIp\":\"10.0.0.8\""), std::string::npos);
  EXPECT_NE(json.s.find("\"VideoTemplate\":\"http://{ip}:9000/mjpg\""),
            std::string::npos);
}

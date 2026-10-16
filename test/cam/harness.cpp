#include "app_session.h"
#include "c2_program.h"
#include "c2_server.h"
#include "http_helpers.h"
#include "led_status.h"
#include "udp_server.h"
#include "ws_server.h"

#include "Arduino.h"
#include "Preferences.h"
#include "WiFi.h"
#include "WiFiUdp.h"

String videoUrl = "http://192.168.4.1:9000/mjpg";
UDP_Server udpServer;
WS_Server wsServer;
WebServer camHttp(80);

void resetCamFirmware() {
  udpServer.close();
  camPinsReset();
  Preferences::resetAll();
  WiFiUDP::reset();
  wifiReset();
  appSessionResetForTest();
  c2ProgramResetForTest();
  c2ResetForTest();
  videoUrl = "http://192.168.4.1:9000/mjpg";
  ledBegin();
  httpBind(camHttp);
  c2Begin(camHttp);
}

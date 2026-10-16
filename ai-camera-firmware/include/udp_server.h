#ifndef __UDP_SERVER_H__
#define __UDP_SERVER_H__

#include <Arduino.h>
#include <WiFiUdp.h>

#include "serial_bridge.h"

#define UDP_CONTROL_PORT 30103
#define UDP_BEACON_INTERVAL_MS 500
#define UDP_DATA_TIMEOUT_MS 3000
#define UDP_RX_BUFFER_SIZE 256

class UDP_Server {
public:
  UDP_Server();
  void begin(String name, String type);
  void close();
  void loop();
  void sendBIN(uint8_t *payload, size_t length);

private:
  WiFiUDP udp;
  bool running = false;
  String wsName;
  String wsType;
  bool hasPeer = false;
  IPAddress peerIP;
  uint16_t peerPort = 0;
  uint32_t lastPeerDataMs = 0;
  uint32_t lastBeaconMs = 0;

  String buildIdentity(String videoHost);
  void sendIdentity(IPAddress ip, uint16_t port);
  void sendBeacon();
  void handlePacket(IPAddress ip, uint16_t port, uint8_t *data, int len);
  bool isProbe(uint8_t *data, int len);
  bool isKeepalive(uint8_t *data, int len);
  void forwardControlToUno(uint8_t *data, int len);
  void markPeer(IPAddress ip, uint16_t port);
  void dropPeer(bool notifyUno);
};

#endif // __UDP_SERVER_H__

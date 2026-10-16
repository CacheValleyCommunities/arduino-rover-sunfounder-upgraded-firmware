#pragma once

#include "IPAddress.h"

#include <vector>

struct UdpDatagram {
  IPAddress ip;
  uint16_t port = 0;
  std::vector<uint8_t> data;
};

class WiFiUDP {
public:
  uint8_t begin(uint16_t /*port*/) { return 1; }
  void stop() {}

  int parsePacket();
  int read(uint8_t *buffer, size_t len);
  IPAddress remoteIP() const { return current.ip; }
  uint16_t remotePort() const { return current.port; }

  int beginPacket(IPAddress ip, uint16_t port);
  size_t write(const uint8_t *buffer, size_t size);
  size_t write(uint8_t b) { return write(&b, 1); }
  int endPacket();

  static void reset();
  static void inject(IPAddress ip, uint16_t port, const uint8_t *data, size_t n);
  static void inject(IPAddress ip, uint16_t port, const char *text);
  static const std::vector<UdpDatagram> &sent();

private:
  UdpDatagram current;
  UdpDatagram pending;
  bool havePending = false;
  static std::vector<UdpDatagram> inbox;
  static size_t inboxPos;
  static std::vector<UdpDatagram> outbox;
};

#include "WiFiUdp.h"

#include <cstring>

std::vector<UdpDatagram> WiFiUDP::inbox;
size_t WiFiUDP::inboxPos = 0;
std::vector<UdpDatagram> WiFiUDP::outbox;

void WiFiUDP::reset() {
  inbox.clear();
  inboxPos = 0;
  outbox.clear();
}

void WiFiUDP::inject(IPAddress ip, uint16_t port, const uint8_t *data,
                     size_t n) {
  UdpDatagram d;
  d.ip = ip;
  d.port = port;
  if (data && n) {
    d.data.assign(data, data + n);
  }
  inbox.push_back(std::move(d));
}

void WiFiUDP::inject(IPAddress ip, uint16_t port, const char *text) {
  inject(ip, port, (const uint8_t *)text, text ? std::strlen(text) : 0);
}

const std::vector<UdpDatagram> &WiFiUDP::sent() { return outbox; }

int WiFiUDP::parsePacket() {
  if (inboxPos >= inbox.size()) {
    current = {};
    return 0;
  }
  current = inbox[inboxPos++];
  return (int)current.data.size();
}

int WiFiUDP::read(uint8_t *buffer, size_t len) {
  if (!buffer) {
    return 0;
  }
  size_t n = std::min(len, current.data.size());
  if (n) {
    std::memcpy(buffer, current.data.data(), n);
  }
  return (int)n;
}

int WiFiUDP::beginPacket(IPAddress ip, uint16_t port) {
  pending = {};
  pending.ip = ip;
  pending.port = port;
  havePending = true;
  return 1;
}

size_t WiFiUDP::write(const uint8_t *buffer, size_t size) {
  if (!havePending || !buffer || size == 0) {
    return 0;
  }
  pending.data.insert(pending.data.end(), buffer, buffer + size);
  return size;
}

int WiFiUDP::endPacket() {
  if (!havePending) {
    return 0;
  }
  outbox.push_back(pending);
  havePending = false;
  pending = {};
  return 1;
}

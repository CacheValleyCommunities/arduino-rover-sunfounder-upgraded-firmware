#include "udp_server.h"

#include <WiFi.h>

#include "app_session.h"
#include "identity.h"
#include "log.h"
#include "serial_bridge.h"
#include "wifi_helper.h"

extern String videoUrl;

static bool payloadEquals(uint8_t *data, int len, const char *text) {
  size_t n = strlen(text);
  if ((size_t)len != n) {
    return false;
  }
  return memcmp(data, text, n) == 0;
}

UDP_Server::UDP_Server() {}

void UDP_Server::begin(String name, String type) {
  close();
  wsName = name;
  wsType = type;
  hasPeer = false;
  lastBeaconMs = 0;

  if (udp.begin(UDP_CONTROL_PORT) != 1) {
    Serial.println("[ERROR] UDP bind failed");
    running = false;
    return;
  }
  running = true;
  debug("UDP control on port ", String(UDP_CONTROL_PORT));
}

void UDP_Server::close() {
  if (running) {
    dropPeer(false);
    udp.stop();
    running = false;
  }
}

void UDP_Server::loop() {
  if (!running) {
    return;
  }

  int packetSize = udp.parsePacket();
  while (packetSize > 0) {
    uint8_t buf[UDP_RX_BUFFER_SIZE];
    int len = udp.read(buf, UDP_RX_BUFFER_SIZE);
    if (len > 0) {
      handlePacket(udp.remoteIP(), udp.remotePort(), buf, len);
    }
    packetSize = udp.parsePacket();
  }

  uint32_t now = millis();
  if (now - lastBeaconMs >= UDP_BEACON_INTERVAL_MS) {
    lastBeaconMs = now;
    sendBeacon();
  }

  if (hasPeer && (now - lastPeerDataMs > UDP_DATA_TIMEOUT_MS)) {
    dropPeer(true);
  }
}

void UDP_Server::sendBIN(uint8_t *payload, size_t length) {
  if (!running || !hasPeer || payload == nullptr || length == 0) {
    return;
  }
  udp.beginPacket(peerIP, peerPort);
  udp.write(payload, length);
  udp.endPacket();
}

String UDP_Server::buildIdentity(String videoHost) {
  String video = String("http://") + videoHost + ":9000/mjpg";
  String sta;
  if (wifiIsStaConnected()) {
    sta = wifiGetStaIp();
  }
  return identityDiscoveryJson(wsName, wsType, UDP_CONTROL_PORT, video,
                               wifiGetApIp(), sta);
}

void UDP_Server::sendIdentity(IPAddress ip, uint16_t port) {
  String host = wifiGetApIp();
  if (wifiIsStaConnected()) {
    String sta = wifiGetStaIp();
    if (sta.length() > 0 && sta != "0.0.0.0") {
      host = sta;
    }
  }
  if (videoUrl.length() > 7) {
    // videoUrl is already "http://IP:9000/mjpg"
    int start = videoUrl.indexOf("://");
    int slash = videoUrl.indexOf("/", start + 3);
    int colon = videoUrl.indexOf(":", start + 3);
    if (start > 0 && colon > start) {
      host = videoUrl.substring(start + 3, colon);
    }
    (void)slash;
  }
  String json = buildIdentity(host);
  udp.beginPacket(ip, port);
  udp.write((const uint8_t *)json.c_str(), json.length());
  udp.endPacket();
}

void UDP_Server::sendBeacon() {
  sendIdentity(IPAddress(255, 255, 255, 255), UDP_CONTROL_PORT);

  IPAddress ap = WiFi.softAPIP();
  if (ap[0] != 0) {
    IPAddress apBcast(ap[0], ap[1], ap[2], 255);
    sendIdentity(apBcast, UDP_CONTROL_PORT);
  }

  if (wifiIsStaConnected()) {
    IPAddress sta = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    IPAddress staBcast((uint8_t)((sta[0] & mask[0]) | (~mask[0] & 0xFF)),
                       (uint8_t)((sta[1] & mask[1]) | (~mask[1] & 0xFF)),
                       (uint8_t)((sta[2] & mask[2]) | (~mask[2] & 0xFF)),
                       (uint8_t)((sta[3] & mask[3]) | (~mask[3] & 0xFF)));
    sendIdentity(staBcast, UDP_CONTROL_PORT);
  }
}

void UDP_Server::handlePacket(IPAddress ip, uint16_t port, uint8_t *data,
                              int len) {
  if (len <= 0) {
    return;
  }
  // Ignore our own beacons / identity JSON echoed back on the broadcast socket.
  if (data[0] == '{') {
    return;
  }
  if (ip == WiFi.softAPIP() || ip == WiFi.localIP()) {
    return;
  }

  if (isProbe(data, len)) {
    sendIdentity(ip, port);
    return;
  }

  if (isKeepalive(data, len)) {
    markPeer(ip, port);
    return;
  }

  markPeer(ip, port);
  forwardControlToUno(data, len);
}

bool UDP_Server::isProbe(uint8_t *data, int len) {
  // Accept GRVR? with optional trailing CR/LF
  while (len > 0 && (data[len - 1] == '\n' || data[len - 1] == '\r' ||
                     data[len - 1] == '\0')) {
    len--;
  }
  return payloadEquals(data, len, "GRVR?");
}

bool UDP_Server::isKeepalive(uint8_t *data, int len) {
  if (len == 1 && data[0] == 0x00) {
    return true;
  }
  while (len > 0 && (data[len - 1] == '\n' || data[len - 1] == '\r')) {
    len--;
  }
  return payloadEquals(data, len, "ping");
}

void UDP_Server::forwardControlToUno(uint8_t *data, int len) {
  serialBridgeForwardToUno(data, len);
}

void UDP_Server::markPeer(IPAddress ip, uint16_t port) {
  peerIP = ip;
  peerPort = port;
  lastPeerDataMs = millis();
  hasPeer = true;
  String detail = String("UDP ") + ip.toString() + ":" + String(port);
  appSessionNote(APP_PEER_UDP, detail.c_str());
}

void UDP_Server::dropPeer(bool notifyUno) {
  if (!hasPeer) {
    return;
  }
  hasPeer = false;
  appSessionDrop(APP_PEER_UDP, notifyUno, "Data timeout from UDP client");
  peerPort = 0;
}

#include "app_session.h"

#include "led_status.h"

static uint8_t peers = 0;
static bool appStopSent = false;

void appSessionNote(AppPeer peer, const char *detail) {
  bool first = peers == 0;
  peers |= (uint8_t)peer;
  appStopSent = false;
  LED_STATUS_CONNECTED();
  if (first) {
    Serial.print("[CONNECTED] Connected ");
    Serial.println(detail ? detail : "");
  }
}

void appSessionDrop(AppPeer peer, bool notifyUno, const char *reason) {
  peers &= (uint8_t) ~(uint8_t)peer;
  if (peers != 0) {
    return;
  }
  if (notifyUno && !appStopSent) {
    Serial.print("[APPSTOP] ");
    Serial.println(reason ? reason : "peer left");
    appStopSent = true;
  }
  LED_STATUS_DISCONNECTED();
}

void appSessionPause(AppPeer peer, const char *reason) {
  uint8_t others = peers & (uint8_t)~(uint8_t)peer;
  if (others != 0 || appStopSent) {
    return;
  }
  Serial.print("[APPSTOP] ");
  Serial.println(reason ? reason : "idle");
  appStopSent = true;
}

bool appSessionHas(AppPeer peer) { return (peers & (uint8_t)peer) != 0; }

void appSessionResetForTest() {
  peers = 0;
  appStopSent = false;
}

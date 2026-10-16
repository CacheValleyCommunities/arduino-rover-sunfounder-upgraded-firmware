#pragma once

#include <Arduino.h>

enum AppPeer : uint8_t {
  APP_PEER_UDP = 1,
  APP_PEER_WS = 2,
  APP_PEER_C2 = 4,
};

void appSessionNote(AppPeer peer, const char *detail);
void appSessionDrop(AppPeer peer, bool notifyUno, const char *reason);
void appSessionPause(AppPeer peer, const char *reason);
bool appSessionHas(AppPeer peer);
void appSessionResetForTest();

#ifndef __WS_SERVER_H__
#define __WS_SERVER_H__

#include "wifi_helper.h"
#include <WebSocketsServer.h>

#define REGIONS                                                                \
  (char[26]){'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',  \
             'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'}
#define REGIONS_LENGTH 26

#define MAX_CLIENTS 5

class WS_Server {
public:
  WS_Server();
  void begin(int port, String name, String type, String check);
  void close();
  void loop();
  void send(String data);
  void sendBIN(uint8_t *payload, size_t length);
};

#endif // __WS_SERVER_H__
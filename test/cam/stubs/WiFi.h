#pragma once

#include "IPAddress.h"

class WiFiClass {
public:
  IPAddress ap{192, 168, 4, 1};
  IPAddress sta{0, 0, 0, 0};
  IPAddress mask{255, 255, 255, 0};

  IPAddress softAPIP() const { return ap; }
  IPAddress localIP() const { return sta; }
  IPAddress subnetMask() const { return mask; }
};

extern WiFiClass WiFi;
void wifiReset();

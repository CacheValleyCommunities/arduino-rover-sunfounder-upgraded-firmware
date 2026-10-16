#include "WiFi.h"

WiFiClass WiFi;

void wifiReset() {
  WiFi.ap = IPAddress(192, 168, 4, 1);
  WiFi.sta = IPAddress(0, 0, 0, 0);
  WiFi.mask = IPAddress(255, 255, 255, 0);
}

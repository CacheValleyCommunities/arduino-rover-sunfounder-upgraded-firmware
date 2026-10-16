#include "wifi_helper.h"

String wifiGetApIp() { return "192.168.4.1"; }
String wifiGetStaIp() { return ""; }
bool wifiIsStaConnected() { return false; }

void wifiBegin() {}
bool wifiConnectAp(String, String, int) { return true; }
bool wifiConnectSta(String, String, uint8_t) { return false; }
void wifiDisconnect() {}
int wifiSetHostname(String) { return 0; }
int wifiScan() { return 0; }
void wifiScanClean() {}
String wifiGetScannedSSID(int) { return ""; }
int32_t wifiGetScannedRSSI(int) { return 0; }
uint8_t wifiGetScannedSecure(int) { return 0; }
int32_t wifiGetScannedChannel(int) { return 0; }
String wifiGetScannedBSSID(int) { return ""; }
String wifiGetMacPrefix() { return "aabbcc"; }
String wifiGetMacAddress() { return "aa:bb:cc:dd:ee:ff"; }

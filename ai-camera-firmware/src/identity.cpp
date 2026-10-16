#include "identity.h"

static String quoted(const String &key, const String &value) {
  return String("\"") + key + "\":\"" + value + "\"";
}

String identityDiscoveryJson(const String &name, const String &type,
                             int udpPort, const String &videoUrl,
                             const String &ap, const String &sta) {
  String json = String("{") + quoted("Name", name) + "," +
                quoted("Type", type) + "," + "\"udp\":" + String(udpPort) +
                "," + quoted("video", videoUrl) + "," + quoted("ap", ap);
  if (sta.length() > 0 && sta != "0.0.0.0") {
    json += String(",") + quoted("sta", sta);
  }
  json += "}";
  return json;
}

String identityWsHelloJson(const String &name, const String &type,
                           const String &check, const String &videoUrl,
                           const String &staIp, const String &videoTemplate) {
  return String("{") + quoted("Name", name) + "," + quoted("Type", type) + "," +
         quoted("Check", check) + "," + quoted("video", videoUrl) + "," +
         quoted("StaIp", staIp) + "," + quoted("VideoTemplate", videoTemplate) +
         "}";
}

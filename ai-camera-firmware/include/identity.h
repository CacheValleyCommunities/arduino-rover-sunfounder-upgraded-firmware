#pragma once

#include <Arduino.h>

String identityDiscoveryJson(const String &name, const String &type,
                             int udpPort, const String &videoUrl,
                             const String &ap, const String &sta);
String identityWsHelloJson(const String &name, const String &type,
                           const String &check, const String &videoUrl,
                           const String &staIp, const String &videoTemplate);

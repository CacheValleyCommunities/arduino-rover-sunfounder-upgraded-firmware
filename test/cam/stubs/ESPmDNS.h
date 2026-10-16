#pragma once

#include "Arduino.h"

class MDNSClass {
public:
  bool addService(const char *, const char *, uint16_t) { return true; }
  bool addServiceTxt(const char *, const char *, const char *, const char *) {
    return true;
  }
};

extern MDNSClass MDNS;

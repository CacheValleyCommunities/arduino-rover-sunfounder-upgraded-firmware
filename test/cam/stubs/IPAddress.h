#pragma once

#include "Arduino.h"

class IPAddress {
public:
  uint8_t oct[4] = {0, 0, 0, 0};

  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    oct[0] = a;
    oct[1] = b;
    oct[2] = c;
    oct[3] = d;
  }

  uint8_t operator[](int i) const { return oct[i]; }
  uint8_t &operator[](int i) { return oct[i]; }

  bool operator==(const IPAddress &o) const {
    return oct[0] == o.oct[0] && oct[1] == o.oct[1] && oct[2] == o.oct[2] &&
           oct[3] == o.oct[3];
  }
  bool operator!=(const IPAddress &o) const { return !(*this == o); }

  String toString() const {
    return String((int)oct[0]) + "." + String((int)oct[1]) + "." +
           String((int)oct[2]) + "." + String((int)oct[3]);
  }
};

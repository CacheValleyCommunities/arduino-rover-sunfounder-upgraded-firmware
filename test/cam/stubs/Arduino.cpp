#include "Arduino.h"

#include <array>

namespace {
unsigned long gMillis = 0;
std::array<uint8_t, 64> gPins{};
} // namespace

CaptureSerial Serial;

unsigned long millis() { return gMillis; }
unsigned long micros() { return gMillis * 1000UL; }

void delay(unsigned long ms) { gMillis += ms; }
void delayMicroseconds(unsigned int us) { gMillis += (us + 999) / 1000; }

void pinMode(uint8_t, uint8_t) {}

void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin < gPins.size()) {
    gPins[pin] = val;
  }
}

int digitalRead(uint8_t pin) {
  if (pin < gPins.size()) {
    return gPins[pin];
  }
  return HIGH;
}

void camMillisSet(unsigned long ms) { gMillis = ms; }
void camMillisAdd(unsigned long ms) { gMillis += ms; }

void camPinsReset() {
  gPins.fill(HIGH);
  gMillis = 0;
  Serial.reset();
}

uint8_t camPin(uint8_t pin) {
  if (pin < gPins.size()) {
    return gPins[pin];
  }
  return HIGH;
}

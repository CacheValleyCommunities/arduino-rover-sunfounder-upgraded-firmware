#include "Arduino.h"
#include "hardware_spy.h"

DummySerial Serial;

unsigned long millis() { return hw.millis_ms; }
unsigned long micros() { return hw.millis_ms * 1000UL; }

void delay(unsigned long) {}
void delayMicroseconds(unsigned int) {}

void pinMode(uint8_t, uint8_t) {}

void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin < 32) {
    hw.digital[pin] = val;
  }
}

int digitalRead(uint8_t pin) {
  if (pin < 32) {
    return hw.digital[pin];
  }
  return HIGH;
}

int analogRead(uint8_t pin) {
  if (pin < 32) {
    return hw.analog[pin];
  }
  return 0;
}

void analogWrite(uint8_t pin, int val) {
  if (pin < 32) {
    hw.pwm[pin] = val;
    hw.pwmWrites++;
  }
}

unsigned long pulseIn(uint8_t, uint8_t, unsigned long) { return hw.pulse_us; }

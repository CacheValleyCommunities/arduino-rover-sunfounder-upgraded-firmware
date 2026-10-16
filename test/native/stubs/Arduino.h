#ifndef Arduino_h
#define Arduino_h

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cmath>
#include <algorithm>

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define LSBFIRST 0
#define MSBFIRST 1
#define CHANGE 1
#define FALLING 2
#define RISING 3
#define HEX 16
#define DEC 10
#define BIN 2

#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19

#define LED_BUILTIN 13
#define PI 3.1415926535897932384626433832795

typedef uint8_t byte;
typedef uint16_t word;
typedef bool boolean;

#define F(x) (x)
#define constrain(amt, low, high)                                              \
  ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout);

class DummySerial {
public:
  void begin(unsigned long) {}
  template <typename T> size_t print(const T &) { return 0; }
  template <typename T> size_t print(const T &, int) { return 0; }
  template <typename T> size_t println(const T &) { return 0; }
  size_t println() { return 0; }
};

extern DummySerial Serial;

#endif

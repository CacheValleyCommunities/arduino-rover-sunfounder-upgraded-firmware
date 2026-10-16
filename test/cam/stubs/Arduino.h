#ifndef Arduino_h
#define Arduino_h

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef ARDUINO
#define ARDUINO 10805
#endif
#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define LED_BUILTIN 13
#define F(x) (x)
#define PROGMEM
#define PGM_P const char *
#define constrain(amt, low, high)                                              \
  ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

typedef uint8_t byte;
typedef bool boolean;

class String {
public:
  std::string s;
  String() = default;
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(uint16_t v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}

  unsigned int length() const { return (unsigned int)s.size(); }
  const char *c_str() const { return s.c_str(); }
  bool isEmpty() const { return s.empty(); }
  void reserve(unsigned int) {}
  unsigned char concat(const char *cstr) {
    if (cstr) {
      s += cstr;
    }
    return 1;
  }
  unsigned char concat(char c) {
    s.push_back(c);
    return 1;
  }
  String &operator=(const char *o) {
    s = o ? o : "";
    return *this;
  }

  bool operator==(const char *o) const { return s == (o ? o : ""); }
  bool operator!=(const char *o) const { return !(*this == o); }
  bool operator==(const String &o) const { return s == o.s; }
  bool operator!=(const String &o) const { return s != o.s; }

  String &operator+=(const String &o) {
    s += o.s;
    return *this;
  }
  String &operator+=(const char *o) {
    if (o) {
      s += o;
    }
    return *this;
  }
  String &operator+=(char c) {
    s.push_back(c);
    return *this;
  }

  int indexOf(const char *needle, unsigned from = 0) const {
    if (!needle) {
      return -1;
    }
    auto pos = s.find(needle, from);
    return pos == std::string::npos ? -1 : (int)pos;
  }
  String substring(unsigned start, unsigned end) const {
    if (start >= s.size() || end <= start) {
      return String("");
    }
    return String(s.substr(start, end - start));
  }
};

inline String operator+(const String &a, const String &b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const String &a, const char *b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const char *a, const String &b) {
  String r(a);
  r += b;
  return r;
}

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);

void camMillisSet(unsigned long ms);
void camMillisAdd(unsigned long ms);
void camPinsReset();
uint8_t camPin(uint8_t pin);

class CaptureSerial {
public:
  std::vector<uint8_t> tx;
  std::vector<uint8_t> rx;
  size_t rxPos = 0;

  void begin(unsigned long) {}
  void reset() {
    tx.clear();
    rx.clear();
    rxPos = 0;
  }
  void loadRx(const uint8_t *data, size_t n) {
    rx.assign(data, data + n);
    rxPos = 0;
  }
  size_t print(const char *s) {
    if (!s) {
      return 0;
    }
    size_t n = std::strlen(s);
    tx.insert(tx.end(), s, s + n);
    return n;
  }
  size_t print(const String &s) { return print(s.c_str()); }
  size_t println(const char *s = "") {
    size_t n = print(s);
    tx.push_back('\n');
    return n + 1;
  }
  size_t println(const String &s) { return println(s.c_str()); }
  size_t write(uint8_t b) {
    tx.push_back(b);
    return 1;
  }
  size_t write(const uint8_t *data, size_t n) {
    if (data && n) {
      tx.insert(tx.end(), data, data + n);
    }
    return n;
  }
  int available() { return (int)(rx.size() - rxPos); }
  int read() {
    if (rxPos >= rx.size()) {
      return -1;
    }
    return rx[rxPos++];
  }
};

extern CaptureSerial Serial;

#endif

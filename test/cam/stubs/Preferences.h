#pragma once

#include "Arduino.h"

#include <map>
#include <string>

class Preferences {
public:
  bool begin(const char *ns, bool readOnly = false) {
    current = ns ? ns : "";
    (void)readOnly;
    return true;
  }
  void end() { current.clear(); }

  bool putString(const char *key, const String &value) {
    strings[current][key] = value.s;
    return true;
  }
  String getString(const char *key, const String &def = "") {
    auto nsIt = strings.find(current);
    if (nsIt == strings.end()) {
      return def;
    }
    auto it = nsIt->second.find(key);
    if (it == nsIt->second.end()) {
      return def;
    }
    return String(it->second);
  }
  bool putBool(const char *key, bool value) {
    bools[current][key] = value;
    return true;
  }
  bool getBool(const char *key, bool def = false) {
    auto nsIt = bools.find(current);
    if (nsIt == bools.end()) {
      return def;
    }
    auto it = nsIt->second.find(key);
    return it == nsIt->second.end() ? def : it->second;
  }
  bool remove(const char *key) {
    strings[current].erase(key);
    bools[current].erase(key);
    return true;
  }

  static void resetAll() {
    strings.clear();
    bools.clear();
  }

private:
  std::string current;
  static std::map<std::string, std::map<std::string, std::string>> strings;
  static std::map<std::string, std::map<std::string, bool>> bools;
};

#pragma once

#include "Arduino.h"

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

enum HTTPMethod {
  HTTP_GET = 1,
  HTTP_POST = 2,
  HTTP_DELETE = 4,
  HTTP_PUT = 8,
  HTTP_PATCH = 16,
  HTTP_HEAD = 32,
  HTTP_OPTIONS = 64,
  HTTP_ANY = 127
};

using THandlerFunction = std::function<void()>;

class WebServer {
public:
  explicit WebServer(int /*port*/ = 80) {}

  void on(const char *uri, HTTPMethod method, THandlerFunction handler) {
    routes[key(uri, method)] = std::move(handler);
  }
  void on(const char *uri, THandlerFunction handler) {
    on(uri, HTTP_ANY, std::move(handler));
  }

  String arg(const char *name) const {
    if (name && std::string(name) == "plain") {
      return plain;
    }
    auto it = args.find(name ? name : "");
    return it == args.end() ? String("") : it->second;
  }
  String arg(const String &name) const { return arg(name.c_str()); }

  void setPlain(const String &body) { plain = body; }

  void sendHeader(const char *name, const String &value) {
    headers.emplace_back(name ? name : "", value.s);
  }
  void sendHeader(const String &name, const String &value) {
    sendHeader(name.c_str(), value);
  }

  void send(int code) {
    lastCode = code;
    lastType.clear();
    lastBody.clear();
  }
  void send(int code, const char *type, const String &content) {
    lastCode = code;
    lastType = type ? type : "";
    lastBody.assign((const uint8_t *)content.c_str(),
                    (const uint8_t *)content.c_str() + content.length());
  }
  void send_P(int code, const char *type, const char *data, size_t len) {
    lastCode = code;
    lastType = type ? type : "";
    lastBody.assign((const uint8_t *)data, (const uint8_t *)data + len);
  }

  bool dispatch(const char *uri, HTTPMethod method) {
    lastCode = 0;
    lastType.clear();
    lastBody.clear();
    headers.clear();
    auto it = routes.find(key(uri, method));
    if (it == routes.end()) {
      it = routes.find(key(uri, HTTP_ANY));
    }
    if (it == routes.end()) {
      lastCode = 404;
      return false;
    }
    it->second();
    return true;
  }

  int lastCode = 0;
  std::string lastType;
  std::vector<uint8_t> lastBody;
  std::vector<std::pair<std::string, std::string>> headers;
  String lastBodyString() const {
    return String(std::string(lastBody.begin(), lastBody.end()));
  }
  bool hasHeader(const char *name, const char *value) const {
    for (const auto &h : headers) {
      if (h.first == name && h.second == value) {
        return true;
      }
    }
    return false;
  }

private:
  static std::pair<std::string, int> key(const char *uri, HTTPMethod method) {
    return {uri ? uri : "", (int)method};
  }
  std::map<std::pair<std::string, int>, THandlerFunction> routes;
  String plain;
  std::map<std::string, String> args;
};

#include "http_helpers.h"

static WebServer *http = nullptr;

#define HEADER_CONTENT_ENCODING F("Content-Encoding")
#define HEADER_CONTENT_ENCODING_GZIP F("gzip")
#define HEADER_CONNECTION F("Connection")
#define HEADER_CONNECTION_CLOSE F("close")

void httpBind(WebServer &server) { http = &server; }

static WebServer &srv() { return *http; }

void httpSetCors() {
  srv().sendHeader("Access-Control-Allow-Origin", "*");
  srv().sendHeader("Access-Control-Allow-Methods",
                   "OPTIONS, GET, POST, PUT, DELETE");
  srv().sendHeader("Access-Control-Allow-Headers",
                   "Origin, X-Requested-With, Content-Type, Accept");
}

void httpSendCloseHeader() {
  srv().sendHeader(HEADER_CONNECTION, HEADER_CONNECTION_CLOSE);
}

void httpSendGzip(const char *mime, const uint8_t *data, size_t len) {
  httpSetCors();
  srv().sendHeader(HEADER_CONTENT_ENCODING, HEADER_CONTENT_ENCODING_GZIP);
  httpSendCloseHeader();
  srv().send_P(200, mime, (const char *)data, len);
}

void httpSendJson(const String &body) {
  httpSetCors();
  httpSendCloseHeader();
  srv().send(200, "application/json", body);
}

void httpSendJsonOk() { httpSendJson("{\"ok\":true}"); }

void httpSendText(int code, const char *mime, const String &body) {
  httpSetCors();
  httpSendCloseHeader();
  srv().send(code, mime, body);
}

void httpSendNoContent() {
  httpSetCors();
  srv().send(204);
}

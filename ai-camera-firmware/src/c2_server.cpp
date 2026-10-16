#include "c2_server.h"

#include <ArduinoJson.h>
#include <pgmspace.h>
#include <string.h>

#include "app_session.h"
#include "c2_program.h"
#include "c2_runtime.h"
#include "http_helpers.h"
#include "serial_bridge.h"
#include "www-c2/c2_blockly.h"
#include "www-c2/c2_css.h"
#include "www-c2/c2_index.h"
#include "www-c2/c2_js.h"

#define C2_HEARTBEAT_TIMEOUT_MS 400
#define ENTITY_MOTORS 0x01
#define ENTITY_RGB 0x02
#define ENTITY_SERVO 0x03
#define ENTITY_LAMP 0x04
#define ENTITY_CONFIG 0x07
#define TELEMETRY_ULTRASONIC 0x81
#define TELEMETRY_IR 0x82
#define TELEMETRY_BATTERY 0x83
#define TELEMETRY_CONFIG 0x85
#define CFG_ID_COUNT 9
#define CFG_TELEMETRY_BYTES 18

static const char *kSettingKeys[CFG_ID_COUNT] = {
    "follow_stop_cm", "follow_slow_cm",    "follow_distance_cm",
    "follow_slow_power", "avoid_threshold_cm", "reverse_cm",
    "move_power",     "ms_per_foot",       "ms_per_deg",
};

static const uint8_t kSettingIds[CFG_ID_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

static int16_t kSettingDefaults[CFG_ID_COUNT] = {12, 25, 50,  30, 35,
                                                15, 70, 1800, 8};

static bool c2Running = false;
static uint32_t c2LastBeatMs = 0;
static bool haveTelemetry = false;
static float distanceCm = -1;
static bool irLeft = false;
static bool irRight = false;
static float batteryV = 0;
static int16_t settingsCache[CFG_ID_COUNT];
static bool settingsInited = false;
static WebServer *c2Http = nullptr;

static void ensureSettingsCache() {
  if (settingsInited) {
    return;
  }
  for (int i = 0; i < CFG_ID_COUNT; i++) {
    settingsCache[i] = kSettingDefaults[i];
  }
  settingsInited = true;
}

static void c2SendMotors(int8_t left, int8_t right) {
  uint8_t frame[3] = {ENTITY_MOTORS, (uint8_t)left, (uint8_t)right};
  serialBridgeForwardToUno(frame, 3);
}

static void c2StopMotors() { c2SendMotors(0, 0); }

static void c2EnsureLinked() {
  c2LastBeatMs = millis();
  appSessionNote(APP_PEER_C2, "C2");
}

static void c2Drop(bool notifyUno) {
  if (!appSessionHas(APP_PEER_C2) && !c2Running) {
    c2Running = false;
    return;
  }
  if (!appSessionHas(APP_PEER_UDP) && !appSessionHas(APP_PEER_WS)) {
    c2StopMotors();
  }
  c2Running = false;
  appSessionDrop(APP_PEER_C2, notifyUno, "C2 heartbeat timeout");
}

static int8_t clampPower(int value) {
  if (value < -100) {
    return -100;
  }
  if (value > 100) {
    return 100;
  }
  return (int8_t)value;
}

static String requestBody() { return c2Http->arg("plain"); }

static void parseBody(JsonDocument &doc) {
  deserializeJson(doc, requestBody());
}

static void handleOptions() { httpSendNoContent(); }

static void handleGetIndex() {
  httpSendGzip("text/html", c2_index_html_gz, c2_index_html_gz_len);
}

static void handleGetJs() {
  httpSendGzip("text/javascript", c2_js_gz, c2_js_gz_len);
}

static void handleGetCss() { httpSendGzip("text/css", c2_css_gz, c2_css_gz_len); }

static void handleGetBlockly() {
  httpSendGzip("text/javascript", c2_blockly_js_gz, c2_blockly_js_gz_len);
}

static void handleGetState() {
  ensureSettingsCache();
  JsonDocument doc;
  JsonObject sensors = doc["sensors"].to<JsonObject>();
  if (!haveTelemetry || distanceCm < 0 || distanceCm > 300) {
    sensors["distance_cm"] = -1;
  } else {
    sensors["distance_cm"] = distanceCm;
  }
  sensors["ir_left"] = irLeft;
  sensors["ir_right"] = irRight;
  sensors["battery_v"] = batteryV;
  JsonObject settings = doc["settings"].to<JsonObject>();
  for (int i = 0; i < CFG_ID_COUNT; i++) {
    settings[kSettingKeys[i]] = settingsCache[i];
  }
  doc["running"] = c2Running || c2ProgramIsRunning();
  doc["deployed"] = c2ProgramIsStored();
  doc["onboard"] = c2ProgramIsRunning();
  String out;
  serializeJson(doc, out);
  httpSendJson(out);
}

static void handleHeartbeat() {
  c2EnsureLinked();
  c2Running = true;
  httpSendJsonOk();
}

static void handleStop() {
  c2ProgramRequestStop();
  c2StopMotors();
  c2Running = false;
  httpSendJsonOk();
}

static void handleGetProgram() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["deployed"] = c2ProgramIsStored();
  doc["onboard"] = c2ProgramIsRunning();
  String raw;
  c2ProgramCopyJson(raw);
  if (raw.length() > 0) {
    doc["program"] = serialized(raw.c_str());
  } else {
    doc["program"] = nullptr;
  }
  String out;
  serializeJson(doc, out);
  httpSendJson(out);
}

static void handlePostProgram() {
  if (!c2ProgramStoreAndStart(requestBody())) {
    httpSendText(400, "application/json",
                 "{\"ok\":false,\"error\":\"bad program\"}");
    return;
  }
  c2EnsureLinked();
  c2Running = true;
  httpSendJsonOk();
}

static void handleDeleteProgram() {
  c2ProgramClear();
  c2StopMotors();
  c2Running = false;
  httpSendJsonOk();
}

static void handleMotors() {
  c2EnsureLinked();
  c2Running = true;
  JsonDocument doc;
  parseBody(doc);
  c2SendMotors(clampPower(doc["left"] | 0), clampPower(doc["right"] | 0));
  httpSendJsonOk();
}

static void handleRgb() {
  c2EnsureLinked();
  JsonDocument doc;
  parseBody(doc);
  uint8_t frame[4] = {
      ENTITY_RGB,
      (uint8_t)constrain((int)(doc["r"] | 0), 0, 255),
      (uint8_t)constrain((int)(doc["g"] | 0), 0, 255),
      (uint8_t)constrain((int)(doc["b"] | 0), 0, 255),
  };
  serialBridgeForwardToUno(frame, 4);
  httpSendJsonOk();
}

static void handleLamp() {
  c2EnsureLinked();
  JsonDocument doc;
  parseBody(doc);
  uint8_t frame[2] = {ENTITY_LAMP,
                      (doc["on"] | false) ? (uint8_t)1 : (uint8_t)0};
  serialBridgeForwardToUno(frame, 2);
  httpSendJsonOk();
}

static void handleServo() {
  c2EnsureLinked();
  JsonDocument doc;
  parseBody(doc);
  uint8_t frame[2] = {
      ENTITY_SERVO, (uint8_t)constrain((int)(doc["angle"] | 90), 0, 140)};
  serialBridgeForwardToUno(frame, 2);
  httpSendJsonOk();
}

static void handleSettings() {
  ensureSettingsCache();
  c2EnsureLinked();
  JsonDocument doc;
  parseBody(doc);
  uint8_t frame[2 + CFG_ID_COUNT * 3];
  uint8_t count = 0;
  uint8_t *out = frame + 2;
  for (int i = 0; i < CFG_ID_COUNT; i++) {
    if (doc[kSettingKeys[i]].isNull()) {
      continue;
    }
    int16_t value = (int16_t)doc[kSettingKeys[i]].as<int>();
    settingsCache[i] = value;
    *out++ = kSettingIds[i];
    *out++ = (uint8_t)((value >> 8) & 0xFF);
    *out++ = (uint8_t)(value & 0xFF);
    count++;
  }
  if (count > 0) {
    frame[0] = ENTITY_CONFIG;
    frame[1] = count;
    serialBridgeForwardToUno(frame, 2 + count * 3);
  }
  httpSendJsonOk();
}

void c2Begin(WebServer &http) {
  c2Http = &http;
  ensureSettingsCache();
  http.on("/c2", HTTP_GET, handleGetIndex);
  http.on("/c2/", HTTP_GET, handleGetIndex);
  http.on("/c2/c2.js", HTTP_GET, handleGetJs);
  http.on("/c2/c2.css", HTTP_GET, handleGetCss);
  http.on("/c2/blockly.js", HTTP_GET, handleGetBlockly);
  http.on("/c2/api/state", HTTP_GET, handleGetState);
  http.on("/c2/api/state", HTTP_OPTIONS, handleOptions);
  http.on("/c2/api/heartbeat", HTTP_POST, handleHeartbeat);
  http.on("/c2/api/heartbeat", HTTP_OPTIONS, handleOptions);
  http.on("/c2/api/stop", HTTP_POST, handleStop);
  http.on("/c2/api/stop", HTTP_OPTIONS, handleOptions);
  http.on("/c2/api/motors", HTTP_POST, handleMotors);
  http.on("/c2/api/motors", HTTP_OPTIONS, handleOptions);
  http.on("/c2/api/rgb", HTTP_POST, handleRgb);
  http.on("/c2/api/rgb", HTTP_OPTIONS, handleOptions);
  http.on("/c2/api/lamp", HTTP_POST, handleLamp);
  http.on("/c2/api/lamp", HTTP_OPTIONS, handleOptions);
  http.on("/c2/api/servo", HTTP_POST, handleServo);
  http.on("/c2/api/servo", HTTP_OPTIONS, handleOptions);
  http.on("/c2/api/settings", HTTP_POST, handleSettings);
  http.on("/c2/api/settings", HTTP_OPTIONS, handleOptions);
  http.on("/c2/api/program", HTTP_GET, handleGetProgram);
  http.on("/c2/api/program", HTTP_POST, handlePostProgram);
  http.on("/c2/api/program", HTTP_DELETE, handleDeleteProgram);
  http.on("/c2/api/program", HTTP_OPTIONS, handleOptions);
  c2ProgramBegin();
}

void c2Loop() {
  if (c2ProgramIsRunning()) {
    c2LastBeatMs = millis();
    c2Running = true;
    return;
  }
  if (!appSessionHas(APP_PEER_C2) && !c2Running) {
    return;
  }
  if (millis() - c2LastBeatMs <= C2_HEARTBEAT_TIMEOUT_MS) {
    return;
  }
  c2Drop(true);
}

void c2RuntimeLink() { c2EnsureLinked(); }

void c2RuntimeMotors(int left, int right) {
  c2EnsureLinked();
  c2Running = true;
  c2SendMotors(clampPower(left), clampPower(right));
}

void c2RuntimeRgb(int r, int g, int b) {
  c2EnsureLinked();
  uint8_t frame[4] = {
      ENTITY_RGB,
      (uint8_t)constrain(r, 0, 255),
      (uint8_t)constrain(g, 0, 255),
      (uint8_t)constrain(b, 0, 255),
  };
  serialBridgeForwardToUno(frame, 4);
}

void c2RuntimeLamp(bool on) {
  c2EnsureLinked();
  uint8_t frame[2] = {ENTITY_LAMP, on ? (uint8_t)1 : (uint8_t)0};
  serialBridgeForwardToUno(frame, 2);
}

void c2RuntimeServo(int angle) {
  c2EnsureLinked();
  uint8_t frame[2] = {ENTITY_SERVO, (uint8_t)constrain(angle, 0, 140)};
  serialBridgeForwardToUno(frame, 2);
}

void c2RuntimeSetPower(int power) {
  ensureSettingsCache();
  c2EnsureLinked();
  settingsCache[6] = (int16_t)power;
  uint8_t frame[5] = {
      ENTITY_CONFIG, 1, 7,
      (uint8_t)((power >> 8) & 0xFF), (uint8_t)(power & 0xFF),
  };
  serialBridgeForwardToUno(frame, 5);
}

float c2RuntimeDistance() { return distanceCm; }

bool c2RuntimeIrLeft() { return irLeft; }

bool c2RuntimeIrRight() { return irRight; }

float c2RuntimeBattery() { return batteryV; }

int c2RuntimeSetting(const char *key) {
  ensureSettingsCache();
  if (key == nullptr) {
    return 0;
  }
  for (int i = 0; i < CFG_ID_COUNT; i++) {
    if (strcmp(kSettingKeys[i], key) == 0) {
      return settingsCache[i];
    }
  }
  return 0;
}

void c2OnUnoTelemetry(const uint8_t *data, size_t len) {
  if (data == nullptr || len < 11 || data[0] != 0xA0 ||
      data[len - 1] != 0xA1) {
    return;
  }
  size_t i = 3;
  while (i < len - 1) {
    switch (data[i]) {
    case TELEMETRY_ULTRASONIC:
      if (i + 2 >= len - 1) {
        return;
      }
      {
        uint16_t mm = ((uint16_t)data[i + 1] << 8) | data[i + 2];
        distanceCm = (mm == 0 || mm > 30000) ? -1.0f : mm / 10.0f;
      }
      i += 3;
      break;
    case TELEMETRY_IR:
      if (i + 1 >= len - 1) {
        return;
      }
      irLeft = (data[i + 1] & 0b10) != 0;
      irRight = (data[i + 1] & 0b01) != 0;
      i += 2;
      break;
    case TELEMETRY_BATTERY:
      if (i + 1 >= len - 1) {
        return;
      }
      batteryV = 6.0f + data[i + 1] / 100.0f;
      i += 2;
      break;
    case TELEMETRY_CONFIG:
      if (i + CFG_TELEMETRY_BYTES >= len - 1) {
        return;
      }
      ensureSettingsCache();
      for (int n = 0; n < CFG_ID_COUNT; n++) {
        settingsCache[n] = (int16_t)(((uint16_t)data[i + 1 + n * 2] << 8) |
                                     data[i + 2 + n * 2]);
      }
      i += 1 + CFG_TELEMETRY_BYTES;
      break;
    default:
      i++;
      break;
    }
  }
  haveTelemetry = true;
}

void c2ResetForTest() {
  c2Running = false;
  c2LastBeatMs = 0;
  haveTelemetry = false;
  distanceCm = -1;
  irLeft = false;
  irRight = false;
  batteryV = 0;
  settingsInited = false;
  c2Http = nullptr;
}

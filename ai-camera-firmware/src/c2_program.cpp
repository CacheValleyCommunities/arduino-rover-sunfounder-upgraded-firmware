#include "c2_program.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <cmath>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "c2_runtime.h"

#define C2_PROGRAM_MAX 3072
#define C2_PREFS_NS "c2prog"
#define C2_PREFS_JSON "json"
#define C2_PREFS_AUTORUN "autorun"

enum C2ProgStatus { C2_PROG_OK = 0, C2_PROG_STOP = 1, C2_PROG_ABORT = 2 };

static String programRaw;
static volatile bool programAbort = false;
static volatile bool programRunning = false;
static TaskHandle_t programTask = nullptr;

static C2ProgStatus runSteps(JsonVariantConst steps);
static C2ProgStatus runStep(JsonVariantConst step);

static void persist(bool autorun) {
  Preferences prefs;
  prefs.begin(C2_PREFS_NS, false);
  if (programRaw.length() == 0) {
    prefs.remove(C2_PREFS_JSON);
    prefs.putBool(C2_PREFS_AUTORUN, false);
  } else {
    prefs.putString(C2_PREFS_JSON, programRaw);
    prefs.putBool(C2_PREFS_AUTORUN, autorun);
  }
  prefs.end();
}

static C2ProgStatus sleepMs(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    if (programAbort) {
      return C2_PROG_ABORT;
    }
    c2RuntimeLink();
    delay(20);
  }
  return C2_PROG_OK;
}

static double evalNumber(JsonVariantConst node) {
  if (node.isNull()) {
    return 0;
  }
  if (node.is<const char *>()) {
    const char *key = node.as<const char *>();
    if (strcmp(key, "distance_cm") == 0) {
      return c2RuntimeDistance();
    }
    if (strcmp(key, "ir_left") == 0) {
      return c2RuntimeIrLeft() ? 1.0 : 0.0;
    }
    if (strcmp(key, "ir_right") == 0) {
      return c2RuntimeIrRight() ? 1.0 : 0.0;
    }
    if (strcmp(key, "battery_v") == 0) {
      return c2RuntimeBattery();
    }
    return (double)c2RuntimeSetting(key);
  }
  if (node.is<JsonObjectConst>()) {
    JsonObjectConst obj = node.as<JsonObjectConst>();
    const char *op = obj["op"] | "eq";
    double a = evalNumber(obj["a"]);
    double b = evalNumber(obj["b"]);
    if (strcmp(op, "neq") == 0) {
      return a != b;
    }
    if (strcmp(op, "lt") == 0) {
      return a < b;
    }
    if (strcmp(op, "lte") == 0) {
      return a <= b;
    }
    if (strcmp(op, "gt") == 0) {
      return a > b;
    }
    if (strcmp(op, "gte") == 0) {
      return a >= b;
    }
    return a == b;
  }
  if (node.is<bool>()) {
    return node.as<bool>() ? 1.0 : 0.0;
  }
  return node.as<double>();
}

static uint32_t parseWaitMs(JsonVariantConst spec) {
  if (spec.is<float>() || spec.is<double>() || spec.is<int>()) {
    return (uint32_t)(spec.as<double>() * 1000.0);
  }
  const char *text = spec.as<const char *>();
  if (text == nullptr) {
    return 1000;
  }
  return (uint32_t)(atof(text) * 1000.0);
}

static void parseMove(const char *spec, float *amount, const char **unit) {
  *amount = spec ? atof(spec) : 1.0f;
  *unit = "sec";
  if (spec == nullptr) {
    return;
  }
  if (strstr(spec, "ft") != nullptr) {
    *unit = "ft";
  } else if (strstr(spec, "cm") != nullptr) {
    *unit = "cm";
  }
}

static C2ProgStatus doMove(const char *direction, const char *spec) {
  float amount = 1;
  const char *unit = "sec";
  parseMove(spec, &amount, &unit);
  int power = c2RuntimeSetting("move_power");
  if (power == 0) {
    power = 70;
  }
  int sign = strcmp(direction, "backward") == 0 ? -1 : 1;
  int msFoot = c2RuntimeSetting("ms_per_foot");
  if (msFoot <= 0) {
    msFoot = 1800;
  }
  uint32_t ms = (uint32_t)(amount * 1000.0f);
  if (strcmp(unit, "ft") == 0) {
    ms = (uint32_t)(amount * (float)msFoot);
  } else if (strcmp(unit, "cm") == 0) {
    ms = (uint32_t)(amount * ((float)msFoot / 30.48f));
  }
  c2RuntimeMotors(sign * power, sign * power);
  C2ProgStatus status = sleepMs(ms);
  c2RuntimeMotors(0, 0);
  return status;
}

static C2ProgStatus doTurn(const char *spec) {
  if (spec == nullptr) {
    return C2_PROG_OK;
  }
  bool left = strstr(spec, "left") != nullptr;
  float deg = 90;
  const char *space = strrchr(spec, ' ');
  if (space != nullptr) {
    deg = atof(space + 1);
  } else {
    deg = atof(spec);
  }
  int power = c2RuntimeSetting("move_power");
  if (power == 0) {
    power = 70;
  }
  int msDeg = c2RuntimeSetting("ms_per_deg");
  if (msDeg <= 0) {
    msDeg = 8;
  }
  uint32_t ms = (uint32_t)(fabsf(deg) * (float)msDeg);
  int leftPower = left ? -power : power;
  int rightPower = left ? power : -power;
  c2RuntimeMotors(leftPower, rightPower);
  C2ProgStatus status = sleepMs(ms);
  c2RuntimeMotors(0, 0);
  return status;
}

static C2ProgStatus runStep(JsonVariantConst step) {
  if (programAbort) {
    return C2_PROG_ABORT;
  }
  if (!step.is<JsonObjectConst>()) {
    return C2_PROG_OK;
  }
  if (step["when"] == "start" || !step["on_start"].isNull()) {
    JsonVariantConst body =
        step["do"].isNull() ? step["on_start"] : step["do"];
    return runSteps(body);
  }
  if (!step["wait"].isNull()) {
    return sleepMs(parseWaitMs(step["wait"]));
  }
  if (!step["repeat"].isNull()) {
    int times = (int)evalNumber(step["repeat"]);
    for (int i = 0; i < times; i++) {
      C2ProgStatus status = runSteps(step["steps"]);
      if (status != C2_PROG_OK) {
        return status;
      }
    }
    return C2_PROG_OK;
  }
  if (!step["forever"].isNull()) {
    while (!programAbort) {
      C2ProgStatus status = runSteps(step["forever"]);
      if (status != C2_PROG_OK) {
        return status;
      }
    }
    return C2_PROG_ABORT;
  }
  if (!step["if"].isNull()) {
    if (evalNumber(step["if"]) != 0) {
      return runSteps(step["do"]);
    }
    if (!step["else"].isNull()) {
      return runSteps(step["else"]);
    }
    return C2_PROG_OK;
  }
  if (!step["wait_until"].isNull()) {
    while (!programAbort) {
      if (evalNumber(step["wait_until"]) != 0) {
        return C2_PROG_OK;
      }
      C2ProgStatus status = sleepMs(80);
      if (status != C2_PROG_OK) {
        return status;
      }
    }
    return C2_PROG_ABORT;
  }
  const char *stop = step["stop"].as<const char *>();
  if (stop != nullptr) {
    c2RuntimeMotors(0, 0);
    if (strcmp(stop, "all") == 0) {
      return C2_PROG_STOP;
    }
    return C2_PROG_OK;
  }
  if (!step["move_forward"].isNull()) {
    return doMove("forward", step["move_forward"].as<const char *>());
  }
  if (!step["move_backward"].isNull()) {
    return doMove("backward", step["move_backward"].as<const char *>());
  }
  if (!step["turn"].isNull()) {
    return doTurn(step["turn"].as<const char *>());
  }
  if (!step["set_power"].isNull()) {
    c2RuntimeSetPower((int)evalNumber(step["set_power"]));
    return C2_PROG_OK;
  }
  if (step["set_color"].is<JsonArrayConst>()) {
    JsonArrayConst color = step["set_color"].as<JsonArrayConst>();
    c2RuntimeRgb((int)evalNumber(color[0]), (int)evalNumber(color[1]),
                 (int)evalNumber(color[2]));
    return C2_PROG_OK;
  }
  if (!step["lamp"].isNull()) {
    const char *lamp = step["lamp"].as<const char *>();
    c2RuntimeLamp(lamp != nullptr && strcmp(lamp, "on") == 0);
    return C2_PROG_OK;
  }
  if (!step["look"].isNull()) {
    c2RuntimeServo((int)evalNumber(step["look"]));
    return C2_PROG_OK;
  }
  return C2_PROG_OK;
}

static C2ProgStatus runSteps(JsonVariantConst steps) {
  if (!steps.is<JsonArrayConst>()) {
    return C2_PROG_OK;
  }
  for (JsonVariantConst step : steps.as<JsonArrayConst>()) {
    C2ProgStatus status = runStep(step);
    if (status != C2_PROG_OK) {
      return status;
    }
  }
  return C2_PROG_OK;
}

static void programTaskFn(void * /*arg*/) {
  programRunning = true;
  delay(300);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, programRaw);
  if (!err) {
    runSteps(doc["steps"]);
  }
  c2RuntimeMotors(0, 0);
  programRunning = false;
  programTask = nullptr;
  vTaskDelete(nullptr);
}

static bool startTask() {
  if (programRaw.length() == 0) {
    return false;
  }
  c2ProgramStop();
  programAbort = false;
  BaseType_t ok =
      xTaskCreate(programTaskFn, "c2prog", 8192, nullptr, 1, &programTask);
  return ok == pdPASS;
}

void c2ProgramBegin() {
  Preferences prefs;
  prefs.begin(C2_PREFS_NS, true);
  programRaw = prefs.getString(C2_PREFS_JSON, "");
  bool autorun = prefs.getBool(C2_PREFS_AUTORUN, false);
  prefs.end();
  if (autorun && programRaw.length() > 0) {
    startTask();
  }
}

bool c2ProgramStoreAndStart(const String &json) {
  if (json.length() == 0 || json.length() > C2_PROGRAM_MAX) {
    return false;
  }
  JsonDocument probe;
  DeserializationError err = deserializeJson(probe, json);
  if (err || !probe["steps"].is<JsonArray>()) {
    return false;
  }
  c2ProgramStop();
  programRaw = json;
  persist(true);
  return startTask();
}

void c2ProgramRequestStop() { programAbort = true; }

void c2ProgramStop() {
  c2ProgramRequestStop();
  uint32_t start = millis();
  while (programRunning && (millis() - start) < 2000) {
    delay(10);
  }
  if (!programRunning) {
    programAbort = false;
  }
}

void c2ProgramClear() {
  c2ProgramStop();
  programRaw = "";
  persist(false);
}

bool c2ProgramIsRunning() { return programRunning; }

bool c2ProgramIsStored() { return programRaw.length() > 0; }

void c2ProgramCopyJson(String &out) { out = programRaw; }

void c2ProgramResetForTest() {
  programAbort = true;
  programRunning = false;
  programTask = nullptr;
  programAbort = false;
  programRaw = "";
}

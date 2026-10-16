#include "settings.h"

#include "camera.h"
#include "esp_camera.h"
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>

#include "c2_server.h"
#include "defaults.h"
#include "http_helpers.h"
#include "log.h"
#include "wifi_helper.h"
#include "www/css.h"
#include "www/favicon.h"
#include "www/index.h"
#include "www/js.h"

WebServer server(80);
Preferences preferences;

String version; // xxx.xxx.xxx
String name;
String type;
String apSsid;
String apPassword;
int apChannel;
String staSsid;
String staPassword;
bool camHFlip;
bool camVFlip;
int camBrightness;
int camContrast;
int camSaturation;
int camSharpness;
int camJpegQuality;
String camFramesize;

bool needReboot = false;

static bool prefsPutString(const char *key, const String &value) {
  preferences.begin(SETTING_PREFERENCES, false);
  bool result = preferences.putString(key, value);
  preferences.end();
  return result;
}

static bool prefsPutInt(const char *key, int value) {
  preferences.begin(SETTING_PREFERENCES, false);
  bool result = preferences.putInt(key, value);
  preferences.end();
  return result;
}

static bool prefsPutBool(const char *key, bool value) {
  preferences.begin(SETTING_PREFERENCES, false);
  bool result = preferences.putBool(key, value);
  preferences.end();
  return result;
}

void returnOk() { httpSendText(200, "text/plain", "OK"); }

void returnNameNotFound(String name) {
  String msg = String("Bad Request") + name + " not found";
  error(msg.c_str());
  httpSendText(400, "text/plain", msg);
}

void returnSetError(String name) {
  preferences.begin(SETTING_PREFERENCES, false);
  size_t free = preferences.freeEntries();
  preferences.end();
  String msg =
      String("Set \"") + name + "\" error, free entries: " + String(free);
  error(msg.c_str());
  httpSendText(502, "text/plain", msg);
}

void postStringHandler(const char *name, bool (*callback)(String)) {
  if (server.hasArg(name)) {
    bool result = callback(server.arg(name));
    if (result) {
      returnOk();
    } else {
      returnSetError(name);
    }
  } else {
    returnNameNotFound(name);
  }
}
void postIntHandler(const char *name, bool (*callback)(int)) {
  if (server.hasArg(name)) {
    int value = server.arg(name).toInt();
    bool result = callback(value);
    if (result) {
      returnOk();
    } else {
      returnSetError(name);
    }
  } else {
    returnNameNotFound(name);
  }
}
void postBoolHandler(const char *name, bool (*callback)(bool)) {
  if (server.hasArg(name)) {
    String value = server.arg(name);
    bool bValue = (value == "true" || value == "1");
    bool result = callback(bValue);
    if (result) {
      returnOk();
    } else {
      returnSetError(name);
    }
  } else {
    returnNameNotFound(name);
  }
}

void handleGetIndex() {
  httpSendGzip("text/html", index_html_gz, index_html_gz_len);
}
void handleGetJs() {
  httpSendGzip("text/javascript", main_js_gz, main_js_gz_len);
}
void handleGetCss() {
  httpSendGzip("text/css", main_css_gz, main_css_gz_len);
}
void handleGetFavicon() {
  httpSendGzip("image/x-icon", favicon_ico_gz, favicon_ico_gz_len);
}
void handleGetSettings() {
  httpSendJson(
      String("{") + "\"version\":\"" + version + "\"," + "\"name\":\"" + name +
          "\"," + "\"type\":\"" + type + "\"," + "\"apSsid\":\"" + apSsid +
          "\"," + "\"apPassword\":\"" + apPassword + "\"," +
          "\"apChannel\":" + String(apChannel) + "," + "\"staSsid\":\"" +
          staSsid + "\"," + "\"staPassword\":\"" + staPassword + "\"," +
          "\"cameraHorizontalMirror\":" + (camHFlip ? "true" : "false") + "," +
          "\"cameraVerticalFlip\":" + (camVFlip ? "true" : "false") + "," +
          "\"cameraBrightness\":" + String(camBrightness) + "," +
          "\"cameraContrast\":" + String(camContrast) + "," +
          "\"cameraSaturation\":" + String(camSaturation) + "," +
          "\"cameraSharpness\":" + String(camSharpness) + "," +
          "\"cameraJpegQuality\":" + String(camJpegQuality) + "," +
          "\"cameraFramesize\":\"" + camFramesize + "\"," +
          "\"macAddress\":\"" + wifiGetMacAddress() +
          "\"," + // Mac address: xx:xx:xx:xx:xx:xx
          "\"macPrefix\":\"" + wifiGetMacPrefix() +
          "\"," + // Mac address prefix: xxxxxx
          "\"staConnected\":\"" + (wifiIsStaConnected() ? "true" : "false") +
          "\"," + "\"ipAddress\":\"" + wifiGetStaIp() + "\"," +
          "\"needReboot\":\"" + (needReboot ? "true" : "false") +
          "\""
          "}");
}

void handleSetName() {
  postStringHandler("name", settingsSetName);
}
void handleSetType() {
  postStringHandler("type", settingsSetType);
}
void handleSetApSsid() {
  postStringHandler("apSsid", settingsSetApSsid);
}
void handleSetApPassword() {
  postStringHandler("apPassword", settingsSetApPassword);
}
void handleSetApChannel() {
  postIntHandler("apChannel", settingsSetApChannel);
}
void handleSetCameraHorizontalMirror() {
  postBoolHandler("camHFlip", settingsSetCameraHorizontalMirror);
  sensor_t *s = esp_camera_sensor_get();
  s->set_hmirror(s, camHFlip);
}
void handleSetCameraVerticalFlip() {
  postBoolHandler("camVFlip", settingsSetCameraVerticalFlip);
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, camVFlip);
}
void handleSetCameraBrightness() {
  postIntHandler("camBrightness", settingsSetCameraBrightness);
  sensor_t *s = esp_camera_sensor_get();
  s->set_brightness(s, camBrightness);
}
void handleSetCameraContrast() {
  postIntHandler("camContrast", settingsSetCameraContrast);
  sensor_t *s = esp_camera_sensor_get();
  s->set_contrast(s, camContrast);
}
void handleSetCameraSaturation() {
  postIntHandler("camSaturation", settingsSetCameraSaturation);
  sensor_t *s = esp_camera_sensor_get();
  s->set_saturation(s, camSaturation);
}
void handleSetCameraSharpness() {
  postIntHandler("camSharpness", settingsSetCameraSharpness);
  sensor_t *s = esp_camera_sensor_get();
  s->set_sharpness(s, camSharpness);
}
void handleSetCameraJpegQuality() {
  postIntHandler("camJpegQuality", settingsSetCameraJpegQuality);
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_quality(s, camJpegQuality);
  }
}
void handleSetCameraFramesize() {
  if (!server.hasArg("camFramesize")) {
    returnNameNotFound("camFramesize");
    return;
  }
  if (!settingsSetCameraFramesize(server.arg("camFramesize"))) {
    httpSendText(400, "text/plain", "camFramesize must be QVGA, HVGA, or VGA");
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, settingsGetCameraFramesize());
  }
  returnOk();
}
static bool _updateError = false;
static String _updateErrorMsg = "";

void handleUpdateReturn() {
  if (_updateError) {
    Serial.println(_updateErrorMsg);
    httpSendText(400, "text/plain", _updateErrorMsg);
  } else if (Update.hasError()) {
    String msg = "Update failed: " + String(Update.errorString());
    Serial.println(msg);
    httpSendText(400, "text/plain", msg);
  } else {
    returnOk();
  }
}
void handleUpdate() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    _updateError = false;
    _updateErrorMsg = "";
    camera_stop();
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      _updateError = true;
      _updateErrorMsg = "Update begin failed: " + String(Update.errorString());
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      _updateError = true;
      _updateErrorMsg = "Update write failed: " + String(Update.errorString());
      return;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u\n", upload.totalSize);
    } else {
      _updateError = true;
      _updateErrorMsg = "Update end failed: " + String(Update.errorString());
      return;
    }
  }
}
void handleScanWifi() {
  uint8_t count = wifiScan();
  String json = "[";
  for (uint8_t i = 0; i < count; i++) {
    json += "{";
    json += "\"ssid\":\"" + wifiGetScannedSSID(i) + "\",";
    json += "\"rssi\":" + String(wifiGetScannedRSSI(i)) + ",";
    json += "\"secure\":" + String(wifiGetScannedSecure(i)) + ",";
    json += "\"channel\":" + String(wifiGetScannedChannel(i)) + ",";
    json += "\"bssid\":\"" + wifiGetScannedBSSID(i) + "\"";
    json += "},";
  }
  if (count > 0) {
    json.remove(json.length() - 1); // Remove the last comma
  }
  json += "]";
  httpSendJson(json);
}
void handleSetSta() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  if (ssid.length() <= 0 || ssid.length() > 32) {
    httpSendText(400, "text/plain", "SSID length should be between 1 and 32.");
    return;
  }
  if (password.length() <= 7 || password.length() > 64) {
    httpSendText(400, "text/plain",
                "Password length should be between 8 and 64.");
    return;
  }
  bool r = wifiConnectSta(ssid, password, 5);
  if (r) {
    settingsSetStaSsid(ssid);
    settingsSetStaPassword(password);
    httpSendText(200, "text/plain", wifiGetStaIp());
  } else {
    httpSendText(400, "text/plain", "Wi-Fi Connection failed.");
  }
}
void handleRestart() {
  returnOk();
  delay(1000);
  ESP.restart();
}

void settingsBegin(String _version) {
  version = _version;
  settingsReadConfig();
  httpBind(server);

  String js_url = String("/static/js/main.") + JS_HASH + ".js";
  String css_url = String("/static/css/main.") + CSS_HASH + ".css";

  server.on("/", HTTP_GET, handleGetIndex);
  server.on(js_url.c_str(), HTTP_GET, handleGetJs);
  server.on(css_url.c_str(), HTTP_GET, handleGetCss);
  server.on("/favicon.ico", HTTP_GET, handleGetFavicon);
  server.on("/settings", HTTP_GET, handleGetSettings);
  server.on("/set-name", HTTP_POST, handleSetName);
  server.on("/set-type", HTTP_POST, handleSetType);
  server.on("/set-apSsid", HTTP_POST, handleSetApSsid);
  server.on("/set-apPassword", HTTP_POST, handleSetApPassword);
  server.on("/set-apChannel", HTTP_POST, handleSetApChannel);
  server.on("/set-cameraHorizontalMirror", HTTP_POST,
            handleSetCameraHorizontalMirror);
  server.on("/set-cameraVerticalFlip", HTTP_POST, handleSetCameraVerticalFlip);
  server.on("/set-cameraBrightness", HTTP_POST, handleSetCameraBrightness);
  server.on("/set-cameraContrast", HTTP_POST, handleSetCameraContrast);
  server.on("/set-cameraSaturation", HTTP_POST, handleSetCameraSaturation);
  server.on("/set-cameraSharpness", HTTP_POST, handleSetCameraSharpness);
  server.on("/set-cameraJpegQuality", HTTP_POST, handleSetCameraJpegQuality);
  server.on("/set-cameraFramesize", HTTP_POST, handleSetCameraFramesize);
  server.on("/update", HTTP_POST, handleUpdateReturn, handleUpdate);
  server.on("/scan-wifi", HTTP_GET, handleScanWifi);
  server.on("/set-sta", HTTP_POST, handleSetSta);
  server.on("/restart", HTTP_POST, handleRestart);
  c2Begin(server);
  server.begin();
}

void settingsReadConfig() {
  preferences.begin(SETTING_PREFERENCES, false);
  name = preferences.getString("name", DEFAULT_NAME);
  type = preferences.getString("type", DEFAULT_TYPE);
  apSsid = preferences.getString("apSsid", DEFAULT_AP_SSID);
  apPassword = preferences.getString("apPassword", DEFAULT_AP_PASSWORD);
  apChannel = preferences.getInt("apChannel", DEFAULT_AP_CHANNEL);
  staSsid = preferences.getString("staSsid", DEFAULT_STA_SSID);
  staPassword = preferences.getString("staPassword", DEFAULT_STA_PASSWORD);
  camHFlip = preferences.getBool("camHFlip", DEFAULT_CAMERA_HORIZONTAL_MIRROR);
  camVFlip = preferences.getBool("camVFlip", DEFAULT_CAMERA_VERTICAL_FLIP);
  camBrightness =
      preferences.getInt("camBrightness", DEFAULT_CAMERA_BRIGHTNESS);
  camContrast = preferences.getInt("camContrast", DEFAULT_CAMERA_CONTRAST);
  camSaturation =
      preferences.getInt("camSaturation", DEFAULT_CAMERA_SATURATION);
  camSharpness = preferences.getInt("camSharpness", DEFAULT_CAMERA_SHARPNESS);
  camJpegQuality =
      preferences.getInt("camJpegQuality", DEFAULT_CAMERA_JPEG_QUALITY);
  if (camJpegQuality < DEFAULT_CAMERA_JPEG_QUALITY_MIN) {
    camJpegQuality = DEFAULT_CAMERA_JPEG_QUALITY_MIN;
  } else if (camJpegQuality > DEFAULT_CAMERA_JPEG_QUALITY_MAX) {
    camJpegQuality = DEFAULT_CAMERA_JPEG_QUALITY_MAX;
  }
  camFramesize =
      preferences.getString("camFramesize", DEFAULT_CAMERA_FRAMESIZE);
  camFramesize.toUpperCase();
  if (camFramesize != "QVGA" && camFramesize != "HVGA" &&
      camFramesize != "VGA") {
    camFramesize = DEFAULT_CAMERA_FRAMESIZE;
  }
  preferences.end();
}

void settingsReset() {
  preferences.begin(SETTING_PREFERENCES, false);
  preferences.clear();
  preferences.end();
  settingsReadConfig();
}

void settingsLoop() {
  server.handleClient();
  c2Loop();
}

String settingsGetName() { return name; }
String settingsGetType() { return type; }
String settingsGetApSsid() { return apSsid; }
String settingsGetApPassword() { return apPassword; }
int settingsGetApChannel() { return apChannel; }
String settingsGetStaSsid() { return staSsid; }
String settingsGetStaPassword() { return staPassword; }
bool settingsGetCameraHorizontalMirror() { return camHFlip; }
bool settingsGetCameraVerticalFlip() { return camVFlip; }
int settingsGetCameraBrightness() { return camBrightness; }
int settingsGetCameraContrast() { return camContrast; }
int settingsGetCameraSaturation() { return camSaturation; }
int settingsGetCameraSharpness() { return camSharpness; }
int settingsGetCameraJpegQuality() { return camJpegQuality; }

static framesize_t framesizeFromName(const String &name) {
  if (name == "QVGA") {
    return FRAMESIZE_QVGA;
  }
  if (name == "VGA") {
    return FRAMESIZE_VGA;
  }
  if (name == "HVGA") {
    return FRAMESIZE_HVGA;
  }
  return FRAMESIZE_INVALID;
}

framesize_t settingsGetCameraFramesize() {
  framesize_t size = framesizeFromName(camFramesize);
  if (size == FRAMESIZE_INVALID) {
    return FRAMESIZE_HVGA;
  }
  return size;
}

bool settingsSetName(String value) {
  if (name == value) {
    return true;
  }
  name = value;
  needReboot = true;
  return prefsPutString("name", name);
}

bool settingsSetType(String value) {
  if (type == value) {
    return true;
  }
  type = value;
  needReboot = true;
  return prefsPutString("type", type);
}

bool settingsSetApSsid(String value) {
  if (apSsid == value) {
    return true;
  }
  apSsid = value;
  needReboot = true;
  return prefsPutString("apSsid", apSsid);
}

bool settingsSetApPassword(String value) {
  if (apPassword == value) {
    return true;
  }
  apPassword = value;
  needReboot = true;
  return prefsPutString("apPassword", apPassword);
}

bool settingsSetApChannel(int value) {
  if (apChannel == value) {
    return true;
  }
  apChannel = value;
  needReboot = true;
  return prefsPutInt("apChannel", apChannel);
}

bool settingsSetStaSsid(String value) {
  if (staSsid == value) {
    return true;
  }
  staSsid = value;
  needReboot = true;
  return prefsPutString("staSsid", staSsid);
}

bool settingsSetStaPassword(String value) {
  if (staPassword == value) {
    return true;
  }
  staPassword = value;
  needReboot = true;
  return prefsPutString("staPassword", staPassword);
}

bool settingsSetCameraHorizontalMirror(bool value) {
  camHFlip = value;
  return prefsPutBool("camHFlip", camHFlip);
}

bool settingsSetCameraVerticalFlip(bool value) {
  camVFlip = value;
  return prefsPutBool("camVFlip", camVFlip);
}

bool settingsSetCameraBrightness(int value) {
  camBrightness = value;
  return prefsPutInt("camBrightness", camBrightness);
}

bool settingsSetCameraContrast(int value) {
  camContrast = value;
  return prefsPutInt("camContrast", camContrast);
}

bool settingsSetCameraSaturation(int value) {
  camSaturation = value;
  return prefsPutInt("camSaturation", camSaturation);
}

bool settingsSetCameraSharpness(int value) {
  camSharpness = value;
  return prefsPutInt("camSharpness", camSharpness);
}

bool settingsSetCameraJpegQuality(int value) {
  if (value < DEFAULT_CAMERA_JPEG_QUALITY_MIN) {
    value = DEFAULT_CAMERA_JPEG_QUALITY_MIN;
  } else if (value > DEFAULT_CAMERA_JPEG_QUALITY_MAX) {
    value = DEFAULT_CAMERA_JPEG_QUALITY_MAX;
  }
  camJpegQuality = value;
  return prefsPutInt("camJpegQuality", camJpegQuality);
}

bool settingsSetCameraFramesize(String value) {
  value.toUpperCase();
  value.trim();
  if (framesizeFromName(value) == FRAMESIZE_INVALID) {
    return false;
  }
  camFramesize = value;
  return prefsPutString("camFramesize", camFramesize);
}

bool settingsNameChanged() { return name.compareTo(DEFAULT_NAME) != 0; }

bool settingsApSsidChanged() { return apSsid.compareTo(DEFAULT_AP_SSID) != 0; }

bool settingsApPasswordChanged() {
  return apPassword.compareTo(DEFAULT_AP_PASSWORD) != 0;
}

bool settingsStaSsidChanged() {
  return staSsid.compareTo(DEFAULT_STA_SSID) != 0;
}

bool settingsStaPasswordChanged() {
  return staPassword.compareTo(DEFAULT_STA_PASSWORD) != 0;
}

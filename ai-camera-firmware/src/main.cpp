/*******************************************************************
  Camera firmware for serial communication with the lower computer,
  and wenbsocket communication with the Sunfounder Controller APP,
  targeted at ESP32-CAM and TTGO_CAMERA.

  Development test environment:
    - Arduino IDE 2.0.3
  Board tools:
    - Arduino AVR Boards 2.0.3
    - esp32 (by Espressif Systems) 2.0.7
  Libraries:
    - ArduinoJson (by Benoit Blanchon)
    - WebSockets (by Markus Sattler) (Links2004)

  Repositorie:
    -- https://github.com/sunfounder/ai-camera-firmware

  Author: Sunfounder
  Website: http://www.sunfounder.com
           https://docs.sunfounder.com
 *******************************************************************/
#define VERSION "1.5.6.a1"

#include "camera.h"
#include "camera_server.h"
#include "led_status.h"
#include "rom/rtc.h" // rst reason
#include "settings.h"
#include "serial_bridge.h"
#include "udp_server.h"
#include "wifi_helper.h"
#include "ws_server.h"

#include "defaults.h" // Default settings
#include "log.h"
#include <string.h>
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/ResetReason/ResetReason.ino

/* Select development board */
#define ESP32_CAM
// #define ESP32_S3_CAM
#include "pins.h" // after define development board

/* ----------------------- Configuration -------------------------------- */
int port = 8765;

/* Set check info for Sunfounder Controller APP */
extern String videoUrl;
extern String videoTemplate;
#define CHECK_TEXT "SC"

/* ----------------------- Global Variables -------------------------- */
WS_Server wsServer = WS_Server();
UDP_Server udpServer = UDP_Server();

static QueueHandle_t xQueueHttpFrame = NULL;
bool isCameraStarted = false;
bool inited =
    false; // For default config settings. All settings send befor inited will
           // be treated as default settings. default settings will be ignored
           // if settings in flash are not empty.

#define SERIAL_RX_CAP 256
static uint8_t serialRx[SERIAL_RX_CAP];

/* ----------------------- Functions -------------------------------- */
void factoryResetCheck();
void wsServerCameraHandler();
void serialReceivedHandler();
void cameraInit();
void handleSet(String cmd);
void start();

/*--------------------- setup() & loop() ------------------------------*/
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(SERIAL_TIMEOUT);
  delay(2);

  int reason = rtc_get_reset_reason(0); // cpu0
  if (reason != 12) {                   // 12, SW_CPU_RESET,  Software reset CPU
    Serial.println(VERSION);
  } else { // send [OK] when Software Reset
    Serial.print(F("[OK] "));
    Serial.println(VERSION);
  }

  videoTemplate = "http://ip:9000/mjpg";

  wifiBegin();
  settingsBegin(VERSION);

  ledBegin(CAMERA_PIN_LED);          // init status_led
  LED_STATUS_DISCONNECTED();         // turn on status_led
  pinMode(CAMERA_PIN_FLASH, OUTPUT); // init flash lamp
  digitalWrite(CAMERA_PIN_FLASH, 0); // 0:turn off flash lamp

  wifiConnectAp(settingsGetApSsid(), settingsGetApPassword(),
                settingsGetApChannel());
  wifiSetHostname(settingsGetName());

  // Check if factory reset needed
  factoryResetCheck();

  debug("Init done");
}

void loop() {
  ledStatusHandler();
  serialReceivedHandler();
  settingsLoop();
  if (inited) {
    wsServerCameraHandler();
    udpServer.loop();
  }
  delay(6);
}

/*--------------------- Functions------------------------------*/
/* websocket loop && camera init */
void wsServerCameraHandler() {
  wsServer.loop();
  if (!isCameraStarted) {
    cameraInit();
    log_i("Total heap: %d", ESP.getHeapSize());
    log_i("Free heap: %d", ESP.getFreeHeap());
    log_i("Total PSRAM: %d", ESP.getPsramSize());
    log_i("Free PSRAM: %d", ESP.getFreePsram());
  }
}

void serialReceivedHandler() {
  size_t len = serialBridgeRead(serialRx, SERIAL_RX_CAP - 1);
  if (len == 0) {
    return;
  }

  if (len >= 4 && memcmp(serialRx, "SET+", 4) == 0) {
    serialRx[len] = 0;
    handleSet(String((char *)serialRx + 4));
  } else if (len >= 3 && memcmp(serialRx, "WS+", 3) == 0) {
    serialRx[len] = 0;
    wsServer.send(String((char *)serialRx + 3));
  } else if (len >= 4 && memcmp(serialRx, "WSB+", 4) == 0) {
    serialBridgeFanoutBinary(serialRx + 4, len - 4);
  }
}

void cameraInit() {
  xQueueHttpFrame = xQueueCreate(1, sizeof(camera_fb_t *));
  pixformat_t pixel_format = PIXFORMAT_JPEG;
  register_camera(
      pixel_format, FRAMESIZE, FB_COUNT, xQueueHttpFrame,
      settingsGetCameraVerticalFlip(), settingsGetCameraHorizontalMirror(),
      CAMERA_PIN_Y2, CAMERA_PIN_Y3, CAMERA_PIN_Y4, CAMERA_PIN_Y5, CAMERA_PIN_Y6,
      CAMERA_PIN_Y7, CAMERA_PIN_Y8, CAMERA_PIN_Y9, CAMERA_PIN_XCLK,
      CAMERA_PIN_PCLK, CAMERA_PIN_VSYNC, CAMERA_PIN_HREF, CAMERA_PIN_SIOD,
      CAMERA_PIN_SIOC, CAMERA_PIN_PWDN, CAMERA_PIN_RESET);
  register_httpd(xQueueHttpFrame, NULL, true);
  isCameraStarted = true;
  log_i("Free PSRAM: %d", ESP.getFreePsram());
  info("camera stream start on: ", videoUrl);
}

void handleSet(String cmd) {
  String temp;
  // ------------ 3 characters command  ------------
  String _3_chars_cmd = cmd.substring(0, 3);
  // PSK
  if (_3_chars_cmd == "PSK") {
    temp = cmd.substring(3);
    settingsSetApPassword(temp);
    Serial.println("[OK] PSK is deprecating, use APPSK or STAPSK");
    debug("Set AP password: ", temp);
    return;
  }

  // ------------ 4 characters command  ------------
  String _4_chars_cmd = cmd.substring(0, 4);
  // NAME
  if (_4_chars_cmd == "NAME") {
    temp = cmd.substring(4);
    if (!settingsNameChanged() || inited == true) {
      settingsSetName(temp);
      Serial.println("[OK]");
      debug("Set NAME: ", temp);
    } else {
      Serial.println("[OK] NAME already set");
    }
    return;
  }
  // TYPE
  else if (_4_chars_cmd == "TYPE") {
    temp = cmd.substring(4);
    settingsSetType(temp);
    Serial.println("[OK]");
    debug("Set TYPE: ", temp);
    return;
  }
  // SSID
  else if (_4_chars_cmd == "SSID") {
    temp = cmd.substring(4);
    settingsSetApSsid(temp);
    Serial.println("[OK] SSID is deprecating, use APSSID or STASSID");
    debug("Set SSID: ", temp);
    return;
  }
  // PORT
  else if (_4_chars_cmd == "PORT") {
    port = cmd.substring(4).toInt();
    Serial.println("[OK]");
    debug("Set port: ", String(port));
    return;
  }
  // MODE
  else if (_4_chars_cmd == "MODE") {
    Serial.println("[OK] MODE is deprecated");
    return;
  }
  // LAMP
  else if (_4_chars_cmd == "LAMP") {
    uint8_t brightness_level = cmd.substring(4).toInt();
    // digitalWrite(CAMERA_PIN_FLASH, lamp_sw); // 0:turn off flash lamp, 1:turn
    // on flash lamp
    if (brightness_level > 10)
      brightness_level = 10;
    analogWrite(CAMERA_PIN_FLASH,
                brightness_level *
                    25); //  brightness_level: 0~10, to pwm 0 ~ 250
    Serial.println("[OK]");
    return;
  }

  // ------------ 5 characters command  ------------
  String _5_chars_cmd = cmd.substring(0, 5);
  // RESET
  if (_5_chars_cmd == "RESET") {
    wifiDisconnect();
    delay(10);
    ESP.restart();
    return;
  }
  // START
  else if (_5_chars_cmd == "START") {
    start();
    return;
  }
  // AP PSK
  else if (_5_chars_cmd == "APPSK") {
    temp = cmd.substring(5);
    settingsSetApPassword(temp);
    Serial.println("[OK]");
    debug("Set AP password: ", temp);
    return;
  }
  // AP Channel
  else if (_5_chars_cmd == "APCHN") {
    temp = cmd.substring(5);
    int channel = temp.toInt();
    if (channel < 1 || channel > 13) {
      Serial.println("[ERROR] AP channel out of range");
      return;
    }
    settingsSetApChannel(channel);
    Serial.println("[OK]");
    debug("Set AP channel: ", String(channel));
    return;
  }

  // ------------ 6 characters command  ------------
  String _6_chars_cmd = cmd.substring(0, 6);
  // AP SSID
  if (_6_chars_cmd == "APSSID") {
    temp = cmd.substring(6);
    settingsSetApSsid(temp);
    Serial.println("[OK]");
    debug("Set AP ssid: ", temp);
    return;
  }
  // STA PSK
  else if (_6_chars_cmd == "STAPSK") {
    temp = cmd.substring(6);
    if (!settingsStaPasswordChanged() || inited == true) {
      settingsSetStaPassword(temp);
      Serial.println("[OK]");
      debug("Set STA password: ", temp);
    } else {
      Serial.println("[OK] STA already set");
    }
    return;
  }
  // Restart STA
  else if (_6_chars_cmd == "RSTSTA") {
    String staSsid = settingsGetStaSsid();
    String staPassword = settingsGetStaPassword();
    if (staSsid.length() > 0 && staPassword.length() > 0) {
      bool connected = wifiConnectSta(staSsid, staPassword, 5);
      if (connected) {
        Serial.printf("[OK] %s\n", wifiGetStaIp().c_str());
        debug("STA connect success");
      } else {
        Serial.println("[ERROR] STA connect failed");
        debug("STA connect failed");
      }
    } else {
      Serial.println("[ERROR] STA not set");
      debug("STA SSID or password is empty");
    }
    return;
  }
  // Reset Config
  else if (_6_chars_cmd == "RSTCFG") {
    settingsReset();
    Serial.println("[OK]");
    debug("Reset config");
    return;
  }
  // ------------ 7 characters command  ------------
  String _7_chars_cmd = cmd.substring(0, 7);
  // STASSID
  if (_7_chars_cmd == "STASSID") {
    if (!settingsStaSsidChanged() || inited == true) {
      settingsSetStaSsid(temp);
      Serial.println("[OK]");
      debug("Set STA SSID: ", temp);
    } else {
      Serial.println("[OK] STA SSID already set");
    }
    return;
  }

  // ----------- if no retrun before -----------
  Serial.printf("[ERROR] SET+ Unknown command: %s", cmd.c_str());
}

void start() {
  LED_STATUS_ERROR();
  wifiConnectAp(settingsGetApSsid(), settingsGetApPassword(),
                settingsGetApChannel());
  String staSsid = settingsGetStaSsid();
  String staPassword = settingsGetStaPassword();
  if (staSsid.length() > 0 || staPassword.length() > 8) {
    wifiConnectSta(staSsid, staPassword);
  } else {
    debug(F("STA SSID or STA password empty!"));
  }

  String apIp = wifiGetApIp();
  Serial.print(F("[OK] "));
  Serial.println(apIp);
  Serial.flush();

  videoUrl = String("http://") + apIp + ":9000/mjpg";
  if (wifiIsStaConnected()) {
    videoUrl = String("http://") + wifiGetStaIp() + ":9000/mjpg";
  }

  wsServer.close();
  wsServer.begin(port, settingsGetName(), settingsGetType(), CHECK_TEXT);
  // Bind :9000 before UDP beacons advertise the stream URL. Otherwise iOS can
  // connect during camera init and get TCP RST / NSURLErrorCannotConnectToServer.
  cameraInit();
  udpServer.begin(settingsGetName(), settingsGetType());
  debug(F("Websocket on!"));
  debug(F("UDP control on!"));
  inited = true;
}

void factoryResetCheck() {
  uint8_t sense = 13;
  uint8_t pull = 15;
  pinMode(sense, INPUT_PULLUP);
  pinMode(pull, OUTPUT);
  digitalWrite(pull, 0);
  if (digitalRead(sense) == 0) {
    for (uint8_t i = 0; i < 2; i++) {
      analogWrite(CAMERA_PIN_FLASH, 1);
      delay(100);
      analogWrite(CAMERA_PIN_FLASH, 0);
      delay(100);
    }
    settingsReset();
    while (1)
      ;
    ;
  }
  pinMode(pull, INPUT);
}
#pragma once

#include "esp_camera.h"
#include <Arduino.h>

void settingsBegin(String version);
void settingsLoop();
void settingsReadConfig();
void settingsReset();

String settingsGetName();
String settingsGetType();
String settingsGetApSsid();
String settingsGetApPassword();
int settingsGetApChannel();
String settingsGetStaSsid();
String settingsGetStaPassword();
bool settingsGetCameraHorizontalMirror();
bool settingsGetCameraVerticalFlip();
int settingsGetCameraBrightness();
int settingsGetCameraContrast();
int settingsGetCameraSaturation();
int settingsGetCameraSharpness();
int settingsGetCameraJpegQuality();
framesize_t settingsGetCameraFramesize();

bool settingsSetName(String name);
bool settingsSetType(String type);
bool settingsSetApSsid(String ssid);
bool settingsSetApPassword(String password);
bool settingsSetApChannel(int channel);
bool settingsSetStaSsid(String ssid);
bool settingsSetStaPassword(String password);
bool settingsSetCameraHorizontalMirror(bool mirror);
bool settingsSetCameraVerticalFlip(bool flip);
bool settingsSetCameraBrightness(int brightness);
bool settingsSetCameraContrast(int contrast);
bool settingsSetCameraSaturation(int saturation);
bool settingsSetCameraSharpness(int sharpness);
bool settingsSetCameraJpegQuality(int quality);
bool settingsSetCameraFramesize(String framesize);

bool settingsNameChanged();
bool settingsApSsidChanged();
bool settingsApPasswordChanged();
bool settingsStaSsidChanged();
bool settingsStaPasswordChanged();

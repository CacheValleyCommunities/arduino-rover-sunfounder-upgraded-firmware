#pragma once

#include <Arduino.h>

void c2ProgramBegin();
bool c2ProgramStoreAndStart(const String &json);
void c2ProgramStop();
void c2ProgramRequestStop();
void c2ProgramClear();
bool c2ProgramIsRunning();
bool c2ProgramIsStored();
void c2ProgramCopyJson(String &out);
void c2ProgramResetForTest();

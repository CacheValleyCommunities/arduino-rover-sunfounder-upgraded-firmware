#pragma once

#include "FreeRTOS.h"

inline BaseType_t xTaskCreate(TaskFunction_t fn, const char * /*name*/,
                              uint32_t /*stack*/, void *arg,
                              UBaseType_t /*prio*/, TaskHandle_t *handle) {
  if (handle) {
    *handle = (TaskHandle_t)1;
  }
  if (fn) {
    fn(arg);
  }
  return pdPASS;
}

inline void vTaskDelete(TaskHandle_t) {}
inline void vTaskDelay(int) {}

#pragma once

#include <cstdint>

typedef void *TaskHandle_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef void (*TaskFunction_t)(void *);

#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)

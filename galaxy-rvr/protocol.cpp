#include "protocol.h"
#include "app_state.h"
#include "rover.h"

#include <SunFounder_AI_Camera.h>

extern AiCamera aiCam;

int16_t followStopCm = FOLLOW_STOP_CM;
int16_t followSlowCm = FOLLOW_SLOW_CM;
int16_t followDistanceCm = FOLLOW_DISTANCE;
int16_t followSlowPower = FOLLOW_SLOW_POWER;
int16_t avoidThresholdCm = ULTRASONIC_AVOIDANCE_THRESHOLD;
int16_t reverseCm = ULTRASONIC_REVERSE_CM;
int16_t movePower = DEFAULT_MOVE_POWER;
int16_t msPerFoot = DEFAULT_MS_PER_FOOT;
int16_t msPerDeg = DEFAULT_MS_PER_DEG;

struct ConfigSlot {
  uint8_t id;
  int16_t *value;
  int16_t min;
  int16_t max;
  int16_t reset;
};

static ConfigSlot slots[] = {
    {CFG_FOLLOW_STOP_CM, &followStopCm, 1, 100, FOLLOW_STOP_CM},
    {CFG_FOLLOW_SLOW_CM, &followSlowCm, 1, 200, FOLLOW_SLOW_CM},
    {CFG_FOLLOW_DISTANCE_CM, &followDistanceCm, 1, 300, FOLLOW_DISTANCE},
    {CFG_FOLLOW_SLOW_POWER, &followSlowPower, 0, 100, FOLLOW_SLOW_POWER},
    {CFG_AVOID_THRESHOLD_CM, &avoidThresholdCm, 1, 300,
     ULTRASONIC_AVOIDANCE_THRESHOLD},
    {CFG_REVERSE_CM, &reverseCm, 1, 100, ULTRASONIC_REVERSE_CM},
    {CFG_MOVE_POWER, &movePower, 0, 100, DEFAULT_MOVE_POWER},
    {CFG_MS_PER_FOOT, &msPerFoot, 100, 10000, DEFAULT_MS_PER_FOOT},
    {CFG_MS_PER_DEG, &msPerDeg, 1, 100, DEFAULT_MS_PER_DEG},
};

static ConfigSlot *slotForId(uint8_t id) {
  for (uint8_t i = 0; i < CFG_ID_COUNT; i++) {
    if (slots[i].id == id) {
      return &slots[i];
    }
  }
  return nullptr;
}

void roverConfigReset() {
  for (uint8_t i = 0; i < CFG_ID_COUNT; i++) {
    *slots[i].value = slots[i].reset;
  }
}

bool roverConfigSet(uint8_t id, int16_t value) {
  ConfigSlot *slot = slotForId(id);
  if (slot == nullptr) {
    return false;
  }
  *slot->value = constrain(value, slot->min, slot->max);
  return true;
}

int16_t roverConfigGet(uint8_t id) {
  ConfigSlot *slot = slotForId(id);
  if (slot == nullptr) {
    return 0;
  }
  return *slot->value;
}

void roverConfigWriteTelemetry(uint8_t *dest) {
  for (uint8_t i = 0; i < CFG_ID_COUNT; i++) {
    int16_t value = *slots[i].value;
    dest[i * 2] = (uint8_t)((value >> 8) & 0xFF);
    dest[i * 2 + 1] = (uint8_t)(value & 0xFF);
  }
}

static const float BATTERY_TELEMETRY_OFFSET_V = 6.0f;

enum SensorPacketOffset : uint8_t {
  PKT_START = 0,
  PKT_LENGTH = 1,
  PKT_CHECKSUM = 2,
  PKT_US_ID = 3,
  PKT_US_MSB = 4,
  PKT_US_LSB = 5,
  PKT_IR_ID = 6,
  PKT_IR_VALUE = 7,
  PKT_BAT_ID = 8,
  PKT_BAT_VALUE = 9,
  PKT_CFG_ID = 10,
  PKT_CFG_VALUES = 11,
  PKT_END = 11 + CFG_TELEMETRY_BYTES,
  PKT_SIZE = 12 + CFG_TELEMETRY_BYTES,
};

struct FrameCursor {
  const uint8_t *buf;
  int length;
  int i;

  bool has(int extra) const { return extra >= 0 && (i + extra) < length; }

  uint8_t u8() { return buf[++i]; }

  int8_t i8() { return (int8_t)u8(); }

  int16_t i16BE() {
    uint8_t hi = u8();
    uint8_t lo = u8();
    return (int16_t)(((uint16_t)hi << 8) | lo);
  }
};

static void writeU16BE(uint8_t *dest, uint16_t value) {
  dest[0] = (uint8_t)(value >> 8);
  dest[1] = (uint8_t)(value & 0xFF);
}

static uint8_t xorChecksum(const uint8_t *data, uint8_t len) {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < len; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

#if DEBUG_WS
static void dumpReceiveBuffer() {
  Serial.print(F("onReceive:"));
  for (int i = 0; i < aiCam.recvBufferLength; i++) {
    Serial.print(aiCam.recvBuffer[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}
#endif

void onReceive() {
#if DEBUG_WS
  dumpReceiveBuffer();
#endif

  FrameCursor cur = {aiCam.recvBuffer, aiCam.recvBufferLength, 0};

  for (; cur.i < cur.length; cur.i++) {
    switch (cur.buf[cur.i]) {
    case ENTITY_MOTORS:
      if (!cur.has(2)) {
        return;
      }
      leftMotorPower = cur.i8();
      rightMotorPower = cur.i8();
      break;
    case ENTITY_RGB:
      if (!cur.has(3)) {
        return;
      }
      rover.lights.set(cur.u8(), cur.u8(), cur.u8());
      break;
    case ENTITY_SERVO:
      if (!cur.has(1)) {
        return;
      }
      rover.look(cur.u8());
      break;
    case ENTITY_LAMP:
      if (!cur.has(1)) {
        return;
      }
      rover.cameraLamp(cur.u8() == 1);
      break;
    case ENTITY_OBSTACLE:
      if (!cur.has(3)) {
        return;
      }
      {
        uint8_t state = cur.u8();
        uint8_t mode = cur.u8();
        obstaclePower = cur.u8();
        if (state) {
          currentMode = (mode == 0) ? MODE_OBSTACLE_AVOIDANCE
                                    : MODE_OBSTACLE_FOLLOWING;
        } else {
          currentMode = MODE_APP_CONTROL;
          leftMotorPower = 0;
          rightMotorPower = 0;
        }
      }
      break;
    case ENTITY_CONFIG:
      if (!cur.has(1)) {
        return;
      }
      {
        uint8_t count = cur.buf[cur.i + 1];
        if (!cur.has(1 + (int)count * 3)) {
          return;
        }
        cur.i++;
        for (uint8_t n = 0; n < count; n++) {
          uint8_t id = cur.u8();
          int16_t value = cur.i16BE();
          roverConfigSet(id, value);
        }
      }
      break;
    default:
      break;
    }
  }
}

static void buildTelemetry(uint8_t *toSend) {
  toSend[PKT_START] = WS_DATA_START_BIT;
  toSend[PKT_LENGTH] = 0x00;
  toSend[PKT_CHECKSUM] = 0x00;

  uint16_t ultrasonicMM = rover.sensors.distanceCm() * 10.0;
  toSend[PKT_US_ID] = TELEMETRY_ULTRASONIC;
  writeU16BE(&toSend[PKT_US_MSB], ultrasonicMM);

  toSend[PKT_IR_ID] = TELEMETRY_IR;
  toSend[PKT_IR_VALUE] = rover.sensors.irBits();

  toSend[PKT_BAT_ID] = TELEMETRY_BATTERY;
  toSend[PKT_BAT_VALUE] =
      round((rover.sensors.batteryVolts() - BATTERY_TELEMETRY_OFFSET_V) * 100);

  toSend[PKT_CFG_ID] = TELEMETRY_CONFIG;
  roverConfigWriteTelemetry(&toSend[PKT_CFG_VALUES]);

  toSend[PKT_END] = WS_DATA_END_BIT;
  toSend[PKT_LENGTH] = PKT_SIZE - 4;
  toSend[PKT_CHECKSUM] = xorChecksum(toSend, PKT_SIZE - 1);
}

void handleSensorData() {
  uint8_t toSend[PKT_SIZE];
  buildTelemetry(toSend);
  aiCam.sendBinaryData(toSend, PKT_SIZE);
}

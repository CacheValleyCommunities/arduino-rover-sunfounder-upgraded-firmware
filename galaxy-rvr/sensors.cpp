#include "sensors.h"
#include "protocol.h"

static float cachedDistance = -1;

static float ultrasonicPing() {
  delay(4);

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  float duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, ULTRASONIC_READ_TIMEOUT);
  float distance = duration * 0.017;

  if (distance > MAX_DISTANCE || distance == 0) {
    return -1;
  }
  return distance;
}

void ultrasonicUpdate() { cachedDistance = ultrasonicPing(); }

float ultrasonicRead() { return cachedDistance; }

bool ultrasonicIsObstacle() { return cachedDistance < avoidThresholdCm; }

bool ultrasonicIsClear() { return cachedDistance >= avoidThresholdCm; }

void irObstacleBegin() {
  pinMode(IR_LEFT_PIN, INPUT);
  pinMode(IR_RIGHT_PIN, INPUT);
}

bool irObstacleIsLeftDetected() { return digitalRead(IR_LEFT_PIN) == LOW; }

bool irObstacleIsRightDetected() { return digitalRead(IR_RIGHT_PIN) == LOW; }

uint8_t irObstacleRead() {
  uint8_t left = irObstacleIsLeftDetected();
  uint8_t right = irObstacleIsRightDetected();
  return (left << 1) | right;
}

void batteryBegin() { pinMode(BATTERY_PIN, INPUT); }

float batteryGetVoltage() {
  int adcValue = analogRead(BATTERY_PIN);
  float adcVoltage = float(adcValue) / 1024.0 * ANALOG_REFERENCE_VOLTAGE;
  float batteryVoltage = adcVoltage * 2;
  batteryVoltage = int(batteryVoltage * 100) / 100.0;
  return batteryVoltage;
}

uint8_t batteryGetPercentage() {
  float voltage = batteryGetVoltage();
  return map(voltage, 6.6, 8.4, 0, 100);
}

#ifdef UNIT_TEST
void ultrasonicSetDistance(float cm) { cachedDistance = cm; }

void ultrasonicResetForTest() { cachedDistance = -1; }
#endif

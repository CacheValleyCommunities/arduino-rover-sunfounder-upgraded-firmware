#include "car_control.h"

#include <Arduino.h>
#if defined(ARDUINO_AVR_UNO)
#include <SoftPWM.h>
#endif

#define MOTOR_POWER_MIN 28  // 28/255

static int8_t lastPower0 = 0;
static int8_t lastPower1 = 0;

/** 
* @brief Initialize the motor, and (block) the initialization compass
*/
void carBegin() {
  #if defined(ARDUINO_AVR_UNO)
  for (uint8_t i = 0; i < 4; i++) {
    SoftPWMSet(MOTOR_PINS[i], 0);
    SoftPWMSetFadeTime(MOTOR_PINS[i], 40, 40);
  }
  #endif
}

/** 
 * @name simple move functions
 */
void carForward(int8_t power)       { carSetMotors( power,  power); }
void carBackward(int8_t power)      { carSetMotors(-power, -power); }
void carTurnLeft(int8_t power)      { carSetMotors(-power,  power); }
void carTurnRight(int8_t power)     { carSetMotors( power, -power); }
void carBackLeft(int8_t power)      { carSetMotors(     0, -power); }
void carBackRight(int8_t power)     { carSetMotors(-power,      0); }
void carStop()                      { carSetMotors(     0,      0); }

/** 
 * @brief Set speed for 2 motors
 *
 * @param power0  0 ~ 100
 * @param power1  0 ~ 100
 */
static int8_t powerToPwm(int8_t power) {
  if (power == 0) {
    return 0;
  }
  return (int8_t)map(abs(power), 0, 100, MOTOR_POWER_MIN, 255);
}

static void writeMotorSide(uint8_t index, int8_t power) {
  bool dir = power > 0;
  if (MOTOR_DIRECTIONS[index]) {
    dir = !dir;
  }
  int8_t pwm = powerToPwm(power);
#if defined(ARDUINO_AVR_UNO)
  SoftPWMSet(MOTOR_PINS[index * 2], dir * pwm);
  SoftPWMSet(MOTOR_PINS[index * 2 + 1], !dir * pwm);
#elif defined(ARDUINO_MINIMA)
  analogWrite(MOTOR_PINS[index * 2], dir * pwm);
  analogWrite(MOTOR_PINS[index * 2 + 1], !dir * pwm);
#endif
}

void carSetMotors(int8_t power0, int8_t power1) {
  if (power0 == lastPower0 && power1 == lastPower1) {
    return;
  }
  lastPower0 = power0;
  lastPower1 = power1;
  writeMotorSide(0, power0);
  writeMotorSide(1, power1);
}

#ifdef UNIT_TEST
void carGetLastPowers(int8_t *left, int8_t *right) {
  if (left) {
    *left = lastPower0;
  }
  if (right) {
    *right = lastPower1;
  }
}

void carResetForTest() {
  lastPower0 = 127;
  lastPower1 = 127;
}
#endif

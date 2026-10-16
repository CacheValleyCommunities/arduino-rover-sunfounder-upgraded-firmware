#include "firmware_test.h"

TEST_F(CamTest, LedOnWritesActiveLow) {
  ledOn();
  ledStatusHandler();
  EXPECT_EQ(camPin(LED_PIN), LOW);
}

TEST_F(CamTest, SlowBlinkTogglesAfterDelay) {
  ledSlowBlink();
  camMillisAdd(SLOW_BLINK_DELAY + 1);
  ledStatusHandler();
  uint8_t first = camPin(LED_PIN);
  camMillisAdd(SLOW_BLINK_DELAY + 1);
  ledStatusHandler();
  EXPECT_NE(camPin(LED_PIN), first);
}

TEST_F(CamTest, FastBlinkTogglesAfterShortDelay) {
  ledFastBlink();
  camMillisAdd(FAST_BLINK_DELAY + 1);
  ledStatusHandler();
  uint8_t first = camPin(LED_PIN);
  camMillisAdd(FAST_BLINK_DELAY + 1);
  ledStatusHandler();
  EXPECT_NE(camPin(LED_PIN), first);
}

TEST_F(CamTest, BeginLeavesLedOffHigh) {
  EXPECT_EQ(camPin(LED_PIN), HIGH);
}

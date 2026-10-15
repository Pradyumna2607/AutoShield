// Native unit tests for trigger_logic.h.  Build & run from the repo root:
//   g++ -std=c++11 -Wall -Wextra -o /tmp/t firmware/test/test_trigger_logic.cpp && /tmp/t
#include <cstdio>
#include "../autoshield/trigger_logic.h"
using namespace autoshield;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)

static Reading calm() { return {1010.0f, 28.0f, 55.0f, 0.9f, false}; }

// Falling pressure/temperature, humid and dark: a pre-rain signature.
static Reading storm(int minute) {
  return {1010.0f - 0.12f * minute, 28.0f - 0.15f * minute, 90.0f, 0.2f, false};
}

static void test_calm_never_triggers() {
  TriggerLogic t;
  for (int i = 0; i < 500; ++i) CHECK(t.step(calm()) == Event::None);
  CHECK(t.state() == State::Idle);
}

static void test_storm_sequence() {
  TriggerLogic t; int alert = -1, notify = -1, deploy = -1;
  for (int m = 0; m < 80; ++m) {
    Event e = t.step(storm(m));
    if (e == Event::AlertRaised && alert < 0) alert = m;
    if (e == Event::PreDeployNotify && notify < 0) notify = m;
    if (e == Event::Deploy && deploy < 0) deploy = m;
  }
  CHECK(alert >= 30);            // needs a full 30-minute pressure window
  CHECK(notify > alert);
  CHECK(deploy == notify + 2);   // 2-minute farmer notification window
  CHECK(t.state() == State::Deployed);
}

static void test_no_alert_without_humidity() {
  TriggerLogic t;
  for (int m = 0; m < 80; ++m) {
    Reading r = storm(m); r.humidity_pct = 60;
    CHECK(t.step(r) == Event::None);
  }
}

static void test_rain_sensor_triggers_during_warmup() {
  TriggerLogic t; Reading r = calm(); r.rain_detected = true;
  CHECK(t.step(r) == Event::PreDeployNotify);
}

static void test_cancel_and_lockout() {
  TriggerLogic t; Reading r = calm(); r.rain_detected = true;
  CHECK(t.step(r) == Event::PreDeployNotify);
  CHECK(t.cancel());
  for (int i = 0; i < 30; ++i) CHECK(t.step(r) == Event::None);  // lockout
  CHECK(t.step(r) == Event::PreDeployNotify);                    // lockout over
}

static void test_hold_until_retracted() {
  Config c; c.predeploy_cycles = 0; TriggerLogic t(c);
  Reading r = calm(); r.rain_detected = true;
  CHECK(t.step(r) == Event::Deploy);
  for (int i = 0; i < 100; ++i) CHECK(t.step(calm()) == Event::None);
  CHECK(t.state() == State::Deployed);
  t.retracted();
  CHECK(t.state() == State::Idle);
}

static void test_ldr_can_be_disabled() {
  Config c; c.use_ldr = false; TriggerLogic t(c); bool alert = false;
  for (int m = 0; m < 50; ++m) {
    Reading r = storm(m); r.light_norm = 1.0f;
    alert |= (t.step(r) == Event::AlertRaised);
  }
  CHECK(alert);
}

int main() {
  test_calm_never_triggers();
  test_storm_sequence();
  test_no_alert_without_humidity();
  test_rain_sensor_triggers_during_warmup();
  test_cancel_and_lockout();
  test_hold_until_retracted();
  test_ldr_can_be_disabled();
  if (failures == 0) std::printf("All trigger-logic tests passed.\n");
  return failures ? 1 : 0;
}

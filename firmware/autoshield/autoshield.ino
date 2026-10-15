/*
 * AutoShield -- reference firmware (ESP32, Arduino framework)
 *
 * Implements the offline-first control loop described in the AutoShield paper:
 * 60 s sensor evaluation -> three-stage trigger logic -> limit-switch-guarded
 * motor control -> SPIFFS data logging.
 *
 * NOT included: Bluetooth/WiFi mobile app, WhatsApp (CallMeBot) alerts,
 * OpenWeatherMap integration. notifyFarmer() is the hook for those.
 * Manual control is available over Serial (115200 baud): see printHelp().
 *
 * Libraries: "DHT sensor library" + "Adafruit Unified Sensor" + "Adafruit BMP280 Library".
 * This is a reference implementation: test on the bench with the motor
 * disconnected before connecting a real canopy. Read SAFETY.md first.
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include "config.h"
#include "trigger_logic.h"

using namespace autoshield;

DHT dht(PIN_DHT, DHT22);
Adafruit_BMP280 bmp;
TriggerLogic logic;

enum class Canopy : uint8_t { Unknown, Retracted, Deployed, Moving };
enum class Motor : uint8_t { Stopped, Deploying, Retracting };

Canopy canopy = Canopy::Unknown;
Motor motor = Motor::Stopped;
uint32_t motorStartedMs = 0;
uint32_t lastEvalMs = 0;
bool bmpOk = false;

// ---------------------------------------------------------------- helpers
const char* eventName(Event e) {
  switch (e) {
    case Event::AlertRaised:     return "ALERT_RAISED";
    case Event::AlertCleared:    return "ALERT_CLEARED";
    case Event::PreDeployNotify: return "PRE_DEPLOY_NOTIFY";
    case Event::Deploy:          return "DEPLOY";
    default:                     return "NONE";
  }
}

const char* stateName(State s) {
  switch (s) {
    case State::Alert:     return "ALERT";
    case State::PreDeploy: return "PRE_DEPLOY";
    case State::Deployed:  return "DEPLOYED";
    default:               return "IDLE";
  }
}

// Hook for BLE / WiFi / WhatsApp (CallMeBot) alerts. Currently prints to Serial.
void notifyFarmer(const char* msg) {
  Serial.printf("[NOTIFY] %s\n", msg);
}

bool limitDeployed()  { return digitalRead(PIN_LIMIT_DEPLOYED)  == LOW; }
bool limitRetracted() { return digitalRead(PIN_LIMIT_RETRACTED) == LOW; }

void logLine(const Reading& r, Event e) {
  File f = SPIFFS.open(LOG_PATH, FILE_APPEND);
  if (!f) return;
  if (f.size() > LOG_MAX_BYTES) {
    f.close();
    SPIFFS.remove(LOG_OLD_PATH);
    SPIFFS.rename(LOG_PATH, LOG_OLD_PATH);
    f = SPIFFS.open(LOG_PATH, FILE_APPEND);
    if (!f) return;
  }
  f.printf("%lu,%.2f,%.2f,%.1f,%.2f,%d,%s,%s\n", (unsigned long)(millis() / 1000UL),
           r.pressure_hpa, r.temp_c, r.humidity_pct, r.light_norm, r.rain_detected ? 1 : 0,
           stateName(logic.state()), eventName(e));
  f.close();
}

// ---------------------------------------------------------------- motor
void motorStop() {
  digitalWrite(PIN_MOTOR_DEPLOY, LOW);
  digitalWrite(PIN_MOTOR_RETRACT, LOW);
  if (motor != Motor::Stopped) {
    motor = Motor::Stopped;
    if (limitDeployed())       canopy = Canopy::Deployed;
    else if (limitRetracted()) canopy = Canopy::Retracted;
    else                       canopy = Canopy::Unknown;
  }
}

void motorStart(Motor dir) {
  if (dir == Motor::Deploying && limitDeployed())   { canopy = Canopy::Deployed;  return; }
  if (dir == Motor::Retracting && limitRetracted()) { canopy = Canopy::Retracted; return; }
  motorStop();
  delay(50);  // dead time before reversing direction
  digitalWrite(dir == Motor::Deploying ? PIN_MOTOR_DEPLOY : PIN_MOTOR_RETRACT, HIGH);
  motor = dir;
  canopy = Canopy::Moving;
  motorStartedMs = millis();
  Serial.println(dir == Motor::Deploying ? "[MOTOR] deploying" : "[MOTOR] retracting");
}

void motorService() {
  if (motor == Motor::Stopped) return;
  if (motor == Motor::Deploying && limitDeployed()) {
    motorStop(); Serial.println("[MOTOR] fully deployed");
  } else if (motor == Motor::Retracting && limitRetracted()) {
    motorStop(); Serial.println("[MOTOR] fully retracted");
    logic.retracted();
  } else if (millis() - motorStartedMs > MOTOR_MAX_RUN_MS) {
    motorStop();
    Serial.println("[FAULT] motor timeout: limit switch not reached");
    notifyFarmer("FAULT: canopy motor timed out. Check mechanism.");
  }
}

// ---------------------------------------------------------------- sensing
bool readSensors(Reading& r) {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float p = bmpOk ? bmp.readPressure() / 100.0f : NAN;
  if (isnan(t) || isnan(h) || isnan(p)) return false;
  float light = analogRead(PIN_LDR_AO) / 4095.0f;
  if (LDR_INVERT) light = 1.0f - light;
  bool wet = digitalRead(PIN_RAIN_DO) == (RAIN_ACTIVE_LOW ? LOW : HIGH);
  r = {p, t, h, light, wet};
  return true;
}

void evaluate() {
  Reading r;
  if (!readSensors(r)) {
    Serial.println("[WARN] sensor read failed; skipping cycle");
    return;
  }
  Event e = logic.step(r);
  Serial.printf("[%lus] P=%.1f hPa T=%.1f C RH=%.0f%% light=%.2f rain=%d state=%s event=%s\n",
                (unsigned long)(millis() / 1000UL), r.pressure_hpa, r.temp_c, r.humidity_pct,
                r.light_norm, r.rain_detected, stateName(logic.state()), eventName(e));
  logLine(r, e);
  switch (e) {
    case Event::AlertRaised:     notifyFarmer("Alert: rain conditions developing."); break;
    case Event::PreDeployNotify: notifyFarmer("Canopy will deploy in 2 minutes. Press button / send 'x' to cancel."); break;
    case Event::Deploy:          notifyFarmer("Deploying canopy."); motorStart(Motor::Deploying); break;
    default: break;
  }
}

// ---------------------------------------------------------------- user input
void printHelp() {
  Serial.println("Commands: d=deploy  r=retract  x=cancel pre-deploy  s=status  h=help");
}

void manualToggle() {
  if (logic.cancel()) { Serial.println("[USER] pre-deployment cancelled"); return; }
  if (canopy == Canopy::Deployed || motor == Motor::Deploying) motorStart(Motor::Retracting);
  else motorStart(Motor::Deploying);
}

void handleSerial() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'd': motorStart(Motor::Deploying);  break;
      case 'r': motorStart(Motor::Retracting); break;
      case 'x': Serial.println(logic.cancel() ? "[USER] cancelled" : "[USER] nothing to cancel"); break;
      case 's': Serial.printf("state=%s canopy=%d motor=%d\n", stateName(logic.state()), (int)canopy, (int)motor); break;
      case 'h': printHelp(); break;
      default: break;
    }
  }
}

void handleButton() {
  static bool last = HIGH;
  static uint32_t changedMs = 0;
  bool now = digitalRead(PIN_BUTTON);
  if (now != last && millis() - changedMs > DEBOUNCE_MS) {
    changedMs = millis();
    last = now;
    if (now == LOW) manualToggle();
  }
}

// ---------------------------------------------------------------- Arduino entry points
void setup() {
  Serial.begin(115200);
  pinMode(PIN_MOTOR_DEPLOY, OUTPUT);
  pinMode(PIN_MOTOR_RETRACT, OUTPUT);
  digitalWrite(PIN_MOTOR_DEPLOY, LOW);
  digitalWrite(PIN_MOTOR_RETRACT, LOW);
  pinMode(PIN_LIMIT_DEPLOYED, INPUT_PULLUP);
  pinMode(PIN_LIMIT_RETRACTED, INPUT_PULLUP);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_RAIN_DO, INPUT);
  pinMode(PIN_LED, OUTPUT);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  bmpOk = bmp.begin(BMP280_I2C_ADDR);
  if (!bmpOk) Serial.println("[ERROR] BMP280 not found - check wiring/address");
  dht.begin();
  if (!SPIFFS.begin(true)) Serial.println("[ERROR] SPIFFS mount failed");

  if (limitDeployed())       canopy = Canopy::Deployed;
  else if (limitRetracted()) canopy = Canopy::Retracted;

  Serial.println("AutoShield reference firmware v1.0.0");
  Serial.println("Note: trigger logic needs ~30 minutes of readings before Stage 1 can fire.");
  printHelp();
  lastEvalMs = millis() - EVAL_INTERVAL_MS;  // evaluate immediately on boot
}

void loop() {
  handleSerial();
  handleButton();
  motorService();
  if (millis() - lastEvalMs >= EVAL_INTERVAL_MS) {
    lastEvalMs += EVAL_INTERVAL_MS;
    evaluate();
  }
  digitalWrite(PIN_LED, logic.state() == State::Idle ? LOW : HIGH);
  delay(10);
}

// AutoShield hardware configuration -- ESP32 DevKit V1.
// Edit this file to match your wiring. See docs/hardware-guide.md.
#pragma once

// ---- Pin map (ADC1 pins only for analog inputs: ADC2 is unusable while WiFi is on) ----
#define PIN_DHT            4    // DHT22 data
#define PIN_I2C_SDA        21   // BMP280
#define PIN_I2C_SCL        22   // BMP280
#define PIN_RAIN_DO        27   // FC-37 digital output
#define PIN_LDR_AO         35   // LDR module analog output (ADC1_CH7)
#define PIN_MOTOR_DEPLOY   25   // driver input: spools canopy out (L298N IN1 / BTS7960 RPWM)
#define PIN_MOTOR_RETRACT  26   // driver input: spools canopy in  (L298N IN2 / BTS7960 LPWM)
#define PIN_LIMIT_DEPLOYED  32  // limit switch, closes at fully deployed (to GND)
#define PIN_LIMIT_RETRACTED 33  // limit switch, closes at fully retracted (to GND)
#define PIN_BUTTON         13   // manual override push button (to GND)
#define PIN_LED            2    // on-board status LED

// ---- Sensor polarity ----
#define BMP280_I2C_ADDR    0x76 // 0x77 on some modules
#define RAIN_ACTIVE_LOW    true // FC-37 module pulls DO low when wet
#define LDR_INVERT         false // set true if your module reads HIGH when dark

// ---- Timing ----
#define EVAL_INTERVAL_MS   60000UL  // sensor evaluation cycle (paper: 60 s)
#define MOTOR_MAX_RUN_MS   180000UL // safety cut-off if a limit switch is never reached
#define DEBOUNCE_MS        40

// ---- Data logging (SPIFFS) ----
#define LOG_PATH           "/log.csv"
#define LOG_OLD_PATH       "/log.old.csv"
#define LOG_MAX_BYTES      200000

# Hardware Guide

> **Read [SAFETY.md](../SAFETY.md) before building.** The system involves a lead-acid battery, a high-current motor and a large structure exposed to wind.

## ESP32 pin map (matches `firmware/autoshield/config.h`)

| Function | GPIO | Notes |
|---|---|---|
| DHT22 data | 4 | 10 k pull-up if your module lacks one |
| BMP280 SDA / SCL | 21 / 22 | I2C address 0x76 (or 0x77) |
| FC-37 rain sensor (digital) | 27 | Module pulls LOW when wet |
| LDR module (analog) | 35 | ADC1 pin (ADC2 is unusable with WiFi) |
| Motor driver - deploy input | 25 | L298N IN1 / BTS7960 RPWM |
| Motor driver - retract input | 26 | L298N IN2 / BTS7960 LPWM |
| Limit switch - fully deployed | 32 | To GND, `INPUT_PULLUP` |
| Limit switch - fully retracted | 33 | To GND, `INPUT_PULLUP` |
| Manual override button | 13 | To GND, `INPUT_PULLUP` |
| Status LED | 2 | On-board LED |

Motor driver notes: with an L298N leave the ENA jumper fitted (full speed). With a BTS7960 tie R_EN and L_EN to 3.3 V/5 V.

## Power budget

| Load | Power | Duty |
|---|---|---|
| ESP32 | ~0.5 W | continuous |
| Sensors | ~0.2 W | continuous |
| Wiper motor | 60-100 W peak | 2-3 minutes per deployment |

Battery: 12 V x 20 Ah = 240 Wh. Standby draw under 1 W, so more than 200 hours without sun (paper measured 218 h).

## Recommended additions not listed in the paper's bill of materials

- **12 V to 5 V DC-DC converter** (2 A or more) to power the ESP32 and sensors from the battery.
- **Inline fuse** on the battery positive lead, sized for the motor stall current.
- **Flyback/TVS protection** across the motor to suppress switching transients.
- **Motor driver sizing:** a 60-100 W wiper motor draws roughly 5-8 A at 12 V under load and more at stall. An L298N is rated for about 2 A continuous, so use a **BTS7960** (or similar) for anything beyond a small bench prototype.
- Shield the BMP280 and DHT22 from direct sun and rain (radiation shield / Stevenson-style housing), but keep the rain sensor and LDR exposed.

## Bill of materials

See [`hardware/bom.csv`](../hardware/bom.csv) for component costs (INR) and [economics.md](economics.md) for system-level costs.

## Bench test checklist

1. Flash the firmware with the **motor disconnected** and confirm sensor readings over Serial (115200 baud).
2. Check limit-switch polarity by pressing each switch and reading `s` (status).
3. Connect the motor with no canopy attached; verify direction and that limit switches stop it.
4. Verify the motor timeout cuts power if a limit switch is never reached.
5. Only then attach the canopy mechanism.

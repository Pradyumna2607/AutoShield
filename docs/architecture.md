# System Architecture

AutoShield has three layers (paper Section 3): structural-mechanical, sensing & control, and user interface.

## Layers

```mermaid
flowchart TB
  subgraph UI[User interface layer]
    APP[Mobile app - Bluetooth / WiFi]
    WA[WhatsApp alerts - CallMeBot]
    BTN[Physical override button]
  end
  subgraph CTRL[Sensing and control layer]
    S[BMP280 / DHT22 / FC-37 / LDR] --> ESP[ESP32 - sensor fusion + state machine]
    ESP --> DRV[Motor driver]
    ESP --> LOG[(SPIFFS CSV log)]
    LIM[Limit switches] --> ESP
  end
  subgraph STR[Structural-mechanical layer]
    DRV --> MOT[12V wiper motor + pulley + cable]
    MOT --> CAN[PVC-coated polyester canopy]
    CW[Counterweight ~60% of canopy weight] --- MOT
    CAN --> GUT[Gutters and pillar pipes] --> TANK[(Water storage / diversion)]
  end
  APP <--> ESP
  ESP --> WA
  BTN --> ESP
```

## Power

```mermaid
flowchart LR
  PV[50 W solar panel] --> CC[10 A PWM charge controller] --> BAT[12 V 20 Ah lead-acid battery]
  BAT --> BUCK[12 V to 5 V converter] --> ESP[ESP32 + sensors ~0.7 W]
  BAT --> DRV[Motor driver] --> MOT[Wiper motor 60-100 W peak, 2-3 min]
```

Battery energy: 12 V x 20 Ah = 240 Wh. At under 1 W standby this exceeds 200 hours without solar input (218 h measured in the paper).

## Trigger state machine

```mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> Alert: Stage 1 conditions met
  Alert --> Idle: Stage 1 conditions lost
  Alert --> PreDeploy: Stage 1 persists AND temp drop, OR rain detected
  Idle --> PreDeploy: rain detected
  PreDeploy --> Idle: farmer cancels (lockout applies)
  PreDeploy --> Deployed: 2-minute countdown ends, motor runs to limit switch
  Deployed --> Idle: manual retract
```

See [trigger-logic.md](trigger-logic.md) for thresholds, interpretation notes and assumptions.

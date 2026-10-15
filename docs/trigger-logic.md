# Trigger Logic

The control loop evaluates sensor data **every 60 seconds** (paper Section 4.2). The reference implementation lives in
[`firmware/autoshield/trigger_logic.h`](../firmware/autoshield/trigger_logic.h) with a Python mirror in
[`simulation/trigger_logic.py`](../simulation/trigger_logic.py).

## Stages

| Stage | Condition | Source |
|---|---|---|
| 1 - Alert | Pressure drop > 2 hPa in 30 min **AND** RH > 80 % **AND** significant cloud cover (LDR) | Paper |
| 2 - Deploy | Stage 1 persists **AND** temperature drop > 1.5 C in 15 min, **OR** rain sensor detects precipitation | Paper |
| Pre-deploy notice | 2-minute notification to the farmer, who can cancel | Paper |
| 3 - Hold | Canopy stays deployed until the farmer retracts manually (app or button) | Paper |

## Interpretation notes and assumptions

The paper leaves some details open. The reference code makes the following choices; all are in `Config` and easy to change.

| Item | Choice in this repo | Why |
|---|---|---|
| Operator precedence in Stage 2 | `(Stage 1 persists AND temp drop) OR rain` | Standard AND-before-OR reading. Rain-sensor confirmation deploys on its own. |
| "Stage 1 persists" | 3 consecutive 60 s cycles (`stage1_persist_cycles`) | Not specified in the paper. |
| Cancel behaviour | 30-cycle (30 min) lockout after a cancel (`cancel_lockout_cycles`) | Prevents immediate re-notification. Not specified in the paper. |
| Warm-up | Stage 1 needs 31 pressure samples (about 30 min) before it can fire; rain detection works immediately | A history window is required for drop calculations. |
| Light scale | `light_norm` in 0-1, cloudy below 0.40 | Depends on the LDR module and mounting; calibrate in the field. |
| Pre-deploy delay when rain is already detected | Same 2-minute window; set `predeploy_cycles = 0` for immediate deployment | Keeps behaviour uniform; consider 0 for production. |

## Known limitations

- **LDR at night:** a dark sky at night looks like heavy cloud. Consider disabling `use_ldr` at night or adding a clock/sun-position check.
- **Rain sensor dew:** the FC-37 responds to condensation as well as rain, which can cause false Stage-2 triggers on humid mornings.
- **Thresholds are not universal:** the 2 hPa / 30 min and 1.5 C / 15 min values should be validated against local weather data.
- **Pressure sensor noise and drift:** the BMP280 drifts slowly with temperature; the 30-minute differential approach reduces but does not remove this.

## Try it

```bash
cd simulation
pip install -r requirements.txt
pytest -q
python demo.py   # regenerates docs/images/trigger_simulation.png (synthetic data)
```

![Synthetic pre-rain scenario](images/trigger_simulation.png)

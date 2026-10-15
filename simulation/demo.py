"""Run the trigger logic on a synthetic pre-rain scenario and plot the result.

    python demo.py            # writes ../docs/images/trigger_simulation.png

The data are SYNTHETIC (generated here), not field measurements.
"""
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

from trigger_logic import Event, Reading, TriggerLogic  # noqa: E402

rng = np.random.default_rng(42)
N, ONSET = 120, 40  # minutes simulated, minute the weather turns


def synth(m):
    k = max(0, m - ONSET)
    p = 1010.0 - 0.12 * k + rng.normal(0, 0.05)
    t = 29.0 - 0.15 * k + rng.normal(0, 0.05)
    h = min(95.0, 55.0 + 1.2 * k) + rng.normal(0, 0.5)
    light = max(0.1, 0.9 - 0.03 * k) + rng.normal(0, 0.01)
    return Reading(p, t, h, light, rain_detected=(m >= ONSET + 62))


logic, rows, marks = TriggerLogic(), [], {}
for m in range(N):
    r = synth(m)
    e = logic.step(r)
    rows.append(r)
    if e is not Event.NONE:
        marks.setdefault(e, m)

fig, ax = plt.subplots(4, 1, figsize=(9, 8), sharex=True)
series = [("Pressure (hPa)", [r.pressure_hpa for r in rows]),
          ("Temperature (°C)", [r.temp_c for r in rows]),
          ("Humidity (%)", [r.humidity_pct for r in rows]),
          ("Light (0-1)", [r.light_norm for r in rows])]
for a, (name, y) in zip(ax, series):
    a.plot(y, lw=1.5)
    a.set_ylabel(name)
    a.grid(alpha=0.3)
    for ev, col in ((Event.ALERT_RAISED, "orange"), (Event.PRE_DEPLOY_NOTIFY, "red"),
                    (Event.DEPLOY, "darkred")):
        if ev in marks:
            a.axvline(marks[ev], color=col, ls="--", lw=1)
ax[0].set_title("AutoShield trigger logic on a synthetic pre-rain scenario\n"
                "orange = alert, red = farmer notified, dark red = canopy deploys")
ax[-1].set_xlabel("Minutes")
fig.tight_layout()
out = os.path.join(os.path.dirname(__file__), "..", "docs", "images", "trigger_simulation.png")
fig.savefig(out, dpi=150)
print({e.value: m for e, m in marks.items()}, "->", os.path.abspath(out))

# Safety Notice

AutoShield involves electrical, mechanical and structural hazards. This repository is a **research prototype** and is provided without warranty.

## Electrical
- Lead-acid batteries can deliver very high short-circuit current. Fit an **inline fuse** on the battery positive lead.
- Use correctly rated wire, an IP65 enclosure and weatherproof connectors. Keep mains-voltage wiring out of the field unit.
- Bench-test the firmware with the **motor disconnected** before any mechanical connection.

## Mechanical
- Keep people and animals clear of the canopy, cables and pulleys whenever the system is powered.
- The firmware cuts the motor on limit-switch contact or after a timeout, but this is **not** a certified safety function. Add a hardware end-stop and an emergency cut-off switch.
- Pillars, foundations and cable tensioning must be designed for local wind loads by a qualified engineer. A large deployed canopy can act as a sail.

## Operational
- Do not rely on AutoShield as the sole protection for crops or property in extreme weather.
- Test thresholds against local weather data before trusting automatic deployment.

## Reporting
Report security or safety issues privately via GitHub **Security -> Report a vulnerability**.

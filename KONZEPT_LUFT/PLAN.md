# BMP280 Air Pressure Sensor as Start/Stop Trigger (AIR Variant)

## Concept

Instead of a physical pressure plate (GPIO + ISR), a BMP280 air pressure sensor inside a
sealed rubber hose is used. When a tyre rolls over the hose, the air inside compresses —
the sensor detects the pressure spike and triggers the timer.
Principle: pneumatic road counter.

---

## Physics

- Hose: ~15 mm ID, ~50 cm long, one end sealed
- Pressure spike on trigger: **~50–500 Pa** (0.5–5 hPa) depending on hose length + tyre width
- BMP280 resolution: 0.0016 Pa → spike is clearly detectable
- Trigger: `currentPressure > basePressure + cfg_pressure_threshold_pa`

---
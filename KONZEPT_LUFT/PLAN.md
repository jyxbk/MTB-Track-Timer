# BMP280 Luftdruck-Sensor als Start/Stop-Trigger (LUFT-Variante)

## Konzept

Statt einer physischen Druckplatte (GPIO + ISR) wird ein BMP280-Luftdrucksensor in einem
geschlossenen Gummischlauch verwendet. Wenn ein Reifen auf den Schlauch fährt, komprimiert
sich die Luft darin — der Sensor erkennt den Drucksprung und löst den Timer aus.
Prinzip: pneumatischer Straßenzähler.

---

## Physik

- Schlauch: ~15 mm ID, ~50 cm lang, ein Ende verschlossen
- Drucksprung beim Überfahren: **~50–500 Pa** (0,5–5 hPa) je nach Schlauchlänge + Reifenbreite
- BMP280-Auflösung: 0,0016 Pa → Sprung gut detektierbar
- Trigger: `aktuellerDruck > Basisdruck + cfg_pressure_threshold_pa`

---

## Hardware

### BMP280 Sensor
| Eigenschaft       | Wert                        |
|-------------------|-----------------------------|
| Protokoll         | I2C (gemeinsam mit OLED)    |
| I2C-Adresse       | **0x76** (SDO=GND)          |
| Spannungsversorgung | 3,3 V                     |
| Druckbereich      | 300–1100 hPa                |
| Sampling-Modus    | NORMAL, OSR_P=1, Standby=1ms → ~140 Hz |

### Verkabelung (Heltec WiFi LoRa 32 V3)
- SDA → GPIO 17 (gleicher Bus wie OLED 0x3C)
- SCL → GPIO 18
- VCC → 3,3 V | GND → GND

### Schlauch-Aufbau
- Gummi- oder Silikon-Schlauch, 15 mm ID, ~50 cm
- Ein Ende: Blindstopfen
- Anderes Ende: Barb-Fitting → BMP280-Anschluss (M5-Fitting oder 3D-gedruckt)
- Montage: quer über die Fahrspur

---

## Software-Architektur

### Polling statt ISR (I2C nicht ISR-sicher)
```
loop() alle ~5 ms:
  float p = bmp.readPressure();
  if (bmpCalibrated && p > bmpBaseline + cfg_pressure_threshold_pa && !plateFlag):
    plateTriggerUs = esp_timer_get_time()
    plateFlag      = true
```

### Einmalige Startup-Kalibrierung
```
setup():
  OLED: "Kalibrierung..."
  N Messungen über cfg_bmp_cal_delay_ms verteilt → Mittelwert = bmpBaseline
  bmpCalibrated = true
  OLED: normale Anzeige
```
Vorteil: einfach, vorhersehbar — keine Baseline-Drift während des Rennens.

Rekalibrierung: Web-UI Button "Neu kalibrieren" → `/calibrate` Endpoint

### Timing-Genauigkeit
| Methode             | Präzision    |
|---------------------|--------------|
| ISR (Druckplatte)   | ~1 µs        |
| BMP280 Polling      | ~5–10 ms     |

Bei 50 km/h: ~7 cm Positionsunsicherheit pro ms — für Hobby-Zeitmessung akzeptabel.

---

## Neue NVS-Parameter

| Parameter                  | NVS-Key    | Standard | Beschreibung                        |
|----------------------------|------------|----------|-------------------------------------|
| `cfg_pressure_threshold_pa`| `bmpthresh`| 80 Pa    | Drucksprung-Schwelle in Pascal      |
| `cfg_bmp_cal_delay_ms`     | `bmpcaldly`| 3000 ms  | Wartezeit nach Einschalten           |

NVS-Namespaces: Start=`mtb-cfg-lu` / Finish=`mtb-cfg2-lu` / Split=`mtb-cfg3-lu`

---

## Neue Endpoints

| Endpoint     | Methode | Beschreibung                              |
|--------------|---------|-------------------------------------------|
| `/calibrate` | GET     | Neue Baseline messen (ohne Neustart)      |

---

## Ordnerstruktur

```
KONZEPT_LUFT/
  PLAN.md                   ← dieses Dokument

MTB_Timer_Start_LUFT/
  MTB_Timer_Start_LUFT.ino  ← BMP280-Polling, kein ISR
  display.ino
  web_html.ino
  web_handlers.ino

MTB_Timer_Finish_LUFT/
  MTB_Timer_Finish_LUFT.ino
  display.ino
  web_html.ino
  web_handlers.ino

MTB_Timer_Split_LUFT/
  MTB_Timer_Split_LUFT.ino
  display.ino
  web_html.ino
  web_handlers.ino
```

**Bibliothek:** `Adafruit BMP280` (Arduino Library Manager)

---

## Verifikation

1. BMP280 erkannt? → Serial: `[LUFT] BMP280 OK @ 0x76`
2. Baseline stabil? → Serial: `[LUFT] Baseline: 101325 Pa`
3. Trigger durch Schlauch eindrücken → plateFlag gesetzt
4. Debounce: zweites Drücken < cfg_debounce_ms → kein zweiter Trigger
5. Web-UI: Schwellwert ändern → sofort wirksam

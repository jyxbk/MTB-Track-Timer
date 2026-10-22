# MTB Track Timer – Electronic Lap Timing System

**Wireless timing for MTB Downhill / time trials** with up to three nodes over LoRa 868 MHz.  
No internet, no mobile network — works completely offline via a local Wi-Fi hotspot.

[![Build firmware](https://github.com/jyxbk/MTB-Track-Timer/actions/workflows/build.yml/badge.svg)](https://github.com/jyxbk/MTB-Track-Timer/actions/workflows/build.yml)
![Version](https://img.shields.io/badge/Version-v11-blue)
![Hardware](https://img.shields.io/badge/Hardware-Heltec%20%7C%20LILYGO-green)
![LoRa](https://img.shields.io/badge/LoRa-868%20MHz-orange)
![Trigger](https://img.shields.io/badge/Trigger-Pressure%20Plate%20%7C%20Air%20Pressure-purple)
![License](https://img.shields.io/badge/License-MIT-lightgrey)

---

## Overview

| Node | Function |
|------|----------|
| **Start** | Records start time, displays results, hosts Web UI, SD logging |
| **Finish** | Records finish time → sends via LoRa to Start |
| **Split** | Records intermediate time → sends via LoRa to Start |

Nodes communicate via **LoRa (868 MHz, SF7, BW 125 kHz)** — range up to ~1 km line-of-sight.  
Operated via browser on a smartphone over the Wi-Fi hotspot (`http://192.168.4.1`).

---

## Supported Hardware

| Board | Chip | LoRa | SD | Flash Tool | AIR-capable |
|-------|------|------|----|-----------|:-----------:|
| **Heltec WiFi LoRa 32 V3** | ESP32-S3 | SX1262 | – | PlatformIO | ✓ |
| **LILYGO TTGO T3 V1.6.1** | ESP32-PICO-D4 | SX1276 | ✓ MicroSD | Arduino IDE | – |

Both variants are **LoRa-compatible** and can be mixed.  
The AIR variant (`_LUFT`) is currently only available for **Heltec** and requires an additional **BMP280 sensor** (I²C, 3.3 V).

---

## Trigger Variants

| Variant | Trigger | Folder suffix | Description |
|---------|---------|---------------|-------------|
| **Standard** | GPIO pressure plate | *(no suffix)* | Physical contact plate, ISR-based |
| **AIR** | BMP280 air pressure sensor | `_LUFT` | Pneumatic rubber hose, no contact needed |

The AIR variant uses a sealed rubber hose with a BMP280 pressure sensor (I²C). When a tyre rolls over the hose, the sensor detects the pressure spike (~50–500 Pa) and triggers the timer — just like a classic pneumatic road counter. No wear, no broken wires.

---

## Project Structure

```
MTB-Track-Timer/
├── MTB_Timer_Start/            # Start node        (Heltec, pressure plate)
├── MTB_Timer_Start_LILYGO/     # Start node        (LILYGO, pressure plate)
├── MTB_Timer_Start_LUFT/       # Start node        (Heltec, air pressure BMP280)
├── MTB_Timer_Finish/           # Finish node       (Heltec, pressure plate)
├── MTB_Timer_Finish_LILYGO/    # Finish node       (LILYGO, pressure plate)
├── MTB_Timer_Finish_LUFT/      # Finish node       (Heltec, air pressure BMP280)
├── MTB_Timer_Split/            # Split node        (Heltec, pressure plate)
├── MTB_Timer_Split_LILYGO/     # Split node        (LILYGO, pressure plate)
├── MTB_Timer_Split_LUFT/       # Split node        (Heltec, air pressure BMP280)
├── KONZEPT_LUFT/               # AIR variant concept & planning
├── docs/
│   ├── BEDIENUNGSANLEITUNG.md  # Full user manual
│   └── MTB_Timer_Preview.html  # Web UI preview (open offline)
└── platformio.ini              # Root placeholder (flash from subfolders!)
```

Each node folder contains:

| File | Content |
|------|---------|
| `*.ino` (main) | Includes, globals, ISRs, `setup()`, `loop()` |
| `display.ino` | OLED drawing functions |
| `web_html.ino` | HTML page + `/state` JSON |
| `web_handlers.ino` | All HTTP handlers |
| `sd_log.ino` | SD card, tracks, sessions *(LILYGO only)* |

---

## Features

- **Precise timing** – µs-accurate ISR timestamps, automatic LoRa latency compensation
- **Web UI** – Apple-style, dark/light mode, runs entirely in the browser without an app
- **Duel mode** – up to 10 riders, automatic leaderboard
- **Lap mode** – unlimited laps, best-time tracking
- **Split time** – optional third node
- **SD logging** *(LILYGO)* – tracks, riders, sessions, leaderboard, CSV export
- **OTA update** – firmware update directly via browser, no USB required
- **Battery powered** – deep sleep (<1 mA), ~10–14 h runtime with 18650
- **LoRa diagnostics** – RTT measurement, signal strength

---

## Quick Start

### Heltec (PlatformIO)

```bash
# Open subfolder as project, e.g. Start node:
code MTB_Timer_Start

# Flash
pio run -t upload

# Serial monitor
pio device monitor --baud 115200

# OTA update (after first flash)
# Browser → http://192.168.4.1/update → select .bin
```

### LILYGO (Arduino IDE)

```
Board:         TTGO LoRa32-OLED
Upload Speed:  115200
Port:          COMxx  (CH9102 driver required)
```

> **Manual download mode** if auto-reset fails:  
> Hold BOOT → press RST briefly → release BOOT → start upload

### First Steps After Flashing

1. Power on device → OLED shows Wi-Fi name
2. Connect smartphone to the Wi-Fi (e.g. `MTB-Time-START`)
3. Browser: `http://192.168.4.1`
4. Tab **LoRa** → **Sync time**
5. Ready to ride 🏔️

---

## Required Libraries

| Library | Version | Heltec | LILYGO |
|---------|---------|:------:|:------:|
| RadioLib | ≥ 6.6.0 | ✓ | ✓ |
| U8g2 | ≥ 2.35.19 | ✓ | ✓ |
| SD | (Core) | – | ✓ |
| WebServer, WiFi, Preferences, Update | (Core) | ✓ | ✓ |

---

## LoRa Configuration

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz |
| Bandwidth | 125 kHz |
| Spreading Factor | SF7 |
| Coding Rate | 4/5 |
| Sync Word | 0x12 (private) |
| TX Power | 14 dBm (configurable 2–20 dBm) |

---

## Message Protocol

| Message | From → To | Meaning |
|---------|-----------|---------|
| `STX` / `STX:<name>` | Start → Finish/Split | Run started |
| `TIM:<µs>` | Finish → Start | Finish time in microseconds |
| `SPL:<µs>` | Split → Start | Intermediate time in microseconds |
| `ACK` | Start → Finish/Split | Acknowledgement |
| `CAN` | Start → Finish/Split | Run cancelled |
| `PNG` / `POG` | Start ↔ Finish/Split | Ping / Pong (RTT) |
| `TSY:<unix_ms>` | Start → Finish/Split | Time synchronisation |

---

## Web Interface

| Tab | Content |
|-----|---------|
| **Live** | Running time, last/best result, battery, connection status |
| **History** | Last 20 results, medals, CSV export |
| **Duel** | Control duel & lap mode |
| **Tracks** | Manage tracks/riders, leaderboard, session archive *(LILYGO)* |
| **LoRa** | Signal strength, RTT measurement, time sync |
| **Settings** | All configurable parameters |

---

## Documentation

- [User Manual](docs/BEDIENUNGSANLEITUNG.md) – full operating guide
- [Web UI Preview](docs/MTB_Timer_Preview.html) – open HTML file locally in browser

---

## License

MIT License – free to use, no warranty.
# MTB Track Timer – Elektronische Zeitmessung

**Drahtlose Zeitmessung für MTB Downhill / Zeitfahren** mit bis zu drei Nodes über LoRa 868 MHz.
Kein Internet, kein Mobilfunk – funktioniert komplett offline über einen lokalen WLAN-Hotspot.

![Version](https://img.shields.io/badge/Version-v11-blue)
![Hardware](https://img.shields.io/badge/Hardware-Heltec%20%7C%20LILYGO-green)
![LoRa](https://img.shields.io/badge/LoRa-868%20MHz-orange)
![Trigger](https://img.shields.io/badge/Trigger-Druckplatte%20%7C%20Luftdruck-purple)
![License](https://img.shields.io/badge/License-MIT-lightgrey)

---

## Übersicht

| Node | Funktion |
|------|----------|
| **Start** | Startzeitpunkt, Ergebnisanzeige, Web-UI, SD-Logging |
| **Finish** | Zielzeitpunkt → sendet Zeit per LoRa an Start |
| **Split** | Zwischenzeit → sendet Zeit per LoRa an Start |

Die Nodes kommunizieren per **LoRa (868 MHz, SF7, BW 125 kHz)** – Reichweite bis ~1 km Sichtlinie.  
Bedienung über den Browser am Smartphone via WLAN-Hotspot (`http://192.168.4.1`).

---

## Unterstützte Hardware

| Board | Chip | LoRa | SD | Flash-Tool | LUFT-fähig |
|-------|------|------|----|-----------|:----------:|
| **Heltec WiFi LoRa 32 V3** | ESP32-S3 | SX1262 | – | PlatformIO | ✓ |
| **LILYGO TTGO T3 V1.6.1** | ESP32-PICO-D4 | SX1276 | ✓ MicroSD | Arduino IDE | – |

Beide Varianten sind **LoRa-kompatibel** und können gemischt eingesetzt werden.  
Die LUFT-Variante (`_LUFT`) ist aktuell nur für **Heltec** verfügbar und benötigt zusätzlich einen **BMP280-Sensor** (I²C, 3,3 V).

---

## Trigger-Varianten

| Variante | Trigger | Ordner-Suffix | Beschreibung |
|----------|---------|---------------|--------------|
| **Standard** | GPIO-Druckplatte | *(kein Suffix)* | Physische Kontaktplatte, ISR-basiert |
| **LUFT** | BMP280 Luftdrucksensor | `_LUFT` | Pneumatischer Gummischlauch, kein Kontakt nötig |

Die LUFT-Variante nutzt einen verschlossenen Gummischlauch mit BMP280-Drucksensor (I²C). Wenn ein Reifen den Schlauch überfährt, erkennt der Sensor den Drucksprung (~50–500 Pa) und löst den Timer aus — wie ein klassischer pneumatischer Straßenzähler. Kein Verschleiß, kein Kabelbruch.

---

## Projektstruktur

```
MTB-Track-Timer/
├── MTB_Timer_Start/            # Start-Node        (Heltec, Druckplatte)
├── MTB_Timer_Start_LILYGO/     # Start-Node        (LILYGO, Druckplatte)
├── MTB_Timer_Start_LUFT/       # Start-Node        (Heltec, Luftdruck BMP280)
├── MTB_Timer_Finish/           # Finish-Node       (Heltec, Druckplatte)
├── MTB_Timer_Finish_LILYGO/    # Finish-Node       (LILYGO, Druckplatte)
├── MTB_Timer_Finish_LUFT/      # Finish-Node       (Heltec, Luftdruck BMP280)
├── MTB_Timer_Split/            # Split-Node        (Heltec, Druckplatte)
├── MTB_Timer_Split_LILYGO/     # Split-Node        (LILYGO, Druckplatte)
├── MTB_Timer_Split_LUFT/       # Split-Node        (Heltec, Luftdruck BMP280)
├── KONZEPT_LUFT/               # Konzept & Planung LUFT-Variante
├── docs/
│   ├── BEDIENUNGSANLEITUNG.md  # Vollständige Bedienungsanleitung
│   └── MTB_Timer_Preview.html  # Web-UI Vorschau (offline öffnen)
└── platformio.ini              # Root-Platzhalter (Unterordner flashen!)
```

Jeder Node-Ordner enthält:

| Datei | Inhalt |
|-------|--------|
| `*.ino` (Haupt) | Includes, Globals, ISRs, `setup()`, `loop()` |
| `display.ino` | OLED-Zeichenfunktionen |
| `web_html.ino` | HTML-Seite + `/state` JSON |
| `web_handlers.ino` | Alle HTTP-Handler |
| `sd_log.ino` | SD-Karte, Strecken, Sessions *(LILYGO only)* |

---

## Features

- **Präzise Zeitmessung** – µs-genaue ISR-Timestamps, LoRa-Kompensation konfigurierbar
- **Web-UI** – Apple-Style, Dark/Light-Mode, läuft komplett im Browser ohne App
- **Duell-Modus** – bis zu 10 Fahrer, automatische Rangliste
- **Runden-Modus** – beliebig viele Runden, Bestzeit-Tracking
- **Zwischenzeit (Split)** – optionaler dritter Node
- **SD-Logging** *(LILYGO)* – Strecken, Fahrer, Sessions, Bestenliste, Export als CSV
- **OTA-Update** – Firmware-Update direkt über den Browser, kein USB nötig
- **Akkubetrieb** – Deep Sleep (<1 mA), Laufzeit ~10–14 h mit 18650
- **LoRa-Diagnose** – RTT-Messung, Signalstärke, automatische Kompensationsempfehlung

---

## Schnellstart

### Heltec (PlatformIO)

```bash
# Unterordner als Projekt öffnen, z.B. Start-Node:
code MTB_Timer_Start

# Flashen
pio run -t upload

# Serieller Monitor
pio device monitor --baud 115200

# OTA-Update (nach erstem Flash)
# Browser → http://192.168.4.1/update → .bin auswählen
```

### LILYGO (Arduino IDE)

```
Board:         TTGO LoRa32-OLED
Upload Speed:  115200
Port:          COMxx  (CH9102-Treiber erforderlich)
```

> **Manueller Download-Modus** falls Auto-Reset fehlschlägt:  
> BOOT halten → RST kurz drücken → BOOT loslassen → Upload starten

### Erste Schritte nach dem Flash

1. Gerät einschalten → OLED zeigt WLAN-Name
2. Am Smartphone mit dem WLAN verbinden (z.B. `MTB-Time-START`)
3. Browser: `http://192.168.4.1`
4. Tab **LoRa** → **Uhrzeit synchronisieren**
5. Los geht's 🏔️

---

## Benötigte Bibliotheken

| Bibliothek | Version | Heltec | LILYGO |
|------------|---------|:------:|:------:|
| RadioLib | ≥ 6.6.0 | ✓ | ✓ |
| U8g2 | ≥ 2.35.19 | ✓ | ✓ |
| SD | (Core) | – | ✓ |
| WebServer, WiFi, Preferences, Update | (Core) | ✓ | ✓ |

---

## LoRa-Konfiguration

| Parameter | Wert |
|-----------|------|
| Frequenz | 868.0 MHz |
| Bandbreite | 125 kHz |
| Spreading Factor | SF7 |
| Coding Rate | 4/5 |
| Sync Word | 0x12 (privat) |
| TX-Leistung | 14 dBm (konfigurierbar 2–20 dBm) |

---

## Nachrichtenprotokoll

| Nachricht | Von → Nach | Bedeutung |
|-----------|------------|-----------|
| `STX` / `STX:<name>` | Start → Finish/Split | Lauf gestartet |
| `TIM:<µs>` | Finish → Start | Zielzeit in Mikrosekunden |
| `SPL:<µs>` | Split → Start | Zwischenzeit in Mikrosekunden |
| `ACK` | Start → Finish/Split | Bestätigung |
| `CAN` | Start → Finish/Split | Lauf abgebrochen |
| `PNG` / `POG` | Start ↔ Finish/Split | Ping / Pong (RTT) |
| `TSY:<unix_ms>` | Start → Finish/Split | Zeitsynchronisation |

---

## Web-Interface

| Tab | Inhalt |
|-----|--------|
| **Live** | Laufende Zeit, letztes/bestes Ergebnis, Akku, Verbindungsstatus |
| **Verlauf** | History (letzte 20), Medaillen, CSV-Export |
| **Duell** | Duell- & Runden-Modus steuern |
| **Strecken** | Strecken/Fahrer verwalten, Bestenliste, Session-Archiv *(LILYGO)* |
| **LoRa** | Signalstärke, RTT-Messung, Zeitsync |
| **Einst.** | Alle konfigurierbaren Parameter |

---

## Dokumentation

- [Bedienungsanleitung](docs/BEDIENUNGSANLEITUNG.md) – vollständige Anleitung für den Betrieb
- [Web-UI Vorschau](docs/MTB_Timer_Preview.html) – HTML-Datei lokal im Browser öffnen

---

## Lizenz

MIT License – frei verwendbar, keine Garantie.

# MTB Timer – User Manual

## System Overview

The MTB timing system consists of up to three devices (nodes) communicating wirelessly via LoRa (868 MHz):

| Node | Function | Marking |
|------|----------|---------|
| **Start** | Starts timing, displays results, manages tracks | Main device |
| **Finish** | Stops time at the finish line | Finish device |
| **Split** | Optional intermediate time | Split device |

All devices open their own Wi-Fi network. Connect your smartphone/tablet and open `http://192.168.4.1` in the browser.

---

## 1. Power On & Connect

1. **Power on device** (connect battery or plug in USB)
2. OLED display shows boot screen with Wi-Fi name and IP address
3. On smartphone: **Wi-Fi settings** → select network:
   - Start: `MTB-Time-START` (Heltec) or `MTB-Start-LILYGO`
   - Finish: `MTB-Timer-Ziel` or `MTB-Ziel-LILYGO`
   - Split: `MTB-Split-Node` or `MTB-Split-LILYGO`
4. Open browser → `http://192.168.4.1`

> **Tip:** Devices have no internet. The browser may show "No internet access" — this is normal.

---

## 2. Initial Setup

### Sync Time
Without time sync, no timestamps are saved in history.

1. Open Start browser → Tab **LoRa** → **Time Sync** → Button **Synchronise**
2. Time is taken from the smartphone and automatically forwarded to Finish/Split
3. Status shows ✓ when synchronised

### Connect Nodes
- Start automatically sends a **ping** to Finish/Split every 30 s
- Connection indicator in Live tab: **green** = connected (< 35 s), **yellow** = weak, **red** = disconnected
- If red: move devices closer together or send ping manually (LoRa tab)

---

## 3. Simple Run (Standard Mode)

1. **Start node** is ready → OLED shows "READY" or browser shows green status
2. **Rider passes the start sensor** → timer starts
3. OLED and browser show running time
4. **Rider passes the finish sensor** → time is stopped and displayed on the start device
5. Result is visible for 8 seconds (configurable)

### Cancel a Run
- Browser: Button **Cancel** (Live tab)
- Device: **press PRG button twice quickly** (double-click < 400 ms)

---

## 4. Tracks & Riders (LILYGO with SD Card)

An SD card must be inserted for full tracking.

### Create a Track
1. Browser → Tab **Tracks**
2. **Tracks** → Button **+ New**
3. Enter name (max. 20 characters), info optional (length, difficulty)
4. **Save** → track appears in the list
5. **Select** → track is now active (highlighted orange)

### Create a Rider
1. Browser → Tab **Tracks** → section **Riders** → **+ New**
2. Enter name → **Save**
3. **Select** → rider is active

### Set Condition
In the **Tracks** tab at the top, select the current track condition:
- ☀ Dry | 💧 Wet | 🍂 Leafy | ❄ Ice | — (no info)

The condition applies to all runs in the current session.

### Session Info in Live Tab
Current track, rider and condition are shown as a card in the Live tab. Click **Manage Tracks & Riders** to open the Tracks tab.

---

## 5. Duel Mode

Multiple riders run one after another, automatic leaderboard at the end.

1. Browser → Tab **Duel** → **Start Duel**
2. Select number of riders (2–10)
3. Enter names (shown on OLED and in the leaderboard)
4. **Start** → system waits for sensor trigger
5. Each rider goes in turn
6. After the last rider: automatic leaderboard with times and medals
7. **Close** exits duel mode

### Skip a Rider
Browser → Tab **Duel** → **DNF (Did Not Finish)** — rider is entered as DNF.

---

## 6. Lap Mode

For circuits or repeated sectors.

1. Browser → Tab **Duel** → **Lap Mode**
2. Sensor trigger starts **Lap 1**
3. Each further trigger ends the current lap and starts the next
4. Browser → **Stop Laps** exits the mode and shows best time

---

## 7. History & Statistics

### History Tab
Shows all runs in the current session (last 20 in memory):
- Sort by time (fastest first) or chronological (button top right)
- Medals for positions 1–3
- DNF entries in italics

### Edit Rider Names (LILYGO)
1. History tab → tap the **pencil icon** next to a name
2. Enter name → confirm with **✓**
3. Name is updated in memory and on the SD card

### Load Older Entries (LILYGO with SD)
At the bottom of the History tab: button **↓ Load More** — loads older entries from the SD card.

### Statistics Card (LILYGO)
A statistics card appears automatically below the history showing:
- Number of runs | Best time | Average time | Trend (↑ better / ↓ worse)

---

## 8. Session Archive (LILYGO)

Browser → Tab **Tracks** → section **Session Archive**

- All previous sessions are listed by date
- Click **View** → table with track, rider, time, date, condition
- Load more pages with **Load More**

### Data Export
- **History tab → 💾 CSV** exports the current session as a CSV file
- The file `log.csv` on the SD card contains all runs ever recorded

---

## 9. Leaderboard (LILYGO)

Browser → Tab **Tracks** → section **Leaderboard**
- Top 10 fastest times on the currently selected track
- Loads automatically when the Tracks tab is opened

---

## 10. Settings

Browser → Tab **Settings** (gear icon)

### Timing
| Setting | Default | Description |
|---------|---------|-------------|
| Debounce | 500 ms | Minimum interval between two triggers |
| Result display | 8 s | How long the result stays visible |
| Run timeout | 5 min | Maximum run duration |

### LoRa
| Setting | Default | Description |
|---------|---------|-------------|
| Extra correction | 0 ms | Added to every measured time; radio latency is compensated automatically, so normally 0 |
| TX power | 14 dBm | 2–20 dBm; higher = more range, more current |
| Ping interval | 30 s | How often Start sends a heartbeat to Finish/Split |

### Display (all nodes)
| Setting | Default | Description |
|---------|---------|-------------|
| Extra button GPIO | 255 | GPIO pin for an external button (255 = disabled) |
| Auto-page | 0 s | Seconds until automatic page change (0 = off) |

### Sensor
| Setting | Default | Description |
|---------|---------|-------------|
| GPIO pin | 2 (Heltec) / 4 (LILYGO) | Pressure plate pin |
| Sensor type | NO | NO = closes on trigger, NC = opens on trigger |

### Battery & Display
| Setting | Description |
|---------|-------------|
| Capacity (mAh) | For remaining runtime calculation |
| OLED brightness | 0–255 |

---

## 11. Accuracy and Radio Latency

Finish and Split start their clock when the `STX` message from the Start node arrives. That message
needs some time (start trigger → radio transmission complete, typically 30–60 ms at SF7, longer with
a rider name). Without correction every time would be too short by this amount.

Since this version the latency is compensated **automatically**:

1. The Start node measures the time from the pressure-plate trigger until the `STX` transmission is complete.
2. Finish/Split take the reception timestamp directly in the radio interrupt (no loop delay).
3. When `TIM`/`SPL` arrives, the Start node adds the measured latency to the result.

The setting **Extra correction** (Settings → LoRa) is only for fine-tuning a remaining constant offset
and is *added* to every time. Leave it at 0 unless a reference measurement shows a systematic deviation.
The **Measure** button in the LoRa tab still shows the round-trip time (RTT) as a link-quality indicator.

> Note: Older firmware *subtracted* the compensation value, which made times even shorter.
> The stored value is not taken over after the update (new setting, default 0).

---

## 12. Time Zone Synchronisation

1. Browser → Tab **LoRa** → section **Time Sync**
2. Shows: current time on node, last sync timestamp, offset to browser
3. Button **Synchronise** → transfers smartphone time to all connected nodes
4. Finish and Split are automatically synchronised via LoRa

---

## 13. Device Controls (Buttons)

### PRG Button (on device)
| Action | Duration | Function |
|--------|----------|----------|
| Short press | < 400 ms | Next OLED page |
| Double-click | 2× < 400 ms | Cancel run (during a run) |
| Long press | > 3 seconds | Deep sleep |
| While sleeping | any | Wake up |
| After result | short | Dismiss result |

### External Button (optional)
An external button can be configured via **Settings → Extra button GPIO**. Restart required. Function: next page / dismiss result.

### OLED Pages – Start Node
| Page | Content |
|------|---------|
| P1 MAIN | Status, running time, last/best time, battery |
| P2 SIGNAL | RSSI, SNR, TX/RX stats, uptime |
| P3 HISTORY | Top 4 times |
| P4 DUEL | Current rider, progress |

---

## 14. Deep Sleep / Battery Saving

- Hold PRG button **3 seconds** → device sleeps (< 1 mA consumption)
- Short PRG press → wake up, all settings are preserved
- History and best times are retained in RTC memory (as long as battery > 0)

---

## 15. Troubleshooting

### No connection between nodes
- Are all nodes powered on?
- Distance < 200 m line-of-sight?
- Browser → LoRa tab → **Manual Ping** → response in logs?
- Frequency and SF7/BW125 identical on all nodes?

### Timing does not start
- Sensor cable on the correct GPIO pin?
- Sensor type setting: NO (closes) or NC (opens)?
- Debounce too high? Reduce to 100 ms.

### LILYGO does not start / blank screen
- Do not use GPIO16 as sensor pin (internally used)
- Default sensor pin for LILYGO: GPIO 4

### SD card not detected
- Card inserted correctly?
- Formatted as FAT32?
- SD-MISO is GPIO2 — do not set sensor pin to GPIO2!
- Browser shows SD badge: ✓ Inserted / Not inserted

### Upload fails (LILYGO)
- Reduce upload speed to **115200** (Settings → Upload Speed)
- **Hold BOOT** → **press RST** → **release BOOT** → start upload
- CH9102 driver installed? (Windows may need manual download)

### Wrong time / no timestamps
- Browser → LoRa tab → press **Synchronise**
- Is the time set correctly on the smartphone?

---

## 16. Technical Specifications

| Property | Heltec V3 | LILYGO T3 V1.6.1 |
|----------|-----------|------------------|
| Processor | ESP32-S3 | ESP32-PICO-D4 |
| LoRa chip | SX1262 | SX1276 |
| Frequency | 868 MHz | 868 MHz |
| Range | ~1 km LOS | ~1 km LOS |
| Display | SSD1306 128×64 | SSD1306 128×64 |
| SD card | – | ✓ MicroSD |
| Active current | ~80 mA | ~80 mA |
| Sleep current | < 1 mA | < 1 mA |
| Battery (typical) | 18650 1100 mAh | 18650 1100 mAh |
| Runtime | ~10–14 h | ~10–14 h |
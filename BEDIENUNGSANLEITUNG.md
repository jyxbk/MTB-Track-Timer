# MTB Timer – Bedienungsanleitung

## Systemübersicht

Das MTB-Zeitmesssystem besteht aus bis zu drei Geräten (Nodes), die drahtlos über LoRa (868 MHz) miteinander kommunizieren:

| Node | Funktion | Farbe/Markierung |
|------|----------|-----------------|
| **Start** | Startet die Zeitmessung, zeigt Ergebnisse, verwaltet Strecken | Hauptgerät |
| **Finish** | Stoppt die Zeit am Ziel | Zielgerät |
| **Split** | Optionale Zwischenzeit | Zwischengerät |

Alle Geräte öffnen ein eigenes WLAN-Netzwerk. Du verbindest dich mit dem Smartphone/Tablet und öffnest `http://192.168.4.1` im Browser.

---

## 1. Einschalten & Verbinden

1. **Gerät einschalten** (Akku anschließen oder USB einstecken)
2. OLED-Display zeigt Boot-Screen mit WLAN-Name und IP-Adresse
3. Am Smartphone: **WLAN-Einstellungen** → Netzwerk auswählen:
   - Start: `MTB-Time-START` (Heltec) oder `MTB-Start-LILYGO`
   - Finish: `MTB-Timer-Ziel` oder `MTB-Ziel-LILYGO`
   - Split: `MTB-Split-Node` oder `MTB-Split-LILYGO`
4. Browser öffnen → `http://192.168.4.1`

> **Tipp:** Die Geräte haben kein Internet. Der Browser zeigt evtl. eine Meldung „Kein Internetzugang" — das ist normal.

---

## 2. Erste Inbetriebnahme

### Uhrzeit synchronisieren
Ohne Zeitsync werden keine Timestamps in der History gespeichert.

1. Start-Browser öffnen → Tab **LoRa** → **Uhrzeit-Sync** → Button **Synchronisieren**
2. Die Zeit wird vom Smartphone übernommen und automatisch an Finish/Split weitergegeben
3. Status zeigt ✓ wenn synchronisiert

### Nodes verbinden
- Start sendet alle 30 s automatisch ein **Ping** an Finish/Split
- Verbindungsanzeige im Live-Tab: **grün** = verbunden (< 35 s), **gelb** = schwach, **rot** = getrennt
- Falls rot: Geräte näher zusammenbringen oder Ping manuell senden (LoRa-Tab)

---

## 3. Einfacher Lauf (Standardmodus)

1. **Start-Node** ist bereit → OLED zeigt „BEREIT" oder Browser zeigt grünen Status
2. **Fahrer passiert den Start-Sensor** → Timer startet
3. OLED und Browser zeigen laufende Zeit
4. **Fahrer passiert den Ziel-Sensor** → Zeit wird gestoppt und auf dem Start-Display angezeigt
5. Ergebnis bleibt für 8 Sekunden (konfigurierbar) sichtbar

### Lauf abbrechen
- Browser: Button **Abbrechen** (Live-Tab)
- Gerät: **PRG-Taste zweimal kurz drücken** (Doppelklick < 400 ms)

---

## 4. Strecken & Fahrer einrichten (LILYGO mit SD-Karte)

Für vollständiges Tracking muss eine SD-Karte eingesteckt sein.

### Strecke anlegen
1. Browser → Tab **Strecken**
2. **Strecken** → Button **+ Neu**
3. Name eingeben (max. 20 Zeichen), Info optional (Streckenlänge, Schwierigkeit)
4. **Speichern** → Strecke erscheint in der Liste
5. **Wählen** → Strecke ist jetzt aktiv (orange markiert)

### Fahrer anlegen
1. Browser → Tab **Strecken** → Abschnitt **Fahrer** → **+ Neu**
2. Name eingeben → **Speichern**
3. **Wählen** → Fahrer ist aktiv

### Bedingung setzen
Im Tab **Strecken** oben die aktuelle Streckenbedingung wählen:
- ☀ Trocken | 💧 Nass | 🍂 Laubig | ❄ Eis | — (keine Angabe)

Die Bedingung gilt für alle Läufe der aktuellen Session.

### Session-Info im Live-Tab
Die aktuelle Strecke, Fahrer und Bedingung werden im Live-Tab als Karte angezeigt. Klick auf **Strecken & Fahrer verwalten** öffnet den Strecken-Tab.

---

## 5. Duell-Modus

Mehrere Fahrer fahren nacheinander, automatische Rangliste am Ende.

1. Browser → Tab **Duell** → **Duell starten**
2. Anzahl der Fahrer wählen (2–10)
3. Namen eingeben (erscheinen auf OLED und in der Rangliste)
4. **Starten** → System wartet auf Sensor-Auslösung
5. Jeder Fahrer fährt der Reihe nach
6. Nach dem letzten Fahrer: automatische Rangliste mit Zeiten und Medaillen
7. **Schließen** beendet den Duell-Modus

### Fahrer überspringen
Browser → Tab **Duell** → **DNF (Nicht gefahren)** — Fahrer wird als DNF eingetragen.

---

## 6. Runden-Modus (Lap)

Für Rundstrecken oder wiederholte Sektoren.

1. Browser → Tab **Duell** → **Runden-Modus**
2. Sensor-Auslösung startet **Runde 1**
3. Jede weitere Auslösung beendet die aktuelle Runde und startet die nächste
4. Browser → **Runden stoppen** beendet den Modus und zeigt Bestzeit

---

## 7. Verlauf & Statistiken

### Verlauf-Tab
Zeigt alle Läufe der aktuellen Session (letzte 20 im Speicher):
- Sortierung nach Zeit (schnellste oben) oder chronologisch (Button rechts oben)
- Medaillen für Platz 1–3
- DNF-Einträge kursiv

### Fahrernamen korrigieren (LILYGO)
1. Verlauf-Tab → Auf den **Bleistift-Icon** neben einem Namen tippen
2. Namen eingeben → **✓** bestätigen
3. Name wird im Speicher und auf der SD-Karte aktualisiert

### Ältere Einträge laden (LILYGO mit SD)
Unten im Verlauf-Tab: Button **↓ Mehr laden** — lädt ältere Einträge von der SD-Karte nach.

### Statistiken-Karte (LILYGO)
Unterhalb des Verlaufs erscheint automatisch eine Statistik-Karte mit:
- Anzahl Läufe | Bestzeit | Durchschnittszeit | Trend (↑ besser / ↓ schlechter)

---

## 8. Session-Archiv (LILYGO)

Browser → Tab **Strecken** → Abschnitt **Session-Archiv**

- Alle bisherigen Sessions werden als Datum-Datei angezeigt
- Klick auf **Anzeigen** → Tabelle mit Strecke, Fahrer, Zeit, Datum, Bedingung
- Weitere Seiten mit **Mehr laden** nachladen

### Datenexport
- **Verlauf-Tab → 💾 CSV** exportiert die aktuelle Session als CSV-Datei
- Die Datei `log.csv` auf der SD-Karte enthält alle jemals gemessenen Läufe

---

## 9. Bestenliste (LILYGO)

Browser → Tab **Strecken** → Abschnitt **Bestenliste**
- Top 10 schnellste Zeiten auf der aktiv gewählten Strecke
- Lädt automatisch beim Öffnen des Strecken-Tabs

---

## 10. Einstellungen

Browser → Tab **Einst.** (Zahnrad)

### Timing
| Einstellung | Standard | Beschreibung |
|-------------|----------|--------------|
| Entprellung | 500 ms | Mindestabstand zwischen zwei Auslösungen |
| Ergebnis-Anzeige | 8 s | Wie lange das Ergebnis sichtbar bleibt |
| Lauf-Timeout | 5 min | Maximale Laufdauer |

### LoRa
| Einstellung | Standard | Beschreibung |
|-------------|----------|--------------|
| Kompensation | 0 ms | LoRa-Verzögerung abziehen (empfohlen: RTT/2) |
| Sendeleistung | 14 dBm | 2–20 dBm; höher = mehr Reichweite, mehr Strom |
| Ping-Intervall | 30 s | Wie oft Start an Finish/Split ein Lebenszeichen sendet |

### Display (alle Nodes)
| Einstellung | Standard | Beschreibung |
|-------------|----------|--------------|
| Zusatztaste GPIO | 255 | GPIO-Pin einer externen Taste (255 = deaktiviert) |
| Auto-Seite | 0 s | Sekunden bis automatischer Seitenwechsel (0 = aus) |

### Sensor
| Einstellung | Standard | Beschreibung |
|-------------|----------|--------------|
| GPIO-Pin | 2 (Heltec) / 4 (LILYGO) | Pin der Druckplatte |
| Sensor-Typ | NO | NO = schließt bei Auslösung, NC = öffnet |

### Akku & Display
| Einstellung | Beschreibung |
|-------------|--------------|
| Kapazität (mAh) | Für Restlaufzeit-Berechnung |
| OLED-Helligkeit | 0–255 |

---

## 11. Messgenauigkeit einstellen (LoRa-Kompensation)

Die Funkübertragung zwischen Start und Finish dauert eine kleine Zeit (RTT = Hin- und Rückweg). Um dies zu korrigieren:

1. Browser → Tab **LoRa** → Button **Messen**
2. Das System misst die Laufzeit (RTT) der Funkverbindung
3. Die Empfehlung wird angezeigt (z.B. „22 ms")
4. Button **Übernehmen** → Wert wird in die Einstellungen eingetragen und gespeichert

---

## 12. Zeitzonensynchronisation

1. Browser → Tab **LoRa** → Abschnitt **Uhrzeit-Sync**
2. Zeigt: aktuelle Uhrzeit auf dem Node, letzter Sync-Zeitpunkt, Versatz zum Browser
3. Button **Synchronisieren** → Überträgt die Smartphone-Zeit an alle verbundenen Nodes
4. Finish und Split werden automatisch via LoRa synchronisiert

---

## 13. Gerätebedienung (Tasten)

### PRG-Taste (auf dem Gerät)
| Aktion | Dauer | Funktion |
|--------|-------|----------|
| Kurz drücken | < 400 ms | Nächste OLED-Seite |
| Doppelklick | 2× < 400 ms | Lauf abbrechen (während Lauf) |
| Lang drücken | > 3 Sekunden | Deep Sleep |
| Im Sleep | beliebig | Aufwachen |
| Im Ergebnis | kurz | Ergebnis wegklicken |

### Externe Taste (optional)
Über **Einst. → Zusatztaste GPIO** kann ein beliebiger GPIO als zweite Taste konfiguriert werden. Neustart erforderlich. Funktion: Seite weiterschalten / Ergebnis wegklicken.

### OLED-Seiten Start-Node
| Seite | Inhalt |
|-------|--------|
| P1 MAIN | Status, laufende Zeit, letzte/beste Zeit, Akku |
| P2 SIGNAL | RSSI, SNR, TX/RX-Statistik, Uptime |
| P3 VERLAUF | Top 4 Zeiten |
| P4 DUELL | Aktueller Fahrer, Fortschritt |

---

## 14. Firmware aktualisieren (OTA)

Keine USB-Verbindung nötig — Update über WLAN.

1. Neue `.bin`-Datei auf dem Smartphone speichern
2. Browser → `http://192.168.4.1/update`
3. **Firmware-Datei (.bin)** auswählen
4. **Hochladen & Flashen** → Gerät startet automatisch neu (~30 Sekunden)

---

## 15. Deep Sleep / Akkuschonen

- PRG-Taste **3 Sekunden halten** → Gerät schläft ein (< 1 mA Verbrauch)
- PRG-Taste kurz drücken → Aufwachen, alle Einstellungen bleiben erhalten
- History und Bestzeiten bleiben im RTC-Speicher erhalten (solange Akku > 0)

---

## 16. Fehlerbehebung

### Keine Verbindung zwischen Nodes
- Sind alle Nodes eingeschaltet?
- Abstand < 200 m in freier Sicht?
- Browser → Tab LoRa → **Manueller Ping** → Antwort in den Logs?
- Frequenz und SF7/BW125 identisch auf allen Nodes?

### Zeitmessung startet nicht
- Sensor-Kabel am richtigen GPIO-Pin?
- Einstellung Sensor-Typ: NO (schließt) oder NC (öffnet)?
- Entprellzeit zu hoch? Auf 100 ms reduzieren

### LILYGO startet nicht / leerer Bildschirm
- Kein GPIO16 als Sensor-Pin verwenden (intern belegt)
- Standard Sensor-Pin für LILYGO: GPIO 4

### SD-Karte nicht erkannt
- Karte korrekt eingesteckt?
- FAT32 formatiert?
- SD-MISO ist GPIO2 — Sensor-Pin nicht auf GPIO2 setzen!
- Browser zeigt SD-Badge: ✓ Eingelegt / Nicht eingelegt

### Upload schlägt fehl (LILYGO)
- Upload Speed auf **115200** reduzieren (Einstellungen → Upload Speed)
- **BOOT-Taste halten** → **RST drücken** → **BOOT loslassen** → Upload starten
- CH9102-Treiber installiert? (Windows benötigt ggf. manuellen Download)

### Zeit falsch / keine Zeitstempel
- Browser → Tab LoRa → **Synchronisieren** drücken
- Uhrzeit auf dem Smartphone korrekt eingestellt?

---

## 17. Technische Daten

| Eigenschaft | Heltec V3 | LILYGO T3 V1.6.1 |
|-------------|-----------|------------------|
| Prozessor | ESP32-S3 | ESP32-PICO-D4 |
| LoRa-Chip | SX1262 | SX1276 |
| Frequenz | 868 MHz | 868 MHz |
| Reichweite | ~1 km LOS | ~1 km LOS |
| Display | SSD1306 128×64 | SSD1306 128×64 |
| SD-Karte | – | ✓ MicroSD |
| Stromverbrauch aktiv | ~80 mA | ~80 mA |
| Stromverbrauch Sleep | < 1 mA | < 1 mA |
| Akku (typisch) | 18650 1100 mAh | 18650 1100 mAh |
| Laufzeit | ~10–14 h | ~10–14 h |

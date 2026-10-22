# PLAN – MTB Track Timer weiterentwickeln

Arbeitsplan für die autonome Weiterentwicklung (Claude). Gedächtnis zwischen den Etappen.

## Ziel
Das bestehende Projekt [jyxbk/MTB-Track-Timer](https://github.com/jyxbk/MTB-Track-Timer)
(LoRa-Zeitmessung fürs MTB, ESP32-Knoten Start/Finish/Split) voranbringen: reproduzierbar
baubar machen, automatisch prüfen, Fehler finden und beheben, Duplikate verringern,
Doku auf den tatsächlichen Stand bringen.

## Ausgangslage (Bestandsaufnahme 2026-10-22)
- Lokaler Ordner war leer; Repo von GitHub (`origin/main`, 8 Commits, Tag `v11.0`) übernommen.
- 9 Firmware-Varianten, je ein Ordner mit mehreren `.ino`-Dateien (~20 600 Zeilen):
  Start/Finish/Split × {Heltec V3 (Druckplatte), LILYGO T3 V1.6.1 (Druckplatte, SD), Heltec LUFT (BMP280)}.
- Nur die 3 Heltec-Standardordner haben eine `platformio.ini`; LILYGO (Arduino IDE) und LUFT haben keine.
- Starke Code-Duplikation zwischen den Varianten (z. B. Start vs. Start_LUFT: ~360 Diff-Zeilen von ~900).
- Versionsangaben uneinheitlich (README „v11“, Firmware v20–v24, LUFT v1/v2/v6, Bootlog teils abweichend).
- Kein Build-Check, keine Tests, keine CI. Hardware-Tests sind von hier aus nicht möglich.
- Remote hat zusätzlich einen alten Branch `master` (Stand vor der Übersetzung).

## Anforderungen
- Bestehende Funktion und Protokoll (LoRa-Nachrichten, Web-UI) dürfen nicht brechen;
  Varianten bleiben untereinander kompatibel (gemischter Betrieb Heltec/LILYGO).
- Alle 9 Varianten müssen mit PlatformIO baubar sein (LILYGO weiterhin auch mit Arduino IDE).
- Werkzeuge nur projektlokal (`.venv`, `.pio`), keine Systemänderungen.
- Änderungen klein und nachvollziehbar committen; nach `origin/main` pushen.

## Aufgaben
- [x] 1. Bestandsaufnahme, Repo übernehmen, PLAN.md anlegen
- [x] 2. Build-Umgebung: PlatformIO in `.venv`; `platformio.ini` für LILYGO- und LUFT-Ordner;
      Skript `tools/build_all.sh`, das alle 9 Varianten baut; Kompilierfehler/-warnungen beheben
  - [x] PlatformIO 6.2.0 in `.venv`, Core-Dir projektlokal `.pio-core` (`PLATFORMIO_CORE_DIR`)
  - [x] `MTB_Timer_Start` baut erfolgreich (espressif32 8.0.0, Arduino-Core 3.3.x; RAM 15.5 %, Flash 36.8 %)
  - [x] Finish/Split (Heltec) bauen
  - [x] `platformio.ini` für 3× LILYGO und 3× LUFT angelegt; alle 9 Varianten bauen
  - [x] `tools/build_all.sh` (baut alle oder genannte Ordner); Plattform auf `espressif32 @ 8.0.0` gepinnt
  - [x] Leere DBG-Makros → `do {} while (0)` (Warnung „empty body“ in `if/else`-Ketten)
  - Restliche Warnungen (Format-Truncation in display/web, `%u` mit `uint32_t`, unbenutztes `isBest`,
    IRAM-Attribut doppelt durch .ino-Prototypen) → in Aufgabe 4 prüfen
- [x] 3. GitHub-Actions-Workflow: alle Varianten bei jedem Push bauen (Firmware als Artefakt)
  - `.github/workflows/build.yml`: Matrix über 9 Ordner, ruft `tools/build_all.sh <Ordner>`,
    lädt `firmware.bin` je Variante als Artefakt hoch; Build-Badge in README. Erster Lauf grün (~3 min).
- [x] 4. Code-Review der Kernlogik (ISR/Entprellung, Zeitberechnung, Überläufe, LoRa-Parsing,
      Web-Handler-Eingaben, Puffergrößen) → gefundene Fehler in allen betroffenen Varianten beheben
  - [x] 4a. Zeitberechnung Start↔Ziel/Split (siehe Entscheidung „Laufzeitkompensation“); alle 9 bauen
  - [x] 4b. Web-Handler und Warnungen:
    - `/duelnext`, `/stagnext`: Schrittzähler nicht begrenzt → erneutes Absenden (Zurück/Neu laden)
      schrieb hinter `duelRiders`/`stagRiders` (RTC-Speicher) → jetzt Weiterleitung zur Bestätigung
    - `/duelgo`, `/staggo` ohne vorherige Einrichtung → aktivierten Modus mit 0 Fahrern → abgefangen
    - `/settime`: nur plausible Unix-ms-Zeitstempel (≥ 1e12), sonst 400 (wie bereits bei `TSY`)
    - `platepin`/`btn2pin`: nur gültige GPIOs (`GPIO_IS_VALID_GPIO`) übernehmen
    - Namen: Steuerzeichen werden entfernt; Stag-Fahrer im `/state`-JSON escapen jetzt auch `\`
    - `fmtTime` mit `snprintf` (begrenzt auf 12 Byte; alle Aufrufer haben ≥ 12)
    - Warnungen: unbenutzte `isBest*` → `[[maybe_unused]]`, `%u` mit `uint32_t` gecastet
    - Verbleibend (bewusst, harmlos): 56× `-Wformat-truncation` (nur `snprintf`, schneidet sicher ab),
      24× `-Wattributes` (IRAM_ATTR an den von PlatformIO erzeugten .ino-Prototypen),
      18× `__VA_ARGS__`-Hinweis aus der .ino→.cpp-Umwandlung
    - Geprüft ohne Befund: Plattenen-ISR (erste Flanke, 64-bit-Zeitstempel mit Spinlock), TIM/SPL-Parsing
      (strtoul + Bereichsprüfung), Ringpuffer der Historie, HTML-Escaping in Seiten und Verlauf
- [x] 5. Gemeinsame, hardwarefreie Logik in eine geteilte Bibliothek (`lib/mtb_common`:
      Protokoll formatieren/parsen, Zeitformatierung, Kompensation) auslagern und von allen
      Varianten nutzen lassen
  - `lib/mtb_common/src/mtb_common.h` (header-only, namespace `mtb`, ohne Arduino-Abhängigkeit):
    `fmtTime`, `fmtDuration`, `voltageToPct`, `parseUsPayload` (TIM/SPL), `parseTsy`, `stxRiderName`,
    `compensatedMs`, `stagResultMs`, `rxReferenceUs`, `histPhys`
  - Alle 9 Varianten nutzen sie (Einbindung per `symlink://../lib/mtb_common` in `platformio.ini`);
    ersetzt ~200 duplizierte Zeilen. Alle 9 bauen, Warnungszahl unverändert (98).
  - `upBuf` in den Web-Seiten von 12 auf 14 Byte (passend zu `fmtDuration`)
- [ ] 6. Native Unit-Tests (`pio test -e native`, Unity) für die geteilte Bibliothek; in CI aufnehmen
- [ ] 7. Versionierung vereinheitlichen (eine zentrale Versionskonstante, Bootlog/OLED/Web-UI daraus)
- [ ] 8. Doku aktualisieren: README (Build mit PlatformIO für alle Varianten, CI-Badge, Versionen),
      Bedienungsanleitung abgleichen, CHANGELOG anlegen
- [ ] 9. Abschluss: alles gepusht, Wiki-Projektseite `projekt-mtb-track-timer` aktualisieren

## Entscheidungen
- 2026-10-22: Der leere lokale Ordner wird mit dem GitHub-Repo verbunden (`origin` = jyxbk/MTB-Track-Timer,
  Branch `main`); gearbeitet und gepusht wird direkt auf `main` (Repo des Nutzers, Pushen erlaubt).
- PLAN.md liegt im Repo-Root (getrennt von `KONZEPT_LUFT/PLAN.md`) und wird auf Deutsch geführt;
  Repo-Doku (README, Anleitung) bleibt wie vom Nutzer übersetzt auf Englisch.
- Bestehende Ordnerstruktur (ein Ordner je Variante, `.ino`-Dateien) bleibt erhalten, damit Arduino IDE
  weiter funktioniert; geteilter Code kommt als Header-Bibliothek dazu.
- Keine Releases/Tags ohne Rückfrage.
- LILYGO-Varianten nutzen Partitionstabelle `min_spiffs.csv` (App 1,9 MB mit OTA): Start_LILYGO war mit
  der Standardtabelle (1,25 MB) zu groß (106 %). Kein SPIFFS/LittleFS im Code → kein Verlust.
  Arduino IDE entspricht „Partition Scheme: Minimal SPIFFS“ (in README dokumentieren, Aufgabe 8).
- LILYGO-Board in PlatformIO: `ttgo-lora32-v21` (Pins sind im Code ohnehin explizit definiert).
- 2026-10-22 **Laufzeitkompensation** (Fehler behoben): Ziel/Split starten ihre Uhr erst beim Empfang
  von `STX`; gemessene Zeiten waren daher um die Funklaufzeit (+ Loop-Verzögerung) zu kurz, und die
  Kompensation wurde zusätzlich noch *abgezogen* statt addiert. Neu:
  Start misst Auslösung → STX-Sendeende (`stxLagUs`) und addiert sie zu `TIM`/`SPL`;
  Ziel/Split nehmen den Empfangszeitpunkt im LoRa-Interrupt (`rxAtUs`, Plausibilitätsprüfung < 1 s).
  `cfg_lora_comp_ms` ist nur noch eine *addierte* Zusatzkorrektur (Standard 0), neuer NVS-Schlüssel
  `loraadd` (alte, abgezogene Werte werden bewusst nicht übernommen). RTT-Empfehlung in der Web-UI entfernt.
  Protokoll unverändert → alte und neue Knoten bleiben kompatibel (alte Ziel-Firmware nur etwas ungenauer).
- Versetzter Start (Stag): Ziel wird beim Neu-Scharfschalten (Re-Arm-STX nach ACK) auf eine neue Referenz
  gesetzt; die Startseite rechnete danach nicht mehr korrekt um (Zeit zu kurz). Jetzt einheitlich:
  `t = Ziel-Messung + (Sendeende des letzten STX − Startzeit Fahrer k)`; Startzeit = ISR-Zeit der Platte.
- Geteilte Bibliothek: Arduino IDE findet `lib/mtb_common` nicht automatisch → einmalig in den
  Arduino-`libraries`-Ordner kopieren (README). Bewusst keine Kopie je Sketch-Ordner (eine Quelle).
  Hardwarenahe Teile (Radio, Display, Web-UI) bleiben vorerst je Variante – zu riskant ohne Hardwaretest.
- Build lokal: `export PLATFORMIO_CORE_DIR=$PWD/.pio-core; .venv/bin/pio run -d <Ordner>`
  (erster Build lädt Toolchain, ~5 min). `.venv/` und `.pio-core/` sind in `.gitignore`.

## Offene Punkte
- Hardware-Tests (Flashen, Funkstrecke, Sensoren) kann nur der Nutzer machen.
- Alter Remote-Branch `master` – löschen? (nur nach Rückfrage; vorerst unangetastet)
- Stag-Modus, Restrisiko: Startet ein weiterer Fahrer, während ein `TIM` des vorherigen noch unterwegs ist
  (oder per Retry kommt), wird mit der neuen Referenz umgerechnet. Lösung bräuchte eine Protokollerweiterung
  (z. B. Fahrer-Index in `TIM`) – nicht ohne Rücksprache.
- Hardware-Test der neuen Kompensation: Referenzmessung (z. B. Platte Start und Ziel nebeneinander,
  gleichzeitig auslösen → Ergebnis sollte ≈ 0 ms sein) durch den Nutzer.
- LUFT-Variante für LILYGO fehlt (README: „currently only Heltec“) – mögliche spätere Aufgabe.

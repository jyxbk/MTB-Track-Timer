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
- [ ] 2. Build-Umgebung: PlatformIO in `.venv`; `platformio.ini` für LILYGO- und LUFT-Ordner;
      Skript `tools/build_all.sh`, das alle 9 Varianten baut; Kompilierfehler/-warnungen beheben
  - [x] PlatformIO 6.2.0 in `.venv`, Core-Dir projektlokal `.pio-core` (`PLATFORMIO_CORE_DIR`)
  - [x] `MTB_Timer_Start` baut erfolgreich (espressif32 8.0.0, Arduino-Core 3.3.x; RAM 15.5 %, Flash 36.8 %)
  - [ ] Finish/Split (Heltec) bauen
  - [ ] `platformio.ini` für 3× LILYGO und 3× LUFT anlegen und bauen
  - [ ] `tools/build_all.sh`; Plattformversion pinnen
- [ ] 3. GitHub-Actions-Workflow: alle Varianten bei jedem Push bauen (Firmware als Artefakt)
- [ ] 4. Code-Review der Kernlogik (ISR/Entprellung, Zeitberechnung, Überläufe, LoRa-Parsing,
      Web-Handler-Eingaben, Puffergrößen) → gefundene Fehler in allen betroffenen Varianten beheben
- [ ] 5. Gemeinsame, hardwarefreie Logik in eine geteilte Bibliothek (`lib/mtb_common`:
      Protokoll formatieren/parsen, Zeitformatierung, Kompensation) auslagern und von allen
      Varianten nutzen lassen
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
- Build lokal: `export PLATFORMIO_CORE_DIR=$PWD/.pio-core; .venv/bin/pio run -d <Ordner>`
  (erster Build lädt Toolchain, ~5 min). `.venv/` und `.pio-core/` sind in `.gitignore`.

## Offene Punkte
- Hardware-Tests (Flashen, Funkstrecke, Sensoren) kann nur der Nutzer machen.
- Alter Remote-Branch `master` – löschen? (nur nach Rückfrage; vorerst unangetastet)
- LUFT-Variante für LILYGO fehlt (README: „currently only Heltec“) – mögliche spätere Aufgabe.
- Exaktes PlatformIO-Board für LILYGO T3 V1.6.1 (`ttgo-lora32-v21`) gegen Pinbelegung im Code prüfen.

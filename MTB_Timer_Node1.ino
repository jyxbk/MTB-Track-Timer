// ============================================================
//  NODE 1 – START-NODE  v2
//  MTB Downhill Zeitmessung
//  Heltec WiFi LoRa 32 V3  (ESP32-S3 + SX1262)
//
//  NEU:
//    - PRG-Taste (GPIO 0): 3 Sek. halten → Deep Sleep
//      Aufwachen: PRG-Taste kurz drücken
//    - WiFi Access Point "MTB-Timer-Start"
//      Webseite: http://192.168.4.1
//    - Zeitverlauf (letzte 20 Läufe) bleibt nach Deep Sleep erhalten
//
//  Bibliotheken: RadioLib >= 6.4, U8g2 >= 2.35
// ============================================================

#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_sleep.h"

// ── Pins ───────────────────────────────────────────────────
#define OLED_SDA   17
#define OLED_SCL   18
#define OLED_RST   21
#define LORA_SCK    9
#define LORA_MISO  11
#define LORA_MOSI  10
#define LORA_NSS    8
#define LORA_RST   12
#define LORA_DIO1  14
#define LORA_BUSY  13
#define PLATE_PIN   2
#define PRG_PIN     0   // Boot / PRG Taste
#define LED_PIN    35   // Onboard LED (aktiv HIGH)
#define VEXT_PIN   36   // OLED/Peripherie-Versorgung (LOW = AN)

// ── LoRa ───────────────────────────────────────────────────
#define LORA_FREQ  868.0f
#define LORA_BW    125.0f
#define LORA_SF        7
#define LORA_CR        5
#define LORA_PWR      14

// ── WiFi ───────────────────────────────────────────────────
#define AP_SSID  "MTB-Timer-Start"
#define AP_PASS  ""              // leer = offenes Netz

// ── Timing ─────────────────────────────────────────────────
#define DEBOUNCE_MS       500UL
#define RESULT_SHOW_MS   8000UL
#define RUN_TIMEOUT_MS   (5UL * 60UL * 1000UL)
#define DISP_REFRESH_MS   250UL
#define PING_INTERVAL_MS (30UL * 1000UL)
#define LONG_PRESS_MS    3000UL
#define MAX_HISTORY          20

// ── Hardware ───────────────────────────────────────────────
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,
  /* reset= */ U8X8_PIN_NONE,
  /* clock= */ OLED_SCL,
  /* data=  */ OLED_SDA);
WebServer server(80);

// ── Im RTC-Speicher: überleben Deep Sleep ──────────────────
RTC_DATA_ATTR unsigned long history[MAX_HISTORY];
RTC_DATA_ATTR uint8_t       historyCnt = 0;
RTC_DATA_ATTR unsigned long bestTimeMs = 0;

// ── Zustandsautomat ────────────────────────────────────────
enum State : uint8_t { IDLE, RUNNING, RESULT };
State appState = IDLE;

// ── Laufvariablen ──────────────────────────────────────────
unsigned long lastTimeMs   = 0;
unsigned long runStartAt   = 0;
unsigned long resultAt     = 0;
unsigned long lastTrigger  = 0;
unsigned long lastDispRefr = 0;
unsigned long lastPingAt   = 0;

// ── LoRa Verbindungsinfo ───────────────────────────────────
float         loraRssi          = 0.0f;
float         loraSnr           = 0.0f;
unsigned long loraLastContact   = 0;

// ── PRG-Taste ──────────────────────────────────────────────
bool          btnPrev   = HIGH;
unsigned long btnDownAt = 0;

volatile bool rxFlag = false;

// ── ISR ────────────────────────────────────────────────────
IRAM_ATTR void onLoRaRx() { rxFlag = true; }

// ── Hilfsfunktionen ────────────────────────────────────────
void fmtTime(unsigned long ms, char* out) {
  sprintf(out, "%02u:%02u.%03u",
    (unsigned)(ms / 60000),
    (unsigned)((ms % 60000) / 1000),
    (unsigned)(ms % 1000));
}

void addHistory(unsigned long t) {
  if (historyCnt < MAX_HISTORY) {
    history[historyCnt++] = t;
  } else {
    memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(unsigned long));
    history[MAX_HISTORY - 1] = t;
  }
}

// ── Display ────────────────────────────────────────────────
void drawDisplay(unsigned long liveMs = 0) {
  char lastBuf[12], bestBuf[12];
  if (lastTimeMs > 0) fmtTime(lastTimeMs, lastBuf); else strcpy(lastBuf, "--:--.---");
  if (bestTimeMs > 0) fmtTime(bestTimeMs, bestBuf); else strcpy(bestBuf, "--:--.---");

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "MTB TIMER  [START]");
  u8g2.drawHLine(0, 12, 128);

  u8g2.setFont(u8g2_font_7x13B_tf);
  char midBuf[12];
  if      (appState == RUNNING && liveMs > 0)  fmtTime(liveMs, midBuf);
  else if (appState == RESULT  && lastTimeMs > 0) fmtTime(lastTimeMs, midBuf);
  else if (appState == RUNNING) strcpy(midBuf, " LAEUFT!");
  else                          strcpy(midBuf, "  BEREIT");
  u8g2.drawStr(20, 28, midBuf);
  u8g2.drawHLine(0, 31, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,  43, "Last:");  u8g2.drawStr(40, 43, lastBuf);
  u8g2.drawStr(0,  57, "Best:");  u8g2.drawStr(40, 57, bestBuf);

  u8g2.sendBuffer();
}

void showSleepProgress(unsigned long heldMs) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(18, 20, "Ausschalten...");
  unsigned long elapsed = (heldMs > 1000) ? (heldMs - 1000) : 0;
  int w = (int)((elapsed * 116) / (LONG_PRESS_MS - 1000));
  if (w > 116) w = 116;
  u8g2.drawFrame(6, 30, 116, 14);
  if (w > 0) u8g2.drawBox(6, 30, w, 14);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(6, 56, "PRG loslassen = Abbrechen");
  u8g2.sendBuffer();
}

// ── LED-Steuerung (non-blocking) ───────────────────────────
void updateLED(unsigned long now) {
  static unsigned long lastToggle = 0;
  static bool          ledOn      = false;

  switch (appState) {
    case IDLE:
      // Herzschlag: 80 ms AN, 1920 ms AUS
      if (!ledOn  && (now - lastToggle) >= 1920) { ledOn = true;  digitalWrite(LED_PIN, HIGH); lastToggle = now; }
      else if (ledOn  && (now - lastToggle) >=   80) { ledOn = false; digitalWrite(LED_PIN, LOW);  lastToggle = now; }
      break;
    case RUNNING:
      // Schnelles Blinken: 150 ms an/aus
      if ((now - lastToggle) >= 150) { ledOn = !ledOn; digitalWrite(LED_PIN, ledOn); lastToggle = now; }
      break;
    case RESULT:
      // Dauerhaft AN
      digitalWrite(LED_PIN, HIGH); ledOn = true;
      break;
  }
}

// ── Deep Sleep ─────────────────────────────────────────────
void goToSleep() {
  digitalWrite(LED_PIN, LOW);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(22, 35, "Schlafe...");
  u8g2.sendBuffer();
  delay(800);
  u8g2.setPowerSave(1);
  vextOff();              // OLED-Strom aus vor Deep Sleep
  radio.sleep();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PRG_PIN, 0);
  delay(100);
  esp_deep_sleep_start();
}

// ── LoRa senden ────────────────────────────────────────────
void loRaSend(const char* msg) {
  radio.transmit(msg);
  radio.startReceive();
}

// ── WebServer: HTML aufbauen ───────────────────────────────
String buildHTML() {
  char buf[12];
  String html;
  html.reserve(4096);

  html = "<!DOCTYPE html><html lang='de'><head>"
         "<meta charset='UTF-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<meta http-equiv='refresh' content='5'>"
         "<title>MTB Timer – START</title>"
         "<style>"
         "body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:16px}"
         "h1{color:#f0a500;margin:0 0 16px;font-size:1.4em}"
         ".card{background:#1e1e1e;border-radius:8px;padding:14px;margin-bottom:12px}"
         ".lbl{color:#888;font-size:.82em;margin-bottom:3px}"
         ".val{font-size:1.25em;font-weight:bold}"
         ".best{color:#f0a500}"
         ".ok{color:#4caf50}.warn{color:#ff9800}.err{color:#f44336}"
         "table{width:100%;border-collapse:collapse}"
         "th{color:#888;font-size:.82em;text-align:left;padding:4px 0;border-bottom:1px solid #333}"
         "td{padding:6px 0;border-bottom:1px solid #222;font-size:.9em}"
         ".br td{color:#f0a500;font-weight:bold}"
         ".btn{display:inline-block;background:#333;color:#eee;padding:8px 16px;"
         "border-radius:6px;text-decoration:none;font-size:.9em;margin-top:10px}"
         ".state{display:inline-block;padding:3px 10px;border-radius:12px;"
         "font-size:.82em;font-weight:bold;background:#333}"
         ".s-idle{background:#1a3a1a;color:#4caf50}"
         ".s-run{background:#3a2a00;color:#ff9800}"
         ".s-res{background:#1a2a3a;color:#2196f3}"
         "</style></head><body>";

  html += "<h1>&#9201; MTB TIMER</h1>";

  // Node + Status
  const char* stateStr  = (appState == IDLE) ? "BEREIT" : (appState == RUNNING) ? "LAEUFT" : "ERGEBNIS";
  const char* stateClass = (appState == IDLE) ? "s-idle" : (appState == RUNNING) ? "s-run" : "s-res";
  html += "<div class='card'>";
  html += "<div class='lbl'>Node &nbsp; <span class='state " + String(stateClass) + "'>" + stateStr + "</span></div>";
  html += "<div class='val'>&#127937; START-NODE</div>";
  html += "</div>";

  // LoRa Verbindung
  unsigned long now = millis();
  unsigned long since = (loraLastContact > 0) ? (now - loraLastContact) / 1000 : 9999;
  String lClass = (loraLastContact == 0) ? "err" : (since < 60) ? "ok" : (since < 120) ? "warn" : "err";
  String lText  = (loraLastContact == 0) ? "Kein Kontakt" : (since < 60) ? "Verbunden" : (since < 120) ? "Schwach" : "Getrennt";

  html += "<div class='card'><div class='lbl'>LoRa Verbindung zum Ziel-Node</div>";
  html += "<div class='val " + lClass + "'>" + lText + "</div>";
  if (loraLastContact > 0) {
    html += "<div class='lbl' style='margin-top:8px'>RSSI: " + String(loraRssi, 1)
          + " dBm &nbsp;&nbsp; SNR: " + String(loraSnr, 1) + " dB</div>";
    html += "<div class='lbl'>Letzter Kontakt: vor " + String(since) + " Sek.</div>";
  }
  html += "</div>";

  // Zeiten
  if (lastTimeMs > 0) fmtTime(lastTimeMs, buf); else strcpy(buf, "--:--.---");
  String lastStr(buf);
  if (bestTimeMs > 0) fmtTime(bestTimeMs, buf); else strcpy(buf, "--:--.---");
  String bestStr(buf);

  html += "<div class='card'>";
  html += "<div class='lbl'>Letzte Zeit</div><div class='val'>" + lastStr + "</div>";
  html += "<div class='lbl' style='margin-top:10px'>Bestzeit</div><div class='val best'>" + bestStr + "</div>";
  html += "</div>";

  // Verlauf
  html += "<div class='card'><div class='lbl'>Verlauf &ndash; " + String(historyCnt) + " L&auml;ufe</div>";
  if (historyCnt == 0) {
    html += "<div style='color:#555;padding:8px 0'>Noch keine Zeiten gemessen.</div>";
  } else {
    html += "<table><tr><th>Lauf</th><th>Zeit</th><th></th></tr>";
    for (int i = (int)historyCnt - 1; i >= 0; i--) {
      fmtTime(history[i], buf);
      bool isBest = (bestTimeMs > 0 && history[i] == bestTimeMs);
      html += isBest ? "<tr class='br'>" : "<tr>";
      html += "<td>" + String(i + 1) + "</td>";
      html += "<td>" + String(buf) + "</td>";
      html += isBest ? "<td>&#127942; Bestzeit</td>" : "<td></td>";
      html += "</tr>";
    }
    html += "</table>";
  }
  html += "<a class='btn' href='/reset'>&#128465; Verlauf &amp; Bestzeit l&ouml;schen</a>";
  html += "</div>";

  html += "<div style='color:#333;font-size:.72em;margin-top:6px'>Auto-Refresh alle 5 Sek. &bull; "
          "WiFi: " AP_SSID " &bull; 192.168.4.1</div>";
  html += "</body></html>";
  return html;
}

void handleRoot()  { server.send(200, "text/html", buildHTML()); }
void handleReset() {
  bestTimeMs = 0;
  historyCnt = 0;
  memset(history, 0, sizeof(history));
  server.sendHeader("Location", "/");
  server.send(303);
}

// ── Vext: OLED-Stromversorgung ─────────────────────────────
void vextOn()  { pinMode(VEXT_PIN, OUTPUT); digitalWrite(VEXT_PIN, LOW);  }
void vextOff() { pinMode(VEXT_PIN, OUTPUT); digitalWrite(VEXT_PIN, HIGH); }

// ── Display-Init Hilfsfunktion (mehrfach aufrufbar) ───────
void initDisplay() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);  delay(50);
  digitalWrite(OLED_RST, HIGH); delay(50);
  Wire.begin(OLED_SDA, OLED_SCL);   // explizit – WiFi kann Wire überschreiben
  Wire.setClock(400000);
  u8g2.begin();
  u8g2.setContrast(255);
}

// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  // Pins früh setzen
  pinMode(PLATE_PIN, INPUT_PULLUP);
  pinMode(PRG_PIN,   INPUT_PULLUP);
  pinMode(LED_PIN,   OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // OLED-Versorgung einschalten (Vext LOW = AN)
  vextOn();
  delay(100);

  // RTC-Daten: beim ersten Kaltstart zurücksetzen
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) {
    bestTimeMs = 0;
    historyCnt = 0;
    memset(history, 0, sizeof(history));
  } else {
    if (historyCnt > 0) lastTimeMs = history[historyCnt - 1];
  }

  // Display: erste Init für "Initialisiere..." und LoRa-Fehlermeldung
  initDisplay();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(8, 35, "Initialisiere...");
  u8g2.sendBuffer();

  // SPI + LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int16_t rc = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                            RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_PWR, 8);
  if (rc != RADIOLIB_ERR_NONE) {
    initDisplay();   // Wire nach SPI wiederherstellen
    u8g2.clearBuffer();
    char err[24]; sprintf(err, "LoRa Fehler: %d", rc);
    u8g2.drawStr(0, 35, err);
    u8g2.sendBuffer();
    for (;;) delay(1000);
  }
  radio.setDio2AsRfSwitch(true);
  radio.setDio1Action(onLoRaRx);
  radio.startReceive();

  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, strlen(AP_PASS) > 0 ? AP_PASS : nullptr);
  server.on("/",      handleRoot);
  server.on("/reset", handleReset);
  server.begin();

  Serial.println("[Node1] Bereit.");
  Serial.print("[WiFi] SSID: "); Serial.println(AP_SSID);
  Serial.print("[WiFi] IP:   "); Serial.println(WiFi.softAPIP());

  // Display: zweite Init NACH WiFi – stellt Wire sicher wieder her
  initDisplay();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(10, 18, "MTB TIMER");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 32, "START NODE  v2");
  u8g2.drawHLine(0, 36, 128);
  u8g2.drawStr(0, 48, "WiFi: " AP_SSID);
  u8g2.drawStr(0, 60, "IP:   192.168.4.1");
  u8g2.sendBuffer();

  delay(2500);
  drawDisplay();
}

// ═══════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  server.handleClient();
  updateLED(now);

  // ── PRG-Taste: Long Press → Sleep ──────────────────────
  bool btnNow = digitalRead(PRG_PIN);
  if (btnNow == LOW && btnPrev == HIGH) {
    btnDownAt = now;                              // Flanke erkannt
  } else if (btnNow == LOW) {
    unsigned long held = now - btnDownAt;
    if (held >= LONG_PRESS_MS) {
      goToSleep();                                // Ausschalten
    } else if (held >= 1000) {
      showSleepProgress(held);                    // Fortschrittsbalken
    }
  } else if (btnNow == HIGH && btnPrev == LOW) {
    if ((now - btnDownAt) < LONG_PRESS_MS) drawDisplay(); // Abgebrochen → Anzeige zurück
  }
  btnPrev = btnNow;

  // ── Druckplatte Start ───────────────────────────────────
  if (digitalRead(PLATE_PIN) == LOW
      && (now - lastTrigger) > DEBOUNCE_MS
      && appState == IDLE) {
    lastTrigger = now;
    runStartAt  = now;
    appState    = RUNNING;
    loRaSend("STX");
    drawDisplay();
    Serial.println("[Node1] START – STX gesendet.");
  }

  // ── Live-Stoppuhr ───────────────────────────────────────
  if (appState == RUNNING && (now - lastDispRefr) >= DISP_REFRESH_MS) {
    lastDispRefr = now;
    drawDisplay(now - runStartAt);
  }

  // ── Lauf-Timeout ────────────────────────────────────────
  if (appState == RUNNING && (now - runStartAt) > RUN_TIMEOUT_MS) {
    appState = IDLE;
    drawDisplay();
    Serial.println("[Node1] Timeout.");
  }

  // ── Ergebnis-Anzeigedauer ───────────────────────────────
  if (appState == RESULT && (now - resultAt) > RESULT_SHOW_MS) {
    appState = IDLE;
    drawDisplay();
  }

  // ── Ping alle 30 Sek. (Verbindungstest) ─────────────────
  if (appState == IDLE && (now - lastPingAt) >= PING_INTERVAL_MS) {
    lastPingAt = now;
    loRaSend("PNG");
  }

  // ── LoRa-Empfang ────────────────────────────────────────
  if (rxFlag) {
    rxFlag = false;
    String msg;
    int16_t rc = radio.readData(msg);
    radio.startReceive();

    if (rc == RADIOLIB_ERR_NONE) {
      loraRssi        = radio.getRSSI();
      loraSnr         = radio.getSNR();
      loraLastContact = now;

      Serial.print("[Node1] RX: "); Serial.println(msg);

      if (msg.startsWith("TIM:") && appState == RUNNING) {
        char* ep;
        unsigned long t = strtoul(msg.c_str() + 4, &ep, 10);
        lastTimeMs = t;
        if (bestTimeMs == 0 || t < bestTimeMs) bestTimeMs = t;
        addHistory(t);
        appState = RESULT;
        resultAt = now;
        drawDisplay();
        loRaSend("ACK");
        char buf[12]; fmtTime(t, buf);
        Serial.print("[Node1] Ergebnis: "); Serial.println(buf);
      }
      // POG = Antwort auf PNG → Verbindungsinfo wurde oben bereits aktualisiert
    }
  }
}

// ============================================================
//  FINISH-NODE  v11
//  MTB Downhill Zeitmessung – Heltec WiFi LoRa 32 V3
//  Bibliotheken: RadioLib >= 6.6, U8g2 >= 2.35
// ============================================================
// ── Debug-Level ────────────────────────────────────────────
//  0 = aus | 1 = Info | 2 = Verbose
#define DEBUG_LEVEL  2

#if DEBUG_LEVEL >= 1
  #define DBG(tag, msg)    Serial.printf("[%8lu] %-9s %s\n",    millis(), tag, msg)
  #define DBGF(tag, ...)   do { Serial.printf("[%8lu] %-9s ", millis(), tag); \
                                Serial.printf(__VA_ARGS__); Serial.println(); } while(0)
#else
  #define DBG(tag, msg)
  #define DBGF(tag, ...)
#endif
#if DEBUG_LEVEL >= 2
  #define DBGV(tag, msg)   DBG(tag, msg)
  #define DBGVF(tag, ...)  DBGF(tag, __VA_ARGS__)
#else
  #define DBGV(tag, msg)
  #define DBGVF(tag, ...)
#endif

#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "esp_timer.h"

// Forward-Declarations (display.ino, web_html.ino, web_handlers.ino)
void drawDisplay(unsigned long liveMs = 0);
void showSleepProgress(unsigned long heldMs);
String buildHTML();
String buildState();
void handleState();
void handleRoot();
void handleCancel();
void handleSetTime();
void handleSettingsSave();
void handleSleep();
void handleRestart();
void handleManualPing();
void handleOtaPage();
void handleOtaUpload();
void handleOtaStream();
void handleExport();
void handleReset();
void saveSettings();
#include <Update.h>

// ── Hardware-Pins (fix) ────────────────────────────────────
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
#define PRG_PIN     0
#define LED_PIN    35
#define VEXT_PIN        36
#define BAT_ADC_PIN      1
#define VBAT_READ_CTRL  37

// ── LoRa (fix) ─────────────────────────────────────────────
#define LORA_FREQ  868.0f
#define LORA_BW    125.0f
#define LORA_SF        7
#define LORA_CR        5
#define LORA_PWR      14

// ── Feste Konstanten ───────────────────────────────────────
#define DISP_REFRESH_MS   100UL
#define LONG_PRESS_MS    3000UL
#define DOUBLE_PRESS_MS   400UL
#define BAT_READ_MS     10000UL
#define MAX_HISTORY          20
#define NAME_MAX_LEN         20
#define NUM_PAGES             4

// ── RSSI Schwellenwerte ────────────────────────────────────
#define RSSI_BAR5  (-65)
#define RSSI_BAR4  (-75)
#define RSSI_BAR3  (-85)
#define RSSI_BAR2  (-95)

#define PEER_TIMEOUT_S   90

// ── Konfigurierbare Werte (NVS) ────────────────────────────
uint32_t cfg_debounce_ms    = 500;
uint32_t cfg_result_show_ms = 8000;
uint32_t cfg_run_timeout_ms = 300000;
uint32_t cfg_lora_comp_ms   = 0;
uint32_t cfg_retry_interval = 2000;
uint32_t cfg_bat_mah        = 1100;
uint8_t  cfg_max_retries    = 3;
uint8_t  cfg_contrast       = 255;
uint8_t  cfg_plate_pin      = PLATE_PIN;
bool     cfg_plate_nc       = false;
char     cfg_ap_ssid[33]    = "MTB-Timer-Ziel";
char     cfg_ap_pass[64]    = "";
uint8_t  cfg_lora_pwr       = 14;
uint8_t  cfg_btn2_pin      = 255;
uint32_t cfg_page_auto_ms  = 0;

Preferences prefs;

// ── Hardware ───────────────────────────────────────────────
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,
  U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
WebServer server(80);

// ── RTC-Speicher ───────────────────────────────────────────
RTC_DATA_ATTR unsigned long history[MAX_HISTORY];
RTC_DATA_ATTR char          historyNames[MAX_HISTORY][NAME_MAX_LEN + 1];
RTC_DATA_ATTR int64_t       historyTimestamp[MAX_HISTORY];
RTC_DATA_ATTR uint8_t       historyCnt = 0;
RTC_DATA_ATTR uint8_t       histHead   = 0;
RTC_DATA_ATTR unsigned long bestTimeMs = 0;

// ── Zustandsautomat ────────────────────────────────────────
enum State : uint8_t { IDLE, ARMED, DONE };
State appState = IDLE;

// ── Laufvariablen ──────────────────────────────────────────
unsigned long lastTimeMs   = 0;
uint64_t      startRecvUs  = 0;
unsigned long doneAt       = 0;
unsigned long lastDispRefr = 0;
unsigned long lastRetryAt  = 0;
uint8_t       retryCnt     = 0;
bool          ackReceived  = false;
char          txBuf[24]    = "";
char          currentRiderName[NAME_MAX_LEN + 1] = "";
unsigned long lastBatReadAt = 0;
unsigned long lastDebugAt   = 0;
unsigned long lastPageAt    = 0;

// ── LED-Blink ──────────────────────────────────────────────
uint8_t       ledBlinkCount = 0;
unsigned long ledBlinkAt    = 0;

// ── Batterie ───────────────────────────────────────────────
float   batVoltage  = 0.0f;
uint8_t batPercent  = 0;

// ── Zeit-Sync (Browser) ────────────────────────────────────
int64_t       timeOffsetMs  = 0;
bool          timeIsSynced  = false;
unsigned long lastSyncAt    = 0;

// ── Display-Seiten ─────────────────────────────────────────
uint8_t currentPage = 0;

// ── Druckplatten-Interrupt ─────────────────────────────────
volatile bool     plateFlag      = false;
volatile uint64_t plateTriggerUs = 0;

IRAM_ATTR void onPlateTrigger() {
  if (!plateFlag) {
    plateTriggerUs = (uint64_t)esp_timer_get_time();
    plateFlag = true;
  }
}

// ── LoRa Verbindungsinfo + Statistik ──────────────────────
float         loraRssi        = 0.0f;
float         loraSnr         = 0.0f;
unsigned long loraLastContact = 0;
unsigned long lastHbAt        = 0;
uint32_t      loraTxCount     = 0;
uint32_t      loraTxFail      = 0;
uint32_t      loraRxCount     = 0;
bool          loraHasRx       = false;
float         loraRssiMin     = 0.0f;
float         loraRssiMax     = 0.0f;

// ── PRG-Taste ──────────────────────────────────────────────
bool          btnPrev        = HIGH;
unsigned long btnDownAt      = 0;
uint8_t       pendingPresses = 0;
unsigned long lastPressAt    = 0;

volatile bool rxFlag = false;
IRAM_ATTR void onLoRaRx() { rxFlag = true; }

volatile bool btn2Flag = false;
IRAM_ATTR void onBtn2() { btn2Flag = true; }

// ── Hilfsfunktionen ────────────────────────────────────────
void fmtTime(unsigned long ms, char* out) {
  sprintf(out, "%02u:%02u.%03u",
    (unsigned)(ms / 60000),
    (unsigned)((ms % 60000) / 1000),
    (unsigned)(ms % 1000));
}

void fmtUptime(char* out) {
  unsigned long s = millis() / 1000;
  unsigned long m = s / 60; s %= 60;
  unsigned long h = m / 60; m %= 60;
  sprintf(out, "%lu:%02lu:%02lu", h, m, s);
}

uint8_t voltageToPct(float v) {
  static const float vpts[] = {3.00f, 3.20f, 3.30f, 3.40f, 3.50f, 3.60f, 3.70f};
  static const float ppts[] = {  0,      2,     8,    20,    40,    60,   100};
  if (v >= 3.70f) return 100;
  if (v <  3.00f) return 0;
  for (int i = 1; i < 7; i++) {
    if (v < vpts[i]) {
      float frac = (v - vpts[i-1]) / (vpts[i] - vpts[i-1]);
      return (uint8_t)(ppts[i-1] + frac * (ppts[i] - ppts[i-1]));
    }
  }
  return 0;
}

int64_t nowUnixMs() {
  return timeIsSynced ? (int64_t)millis() + timeOffsetMs : 0;
}

const char* rssiStatus(float rssi) {
  if (rssi >= RSSI_BAR5) return "GUT";
  if (rssi >= RSSI_BAR3) return "MITTEL";
  return "SCHWACH";
}

static String htmlEsc(const char* s) {
  String r(s);
  r.replace("&", "&amp;"); r.replace("<", "&lt;");
  r.replace(">", "&gt;");  r.replace("\"", "&quot;");
  return r;
}

static inline uint8_t histPhys(uint8_t i) {
  return (uint8_t)((histHead + MAX_HISTORY - historyCnt + i) % MAX_HISTORY);
}

void addHistory(unsigned long t, const char* name) {
  uint8_t idx = histHead;
  history[idx]          = t;
  historyTimestamp[idx] = nowUnixMs();
  strncpy(historyNames[idx], name ? name : "", NAME_MAX_LEN);
  historyNames[idx][NAME_MAX_LEN] = '\0';
  if (historyCnt < MAX_HISTORY) historyCnt++;
  histHead = (histHead + 1) % MAX_HISTORY;
}

void vextOn()  { pinMode(VEXT_PIN, OUTPUT); digitalWrite(VEXT_PIN, LOW);  }
void vextOff() { pinMode(VEXT_PIN, OUTPUT); digitalWrite(VEXT_PIN, HIGH); }

// ── Zusatztaste / Auto-Page ────────────────────────────────
void advancePage() {
  currentPage = (currentPage + 1) % NUM_PAGES;
  lastPageAt  = millis();
  drawDisplay();
}

void setupBtn2() {
  if (cfg_btn2_pin < 40) {
    pinMode(cfg_btn2_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(cfg_btn2_pin), onBtn2, FALLING);
    DBGF("BTN2", "Zusatztaste auf GPIO%u", cfg_btn2_pin);
  }
}

// ── LED (non-blocking) ─────────────────────────────────────
void updateLED(unsigned long now) {
  static unsigned long lastToggle = 0;
  static bool          ledOn      = false;
  if (ledBlinkCount > 0) {
    if (now - ledBlinkAt >= 50) {
      ledBlinkAt = now;
      ledBlinkCount--;
      ledOn = (ledBlinkCount % 2 != 0);
      digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    }
    return;
  }
  switch (appState) {
    case IDLE:
      if (!ledOn  && (now - lastToggle) >= 1920) { ledOn = true;  digitalWrite(LED_PIN, HIGH); lastToggle = now; }
      else if (ledOn && (now - lastToggle) >= 80) { ledOn = false; digitalWrite(LED_PIN, LOW);  lastToggle = now; }
      break;
    case ARMED:
      if ((now - lastToggle) >= 150) { ledOn = !ledOn; digitalWrite(LED_PIN, ledOn); lastToggle = now; }
      break;
    case DONE:
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
  vextOff();
  radio.sleep();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PRG_PIN, 0);
  delay(100);
  esp_deep_sleep_start();
}

// ── LoRa senden ────────────────────────────────────────────
void loRaSend(const char* msg) {
  int16_t rc = radio.transmit(msg);
  if (rc == RADIOLIB_ERR_NONE) {
    loraTxCount++;
    DBGVF("LORA-TX", "\"%s\"  OK  (#%lu)", msg, loraTxCount);
  } else {
    loraTxFail++;
    DBGF("LORA-TX",  "\"%s\"  FEHLER rc=%d  (#fail=%lu)", msg, rc, loraTxFail);
  }
  radio.startReceive();
}

// ── Cancel Run ─────────────────────────────────────────────
void cancelRun() {
  appState = IDLE;
  plateFlag = false;
  currentRiderName[0] = '\0';
  currentPage = 0;
  drawDisplay();
  DBG("STATE",  "IDLE ← Lauf abgebrochen");
}

// ── Display-Init ───────────────────────────────────────────
void initDisplay() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);  delay(50);
  digitalWrite(OLED_RST, HIGH); delay(50);
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  u8g2.begin();
  u8g2.setContrast(cfg_contrast);
}

// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  prefs.begin("mtb-cfg2", true);
  cfg_debounce_ms    = prefs.getUInt("debounce",   500);
  cfg_result_show_ms = prefs.getUInt("result",     8000);
  cfg_run_timeout_ms = prefs.getUInt("timeout",    300000);
  cfg_lora_comp_ms   = prefs.getUInt("loracomp",   0);
  cfg_retry_interval = prefs.getUInt("retryiv",    2000);
  cfg_bat_mah        = prefs.getUInt("batmah",     1100);
  cfg_max_retries    = prefs.getUChar("maxretry",  3);
  cfg_contrast       = prefs.getUChar("contrast",  255);
  cfg_plate_pin      = prefs.getUChar("platepin",  PLATE_PIN);
  cfg_plate_nc       = prefs.getBool("platenc",    false);
  cfg_lora_pwr       = prefs.getUChar("lorapwr",   14);
  cfg_btn2_pin       = prefs.getUChar("btn2pin",   255);
  cfg_page_auto_ms   = prefs.getUInt("autopage",   0);
  prefs.getString("apssid", cfg_ap_ssid, sizeof(cfg_ap_ssid));
  prefs.getString("appass", cfg_ap_pass, sizeof(cfg_ap_pass));
  prefs.end();
  if (cfg_bat_mah == 2000) cfg_bat_mah = 1100;
  if (strlen(cfg_ap_ssid) == 0) strcpy(cfg_ap_ssid, "MTB-Timer-Ziel");

  pinMode(cfg_plate_pin, INPUT_PULLUP);
  pinMode(PRG_PIN,        INPUT_PULLUP);
  pinMode(LED_PIN,        OUTPUT);
  digitalWrite(LED_PIN, LOW);

  vextOn();
  delay(100);

  pinMode(VBAT_READ_CTRL, OUTPUT);
  digitalWrite(VBAT_READ_CTRL, HIGH);

  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) {
    bestTimeMs = 0; historyCnt = 0; histHead = 0;
    memset(history,           0, sizeof(history));
    memset(historyNames,      0, sizeof(historyNames));
    memset(historyTimestamp,  0, sizeof(historyTimestamp));
  } else {
    if (historyCnt > 0) lastTimeMs = history[histPhys(historyCnt - 1)];
  }

  initDisplay();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(8, 35, "Initialisiere...");
  u8g2.sendBuffer();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int16_t rc = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                            RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_PWR, 8);
  if (rc != RADIOLIB_ERR_NONE) {
    initDisplay();
    u8g2.clearBuffer();
    char err[24]; sprintf(err, "LoRa Fehler: %d", rc);
    u8g2.drawStr(0, 35, err);
    u8g2.sendBuffer();
    delay(5000); ESP.restart();
  }
  radio.setDio2AsRfSwitch(true);
  radio.setOutputPower(cfg_lora_pwr);
  radio.setDio1Action(onLoRaRx);
  radio.startReceive();

  attachInterrupt(digitalPinToInterrupt(cfg_plate_pin), onPlateTrigger,
                  cfg_plate_nc ? RISING : FALLING);
  setupBtn2();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(cfg_ap_ssid, strlen(cfg_ap_pass) > 0 ? cfg_ap_pass : nullptr);
  server.on("/",              handleRoot);
  server.on("/state",         handleState);
  server.on("/export",        handleExport);
  server.on("/reset",         handleReset);
  server.on("/ping",          handleManualPing);
  server.on("/cancel",        handleCancel);
  server.on("/settime",       handleSetTime);
  server.on("/settings/save", HTTP_POST, handleSettingsSave);
  server.on("/update",        HTTP_GET,  handleOtaPage);
  server.on("/update",        HTTP_POST, handleOtaUpload, handleOtaStream);
  server.on("/sleep",         handleSleep);
  server.on("/restart",       handleRestart);
  server.begin();

  DBG("BOOT",  "FINISH-NODE v11 bereit");
  DBGF("WIFI",  "SSID: %s  IP: %s", cfg_ap_ssid, WiFi.softAPIP().toString().c_str());
  DBGF("CFG",   "Entprell=%ums  Timeout=%ums  Comp=%ums  Retry=%ums/#%u",
       cfg_debounce_ms, cfg_run_timeout_ms, cfg_lora_comp_ms,
       cfg_retry_interval, cfg_max_retries);
  DBGF("CFG",   "Bat=%umAh  Kontrast=%u  Pin=%u  NC=%s",
       cfg_bat_mah, cfg_contrast, cfg_plate_pin, cfg_plate_nc ? "ja" : "nein");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(10, 18, "MTB TIMER");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 32, "FINISH NODE v9");
  u8g2.drawHLine(0, 36, 128);
  char wln[24]; snprintf(wln, sizeof(wln), "WiFi: %.16s", cfg_ap_ssid);
  u8g2.drawStr(0, 48, wln);
  u8g2.drawStr(0, 60, "IP:   192.168.4.1");
  u8g2.sendBuffer();

  delay(2500);
  lastPageAt = millis();
  drawDisplay();
}

// ═══════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  server.handleClient();
  updateLED(now);

  // ── Batterie-Messung ───────────────────────────────────
  if (now - lastBatReadAt >= BAT_READ_MS) {
    lastBatReadAt = now;
    uint32_t adcSum = 0;
    for (int _i = 0; _i < 8; _i++) adcSum += analogReadMilliVolts(BAT_ADC_PIN);
    batVoltage = (adcSum / 8.0f) * 4.9f / 1000.0f;
    batPercent = voltageToPct(batVoltage);
    DBGVF("BAT",    "%.2f V  %u%%  (~%u mAh)",
          batVoltage, batPercent, (unsigned)(cfg_bat_mah * batPercent / 100));
    if (batPercent <= 5)       DBG("BAT",  "*** AKKU KRITISCH ***");
    else if (batPercent <= 15) DBG("BAT",  "Warnung: Akku niedrig");
  }

  // ── PRG-Taste ──────────────────────────────────────────
  bool btnNow = digitalRead(PRG_PIN);
  if (btnNow == LOW && btnPrev == HIGH) {
    btnDownAt = now;
  } else if (btnNow == LOW) {
    unsigned long held = now - btnDownAt;
    if (held >= LONG_PRESS_MS)  goToSleep();
    else if (held >= 1000)      showSleepProgress(held);
  } else if (btnNow == HIGH && btnPrev == LOW) {
    if ((now - btnDownAt) < LONG_PRESS_MS) {
      pendingPresses++;
      lastPressAt = now;
    }
  }
  btnPrev = btnNow;

  if (pendingPresses > 0 && (now - lastPressAt) > DOUBLE_PRESS_MS) {
    if (pendingPresses >= 2 && appState == ARMED) {
      cancelRun();
    } else if (pendingPresses == 1) {
      if (appState == DONE) {
        appState = IDLE;
        currentRiderName[0] = '\0';
        drawDisplay();
      } else {
        advancePage();
      }
    }
    pendingPresses = 0;
  }

  // ── Zusatztaste ────────────────────────────────────────
  if (btn2Flag) {
    btn2Flag = false;
    static unsigned long btn2LastMs = 0;
    if (now - btn2LastMs > 300) {
      btn2LastMs = now;
      if (appState == DONE) { appState = IDLE; currentRiderName[0] = '\0'; drawDisplay(); }
      else advancePage();
      DBGV("BTN2", "Seite weitergeschaltet");
    }
  }

  // ── Auto-Page ──────────────────────────────────────────
  if (cfg_page_auto_ms > 0
      && appState != ARMED
      && (now - lastPageAt) >= cfg_page_auto_ms) {
    advancePage();
  }

  // ── LoRa-Empfang ───────────────────────────────────────
  if (rxFlag) {
    rxFlag = false;
    String msg;
    int16_t rc = radio.readData(msg);
    radio.startReceive();

    if (rc == RADIOLIB_ERR_NONE) {
      loraRssi        = radio.getRSSI();
      loraSnr         = radio.getSNR();
      loraLastContact = now;
      loraRxCount++;
      if (!loraHasRx || loraRssi < loraRssiMin) loraRssiMin = loraRssi;
      if (!loraHasRx || loraRssi > loraRssiMax) loraRssiMax = loraRssi;
      loraHasRx = true;

      DBGVF("LORA-RX", "\"%s\"  RSSI=%d dBm  SNR=%.1f dB", msg.c_str(), (int)loraRssi, loraSnr);

      if (msg.startsWith("STX")) {
        if (msg.length() > 4 && msg[3] == ':') {
          strncpy(currentRiderName, msg.c_str() + 4, NAME_MAX_LEN);
          currentRiderName[NAME_MAX_LEN] = '\0';
        } else {
          currentRiderName[0] = '\0';
        }
        startRecvUs  = (uint64_t)esp_timer_get_time();
        plateFlag    = false;
        retryCnt     = 0;
        ackReceived  = false;
        appState     = ARMED;
        drawDisplay();
        DBGF("STATE",  "ARMED ← STX  Fahrer: %s",
             strlen(currentRiderName) > 0 ? currentRiderName : "(kein)");
      }
      if (msg == "ACK" && appState == DONE) {
        ackReceived = true;
        DBG("LORA-RX", "ACK – Messung bestätigt");
      }
      if (msg == "PNG") {
        loRaSend("POG");
        DBGV("LORA-RX", "PNG → POG gesendet");
      }
      if (msg.startsWith("TSY:")) {
        int64_t rxTs = (int64_t)atoll(msg.c_str() + 4);
        if (rxTs > 1000000000000LL) {
          timeOffsetMs = rxTs - (int64_t)millis();
          timeIsSynced = true;
          lastSyncAt   = millis();
          DBGF("SYNC",   "TSY – Zeit synchronisiert (Offset=%lld ms)", timeOffsetMs);
        }
      }
      if (msg == "CAN" && appState == ARMED) {
        cancelRun();
        DBG("STATE",  "IDLE ← CAN empfangen");
      }
    }
  }

  // ── Druckplatte Ziel (Interrupt) ───────────────────────
  if (plateFlag && appState == ARMED) {
    plateFlag = false;
    if (plateTriggerUs < startRecvUs) return;
    uint64_t elapsedUs = plateTriggerUs - startRecvUs;

    if (elapsedUs > (cfg_debounce_ms * 1000ULL)) {
      // Rohe µs an Start senden – Kompensation erfolgt dort
      uint32_t elapsedRaw = (uint32_t)elapsedUs;
      unsigned long elapsedMs = (unsigned long)((elapsedUs + 500ULL) / 1000ULL);

      lastTimeMs    = elapsedMs;
      if (bestTimeMs == 0 || elapsedMs < bestTimeMs) bestTimeMs = elapsedMs;
      addHistory(elapsedMs, currentRiderName);
      ledBlinkCount = 6; ledBlinkAt = now;
      sprintf(txBuf, "TIM:%lu", (unsigned long)elapsedRaw);  // µs-Wert
      loRaSend(txBuf);
      retryCnt    = 0;
      ackReceived = false;
      lastRetryAt = now;
      doneAt      = now;
      appState    = DONE;
      drawDisplay();
      char tbuf[12]; fmtTime(elapsedMs, tbuf);
      bool isBestF = (bestTimeMs > 0 && elapsedMs == bestTimeMs);
      DBGF("RESULT",  "%s  Fahrer: %s%s  (%luus roh)",
           tbuf,
           strlen(currentRiderName) > 0 ? currentRiderName : "(kein)",
           isBestF ? "  *** BESTZEIT ***" : "",
           (unsigned long)elapsedRaw);
    }
  }

  // ── Anzeige-Refresh ────────────────────────────────────
  if (appState == ARMED) {
    if ((now - lastDispRefr) >= DISP_REFRESH_MS) {
      lastDispRefr = now;
      uint64_t curUs = (uint64_t)esp_timer_get_time();
      drawDisplay((unsigned long)((curUs - startRecvUs) / 1000));
    }
  } else if ((now - lastDispRefr) >= 1000UL) {
    lastDispRefr = now;
    drawDisplay();
  }

  // ── Lauf-Timeout ───────────────────────────────────────
  if (appState == ARMED) {
    uint64_t curUs = (uint64_t)esp_timer_get_time();
    if ((curUs - startRecvUs) / 1000 > cfg_run_timeout_ms) {
      appState = IDLE;
      currentRiderName[0] = '\0';
      drawDisplay();
      DBGF("STATE",  "IDLE ← Timeout nach %lus", cfg_run_timeout_ms / 1000);
    }
  }

  // ── Heartbeat ──────────────────────────────────────────
  if (now - lastHbAt >= 25000) {
    lastHbAt = now;
    if (appState == IDLE) {
      loRaSend("HBT");
      DBGV("HBT",    "HBT gesendet");
    }
  }

  // ── Periodischer Status-Dump ───────────────────────────
#if DEBUG_LEVEL >= 2
  if (now - lastDebugAt >= 30000UL) {
    lastDebugAt = now;
    const char* stStr2 = appState==IDLE?"IDLE":appState==ARMED?"ARMED":"DONE";
    char up[14]; fmtUptime(up);
    DBGF("STATUS",  "State=%-6s  Bat=%u%%/%.2fV  TX=%lu/%lu  RX=%lu  Up=%s",
         stStr2, batPercent, batVoltage, loraTxCount, loraTxFail, loraRxCount, up);
  }
#endif

  // ── Ergebnis-Retries ───────────────────────────────────
  if (appState == DONE && !ackReceived && retryCnt < cfg_max_retries && (now - lastRetryAt) >= cfg_retry_interval) {
    loRaSend(txBuf);
    lastRetryAt = now;
    retryCnt++;
    DBGF("RETRY",  "#%u/%u  TIM neu gesendet", retryCnt, cfg_max_retries);
  }

  // ── Ergebnis-Anzeigedauer ──────────────────────────────
  if (appState == DONE && (now - doneAt) >= cfg_result_show_ms) {
    appState = IDLE;
    currentRiderName[0] = '\0';
    drawDisplay();
  }
}

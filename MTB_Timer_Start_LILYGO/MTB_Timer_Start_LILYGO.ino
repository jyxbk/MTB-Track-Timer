// ============================================================
//  START-NODE  v24  –  LILYGO TTGO T3 V1.6.1 (ESP32-PICO-D4 + SX1276)
//  MTB Downhill Zeitmessung
//
//  Arduino IDE Board: "TTGO LoRa32-OLED"
//  Bibliotheken: RadioLib >= 6.6, U8g2 >= 2.35, SD (Arduino Core)
// ============================================================
// ── Debug-Level ────────────────────────────────────────────
//  0 = aus  |  1 = Info  |  2 = Verbose
#define DEBUG_LEVEL  0
#define TZ_OFFSET_SEC 7200  // Zeitzone: CEST=7200, CET=3600

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
#include <SD.h>
#include "esp_sleep.h"
#include "esp_timer.h"

// ESP32 Arduino Core 3.x Kompatibilität
#ifndef HSPI
#define HSPI 2
#endif
#ifndef VSPI
#define VSPI 3
#endif

// ── Hardware-Pins LILYGO T3 V1.6.1 ───────────────────────
#define OLED_SDA   21
#define OLED_SCL   22
// GPIO16 = PSRAM_CS auf ESP32-PICO-D4 → NICHT verwenden!

#define LORA_SCK    5   // SX1276 (HSPI)
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_NSS   18
#define LORA_RST   23
#define LORA_DIO0  26
#define LORA_DIO1  33

#define SD_CS      13   // SD-Karte (VSPI)
#define SD_SCK     14
#define SD_MISO     2
#define SD_MOSI    15

// ⚠ GPIO2 = SD_MISO → PLATE_PIN nicht auf GPIO2 lassen!
// Standard GPIO4; über Web-UI auf freien Pin ändern
#define PLATE_PIN   4
#define PRG_PIN     0
#define LED_PIN    25
#define BAT_ADC_PIN 35

// ── LoRa (identisch mit Heltec → Kompatibilität) ──────────
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
#define DUEL_MAX_RIDERS      10
#define NUM_PAGES             5

// ── RSSI Schwellenwerte ────────────────────────────────────
#define RSSI_BAR5  (-65)
#define RSSI_BAR4  (-75)
#define RSSI_BAR3  (-85)
#define RSSI_BAR2  (-95)

// ── Konfigurierbare Werte (NVS) ────────────────────────────
uint32_t cfg_debounce_ms    = 500;
uint32_t cfg_result_show_ms = 8000;
uint32_t cfg_run_timeout_ms = 300000;
uint32_t cfg_ping_ms        = 30000;
uint32_t cfg_lora_comp_ms   = 0;
uint32_t cfg_bat_mah        = 1100;
uint8_t  cfg_contrast       = 255;
uint8_t  cfg_plate_pin      = PLATE_PIN;
bool     cfg_plate_nc       = false;
char     cfg_ap_ssid[33]    = "MTB-Start-LILYGO";
char     cfg_ap_pass[64]    = "";
// Neue Einstellungen: Zusatztaste + Auto-Seitenumschaltung
uint8_t  cfg_btn2_pin       = 255;   // 255 = deaktiviert
uint32_t cfg_page_auto_ms   = 0;     // 0 = deaktiviert (in ms)
uint8_t  cfg_lora_pwr       = 14;    // Sendeleistung dBm (2–20)
uint8_t  cfg_stag_offset_s  = 30;   // Versatz-Offset Sekunden (5–255)
// ── SD-Tracking ─────────────────────────────────────────────
uint8_t  cfg_track_id       = 0;     // Aktive Strecken-ID (0=keine)
uint8_t  cfg_rider_id       = 0;     // Aktiver Fahrer-ID (0=kein)
uint8_t  cfg_condition      = 0;     // 0=–,1=☀,2=💧,3=🍂,4=❄
uint8_t  cfg_session_num    = 1;     // Session-Nr. am heutigen Tag
uint32_t cfg_session_day    = 0;     // Datum der aktuellen Session (Unix-Tage)
char     activeTrackName[21] = "";
char     activeRiderName[21] = "";

// ── SD-Strukturen (müssen vor Forward-Declarations stehen) ──
#define MAX_TRACKS  10
#define MAX_RIDERS  10

struct Track {
  uint8_t       id;
  char          name[21];
  char          info[41];
  unsigned long best_ms;
};

struct Rider {
  uint8_t       id;
  char          name[21];
  unsigned long bests[MAX_TRACKS];
};

Track   trackList[MAX_TRACKS];
uint8_t trackCount = 0;
Rider   riderList[MAX_RIDERS];
uint8_t riderCount = 0;

Preferences prefs;

// ── Hardware: HSPI für LoRa, VSPI für SD ──────────────────
SPIClass LoRaSPI(HSPI);
SPIClass SDSPI(VSPI);
SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1, LoRaSPI);
// RST = U8X8_PIN_NONE: kein Hardware-Reset (GPIO16 gesperrt)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,
  U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
WebServer server(80);

// ── SD-Karte ───────────────────────────────────────────────
bool sdPresent = false;

// ── RTC-Speicher ───────────────────────────────────────────
RTC_DATA_ATTR unsigned long history[MAX_HISTORY];
RTC_DATA_ATTR char          historyNames[MAX_HISTORY][NAME_MAX_LEN + 1];
RTC_DATA_ATTR int64_t       historyTimestamp[MAX_HISTORY];
RTC_DATA_ATTR uint8_t       historyCnt = 0;
RTC_DATA_ATTR uint8_t       histHead   = 0;
RTC_DATA_ATTR unsigned long bestTimeMs = 0;

// ── Split-Zeit ─────────────────────────────────────────────
#define SPLIT_SHOW_MS 5000UL
unsigned long splitTimeMs = 0;
unsigned long splitRxAt   = 0;

// ── Zustandsautomat ────────────────────────────────────────
enum State : uint8_t { IDLE, RUNNING, RESULT, LAP_IDLE, LAP_RUNNING };
State appState = IDLE;

// ── Laufvariablen ──────────────────────────────────────────
unsigned long lastTimeMs    = 0;
unsigned long runStartAt    = 0;
unsigned long resultAt      = 0;
unsigned long lastDispRefr  = 0;
unsigned long lastPageAt    = 0;   // für Auto-Seitenumschaltung
unsigned long lastPingAt    = 0;
unsigned long lastRttMs     = 0;
unsigned long lastBatReadAt = 0;
unsigned long lastDebugAt   = 0;
unsigned long plateLastMs   = 0;

// ── LED ────────────────────────────────────────────────────
uint8_t       ledBlinkCount = 0;
unsigned long ledBlinkAt    = 0;

// ── Batterie ───────────────────────────────────────────────
float   batVoltage  = 0.0f;
uint8_t batPercent  = 0;

// ── Zeit-Sync ──────────────────────────────────────────────
int64_t       timeOffsetMs  = 0;
bool          timeIsSynced  = false;
unsigned long lastSyncAt    = 0;

// ── Display ────────────────────────────────────────────────
uint8_t currentPage = 0;

// ── Duell-Modus (RTC) ──────────────────────────────────────
RTC_DATA_ATTR bool    duelMode     = false;
RTC_DATA_ATTR bool    duelDone     = false;
RTC_DATA_ATTR char    duelRiders[DUEL_MAX_RIDERS][NAME_MAX_LEN + 1];
RTC_DATA_ATTR uint8_t duelCount    = 0;
RTC_DATA_ATTR uint8_t duelCurrent  = 0;
RTC_DATA_ATTR uint8_t duelStartIdx = 0;
uint8_t duelSetupCount = 0;
uint8_t duelSetupStep  = 0;

// ── Versetzter Start-Modus (RTC) ───────────────────────────
RTC_DATA_ATTR bool    stagMode     = false;
RTC_DATA_ATTR bool    stagDone     = false;
RTC_DATA_ATTR uint8_t stagCount    = 0;
RTC_DATA_ATTR uint8_t stagStarted  = 0;
RTC_DATA_ATTR uint8_t stagFinished = 0;
RTC_DATA_ATTR char          stagRiders[DUEL_MAX_RIDERS][NAME_MAX_LEN + 1];
RTC_DATA_ATTR unsigned long stagStartMs[DUEL_MAX_RIDERS];
RTC_DATA_ATTR unsigned long stagTimes[DUEL_MAX_RIDERS];
RTC_DATA_ATTR uint16_t      stagFinishedMask;
unsigned long stagLastStartMs = 0;
uint8_t       stagArmedFor    = 0xFF;
unsigned long stagRearmAt     = 0;
uint8_t       stagRearmIdx    = 0xFF;
uint8_t stagSetupCount = 0;
uint8_t stagSetupStep  = 0;

// ── Runden-Modus (RTC) ─────────────────────────────────────
RTC_DATA_ATTR bool          lapMode       = false;
RTC_DATA_ATTR uint8_t       lapRoundNum   = 0;
RTC_DATA_ATTR unsigned long lapRoundStart = 0;
RTC_DATA_ATTR unsigned long lapBestMs     = 0;
RTC_DATA_ATTR unsigned long lapLastMs     = 0;

// ── Druckplatten-Interrupt ─────────────────────────────────
volatile bool     plateFlag      = false;
volatile uint64_t plateTriggerUs = 0;
static portMUX_TYPE plateMux = portMUX_INITIALIZER_UNLOCKED;

IRAM_ATTR void onPlate() {
  if (!plateFlag) {
    portENTER_CRITICAL_ISR(&plateMux);
    plateTriggerUs = (uint64_t)esp_timer_get_time();
    portEXIT_CRITICAL_ISR(&plateMux);
    plateFlag = true;
  }
}
static inline uint64_t readPlateTrigger() {
  portENTER_CRITICAL(&plateMux);
  uint64_t v = plateTriggerUs;
  portEXIT_CRITICAL(&plateMux);
  return v;
}

// ── Zusatztaste (btn2) ─────────────────────────────────────
volatile bool     btn2Flag  = false;
IRAM_ATTR void onBtn2() { btn2Flag = true; }

// ── LoRa Verbindungsinfo ───────────────────────────────────
float         loraRssi          = 0.0f;
float         loraSnr           = 0.0f;
unsigned long loraLastContact   = 0;
unsigned long finishLastContact = 0;
unsigned long splitLastContact  = 0;
uint32_t      loraTxCount       = 0;
uint32_t      loraTxFail        = 0;
uint32_t      loraRxCount       = 0;
bool          loraHasRx         = false;
float         loraRssiMin       = 0.0f;
float         loraRssiMax       = 0.0f;

// ── PRG-Taste ──────────────────────────────────────────────
bool          btnPrev        = HIGH;
unsigned long btnDownAt      = 0;
uint8_t       pendingPresses = 0;
unsigned long lastPressAt    = 0;

volatile bool rxFlag = false;
IRAM_ATTR void onLoRaRx() { rxFlag = true; }

// ── Forward-Declarations ───────────────────────────────────
void drawDisplay(unsigned long liveMs = 0);
void showSleepProgress(unsigned long heldMs);
String buildState();
void handleState();
void handleRoot();
void handleCancel();
void handleSetTime();
void handleSettingsSave();
void handleSleep();
void handleRestart();
void handleManualPing();
void handleExport();
void handleReset();
void handleDuelPage();
void handleDuelCount();
void handleDuelName();
void handleDuelNext();
void handleDuelConfirm();
void handleDuelGo();
void handleDuelExit();
void handleDuelSkip();
void handleStagPage();
void handleStagCount();
void handleStagName();
void handleStagNext();
void handleStagConfirm();
void handleStagGo();
void handleStagExit();
void handleLapStart();
void handleLapStop();
void handleLapReset();
void saveSettings();
void sdInit();
void sdAppendEntry(unsigned long ms, const char* name, int64_t ts);
String sdReadCsv();
void sdClear();
void sdClearAll();
void sdUpdateBest(unsigned long ms);
bool sdEditLastEntry(int64_t ts, const char* newName);
bool sdLoadTracks();  bool sdSaveTracks();
bool sdLoadRiders();  bool sdSaveRiders();
Track* sdFindTrack(uint8_t id);
Rider* sdFindRider(uint8_t id);
String sdListSessions();
String sdReadSession(const char* f, uint8_t page);
String sdGetStats(uint8_t trackId);
String sdGetBestlist(uint8_t trackId);
void handleSdFormat();
void handleTracks();     void handleTrackSave();
void handleTrackDel();   void handleTrackSet();
void handleRiders();     void handleRiderSave();
void handleRiderDel();   void handleRiderSet();
void handleCondition();  void handleEditName();
void handleSessions();   void handleSessionData();
void handleSdStats();    void handleBestlist();
void handleSdClearAll();

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

String htmlEsc(const char* s) {
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
  else { if (duelStartIdx == histHead) duelStartIdx = (duelStartIdx + 1) % MAX_HISTORY; }
  histHead = (histHead + 1) % MAX_HISTORY;
  sdAppendEntry(t, name ? name : "", historyTimestamp[idx]);
  sdUpdateBest(t);
}

void vextOn()  {}
void vextOff() {}

// ── Seite weiterschalten (zentral für Taste + Auto) ────────
void advancePage() {
  currentPage = (currentPage + 1) % NUM_PAGES;
  lastPageAt  = millis();
  drawDisplay();
}

// ── LED ────────────────────────────────────────────────────
void updateLED(unsigned long now) {
  static unsigned long lastToggle = 0;
  static bool          ledOn      = false;
  if (ledBlinkCount > 0) {
    if (now - ledBlinkAt >= 50) {
      ledBlinkAt = now; ledBlinkCount--;
      ledOn = (ledBlinkCount % 2 != 0);
      digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    }
    return;
  }
  switch (appState) {
    case IDLE: case LAP_IDLE:
      if (!ledOn  && (now - lastToggle) >= 1920) { ledOn = true;  digitalWrite(LED_PIN, HIGH); lastToggle = now; }
      else if (ledOn && (now - lastToggle) >= 80) { ledOn = false; digitalWrite(LED_PIN, LOW);  lastToggle = now; }
      break;
    case RUNNING: case LAP_RUNNING:
      if ((now - lastToggle) >= 150) { ledOn = !ledOn; digitalWrite(LED_PIN, ledOn); lastToggle = now; }
      break;
    case RESULT:
      digitalWrite(LED_PIN, HIGH); ledOn = true; break;
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
  SD.end();
  radio.sleep();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PRG_PIN, 0);
  delay(100);
  esp_deep_sleep_start();
}

// ── LoRa senden (SX1276) ───────────────────────────────────
void loRaSend(const char* msg) {
  int16_t rc = radio.transmit(msg);
  if (rc == RADIOLIB_ERR_NONE) {
    loraTxCount++;
    DBGVF("LORA-TX", "\"%s\"  OK  (#%lu)", msg, loraTxCount);
  } else {
    loraTxFail++;
    DBGF("LORA-TX", "\"%s\"  FEHLER rc=%d", msg, rc);
  }
  radio.startReceive();
}

// ── Cancel Run ─────────────────────────────────────────────
void cancelRun() {
  appState    = IDLE;
  runStartAt  = 0;
  plateFlag   = false;
  splitTimeMs = 0;
  splitRxAt   = 0;
  loRaSend("CAN");
  currentPage = 0;
  drawDisplay();
  DBG("STATE", "IDLE  ← Lauf abgebrochen (CAN gesendet)");
}

// ── I2C Bus-Recovery (unstuckt SDA wenn Low hängt) ────────
static void i2cBusRecover() {
  pinMode(OLED_SDA, OUTPUT); digitalWrite(OLED_SDA, HIGH);
  pinMode(OLED_SCL, OUTPUT); digitalWrite(OLED_SCL, HIGH);
  delayMicroseconds(10);
  for (int i = 0; i < 9; i++) {
    digitalWrite(OLED_SCL, LOW);  delayMicroseconds(5);
    digitalWrite(OLED_SCL, HIGH); delayMicroseconds(5);
  }
  // STOP-Condition
  digitalWrite(OLED_SDA, LOW);  delayMicroseconds(5);
  digitalWrite(OLED_SCL, HIGH); delayMicroseconds(5);
  digitalWrite(OLED_SDA, HIGH); delayMicroseconds(5);
  pinMode(OLED_SDA, INPUT);
  pinMode(OLED_SCL, INPUT);
}

// ── Display-Init ───────────────────────────────────────────
void initDisplay() {
  // Kein Hardware-RST (GPIO16 = PSRAM_CS, gesperrt)
  // I2C Bus-Recovery falls SDA nach Reset stuck-low ist
  i2cBusRecover();
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  u8g2.begin();
  u8g2.setContrast(cfg_contrast);
}

// ── Btn2 einrichten ────────────────────────────────────────
void setupBtn2() {
  if (cfg_btn2_pin < 40) {
    pinMode(cfg_btn2_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(cfg_btn2_pin), onBtn2, FALLING);
    DBGF("BTN2", "Zusatztaste auf GPIO%u", cfg_btn2_pin);
  }
}

// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(200);
  DBG("BOOT", "START-NODE LILYGO v13 startet...");

  prefs.begin("mtb-cfg-l", true);
  cfg_debounce_ms    = prefs.getUInt("debounce",   500);
  cfg_result_show_ms = prefs.getUInt("result",     8000);
  cfg_run_timeout_ms = prefs.getUInt("timeout",    300000);
  cfg_ping_ms        = prefs.getUInt("ping",       30000);
  cfg_lora_comp_ms   = prefs.getUInt("loracomp",   0);
  cfg_bat_mah        = prefs.getUInt("batmah",     1100);
  cfg_contrast       = prefs.getUChar("contrast",  255);
  cfg_plate_pin      = prefs.getUChar("platepin",  PLATE_PIN);
  cfg_plate_nc       = prefs.getBool("platenc",    false);
  cfg_btn2_pin       = prefs.getUChar("btn2pin",   255);
  cfg_page_auto_ms   = prefs.getUInt("autopage",   0);
  cfg_lora_pwr       = prefs.getUChar("lorapwr",   14);
  cfg_stag_offset_s  = prefs.getUChar("stagoffset", 30);
  cfg_track_id       = prefs.getUChar("trackid",   0);
  cfg_rider_id       = prefs.getUChar("riderid",   0);
  cfg_condition      = prefs.getUChar("condition",  0);
  cfg_session_num    = prefs.getUChar("sessionnum", 1);
  cfg_session_day    = prefs.getUInt("sessionday",  0);
  prefs.getString("apssid", cfg_ap_ssid, sizeof(cfg_ap_ssid));
  prefs.getString("appass", cfg_ap_pass, sizeof(cfg_ap_pass));
  prefs.end();
  if (strlen(cfg_ap_ssid) == 0) strcpy(cfg_ap_ssid, "MTB-Start-LILYGO");

  pinMode(cfg_plate_pin, INPUT_PULLUP);
  pinMode(PRG_PIN,        INPUT_PULLUP);
  pinMode(LED_PIN,        OUTPUT);
  digitalWrite(LED_PIN,  LOW);

  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) {
    bestTimeMs = 0; historyCnt = 0; histHead = 0;
    memset(history,          0, sizeof(history));
    memset(historyNames,     0, sizeof(historyNames));
    memset(historyTimestamp, 0, sizeof(historyTimestamp));
    lapMode = false; lapRoundNum = 0; lapRoundStart = 0; lapBestMs = 0; lapLastMs = 0;
    duelMode = false; duelDone = false; duelCount = 0; duelCurrent = 0;
    stagMode = false; stagDone = false; stagCount = 0; stagStarted = 0; stagFinished = 0;
    stagFinishedMask = 0;
    memset(stagStartMs, 0, sizeof(stagStartMs));
    memset(stagTimes,   0, sizeof(stagTimes));
  } else {
    if (historyCnt > 0) lastTimeMs = history[histPhys(historyCnt - 1)];
    if (lapMode) lapRoundStart = 0;
  }

  DBG("BOOT", "Display init...");
  initDisplay();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(8, 35, "Initialisiere...");
  u8g2.sendBuffer();
  DBG("BOOT", "Display OK");

  // ── SD-Karte (VSPI) ───────────────────────────────────
  SDSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdInit();  // lädt auch Tracks + Rider
  // Aktive Namen aus IDs auflösen
  if (cfg_track_id > 0) {
    Track* tr = sdFindTrack(cfg_track_id);
    if (tr) strncpy(activeTrackName, tr->name, sizeof(activeTrackName));
  }
  if (cfg_rider_id > 0) {
    Rider* rd = sdFindRider(cfg_rider_id);
    if (rd) strncpy(activeRiderName, rd->name, sizeof(activeRiderName));
  }

  // ── LoRa SX1276 (HSPI) ────────────────────────────────
  LoRaSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int16_t rc = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, 0x12, LORA_PWR, 8);
  if (rc != RADIOLIB_ERR_NONE) {
    u8g2.clearBuffer();
    char err[24]; sprintf(err, "LoRa Fehler: %d", rc);
    u8g2.drawStr(0, 35, err);
    u8g2.sendBuffer();
    DBGF("LORA", "FEHLER rc=%d", rc);
    delay(5000); ESP.restart();
  }
  radio.setOutputPower(cfg_lora_pwr);
  radio.setDio0Action(onLoRaRx, RISING);
  radio.startReceive();
  DBG("BOOT", "LoRa OK");

  attachInterrupt(digitalPinToInterrupt(cfg_plate_pin), onPlate,
                  cfg_plate_nc ? RISING : FALLING);
  setupBtn2();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(cfg_ap_ssid, strlen(cfg_ap_pass) > 0 ? cfg_ap_pass : nullptr);
  server.on("/",              handleRoot);
  server.on("/state",         handleState);
  server.on("/export",        handleExport);
  server.on("/reset",         handleReset);
  server.on("/cancel",        handleCancel);
  server.on("/settime",       handleSetTime);
  server.on("/duel",          handleDuelPage);
  server.on("/duelcount",     handleDuelCount);
  server.on("/duelname",      handleDuelName);
  server.on("/duelnext",      handleDuelNext);
  server.on("/duelconfirm",   handleDuelConfirm);
  server.on("/duelgo",        handleDuelGo);
  server.on("/duelexit",      handleDuelExit);
  server.on("/duelskip",      handleDuelSkip);
  server.on("/stag",          handleStagPage);
  server.on("/stagcount",     handleStagCount);
  server.on("/stagname",      handleStagName);
  server.on("/stagnext",      handleStagNext);
  server.on("/stagconfirm",   handleStagConfirm);
  server.on("/staggo",        handleStagGo);
  server.on("/stagexit",      handleStagExit);
  server.on("/lapstart",      handleLapStart);
  server.on("/lapstop",       handleLapStop);
  server.on("/lapreset",      handleLapReset);
  server.on("/settings/save", HTTP_POST, handleSettingsSave);
  server.on("/sleep",         handleSleep);
  server.on("/restart",       handleRestart);
  server.on("/ping",          handleManualPing);
  server.on("/sdformat",      handleSdFormat);
  server.on("/sdclearall",    handleSdClearAll);
  server.on("/tracks",        handleTracks);
  server.on("/tracksave",     HTTP_POST, handleTrackSave);
  server.on("/trackdel",      handleTrackDel);
  server.on("/trackset",      handleTrackSet);
  server.on("/riders",        handleRiders);
  server.on("/ridersave",     HTTP_POST, handleRiderSave);
  server.on("/riderdel",      handleRiderDel);
  server.on("/riderset",      handleRiderSet);
  server.on("/condition",     handleCondition);
  server.on("/editname",      HTTP_POST, handleEditName);
  server.on("/sessions",      handleSessions);
  server.on("/sessiondata",   handleSessionData);
  server.on("/sdstats",       handleSdStats);
  server.on("/bestlist",      handleBestlist);
  server.begin();

  DBG("BOOT",  "START-NODE LILYGO v13 bereit");
  DBGF("WIFI",  "SSID: %s  IP: %s", cfg_ap_ssid, WiFi.softAPIP().toString().c_str());
  DBGF("CFG",   "Plate=GPIO%u  Btn2=GPIO%u  AutoPage=%lums",
       cfg_plate_pin,
       cfg_btn2_pin < 40 ? cfg_btn2_pin : 255,
       cfg_page_auto_ms);
  DBGF("SD",    "%s", sdPresent ? "vorhanden" : "nicht eingelegt");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(4, 18, "MTB TIMER");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(4, 32, "START  LILYGO v24");
  u8g2.drawHLine(0, 36, 128);
  char wln[24]; snprintf(wln, sizeof(wln), "WiFi: %.16s", cfg_ap_ssid);
  u8g2.drawStr(0, 48, wln);
  u8g2.drawStr(0, 60, sdPresent ? "IP: 192.168.4.1 SD" : "IP: 192.168.4.1");
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

  // ── Batterie (T3 V1.6.1: analogRead, 2× Teiler GPIO35) ─
  if (now - lastBatReadAt >= BAT_READ_MS) {
    lastBatReadAt = now;
    uint32_t adcSum = 0;
    for (int _i = 0; _i < 8; _i++) adcSum += analogRead(BAT_ADC_PIN);
    batVoltage  = (adcSum / 8.0f) / 4095.0f * 2.0f * 3.3f;
    batPercent  = voltageToPct(batVoltage);
    DBGVF("BAT", "%.2f V  %u%%", batVoltage, batPercent);
    if (batPercent <= 5)       DBG("BAT", "*** AKKU KRITISCH ***");
    else if (batPercent <= 15) DBG("BAT", "Warnung: Akku niedrig");
  }

#if DEBUG_LEVEL >= 2
  if (now - lastDebugAt >= 30000UL) {
    lastDebugAt = now;
    const char* st = (appState==IDLE)?"IDLE":(appState==RUNNING)?"RUNNING":
                     (appState==RESULT)?"RESULT":(appState==LAP_IDLE)?"LAP_IDLE":"LAP_RUN";
    char up[14]; fmtUptime(up);
    DBGF("STATUS", "State=%-9s  Bat=%u%%/%.2fV  RTT=%lums  TX=%lu/%lu  Up=%s",
         st, batPercent, batVoltage, lastRttMs, loraTxCount, loraTxFail, up);
  }
#endif

  // ── PRG-Taste (GPIO0) ──────────────────────────────────
  bool btnNow = digitalRead(PRG_PIN);
  if (btnNow == LOW && btnPrev == HIGH) { btnDownAt = now; }
  else if (btnNow == LOW) {
    unsigned long held = now - btnDownAt;
    if (held >= LONG_PRESS_MS) goToSleep();
    else if (held >= 1000)     showSleepProgress(held);
  } else if (btnNow == HIGH && btnPrev == LOW) {
    if ((now - btnDownAt) < LONG_PRESS_MS) { pendingPresses++; lastPressAt = now; }
  }
  btnPrev = btnNow;

  if (pendingPresses > 0 && (now - lastPressAt) > DOUBLE_PRESS_MS) {
    if (pendingPresses >= 2 && (appState == RUNNING || appState == LAP_RUNNING)) {
      if (appState == RUNNING) cancelRun();
      else { lapMode = false; appState = IDLE; plateFlag = false; currentPage = 0; drawDisplay(); }
    } else if (pendingPresses == 1) {
      if (appState == RESULT) { appState = IDLE; drawDisplay(); }
      else advancePage();
    }
    pendingPresses = 0;
  }

  // ── Zusatztaste btn2 (Seite weiterschalten) ────────────
  if (btn2Flag) {
    btn2Flag = false;
    // kurze Entprellung
    static unsigned long btn2LastMs = 0;
    if (now - btn2LastMs > 300) {
      btn2LastMs = now;
      if (appState == RESULT) { appState = IDLE; drawDisplay(); }
      else advancePage();
      DBGV("BTN2", "Seite weitergeschaltet");
    }
  }

  // ── Auto-Seitenumschaltung ─────────────────────────────
  if (cfg_page_auto_ms > 0
      && appState != RUNNING && appState != LAP_RUNNING  // nicht während Messung
      && (now - lastPageAt) >= cfg_page_auto_ms) {
    advancePage();
  }

  // ── Druckplatte Start ──────────────────────────────────
  if (plateFlag) {
    plateFlag = false;
    uint64_t safeTrigUs = readPlateTrigger();
    unsigned long trigMs = (unsigned long)(safeTrigUs / 1000ULL);
    if (stagMode && stagStarted < stagCount && (now - plateLastMs) >= cfg_debounce_ms) {
      bool offsetOk = (stagStarted == 0) ||
                      ((now - stagLastStartMs) >= (unsigned long)cfg_stag_offset_s * 1000UL);
      if (offsetOk) {
        plateLastMs = now;
        uint8_t idx = stagStarted++;
        stagStartMs[idx]  = now;
        stagLastStartMs   = now;
        stagArmedFor      = idx;
        char stxBuf[28];
        snprintf(stxBuf, sizeof(stxBuf), "STX:%s", stagRiders[idx]);
        loRaSend(stxBuf);
        appState = RUNNING;
        ledBlinkCount = 6; ledBlinkAt = now;
        drawDisplay();
      }
    } else if (appState == IDLE && (now - plateLastMs) >= cfg_debounce_ms) {
      plateLastMs = now; runStartAt = trigMs;
      splitTimeMs = 0; splitRxAt = 0;
      ledBlinkCount = 6; ledBlinkAt = now;
      appState = RUNNING;
      char stxBuf[28];
      const char* rn = (duelMode && duelCurrent < duelCount) ? duelRiders[duelCurrent] : "";
      if (strlen(rn) > 0) snprintf(stxBuf, sizeof(stxBuf), "STX:%s", rn);
      else                 strcpy(stxBuf, "STX");
      loRaSend(stxBuf); drawDisplay();
      DBGF("STATE", "RUNNING ← Sensor (ISR @ %lluus)  STX gesendet", safeTrigUs);
    } else if (appState == LAP_IDLE) {
      lapRoundNum = 1; lapRoundStart = trigMs; lapLastMs = 0;
      plateLastMs = now; ledBlinkCount = 6; ledBlinkAt = now;
      appState = LAP_RUNNING; drawDisplay();
      DBG("STATE", "LAP_RUNNING ← R1 gestartet");
    } else if (appState == LAP_RUNNING) {
      unsigned long lapMs = trigMs - lapRoundStart;
      if (lapMs >= cfg_debounce_ms) {
        char rndName[6]; snprintf(rndName, sizeof(rndName), "R%u", lapRoundNum);
        addHistory(lapMs, rndName);
        if (lapBestMs == 0 || lapMs < lapBestMs) lapBestMs = lapMs;
        if (bestTimeMs == 0 || lapMs < bestTimeMs) bestTimeMs = lapMs;
        lapLastMs = lapMs; lapRoundNum++; lapRoundStart = trigMs;
        plateLastMs = now; ledBlinkCount = 6; ledBlinkAt = now;
        drawDisplay(0);
        char tbuf[12]; fmtTime(lapMs, tbuf);
        DBGF("LAP", "R%u: %s", lapRoundNum - 1, tbuf);
      }
    }
  }

  // ── Anzeige-Refresh ────────────────────────────────────
  if (appState == RUNNING || appState == LAP_RUNNING) {
    if ((now - lastDispRefr) >= DISP_REFRESH_MS) {
      lastDispRefr = now;
      unsigned long disp = (appState == RUNNING) ? (now - runStartAt) : (now - lapRoundStart);
      drawDisplay(disp);
    }
  } else if ((now - lastDispRefr) >= 1000UL) {
    lastDispRefr = now; drawDisplay();
  }

  // ── Timeouts ───────────────────────────────────────────
  if (appState == RUNNING && (now - runStartAt) > cfg_run_timeout_ms) {
    appState = IDLE; drawDisplay();
    DBGF("STATE", "IDLE ← Timeout nach %lus", cfg_run_timeout_ms / 1000);
  }
  if (appState == LAP_RUNNING) {
    unsigned long lapElapsed = (unsigned long)((uint64_t)esp_timer_get_time() / 1000ULL) - lapRoundStart;
    if (lapElapsed > cfg_run_timeout_ms) { appState = LAP_IDLE; plateFlag = false; drawDisplay(); }
  }
  if (appState == RESULT && (now - resultAt) > cfg_result_show_ms) {
    appState = IDLE; drawDisplay();
  }

  // ── Ping + TSY-Broadcast ───────────────────────────────
  if (appState == IDLE && (now - lastPingAt) >= cfg_ping_ms) {
    lastPingAt = now;
    DBGV("PING", "PNG gesendet – warte auf POG");
    loRaSend("PNG");
    if (timeIsSynced) {
      char tsyBuf[28];
      snprintf(tsyBuf, sizeof(tsyBuf), "TSY:%lld", (long long)nowUnixMs());
      loRaSend(tsyBuf);
    }
  }

  // ── LoRa-Empfang ───────────────────────────────────────
  if (rxFlag) {
    rxFlag = false;
    String msg; int16_t rc = radio.readData(msg);
    radio.startReceive();

    if (rc == RADIOLIB_ERR_NONE) {
      loraRssi = radio.getRSSI(); loraSnr = radio.getSNR();
      loraLastContact = now; loraRxCount++;
      if (!loraHasRx || loraRssi < loraRssiMin) loraRssiMin = loraRssi;
      if (!loraHasRx || loraRssi > loraRssiMax) loraRssiMax = loraRssi;
      loraHasRx = true;
      DBGVF("LORA-RX", "\"%s\"  RSSI=%d dBm  SNR=%.1f dB", msg.c_str(), (int)loraRssi, loraSnr);

      if (msg.startsWith("TIM:") && stagMode && stagStarted > 0 && stagFinished < stagCount) {
        finishLastContact = now;
        char* ep; unsigned long raw_us = strtoul(msg.c_str() + 4, &ep, 10);
        if (ep != msg.c_str() + 4 && raw_us > 0 && raw_us <= 3600000000UL) {
          unsigned long t = (raw_us + 500UL) / 1000UL;
          if (cfg_lora_comp_ms > 0 && t >= cfg_lora_comp_ms) t -= cfg_lora_comp_ms;
          // FIFO: ältester nicht-fertiger Fahrer
          uint8_t k = 0xFF;
          for (uint8_t i = 0; i < stagStarted; i++) {
            if (!(stagFinishedMask & (1u << i))) { k = i; break; }
          }
          if (k != 0xFF) {
            // Zeitkorrektur falls Finish für anderen Fahrer gewappnet war
            if (stagArmedFor != 0xFF && stagArmedFor < stagCount && stagArmedFor != k) {
              long corr = (long)stagStartMs[stagArmedFor] - (long)stagStartMs[k];
              if (corr > 0 && (unsigned long)corr < 3600000UL) t += (unsigned long)corr;
            }
            stagTimes[k] = t;
            stagFinishedMask |= (1u << k);
            stagFinished++;
            addHistory(t, stagRiders[k]);
            lastTimeMs = t;
            if (bestTimeMs == 0 || t < bestTimeMs) bestTimeMs = t;
            // Re-arm Finish für nächsten gestarteten-aber-nicht-fertigen Fahrer
            uint8_t next = 0xFF;
            for (uint8_t i = k + 1; i < stagStarted; i++) {
              if (!(stagFinishedMask & (1u << i))) { next = i; break; }
            }
            stagArmedFor = next;
            loRaSend("ACK");
            // Re-Arm verzögert: erst nach 300ms damit Finish ACK verarbeiten und
            // IDLE werden kann, bevor das neue STX ankommt
            if (next != 0xFF) {
              stagRearmIdx = next;
              stagRearmAt  = millis() + 300;
            }
            if (stagFinished >= stagCount) {
              stagDone = true; stagMode = false;
              appState = RESULT; resultAt = now;
            } else if (stagFinished >= stagStarted) {
              appState = IDLE; // warten auf nächste Starts
            }
            currentPage = 0; drawDisplay();
          } else { loRaSend("ACK"); }
        }
      } else if (msg.startsWith("TIM:") && appState == RUNNING) {
        finishLastContact = now;
        char* ep; unsigned long t = strtoul(msg.c_str() + 4, &ep, 10);
        if (ep != msg.c_str() + 4 && t > 0 && t <= 3600000000UL) {
          if (cfg_lora_comp_ms > 0 && t >= cfg_lora_comp_ms * 1000UL) t -= cfg_lora_comp_ms * 1000UL;
          t = (t + 500UL) / 1000UL;
          const char* riderName = (duelMode && duelCurrent < duelCount) ? duelRiders[duelCurrent] : "";
          lastTimeMs = t;
          if (bestTimeMs == 0 || t < bestTimeMs) bestTimeMs = t;
          addHistory(t, riderName);
          if (duelMode) { duelCurrent++; if (duelCurrent >= duelCount) { duelMode = false; duelDone = true; } }
          appState = RESULT; resultAt = now; currentPage = 0;
          drawDisplay(); loRaSend("ACK");
          char tbuf[12]; fmtTime(t, tbuf);
          bool isBest = (bestTimeMs > 0 && t == bestTimeMs);
          DBGF("RESULT", "%s%s  (comp=-%lums gerundet)", tbuf, isBest ? "  *** BESTZEIT ***" : "", cfg_lora_comp_ms);
          if (duelDone) DBG("DUEL", "Duell beendet");
          else if (duelMode) DBGF("DUEL", "Fahrer %u/%u als naechster", duelCurrent + 1, duelCount);
        } else {
          DBGF("LORA-RX", "TIM: ungueltige Nutzlast ignoriert: \"%s\"", msg.c_str());
        }
      }
      if (msg == "HBT") { finishLastContact = now; DBGV("LORA-RX", "HBT empfangen (Ziel-Heartbeat)"); }
      if (msg == "POG") {
        finishLastContact = now;
        if (lastPingAt > 0) lastRttMs = now - lastPingAt;
        DBGF("LORA-RX", "POG – RTT=%lums  Empfehlung=%.0f ms", lastRttMs, lastRttMs / 2.0f);
      }
      if (msg.startsWith("SPL:") && appState == RUNNING) {
        splitLastContact = now;
        char* ep; unsigned long t = strtoul(msg.c_str() + 4, &ep, 10);
        if (ep != msg.c_str() + 4 && t > 0 && t <= 3600000000UL) {
          if (cfg_lora_comp_ms > 0 && t >= cfg_lora_comp_ms * 1000UL) t -= cfg_lora_comp_ms * 1000UL;
          t = (t + 500UL) / 1000UL;
          splitTimeMs = t; splitRxAt = now; loRaSend("ACK");
          char tbuf[12]; fmtTime(t, tbuf);
          DBGF("SPLIT", "%s  (comp=-%lums gerundet)", tbuf, cfg_lora_comp_ms);
        } else {
          DBGF("LORA-RX", "SPL: ungueltige Nutzlast ignoriert: \"%s\"", msg.c_str());
        }
      }
    }
  }

  // ── Stag Re-Arm (verzögert nach ACK) ──────────────────────
  if (stagRearmIdx != 0xFF && millis() >= stagRearmAt) {
    char stxBuf[28];
    snprintf(stxBuf, sizeof(stxBuf), "STX:%s", stagRiders[stagRearmIdx]);
    loRaSend(stxBuf);
    stagRearmIdx = 0xFF;
    stagRearmAt  = 0;
  }
}

// ============================================================
//  SPLIT-NODE  v11  –  LILYGO TTGO T3 V1.6.1 (ESP32 + SX1276)
//  MTB Downhill Zeitmessung
//  Bibliotheken: RadioLib >= 6.6, U8g2 >= 2.35, SD (Arduino Core)
// ============================================================
// ── Debug-Level ────────────────────────────────────────────
//  0 = aus  |  1 = Info  |  2 = Verbose
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
#include <SD.h>
#include "esp_sleep.h"
#include "esp_timer.h"
#include <Update.h>

#ifndef HSPI
#define HSPI 2
#endif
#ifndef VSPI
#define VSPI 3
#endif

#define OLED_SDA   21
#define OLED_SCL   22
// GPIO16 = PSRAM_CS auf ESP32-PICO-D4 → NICHT verwenden!
#define LORA_SCK    5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_NSS   18
#define LORA_RST   23
#define LORA_DIO0  26
#define LORA_DIO1  33
#define SD_CS      13
#define SD_SCK     14
#define SD_MISO     2
#define SD_MOSI    15
#define PLATE_PIN   2
#define PRG_PIN     0
#define LED_PIN    25
#define BAT_ADC_PIN 35

#define LORA_FREQ  868.0f
#define LORA_BW    125.0f
#define LORA_SF        7
#define LORA_CR        5
#define LORA_PWR      14

#define DISP_REFRESH_MS   100UL
#define LONG_PRESS_MS    3000UL
#define DOUBLE_PRESS_MS   400UL
#define BAT_READ_MS     10000UL
#define MAX_HISTORY          20
#define NAME_MAX_LEN         20
#define NUM_PAGES             4
#define PEER_TIMEOUT_S       90

#define RSSI_BAR5  (-65)
#define RSSI_BAR4  (-75)
#define RSSI_BAR3  (-85)
#define RSSI_BAR2  (-95)

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
char     cfg_ap_ssid[33]    = "MTB-Split-LILYGO";
char     cfg_ap_pass[64]    = "";
uint8_t  cfg_lora_pwr       = 14;
uint8_t  cfg_btn2_pin      = 255;
uint32_t cfg_page_auto_ms  = 0;

Preferences prefs;

SPIClass LoRaSPI(HSPI);
SPIClass SDSPI(VSPI);
SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1, LoRaSPI);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
WebServer server(80);

bool sdPresent = false;
static const char* SD_LOGFILE = "/split_log.csv";

RTC_DATA_ATTR unsigned long history[MAX_HISTORY];
RTC_DATA_ATTR char          historyNames[MAX_HISTORY][NAME_MAX_LEN + 1];
RTC_DATA_ATTR int64_t       historyTimestamp[MAX_HISTORY];
RTC_DATA_ATTR uint8_t       historyCnt = 0;
RTC_DATA_ATTR uint8_t       histHead   = 0;
RTC_DATA_ATTR unsigned long bestTimeMs = 0;

enum State : uint8_t { IDLE, ARMED, DONE };
State appState = IDLE;

unsigned long lastTimeMs   = 0;
uint64_t      startRecvUs  = 0;
unsigned long doneAt       = 0;
unsigned long lastDispRefr = 0;
unsigned long lastRetryAt  = 0;
uint8_t       retryCnt     = 0;
bool          ackReceived  = false;
char          txBuf[24]    = "";
unsigned long lastBatReadAt = 0;
unsigned long lastDebugAt   = 0;
unsigned long lastPageAt    = 0;

uint8_t       ledBlinkCount = 0;
unsigned long ledBlinkAt    = 0;
float   batVoltage  = 0.0f;
uint8_t batPercent  = 0;
int64_t       timeOffsetMs  = 0;
bool          timeIsSynced  = false;
unsigned long lastSyncAt    = 0;
uint8_t currentPage = 0;

volatile bool     plateFlag      = false;
volatile uint64_t plateTriggerUs = 0;
IRAM_ATTR void onPlateTrigger() { if(!plateFlag){plateTriggerUs=(uint64_t)esp_timer_get_time();plateFlag=true;} }

float         loraRssi        = 0.0f;
float         loraSnr         = 0.0f;
unsigned long loraLastContact = 0;
uint32_t      loraTxCount     = 0;
uint32_t      loraTxFail      = 0;
uint32_t      loraRxCount     = 0;
bool          loraHasRx       = false;
float         loraRssiMin     = 0.0f;
float         loraRssiMax     = 0.0f;

bool          btnPrev        = HIGH;
unsigned long btnDownAt      = 0;
uint8_t       pendingPresses = 0;
unsigned long lastPressAt    = 0;

volatile bool rxFlag = false;
IRAM_ATTR void onLoRaRx() { rxFlag = true; }

volatile bool btn2Flag = false;
IRAM_ATTR void onBtn2() { btn2Flag = true; }

// ── Forward-Declarations ───────────────────────────────────
void drawDisplay(unsigned long liveMs = 0);
void showSleepProgress(unsigned long heldMs);
void sendHTML();
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
void sdInit();
void sdAppendEntry(unsigned long ms, const char* name, int64_t ts);
String sdReadCsv();
void sdClear();
void handleSdFormat();

// ── Hilfsfunktionen ────────────────────────────────────────
void fmtTime(unsigned long ms, char* out) { sprintf(out,"%02u:%02u.%03u",(unsigned)(ms/60000),(unsigned)((ms%60000)/1000),(unsigned)(ms%1000)); }
void fmtUptime(char* out) { unsigned long s=millis()/1000,m=s/60;s%=60;unsigned long h=m/60;m%=60;sprintf(out,"%lu:%02lu:%02lu",h,m,s); }
uint8_t voltageToPct(float v) {
  static const float vpts[]={3.00f,3.20f,3.30f,3.40f,3.50f,3.60f,3.70f};
  static const float ppts[]={0,2,8,20,40,60,100};
  if(v>=3.70f)return 100;if(v<3.00f)return 0;
  for(int i=1;i<7;i++){if(v<vpts[i])return(uint8_t)(ppts[i-1]+(v-vpts[i-1])/(vpts[i]-vpts[i-1])*(ppts[i]-ppts[i-1]));}
  return 0;
}
int64_t nowUnixMs() { return timeIsSynced?(int64_t)millis()+timeOffsetMs:0; }
const char* rssiStatus(float r){return r>=RSSI_BAR5?"GUT":r>=RSSI_BAR3?"MITTEL":"SCHWACH";}

static inline uint8_t histPhys(uint8_t i) {
  return (uint8_t)((histHead + MAX_HISTORY - historyCnt + i) % MAX_HISTORY);
}

void addHistory(unsigned long t, const char* name) {
  uint8_t idx = histHead;
  history[idx]=t; historyTimestamp[idx]=nowUnixMs();
  strncpy(historyNames[idx],name?name:"",NAME_MAX_LEN); historyNames[idx][NAME_MAX_LEN]='\0';
  if(historyCnt<MAX_HISTORY)historyCnt++;
  histHead=(histHead+1)%MAX_HISTORY;
  sdAppendEntry(t,name?name:"",historyTimestamp[idx]);
}
void vextOn()  {}
void vextOff() {}

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

void updateLED(unsigned long now) {
  static unsigned long lastToggle=0; static bool ledOn=false;
  if(ledBlinkCount>0){if(now-ledBlinkAt>=50){ledBlinkAt=now;ledBlinkCount--;ledOn=(ledBlinkCount%2!=0);digitalWrite(LED_PIN,ledOn?HIGH:LOW);}return;}
  switch(appState){
    case IDLE:if(!ledOn&&(now-lastToggle)>=1920){ledOn=true;digitalWrite(LED_PIN,HIGH);lastToggle=now;}else if(ledOn&&(now-lastToggle)>=80){ledOn=false;digitalWrite(LED_PIN,LOW);lastToggle=now;}break;
    case ARMED:if((now-lastToggle)>=150){ledOn=!ledOn;digitalWrite(LED_PIN,ledOn);lastToggle=now;}break;
    case DONE:digitalWrite(LED_PIN,HIGH);ledOn=true;break;
  }
}

void goToSleep() {
  digitalWrite(LED_PIN,LOW); u8g2.clearBuffer(); u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(22,35,"Schlafe..."); u8g2.sendBuffer(); delay(800); u8g2.setPowerSave(1);
  SD.end(); radio.sleep(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PRG_PIN,0); delay(100); esp_deep_sleep_start();
}

void loRaSend(const char* msg) {
  int16_t rc=radio.transmit(msg);
  if(rc==RADIOLIB_ERR_NONE){loraTxCount++;DBGVF("LORA-TX","\"%s\"  OK  (#%lu)",msg,loraTxCount);}
  else{loraTxFail++;DBGF("LORA-TX","\"%s\"  FEHLER rc=%d",msg,rc);}
  radio.startReceive();
}

void cancelRun() { appState=IDLE;plateFlag=false;currentPage=0;drawDisplay();DBG("STATE","IDLE ← Lauf abgebrochen"); }

static void i2cBusRecover() {
  pinMode(OLED_SDA,OUTPUT); digitalWrite(OLED_SDA,HIGH);
  pinMode(OLED_SCL,OUTPUT); digitalWrite(OLED_SCL,HIGH); delayMicroseconds(10);
  for(int i=0;i<9;i++){digitalWrite(OLED_SCL,LOW);delayMicroseconds(5);digitalWrite(OLED_SCL,HIGH);delayMicroseconds(5);}
  digitalWrite(OLED_SDA,LOW);delayMicroseconds(5);digitalWrite(OLED_SCL,HIGH);delayMicroseconds(5);digitalWrite(OLED_SDA,HIGH);delayMicroseconds(5);
  pinMode(OLED_SDA,INPUT);pinMode(OLED_SCL,INPUT);
}

void initDisplay() {
  // Kein Hardware-RST (GPIO16 = PSRAM_CS, gesperrt)
  i2cBusRecover();
  Wire.begin(OLED_SDA,OLED_SCL); Wire.setClock(400000); u8g2.begin(); u8g2.setContrast(cfg_contrast);
}

void setup() {
  Serial.begin(115200);
  prefs.begin("mtb-cfg3-l",true);
  cfg_debounce_ms=prefs.getUInt("debounce",500); cfg_result_show_ms=prefs.getUInt("result",8000);
  cfg_run_timeout_ms=prefs.getUInt("timeout",300000); cfg_lora_comp_ms=prefs.getUInt("loracomp",0);
  cfg_lora_pwr=prefs.getUChar("lorapwr",14);
  cfg_btn2_pin=prefs.getUChar("btn2pin",255);
  cfg_page_auto_ms=prefs.getUInt("autopage",0);
  cfg_retry_interval=prefs.getUInt("retryiv",2000); cfg_bat_mah=prefs.getUInt("batmah",1100);
  cfg_max_retries=prefs.getUChar("maxretry",3); cfg_contrast=prefs.getUChar("contrast",255);
  cfg_plate_pin=prefs.getUChar("platepin",PLATE_PIN); cfg_plate_nc=prefs.getBool("platenc",false);
  prefs.getString("apssid",cfg_ap_ssid,sizeof(cfg_ap_ssid));
  prefs.getString("appass",cfg_ap_pass,sizeof(cfg_ap_pass));
  prefs.end();
  if(strlen(cfg_ap_ssid)==0)strcpy(cfg_ap_ssid,"MTB-Split-LILYGO");

  pinMode(cfg_plate_pin,INPUT_PULLUP); pinMode(PRG_PIN,INPUT_PULLUP);
  pinMode(LED_PIN,OUTPUT); digitalWrite(LED_PIN,LOW);

  if(esp_sleep_get_wakeup_cause()!=ESP_SLEEP_WAKEUP_EXT0){
    bestTimeMs=0;historyCnt=0;histHead=0;
    memset(history,0,sizeof(history));memset(historyNames,0,sizeof(historyNames));memset(historyTimestamp,0,sizeof(historyTimestamp));
  } else { if(historyCnt>0)lastTimeMs=history[histPhys(historyCnt-1)]; }

  initDisplay();
  u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tf);u8g2.drawStr(8,35,"Initialisiere...");u8g2.sendBuffer();

  sdInit();

  LoRaSPI.begin(LORA_SCK,LORA_MISO,LORA_MOSI,LORA_NSS);
  int16_t rc=radio.begin(LORA_FREQ,LORA_BW,LORA_SF,LORA_CR,0x12,LORA_PWR,8);
  if(rc!=RADIOLIB_ERR_NONE){u8g2.clearBuffer();char err[24];sprintf(err,"LoRa Fehler: %d",rc);u8g2.drawStr(0,35,err);u8g2.sendBuffer();delay(5000);ESP.restart();}
  radio.setOutputPower(cfg_lora_pwr);
  radio.setDio0Action(onLoRaRx,RISING);
  radio.startReceive();
  attachInterrupt(digitalPinToInterrupt(cfg_plate_pin),onPlateTrigger,cfg_plate_nc?RISING:FALLING);
  setupBtn2();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(cfg_ap_ssid,strlen(cfg_ap_pass)>0?cfg_ap_pass:nullptr);
  server.on("/",handleRoot); server.on("/state",handleState); server.on("/export",handleExport);
  server.on("/reset",handleReset); server.on("/ping",handleManualPing); server.on("/cancel",handleCancel);
  server.on("/settime",handleSetTime); server.on("/settings/save",HTTP_POST,handleSettingsSave);
  server.on("/update",HTTP_GET,handleOtaPage); server.on("/update",HTTP_POST,handleOtaUpload,handleOtaStream);
  server.on("/sleep",handleSleep); server.on("/restart",handleRestart);
  server.on("/sdformat",handleSdFormat);
  server.begin();

  DBG("BOOT",  "SPLIT-NODE LILYGO v11 bereit");
  DBGF("WIFI",  "SSID: %s  IP: %s", cfg_ap_ssid, WiFi.softAPIP().toString().c_str());
  DBGF("SD",    "%s", sdPresent?"vorhanden":"nicht eingelegt");

  u8g2.clearBuffer();u8g2.setFont(u8g2_font_7x13B_tf);u8g2.drawStr(4,18,"MTB TIMER");
  u8g2.setFont(u8g2_font_6x10_tf);u8g2.drawStr(4,32,"SPLIT  LILYGO v11");
  u8g2.drawHLine(0,36,128);char wln[24];snprintf(wln,sizeof(wln),"WiFi: %.16s",cfg_ap_ssid);
  u8g2.drawStr(0,48,wln);u8g2.drawStr(0,60,sdPresent?"IP: 192.168.4.1 SD":"IP: 192.168.4.1");
  u8g2.sendBuffer();delay(2500);lastPageAt=millis();drawDisplay();
}

void loop() {
  unsigned long now=millis();
  server.handleClient(); updateLED(now);

  // ── Batterie-Messung (LILYGO: 2× Teiler, GPIO35) ──────
  if(now-lastBatReadAt>=BAT_READ_MS){
    lastBatReadAt=now; uint32_t s=0;
    for(int i=0;i<8;i++)s+=analogRead(BAT_ADC_PIN);
    batVoltage=(s/8.0f)/4095.0f*2.0f*3.3f; batPercent=voltageToPct(batVoltage);
    DBGVF("BAT","%.2f V  %u%%",batVoltage,batPercent);
    if(batPercent<=5)DBG("BAT","*** AKKU KRITISCH ***");
    else if(batPercent<=15)DBG("BAT","Warnung: Akku niedrig");
  }

  // ── PRG-Taste ──────────────────────────────────────────
  bool btnNow=digitalRead(PRG_PIN);
  if(btnNow==LOW&&btnPrev==HIGH){btnDownAt=now;}
  else if(btnNow==LOW){unsigned long h=now-btnDownAt;if(h>=LONG_PRESS_MS)goToSleep();else if(h>=1000)showSleepProgress(h);}
  else if(btnNow==HIGH&&btnPrev==LOW){if((now-btnDownAt)<LONG_PRESS_MS){pendingPresses++;lastPressAt=now;}}
  btnPrev=btnNow;
  if(pendingPresses>0&&(now-lastPressAt)>DOUBLE_PRESS_MS){
    if(pendingPresses>=2&&appState==ARMED)cancelRun();
    else if(pendingPresses==1){if(appState==DONE){appState=IDLE;drawDisplay();}else advancePage();}
    pendingPresses=0;
  }

  // ── Zusatztaste ────────────────────────────────────────
  if (btn2Flag) {
    btn2Flag = false;
    static unsigned long btn2LastMs = 0;
    if (now - btn2LastMs > 300) {
      btn2LastMs = now;
      if (appState == DONE) { appState = IDLE; drawDisplay(); }
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
  if(rxFlag){
    rxFlag=false; String msg; int16_t rc=radio.readData(msg); radio.startReceive();
    if(rc==RADIOLIB_ERR_NONE){
      loraRssi=radio.getRSSI();loraSnr=radio.getSNR();loraLastContact=now;loraRxCount++;
      if(!loraHasRx||loraRssi<loraRssiMin)loraRssiMin=loraRssi;
      if(!loraHasRx||loraRssi>loraRssiMax)loraRssiMax=loraRssi;
      loraHasRx=true;
      DBGVF("LORA-RX","\"%s\"  RSSI=%d dBm",msg.c_str(),(int)loraRssi);

      if(msg.startsWith("STX")){
        startRecvUs=esp_timer_get_time();plateFlag=false;retryCnt=0;ackReceived=false;appState=ARMED;drawDisplay();DBG("STATE","ARMED ← STX");
      }
      if(msg=="ACK"&&appState==DONE){ackReceived=true;DBG("LORA-RX","ACK – bestätigt");}
      if(msg=="PNG")loRaSend("POG");
      if(msg.startsWith("TSY:")){
        int64_t rxTs=(int64_t)atoll(msg.c_str()+4);
        if(rxTs>1000000000000LL){
          timeOffsetMs=rxTs-(int64_t)millis(); timeIsSynced=true; lastSyncAt=millis();
          DBGF("SYNC","TSY – Offset=%lld ms",timeOffsetMs);
        }
      }
      if(msg=="CAN"&&appState==ARMED){cancelRun();DBG("STATE","IDLE ← CAN");}
    }
  }

  // ── Druckplatte Split (Interrupt) ──────────────────────
  if(plateFlag&&appState==ARMED){
    plateFlag=false;
    if(plateTriggerUs<startRecvUs)return;
    uint64_t eUs=plateTriggerUs-startRecvUs;
    if(eUs>(cfg_debounce_ms*1000ULL)){
      // Rohe µs senden – Kompensation erfolgt auf Start-Node
      uint32_t eRaw=(uint32_t)eUs;
      unsigned long eMs=(unsigned long)((eUs+500ULL)/1000ULL);
      lastTimeMs=eMs;
      if(bestTimeMs==0||eMs<bestTimeMs)bestTimeMs=eMs;
      addHistory(eMs,"Split");
      ledBlinkCount=6;ledBlinkAt=now;
      sprintf(txBuf,"SPL:%lu",(unsigned long)eRaw);  // µs-Wert
      loRaSend(txBuf);
      retryCnt=0;ackReceived=false;lastRetryAt=now;doneAt=now;appState=DONE;drawDisplay();
      char tbuf[12];fmtTime(eMs,tbuf);
      DBGF("SPLIT","SPLIT! %s  (%luus roh)",tbuf,(unsigned long)eRaw);
    }
  }

  // ── Anzeige-Refresh ────────────────────────────────────
  if(appState==ARMED){
    if((now-lastDispRefr)>=DISP_REFRESH_MS){lastDispRefr=now;drawDisplay((unsigned long)(((uint64_t)esp_timer_get_time()-startRecvUs)/1000));}
  } else if((now-lastDispRefr)>=1000UL){
    lastDispRefr=now; drawDisplay();
  }

  // ── Lauf-Timeout ───────────────────────────────────────
  if(appState==ARMED){
    if(((uint64_t)esp_timer_get_time()-startRecvUs)/1000>cfg_run_timeout_ms){
      appState=IDLE;drawDisplay();DBG("STATE","IDLE ← Timeout");
    }
  }

  // ── Periodischer Status-Dump ───────────────────────────
#if DEBUG_LEVEL >= 2
  if(now-lastDebugAt>=30000UL){
    lastDebugAt=now;
    const char* stStr2=appState==IDLE?"IDLE":appState==ARMED?"ARMED":"DONE";
    char up[14];fmtUptime(up);
    DBGF("STATUS","State=%-6s  Bat=%u%%/%.2fV  TX=%lu/%lu  RX=%lu  Up=%s",
         stStr2,batPercent,batVoltage,loraTxCount,loraTxFail,loraRxCount,up);
  }
#endif

  // ── Ergebnis-Retries ───────────────────────────────────
  if(appState==DONE&&!ackReceived&&retryCnt<cfg_max_retries&&(now-lastRetryAt)>=cfg_retry_interval){
    loRaSend(txBuf);lastRetryAt=now;retryCnt++;
    DBGF("RETRY","#%u/%u  SPL neu gesendet",retryCnt,cfg_max_retries);
  }
  if(appState==DONE&&(now-doneAt)>=cfg_result_show_ms){appState=IDLE;drawDisplay();}
}

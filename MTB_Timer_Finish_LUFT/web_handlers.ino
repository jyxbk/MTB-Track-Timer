void handleCancel() {
  cancelRun();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetTime() {
  if (server.hasArg("ts")) {
    int64_t rxTs = (int64_t)atoll(server.arg("ts").c_str());
    timeOffsetMs = rxTs - (int64_t)millis();
    timeIsSynced = true;
    lastSyncAt   = millis();
  }
  server.send(204, "text/plain", "");
}

// ── BMP280 Rekalibrierung ──────────────────────────────────
void handleCalibrate() {
  if (!bmpCalibrated) {
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"BMP280 nicht initialisiert\"}");
    return;
  }
  float sum = 0.0f;
  const int N = 20;
  for (int i = 0; i < N; i++) {
    sum += bmp.readPressure();
    delay(150);
  }
  bmpBaseline = sum / (float)N;
  plateFlag   = false;
  char resp[64];
  snprintf(resp, sizeof(resp), "{\"ok\":true,\"baseline\":%.0f}", bmpBaseline);
  server.send(200, "application/json", resp);
}

// ── Settings: NVS lesen/schreiben ──────────────────────────
void saveSettings() {
  prefs.begin("mtb-cfg2-lu", false);
  if (prefs.getUInt("debounce",  500)    != cfg_debounce_ms)           prefs.putUInt("debounce",   cfg_debounce_ms);
  if (prefs.getUInt("result",    8000)   != cfg_result_show_ms)        prefs.putUInt("result",     cfg_result_show_ms);
  if (prefs.getUInt("timeout",   300000) != cfg_run_timeout_ms)        prefs.putUInt("timeout",    cfg_run_timeout_ms);
  if (prefs.getUInt("loracomp",  0)      != cfg_lora_comp_ms)          prefs.putUInt("loracomp",   cfg_lora_comp_ms);
  if (prefs.getUInt("retryiv",   2000)   != cfg_retry_interval)        prefs.putUInt("retryiv",    cfg_retry_interval);
  if (prefs.getUInt("batmah",    1100)   != cfg_bat_mah)               prefs.putUInt("batmah",     cfg_bat_mah);
  if (prefs.getUInt("bmpthresh", 80)     != cfg_pressure_threshold_pa) prefs.putUInt("bmpthresh",  cfg_pressure_threshold_pa);
  if (prefs.getUInt("bmpcaldly", 3000)   != cfg_bmp_cal_delay_ms)      prefs.putUInt("bmpcaldly",  cfg_bmp_cal_delay_ms);
  if (prefs.getUChar("maxretry", 3)      != cfg_max_retries)           prefs.putUChar("maxretry",  cfg_max_retries);
  if (prefs.getUChar("contrast", 255)    != cfg_contrast)              prefs.putUChar("contrast",  cfg_contrast);
  if (prefs.getUChar("lorapwr",  14)     != cfg_lora_pwr)              prefs.putUChar("lorapwr",   cfg_lora_pwr);
  if (prefs.getUChar("btn2pin",  255)    != cfg_btn2_pin)              prefs.putUChar("btn2pin",   cfg_btn2_pin);
  if (prefs.getUInt("autopage",  0)      != cfg_page_auto_ms)          prefs.putUInt("autopage",   cfg_page_auto_ms);
  prefs.putString("apssid", cfg_ap_ssid);
  prefs.putString("appass", cfg_ap_pass);
  prefs.end();
}

void handleSettingsSave() {
  bool needsRestart = false;
  if (server.hasArg("debounce"))
    cfg_debounce_ms = (uint32_t)constrain(server.arg("debounce").toInt(), 50, 2000);
  if (server.hasArg("result"))
    cfg_result_show_ms = (uint32_t)constrain(server.arg("result").toInt(), 2000, 30000);
  if (server.hasArg("timeout"))
    cfg_run_timeout_ms = (uint32_t)(constrain(server.arg("timeout").toInt(), 1, 30) * 60000);
  if (server.hasArg("loracomp"))
    cfg_lora_comp_ms = (uint32_t)constrain(server.arg("loracomp").toInt(), 0, 500);
  if (server.hasArg("lorapwr")) {
    cfg_lora_pwr = (uint8_t)constrain(server.arg("lorapwr").toInt(), 2, 20);
    radio.setOutputPower(cfg_lora_pwr);
  }
  if (server.hasArg("retryiv"))
    cfg_retry_interval = (uint32_t)constrain(server.arg("retryiv").toInt(), 500, 10000);
  if (server.hasArg("maxretry"))
    cfg_max_retries = (uint8_t)constrain(server.arg("maxretry").toInt(), 0, 10);
  if (server.hasArg("batmah"))
    cfg_bat_mah = (uint32_t)constrain(server.arg("batmah").toInt(), 100, 10000);
  if (server.hasArg("contrast")) {
    cfg_contrast = (uint8_t)server.arg("contrast").toInt();
    u8g2.setContrast(cfg_contrast);
  }
  if (server.hasArg("bmpthresh"))
    cfg_pressure_threshold_pa = (uint32_t)constrain(server.arg("bmpthresh").toInt(), 10, 5000);
  if (server.hasArg("bmpcaldly"))
    cfg_bmp_cal_delay_ms = (uint32_t)constrain(server.arg("bmpcaldly").toInt(), 1000, 10000);
  if (server.hasArg("btn2pin")) {
    uint8_t np = (uint8_t)constrain(server.arg("btn2pin").toInt(), 0, 255);
    if (np != cfg_btn2_pin) { cfg_btn2_pin = np; needsRestart = true; }
  }
  if (server.hasArg("autopage")) {
    int secs = server.arg("autopage").toInt();
    cfg_page_auto_ms = (secs > 0) ? (uint32_t)constrain(secs, 2, 60) * 1000UL : 0;
  }
  if (server.hasArg("apssid")) {
    String s = server.arg("apssid"); s.trim();
    if (s.length() > 0 && s != String(cfg_ap_ssid)) {
      s.toCharArray(cfg_ap_ssid, sizeof(cfg_ap_ssid));
      needsRestart = true;
    }
  }
  if (server.hasArg("appass")) {
    String pass = server.arg("appass");
    if ((pass.length() == 0 || pass.length() >= 8) && pass != String(cfg_ap_pass)) {
      pass.toCharArray(cfg_ap_pass, sizeof(cfg_ap_pass));
      needsRestart = true;
    }
  }
  saveSettings();
  String loc = "/?tab=cfg&saved=1";
  if (needsRestart) loc += "&restart=1";
  server.sendHeader("Location", loc);
  server.send(303);
}

// ── System-Handler ─────────────────────────────────────────
void handleSleep() {
  server.send(200, "text/html; charset=utf-8",
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>body{background:#0a0a0a;color:#aaa;font-family:Arial,sans-serif;"
    "display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}"
    "p{font-size:1.1em}</style></head><body>"
    "<p>&#128274; Ger&auml;t schaltet ab...</p></body></html>");
  delay(400);
  goToSleep();
}

void handleRestart() {
  server.send(200, "text/html; charset=utf-8",
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta http-equiv='refresh' content='6;url=/'>"
    "<style>body{background:#0a0a0a;color:#aaa;font-family:Arial,sans-serif;"
    "display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}"
    "p{font-size:1.1em}</style></head><body>"
    "<p>&#128260; Neustart...<br><small style='color:#333'>Seite l&auml;dt in 6 s</small></p>"
    "</body></html>");
  delay(400);
  ESP.restart();
}

void handleManualPing() {
  loRaSend("PNG");
  server.send(200, "application/json", "{\"sent\":true}");
}

void handleExport() {
  uint8_t sortIdx[MAX_HISTORY]; uint8_t validCnt = 0;
  for (uint8_t i = 0; i < historyCnt; i++) { if (history[i] > 0) sortIdx[validCnt++] = i; }
  for (uint8_t i = 1; i < validCnt; i++) {
    uint8_t key = sortIdx[i]; int8_t j = i - 1;
    while (j >= 0 && history[sortIdx[j]] > history[key]) { sortIdx[j+1] = sortIdx[j]; j--; }
    sortIdx[j+1] = key;
  }
  String csv = "Rang,Name,Zeit_ms,Zeit,Datum\r\n";
  for (uint8_t rank = 0; rank < validCnt; rank++) {
    uint8_t i = sortIdx[rank];
    char tbuf[12]; fmtTime(history[i], tbuf);
    String nm = String(historyNames[i]); nm.replace(",", ";"); nm.replace("\r", ""); nm.replace("\n", " ");
    char dateBuf[20] = "";
    if (historyTimestamp[i] > 0) {
      time_t t2 = (time_t)(historyTimestamp[i] / 1000LL);
      struct tm* tm2 = gmtime(&t2);
      snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d.%04d %02d:%02d",
               tm2->tm_mday, tm2->tm_mon + 1, tm2->tm_year + 1900, tm2->tm_hour, tm2->tm_min);
    }
    csv += String(rank + 1) + "," + nm + "," + String(history[i]) + "," + String(tbuf) + "," + String(dateBuf) + "\r\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=\"ziel_luft_history.csv\"");
  server.send(200, "text/csv; charset=utf-8", csv);
}

void handleReset() {
  bestTimeMs = 0; historyCnt = 0; histHead = 0;
  memset(history,           0, sizeof(history));
  memset(historyNames,      0, sizeof(historyNames));
  memset(historyTimestamp,  0, sizeof(historyTimestamp));
  currentRiderName[0] = '\0';
  server.sendHeader("Location", "/?tab=hist");
  server.send(303);
}

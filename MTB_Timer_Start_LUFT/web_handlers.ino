void handleCancel() {
  cancelRun();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ── Zeit-Sync ──────────────────────────────────────────────
void handleSetTime() {
  if (server.hasArg("ts")) {
    int64_t rxTs = (int64_t)atoll(server.arg("ts").c_str());
    timeOffsetMs = rxTs - (int64_t)millis();
    timeIsSynced = true;
    lastSyncAt   = millis();
    char tsyBuf[28];
    snprintf(tsyBuf, sizeof(tsyBuf), "TSY:%lld", (long long)nowUnixMs());
    loRaSend(tsyBuf);
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

// ── Duell-Handler ──────────────────────────────────────────
static const char* CSS_DUEL =
  "body{font-family:Arial,sans-serif;background:#0a0a0a;color:#eee;margin:0;padding:16px;max-width:500px}"
  "h1{color:#f0a500;margin:0 0 6px;font-size:1.2em}"
  ".sub{color:#555;font-size:.8em;margin-bottom:16px}"
  ".lb{color:#555;font-size:.72em;text-transform:uppercase;letter-spacing:.07em;margin-bottom:8px}"
  "input[type=text]{width:100%;background:#111;color:#eee;border:1px solid #2a2a2a;"
  "border-radius:8px;padding:10px;font-size:1em;box-sizing:border-box;outline:none}"
  "input:focus{border-color:#f0a500}"
  ".hint{color:#333;font-size:.75em;margin-top:6px}"
  ".row{display:flex;gap:10px;margin-top:14px}"
  ".ok{flex:1;background:#f0a500;color:#000;border:none;border-radius:8px;padding:12px;"
  "font-weight:bold;font-size:1em;cursor:pointer;text-align:center;text-decoration:none;"
  "display:flex;align-items:center;justify-content:center}"
  ".back{flex:1;background:#1a1a1a;color:#777;border:1px solid #232323;border-radius:8px;"
  "padding:12px;text-decoration:none;text-align:center;font-size:1em}"
  ".list{list-style:none;padding:0;margin:0 0 14px}"
  ".list li{padding:8px 4px;border-bottom:1px solid #1a1a1a;font-size:.92em}"
  ".list li span{color:#f0a500;margin-right:8px;font-weight:bold}"
  ".cnt{display:flex;flex-wrap:wrap;gap:10px;margin-top:10px}"
  ".cnum{width:44px;height:44px;border-radius:8px;background:#161616;border:2px solid #2a2a2a;"
  "color:#eee;font-size:1.1em;font-weight:bold;cursor:pointer;display:flex;align-items:center;"
  "justify-content:center;text-decoration:none}"
  ".cnum:hover{border-color:#f0a500;color:#f0a500}"
  ".prog{background:#1a1a1a;border-radius:4px;height:6px;margin-bottom:14px}"
  ".progb{background:#f0a500;border-radius:4px;height:6px}";

static String duelPageHead(const char* title) {
  String h = "<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>"; h += title; h += "</title><style>";
  h += CSS_DUEL;
  h += "</style></head><body>";
  return h;
}

void handleDuelPage() {
  String html = duelPageHead("Duell einrichten");
  html += "<h1>&#127944; Duell-Modus</h1>"
    "<div class='sub'>Schritt 1 von 3 &ndash; Anzahl der Fahrer</div>"
    "<div class='lb'>Wie viele Fahrer?</div>"
    "<div class='cnt'>";
  for (int n = 2; n <= DUEL_MAX_RIDERS; n++)
    html += "<a class='cnum' href='/duelcount?n=" + String(n) + "'>" + String(n) + "</a>";
  html += "</div>"
    "<div class='row'><a class='back' href='/'>&#8592; Abbrechen</a></div>"
    "</body></html>";
  server.send(200, "text/html", html);
}

void handleDuelCount() {
  int n = server.arg("n").toInt();
  if (n < 1) n = 1;
  if (n > DUEL_MAX_RIDERS) n = DUEL_MAX_RIDERS;
  duelSetupCount = (uint8_t)n;
  duelSetupStep  = 0;
  duelCount = 0;
  memset(duelRiders, 0, sizeof(duelRiders));
  server.sendHeader("Location", "/duelname");
  server.send(303);
}

void handleDuelName() {
  if (duelSetupCount == 0) { server.sendHeader("Location", "/duel"); server.send(303); return; }
  int pct = (duelSetupCount > 0) ? (int)((duelSetupStep * 100) / duelSetupCount) : 0;
  String html = duelPageHead("Fahrername");
  html += "<h1>&#127944; Duell-Modus</h1>";
  html += "<div class='sub'>Schritt 2 von 3 &ndash; Fahrer " + String(duelSetupStep + 1) +
    " von " + String(duelSetupCount) + "</div>";
  html += "<div class='prog'><div class='progb' style='width:" + String(pct) + "%'></div></div>";
  html += "<form action='/duelnext' method='GET'>"
    "<div class='lb'>Name</div>"
    "<input type='text' name='name' maxlength='" + String(NAME_MAX_LEN) + "' autofocus "
    "placeholder='Fahrername...' autocomplete='off'>"
    "<div class='hint'>Max. " + String(NAME_MAX_LEN) + " Zeichen</div>"
    "<div class='row'>"
    "<a class='back' href='/duel'>&#8592; Neu starten</a>"
    "<button type='submit' class='ok'>Weiter &#9658;</button>"
    "</div></form></body></html>";
  server.send(200, "text/html", html);
}

void handleDuelNext() {
  String name = server.arg("name");
  name.trim();
  if (name.length() == 0) name = "Fahrer " + String(duelSetupStep + 1);
  if ((int)name.length() > NAME_MAX_LEN) name = name.substring(0, NAME_MAX_LEN);
  strncpy(duelRiders[duelSetupStep], name.c_str(), NAME_MAX_LEN);
  duelRiders[duelSetupStep][NAME_MAX_LEN] = '\0';
  duelSetupStep++;
  if (duelSetupStep < duelSetupCount)
    server.sendHeader("Location", "/duelname");
  else
    server.sendHeader("Location", "/duelconfirm");
  server.send(303);
}

void handleDuelConfirm() {
  String html = duelPageHead("Duell bestätigen");
  html += "<h1>&#127944; Duell-Modus</h1>"
    "<div class='sub'>Schritt 3 von 3 &ndash; Startreihenfolge pr&uuml;fen</div>"
    "<ul class='list'>";
  for (uint8_t i = 0; i < duelSetupCount; i++)
    html += "<li><span>" + String(i + 1) + ".</span>" +
      (strlen(duelRiders[i]) > 0 ? htmlEsc(duelRiders[i]) : String("&mdash;")) + "</li>";
  html += "</ul>"
    "<div class='row'>"
    "<a class='back' href='/duel'>&#8592; Neu starten</a>"
    "<a class='ok' href='/duelgo'>&#9658; Starten</a>"
    "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleDuelGo() {
  duelMode     = true;
  duelDone     = false;
  duelCurrent  = 0;
  duelCount    = duelSetupCount;
  duelStartIdx = histHead;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDuelExit() {
  duelMode = duelDone = false;
  duelCount = duelCurrent = duelStartIdx = 0;
  duelSetupCount = duelSetupStep = 0;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDuelSkip() {
  if (duelMode && appState == IDLE && duelCurrent < duelCount) {
    char dnfName[NAME_MAX_LEN + 1];
    snprintf(dnfName, sizeof(dnfName), "%.16s DNF", duelRiders[duelCurrent]);
    addHistory(0, dnfName);
    duelCurrent++;
    if (duelCurrent >= duelCount) { duelMode = false; duelDone = true; }
    drawDisplay();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ── Versetzter Start-Modus ─────────────────────────────────
static String stagPageHead(const char* title) {
  return String("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>") + title + "</title>"
    "<style>body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;"
    "background:#0a0a0a;color:#eee;padding:20px;max-width:420px}"
    "h1{color:#f0a500;margin:0 0 4px;font-size:1.2em}"
    ".sub{color:#555;font-size:.8em;margin-bottom:16px}"
    ".lb{color:#888;font-size:.75em;text-transform:uppercase;margin-bottom:8px}"
    ".cnt{display:flex;flex-wrap:wrap;gap:10px;margin-bottom:16px}"
    ".cnum{background:#1c1c1e;color:#f0a500;border:1px solid #333;border-radius:12px;"
    "padding:14px 20px;text-decoration:none;font-size:1.1em;font-weight:bold}"
    "input[type=text]{background:#1c1c1e;border:1px solid #333;border-radius:12px;"
    "color:#eee;padding:12px 16px;font-size:1em;width:100%;box-sizing:border-box;margin:4px 0 8px}"
    ".hint{color:#555;font-size:.75em;margin-bottom:16px}"
    ".prog{background:#1c1c1e;border-radius:8px;height:6px;margin-bottom:16px}"
    ".progb{background:#f0a500;border-radius:8px;height:6px}"
    ".row{display:flex;justify-content:space-between;align-items:center;margin-top:16px}"
    ".back{color:#888;text-decoration:none;font-size:.9em}"
    ".ok{background:#f0a500;color:#000;border:none;border-radius:12px;"
    "padding:12px 24px;font-size:1em;font-weight:bold;cursor:pointer;text-decoration:none}"
    "ul.list{list-style:none;padding:0;margin:0 0 16px}"
    "ul.list li{background:#1c1c1e;border-radius:10px;padding:10px 14px;margin-bottom:6px;"
    "display:flex;align-items:center;gap:10px}"
    "ul.list li span{color:#555;font-size:.8em;min-width:20px}"
    ".info{background:#1c1c1e;border-radius:12px;padding:12px 14px;margin-bottom:12px;"
    "font-size:.85em;color:#888}"
    ".inf2{color:#f0a500;font-weight:bold}</style></head><body>";
}

void handleStagPage() {
  String html = stagPageHead("Versetzter Start");
  html += "<h1>&#9201; Versetzter Start</h1>"
    "<div class='sub'>Schritt 1 von 3 &ndash; Anzahl der Fahrer</div>"
    "<div class='info'>Offset: <span class='inf2'>" + String(cfg_stag_offset_s) +
    " s</span> (einstellbar in Einst. &rarr; LoRa)</div>"
    "<div class='lb'>Wie viele Fahrer?</div><div class='cnt'>";
  for (int n = 2; n <= DUEL_MAX_RIDERS; n++)
    html += "<a class='cnum' href='/stagcount?n=" + String(n) + "'>" + String(n) + "</a>";
  html += "</div><div class='row'><a class='back' href='/'>&#8592; Abbrechen</a></div></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleStagCount() {
  int n = server.arg("n").toInt();
  if (n < 2) n = 2; if (n > DUEL_MAX_RIDERS) n = DUEL_MAX_RIDERS;
  stagSetupCount = (uint8_t)n; stagSetupStep = 0;
  memset(stagRiders, 0, sizeof(stagRiders));
  server.sendHeader("Location", "/stagname"); server.send(303);
}

void handleStagName() {
  if (stagSetupCount == 0) { server.sendHeader("Location", "/stag"); server.send(303); return; }
  int pct = (int)((stagSetupStep * 100) / stagSetupCount);
  String html = stagPageHead("Fahrername");
  html += "<h1>&#9201; Versetzter Start</h1><div class='sub'>Schritt 2 von 3 &ndash; Fahrer " +
    String(stagSetupStep + 1) + " von " + String(stagSetupCount) + "</div>";
  html += "<div class='prog'><div class='progb' style='width:" + String(pct) + "%'></div></div>";
  html += "<form action='/stagnext' method='GET'><div class='lb'>Name</div>"
    "<input type='text' name='name' maxlength='" + String(NAME_MAX_LEN) +
    "' autofocus placeholder='Fahrername...' autocomplete='off'>"
    "<div class='hint'>Max. " + String(NAME_MAX_LEN) + " Zeichen</div>"
    "<div class='row'><a class='back' href='/stag'>&#8592; Neu starten</a>"
    "<button type='submit' class='ok'>Weiter &#9658;</button></div></form></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleStagNext() {
  String name = server.arg("name"); name.trim();
  if (name.length() == 0) name = "Fahrer " + String(stagSetupStep + 1);
  if ((int)name.length() > NAME_MAX_LEN) name = name.substring(0, NAME_MAX_LEN);
  strncpy(stagRiders[stagSetupStep], name.c_str(), NAME_MAX_LEN);
  stagRiders[stagSetupStep][NAME_MAX_LEN] = '\0';
  stagSetupStep++;
  server.sendHeader("Location", stagSetupStep < stagSetupCount ? "/stagname" : "/stagconfirm");
  server.send(303);
}

void handleStagConfirm() {
  String html = stagPageHead("Versetzt bestätigen");
  html += "<h1>&#9201; Versetzter Start</h1>"
    "<div class='sub'>Schritt 3 von 3 &ndash; Startreihenfolge pr&uuml;fen</div>"
    "<div class='info'>Offset: <span class='inf2'>" + String(cfg_stag_offset_s) + " s</span></div>"
    "<ul class='list'>";
  for (uint8_t i = 0; i < stagSetupCount; i++)
    html += "<li><span>" + String(i + 1) + ".</span>" +
      (strlen(stagRiders[i]) > 0 ? htmlEsc(stagRiders[i]) : String("&mdash;")) + "</li>";
  html += "</ul><div class='row'><a class='back' href='/stag'>&#8592; Neu starten</a>"
    "<a class='ok' href='/staggo'>&#9658; Starten</a></div></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleStagGo() {
  stagCount = stagSetupCount;
  stagStarted = 0; stagFinished = 0;
  stagFinishedMask = 0; stagLastStartMs = 0;
  stagArmedFor = 0xFF; stagRearmIdx = 0xFF; stagRearmAt = 0;
  memset(stagStartMs, 0, sizeof(stagStartMs));
  memset(stagTimes,   0, sizeof(stagTimes));
  stagMode = true; stagDone = false;
  if (appState == RUNNING) { appState = IDLE; loRaSend("CAN"); }
  server.sendHeader("Location", "/?tab=modes"); server.send(303);
}

void handleStagExit() {
  stagMode = false; stagDone = false;
  stagCount = 0; stagStarted = 0; stagFinished = 0;
  stagFinishedMask = 0; stagRearmIdx = 0xFF; stagRearmAt = 0;
  if (appState == RUNNING) { appState = IDLE; loRaSend("CAN"); }
  drawDisplay();
  server.sendHeader("Location", "/?tab=modes"); server.send(303);
}

// ── Runden-Modus Handler ───────────────────────────────────
void handleLapStart() {
  if (appState == IDLE && !duelMode && !duelDone) {
    lapMode       = true;
    lapRoundNum   = 0;
    lapRoundStart = 0;
    lapLastMs     = 0;
    lapBestMs     = 0;
    plateFlag     = false;
    appState      = LAP_IDLE;
    drawDisplay();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLapStop() {
  if (appState == LAP_RUNNING) {
    unsigned long lapMs = millis() - lapRoundStart;
    char rndName[6]; snprintf(rndName, sizeof(rndName), "R%u", lapRoundNum);
    addHistory(lapMs, rndName);
    if (lapBestMs == 0 || lapMs < lapBestMs) lapBestMs = lapMs;
    lapLastMs  = lapMs;
    lastTimeMs = lapMs;
    if (bestTimeMs == 0 || lapMs < bestTimeMs) bestTimeMs = lapMs;
    lapMode  = false;
    appState = RESULT;
    resultAt = millis();
    currentPage = 0;
    drawDisplay();
  } else if (appState == LAP_IDLE) {
    lapMode  = false;
    appState = IDLE;
    drawDisplay();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLapReset() {
  lapMode       = true;
  lapRoundNum   = 0;
  lapRoundStart = 0;
  lapLastMs     = 0;
  lapBestMs     = 0;
  bestTimeMs    = 0;
  historyCnt    = 0;
  histHead      = 0;
  plateFlag     = false;
  memset(history,           0, sizeof(history));
  memset(historyNames,      0, sizeof(historyNames));
  memset(historyTimestamp,  0, sizeof(historyTimestamp));
  appState = LAP_IDLE;
  drawDisplay();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ── Settings: NVS lesen/schreiben (nur bei Änderung) ───────
void saveSettings() {
  prefs.begin("mtb-cfg-lu", false);
  if (prefs.getUInt("debounce",  500)    != cfg_debounce_ms)           prefs.putUInt("debounce",  cfg_debounce_ms);
  if (prefs.getUInt("result",    8000)   != cfg_result_show_ms)        prefs.putUInt("result",    cfg_result_show_ms);
  if (prefs.getUInt("timeout",   300000) != cfg_run_timeout_ms)        prefs.putUInt("timeout",   cfg_run_timeout_ms);
  if (prefs.getUInt("ping",      30000)  != cfg_ping_ms)               prefs.putUInt("ping",      cfg_ping_ms);
  if (prefs.getUInt("loracomp",  0)      != cfg_lora_comp_ms)          prefs.putUInt("loracomp",  cfg_lora_comp_ms);
  if (prefs.getUInt("batmah",    1100)   != cfg_bat_mah)               prefs.putUInt("batmah",    cfg_bat_mah);
  if (prefs.getUInt("bmpthresh", 80)     != cfg_pressure_threshold_pa) prefs.putUInt("bmpthresh", cfg_pressure_threshold_pa);
  if (prefs.getUInt("bmpcaldly", 3000)   != cfg_bmp_cal_delay_ms)      prefs.putUInt("bmpcaldly", cfg_bmp_cal_delay_ms);
  if (prefs.getUChar("contrast", 255)    != cfg_contrast)              prefs.putUChar("contrast", cfg_contrast);
  if (prefs.getUChar("lorapwr",  14)     != cfg_lora_pwr)              prefs.putUChar("lorapwr",  cfg_lora_pwr);
  if (prefs.getUChar("stagoffset", 30)   != cfg_stag_offset_s)         prefs.putUChar("stagoffset", cfg_stag_offset_s);
  if (prefs.getUChar("btn2pin",  255)    != cfg_btn2_pin)              prefs.putUChar("btn2pin",  cfg_btn2_pin);
  if (prefs.getUInt("autopage",  0)      != cfg_page_auto_ms)          prefs.putUInt("autopage",  cfg_page_auto_ms);
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
  if (server.hasArg("ping"))
    cfg_ping_ms = (uint32_t)(constrain(server.arg("ping").toInt(), 5, 300) * 1000);
  if (server.hasArg("loracomp"))
    cfg_lora_comp_ms = (uint32_t)constrain(server.arg("loracomp").toInt(), 0, 500);
  if (server.hasArg("lorapwr")) {
    cfg_lora_pwr = (uint8_t)constrain(server.arg("lorapwr").toInt(), 2, 20);
    radio.setOutputPower(cfg_lora_pwr);
  }
  if (server.hasArg("stagoffset"))
    cfg_stag_offset_s = (uint8_t)constrain(server.arg("stagoffset").toInt(), 5, 255);
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
  bool sent = (appState == IDLE || appState == LAP_IDLE);
  if (sent) { lastPingAt = millis(); loRaSend("PNG"); }
  server.send(200, "application/json", sent ? "{\"sent\":true}" : "{\"sent\":false}");
}

void handleExport() {
  uint8_t sortIdx[MAX_HISTORY];
  uint8_t validCnt = 0;
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
  for (uint8_t i = 0; i < historyCnt; i++) {
    if (history[i] == 0) {
      String nm = String(historyNames[i]); nm.replace(",", ";"); nm.replace("\r", ""); nm.replace("\n", " ");
      csv += "DNF," + nm + ",0,DNF,\r\n";
    }
  }
  server.sendHeader("Content-Disposition", "attachment; filename=\"start_luft_history.csv\"");
  server.send(200, "text/csv; charset=utf-8", csv);
}

void handleReset() {
  bestTimeMs = 0; historyCnt = 0; histHead = 0;
  memset(history,           0, sizeof(history));
  memset(historyNames,      0, sizeof(historyNames));
  memset(historyTimestamp,  0, sizeof(historyTimestamp));
  duelMode = duelDone = false;
  duelCount = duelCurrent = duelStartIdx = 0;
  duelSetupCount = duelSetupStep = 0;
  lapMode = false; lapRoundNum = 0; lapRoundStart = 0; lapLastMs = 0; lapBestMs = 0;
  server.sendHeader("Location", "/?tab=hist");
  server.send(303);
}

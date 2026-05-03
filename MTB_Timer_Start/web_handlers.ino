void handleCancel() {
  cancelRun();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ── Zeit-Sync ────────────────────────────────────────��─────
void handleSetTime() {
  if (server.hasArg("ts")) {
    double tsDouble   = server.arg("ts").toDouble();
    timeOffsetMs      = (int64_t)tsDouble - (int64_t)millis();
    timeIsSynced      = true;
    lastSyncAt        = millis();
    // Zeitstempel an Finish/Split broadcasten
    char tsyBuf[28];
    snprintf(tsyBuf, sizeof(tsyBuf), "TSY:%lld", (long long)nowUnixMs());
    loRaSend(tsyBuf);
  }
  server.send(204, "text/plain", "");
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
  duelStartIdx = histHead;   // physischer Ringpuffer-Index des ersten Duell-Eintrags
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
    Serial.println("[Start] Runden-Modus aktiviert.");
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
    char tbuf[12]; fmtTime(lapMs, tbuf);
    Serial.print("[Start] LAP Stopp. Letzte Runde: "); Serial.println(tbuf);
  } else if (appState == LAP_IDLE) {
    lapMode  = false;
    appState = IDLE;
    drawDisplay();
    Serial.println("[Start] Runden-Modus beendet.");
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
  plateFlag     = false;
  memset(history,           0, sizeof(history));
  memset(historyNames,      0, sizeof(historyNames));
  memset(historyTimestamp,  0, sizeof(historyTimestamp));
  appState = LAP_IDLE;
  drawDisplay();
  Serial.println("[Start] LAP Reset – LAP_IDLE.");
  server.sendHeader("Location", "/");
  server.send(303);
}

// ── Settings: NVS lesen/schreiben (nur bei Änderung) ───────
// Jeder NVS-Schreibzugriff verbraucht Flash-Lebenszyklen.
// Wir lesen den gespeicherten Wert und schreiben nur wenn nötig.
void saveSettings() {
  prefs.begin("mtb-cfg", false);
  // UInt-Werte: nur schreiben wenn geändert
  if (prefs.getUInt("debounce",  500)     != cfg_debounce_ms)    prefs.putUInt("debounce",  cfg_debounce_ms);
  if (prefs.getUInt("result",    8000)    != cfg_result_show_ms) prefs.putUInt("result",    cfg_result_show_ms);
  if (prefs.getUInt("timeout",   300000)  != cfg_run_timeout_ms) prefs.putUInt("timeout",   cfg_run_timeout_ms);
  if (prefs.getUInt("ping",      30000)   != cfg_ping_ms)        prefs.putUInt("ping",      cfg_ping_ms);
  if (prefs.getUInt("loracomp",  0)       != cfg_lora_comp_ms)   prefs.putUInt("loracomp",  cfg_lora_comp_ms);
  if (prefs.getUInt("batmah",    1100)    != cfg_bat_mah)        prefs.putUInt("batmah",    cfg_bat_mah);
  // UChar-Werte
  if (prefs.getUChar("contrast", 255)     != cfg_contrast)       prefs.putUChar("contrast", cfg_contrast);
  if (prefs.getUChar("platepin", PLATE_PIN)!= cfg_plate_pin)     prefs.putUChar("platepin", cfg_plate_pin);
  // Bool
  if (prefs.getBool("platenc",   false)   != cfg_plate_nc)       prefs.putBool("platenc",   cfg_plate_nc);
  if (prefs.getUChar("lorapwr", 14)      != cfg_lora_pwr)        prefs.putUChar("lorapwr",  cfg_lora_pwr);
  if (prefs.getUChar("btn2pin", 255)     != cfg_btn2_pin)        prefs.putUChar("btn2pin",  cfg_btn2_pin);
  if (prefs.getUInt("autopage",  0)      != cfg_page_auto_ms)    prefs.putUInt("autopage",  cfg_page_auto_ms);
  // Strings: immer schreiben (Vergleich wäre aufwendiger als Schreiben)
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
  if (server.hasArg("batmah"))
    cfg_bat_mah = (uint32_t)constrain(server.arg("batmah").toInt(), 100, 10000);
  if (server.hasArg("contrast")) {
    cfg_contrast = (uint8_t)server.arg("contrast").toInt();
    u8g2.setContrast(cfg_contrast);
  }
  if (server.hasArg("platepin")) {
    uint8_t np = (uint8_t)server.arg("platepin").toInt();
    if (np != cfg_plate_pin) { cfg_plate_pin = np; needsRestart = true; }
  }
  if (server.hasArg("platenc") && !needsRestart) {
    bool newNc = (server.arg("platenc") == "1");
    if (newNc != cfg_plate_nc) {
      cfg_plate_nc = newNc;
      detachInterrupt(digitalPinToInterrupt(cfg_plate_pin));
      attachInterrupt(digitalPinToInterrupt(cfg_plate_pin), onPlate,
                      cfg_plate_nc ? RISING : FALLING);
    }
  } else if (server.hasArg("platenc")) {
    cfg_plate_nc = (server.arg("platenc") == "1");
  }
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

void handleOtaPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OTA Update – START</title>"
    "<style>body{font-family:Arial,sans-serif;background:#0a0a0a;color:#eee;padding:20px;max-width:420px}"
    "h2{color:#f0a500}input[type=file]{width:100%;margin:10px 0;padding:6px}"
    ".btn{display:inline-block;background:#1a3a1a;color:#4caf50;border:none;padding:10px 20px;"
    "border-radius:8px;font-size:1em;cursor:pointer;text-decoration:none;margin-top:8px}"
    ".warn{color:#ff9800;font-size:.85em;margin-top:6px}"
    "progress{width:100%;height:18px;margin-top:8px}</style></head><body>"
    "<h2>&#128640; Firmware-Update (START)</h2>"
    "<p class='warn'>&#9888; Ger&auml;t startet nach dem Update automatisch neu.</p>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<label>Firmware-Datei (.bin):</label>"
    "<input type='file' name='firmware' accept='.bin' required>"
    "<br><button class='btn' type='submit'>&#11014; Hochladen &amp; Flashen</button>"
    "</form>"
    "<br><a href='/'>&#8592; Zur&uuml;ck</a></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleOtaUpload() {
  server.sendHeader("Connection", "close");
  if (Update.hasError()) {
    server.send(200, "text/html; charset=utf-8",
      "<h2 style='color:#f44'>Update fehlgeschlagen!</h2><a href='/update'>Nochmal versuchen</a>");
  } else {
    server.send(200, "text/html; charset=utf-8",
      "<h2 style='color:#4caf50'>Update erfolgreich!</h2><p>Ger&auml;t startet neu...</p>");
  }
  delay(500);
  ESP.restart();
}

void handleOtaStream() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("[OTA] Fertig: %u Bytes\n", upload.totalSize);
    else Update.printError(Serial);
  }
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
  server.sendHeader("Content-Disposition", "attachment; filename=\"start_history.csv\"");
  server.send(200, "text/csv; charset=utf-8", csv);
}

void handleReset() {
  bestTimeMs = 0; historyCnt = 0;
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

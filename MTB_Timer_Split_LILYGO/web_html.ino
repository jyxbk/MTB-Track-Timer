// ── WebServer JSON-State ───────────────────────────────────
String buildState() {
  String j; j.reserve(1536);
  unsigned long liveMs = 0;
  if (appState == ARMED) {
    uint64_t curUs = (uint64_t)esp_timer_get_time();
    liveMs = (unsigned long)((curUs - startRecvUs) / 1000);
  } else if (appState == DONE) {
    liveMs = lastTimeMs;
  }
  unsigned long now   = millis();
  unsigned long since = (loraLastContact > 0) ? (now - loraLastContact) / 1000 : 9999;
  bool peerOk = loraLastContact > 0 && since < PEER_TIMEOUT_S;
  const char* stStr = (appState == IDLE)  ? (peerOk ? "WARTET" : "KEIN SIG.")
                    : (appState == ARMED)  ? "LAEUFT" : "SPLIT-OK";

  j  = "{\"state\":\"";   j += stStr;
  j += "\",\"liveMs\":";  j += liveMs;
  j += ",\"lastMs\":";    j += lastTimeMs;
  j += ",\"bestMs\":";    j += bestTimeMs;
  j += ",\"histCnt\":";   j += historyCnt;
  j += ",\"rssi\":";      j += (int)loraRssi;
  j += ",\"snr\":";       j += String(loraSnr, 1);
  j += ",\"since\":";     j += since;
  j += ",\"bat\":{\"pct\":"; j += batPercent;
  j += ",\"mv\":";            j += (uint32_t)(batVoltage * 1000.0f);
  j += ",\"mah\":";           j += cfg_bat_mah;
  j += "}";
  j += ",\"lora_pwr\":";    j += cfg_lora_pwr;
  j += ",\"btn2_pin\":";    j += cfg_btn2_pin;
  j += ",\"auto_page\":";   j += cfg_page_auto_ms;
  j += ",\"lora\":{\"txOk\":"; j += loraTxCount;
  j += ",\"txFail\":";          j += loraTxFail;
  j += ",\"rxCnt\":";           j += loraRxCount;
  j += ",\"rssiMin\":";
  if (loraHasRx) j += (int)loraRssiMin; else j += "0";
  j += ",\"rssiMax\":";
  if (loraHasRx) j += (int)loraRssiMax; else j += "0";
  j += ",\"freq\":868,\"bw\":125,\"sf\":7,\"cr\":5}";
  { unsigned long sa = (lastSyncAt > 0) ? (now - lastSyncAt) / 1000 : 9999UL;
    j += ",\"timeSynced\":"; j += (timeIsSynced ? "true" : "false");
    j += ",\"syncAgo\":";    j += sa;
    char nowBuf[22]; snprintf(nowBuf, sizeof(nowBuf), "%lld", (long long)nowUnixMs());
    j += ",\"nowMs\":";      j += nowBuf;
  }
  j += ",\"hist\":[";
  for (uint8_t i = 0; i < historyCnt; i++) {
    if (i) j += ",";
    uint8_t pi = histPhys(i);
    String n = String(historyNames[pi]);
    n.replace("\\", "\\\\"); n.replace("\"", "\\\"");
    n.replace("\n", "\\n");  n.replace("\r", "\\r");
    char tsBuf[22]; snprintf(tsBuf, sizeof(tsBuf), "%lld", (long long)historyTimestamp[pi]);
    j += "{\"ms\":"; j += history[pi];
    j += ",\"n\":\""; j += n;
    j += "\",\"ts\":"; j += tsBuf;
    j += "}";
  }
  j += "]";
  j += ",\"sdPresent\":"; j += sdPresent ? "true" : "false";
  j += "}";
  return j;
}

void handleState() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", buildState());
}

// ── WebServer HTML ─────────────────────────────────────────
String buildHTML() {
  char buf[20];
  String html;
  html.reserve(10240);

  char upBuf[12]; fmtUptime(upBuf);
  unsigned long now   = millis();
  unsigned long since = (loraLastContact > 0) ? (now - loraLastContact) / 1000 : 9999;
  bool peerOk = loraLastContact > 0 && since < PEER_TIMEOUT_S;
  const char* stStr = (appState == IDLE)  ? (peerOk ? "WARTET" : "KEIN SIG.")
                    : (appState == ARMED)  ? "LAEUFT" : "SPLIT-OK";
  const char* stCls = (appState == IDLE)  ? (peerOk ? "si" : "se")
                    : (appState == ARMED)  ? "sr" : "sd";

  unsigned long liveMs = 0;
  if (appState == ARMED) {
    uint64_t curUs = (uint64_t)esp_timer_get_time();
    liveMs = (unsigned long)((curUs - startRecvUs) / 1000);
  } else if (appState == DONE) {
    liveMs = lastTimeMs;
  }
  const char* lqCls = (loraLastContact == 0) ? "e" : (since < 35) ? "ok" : (since < 70) ? "w" : "e";
  const char* lqTxt = (loraLastContact == 0) ? "Kein Kontakt" : (since < 35) ? "Verbunden" : (since < 70) ? "Schwach" : "Getrennt";

  int bars = 0;
  if (loraLastContact > 0 && since < 300) {
    if      (loraRssi >= -65) bars = 5;
    else if (loraRssi >= -75) bars = 4;
    else if (loraRssi >= -85) bars = 3;
    else if (loraRssi >= -95) bars = 2;
    else                      bars = 1;
  }

  if (lastTimeMs > 0) fmtTime(lastTimeMs, buf); else strcpy(buf, "--:--.---");
  String sLast(buf);
  if (bestTimeMs > 0) fmtTime(bestTimeMs, buf); else strcpy(buf, "--:--.---");
  String sBest(buf);

  html = "<!DOCTYPE html><html lang='de'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<script>var _P=Date.now(),_S='" + String(stStr) + "',_B=" + String(liveMs) +
    ",_H=" + String(historyCnt) + ";</script>"
    "<title>MTB SPLIT</title><style>"
    ":root{--bg:#0a0a0a;--card:#1c1c1e;--card2:#2a2a2c;--txt:#eee;--sub:#555;--sub2:#383838;"
    "--acc:#f0a500;--acc-txt:#000;--ok:#4caf50;--ok-bg:#0d1f0d;--err:#f44336;--err-bg:#200d0d;"
    "--border:#2a2a2a;--border2:#1c1c1e;--blue:#64b5f6;--blue-bg:#0d1525}"
    "body.light{--bg:#f2f2f7;--card:#fff;--card2:#e5e5ea;--txt:#1c1c1e;--sub:#8e8e93;"
    "--sub2:#c7c7cc;--acc:#c87800;--acc-txt:#fff;--ok:#1a7a3a;--ok-bg:#d4edda;"
    "--err:#c0392b;--err-bg:#fde8e8;--border:#d1d1d6;--border2:#e5e5ea}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;background:var(--bg);color:var(--txt);margin:0;padding:16px;padding-bottom:80px;max-width:500px}"
    "h1{color:var(--acc);margin:0 0 12px;font-size:1.3em}"
    ".c{background:var(--card);border-radius:16px;padding:16px;margin-bottom:12px}"
    ".lb{color:var(--sub);font-size:.72em;text-transform:uppercase;letter-spacing:.07em;margin-bottom:5px}"
    ".row{display:flex;justify-content:space-between;align-items:center}"
    ".bdg{padding:4px 14px;border-radius:20px;font-weight:bold;font-size:.82em}"
    ".si{background:var(--ok-bg);color:var(--ok)}.sr{background:#221500;color:#ff9800}.sd{background:var(--blue-bg);color:var(--blue)}.se{background:var(--err-bg);color:var(--err)}"
    ".batw{color:#ffcc00}.batc{color:#ff4444;font-weight:bold}"
    ".big{font-size:2.8em;font-weight:bold;font-variant-numeric:tabular-nums;letter-spacing:.02em;line-height:1.15}"
    ".bc{color:var(--acc)}"
    ".half{display:flex;gap:12px}.half .c{flex:1;margin-bottom:0}"
    ".md{font-size:1.5em;font-weight:bold;font-variant-numeric:tabular-nums}"
    ".sk{display:inline-flex;align-items:flex-end;gap:3px;height:16px;vertical-align:middle}"
    ".sb{width:5px;border-radius:2px 2px 0 0}"
    ".on{background:var(--ok)}.off{background:#252525}body.light .off{background:#ccc}"
    ".ok{color:var(--ok)}.w{color:#ff9800}.e{color:var(--err)}"
    ".sm{color:var(--sub);font-size:.78em}"
    "table{width:100%;border-collapse:collapse;margin-top:6px}"
    "th{color:#444;font-size:.7em;text-transform:uppercase;letter-spacing:.06em;padding:3px 0 6px;border-bottom:1px solid var(--border)}"
    "td{padding:6px 2px;border-bottom:1px solid var(--border2);font-size:.88em}"
    ".rk{color:#333;width:22px}"
    ".nm{color:#aaa;max-width:90px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
    ".dt{color:var(--sub)}"
    ".r1 td{color:var(--acc);font-weight:bold}.r2 td{color:#ccc}.r3 td{color:#cd7f32}"
    "body.light .r2 td{color:#444}"
    ".btn{display:inline-block;background:var(--card2);color:#888;padding:12px 20px;border-radius:12px;"
    "text-decoration:none;font-size:.85em;border:none;margin-top:10px}"
    ".cbtn{background:var(--err-bg);color:var(--err);display:block;text-align:center;margin-bottom:10px}"
    ".ld{display:inline-block;width:7px;height:7px;border-radius:50%;margin-left:6px;vertical-align:middle;transition:background .4s}"
    ".ld.on{background:var(--ok)}.ld.off{background:var(--err)}"
    "@keyframes fl{from{background:#0d2a0d}to{background:transparent}}"
    ".new-row{animation:fl 1.8s ease-out}"
    ".tab-pane{display:none}.tab-pane.act{display:block}"
    ".bnav{position:fixed;bottom:0;left:0;right:0;background:rgba(18,18,18,.97);"
    "backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);"
    "border-top:1px solid var(--border);display:flex;padding:6px 0 env(safe-area-inset-bottom,6px);z-index:100}"
    "body.light .bnav{background:rgba(242,242,247,.97)}"
    ".bni{flex:1;display:flex;flex-direction:column;align-items:center;gap:3px;"
    "background:none;border:none;color:var(--sub);font-size:.62em;padding:6px 2px;"
    "cursor:pointer;font-family:inherit;transition:color .15s;line-height:1.2}"
    ".bni.active{color:var(--acc)}"
    ".bni svg{width:22px;height:22px;fill:none;stroke:currentColor;stroke-width:1.8;"
    "stroke-linecap:round;stroke-linejoin:round}"
    ".sg{background:var(--card);border-radius:16px;margin-bottom:12px;overflow:hidden}"
    ".sr2{display:flex;flex-direction:column;padding:10px 16px;border-bottom:1px solid var(--card2)}"
    ".sr2:last-child{border-bottom:none}"
    ".sl{display:flex;justify-content:space-between;align-items:center}"
    ".slb{font-size:.88em;color:var(--txt)}"
    ".sg input[type=number],.sg input[type=text],.sg input[type=password]{"
    "background:transparent;border:none;color:var(--acc);font-size:.9em;"
    "text-align:right;width:90px;outline:none;font-family:inherit}"
    ".sg input[type=range]{width:100%;accent-color:var(--acc);margin-top:8px}"
    ".sg input[type=radio]{accent-color:var(--acc)}"
    ".sg .srow{display:flex;gap:16px;margin-top:6px}"
    ".sg .srow label{display:flex;align-items:center;gap:6px;color:#aaa;font-size:.9em}"
    ".shdr{color:var(--acc);font-size:.8em;font-weight:bold;margin:16px 0 6px;"
    "padding-bottom:4px;border-bottom:1px solid #222}"
    ".sbtn{background:var(--acc);color:var(--acc-txt);border:none;border-radius:14px;padding:14px 20px;"
    "font-weight:bold;font-size:1em;cursor:pointer;margin-top:14px;width:100%}"
    ".hint{color:var(--sub2);font-size:.68em;margin-top:3px;display:block}"
    ".fahr{display:inline-block;background:var(--card2);color:var(--sub);border-radius:4px;font-size:.7em;padding:1px 5px;margin-left:4px;vertical-align:middle}"
    ".toast{display:none;border-radius:12px;padding:10px 14px;margin-bottom:12px;font-size:.85em}"
    ".tok{background:var(--ok-bg);color:var(--ok);border:1px solid #2a4a2a}"
    ".twarn{background:#2a1a0d;color:#ff9800;border:1px solid #4a3a1a}"
    "@keyframes batblink{0%,100%{opacity:1}50%{opacity:.2}}"
    ".batblink{animation:batblink 1s infinite}"
    ".rc-banner{display:none;background:#2a1a00;color:#ff9800;border:1px solid #4a3a1a;border-radius:12px;padding:8px 14px;margin-bottom:12px;font-size:.82em}"
    ".evt-toast{display:none;border-radius:12px;padding:10px 14px;margin-bottom:12px;font-size:.85em;color:#eee}"
    ".dev-row{display:flex;gap:12px;margin-top:10px;font-size:.82em}"
    ".dev-b{display:flex;align-items:center;gap:6px;color:#aaa}"
    ".dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}"
    "@keyframes newbest{0%{color:#fff;text-shadow:0 0 20px #f0a500}100%{color:#f0a500;text-shadow:none}}"
    ".newbest{animation:newbest 1.5s ease-out}"
    "@media(min-width:640px){"
    "body{max-width:640px;padding:20px;padding-bottom:96px}"
    ".bnav{width:640px;left:50%;right:auto;transform:translateX(-50%)}"
    ".bni{padding:8px 4px;font-size:.68em}.bni svg{width:24px;height:24px}}"
    "@media print{.bnav,#bat-warn,#rc-banner,#res-ov,.toast,.evt-toast,"
    "#tab-lora,#tab-cfg{display:none!important}"
    "#tab-hist{display:block!important}"
    "body{background:#fff;color:#000;padding:0}}"
    "</style></head><body>"
    "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:12px'>"
    "<h1 style='margin:0'>&#9201; MTB SPLIT <span id='_ld' class='ld on'></span></h1>"
    "<div style='display:flex;align-items:center;gap:8px'>"
    "<button onclick='_fs()' style='background:none;border:none;color:var(--sub);font-size:1.2em;cursor:pointer;padding:4px'>&#9974;</button>"
    "<button id='theme-btn' onclick='_toggleTheme()' style='background:none;border:1px solid var(--border);"
    "color:var(--sub);border-radius:20px;padding:5px 12px;font-size:.8em;cursor:pointer'>"
    "&#9728; Hell</button>"
    "</div>"
    "</div>";

  html += "<div id='bat-warn' style='display:none;position:fixed;top:0;left:0;right:0;z-index:150;"
    "padding:6px;text-align:center;font-size:.85em;font-weight:bold'></div>"
    "<div id='rc-banner' class='rc-banner'>&#9888; Verbindung unterbrochen &ndash; stelle wieder her...</div>"
    "<div id='evt-toast' class='evt-toast'></div>"
    "<div id='toast-ok' class='toast tok'>&#10003; Gespeichert.</div>"
    "<div id='toast-rs' class='toast twarn'>&#9888; Neustart f&uuml;r WiFi/Pin n&ouml;tig.</div>";

  // ══ TAB: LIVE ══════════════════════════════════════════════
  html += "<div class='tab-pane' id='tab-live'>";

  String sk = "<span class='sk'>";
  int ht[] = {4, 6, 9, 12, 16};
  for (int i = 0; i < 5; i++)
    sk += "<span class='sb " + String(i < bars ? "on" : "off") + "' style='height:" + String(ht[i]) + "px'></span>";
  sk += "</span>";

  html += "<div class='c'><div class='row'>"
    "<div><div class='lb'>Split-Node</div><span id='st' class='bdg " + String(stCls) + "'>" + stStr + "</span></div>"
    "<div style='text-align:right'><div class='lb'>Uptime</div>"
    "<div class='sm'>" + upBuf + " <span id='sync-ind' style='font-size:.75em;"
    + String(timeIsSynced ? "color:#66bb6a'" : "color:#555'") + ">"
    + String(timeIsSynced ? "&#10003; Uhr sync" : "&#9888; Uhr nicht sync") + "</span></div>"
    "</div>"
    "</div><div id='sig' style='margin-top:8px;font-size:.85em'>"
    + sk + " <span class='" + lqCls + "'>" + lqTxt + "</span></div></div>";

  html += "<div id='cwrap'" + String(appState == ARMED ? "" : " style='display:none'") + ">"
    "<a class='btn cbtn' href='/cancel' onclick=\"return confirm('Lauf abbrechen?')\">"
    "&#10005; Lauf abbrechen</a></div>";

  html += "<div class='c'><div class='lb'>Split-Zeit</div><div class='big' id='_T'>";
  if (liveMs > 0) { fmtTime(liveMs, buf); html += buf; } else html += "--:--.---";
  html += "</div></div>";

  html += "<div class='half'>"
    "<div class='c'><div class='lb'>Letzter Split</div><div class='md' id='t-last'>" + sLast + "</div></div>"
    "<div class='c'><div class='lb'>Bester Split</div><div class='md bc' id='t-best'>" + sBest + "</div></div>"
    "</div>";

  html += "</div>"; // tab-live

  // ══ TAB: VERLAUF ═══════════════════════════════════════════
  html += "<div class='tab-pane' id='tab-hist'>";
  html += "<div class='c'>"
    "<div class='row' style='margin-bottom:8px'>"
    "<div class='lb' id='h-hdr'>Verlauf &mdash; " + String(historyCnt) + " Splits</div>"
    "<button id='h-vbtn' onclick='_togHist()' style='background:#1a1a1a;border:1px solid #2a2a2a;"
    "color:#888;border-radius:6px;padding:4px 10px;font-size:.72em;cursor:pointer'>"
    "&#9201; Bestzeit</button></div>";
  html += "<div id='h-tbl'>";
  if (historyCnt == 0) {
    html += "<div style='color:#2a2a2a;padding:10px 0'>Keine Splits.</div>";
  } else {
    uint8_t sortIdx[MAX_HISTORY];
    uint8_t validCnt = 0;
    for (uint8_t i = 0; i < historyCnt; i++) { uint8_t pi=histPhys(i); if (history[pi] > 0) sortIdx[validCnt++] = pi; }
    for (uint8_t i = 1; i < validCnt; i++) {
      uint8_t key = sortIdx[i]; int8_t j = i - 1;
      while (j >= 0 && history[sortIdx[j]] > history[key]) { sortIdx[j+1] = sortIdx[j]; j--; }
      sortIdx[j+1] = key;
    }
    html += "<table><tr><th class='rk'>#</th><th>Zeit</th><th>+Delta</th><th></th></tr>";
    for (uint8_t rank = 0; rank < validCnt; rank++) {
      uint8_t i = sortIdx[rank];
      fmtTime(history[i], buf);
      const char* rc  = (rank == 0) ? "r1" : (rank == 1) ? "r2" : (rank == 2) ? "r3" : "";
      const char* med = (rank == 0) ? "&#127942;" : (rank == 1) ? "&#129352;" : (rank == 2) ? "&#129353;" : "";
      char tsBuf[22]; snprintf(tsBuf, sizeof(tsBuf), "%lld", (long long)historyTimestamp[i]);
      html += "<tr class='"; html += rc; html += "'>";
      html += "<td class='rk'>"; html += String(rank + 1); html += "</td>";
      html += "<td";
      if (historyTimestamp[i] > 0) {
        time_t t2 = (time_t)(historyTimestamp[i] / 1000LL);
        struct tm* tm2 = gmtime(&t2);
        char dtBuf[20];
        snprintf(dtBuf, sizeof(dtBuf), "%02d.%02d.%04d %02d:%02d",
                 tm2->tm_mday, tm2->tm_mon + 1, tm2->tm_year + 1900, tm2->tm_hour, tm2->tm_min);
        html += " title='"; html += dtBuf; html += "'";
      }
      html += " style='cursor:default'>"; html += buf;
      html += "<span class='fahr'>F"; html += String(i + 1); html += "</span></td>";
      if (rank == 0 || bestTimeMs == 0) {
        html += "<td class='dt'>&mdash;</td><td>"; html += med; html += "</td>";
      } else {
        unsigned long d = history[i] - bestTimeMs;
        char dBuf[14];
        if (d < 60000) sprintf(dBuf, "+%lu.%03lu", d / 1000, d % 1000);
        else           sprintf(dBuf, "+%lu:%02lu.%03lu", d / 60000, (d % 60000) / 1000, d % 1000);
        html += "<td class='dt'>"; html += dBuf; html += "s</td><td>"; html += med; html += "</td>";
      }
      html += "</tr>";
    }
    html += "</table>";
  }
  html += "</div>";
  html += "<div style='display:flex;gap:8px'>"
          "<a class='btn' href='/export'>&#128190; CSV</a>"
          "<button onclick='_share()' class='btn'>&#8599; Teilen</button>"
          "<a class='btn' style='background:#2a0a0a' href='/reset' onclick=\"return confirm('Alle Splits l\\u00f6schen?')\">&#128465; Zur&uuml;cksetzen</a>"
          "</div></div>";
  html += "</div>"; // tab-hist

  // ══ TAB: LORA ══════════════════════════════════════════════
  html += "<div class='tab-pane' id='tab-lora'>";
  html += "<div class='c'><div class='row'>"
    "<div><div class='lb'>Verbindung</div>"
    "<span id='lora-q-badge' class='" + String(lqCls) + "'>" + String(lqTxt) + "</span></div>"
    "<div style='text-align:right'><div class='lb'>Letzter Kontakt</div>"
    "<div id='lora-since' class='sm'>" +
    String(loraLastContact == 0 ? "&mdash;" : (String("vor ") + since + " s").c_str()) +
    "</div></div></div>"
    "<div id='lora-sig2' style='margin-top:10px;font-size:.88em'>" + sk + " <span class='" + lqCls + "'>" + lqTxt + "</span></div></div>";

  {
    char rssiStr[10]; sprintf(rssiStr, "%d", (int)loraRssi);
    char snrStr[10];  sprintf(snrStr,  "%.1f", loraSnr);
    html += "<div class='c'><div class='lb'>Empfangsqualit&auml;t</div>"
      "<div id='lora-rssi-val' style='font-size:.95em;line-height:2'>";
    if (loraLastContact > 0) {
      html += "RSSI: <b>" + String(rssiStr) + " dBm</b>"
        " &bull; SNR: <b>" + String(snrStr) + " dB</b>"
        " &bull; <b>" + String(rssiStatus(loraRssi)) + "</b>";
    } else {
      html += "<span class='sm'>Kein Kontakt</span>";
    }
    html += "</div></div>";
  }

  html += "<div class='c'><div class='lb'>Statistik</div>"
    "<div id='lora-stat' style='font-size:.85em;color:#aaa;line-height:1.9'>"
    "TX: " + String(loraTxCount) + " OK &bull; " + String(loraTxFail) + " Fehler<br>"
    "RX: " + String(loraRxCount) + " Pakete";
  if (loraHasRx) {
    char mn[8], mx[8];
    sprintf(mn, "%d", (int)loraRssiMin);
    sprintf(mx, "%d", (int)loraRssiMax);
    html += "<br>RSSI Min/Max: " + String(mn) + " / " + String(mx) + " dBm";
  }
  html += "</div></div>";

  html += "<div class='c'><div class='lb'>LoRa Konfiguration</div>"
    "<div style='font-size:.85em;color:#555;line-height:1.9'>"
    "868 MHz &bull; BW 125 kHz &bull; SF7 &bull; CR 4/5 &bull; 14 dBm TX<br>"
    "Sync Word: 0x12 (privat)"
    "</div></div>";

  {
    unsigned long syncAge4 = (lastSyncAt > 0) ? (millis() - lastSyncAt) / 1000 : 9999UL;
    const char* syncBadgeCls = (timeIsSynced && syncAge4 < 300) ? "ok" : (timeIsSynced ? "w" : "e");
    const char* syncBadgeTxt = (timeIsSynced && syncAge4 < 300) ? "&#10003; Synchronisiert"
                             : (timeIsSynced ? "&#9888; Sync veraltet" : "&#10007; Nicht synchronisiert");
    String syncDetailTxt;
    if (timeIsSynced && syncAge4 < 9999) {
      if (syncAge4 < 60) syncDetailTxt = "Letzte Sync: vor " + String(syncAge4) + " s";
      else               syncDetailTxt = "Letzte Sync: vor " + String(syncAge4 / 60) + " min";
    } else {
      syncDetailTxt = "Browser-Seite &ouml;ffnen zum Synchronisieren";
    }
    html += "<div class='c'><div class='lb'>&#9201; Uhrzeit-Sync</div>"
      "<div style='margin-bottom:8px'><span class='" + String(syncBadgeCls) + "' id='sync-lora-badge'>" + syncBadgeTxt + "</span></div>"
      "<div id='sync-lora-detail' style='font-size:.82em;color:#555;line-height:1.8'>" + syncDetailTxt + "</div>"
      "<button onclick='doSync()' class='btn' style='margin-top:10px;width:100%'>&#8635; Jetzt synchronisieren</button>"
      "</div>";
  }
  html += "<div class='c'><div class='lb'>Verbindungstest</div>"
    "<button id='ping-btn' onclick='doPing()' style='background:#111;border:1px solid #1f1f1f;color:#64b5f6;"
    "border-radius:8px;padding:10px 18px;font-size:.85em;cursor:pointer;margin-top:4px'>&#9654; Ping senden</button>"
    "<span id='ping-res' style='color:#555;font-size:.82em;margin-left:10px'></span></div>";

  html += "</div>"; // tab-lora

  // ══ TAB: SETTINGS ══════════════════════════════════════════
  html += "<div class='tab-pane' id='tab-cfg'>";
  html += "<div class='c'><div class='lb'>&#128190; SD-Karte</div>"
    "<div id='sd-badge'>" + String(sdPresent ? "<span style='color:#4caf50'>&#10003; Eingelegt</span>" : "<span style='color:#555'>Nicht eingelegt</span>") + "</div></div>";
  html += "<div class='c'><div class='lb'>&#128267; Batterie</div><div id='bat-val'>";
  if (batVoltage > 2.0f) {
    char vBuf[8]; sprintf(vBuf, "%.2f", batVoltage);
    uint32_t rem = (uint32_t)((uint32_t)cfg_bat_mah * batPercent / 100);
    html += "<b>" + String(batPercent) + "%</b><span class='sm'> (" + vBuf + " V &bull; ~" + String(rem) + " mAh)</span>";
  } else {
    html += "<span class='sm'>Wird gemessen...</span>";
  }
  html += "</div></div>";

  html += "<form action='/settings/save' method='POST'>";
  html += "<div class='shdr'>&#9201; Timing</div>";
  html += "<div class='sg'>"
    "<div class='sr2'><div class='sl'><span class='slb'>Entprellzeit (ms)</span>"
    "<input type='number' name='debounce' value='" + String(cfg_debounce_ms) + "' min='50' max='2000'></div>"
    "<small class='hint'>Mindestzeit zwischen zwei Ausl&ouml;sungen (50&ndash;2000 ms)</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Ergebnisanzeige (ms)</span>"
    "<input type='number' name='result' value='" + String(cfg_result_show_ms) + "' min='2000' max='30000'></div>"
    "<small class='hint'>Wie lange das Ergebnis auf dem Display bleibt</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Lauf-Timeout (min)</span>"
    "<input type='number' name='timeout' value='" + String(cfg_run_timeout_ms / 60000) + "' min='1' max='30'></div>"
    "<small class='hint'>Max. Laufdauer &ndash; danach Zur&uuml;ck zu BEREIT</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>LoRa Kompensation (ms)</span>"
    "<input type='number' name='loracomp' value='" + String(cfg_lora_comp_ms) + "' min='0' max='500'></div>"
    "<small class='hint'>Funklatenz von gemessener Zeit abziehen (typisch 0&ndash;50 ms)</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Sendeleistung (dBm)</span>"
    "<input type='number' name='lorapwr' value='" + String(cfg_lora_pwr) + "' min='2' max='20' inputmode='numeric'></div>"
    "<small class='hint'>2&ndash;20 dBm &bull; Standard: 14 dBm &bull; sofort wirksam</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Retry-Intervall (ms)</span>"
    "<input type='number' name='retryiv' value='" + String(cfg_retry_interval) + "' min='500' max='10000'></div>"
    "<small class='hint'>Wiederholungsintervall wenn keine Best&auml;tigung kommt</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Max. Retries</span>"
    "<input type='number' name='maxretry' value='" + String(cfg_max_retries) + "' min='1' max='10'></div>"
    "<small class='hint'>Maximale Anzahl Sendeversuche bis zum Aufgeben</small></div>"
    "</div>";
  html += "<div class='shdr'>&#128267; Batterie</div>";
  html += "<div class='sg'>"
    "<div class='sr2'><div class='sl'><span class='slb'>Akkukapazit&auml;t (mAh)</span>"
    "<input type='number' name='batmah' value='" + String(cfg_bat_mah) + "' min='100' max='10000'></div>"
    "<small class='hint'>Kapazit&auml;t des eingebauten Akkus</small></div>"
    "</div>";
  html += "<div class='shdr'>&#128261; Display</div>";
  html += "<div class='sg'>"
    "<div class='sr2'><div class='sl'><span class='slb'>Helligkeit: <span id='cl'>" + String(cfg_contrast) + "</span></span></div>"
    "<input type='range' name='contrast' value='" + String(cfg_contrast) + "' min='0' max='255' "
    "oninput=\"document.getElementById('cl').textContent=this.value\">"
    "<small class='hint'>OLED-Helligkeit (0&ndash;255)</small></div>"
    "</div>";
  html += "<div class='shdr'>&#9000; Sensor</div>";
  html += "<div class='sg'>"
    "<div class='sr2'><div class='sl'><span class='slb'>GPIO Pin</span>"
    "<input type='number' name='platepin' value='" + String(cfg_plate_pin) + "' min='0' max='39'></div>"
    "<small class='hint'>Pin des Drucksensors (Neustart nach &Auml;nderung)</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Sensor-Typ</span></div>"
    "<div class='srow'>"
    "<label><input type='radio' name='platenc' value='0'" + String(!cfg_plate_nc ? " checked" : "") + "> NO</label>"
    "<label><input type='radio' name='platenc' value='1'" + String(cfg_plate_nc ? " checked" : "") + "> NC</label></div></div>"
    "</div>";
  html += "<div class='shdr'>&#128065; Display</div>";
  html += "<div class='sg'>"
    "<div class='sr2'><div class='sl'><span class='slb'>Zusatztaste GPIO</span>"
    "<input type='number' name='btn2pin' value='" + String(cfg_btn2_pin < 40 ? (int)cfg_btn2_pin : 255) + "' min='0' max='255' inputmode='numeric'></div>"
    "<small class='hint'>GPIO-Pin einer externen Taste (Seite weiterschalten) &bull; 255 = deaktiviert &bull; Neustart n&ouml;tig</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Auto-Seite (Sek)</span>"
    "<input type='number' name='autopage' value='" + String(cfg_page_auto_ms > 0 ? (int)(cfg_page_auto_ms/1000) : 0) + "' min='0' max='60' inputmode='numeric'></div>"
    "<small class='hint'>Sekunden pro Seite automatisch weiterschalten &bull; 0 = deaktiviert</small></div>"
    "</div>";

  html += "<div class='shdr'>&#128225; WiFi <span style='color:#555;font-weight:normal'>(Neustart n&ouml;tig)</span></div>";
  html += "<div class='sg'>"
    "<div class='sr2'><div class='sl'><span class='slb'>SSID</span>"
    "<input type='text' name='apssid' value='" + String(cfg_ap_ssid) + "' maxlength='32'></div></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Passwort</span>"
    "<input type='password' name='appass' value='' placeholder='leer lassen' maxlength='63'></div>"
    "<small class='hint'>Leer = offenes Netz &bull; Mind. 8 Zeichen f&uuml;r WPA2</small></div>"
    "</div>";
  html += "<button type='submit' class='sbtn'>&#10003; Speichern</button>";
  html += "</form>";
  if (sdPresent) {
    html += "<div class='shdr' style='margin-top:16px'>&#128190; SD-Karte</div>"
      "<div class='sg'><div class='sr2'>"
      "<small class='hint' style='color:#555'>SD-Log l&ouml;scht nur die Messdaten-CSV &ndash; keine Systemdaten.</small></div>"
      "<button onclick=\"if(confirm('SD-Log l\\u00f6schen?')){fetch('/sdformat').then(r=>r.json()).then(d=>{this.textContent=d.ok?'\\u2713 Gel\\u00f6scht':'Fehler'});}return false;\" "
      "style='background:#2a0a0a;color:#f44336;border:1px solid #3a1010;border-radius:10px;"
      "padding:10px 20px;font-size:.9em;cursor:pointer;margin-top:8px'>&#128465; SD-Log l&ouml;schen</button></div>";
  }
  html += "<div class='c' style='margin-top:12px'>"
    "<div class='lb'>Verbindung</div>"
    "<div style='font-size:.85em;color:#666;line-height:1.9'>"
    "&#128225; SSID: <span style='color:#aaa'>" + String(cfg_ap_ssid) + "</span><br>"
    "&#127760; IP: <span style='color:#aaa'>192.168.4.1</span>"
    "</div></div>"
    "<div class='c'><div class='shdr'>&#128640; Firmware &amp; Ger&auml;t</div>"
    "<div style='display:flex;gap:8px;flex-wrap:wrap'>"
    "<a class='btn' style='background:#1f1a00;color:#ff9800' href='/restart' "
    "onclick=\"return confirm('Ger\\u00e4t neu starten?')\">&#128260; Neustart</a>"
    "<a class='btn' style='background:#1a0a0a;color:#f44336' href='/sleep' "
    "onclick=\"return confirm('Ger\\u00e4t ausschalten?')\">&#128274; Ausschalten</a>"
    "</div></div>"
    "</div>"; // tab-cfg

  // ── JavaScript ─────────────────────────────────────────────
  html += "<script>"
    "function _st(id,el){"
    "document.querySelectorAll('.tab-pane').forEach(function(p){p.classList.remove('act')});"
    "document.querySelectorAll('.bni').forEach(function(t){t.classList.remove('active')});"
    "document.getElementById('tab-'+id).classList.add('act');"
    "el.classList.add('active');"
    "localStorage.setItem('_tabS',id);}"
    "(function(){"
    "fetch('/settime?ts='+Date.now()).catch(function(){});"
    "var p=new URLSearchParams(location.search);"
    "var t=p.get('tab')||localStorage.getItem('_tabS')||'live';"
    "var el=document.getElementById('bn-'+t);"
    "if(el)_st(t,el);else _st('live',document.getElementById('bn-live'));"
    "if(p.get('saved')==='1')document.getElementById('toast-ok').style.display='block';"
    "if(p.get('restart')==='1')document.getElementById('toast-rs').style.display='block';"
    "})();"
    "function _f(ms){if(ms<0)ms=0;"
    "var m=Math.floor(ms/60000),s=Math.floor((ms%60000)/1000),t=ms%1000;"
    "return('0'+m).slice(-2)+':'+('0'+s).slice(-2)+'.'+('00'+t).slice(-3);}"
    "function _fts(ts){if(!ts)return'\\u2014';"
    "try{var d=new Date(+ts);"
    "return d.toLocaleDateString('de-DE',{day:'2-digit',month:'2-digit'})+'\\u00a0'+"
    "d.toLocaleTimeString('de-DE',{hour:'2-digit',minute:'2-digit'});"
    "}catch(e){return'?';}}"
    "function _esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}"
    "var _el=document.getElementById('_T'),_TI=null;"
    "function _startTimer(){if(_TI)return;_TI=setInterval(function(){"
    "var e=_el||document.getElementById('_T');"
    "if(e&&_S==='LAEUFT')e.textContent=_f(_B+(Date.now()-_P));},50);}"
    "function _stopTimer(){if(_TI){clearInterval(_TI);_TI=null;}}"
    "if(_S==='LAEUFT')_startTimer();else if(_B>0)_el.textContent=_f(_B);"
    "function _mkSig(d){"
    "var bars=0,ht=[4,6,9,12,16];"
    "if(d.since<300){if(d.rssi>=-65)bars=5;else if(d.rssi>=-75)bars=4;else if(d.rssi>=-85)bars=3;else if(d.rssi>=-95)bars=2;else bars=1;}"
    "var sk=\"<span class='sk'>\";"
    "for(var i=0;i<5;i++)sk+=\"<span class='sb \"+(i<bars?'on':'off')+\"' style='height:\"+ht[i]+\"px'></span>\";"
    "sk+='</span>';return{sk:sk,bars:bars};}"
    "function _sig(d){"
    "var r=_mkSig(d);"
    "var lc=d.since>=9999?'e':d.since<35?'ok':d.since<70?'w':'e';"
    "var lt=d.since>=9999?'Kein Kontakt':d.since<35?'Verbunden':d.since<70?'Schwach':'Getrennt';"
    "document.getElementById('sig').innerHTML=r.sk+\" <span class='\"+lc+\"'>\"+lt+\"</span>\";}"
    "function _loraTab(d){"
    "var r=_mkSig(d);"
    "var lc=d.since>=9999?'e':d.since<35?'ok':d.since<70?'w':'e';"
    "var lt=d.since>=9999?'Kein Kontakt':d.since<35?'Verbunden':d.since<70?'Schwach':'Getrennt';"
    "var e;e=document.getElementById('lora-q-badge');if(e){e.className=lc;e.textContent=lt;}"
    "e=document.getElementById('lora-sig2');if(e)e.innerHTML=r.sk+\" <span class='\"+lc+\"'>\"+lt+\"</span>\";"
    "e=document.getElementById('lora-since');if(e)e.textContent=d.since<9999?'vor '+d.since+' s':'\\u2014';"
    "var qst=d.rssi>=-65?'GUT':d.rssi>=-85?'MITTEL':'SCHWACH';"
    "e=document.getElementById('lora-rssi-val');"
    "if(e&&d.since<9999)e.innerHTML='RSSI: <b>'+d.rssi+' dBm</b> \\u00b7 SNR: <b>'+d.snr+' dB</b> \\u00b7 <b>'+qst+'</b>';"
    "else if(e)e.innerHTML='<span class=\\'sm\\'>Kein Kontakt</span>';"
    "if(d.lora){e=document.getElementById('lora-stat');"
    "if(e){var s='TX: '+d.lora.txOk+' OK \\u00b7 '+d.lora.txFail+' Fehler<br>'"
    "+'RX: '+d.lora.rxCnt+' Pakete';"
    "if(d.lora.rxCnt>0)s+='<br>RSSI Min/Max: '+d.lora.rssiMin+' / '+d.lora.rssiMax+' dBm';"
    "e.innerHTML=s;}}}"
    "function _bat(d){if(!d.bat||!d.bat.mv)return;"
    "var pct=d.bat.pct,mv=d.bat.mv,mah=d.bat.mah;"
    "var rem=Math.round(mah*pct/100);"
    "var v=(mv/1000).toFixed(2);"
    "var bel=document.getElementById('bat-val');"
    "var bc=pct<=5?'batc':pct<=15?'batw':'';"
    "var warn=pct<=5?' &#9888; KRITISCH':pct<=15?' &#9888; Niedrig':'';"
    "if(bel)bel.innerHTML='<b class=\\''+bc+'\\'>'+pct+'%'+warn+'</b><span class=\\'sm\\'> ('+v+' V \\u00b7 ~'+rem+' mAh)</span>';}"
    "var _hMode=0,_lastHist=-1,_lastBest=-1;"
    "function _hist(d,fl){"
    "var h=d.hist,best=d.bestMs;"
    "var htbl=document.getElementById('h-tbl');"
    "var hhdr=document.getElementById('h-hdr');"
    "if(h.length===0){htbl.innerHTML=\"<div style='color:#2a2a2a;padding:10px 0'>Keine Splits.</div>\";hhdr.textContent='Verlauf \\u2014 0 Splits';return;}"
    "var valid=[];for(var i=0;i<h.length;i++){if(h[i].ms>0)valid.push({ms:h[i].ms,ts:h[i].ts,fi:i+1});}"
    "var display=_hMode===1?valid.slice():valid.slice().sort(function(a,b){return a.ms-b.ms;});"
    "var s=\"<table><tr><th class='rk'>#</th><th>Zeit</th><th>+Delta</th><th></th></tr>\";"
    "for(var rank=0;rank<display.length;rank++){"
    "var it=display[rank],ms=it.ms,ts=it.ts||0;"
    "var rc=_hMode===0?(rank===0?'r1':rank===1?'r2':rank===2?'r3':''):(it.fi===1?'r1':it.fi===2?'r2':it.fi===3?'r3':'');"
    "var med=_hMode===0?(rank===0?'&#127942;':rank===1?'&#129352;':rank===2?'&#129353;':''):(it.fi===1?'&#127942;':it.fi===2?'&#129352;':it.fi===3?'&#129353;':'');"
    "var dts=_fts(ts);"
    "var ttl=dts!=='\\u2014'?' title=\"'+dts+'\"':'';"
    "s+='<tr class=\"'+rc+'\">';"
    "s+='<td class=\"rk\">'+(rank+1)+'</td>';"
    "s+='<td'+ttl+' style=\"cursor:default\">'+_f(ms)+\"<span class='fahr'>F\"+it.fi+'</span></td>';"
    "if(_hMode===0){"
    "if(rank===0||best===0){s+='<td class=\"dt\">\\u2014</td>';}"
    "else{var dv=ms-best;"
    "var ds=dv<60000?'+'+Math.floor(dv/1000)+'.'+('00'+dv%1000).slice(-3)+'s'"
    ":'+'+Math.floor(dv/60000)+':'+('0'+Math.floor((dv%60000)/1000)).slice(-2)+'.'+('00'+dv%1000).slice(-3)+'s';"
    "s+='<td class=\"dt\">'+ds+'</td>';}"
    "}else{s+='<td class=\"dt\">\\u2014</td>';}"
    "s+='<td>'+med+'</td></tr>';}"
    "s+='</table>';"
    "htbl.innerHTML=s;"
    "hhdr.textContent='Verlauf \\u2014 '+d.histCnt+' Splits';}"
    "function _togHist(){"
    "_hMode=_hMode===0?1:0;"
    "var btn=document.getElementById('h-vbtn');"
    "if(btn)btn.textContent=_hMode===0?'\\u23f1 Bestzeit':'\\uD83D\\uDCCB Fahrten';"
    "if(_lastHist!==-1)_hist({hist:_lastHist,bestMs:_lastBest,histCnt:_lastHist.length},false);}"
    "var _lastState2='';"
    "var _firstPoll=true;"
    "function _poll(){"
    "fetch('/state').then(function(r){return r.json();}).then(function(d){"
    "document.getElementById('_ld').className='ld on';"
    "var cls={WARTET:'si','KEIN SIG.':'se',LAEUFT:'sr','SPLIT-OK':'sd'};"
    "var bdg=document.getElementById('st');"
    "bdg.textContent=d.state;bdg.className='bdg '+(cls[d.state]||'si');"
    "var cw=document.getElementById('cwrap');"
    "if(cw)cw.style.display=d.state==='LAEUFT'?'block':'none';"
    "if(d.state==='LAEUFT'){_B=d.liveMs;_P=Date.now();_S='LAEUFT';"
    "var _te=document.getElementById('_T');if(_te){_te.textContent=_f(_B);_el=_te;}"
    "_startTimer();}"
    "else{_stopTimer();_S=d.state;"
    "var _te2=document.getElementById('_T');if(_te2)_te2.textContent=d.liveMs>0?_f(d.liveMs):'--:--.---';}"
    "document.getElementById('t-last').textContent=d.lastMs>0?_f(d.lastMs):'--:--.---';"
    "document.getElementById('t-best').textContent=d.bestMs>0?_f(d.bestMs):'--:--.---';"
    "var fl=!_firstPoll&&d.histCnt>_H;"
    "_lastHist=d.hist;_lastBest=d.bestMs;"
    "if(d.histCnt!==_H||_firstPoll){_H=d.histCnt;_hist(d,fl);}"
    "_sig(d);_bat(d);_loraTab(d);"
    "var si=document.getElementById('sync-ind');"
    "if(si){if(d.timeSynced&&d.syncAgo<300){si.textContent='\\u2713 Uhr sync';si.style.color='#66bb6a';}"
    "else{si.textContent='\\u26a0 Uhr nicht sync';si.style.color='#555';}}"
    "var sdb=document.getElementById('sd-badge');"
    "if(sdb)sdb.innerHTML=d.sdPresent?'<span style=\"color:#4caf50\">&#10003; Eingelegt</span>':'<span style=\"color:#555\">Nicht eingelegt</span>';"
    "if(d.state==='ERGEBNIS'&&_lastState2!=='ERGEBNIS'&&!_firstPoll)_showOv(d);"
    "if(!_firstPoll&&_lastState2!=='LAEUFT'&&d.state==='LAEUFT')_beep(880,.15);"
    "if(d.bat&&d.bat.pct!==undefined)_batWarn(d.bat.pct);"
    "_setRC(false);"
    "_lastState2=d.state;"
    "_firstPoll=false;"
    "}).catch(function(){document.getElementById('_ld').className='ld off';_setRC(true);setTimeout(_poll,1000);});}"
    "var _evtTimer=null;"
    "function _evtToast(msg,col){var t=document.getElementById('evt-toast');if(!t)return;"
    "t.textContent=msg;t.style.background=col||'#1a2a1a';t.style.display='block';"
    "if(_evtTimer)clearTimeout(_evtTimer);"
    "_evtTimer=setTimeout(function(){t.style.display='none';},3000);}"
    "function _toggleTheme(){"
    "var light=document.body.classList.toggle('light');"
    "localStorage.setItem('_theme',light?'light':'dark');"
    "var b=document.getElementById('theme-btn');if(b)b.textContent=light?'\\uD83C\\uDF19 Dunkel':'\\u2600 Hell';}"
    "(function(){var th=localStorage.getItem('_theme');"
    "if(th==='light'){document.body.classList.add('light');"
    "var b=document.getElementById('theme-btn');if(b)b.textContent='\\uD83C\\uDF19 Dunkel';}})();"
    "setInterval(_poll,2000);_poll();"
    "var _wl=null;"
    "function _reqWL(){if('wakeLock' in navigator)navigator.wakeLock.request('screen')"
    ".then(function(w){_wl=w;}).catch(function(){});}"
    "document.addEventListener('visibilitychange',function(){if(document.visibilityState==='visible'){_reqWL();_poll();}});"
    "_reqWL();"
    "var _soundOn=localStorage.getItem('_sound')!=='off';"
    "var _actx=null;"
    "function _unlockAudio(){"
    "if(!_actx)try{_actx=new(window.AudioContext||window.webkitAudioContext)();}catch(e){}"
    "if(_actx&&_actx.state==='suspended')_actx.resume();}"
    "document.addEventListener('click',_unlockAudio,true);"
    "document.addEventListener('touchend',_unlockAudio,true);"
    "function _beep(freq,dur,vol){"
    "if(!_soundOn)return;"
    "try{"
    "if(!_actx)_actx=new(window.AudioContext||window.webkitAudioContext)();"
    "if(_actx.state==='suspended'){_actx.resume().then(function(){_beep(freq,dur,vol);});return;}"
    "var o=_actx.createOscillator(),g=_actx.createGain();"
    "o.connect(g);g.connect(_actx.destination);o.frequency.value=freq;"
    "g.gain.setValueAtTime(vol||0.25,_actx.currentTime);"
    "g.gain.exponentialRampToValueAtTime(0.001,_actx.currentTime+dur);"
    "o.start();o.stop(_actx.currentTime+dur);}catch(e){}}"
    "var _roTm=null;"
    "function _showOv(d){"
    "var ov=document.getElementById('res-ov');if(!ov)return;"
    "document.getElementById('ro-time').textContent=_f(d.lastMs);"
    "var nm=d.hist&&d.hist.length>0?d.hist[d.hist.length-1].n||'':'';"
    "document.getElementById('ro-name').textContent=nm;"
    "var isBest=d.lastMs>0&&d.bestMs>0&&d.lastMs===d.bestMs;"
    "var dEl=document.getElementById('ro-delta'),bEl=document.getElementById('ro-bst');"
    "if(isBest){dEl.textContent='\\uD83C\\uDFC6 Neue Bestzeit!';dEl.style.color='#f0a500';bEl.textContent='';}"
    "else if(d.bestMs>0&&d.lastMs>0){"
    "dEl.textContent='+'+_f(d.lastMs-d.bestMs)+' zur Bestzeit';dEl.style.color='#aaa';"
    "bEl.textContent='Bestzeit: '+_f(d.bestMs);}"
    "else{dEl.textContent='';bEl.textContent='';}"
    "ov.style.display='flex';"
    "if(isBest){_beep(660,.1);setTimeout(function(){_beep(880,.1);},150);setTimeout(function(){_beep(1100,.3);},300);}"
    "else{_beep(660,.2);setTimeout(function(){_beep(880,.25);},250);}"
    "if(navigator.vibrate)navigator.vibrate(isBest?[200,100,200,100,300]:[200,100,200]);"
    "if(_roTm)clearTimeout(_roTm);"
    "_roTm=setTimeout(_closeOv,d.cfg&&d.cfg.result_show?d.cfg.result_show:8000);}"
    "function _closeOv(){var ov=document.getElementById('res-ov');if(ov)ov.style.display='none';if(_roTm)clearTimeout(_roTm);}"
    "function _batWarn(pct){"
    "var bw=document.getElementById('bat-warn');if(!bw)return;"
    "if(pct<=5){bw.style.display='block';bw.style.background='#f44336';bw.style.color='#fff';"
    "bw.className='batblink';bw.textContent='\\u26A0 AKKU KRITISCH: '+pct+'%';}"
    "else if(pct<=15){bw.style.display='block';bw.style.background='#ff9800';bw.style.color='#000';"
    "bw.className='';bw.textContent='\\u26A0 Akku niedrig: '+pct+'%';}"
    "else{bw.style.display='none';bw.className='';}}"
    "function _setRC(show){"
    "var b=document.getElementById('rc-banner');if(b)b.style.display=show?'block':'none';}"
    "function _share(){"
    "var rows=document.querySelectorAll('#h-tbl tr');"
    "var txt='MTB Timer Ergebnisse\\n';"
    "rows.forEach(function(r){var t=r.innerText.replace(/\\t/g,' ');if(t.trim())txt+=t+'\\n';});"
    "if(navigator.share){navigator.share({title:'MTB Ergebnisse',text:txt}).catch(function(){_copyFb(txt);});return;}"
    "_copyFb(txt);}"
    "function _copyFb(txt){"
    "try{"
    "var ta=document.createElement('textarea');"
    "ta.value=txt;ta.style.position='fixed';ta.style.opacity='0';"
    "document.body.appendChild(ta);ta.select();"
    "var ok=document.execCommand('copy');"
    "document.body.removeChild(ta);"
    "if(ok){var t=document.getElementById('evt-toast');if(t){t.textContent='\\uD83D\\uDCCB Kopiert!';t.style.background='#0d1f0d';t.style.display='block';setTimeout(function(){t.style.display='none';},3000);}}"
    "}catch(e){}}"
    "function _fs(){if(!document.fullscreenElement)document.documentElement.requestFullscreen().catch(function(){});else document.exitFullscreen();}"
    "function doPing(){"
    "var b=document.getElementById('ping-btn'),r=document.getElementById('ping-res');"
    "if(b)b.disabled=true;if(r)r.textContent='...';"
    "fetch('/ping').then(function(x){return x.json();}).then(function(j){"
    "if(r)r.textContent=j.sent?'Ping gesendet':'Fehler';"
    "if(b){b.disabled=false;}}).catch(function(){if(r)r.textContent='Fehler';if(b)b.disabled=false;});}"
    "function doSync(){"
    "fetch('/settime?ts='+Date.now()).then(function(){"
    "_evtToast('\\u2713 Uhrzeit synchronisiert','#0d1f0d');_poll();"
    "}).catch(function(){_evtToast('Sync fehlgeschlagen','#2a1a00');});}"
    "</script>"
    "<nav class='bnav'>"
    "<button class='bni' id='bn-live' onclick=\"_st('live',this)\">"
    "<svg viewBox='0 0 24 24'><polyline points='2,12 6,6 10,18 14,8 18,12 22,12'/></svg>Live</button>"
    "<button class='bni' id='bn-hist' onclick=\"_st('hist',this)\">"
    "<svg viewBox='0 0 24 24'><circle cx='12' cy='12' r='9'/><polyline points='12,6 12,12 16,12'/></svg>Verlauf</button>"
    "<button class='bni' id='bn-lora' onclick=\"_st('lora',this)\">"
    "<svg viewBox='0 0 24 24'><path d='M5 12.55a11 11 0 0 1 14.08 0'/><path d='M1.42 9a16 16 0 0 1 21.16 0'/><path d='M8.53 16.11a6 6 0 0 1 6.95 0'/><circle cx='12' cy='20' r='.5' fill='currentColor'/></svg>LoRa</button>"
    "<button class='bni' id='bn-cfg' onclick=\"_st('cfg',this)\">"
    "<svg viewBox='0 0 24 24'><circle cx='12' cy='12' r='3'/>"
    "<path d='M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z'/></svg>Einst.</button>"
    "</nav>"
    "</body></html>";

  return html;
}

void handleRoot() { server.send(200, "text/html", buildHTML()); }

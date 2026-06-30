// ── WebServer JSON-State ───────────────────────────────────
String buildState() {
  String j; j.reserve(3072);
  unsigned long now   = millis();
  unsigned long liveMs = 0;
  if      (appState == RUNNING)     liveMs = now - runStartAt;
  else if (appState == RESULT)      liveMs = lastTimeMs;
  else if (appState == LAP_RUNNING) liveMs = now - lapRoundStart;
  unsigned long since = (loraLastContact > 0) ? (now - loraLastContact) / 1000 : 9999;
  bool peerOk = loraLastContact > 0 && since < (cfg_ping_ms * 3 / 1000);
  bool inLap  = (appState == LAP_IDLE || appState == LAP_RUNNING);
  const char* stStr = (appState == IDLE)        ? (peerOk ? "BEREIT" : "KEIN ZIEL")
                    : (appState == RUNNING)      ? "LAEUFT"
                    : (appState == LAP_IDLE)     ? "LAP-BEREIT"
                    : (appState == LAP_RUNNING)  ? "RUNDE"
                    : "ERGEBNIS";

  j  = "{\"state\":\"";   j += stStr;
  j += "\",\"liveMs\":";  j += liveMs;
  j += ",\"lastMs\":";    j += lastTimeMs;
  j += ",\"bestMs\":";    j += bestTimeMs;
  j += ",\"histCnt\":";   j += historyCnt;
  j += ",\"rssi\":";      j += (int)loraRssi;
  j += ",\"snr\":";       j += String(loraSnr, 1);
  j += ",\"since\":";     j += since;
  { unsigned long fs = (finishLastContact > 0) ? (now - finishLastContact) / 1000 : 9999UL;
    unsigned long ss = (splitLastContact  > 0) ? (now - splitLastContact)  / 1000 : 9999UL;
    unsigned long sa = (lastSyncAt > 0) ? (now - lastSyncAt) / 1000 : 9999UL;
    j += ",\"finSince\":";   j += fs;
    j += ",\"splSince\":";   j += ss;
    j += ",\"timeSynced\":"; j += (timeIsSynced ? "true" : "false");
    j += ",\"syncAgo\":";    j += sa;
  }
  j += ",\"duelMode\":";  j += duelMode ? "true" : "false";
  j += ",\"duelDone\":";  j += duelDone ? "true" : "false";
  j += ",\"lapMode\":";   j += inLap ? "true" : "false";
  j += ",\"lapRound\":";  j += lapRoundNum;
  j += ",\"lapLast\":";   j += lapLastMs;
  j += ",\"lapBest\":";   j += lapBestMs;
  j += ",\"splitMs\":";   j += splitTimeMs;
  j += ",\"splitAge\":";  j += (splitRxAt > 0 ? (now - splitRxAt) / 1000 : 9999UL);
  j += ",\"bat\":{\"pct\":"; j += batPercent;
  j += ",\"mv\":";            j += (uint32_t)(batVoltage * 1000.0f);
  j += ",\"mah\":";           j += cfg_bat_mah;
  j += "}";
  j += ",\"lora\":{\"txOk\":"; j += loraTxCount;
  j += ",\"txFail\":";          j += loraTxFail;
  j += ",\"rxCnt\":";           j += loraRxCount;
  j += ",\"rssiMin\":";
  if (loraHasRx) j += (int)loraRssiMin; else j += "0";
  j += ",\"rssiMax\":";
  if (loraHasRx) j += (int)loraRssiMax; else j += "0";
  j += ",\"freq\":868,\"bw\":125,\"sf\":7,\"cr\":5}";
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
  j += "],\"cfg\":{";
  j += "\"debounce\":";    j += cfg_debounce_ms;
  j += ",\"result_show\":";j += cfg_result_show_ms;
  j += ",\"timeout\":";    j += cfg_run_timeout_ms;
  j += ",\"ping\":";       j += cfg_ping_ms;
  j += ",\"lora_comp\":";  j += cfg_lora_comp_ms;
  j += ",\"contrast\":";   j += cfg_contrast;
  j += ",\"plate_pin\":";  j += cfg_plate_pin;
  j += ",\"plate_nc\":";   j += cfg_plate_nc ? "true" : "false";
  j += ",\"bat_mah\":";    j += cfg_bat_mah;
  j += ",\"btn2_pin\":";   j += cfg_btn2_pin;
  j += ",\"auto_page\":";  j += cfg_page_auto_ms;
  j += "}";
  j += ",\"sdPresent\":"; j += sdPresent ? "true" : "false";
  j += ",\"trackId\":";  j += cfg_track_id;
  { String tn = String(activeTrackName); tn.replace("\"","\\\"");
    j += ",\"trackName\":\""; j += tn; j += "\""; }
  j += ",\"riderId\":";  j += cfg_rider_id;
  { String rn = String(activeRiderName); rn.replace("\"","\\\"");
    j += ",\"riderName\":\""; j += rn; j += "\""; }
  j += ",\"condition\":"; j += cfg_condition;
  // ── Versetzter Start-Modus ──────────────────────────────
  j += ",\"stagMode\":";     j += stagMode ? "true" : "false";
  j += ",\"stagDone\":";     j += stagDone ? "true" : "false";
  j += ",\"stagCount\":";    j += stagCount;
  j += ",\"stagStarted\":";  j += stagStarted;
  j += ",\"stagFinished\":"; j += stagFinished;
  j += ",\"stagOffset\":";   j += cfg_stag_offset_s;
  { unsigned long nextIn = 0;
    if (stagMode && stagStarted < stagCount && stagStarted > 0) {
      unsigned long elapsed = millis() - stagLastStartMs;
      unsigned long needed  = (unsigned long)cfg_stag_offset_s * 1000UL;
      if (elapsed < needed) nextIn = needed - elapsed;
    }
    j += ",\"stagNextIn\":"; j += nextIn;
  }
  j += ",\"stagRiders\":[";
  for (uint8_t i = 0; i < stagCount; i++) {
    if (i) j += ",";
    String rn = String(stagRiders[i]); rn.replace("\"","\\\"");
    bool started  = (i < stagStarted);
    bool finished = (stagFinishedMask & (1u << i)) != 0;
    j += "{\"n\":\""; j += rn;
    j += "\",\"st\":";  j += started  ? "true" : "false";
    j += ",\"fin\":"; j += finished ? "true" : "false";
    j += ",\"ms\":";  j += (finished ? stagTimes[i] : 0UL);
    j += "}";
  }
  j += "]";
  j += "}";
  return j;
}

void handleState() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", buildState());
}

// ── WebServer HTML ─────────────────────────────────────────
void sendHTML() {
  char buf[20];
  String html;
  html.reserve(16384);

  char upBuf[12]; fmtUptime(upBuf);
  unsigned long now   = millis();
  unsigned long since = (loraLastContact > 0) ? (now - loraLastContact) / 1000 : 9999;
  bool peerOk = loraLastContact > 0 && since < (cfg_ping_ms * 3 / 1000);
  bool inLapH = (appState == LAP_IDLE || appState == LAP_RUNNING);
  const char* stStr = (appState == IDLE)        ? (peerOk ? "BEREIT" : "KEIN ZIEL")
                    : (appState == RUNNING)      ? "LAEUFT"
                    : (appState == LAP_IDLE)     ? "LAP-BEREIT"
                    : (appState == LAP_RUNNING)  ? "RUNDE"
                    : "ERGEBNIS";
  const char* stCls = (appState == IDLE)        ? (peerOk ? "si" : "se")
                    : (appState == RUNNING)      ? "sr"
                    : (appState == LAP_IDLE)     ? "si"
                    : (appState == LAP_RUNNING)  ? "sr"
                    : "sd";

  unsigned long liveMs = 0;
  if      (appState == RUNNING)     liveMs = now - runStartAt;
  else if (appState == RESULT)      liveMs = lastTimeMs;
  else if (appState == LAP_RUNNING) liveMs = now - lapRoundStart;
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

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");

  html = "<!DOCTYPE html><html lang='de'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<script>var _P=Date.now(),_S='" + String(stStr) + "',_B=" + String(liveMs) +
    ",_H=" + String(historyCnt) + ",_DM=" + (duelMode?"true":"false") +
    ",_DD=" + (duelDone?"true":"false") +
    ",_LM=" + (inLapH?"true":"false") +
    ",_SM=" + (stagMode?"true":"false") +
    ",_SD=" + (stagDone?"true":"false") +
    ",_SS=" + String(stagStarted) + ";</script>"
    "<title>MTB START</title><style>"
    ":root{--bg:#0a0a0a;--card:#1c1c1e;--card2:#2a2a2c;--txt:#eee;--sub:#555;--sub2:#383838;"
    "--acc:#f0a500;--acc-txt:#000;--ok:#4caf50;--ok-bg:#0d1f0d;--err:#f44336;--err-bg:#200d0d;"
    "--border:#2a2a2a;--border2:#1c1c1e;--blue:#64b5f6;--blue-bg:#0d1525}"
    "body.light{--bg:#f2f2f7;--card:#ffffff;--card2:#e5e5ea;--txt:#1c1c1e;--sub:#8e8e93;"
    "--sub2:#c7c7cc;--acc:#c87800;--acc-txt:#fff;--ok:#1a7a3a;--ok-bg:#d4edda;"
    "--err:#c0392b;--err-bg:#fde8e8;--border:#d1d1d6;--border2:#e5e5ea}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;background:var(--bg);color:var(--txt);margin:0;padding:16px;padding-bottom:80px;max-width:500px}"
    "h1{color:#f0a500;margin:0 0 12px;font-size:1.3em}"
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
    ".on{background:var(--ok)}.off{background:#252525}"
    ".ok{color:var(--ok)}.w{color:#ff9800}.e{color:var(--err)}"
    ".sm{color:var(--sub);font-size:.78em}"
    "table{width:100%;border-collapse:collapse;margin-top:6px}"
    "th{color:#444;font-size:.7em;text-transform:uppercase;letter-spacing:.06em;padding:3px 0 6px;border-bottom:1px solid var(--border)}"
    "td{padding:6px 2px;border-bottom:1px solid var(--border2);font-size:.88em}"
    ".rk{color:#333;width:22px}"
    ".nm{color:#aaa;max-width:90px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
    ".dt{color:var(--sub)}"
    ".r1 td{color:var(--acc);font-weight:bold}.r2 td{color:#ccc}.r3 td{color:#cd7f32}"
    ".btn{display:inline-block;background:var(--card2);color:#888;padding:12px 20px;border-radius:12px;"
    "text-decoration:none;font-size:.85em;border:none;margin-top:10px}"
    ".cbtn{background:var(--err-bg);color:var(--err);display:block;text-align:center;margin-bottom:10px}"
    ".dbtn{display:block;text-align:center;background:var(--ok-bg);color:var(--ok);"
    "border:1px solid #1a3a1a;padding:10px;border-radius:12px;text-decoration:none;"
    "font-size:.9em;font-weight:bold;margin-bottom:10px}"
    ".dcard{border-left:3px solid var(--acc)}"
    ".dname{font-size:1.15em;font-weight:bold;margin:4px 0}"
    ".dnext{color:#888;font-size:.82em;margin-top:4px}"
    ".rcard{border-left:3px solid var(--acc)}"
    ".ft{color:var(--sub2);font-size:.68em;text-align:center;margin-top:8px}"
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
    ".toast{display:none;border-radius:12px;padding:10px 14px;margin-bottom:12px;font-size:.85em}"
    ".tok{background:var(--ok-bg);color:var(--ok);border:1px solid #2a4a2a}"
    ".twarn{background:#2a1a0d;color:#ff9800;border:1px solid #4a3a1a}"
    ".det{background:var(--card);border-radius:16px;padding:10px 14px;margin-top:-6px;margin-bottom:12px}"
    ".det summary{cursor:pointer;color:var(--sub);font-size:.72em;text-transform:uppercase;letter-spacing:.07em;list-style:none}"
    ".det summary::marker,.det summary::-webkit-details-marker{display:none}"
    ".det summary::before{content:'\\25B6 ';font-size:.85em}"
    "details[open] .det summary::before{content:'\\25BC ';}"
    ".hint{color:var(--sub2);font-size:.68em;margin-top:3px;display:block}"
    ".dev-row{display:flex;gap:8px;margin-top:8px;flex-wrap:wrap}"
    ".dev-b{display:inline-flex;align-items:center;gap:5px;padding:4px 10px;border-radius:12px;font-size:.72em;background:#111;border:1px solid #1f1f1f}"
    ".dev-b .dot{width:7px;height:7px;border-radius:50%;flex-shrink:0}"
    ".mbtns{display:flex;gap:8px;margin-bottom:12px}"
    ".mbtn{flex:1;display:block;text-align:center;padding:14px 8px;border-radius:12px;text-decoration:none;font-size:.85em;font-weight:bold;border:1px solid #232323;min-height:44px}"
    ".mbtn-l{background:var(--blue-bg);color:var(--blue);border-color:#1a2a3a}"
    ".mbtn-d{background:#1a1000;color:var(--acc);border-color:#2a2000}"
    ".fahr{display:inline-block;background:var(--card2);color:var(--sub);border-radius:4px;font-size:.7em;padding:1px 5px;margin-left:4px;vertical-align:middle}"
    ".batblink{animation:batblink 1s infinite}"
    "@keyframes batblink{0%{opacity:1}50%{opacity:.4}100%{opacity:1}}"
    "@keyframes newbest{0%{color:#fff;text-shadow:0 0 20px #f0a500}100%{color:var(--acc);text-shadow:none}}"
    ".newbest{animation:newbest 1.5s ease-out}"
    ".rc-banner{display:none;background:#2a1a00;color:#ff9800;border:1px solid #4a3a1a;"
    "border-radius:12px;padding:8px 14px;margin-bottom:12px;font-size:.82em}"
    "</style></head><body>"
    "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:12px'>"
    "<h1 style='margin:0'>&#9201; MTB TIMER <span id='_ld' class='ld on'></span></h1>"
    "<div style='display:flex;align-items:center;gap:6px'>"
    "<button onclick='_fs()' style='background:none;border:none;color:var(--sub);font-size:1.2em;cursor:pointer;padding:4px'>&#9974;</button>"
    "<button onclick='_openViewer()' title='Zuschauer' style='background:none;border:none;color:var(--sub);font-size:1.2em;cursor:pointer;padding:4px'>&#128065;</button>"
    "<button id='sound-btn' onclick='_toggleSound()' style='background:none;border:1px solid var(--border);"
    "color:var(--sub);border-radius:20px;padding:5px 10px;font-size:.75em;cursor:pointer'>&#128266; AN</button>"
    "<button id='theme-btn' onclick='_toggleTheme()' style='background:none;border:1px solid var(--border);"
    "color:var(--sub);border-radius:20px;padding:5px 12px;font-size:.8em;cursor:pointer'>"
    "&#9728; Hell</button>"
    "</div>"
    "</div>";

  html += "<div id='bat-warn' style='display:none;position:fixed;top:0;left:0;right:0;z-index:150;"
    "padding:6px;text-align:center;font-size:.85em;font-weight:bold;max-width:500px'></div>"
    "<div id='rc-banner' class='rc-banner'>&#9888; Verbindung unterbrochen &ndash; stelle wieder her...</div>"
    "<div id='toast-ok' class='toast tok'>&#10003; Gespeichert.</div>"
    "<div id='toast-rs' class='toast twarn'>&#9888; Neustart f&uuml;r WiFi/Pin n&ouml;tig.</div>"
    "<div id='evt-toast' style='display:none;border-radius:12px;padding:10px 14px;"
    "margin-bottom:12px;font-size:.85em;color:#eee'></div>";

  // ══ TAB: LIVE ══════════════════════════════════════════════
  html += "<div class='tab-pane' id='tab-live'>";

  String sk = "<span class='sk'>";
  int ht[] = {4, 6, 9, 12, 16};
  for (int i = 0; i < 5; i++)
    sk += "<span class='sb " + String(i < bars ? "on" : "off") + "' style='height:" + String(ht[i]) + "px'></span>";
  sk += "</span>";

  {
    unsigned long fSince = (finishLastContact > 0) ? (millis() - finishLastContact) / 1000 : 9999UL;
    unsigned long sSince = (splitLastContact  > 0) ? (millis() - splitLastContact)  / 1000 : 9999UL;
    const char* fc = fSince < 35 ? "#4caf50" : fSince < 70 ? "#ff9800" : "#444";
    const char* sc2 = sSince < 35 ? "#4caf50" : sSince < 70 ? "#ff9800" : "#444";
    const char* ft = fSince < 35 ? "Verbunden" : fSince < 70 ? "Schwach" : fSince < 9999 ? "Getrennt" : "Kein Signal";
    const char* st2 = sSince < 35 ? "Verbunden" : sSince < 70 ? "Schwach" : sSince < 9999 ? "Getrennt" : "Kein Signal";
    unsigned long syncAge = (lastSyncAt > 0) ? (millis() - lastSyncAt) / 1000 : 9999UL;
    String syncTxt = timeIsSynced && syncAge < 300 ? "&#10003; Uhr sync" : "&#9888; Uhr nicht sync";
    html += "<div class='c'><div class='row'>"
      "<div><div class='lb'>Start-Node</div><span id='st' class='bdg " + String(stCls) + "'>" + stStr + "</span></div>"
      "<div style='text-align:right'><div class='lb'>Uptime</div><div class='sm'>" + upBuf + "</div>"
      "<div id='sync-ind' style='font-size:.65em;color:#383838;margin-top:2px'>" + syncTxt + "</div></div>"
      "</div><div id='sig' style='margin-top:8px;font-size:.85em'>"
      + sk + " <span class='" + lqCls + "'>" + lqTxt + "</span></div>"
      "<div class='dev-row'>"
      "<span class='dev-b' id='dev-fin'><span class='dot' style='background:" + String(fc) + "'></span>Ziel: " + ft + "</span>"
      "<span class='dev-b' id='dev-spl'><span class='dot' style='background:" + String(sc2) + "'></span>Split: " + st2 + "</span>"
      "</div></div>";
  }

  html += "<div id='cwrap'" + String(appState == RUNNING ? "" : " style='display:none'") + ">"
    "<a class='btn cbtn' href='/cancel' onclick=\"return confirm('Lauf abbrechen?')\">"
    "&#10005; Lauf abbrechen</a></div>";

  html += "<div class='c'><div class='lb'>Laufzeit</div><div class='big' id='_T'>";
  if (liveMs > 0) { fmtTime(liveMs, buf); html += buf; } else html += "--:--.---";
  html += "</div></div>";

  html += "<div class='half'>"
    "<div class='c'><div class='lb'>Letzte Zeit</div><div class='md' id='t-last'>" + sLast + "</div></div>"
    "<div class='c'><div class='lb'>Bestzeit</div><div class='md bc' id='t-best'>" + sBest + "</div></div>"
    "</div>";

  html += "<div id='split-area' style='display:none'>"
    "<div class='c'><div class='lb'>&#9654; Split-Zeit</div><div class='md' id='split-t'>--:--.---</div></div>"
    "</div>";

  {
    char lr[4]; snprintf(lr, 4, "%u", lapRoundNum);
    char ll[12], lb[12];
    if (lapLastMs > 0) fmtTime(lapLastMs, ll); else strcpy(ll, "--:--.---");
    if (lapBestMs > 0) fmtTime(lapBestMs, lb); else strcpy(lb, "--:--.---");
    html += "<div id='lap-area' style='display:" + String(inLapH?"block":"none") + "'>";
    html += "<div class='c'><div class='lb'>Runden &mdash; R<span id='lap-r'>" + String(lr) + "</span></div>";
    html += "<div style='display:flex;gap:10px;margin:4px 0 10px'>"
      "<div><div class='lb' style='margin:0'>Letzte</div><div class='md' id='lap-l'>" + String(ll) + "</div></div>"
      "<div><div class='lb' style='margin:0'>Beste</div><div class='md bc' id='lap-b'>" + String(lb) + "</div></div>"
      "</div>"
      "<div style='display:flex;gap:8px'>"
      "<a class='btn cbtn' href='/lapstop' style='flex:1;margin:0;text-align:center;display:block'>&#9209; Stopp</a>"
      "<a class='btn' href='/lapreset' onclick=\"return confirm('Alle Zeiten l\\u00f6schen?')\" style='flex:1;margin:0;text-align:center;display:block'>&#8635; Reset</a>"
      "</div></div></div>";
  }

  // Mode-Buttons sind im Modus-Tab

  // ── Session-Info-Karte (nur wenn SD vorhanden) ─────────
  if (sdPresent) {
    static const char* condIcon[] = {"&mdash;","&#9728;","&#128167;","&#127810;","&#10052;"};
    static const char* condTxt[]  = {"","Trocken","Nass","Laubig","Eis"};
    html += "<div class='c' id='sess-card' style='margin-top:0'>"
      "<div class='lb'>Aktuelle Session</div>"
      "<div style='display:flex;gap:10px;flex-wrap:wrap;margin-top:6px'>";
    // Strecke
    html += "<div style='flex:1;min-width:0'><div style='color:#555;font-size:.7em'>STRECKE</div>"
      "<div style='font-size:.95em;font-weight:bold;overflow:hidden;text-overflow:ellipsis;white-space:nowrap'>"
      + (activeTrackName[0] ? htmlEsc(activeTrackName) : "<span style='color:#555'>&#8212; keine &#8212;</span>") +
      "</div></div>";
    // Fahrer
    html += "<div style='flex:1;min-width:0'><div style='color:#555;font-size:.7em'>FAHRER</div>"
      "<div style='font-size:.95em;font-weight:bold;overflow:hidden;text-overflow:ellipsis;white-space:nowrap'>"
      + (activeRiderName[0] ? htmlEsc(activeRiderName) : "<span style='color:#555'>&#8212; kein &#8212;</span>") +
      "</div></div>";
    // Bedingung
    uint8_t cond = cfg_condition < 5 ? cfg_condition : 0;
    html += "<div style='flex:0'><div style='color:#555;font-size:.7em'>BEDINGUNG</div>"
      "<div style='font-size:.95em;font-weight:bold'>"
      + String(condIcon[cond]) + (cond > 0 ? (" " + String(condTxt[cond])) : "") +
      "</div></div>";
    html += "</div>"
      "<div style='margin-top:8px'>"
      "<a href='javascript:void(0)' onclick='_st(\"track\",document.getElementById(\"bn-track\"));_loadCondBtns();_stSub(\"trk\")'"
      " style='font-size:.78em;color:#f0a500;text-decoration:none'>&#9881; Strecken &amp; Fahrer verwalten &#8594;</a>"
      "</div></div>";
  }

  html += "</div>"; // tab-live
  server.sendContent(html); html = ""; html.reserve(16384);

  // ══ TAB: VERLAUF ═══════════════════════════════════════════
  html += "<div class='tab-pane' id='tab-hist'>";
  html += "<div class='c'>"
    "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:4px'>"
    "<div class='lb' id='h-hdr'>Verlauf &mdash; " + String(historyCnt) + " L&auml;ufe</div>"
    "<button onclick='_hv()' id='h-vbtn' style='background:#111;border:1px solid #1f1f1f;color:#555;"
    "border-radius:6px;padding:5px 9px;font-size:.7em;cursor:pointer'>&#128203; Fahrten</button>"
    "</div>";
  html += "<div id='h-tbl'>";
  if (historyCnt == 0) {
    html += "<div style='color:#2a2a2a;padding:10px 0'>Keine Messungen.</div>";
  } else {
    uint8_t sortIdx[MAX_HISTORY];
    uint8_t validCnt = 0;
    for (uint8_t i = 0; i < historyCnt; i++) { uint8_t pi=histPhys(i); if (history[pi] > 0) sortIdx[validCnt++] = pi; }
    for (uint8_t i = 1; i < validCnt; i++) {
      uint8_t key = sortIdx[i]; int8_t j = i - 1;
      while (j >= 0 && history[sortIdx[j]] > history[key]) { sortIdx[j+1] = sortIdx[j]; j--; }
      sortIdx[j+1] = key;
    }
    html += "<table><tr><th class='rk'>#</th><th>Fahrer</th><th>Zeit</th><th>Delta</th></tr>";
    for (uint8_t rank = 0; rank < validCnt; rank++) {
      uint8_t i = sortIdx[rank];
      fmtTime(history[i], buf);
      bool hasName = strlen(historyNames[i]) > 0;
      const char* rc  = (rank == 0) ? "r1" : (rank == 1) ? "r2" : (rank == 2) ? "r3" : "";
      const char* med = (rank == 0) ? "&#127942;" : (rank == 1) ? "&#129352;" : (rank == 2) ? "&#129353;" : "";
      html += "<tr class='"; html += rc; html += "'>";
      html += "<td class='rk'>"; html += String(rank + 1); html += "</td>";
      // Logischen Index für diesen physischen Index finden (für Edit)
      uint8_t logIdx = 0;
      for (uint8_t li = 0; li < historyCnt; li++) { if (histPhys(li) == i) { logIdx = li; break; } }
      html += "<td class='nm' id='nm-"; html += logIdx; html += "'>";
      if (hasName) html += htmlEsc(historyNames[i]); else html += "&mdash;";
      html += " <span onclick='_editName("; html += logIdx;
      html += ",\""; html += htmlEsc(historyNames[i]); html += "\")' "
        "style='cursor:pointer;color:#555;font-size:.75em' title='Name bearbeiten'>&#9998;</span>";
      html += "</td>";
      if (historyTimestamp[i] > 0) {
        char tsBuf[20];
        time_t t2 = (time_t)(historyTimestamp[i] / 1000LL);
        struct tm* tm2 = gmtime(&t2);
        snprintf(tsBuf, sizeof(tsBuf), "%02d.%02d %02d:%02d", tm2->tm_mday, tm2->tm_mon + 1, tm2->tm_hour, tm2->tm_min);
        html += "<td title='"; html += tsBuf; html += "'>"; html += buf; html += "</td>";
      } else {
        html += "<td>"; html += buf; html += "</td>";
      }
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
    for (uint8_t i = 0; i < historyCnt; i++) {
      uint8_t pi = histPhys(i);
      if (history[pi] == 0) {
        bool hasName = strlen(historyNames[pi]) > 0;
        html += "<tr style='opacity:.45;font-style:italic'><td class='rk'>DNF</td>";
        html += "<td class='nm'>"; html += (hasName ? htmlEsc(historyNames[pi]) : "&mdash;"); html += "</td>";
        html += "<td colspan='3'>&mdash;</td></tr>";
      }
    }
    html += "</table>";
  }
  html += "</div>";
  html += "<div style='display:flex;gap:8px;flex-wrap:wrap'>"
          "<a class='btn' href='/export'>&#128190; CSV</a>"
          "<button class='btn' onclick='_share()'>&#8599; Teilen</button>"
          "<button class='btn' onclick='window.print()'>&#128424; Drucken</button>"
          + (sdPresent ? String("<button class='btn' onclick='_loadMore()' id='load-more-btn'>&#8595; Mehr laden</button>") : String("")) +
          "<a class='btn' style='background:#2a0a0a' href='/reset' onclick=\"return confirm('Alle Zeiten l\\u00f6schen?')\">&#128465; Zur&uuml;cksetzen</a>"
          "</div>";
  // ── SD-Stats-Karte ────────────────────────────────────
  if (sdPresent) {
    html += "<div class='c' id='stats-card' style='margin-top:12px'>"
      "<div class='lb'>&#128202; Statistiken"
      + (activeTrackName[0] ? (" &mdash; " + htmlEsc(activeTrackName)) : String("")) +
      "</div>"
      "<div id='stats-body' style='color:#555;font-size:.85em;padding-top:4px'>Lade...</div>"
      "</div>";
    // Archiv-Karte
    html += "<div class='c' style='margin-top:12px'>"
      "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'>"
      "<div class='lb' style='margin:0'>&#128194; Session-Archiv</div>"
      "<button onclick='_loadArchive()' style='background:#1c1c1e;color:#888;border:none;"
      "border-radius:8px;padding:5px 10px;font-size:.75em;cursor:pointer'>&#8635; Laden</button>"
      "</div>"
      "<div id='arch-list' style='color:#555;font-size:.85em'>Tippe auf Laden zum Anzeigen.</div>"
      "</div>";
  }
  html += "</div></div>"; // .c / tab-hist
  html += "</div>"; // tab-hist
  server.sendContent(html); html = ""; html.reserve(16384);

  // ══ TAB: STRECKEN ══════════════════════════════════════════
  html += "<div class='tab-pane' id='tab-track'>";
  if (!sdPresent) {
    html += "<div class='c'><div style='color:#555;padding:10px;text-align:center'>"
      "&#128190; Keine SD-Karte eingelegt.<br>"
      "<span style='font-size:.8em;color:#333;margin-top:4px;display:block'>"
      "SD-Karte einlegen und neu starten</span></div></div>";
  } else {
    // ── Session-Status-Karte (kompakt, immer sichtbar) ─────
    html += "<div class='c' style='padding:12px 16px'>";
    html += "<div style='display:flex;align-items:center;gap:10px;flex-wrap:wrap'>";
    // Bedingung-Pill
    static const char* condIcons3[] = {"&mdash;","&#9728;","&#128167;","&#127810;","&#10052;"};
    static const char* condColors[] = {"#1c1c1e","#221500","#001a2a","#1a1500","#0d1a25"};
    static const char* condTxtC[]   = {"#555","#f0a500","#64b5f6","#ffc107","#90caf9"};
    uint8_t cc = cfg_condition < 5 ? cfg_condition : 0;
    html += "<div id='cond-pill' style='background:" + String(condColors[cc]) + ";color:" + String(condTxtC[cc]) + ";"
      "border-radius:20px;padding:5px 12px;font-size:.82em;cursor:pointer;white-space:nowrap;"
      "border:1px solid " + String(cc>0?"#3a3a3a":"#2a2a2a") + ";transition:all .2s'"
      " onclick='_toggleCondPanel()'>"
      + String(condIcons3[cc]) + " <span id='cond-lbl'>" + String(cc>0?conditionStr(cc):"Bedingung") + "</span>"
      " <span style='font-size:.7em;opacity:.6'>&#9660;</span></div>";
    // Aktive Strecke
    html += "<div style='flex:1;min-width:0'>"
      "<div style='font-size:.7em;color:#555;text-transform:uppercase;letter-spacing:.06em'>Strecke</div>"
      "<div id='trkpill' style='font-weight:bold;font-size:.88em;white-space:nowrap;"
      "overflow:hidden;text-overflow:ellipsis'>"
      + (activeTrackName[0] ? htmlEsc(activeTrackName) : "<span style='color:#555'>&#8212; keine &#8212;</span>") +
      "</div></div>";
    // Aktiver Fahrer
    html += "<div style='flex:1;min-width:0'>"
      "<div style='font-size:.7em;color:#555;text-transform:uppercase;letter-spacing:.06em'>Fahrer</div>"
      "<div id='ridpill' style='font-weight:bold;font-size:.88em;white-space:nowrap;"
      "overflow:hidden;text-overflow:ellipsis'>"
      + (activeRiderName[0] ? htmlEsc(activeRiderName) : "<span style='color:#555'>&#8212; kein &#8212;</span>") +
      "</div></div>";
    html += "</div>";
    // Bedingung-Panel — Buttons werden von _loadCondBtns() gefüllt (dynamisch)
    html += "<div id='cond-panel' style='display:none;margin-top:10px;padding-top:10px;"
      "border-top:1px solid #2a2a2a'>"
      "<div id='cond-btns' style='display:flex;gap:6px'></div>"
      "</div>";
    html += "</div>"; // session-karte
    // ── Sub-Navigation ─────────────────────────────────────
    html += "<div style='display:flex;gap:6px;margin-bottom:12px'>";
    static const char* subTabs[] = {"Strecken","Fahrer","Top 10"};
    static const char* subIds[]  = {"trk","rid","top"};
    for (uint8_t si = 0; si < 3; si++) {
      html += "<button id='sbn-" + String(subIds[si]) + "' onclick='_stSub(\"" + String(subIds[si]) + "\")' "
        "style='flex:1;padding:8px 2px;border-radius:12px;font-size:.78em;cursor:pointer;"
        "border:none;transition:all .15s;"
        "background:" + String(si==0?"#f0a500":"#1c1c1e") + ";"
        "color:" + String(si==0?"#000":"#888") + ";font-weight:" + String(si==0?"bold":"normal") + "'>"
        + String(subTabs[si]) + "</button>";
    }
    html += "</div>";
    // ── Sub-Panels ─────────────────────────────────────────
    // Strecken
    html += "<div id='sp-trk'>";
    html += "<div class='c'>"
      "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'>"
      "<div class='lb' style='margin:0'>&#127942; Strecken</div>"
      "<button onclick='_openTrackEdit(0)' style='background:#1a2a1a;color:#4caf50;border:none;"
      "border-radius:10px;padding:6px 14px;font-size:.8em;cursor:pointer;font-weight:bold'>+ Neu</button>"
      "</div>"
      "<div id='track-list' style='color:#555;font-size:.85em'>Lade...</div></div>";
    html += "</div>";
    // Fahrer
    html += "<div id='sp-rid' style='display:none'>";
    html += "<div class='c'>"
      "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'>"
      "<div class='lb' style='margin:0'>&#128101; Fahrer</div>"
      "<button onclick='_openRiderEdit(0)' style='background:#1a2a1a;color:#4caf50;border:none;"
      "border-radius:10px;padding:6px 14px;font-size:.8em;cursor:pointer;font-weight:bold'>+ Neu</button>"
      "</div>"
      "<div id='rider-list' style='color:#555;font-size:.85em'>Lade...</div></div>";
    html += "</div>";
    // (Archiv ist im Verlauf-Tab)
    // Top 10
    html += "<div id='sp-top' style='display:none'>";
    html += "<div class='c'><div class='lb' style='margin-bottom:8px'>&#127942; Bestenliste</div>"
      "<div id='best-list' style='color:#555;font-size:.85em'>Lade...</div></div>";
    html += "</div>";
  }
  html += "</div>"; // tab-track

  // ══ TAB: MODUS ═════════════════════════════════════════════
  html += "<div class='tab-pane' id='tab-modus'>";
  // Modus-Auswahl (nur im IDLE und kein aktiver Modus)
  if (!inLapH && !duelMode && !duelDone && !stagMode && !stagDone) {
    html += "<div class='c'><div class='lb'>Messmodus w&auml;hlen</div>"
      "<div style='display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin-top:10px'>"
      "<a href='/lapstart' style='background:#1a2a1a;color:#4caf50;border:1px solid #2a3a2a;"
      "border-radius:14px;padding:16px 6px;text-align:center;text-decoration:none;"
      "display:flex;flex-direction:column;align-items:center;gap:4px'>"
      "<span style='font-size:1.6em'>&#8635;</span>"
      "<span style='font-weight:bold;font-size:.88em'>Runden</span>"
      "<span style='font-size:.7em;color:#555'>Rundstrecke</span></a>"
      "<a href='/duel' style='background:#2a1a00;color:#f0a500;border:1px solid #3a2a00;"
      "border-radius:14px;padding:16px 6px;text-align:center;text-decoration:none;"
      "display:flex;flex-direction:column;align-items:center;gap:4px'>"
      "<span style='font-size:1.6em'>&#9876;</span>"
      "<span style='font-weight:bold;font-size:.88em'>Duell</span>"
      "<span style='font-size:.7em;color:#555'>Mehrere Fahrer</span></a>"
      "<a href='/stag' style='background:#0d1a25;color:#64b5f6;border:1px solid #1a2a3a;"
      "border-radius:14px;padding:16px 6px;text-align:center;text-decoration:none;"
      "display:flex;flex-direction:column;align-items:center;gap:4px'>"
      "<span style='font-size:1.6em'>&#9201;</span>"
      "<span style='font-weight:bold;font-size:.88em'>Versetzt</span>"
      "<span style='font-size:.7em;color:#555'>Zeitfahren</span></a>"
      "</div></div>";
  } else if (stagDone) {
    // ── Versetzter Start abgeschlossen ──
    html += "<div class='c rcard'><div class='lb'>&#9201; Versetzter Start &ndash; Ergebnisse</div>";
    // Ergebnisse sortiert nach Zeit
    uint8_t si[DUEL_MAX_RIDERS]; uint8_t sn = stagCount;
    for (uint8_t i = 0; i < sn; i++) si[i] = i;
    for (uint8_t i = 0; i < sn - 1; i++)
      for (uint8_t j = 0; j < sn - 1 - i; j++)
        if (stagTimes[si[j]] > stagTimes[si[j+1]] || stagTimes[si[j]] == 0) {
          uint8_t t = si[j]; si[j] = si[j+1]; si[j+1] = t;
        }
    html += "<table><tr><th class='rk'>Rang</th><th>Fahrer</th><th>Zeit</th></tr>";
    for (uint8_t i = 0; i < sn; i++) {
      String rc = (i==0)?"r1":(i==1)?"r2":(i==2)?"r3":"";
      const char* med = (i==0)?"&#127942;":(i==1)?"&#129352;":(i==2)?"&#129353;":"";
      html += "<tr class='" + rc + "'><td class='rk'>" + String(i+1) + "</td>";
      html += "<td>" + (strlen(stagRiders[si[i]])>0 ? htmlEsc(stagRiders[si[i]]) : String("&mdash;")) + "</td>";
      if (stagTimes[si[i]] > 0) { fmtTime(stagTimes[si[i]], buf); html += "<td>" + String(buf) + " " + med + "</td>"; }
      else html += "<td style='color:#555'>DNF</td>";
      html += "</tr>";
    }
    html += "</table>";
    html += "<a class='btn' href='/stagexit'>&#10006; Schlie&szlig;en</a></div>";
  } else if (stagMode) {
    // ── Versetzter Start aktiv ──
    html += "<div class='c' style='border-left:3px solid #64b5f6'>";
    html += "<div class='lb'>&#9201; Versetzter Start &mdash; " + String(stagStarted) + "/" + String(stagCount) + " gestartet</div>";
    // Countdown zum nächsten Fahrer
    if (stagStarted < stagCount) {
      html += "<div id='stag-cd-wrap' style='margin:10px 0'>";
      if (stagStarted == 0) {
        html += "<div style='color:#4caf50;font-weight:bold'>&#9654; Bereit &mdash; ersten Fahrer starten</div>";
        html += "<div style='color:#555;font-size:.8em'>" + htmlEsc(stagRiders[0]) + "</div>";
      } else {
        unsigned long elapsed = millis() - stagLastStartMs;
        unsigned long offset_ms = (unsigned long)cfg_stag_offset_s * 1000UL;
        unsigned long rem = (elapsed < offset_ms) ? (offset_ms - elapsed) : 0;
        if (rem > 0) {
          char cdBuf[12]; snprintf(cdBuf, sizeof(cdBuf), "%02lu:%02lu", rem/60000, (rem%60000)/1000);
          html += "<div class='lb'>N&auml;chster in</div><div id='stag-cd' style='font-size:2em;font-weight:bold;color:#64b5f6'>" + String(cdBuf) + "</div>";
        } else {
          html += "<div style='color:#4caf50;font-weight:bold'>&#9654; Bereit &mdash; n&auml;chsten Fahrer starten</div>";
        }
        html += "<div style='color:#555;font-size:.8em'>" + htmlEsc(stagRiders[stagStarted]) + "</div>";
      }
      html += "</div>";
    } else {
      html += "<div style='color:#888;font-size:.85em;margin:8px 0'>Alle " + String(stagCount) + " Fahrer gestartet &mdash; warte auf Ergebnisse&hellip;</div>";
    }
    // Status-Tabelle
    html += "<table><tr><th class='rk'>#</th><th>Fahrer</th><th>Status</th></tr>";
    for (uint8_t i = 0; i < stagCount; i++) {
      bool started  = (i < stagStarted);
      bool finished = (stagFinishedMask & (1u << i)) != 0;
      String status;
      if (finished) { fmtTime(stagTimes[i], buf); status = "<span style='color:#4caf50'>" + String(buf) + "</span>"; }
      else if (started) status = "<span style='color:#ff9800'>&#8987; l&auml;uft</span>";
      else status = "<span style='color:#555'>wartet</span>";
      html += "<tr><td class='rk'>" + String(i+1) + "</td><td>" + htmlEsc(stagRiders[i]) + "</td><td>" + status + "</td></tr>";
    }
    html += "</table>";
    html += "<a class='btn' style='margin-top:10px;background:#2a0a0a;color:#f44' href='/stagexit' onclick=\"return confirm('Versetzten Start abbrechen?')\">&#10006; Abbrechen</a>";
    html += "</div>";
  } else if (duelDone) {
    html += "<div class='c rcard'><div class='lb'>&#127937; Duell abgeschlossen &ndash; Rangliste</div>";
    uint8_t n = (duelStartIdx + duelCount <= historyCnt) ? duelCount
              : (historyCnt > duelStartIdx ? historyCnt - duelStartIdx : 0);
    if (n > 0) {
      uint8_t si[DUEL_MAX_RIDERS];
      for (uint8_t i = 0; i < n; i++) si[i] = duelStartIdx + i;
      for (uint8_t i = 0; i < n - 1; i++)
        for (uint8_t j = 0; j < n - 1 - i; j++)
          if (history[si[j]] > history[si[j+1]]) { uint8_t t = si[j]; si[j] = si[j+1]; si[j+1] = t; }
      html += "<table><tr><th class='rk'>Rang</th><th>Fahrer</th><th>Zeit</th></tr>";
      for (uint8_t i = 0; i < n; i++) {
        fmtTime(history[si[i]], buf);
        String rc = (i == 0) ? "r1" : (i == 1) ? "r2" : (i == 2) ? "r3" : "";
        const char* med = (i == 0) ? "&#127942;" : (i == 1) ? "&#129352;" : (i == 2) ? "&#129353;" : "";
        html += "<tr class='" + rc + "'><td class='rk'>" + String(i + 1) + "</td>";
        html += "<td>" + (strlen(historyNames[si[i]]) > 0 ? htmlEsc(historyNames[si[i]]) : String("&mdash;")) + "</td>";
        html += "<td>" + String(buf) + " " + med + "</td></tr>";
      }
      html += "</table>";
    }
    html += "<a class='btn' href='/duelexit'>&#10006; Schlie&szlig;en</a></div>";
  } else if (duelMode && duelCurrent < duelCount) {
    html += "<div class='c dcard'><div class='row'><div>";
    html += "<div class='lb'>Duell &bull; Fahrer " + String(duelCurrent + 1) + " / " + String(duelCount) + "</div>";
    html += "<div class='dname'>&#9658; " + htmlEsc(duelRiders[duelCurrent]) + "</div>";
    if (duelCurrent + 1 < duelCount)
      html += "<div class='dnext'>N&auml;chster: " + htmlEsc(duelRiders[duelCurrent + 1]) + "</div>";
    html += "</div><div style='display:flex;flex-direction:column;gap:6px'>"
            "<a class='btn' style='margin:0;background:#333;color:#aaa' href='/duelskip'>&#9654;&#9654; Skip</a>"
            "<a class='btn' style='margin:0' href='/duelexit'>&#10006;</a>"
            "</div></div></div>";
  } else {
    html += "<a class='dbtn' href='/duel'>&#127944; Duell einrichten</a>";
    html += "<div class='c'><div class='lb'>Info</div>"
      "<div style='color:#555;font-size:.85em'>Bis zu " + String(DUEL_MAX_RIDERS) +
      " Fahrer &bull; automatische Rangliste nach dem letzten Lauf.</div></div>";
  }
  html += "</div>"; // tab-modus
  server.sendContent(html); html = ""; html.reserve(16384);

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

  // ── LoRa-Status (vormals eigener Tab) ──────────────────────
  {
    char rssiStr[10]; sprintf(rssiStr, "%d", (int)loraRssi);
    char snrStr[10];  sprintf(snrStr,  "%.1f", loraSnr);
    html += "<div class='shdr'>&#128246; LoRa</div>";
    html += "<div class='c'><div class='row'>"
      "<div><div class='lb'>Verbindung</div>"
      "<span id='lora-q-badge' class='" + String(lqCls) + "'>" + String(lqTxt) + "</span></div>"
      "<div style='text-align:right'><div class='lb'>Letzter Kontakt</div>"
      "<div id='lora-since' class='sm'>" +
      String(loraLastContact == 0 ? "&mdash;" : (String("vor ") + since + " s").c_str()) +
      "</div></div></div>"
      "<div id='lora-sig2' style='margin-top:10px;font-size:.88em'>" + sk + " <span class='" + lqCls + "'>" + lqTxt + "</span></div>"
      "<div id='lora-rssi-val' style='font-size:.85em;margin-top:6px;color:#aaa'>";
    if (loraLastContact > 0) {
      html += "RSSI: <b>" + String(rssiStr) + " dBm</b>"
        " &bull; SNR: <b>" + String(snrStr) + " dB</b>"
        " &bull; <b>" + String(rssiStatus(loraRssi)) + "</b>";
    } else {
      html += "<span class='sm'>Kein Kontakt</span>";
    }
    html += "</div>"
      "<div id='lora-stat' style='font-size:.82em;color:#555;margin-top:6px;line-height:1.8'>"
      "TX: " + String(loraTxCount) + " OK &bull; " + String(loraTxFail) + " Fehler &bull; "
      "RX: " + String(loraRxCount) + " Pakete";
    if (loraRxCount > 0) {
      char mn[8], mx[8];
      sprintf(mn, "%d", (int)loraRssiMin);
      sprintf(mx, "%d", (int)loraRssiMax);
      html += "<br>RSSI Min/Max: " + String(mn) + " / " + String(mx) + " dBm";
    }
    html += "<br><span style='color:#383838'>868 MHz &bull; BW 125 kHz &bull; SF7 &bull; CR 4/5 &bull; Sync 0x12</span>"
      "</div>"
      "</div>";
  }
  // ── Messgenauigkeit / LoRa-Latenz ──────────────────────────
  {
    char compBuf[8]; snprintf(compBuf, sizeof(compBuf), "%lu", (unsigned long)cfg_lora_comp_ms);
    unsigned long nowMs2 = millis();
    unsigned long fS2 = (finishLastContact > 0) ? (nowMs2 - finishLastContact) / 1000 : 9999UL;
    unsigned long sS2 = (splitLastContact  > 0) ? (nowMs2 - splitLastContact)  / 1000 : 9999UL;
    const char* fCol = fS2 < 35 ? "#4caf50" : fS2 < 70 ? "#ff9800" : "#555";
    const char* sCol = sS2 < 35 ? "#4caf50" : sS2 < 70 ? "#ff9800" : "#555";
    const char* fTxt = fS2 < 9999 ? (fS2 < 35 ? "Verbunden" : fS2 < 70 ? "Schwach" : "Getrennt") : "Nicht erkannt";
    const char* sTxt = sS2 < 9999 ? (sS2 < 35 ? "Verbunden" : sS2 < 70 ? "Schwach" : "Getrennt") : "Nicht erkannt";
    char rttBuf[8]; snprintf(rttBuf, sizeof(rttBuf), "%lu", lastRttMs);
    char recBuf[8]; snprintf(recBuf, sizeof(recBuf), "%lu", lastRttMs / 2);
    const char* rttCls = lastRttMs == 0 ? "e" : lastRttMs < 300 ? "ok" : lastRttMs < 600 ? "w" : "e";
    const char* compHint = lastRttMs == 0 ? "&#9201; Latenz messen um Empfehlung zu erhalten"
                         : cfg_lora_comp_ms == 0                       ? "&#9888; Kompensation nicht gesetzt"
                         : (long)cfg_lora_comp_ms > (long)(lastRttMs/2)+50 ? "&#9888; Kompensation zu hoch"
                         : (long)cfg_lora_comp_ms < (long)(lastRttMs/2)-50 ? "&#9888; Kompensation zu niedrig"
                         : "&#10003; Kompensation passt";
    html += "<div class='c'><div class='lb'>&#127919; Messgenauigkeit</div>"
      "<div style='display:flex;gap:16px;margin-bottom:10px;font-size:.82em'>"
      "<div><span class='dot' style='background:" + String(fCol) + "'></span> Ziel: "
      "<span id='meas-fin-status' style='color:" + String(fCol) + "'>" + fTxt + "</span>"
      "<span id='meas-fin-rtt' style='color:#555'></span></div>"
      "<div><span class='dot' style='background:" + String(sCol) + "'></span> Split: "
      "<span id='meas-spl-status' style='color:" + String(sCol) + "'>" + sTxt + "</span>"
      "<span id='meas-spl-rtt' style='color:#555'></span></div>"
      "</div>"
      "<div style='border-top:1px solid var(--border);padding-top:10px;font-size:.85em;line-height:2.1'>"
      "Gemessene Latenz (RTT): <span class='" + String(rttCls) + "' id='rtt-val'>"
      + (lastRttMs > 0 ? String(rttBuf) + " ms" : "&mdash;") + "</span><br>"
      "Einseitige Latenz (&frac12;&thinsp;RTT): <b id='rtt-half'>"
      + (lastRttMs > 0 ? String(recBuf) + " ms" : "&mdash;") + "</b><br>"
      "Aktive Kompensation: <b id='comp-val'>" + String(compBuf) + " ms</b><br>"
      "<span id='comp-hint' style='font-size:.8em;color:#555'>" + compHint + "</span>"
      "</div>"
      "<div id='rtt-apply' style='display:" + String(lastRttMs > 0 ? "flex" : "none") + ";"
      "gap:8px;align-items:center;margin-top:8px;font-size:.82em;color:var(--sub)'>"
      "Empfehlung: <b id='rtt-rec-val'>" + String(lastRttMs > 0 ? String(recBuf) + " ms" : "") + "</b>"
      "<button onclick='_applyComp()' class='btn' style='padding:6px 14px;margin:0;font-size:.82em'>"
      "&#10003; &Uuml;bernehmen</button></div>"
      "<button id='meas-btn' onclick='doMeasure()' class='btn' style='"
      "width:100%;margin-top:10px;background:#0d1f2d;color:#64b5f6;border:1px solid #1a3a5a'>"
      "&#128257; Latenz messen</button>"
      "<div id='meas-res' style='font-size:.85em;margin-top:6px;text-align:center;min-height:1.3em'></div>"
      "</div>";
  }
  // ── Uhrzeit-Sync ───────────────────────────────────────────
  {
    unsigned long syncAge2 = (lastSyncAt > 0) ? (millis() - lastSyncAt) / 1000 : 9999UL;
    const char* syncBadgeCls = (timeIsSynced && syncAge2 < 300) ? "ok" : (timeIsSynced ? "w" : "e");
    const char* syncBadgeTxt = (timeIsSynced && syncAge2 < 300) ? "&#10003; Synchronisiert"
                             : (timeIsSynced ? "&#9888; Sync veraltet" : "&#10007; Nicht synchronisiert");
    String syncDetailTxt;
    if (timeIsSynced && syncAge2 < 9999) {
      if (syncAge2 < 60) syncDetailTxt = "Letzte Sync: vor " + String(syncAge2) + " s";
      else               syncDetailTxt = "Letzte Sync: vor " + String(syncAge2 / 60) + " min";
    } else {
      syncDetailTxt = "Browser-Seite &ouml;ffnen zum Synchronisieren";
    }
    html += "<div class='c'><div class='lb'>&#9201; Uhrzeit-Sync</div>"
      "<div style='margin-bottom:8px'><span class='" + String(syncBadgeCls) + "' id='sync-lora-badge'>" + syncBadgeTxt + "</span></div>"
      "<div id='sync-lora-detail' style='font-size:.82em;color:var(--sub);line-height:1.8'>" + syncDetailTxt + "</div>"
      "<button onclick='doSync()' class='btn' style='margin-top:10px;width:100%'>&#8635; Jetzt synchronisieren</button>"
      "</div>";
  }
  {
    html += "<div class='c'>"
      "<button id='ping-btn' onclick='doPing()' style='"
      "width:100%;background:#0d1f2d;color:#64b5f6;border:1px solid #1a3a5a;"
      "border-radius:8px;padding:11px;font-size:.9em;font-weight:bold;cursor:pointer;margin-top:4px'>"
      "&#128246; Verbindung testen (PNG)</button>"
      "<div id='ping-res' style='color:#555;font-size:.82em;margin-top:8px;text-align:center'></div>"
      "</div>";
  }

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
    "<div class='sr2'><div class='sl'><span class='slb'>Ping-Intervall (s)</span>"
    "<input type='number' name='ping' value='" + String(cfg_ping_ms / 1000) + "' min='5' max='300'></div>"
    "<small class='hint'>Lebenszeichen-Intervall ans Ziel (5&ndash;300 s)</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>LoRa Kompensation (ms)</span>"
    "<input type='number' name='loracomp' value='" + String(cfg_lora_comp_ms) + "' min='0' max='500'></div>"
    "<small class='hint'>Funklatenz von gemessener Zeit abziehen (typisch 0&ndash;50 ms)</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Sendeleistung (dBm)</span>"
    "<input type='number' name='lorapwr' value='" + String(cfg_lora_pwr) + "' min='2' max='20' inputmode='numeric'></div>"
    "<small class='hint'>2&ndash;20 dBm &bull; Standard: 14 dBm &bull; sofort wirksam</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Versatz-Offset (s)</span>"
    "<input type='number' name='stagoffset' value='" + String(cfg_stag_offset_s) + "' min='5' max='255' inputmode='numeric'></div>"
    "<small class='hint'>Mindestabstand zwischen zwei Starts im Versetzt-Modus</small></div>"
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
    "<small class='hint'>OLED-Helligkeit (0&ndash;255) &ndash; niedrigere Werte schonen Akku</small></div>"
    "</div>";

  html += "<div class='shdr'>&#9000; Sensor</div>";
  html += "<div class='sg'>"
    "<div class='sr2'><div class='sl'><span class='slb'>GPIO Pin</span>"
    "<input type='number' name='platepin' value='" + String(cfg_plate_pin) + "' min='0' max='39'></div>"
    "<small class='hint'>Pin des Drucksensors (Neustart nach &Auml;nderung)</small></div>"
    "<div class='sr2'><div class='sl'><span class='slb'>Sensor-Typ</span></div>"
    "<div class='srow'>"
    "<label><input type='radio' name='platenc' value='0'" + String(!cfg_plate_nc ? " checked" : "") +
    "> NO</label>"
    "<label><input type='radio' name='platenc' value='1'" + String(cfg_plate_nc ? " checked" : "") +
    "> NC</label></div>"
    "<small class='hint'>NO = schlie&szlig;t bei Ausl&ouml;sung &bull; NC = &ouml;ffnet bei Ausl&ouml;sung</small></div>"
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
  server.sendContent(html); html = ""; html.reserve(16384);

  // ── JavaScript ─────────────────────────────────────────────
  html += "<script>"
    "function _st(id,el){"
    "document.querySelectorAll('.tab-pane').forEach(function(p){p.classList.remove('act')});"
    "document.querySelectorAll('.bni').forEach(function(t){t.classList.remove('active')});"
    "var pane=document.getElementById('tab-'+id);if(!pane)return;"
    "pane.classList.add('act');"
    "if(el)el.classList.add('active');"
    "localStorage.setItem('_tab',id);}"
    "var _evtTimer=null;"
    "function _evtToast(msg,col){"
    "var t=document.getElementById('evt-toast');if(!t)return;"
    "t.textContent=msg;t.style.background=col||'#1a2a1a';t.style.display='block';"
    "if(_evtTimer)clearTimeout(_evtTimer);"
    "_evtTimer=setTimeout(function(){t.style.display='none';},3000);}"
    "function _toggleTheme(){"
    "var light=document.body.classList.toggle('light');"
    "localStorage.setItem('_theme',light?'light':'dark');"
    "document.getElementById('theme-btn').textContent=light?'\\uD83C\\uDF19 Dunkel':'\\u2600 Hell';}"
    "(function(){"
    "var th=localStorage.getItem('_theme');"
    "if(th==='light'){document.body.classList.add('light');var b=document.getElementById('theme-btn');if(b)b.textContent='\\uD83C\\uDF19 Dunkel';}"
    "})();"
    "(function(){"
    "fetch('/settime?ts='+Date.now()).catch(function(){});"
    "var p=new URLSearchParams(location.search);"
    "var t=p.get('tab')||localStorage.getItem('_tab')||'live';"
    // Map old tab ids to new ones
    "if(t==='duel')t='modus';if(t==='lora')t='cfg';"
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
    "e.innerHTML=s;}}"
    // RTT / Messgenauigkeit live aktualisieren
    "{"
    "var fs2=d.finSince!==undefined?d.finSince:9999;"
    "var ss2=d.splSince!==undefined?d.splSince:9999;"
    "var fCol2=fs2<35?'#4caf50':fs2<70?'#ff9800':'#555';"
    "var sCol2=ss2<35?'#4caf50':ss2<70?'#ff9800':'#555';"
    "var fTxt2=fs2<9999?(fs2<35?'Verbunden':fs2<70?'Schwach':'Getrennt'):'Nicht erkannt';"
    "var sTxt2=ss2<9999?(ss2<35?'Verbunden':ss2<70?'Schwach':'Getrennt'):'Nicht erkannt';"
    "var mfs=document.getElementById('meas-fin-status');if(mfs){mfs.textContent=fTxt2;mfs.style.color=fCol2;}"
    "var mss=document.getElementById('meas-spl-status');if(mss){mss.textContent=sTxt2;mss.style.color=sCol2;}"
    "if(d.rtt!==undefined){"
    "var rv=document.getElementById('rtt-val');"
    "var rh=document.getElementById('rtt-half');"
    "var ch=document.getElementById('comp-hint');"
    "var cv=document.getElementById('comp-val');"
    "var ra=document.getElementById('rtt-apply');"
    "var rrv=document.getElementById('rtt-rec-val');"
    "var rc=d.rtt>0&&d.rtt<300?'ok':d.rtt<600?'w':'e';"
    "if(rv){rv.className=rc;rv.textContent=d.rtt>0?d.rtt+' ms':'\\u2014';}"
    "var half=Math.round(d.rtt/2);"
    "if(rh)rh.textContent=d.rtt>0?half+' ms':'\\u2014';"
    "if(cv&&d.cfg)cv.textContent=(d.cfg.lora_comp!==undefined?d.cfg.lora_comp:d.lora_comp||0)+' ms';"
    "if(rrv&&d.rtt>0)rrv.textContent=half+' ms';"
    "if(ra)ra.style.display=d.rtt>0?'flex':'none';"
    "if(ch){"
    "var comp=d.cfg?d.cfg.lora_comp:d.lora_comp||0;"
    "if(d.rtt===0)ch.innerHTML='&#9201; Latenz messen um Empfehlung zu erhalten';"
    "else if(comp===0&&half>20)ch.innerHTML='\\u26A0 Kompensation nicht gesetzt (Empf.: '+half+' ms)';"
    "else if(comp>half+50)ch.innerHTML='\\u26A0 Kompensation zu hoch (Empf.: '+half+' ms)';"
    "else if(comp+50<half)ch.innerHTML='\\u26A0 Kompensation zu niedrig (Empf.: '+half+' ms)';"
    "else ch.innerHTML='\\u2713 Kompensation passt ('+comp+' ms \\u2248 RTT/2)';}}"
    "}"
    // Sync-Sektion live aktualisieren
    "var sb=document.getElementById('sync-lora-badge');"
    "var sd=document.getElementById('sync-lora-detail');"
    "if(sb){"
    "if(d.timeSynced&&d.syncAgo<300){sb.className='ok';sb.innerHTML='\\u2713 Synchronisiert';}"
    "else if(d.timeSynced){sb.className='w';sb.innerHTML='\\u26A0 Sync veraltet';}"
    "else{sb.className='e';sb.innerHTML='\\u2717 Nicht synchronisiert';}}"
    "if(sd&&d.timeSynced&&d.nowMs){"
    "var drift=Date.now()-d.nowMs;"
    "var driftTxt=(Math.abs(drift)<60000?(drift>=0?'+':'')+drift+' ms':(drift>=0?'+':'')+(drift/1000).toFixed(1)+' s');"
    "var nd=new Date(d.nowMs);"
    "var timeTxt=nd.toLocaleTimeString('de-DE',{hour:'2-digit',minute:'2-digit',second:'2-digit'});"
    "sd.innerHTML='Zeit: <b>'+timeTxt+'</b> &bull; Drift: <b>'+driftTxt+'</b>';}"
    "}"
    "function doPing(){"
    "var btn=document.getElementById('ping-btn');"
    "var res=document.getElementById('ping-res');"
    "btn.disabled=true;res.textContent='Sende...';"
    "fetch('/ping').then(function(r){return r.json();}).then(function(d){"
    "res.textContent=d.sent?'\\u2713 Ping gesendet \\u2014 warte auf Antwort...':'Fehler.';"
    "setTimeout(function(){btn.disabled=false;res.textContent='';},4000);"
    "}).catch(function(){res.textContent='Verbindung getrennt.';btn.disabled=false;});}"
    "function _bat(d){if(!d.bat||!d.bat.mv)return;"
    "var pct=d.bat.pct,mv=d.bat.mv,mah=d.bat.mah;"
    "var rem=Math.round(mah*pct/100);"
    "var v=(mv/1000).toFixed(2);"
    "var bel=document.getElementById('bat-val');"
    "var bc=pct<=5?'batc':pct<=15?'batw':'';"
    "var warn=pct<=5?' &#9888; KRITISCH':pct<=15?' &#9888; Niedrig':'';"
    "if(bel)bel.innerHTML='<b class=\\''+bc+'\\'>'+pct+'%'+warn+'</b><span class=\\'sm\\'> ('+v+' V \\u00b7 ~'+rem+' mAh)</span>';}"
    "var _hMode=0,_lastHist=[],_lastBest=0;"
    "function _hv(){"
    "_hMode=1-_hMode;"
    "var b=document.getElementById('h-vbtn');"
    "if(b)b.textContent=_hMode?'\\u23F1 Bestzeit':'\\u{1F4CB} Fahrten';"
    "_hist({hist:_lastHist,histCnt:_lastHist.length,bestMs:_lastBest},false);}"
    "function _hist(d,fl){"
    "_lastHist=d.hist;_lastBest=d.bestMs;"
    "var h=d.hist,best=d.bestMs;"
    "var htbl=document.getElementById('h-tbl');"
    "var hhdr=document.getElementById('h-hdr');"
    "if(!h||h.length===0){htbl.innerHTML=\"<div style='color:#2a2a2a;padding:10px 0'>Keine Messungen.</div>\";if(hhdr)hhdr.textContent='Verlauf \\u2014 0 L\\u00e4ufe';return;}"
    "var valid=h.map(function(e,i){return{ms:e.ms,n:e.n,ts:e.ts,fi:i+1};}).filter(function(e){return e.ms>0;});"
    "var dnfs=h.filter(function(e){return e.ms===0;});"
    "var rows=_hMode?valid.slice():valid.slice().sort(function(a,b){return a.ms-b.ms;});"
    "var s=\"<table><tr><th class='rk'>#</th><th>Fahrer</th><th>Zeit</th><th>Delta</th></tr>\";"
    "for(var rank=0;rank<rows.length;rank++){"
    "var ms=rows[rank].ms,nm=rows[rank].n||'\\u2014',ts=rows[rank].ts||0,fi=rows[rank].fi;"
    "var rc='';"
    "if(!_hMode){rc=rank===0?'r1':rank===1?'r2':rank===2?'r3':'';}"
    "var med=(!_hMode&&rank===0)?'&#127942;':(!_hMode&&rank===1)?'&#129352;':(!_hMode&&rank===2)?'&#129353;':'';"
    "var tAttr=ts?(' title=\"'+_fts(ts)+'\"'):'';s+='<tr class=\"'+rc+'\">';"
    "s+='<td class=\"rk\">'+(rank+1)+'</td>';"
    "s+='<td class=\"nm\">'+_esc(nm)+'<span class=\"fahr\">F'+fi+'</span></td>';"
    "s+='<td'+tAttr+'>'+_f(ms)+'</td>';"
    "if(!_hMode&&(rank===0||best===0)){s+='<td class=\"dt\">\\u2014</td><td>'+med+'</td>';}"
    "else if(!_hMode){var dv=ms-best;"
    "var ds=dv<60000?'+'+Math.floor(dv/1000)+'.'+('00'+dv%1000).slice(-3)+'s'"
    ":'+'+Math.floor(dv/60000)+':'+('0'+Math.floor((dv%60000)/1000)).slice(-2)+'.'+('00'+dv%1000).slice(-3)+'s';"
    "s+='<td class=\"dt\">'+ds+'</td><td>'+med+'</td>';}"
    "else{s+='<td class=\"dt\">\\u2014</td><td></td>';}"
    "s+='</tr>';}"
    "for(var di=0;di<dnfs.length;di++){"
    "var dnm=dnfs[di].n||'\\u2014';"
    "s+='<tr style=\"opacity:.45;font-style:italic\"><td class=\"rk\">DNF</td><td class=\"nm\">'+_esc(dnm)+'</td><td colspan=\"2\">\\u2014</td></tr>';}"
    "s+='</table>';"
    "if(fl){var nr=htbl.querySelectorAll('tr');if(nr.length>1)nr[1].classList.add('new-row');}"
    "htbl.innerHTML=s;"
    "if(hhdr)hhdr.textContent='Verlauf \\u2014 '+d.histCnt+' L\\u00e4ufe';}"
    "var _firstPoll=true,_lastState2='';"
    "function _poll(){"
    "fetch('/state').then(function(r){return r.json();}).then(function(d){"
    "document.getElementById('_ld').className='ld on';_setRC(false);"
    "if(d.duelMode!==_DM||d.duelDone!==_DD){_DM=d.duelMode;_DD=d.duelDone;location.reload();return;}"
    "if(d.stagMode!==_SM||d.stagDone!==_SD||d.stagStarted!==_SS){"
    "_SM=d.stagMode;_SD=d.stagDone;_SS=d.stagStarted;location.reload();return;}"
    "var cls={BEREIT:'si','KEIN ZIEL':'se',LAEUFT:'sr',ERGEBNIS:'sd','LAP-BEREIT':'si',RUNDE:'sr'};"
    "var bdg=document.getElementById('st');"
    "if(d.state==='RUNDE'){bdg.textContent='RUNDE '+d.lapRound;bdg.className='bdg sr';}"
    "else{bdg.textContent=d.state;bdg.className='bdg '+(cls[d.state]||'si');}"
    "var cw=document.getElementById('cwrap');"
    "var la=document.getElementById('lap-area');"
    "if(d.lapMode){"
    "if(la)la.style.display='block';"
    "if(cw)cw.style.display='none';"
    "var e;"
    "e=document.getElementById('lap-r');if(e)e.textContent=d.lapRound||0;"
    "e=document.getElementById('lap-l');if(e)e.textContent=d.lapLast>0?_f(d.lapLast):'--:--.---';"
    "e=document.getElementById('lap-b');if(e)e.textContent=d.lapBest>0?_f(d.lapBest):'--:--.---';"
    "if(d.state==='RUNDE'){_B=d.liveMs;_P=Date.now();_S='LAEUFT';_startTimer();}"
    "else{_stopTimer();_S=d.state;_el.textContent=d.liveMs>0?_f(d.liveMs):'--:--.---';}"
    "}else{"
    "if(la)la.style.display='none';"
    "if(cw)cw.style.display=d.state==='LAEUFT'?'block':'none';"
    "if(d.state==='LAEUFT'){_B=d.liveMs;_P=Date.now();_S='LAEUFT';"
    "var _te=document.getElementById('_T');if(_te){_te.textContent=_f(_B);_el=_te;}"
    "_startTimer();}"
    "else{_stopTimer();_S=d.state;"
    "var _te2=document.getElementById('_T');if(_te2)_te2.textContent=d.liveMs>0?_f(d.liveMs):'--:--.---';}"
    "}"
    "document.getElementById('t-last').textContent=d.lastMs>0?_f(d.lastMs):'--:--.---';"
    "document.getElementById('t-best').textContent=d.bestMs>0?_f(d.bestMs):'--:--.---';"
    "var sa=document.getElementById('split-area');"
    "var st=document.getElementById('split-t');"
    "if(d.splitMs>0&&d.splitAge<30){if(sa)sa.style.display='block';if(st)st.textContent=_f(d.splitMs);}"
    "else{if(sa)sa.style.display='none';}"
    "var fl=!_firstPoll&&d.histCnt>_H;"
    "var _prevBest=_lastBest;"
    "if(d.histCnt!==_H||_firstPoll){_H=d.histCnt;_hist(d,fl);}"
    "_sig(d);_bat(d);_loraTab(d);"
    "var fc,ft2,sc,st3;"
    "if(d.finSince===undefined||d.finSince>=9999){fc='#444';ft2='Kein Signal';}"
    "else if(d.finSince<35){fc='#4caf50';ft2='Verbunden';}"
    "else if(d.finSince<70){fc='#ff9800';ft2='Schwach';}"
    "else{fc='#f44336';ft2='Getrennt';}"
    "if(d.splSince===undefined||d.splSince>=9999){sc='#444';st3='Kein Signal';}"
    "else if(d.splSince<35){sc='#4caf50';st3='Verbunden';}"
    "else if(d.splSince<70){sc='#ff9800';st3='Schwach';}"
    "else{sc='#f44336';st3='Getrennt';}"
    "var ef=document.getElementById('dev-fin'),es=document.getElementById('dev-spl');"
    "if(ef)ef.innerHTML='<span class=\"dot\" style=\"background:'+fc+'\"></span>Ziel: '+ft2;"
    "if(es)es.innerHTML='<span class=\"dot\" style=\"background:'+sc+'\"></span>Split: '+st3;"
    "var si=document.getElementById('sync-ind');"
    "if(si)si.innerHTML=(d.timeSynced&&d.syncAgo<300)?'\\u2713 Uhr sync':'\\u26A0 Uhr nicht sync';"
    "var sdb=document.getElementById('sd-badge');"
    "if(sdb)sdb.innerHTML=d.sdPresent?'<span style=\"color:#4caf50\">&#10003; Eingelegt</span>':'<span style=\"color:#555\">Nicht eingelegt</span>';"
    // Tracking-State aus Poll aktualisieren
    "if(typeof d.condition!=='undefined'&&d.condition!==_activeCond){"
    "_activeCond=d.condition;_loadCondBtns();_updateCondPill(d.condition);}"
    "if(typeof d.trackId!=='undefined')_activeTrackId=d.trackId;"
    "if(d.sdPresent)_updateSessionCard(d);"
    "if(d.state==='ERGEBNIS'&&_lastState2!=='ERGEBNIS'&&!_firstPoll)_showOv(d);"
    "if(!_firstPoll&&_lastState2!=='LAEUFT'&&d.state==='LAEUFT')_beep(880,.15);"
    "if(!_firstPoll&&d.bestMs>0&&_prevBest>0&&d.bestMs<_prevBest){"
    "var be=document.getElementById('t-best');"
    "if(be){be.classList.remove('newbest');void be.offsetWidth;be.classList.add('newbest');}}"
    "if(_viewerOpen){"
    "var vt=document.getElementById('vw-time');"
    "if(vt){var _te5=document.getElementById('_T');vt.textContent=_te5?_te5.textContent:'--:--.---';}"
    "var vs=document.getElementById('vw-state');if(vs)vs.textContent=d.state;"
    "var vl=document.getElementById('vw-last');if(vl)vl.textContent=d.lastMs>0?_f(d.lastMs):'--:--.---';"
    "var vb=document.getElementById('vw-best');if(vb)vb.textContent=d.bestMs>0?_f(d.bestMs):'--:--.---';}"
    "if(d.bat&&d.bat.pct!==undefined)_batWarn(d.bat.pct);"
    "if(d.stagMode&&typeof d.stagNextIn!=='undefined')_stagStartCd(d.stagNextIn);"
    "_lastState2=d.state;"
    "_firstPoll=false;"
    "}).catch(function(){document.getElementById('_ld').className='ld off';_setRC(true);setTimeout(_poll,1000);});}"
    "setInterval(_poll,2000);_poll();";
  server.sendContent(html); html = ""; html.reserve(16384);
  html +=
    // ── SD-Tracking JavaScript ──────────────────────────
    "var _sdPage=0,_sdFile='';"
    "var _condIcons=['&mdash;','&#9728; Trocken','&#128167; Nass','&#127810; Laubig','&#10052; Eis'];"
    "var _condColors=['#1c1c1e','#221500','#001a2a','#1a1500','#0d1a25'];"
    "var _condTxtC=['#555','#f0a500','#64b5f6','#ffc107','#90caf9'];"
    "var _activeCond=" + String(cfg_condition) + ";"
    "var _activeTrackId=" + String(cfg_track_id) + ";"
    "var _activeSub='trk';"  // aktiver Sub-Tab

    // Sub-Navigation
    "function _stSub(id){"
    "_activeSub=id;"
    "['trk','rid','top'].forEach(function(s){"
    "var p=document.getElementById('sp-'+s);"
    "var b=document.getElementById('sbn-'+s);"
    "var active=(s===id);"
    "if(p)p.style.display=active?'block':'none';"
    "if(b){b.style.background=active?'#f0a500':'#1c1c1e';"
    "b.style.color=active?'#000':'#888';"
    "b.style.fontWeight=active?'bold':'normal';}"
    "});"
    "if(id==='top')_loadBestlist();"
    "else if(id==='trk')_loadTrackList();"
    "else if(id==='rid')_loadRiderList();"
    "}"

    // Bedingung-Panel toggle
    "function _toggleCondPanel(){"
    "var p=document.getElementById('cond-panel');"
    "if(p)p.style.display=p.style.display==='none'?'block':'none';"
    "}"

    // Bedingung-Pill aktualisieren
    "function _updateCondPill(c){"
    "var pill=document.getElementById('cond-pill');"
    "var lbl=document.getElementById('cond-lbl');"
    "if(!pill||!lbl)return;"
    "pill.style.background=_condColors[c]||'#1c1c1e';"
    "pill.style.color=_condTxtC[c]||'#555';"
    "lbl.textContent=c>0?_condIcons[c].replace(/&#[^;]+;\\s*/g,'').trim()||('Bedingung '+c):'Bedingung';"
    // Pill-Text kompakter ohne HTML-Entities
    "var labels=['Bedingung','Trocken','Nass','Laubig','Eis'];"
    "lbl.textContent=labels[c]||'Bedingung';"
    "}"

    // Session-Karte Strecke/Fahrer aktualisieren
    "function _updateSessionCard(d){"
    "var tp=document.getElementById('trkpill');"
    "var rp=document.getElementById('ridpill');"
    "if(tp)tp.innerHTML=d.trackName&&d.trackName.length?d.trackName:'<span style=\"color:#555\">&#8212; keine &#8212;</span>';"
    "if(rp)rp.innerHTML=d.riderName&&d.riderName.length?d.riderName:'<span style=\"color:#555\">&#8212; kein &#8212;</span>';"
    "}"

    // Bedingung-Buttons rendern
    "function _loadCondBtns(){"
    "var el=document.getElementById('cond-btns');if(!el)return;"
    "var h='';"
    "for(var c=0;c<=4;c++){"
    "var act=(_activeCond===c);"
    "h+='<button onclick=\"_setCond('+c+')\" '"
    "+'style=\"flex:1;min-width:56px;padding:8px 4px;border-radius:10px;cursor:pointer;font-size:.82em;'"
    "+'border:2px solid '+(act?'#f0a500':'#2a2a2a')+';'"
    "+'background:'+(act?'#221500':'#111')+';'"
    "+'color:'+(act?'#f0a500':'#aaa')+'\">'+_condIcons[c]+'</button>';"
    "}el.innerHTML=h;"
    "}"
    "function _setCond(c){"
    "fetch('/condition?c='+c).then(function(){"
    "_activeCond=c;"
    "_loadCondBtns();"
    "_updateCondPill(c);"
    // Panel schließen nach Auswahl
    "var p=document.getElementById('cond-panel');if(p)p.style.display='none';"
    "_evtToast('Bedingung: '+['—','Trocken','Nass','Laubig','Eis'][c]||c,'#0d1f0d');"
    "}).catch(function(){});"
    "}"

    // Strecken-Liste
    "function _loadTrackList(){"
    "fetch('/tracks').then(function(r){return r.json();}).then(function(d){"
    "var el=document.getElementById('track-list');if(!el)return;"
    "if(!d||!d.length){el.innerHTML='<span style=\"color:#555\">Noch keine Strecken.</span>';return;}"
    "var h='';"
    "for(var i=0;i<d.length;i++){"
    "var t=d[i];var act=t.active;"
    "h+='<div style=\"padding:10px 0;border-bottom:1px solid #1c1c1e;display:flex;align-items:center;gap:8px\">';"
    "h+='<div style=\"flex:1;min-width:0\">';"
    "h+='<div style=\"font-weight:bold;font-size:.95em\">'+t.name+'</div>';"
    "if(t.info)h+='<div style=\"color:#555;font-size:.75em\">'+t.info+'</div>';"
    "if(t.best)h+='<div style=\"color:#f0a500;font-size:.75em\">&#9889; '+t.bestFmt+'</div>';"
    "h+='</div>';"
    "h+='<button onclick=\"fetch(\\'/trackset?id='+t.id+'\\').then(function(){_loadTrackList();_loadRiderList();_poll();})\" '"
    "+'style=\"background:'+(act?'#f0a500':'#1c1c1e')+';color:'+(act?'#000':'#888')+';'"
    "+'border:none;border-radius:8px;padding:5px 10px;font-size:.78em;cursor:pointer\">'+(act?'&#10003; Aktiv':'W&auml;hlen')+'</button>';"
    "h+='<button onclick=\"_openTrackEdit('+t.id+')\" style=\"background:#1c1c1e;color:#888;border:none;border-radius:8px;padding:5px 8px;font-size:.78em;cursor:pointer\">&#9998;</button>';"
    "h+='<button onclick=\"if(confirm(\\'Strecke l\\u00f6schen?\\'))fetch(\\'/trackdel?id='+t.id+'\\').then(function(){_loadTrackList()})\" '"
    "+'style=\"background:#200d0d;color:#f44336;border:none;border-radius:8px;padding:5px 8px;font-size:.78em;cursor:pointer\">&#128465;</button>';"
    "h+='</div>';"
    "}el.innerHTML=h;"
    "}).catch(function(){});"
    "}"

    // Fahrer-Liste
    "function _loadRiderList(){"
    "fetch('/riders').then(function(r){return r.json();}).then(function(d){"
    "var el=document.getElementById('rider-list');if(!el)return;"
    "if(!d||!d.length){el.innerHTML='<span style=\"color:#555\">Noch keine Fahrer.</span>';return;}"
    "var h='';"
    "for(var i=0;i<d.length;i++){"
    "var r=d[i];var act=r.active;"
    "h+='<div style=\"padding:10px 0;border-bottom:1px solid #1c1c1e;display:flex;align-items:center;gap:8px\">';"
    "h+='<div style=\"flex:1;min-width:0\">';"
    "h+='<div style=\"font-weight:bold;font-size:.95em\">'+r.name+'</div>';"
    "if(r.best)h+='<div style=\"color:#f0a500;font-size:.75em\">&#9889; '+r.bestFmt+'</div>';"
    "h+='</div>';"
    "h+='<button onclick=\"fetch(\\'/riderset?id='+r.id+'\\').then(function(){_loadRiderList();_poll();})\" '"
    "+'style=\"background:'+(act?'#f0a500':'#1c1c1e')+';color:'+(act?'#000':'#888')+';'"
    "+'border:none;border-radius:8px;padding:5px 10px;font-size:.78em;cursor:pointer\">'+(act?'&#10003; Aktiv':'W&auml;hlen')+'</button>';"
    "h+='<button onclick=\"_openRiderEdit('+r.id+')\" style=\"background:#1c1c1e;color:#888;border:none;border-radius:8px;padding:5px 8px;font-size:.78em;cursor:pointer\">&#9998;</button>';"
    "h+='<button onclick=\"if(confirm(\\'Fahrer l\\u00f6schen?\\'))fetch(\\'/riderdel?id='+r.id+'\\').then(function(){_loadRiderList()})\" '"
    "+'style=\"background:#200d0d;color:#f44336;border:none;border-radius:8px;padding:5px 8px;font-size:.78em;cursor:pointer\">&#128465;</button>';"
    "h+='</div>';"
    "}el.innerHTML=h;"
    "}).catch(function(){});"
    "}"

    // Stats laden wenn Strecken-Tab aktiv
    "function _loadStats(){"
    "var tid=_activeTrackId;"
    // Alle Strecken laden wenn keine aktiv (zeigt Gesamt-Stats)
    "fetch('/sdstats?id='+tid).then(function(r){return r.json();}).then(function(d){"
    "var el=document.getElementById('stats-body');if(!el)return;"
    "if(!d||!d.total){"
    // Wenn keine Daten für aktive Strecke, zeige Gesamt-Stats
    "if(tid>0){fetch('/sdstats?id=0').then(function(r){return r.json();}).then(function(all){"
    "if(all&&all.total)el.innerHTML='<span style=\"color:#ff9800;font-size:.8em\">Keine Daten f&uuml;r aktive Strecke &mdash; '+"
    "'gesamt '+all.total+' L&auml;ufe (ohne Strecken-Zuordnung)</span>';"
    "else el.innerHTML='<span style=\"color:#555\">Noch keine Messungen auf SD.</span>';"
    "}).catch(function(){el.innerHTML='<span style=\"color:#555\">Noch keine Messungen auf SD.</span>';});}"
    "else el.innerHTML='<span style=\"color:#555\">Noch keine Messungen auf SD.</span>';"
    "return;}"
    "var tr=d.trend>0?'<span style=\"color:#4caf50\">&#8593; Besser</span>':d.trend<0?'<span style=\"color:#f44336\">&#8595; Schlechter</span>':'<span style=\"color:#555\">&#8594; Gleich</span>';"
    "el.innerHTML='<div style=\"display:flex;gap:16px;flex-wrap:wrap\">"
    "<div><div style=\"color:#555;font-size:.7em\">LÄUFE</div><div style=\"font-weight:bold\">'+d.total+'</div></div>"
    "<div><div style=\"color:#555;font-size:.7em\">BEST</div><div style=\"font-weight:bold;color:#f0a500\">'+d.bestFmt+'</div></div>"
    "<div><div style=\"color:#555;font-size:.7em\">&#216;</div><div style=\"font-weight:bold\">'+d.avgFmt+'</div></div>"
    "<div><div style=\"color:#555;font-size:.7em\">TREND</div>'+tr+'</div>"
    "</div>';"
    "}).catch(function(){});"
    "}"

    // Archiv laden
    "function _loadArchive(){"
    "fetch('/sessions').then(function(r){return r.json();}).then(function(d){"
    "var el=document.getElementById('arch-list');if(!el)return;"
    "if(!d||!d.length){el.innerHTML='Noch keine Sessions.';return;}"
    "var h='';for(var i=d.length-1;i>=0;i--){"
    "h+='<div style=\"padding:6px 0;border-bottom:1px solid #1c1c1e;display:flex;align-items:center;gap:8px\">';"
    "h+='<div style=\"flex:1;font-size:.88em\">'+d[i]+'</div>';"
    "h+='<button onclick=\"_openSession(\\''+d[i]+'\\')\" style=\"background:#1c1c1e;color:#888;border:none;border-radius:8px;padding:5px 10px;font-size:.75em;cursor:pointer\">Anzeigen</button>';"
    "h+='<a href=\"/export\" style=\"background:#1c1c1e;color:#888;padding:5px 10px;border-radius:8px;font-size:.75em;text-decoration:none\">CSV</a>';"
    "h+='</div>';"
    "}el.innerHTML=h;"
    "}).catch(function(){});"
    "}";
  server.sendContent(html); html = ""; html.reserve(16384);
  html +=
    // Bestenliste laden
    "function _loadBestlist(){"
    "var tid=_activeTrackId;if(!tid)return;"
    "fetch('/bestlist?id='+tid).then(function(r){return r.json();}).then(function(d){"
    "var el=document.getElementById('best-list');if(!el)return;"
    "if(!d||!d.length){el.innerHTML='Noch keine Einträge.';return;}"
    "var h='<table style=\"width:100%\"><tr><th style=\"color:#444;font-size:.7em;text-transform:uppercase;text-align:left;padding-bottom:6px\">#</th>"
    "<th style=\"color:#444;font-size:.7em;text-transform:uppercase;text-align:left\">Fahrer</th>"
    "<th style=\"color:#444;font-size:.7em;text-transform:uppercase;text-align:right\">Zeit</th>"
    "<th style=\"color:#444;font-size:.7em;text-transform:uppercase;text-align:right\">Datum</th></tr>';"
    "for(var i=0;i<d.length;i++){"
    "var cls=i===0?'style=\"color:#f0a500;font-weight:bold\"':'style=\"color:#aaa\"';"
    "h+='<tr '+cls+'><td>'+d[i].rank+'</td><td>'+d[i].rider+'</td>"
    "<td style=\"text-align:right;font-variant-numeric:tabular-nums\">'+d[i].time+'</td>"
    "<td style=\"text-align:right;color:#555;font-size:.8em\">'+d[i].date+'</td></tr>';"
    "}h+='</table>';el.innerHTML=h;"
    "}).catch(function(){});"
    "}"

    // Session-Modal anzeigen
    "var _sessModal=null;"
    "function _openSession(f){"
    "_sdFile=f;_sdPage=0;"
    "var m=document.createElement('div');"
    "m.style.cssText='position:fixed;inset:0;background:rgba(0,0,0,.95);z-index:250;overflow-y:auto;padding:16px';"
    "m.id='sess-modal';"
    "m.innerHTML='<div style=\"display:flex;justify-content:space-between;align-items:center;margin-bottom:12px\">"
    "<div style=\"font-weight:bold;color:#f0a500\">&#128194; '+f+'</div>"
    "<button onclick=\"document.getElementById(\\'sess-modal\\').remove()\" "
    "style=\"background:none;border:none;color:#555;font-size:1.4em;cursor:pointer\">&#10005;</button>"
    "</div><div id=\"sess-tbl\">Lade...</div>"
    "<button id=\"sess-more\" onclick=\"_loadSessionMore()\" "
    "style=\"display:none;width:100%;margin-top:10px;background:#1c1c1e;color:#888;border:none;border-radius:10px;padding:10px;cursor:pointer\">Mehr laden</button>';"
    "document.body.appendChild(m);"
    "_sessModal=m;_loadSessionPage(0);"
    "}"
    "function _loadSessionPage(p){"
    "fetch('/sessiondata?f='+_sdFile+'&p='+p).then(function(r){return r.json();}).then(function(d){"
    "var el=document.getElementById('sess-tbl');if(!el)return;"
    "var h=(p===0?'<table style=\"width:100%;font-size:.85em\"><tr>"
    "<th style=\"color:#444;text-align:left\">Strecke</th><th style=\"color:#444;text-align:left\">Fahrer</th>"
    "<th style=\"color:#444;text-align:right\">Zeit</th><th style=\"color:#444;text-align:left\">Bed.</th></tr>':el.innerHTML.replace('</table>',''));"
    "for(var i=0;i<d.length;i++){"
    "h+='<tr><td style=\"color:#aaa\">'+d[i].track+'</td><td>'+d[i].rider+'</td>"
    "<td style=\"text-align:right;color:#f0a500;font-variant-numeric:tabular-nums\">'+d[i].time+'</td>"
    "<td style=\"color:#555\">'+d[i].cond+'</td></tr>';"
    "}h+='</table>';el.innerHTML=h;"
    "var mb=document.getElementById('sess-more');"
    "if(mb)mb.style.display=d.length>=50?'block':'none';"
    "}).catch(function(){});"
    "}"
    "function _loadSessionMore(){_sdPage++;_loadSessionPage(_sdPage);}"

    // Mehr laden (Verlauf-Tab)
    "var _histPage=-1;"  // -1 = noch nicht geladen; erstes Laden gibt Seite 0
    "function _loadMore(){"
    "if(!_sdFile){"
    "fetch('/sessions').then(function(r){return r.json();}).then(function(d){"
    "if(d&&d.length){_sdFile=d[d.length-1];_loadMorePage();}"
    "}).catch(function(){});"
    "} else { _loadMorePage(); }"
    "}"
    "function _loadMorePage(){"
    "_histPage++;"
    "fetch('/sessiondata?f='+_sdFile+'&p='+_histPage).then(function(r){return r.json();}).then(function(d){"
    "var tbl=document.getElementById('h-tbl');if(!tbl||!d||!d.length)return;"
    "var t=tbl.querySelector('#sd-extra');"
    "if(!t){var nt=document.createElement('table');nt.id='sd-extra';tbl.appendChild(nt);t=nt;}"
    "for(var i=0;i<d.length;i++){"
    "var tr2=document.createElement('tr');tr2.innerHTML="
    "'<td style=\"color:#555;font-size:.8em\">'+d[i].track+'</td><td style=\"color:#aaa\">'+d[i].rider+'</td>"
    "<td style=\"font-variant-numeric:tabular-nums;color:#eee\">'+d[i].time+'</td>"
    "<td style=\"color:#555;font-size:.8em\">'+d[i].date+'</td>';"
    "t.appendChild(tr2);"
    "}"
    "var lmb=document.getElementById('load-more-btn');"
    "if(lmb)lmb.style.display=d.length>=50?'block':'none';"
    "}).catch(function(){});"
    "}"

    // Track/Rider Edit Modal
    "function _openTrackEdit(id){"
    "var name='',info='';"
    "if(id>0){"
    "fetch('/tracks').then(function(r){return r.json();}).then(function(d){"
    "for(var i=0;i<d.length;i++){if(d[i].id===id){name=d[i].name;info=d[i].info;break;}}"
    "_showTrackModal(id,name,info);"
    "}).catch(function(){_showTrackModal(id,'','');});} else {_showTrackModal(0,'','');}"
    "}"
    "function _showTrackModal(id,name,info){"
    "var ttl=id?'Strecke bearbeiten':'Neue Strecke';"
    "var m=document.createElement('div');"
    "m.style.cssText='position:fixed;inset:0;background:rgba(0,0,0,.95);z-index:250;display:flex;align-items:center;justify-content:center;padding:16px';"
    "m.innerHTML='<div style=\"background:#1c1c1e;border-radius:16px;padding:20px;width:100%;max-width:400px\">"
    "<div style=\"font-weight:bold;margin-bottom:12px;color:#f0a500\">'+ttl+'</div>"
    "<div style=\"color:#555;font-size:.72em;text-transform:uppercase;margin-bottom:4px\">Name</div>"
    "<input id=\"te-name\" type=\"text\" maxlength=\"20\" value=\"'+name+'\" "
    "style=\"width:100%;background:#111;color:#eee;border:1px solid #2a2a2a;border-radius:8px;padding:10px;font-size:1em;box-sizing:border-box\">"
    "<div style=\"color:#555;font-size:.72em;text-transform:uppercase;margin-top:10px;margin-bottom:4px\">Info (optional)</div>"
    "<input id=\"te-info\" type=\"text\" maxlength=\"40\" value=\"'+info+'\" "
    "style=\"width:100%;background:#111;color:#eee;border:1px solid #2a2a2a;border-radius:8px;padding:10px;font-size:1em;box-sizing:border-box\">"
    "<div style=\"display:flex;gap:8px;margin-top:14px\">"
    "<button onclick=\"this.closest(\\'div\\').closest(\\'div\\').remove()\" "
    "style=\"flex:1;background:#1a1a1a;color:#777;border:1px solid #232323;border-radius:8px;padding:12px;cursor:pointer\">Abbrechen</button>"
    "<button onclick=\"_saveTrack('+id+')\" "
    "style=\"flex:1;background:#f0a500;color:#000;border:none;border-radius:8px;padding:12px;font-weight:bold;cursor:pointer\">Speichern</button>"
    "</div></div>';"
    "document.body.appendChild(m);"
    "}"
    "function _saveTrack(id){"
    "var n=document.getElementById('te-name').value.trim();"
    "var info=document.getElementById('te-info').value.trim();"
    "if(!n){_evtToast('Name darf nicht leer sein','#3a0a0a');return;}"
    "var fd=new FormData();fd.append('id',id);fd.append('name',n);fd.append('info',info);"
    "fetch('/tracksave',{method:'POST',body:fd}).then(function(r){return r.json();})"
    ".then(function(d){"
    "if(!d.ok){_evtToast(d.msg||'Fehler beim Speichern','#3a0a0a');return;}"
    // Modal schließen: input-Element → übergeordnetes Modal-Div
    "var inp=document.getElementById('te-name');"
    "if(inp){var m=inp.closest('div[style*=\"inset\"]');if(m)m.remove();}"
    "_loadTrackList();_loadRiderList();_evtToast('Strecke gespeichert','#0d1f0d');"
    "}).catch(function(){_evtToast('Verbindungsfehler','#3a0a0a');});"
    "}"
    "function _openRiderEdit(id){"
    "var name='';"
    "if(id>0){"
    "fetch('/riders').then(function(r){return r.json();}).then(function(d){"
    "for(var i=0;i<d.length;i++){if(d[i].id===id){name=d[i].name;break;}}"
    "_showRiderModal(id,name);"
    "});} else {_showRiderModal(0,'');}"
    "}"
    "function _showRiderModal(id,name){"
    "var ttl2=id?'Fahrer bearbeiten':'Neuer Fahrer';"
    "var m=document.createElement('div');"
    "m.style.cssText='position:fixed;inset:0;background:rgba(0,0,0,.95);z-index:250;display:flex;align-items:center;justify-content:center;padding:16px';"
    "m.innerHTML='<div style=\"background:#1c1c1e;border-radius:16px;padding:20px;width:100%;max-width:400px\">"
    "<div style=\"font-weight:bold;margin-bottom:12px;color:#f0a500\">'+ttl2+'</div>"
    "<div style=\"color:#555;font-size:.72em;text-transform:uppercase;margin-bottom:4px\">Name</div>"
    "<input id=\"re-name\" type=\"text\" maxlength=\"20\" value=\"'+name+'\" "
    "style=\"width:100%;background:#111;color:#eee;border:1px solid #2a2a2a;border-radius:8px;padding:10px;font-size:1em;box-sizing:border-box\">"
    "<div style=\"display:flex;gap:8px;margin-top:14px\">"
    "<button onclick=\"this.closest(\\'div\\').closest(\\'div\\').remove()\" "
    "style=\"flex:1;background:#1a1a1a;color:#777;border:1px solid #232323;border-radius:8px;padding:12px;cursor:pointer\">Abbrechen</button>"
    "<button onclick=\"_saveRider('+id+')\" "
    "style=\"flex:1;background:#f0a500;color:#000;border:none;border-radius:8px;padding:12px;font-weight:bold;cursor:pointer\">Speichern</button>"
    "</div></div>';"
    "document.body.appendChild(m);"
    "}"
    "function _saveRider(id){"
    "var n=document.getElementById('re-name').value.trim();"
    "if(!n){_evtToast('Name darf nicht leer sein','#3a0a0a');return;}"
    "var fd=new FormData();fd.append('id',id);fd.append('name',n);"
    "fetch('/ridersave',{method:'POST',body:fd}).then(function(r){return r.json();})"
    ".then(function(d){"
    "if(!d.ok){_evtToast(d.msg||'Fehler beim Speichern','#3a0a0a');return;}"
    "var inp=document.getElementById('re-name');"
    "if(inp){var m=inp.closest('div[style*=\"inset\"]');if(m)m.remove();}"
    "_loadRiderList();_evtToast('Fahrer gespeichert','#0d1f0d');"
    "}).catch(function(){_evtToast('Verbindungsfehler','#3a0a0a');});"
    "}"

    // Inline-Edit für Fahrernamen im Verlauf
    "function _editName(idx,current){"
    "var cell=document.getElementById('nm-'+idx);"
    "if(!cell)return;"
    "cell.innerHTML='<input id=\"ne-'+idx+'\" type=\"text\" value=\"'+current+'\" maxlength=\"20\" "
    "style=\"background:#111;color:#eee;border:1px solid #f0a500;border-radius:6px;padding:3px 6px;font-size:.88em;width:100px\">"
    "<button onclick=\"_submitName('+idx+')\" style=\"background:#f0a500;color:#000;border:none;border-radius:6px;padding:3px 8px;margin-left:4px;cursor:pointer\">&#10003;</button>';"
    "}"
    "function _submitName(idx){"
    "var inp=document.getElementById('ne-'+idx);if(!inp)return;"
    "var fd=new FormData();fd.append('idx',idx);fd.append('name',inp.value.trim());"
    "fetch('/editname',{method:'POST',body:fd}).then(function(r){return r.json();}).then(function(d){"
    "if(d.ok)_poll();"
    "}).catch(function(){});"
    "}"

    // Stats beim ersten Laden des hist-Tabs
    "(function(){var _it=new URLSearchParams(location.search).get('tab')||localStorage.getItem('_tab');if(_it==='hist')setTimeout(_loadStats,600);})();"

    // ── RTT-Messung ───────────────────────────────────────────
    "function doMeasure(){"
    "var btn=document.getElementById('meas-btn');"
    "var res=document.getElementById('meas-res');"
    "if(!btn||btn.disabled)return;"
    "btn.disabled=true;"
    "var mfr=document.getElementById('meas-fin-rtt');"
    "var msr=document.getElementById('meas-spl-rtt');"
    "if(mfr)mfr.textContent='';if(msr)msr.textContent='';"
    "res.innerHTML='<span style=\"color:#ff9800\">&#128225; Sende Ping...</span>';"
    "var t0=Date.now();var finDone=false,splDone=false;"
    "fetch('/ping').then(function(r){return r.json();}).then(function(d){"
    "if(!d.sent){"
    "res.innerHTML='<span style=\"color:#f44336\">&#10007; Nur im Bereit-Zustand m\\u00f6glich</span>';"
    "setTimeout(function(){btn.disabled=false;res.textContent='';},3000);return;}"
    "res.innerHTML='<span style=\"color:#ff9800\">&#8987; Warte auf Antworten...</span>';"
    "var ck=setInterval(function(){"
    "fetch('/state').then(function(r){return r.json();}).then(function(sd){"
    "var elapsed=Date.now()-t0;"
    "var fs=sd.finSince!==undefined?sd.finSince:9999;"
    "var ss=sd.splSince!==undefined?sd.splSince:9999;"
    "if(!finDone&&fs<3){finDone=true;if(mfr)mfr.innerHTML=' <span style=\"color:#4caf50\">('+elapsed+' ms)</span>';}"
    "if(!splDone&&ss<3){splDone=true;if(msr)msr.innerHTML=' <span style=\"color:#4caf50\">('+elapsed+' ms)</span>';}"
    "if((finDone||splDone)&&sd.rtt>0){"
    "clearInterval(ck);"
    "var half=Math.round(sd.rtt/2);"
    "res.innerHTML='<span style=\"color:#4caf50\">&#10003; RTT: '+sd.rtt+' ms &bull; Empfehlung: '+half+' ms</span>';"
    "_poll();setTimeout(function(){btn.disabled=false;res.textContent='';},5000);}"
    "else if(elapsed>8000){"
    "clearInterval(ck);"
    "res.innerHTML=finDone||splDone?'<span style=\"color:#ff9800\">&#10003; Antwort erhalten</span>'"
    ":'<span style=\"color:#f44336\">&#10007; Keine Antwort (8 s)</span>';"
    "setTimeout(function(){btn.disabled=false;res.textContent='';},4000);}}"
    ").catch(function(){clearInterval(ck);btn.disabled=false;res.textContent='';});},300);"
    "}).catch(function(){res.innerHTML='<span style=\"color:#f44336\">&#10007; Fehler</span>';btn.disabled=false;});}"
    "function _applyComp(){"
    "var rh=document.getElementById('rtt-half');if(!rh)return;"
    "var val=parseInt(rh.textContent);if(isNaN(val)||val<=0)return;"
    "_st('cfg',document.getElementById('bn-cfg'));"
    "var inp=document.querySelector('input[name=\"loracomp\"]');"
    "if(inp){inp.value=val;}"
    "_evtToast('Wert '+val+' ms eingetragen \\u2013 bitte Speichern!','#0d1f2d');}"
    "function doSync(){"
    "fetch('/settime?ts='+Date.now()).then(function(){"
    "_evtToast('\\u2713 Uhrzeit synchronisiert','#0d1f0d');_poll();"
    "}).catch(function(){_evtToast('Sync fehlgeschlagen','#2a1a00');});}"
    // ── Auto-Messung alle 5 Minuten ───────────────────────────
    "var _autoMeasTm=null;"
    "function _schedAutoMeas(){"
    "if(_autoMeasTm)clearTimeout(_autoMeasTm);"
    "_autoMeasTm=setTimeout(function(){"
    "fetch('/state').then(function(r){return r.json();}).then(function(d){"
    "if(d.state==='BEREIT'&&(d.finSince<35||d.splSince<35))doMeasure();"
    "_schedAutoMeas();"
    "}).catch(function(){_schedAutoMeas();});},300000);}"
    "_schedAutoMeas();"
    // ── Screen Wake Lock ──────────────────────────────────────
    "var _wl=null;"
    "function _reqWL(){if('wakeLock' in navigator)navigator.wakeLock.request('screen')"
    ".then(function(w){_wl=w;}).catch(function(){});}"
    "document.addEventListener('visibilitychange',function(){if(document.visibilityState==='visible'){_reqWL();_poll();}});"
    "_reqWL();"
    // ── Sound ─────────────────────────────────────────────────
    "var _soundOn=localStorage.getItem('_sound')!=='off';"
    "function _applySound(){"
    "var b=document.getElementById('sound-btn');"
    "if(b)b.textContent='\\uD83D\\uDD0A Sound: '+(_soundOn?'AN':'AUS');}"
    "function _toggleSound(){"
    "_soundOn=!_soundOn;localStorage.setItem('_sound',_soundOn?'on':'off');_applySound();}"
    "_applySound();"
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
    // ── Ergebnis-Overlay ─────────────────────────────────────
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
    "var dv=d.lastMs-d.bestMs;"
    "dEl.textContent='+'+_f(dv)+' zur Bestzeit';dEl.style.color='#aaa';"
    "bEl.textContent='Bestzeit: '+_f(d.bestMs);}"
    "else{dEl.textContent='';bEl.textContent='';}"
    "ov.style.display='flex';"
    "if(isBest){_beep(660,.1);setTimeout(function(){_beep(880,.1);},150);setTimeout(function(){_beep(1100,.3);},300);}"
    "else{_beep(660,.2);setTimeout(function(){_beep(880,.25);},250);}"
    "if(navigator.vibrate)navigator.vibrate(isBest?[200,100,200,100,300]:[200,100,200]);"
    "if(_roTm)clearTimeout(_roTm);"
    "_roTm=setTimeout(_closeOv,d.cfg&&d.cfg.result_show?d.cfg.result_show:8000);}"
    "function _closeOv(){var ov=document.getElementById('res-ov');if(ov)ov.style.display='none';if(_roTm)clearTimeout(_roTm);}"
    // ── Vollbild ──────────────────────────────────────────────
    "function _fs(){if(!document.fullscreenElement)document.documentElement.requestFullscreen().catch(function(){});else document.exitFullscreen();}"
    // ── Teilen ────────────────────────────────────────────────
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
    "if(ok)_evtToast('\\uD83D\\uDCCB Kopiert!','#0d1f0d');"
    "else _evtToast('Tippe & halte zum Kopieren','#1a2a3a');"
    "}catch(e){_evtToast('Kopieren nicht m\\u00f6glich','#2a1a00');}}"
    // ── Reconnect-Banner ─────────────────────────────────────
    "function _setRC(show){"
    "var b=document.getElementById('rc-banner');if(b)b.style.display=show?'block':'none';}"
    // ── Zuschauer-Modus ───────────────────────────────────────
    "var _viewerOpen=false;"
    "function _openViewer(){var v=document.getElementById('viewer');if(v){v.style.display='flex';_viewerOpen=true;}}"
    "function _closeViewer(){var v=document.getElementById('viewer');if(v){v.style.display='none';_viewerOpen=false;}}"
    "document.addEventListener('keydown',function(e){if(e.key==='Escape')_closeViewer();});"
    "document.addEventListener('dblclick',function(e){"
    "if(_viewerOpen&&document.getElementById('viewer')&&!document.getElementById('viewer').contains(e.target))_closeViewer();});"
    // ── Akku-Warnung ──────────────────────────────────────────
    "function _batWarn(pct){"
    "var bw=document.getElementById('bat-warn');if(!bw)return;"
    "if(pct<=5){bw.style.display='block';bw.style.background='#f44336';bw.style.color='#fff';"
    "bw.className='batblink';bw.textContent='\\u26A0 AKKU KRITISCH: '+pct+'%';}"
    "else if(pct<=15){bw.style.display='block';bw.style.background='#ff9800';bw.style.color='#000';"
    "bw.className='';bw.textContent='\\u26A0 Akku niedrig: '+pct+'%';}"
    "else{bw.style.display='none';bw.className='';}}"
    // ── Versetzter Start: Countdown ───────────────────────
    "var _stagCdTm=null,_stagCdEnd=0;"
    "function _stagStartCd(nextInMs){"
    "if(_stagCdTm)clearInterval(_stagCdTm);"
    "if(nextInMs<=0){var e=document.getElementById('stag-cd');if(e)e.textContent='\\u2713 Los!';return;}"
    "_stagCdEnd=Date.now()+nextInMs;"
    "_stagCdTm=setInterval(function(){"
    "var rem=Math.max(0,_stagCdEnd-Date.now());"
    "var e=document.getElementById('stag-cd');"
    "if(e){var m=Math.floor(rem/60000),s=Math.floor((rem%60000)/1000);"
    "e.textContent=('0'+m).slice(-2)+':'+('0'+s).slice(-2);}"
    "if(rem===0){clearInterval(_stagCdTm);_stagCdTm=null;}},200);}"
    "if(_SM&&document.getElementById('stag-cd'))_stagStartCd(0);"
    "</script>"
    "<nav class='bnav'>"
    "<button class='bni' id='bn-live' onclick=\"_st('live',this)\">"
    "<svg viewBox='0 0 24 24'><polyline points='2,12 6,6 10,18 14,8 18,12 22,12'/></svg>Live</button>"
    "<button class='bni' id='bn-hist' onclick=\"_st('hist',this);_histPage=-1;_sdFile='';setTimeout(_loadStats,200)\">"
    "<svg viewBox='0 0 24 24'><circle cx='12' cy='12' r='9'/><polyline points='12,6 12,12 16,12'/></svg>Verlauf</button>"
    "<button class='bni' id='bn-modus' onclick=\"_st('modus',this)\">"
    "<svg viewBox='0 0 24 24'><line x1='2' y1='22' x2='22' y2='2'/><line x1='7' y1='2' x2='2' y2='7'/><line x1='17' y1='22' x2='22' y2='17'/></svg>Modus</button>"
    + (sdPresent ? String(
    "<button class='bni' id='bn-track' onclick=\"_st('track',this);_loadCondBtns();_stSub(_activeSub||'trk')\">"
    "<svg viewBox='0 0 24 24'><path d='M3 12h18M3 6l9-3 9 3M3 18l9 3 9-3'/></svg>Strecken</button>") : String("")) +
    "<button class='bni' id='bn-cfg' onclick=\"_st('cfg',this)\">"
    "<svg viewBox='0 0 24 24'><circle cx='12' cy='12' r='3'/>"
    "<path d='M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z'/></svg>Einst.</button>"
    "</nav>"
    // ── Ergebnis-Overlay ─────────────────────────────────────
    "<div id='res-ov' style='display:none;position:fixed;inset:0;background:rgba(0,0,0,.93);"
    "z-index:200;flex-direction:column;align-items:center;justify-content:center;padding:24px;text-align:center'>"
    "<div style='font-size:.9em;color:#555;margin-bottom:8px'>&#127937; ERGEBNIS</div>"
    "<div id='ro-name' style='font-size:1.1em;color:#aaa;margin-bottom:4px;min-height:1.4em'></div>"
    "<div id='ro-time' style='font-size:4.5em;font-weight:bold;color:#f0a500;"
    "font-variant-numeric:tabular-nums;line-height:1.1'></div>"
    "<div id='ro-delta' style='font-size:1.15em;color:#aaa;margin-top:12px'></div>"
    "<div id='ro-bst' style='font-size:.9em;color:#555;margin-top:4px'></div>"
    "<button onclick='_closeOv()' style='margin-top:24px;background:#1c1c1e;border:1px solid #2a2a2a;"
    "color:#eee;border-radius:12px;padding:12px 32px;font-size:1em;cursor:pointer'>Weiter</button></div>"
    // ── Zuschauer-Overlay ────────────────────────────────────
    "<div id='viewer' style='display:none;position:fixed;inset:0;background:var(--bg);"
    "z-index:180;flex-direction:column;align-items:center;justify-content:center;gap:8px'>"
    "<div id='vw-state' style='font-size:.9em;color:var(--sub);text-transform:uppercase;letter-spacing:.1em'></div>"
    "<div id='vw-time' style='font-size:5.5em;font-weight:bold;color:var(--acc);"
    "font-variant-numeric:tabular-nums;line-height:1'></div>"
    "<div style='display:flex;gap:40px;margin-top:16px'>"
    "<div style='text-align:center'><div style='color:var(--sub);font-size:.75em;text-transform:uppercase'>Letzte</div>"
    "<div id='vw-last' style='font-size:2em;font-weight:bold;color:var(--txt)'></div></div>"
    "<div style='text-align:center'><div style='color:var(--sub);font-size:.75em;text-transform:uppercase'>Beste</div>"
    "<div id='vw-best' style='font-size:2em;font-weight:bold;color:var(--acc)'></div></div>"
    "</div>"
    "<button onclick='_closeViewer()' style='position:absolute;top:16px;right:16px;"
    "background:none;border:none;color:var(--sub);font-size:1.8em;cursor:pointer;padding:8px'>&#10005;</button>"
    "</div>"
    "</body></html>";

  server.sendContent(html);
  server.sendContent(""); // HTTP chunked end
}

void handleRoot() { sendHTML(); }

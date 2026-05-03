// ── SD-Karten-Log v2 ───────────────────────────────────────
// Strecken, Fahrerprofile, Tages-Sessions, globales Log
// Hardware-Pins + SDSPI definiert in MTB_Timer_Start_LILYGO.ino

// Track/Rider-Strukturen und Arrays sind in MTB_Timer_Start_LILYGO.ino definiert
// (müssen vor allen Forward-Declarations stehen → Haupt-.ino wird zuerst kompiliert)

// Aktive Session-Datei
static char sessionFile[32] = "";

// ── JSON-Hilfsfunktionen (manuell, ohne ArduinoJSON) ───────
// Liest String-Wert nach "key":" aus src, schreibt in dst (max dstLen)
static bool jsonGetStr(const char* src, const char* key, char* dst, uint8_t dstLen) {
  char pat[28]; snprintf(pat, sizeof(pat), "\"%s\":\"", key);
  const char* p = strstr(src, pat);
  if (!p) return false;
  p += strlen(pat);
  const char* end = strchr(p, '"');
  if (!end) return false;
  uint8_t len = (uint8_t)min((int)(end - p), (int)(dstLen - 1));
  strncpy(dst, p, len); dst[len] = '\0';
  return true;
}

// Liest unsigned long nach "key": aus src
static bool jsonGetUL(const char* src, const char* key, unsigned long* dst) {
  char pat[28]; snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(src, pat);
  if (!p) return false;
  p += strlen(pat);
  char* ep; unsigned long v = strtoul(p, &ep, 10);
  if (ep == p) return false;
  *dst = v; return true;
}

// Liest uint8_t nach "key": aus src
static bool jsonGetU8(const char* src, const char* key, uint8_t* dst) {
  unsigned long v; if (!jsonGetUL(src, key, &v)) return false;
  *dst = (uint8_t)v; return true;
}

// ── Datums-String (für Dateinamen) ─────────────────────────
static void dateStr(int64_t tsMs, char* out, uint8_t len) {
  if (tsMs <= 0) { strncpy(out, "00000000", len); return; }
  time_t t = (time_t)(tsMs / 1000LL);
  struct tm* tm2 = gmtime(&t);
  snprintf(out, len, "%04d%02d%02d", tm2->tm_year + 1900, tm2->tm_mon + 1, tm2->tm_mday);
}

static void dateTimeStr(int64_t tsMs, char* out, uint8_t len) {
  if (tsMs <= 0) { strncpy(out, "", len); return; }
  time_t t = (time_t)(tsMs / 1000LL);
  struct tm* tm2 = gmtime(&t);
  snprintf(out, len, "%02d.%02d.%04d %02d:%02d",
           tm2->tm_mday, tm2->tm_mon + 1, tm2->tm_year + 1900,
           tm2->tm_hour, tm2->tm_min);
}

// ── Aktuelle Session-Datei bestimmen ───────────────────────
static void updateSessionFile() {
  if (!sdPresent || !timeIsSynced) { sessionFile[0] = '\0'; return; }
  char dateS[10]; dateStr(nowUnixMs(), dateS, sizeof(dateS));
  // Datum der Session prüfen → wenn neuer Tag, sessionnum zurücksetzen
  uint32_t today = (uint32_t)(nowUnixMs() / 86400000LL);
  if (today != cfg_session_day) {
    cfg_session_day = today;
    cfg_session_num = 1;
    // NVS schreiben
    prefs.begin("mtb-cfg-l", false);
    prefs.putUInt("sessionday", cfg_session_day);
    prefs.putUChar("sessionnum", cfg_session_num);
    prefs.end();
  }
  snprintf(sessionFile, sizeof(sessionFile), "/logs/%s_%u.csv", dateS, cfg_session_num);
}

// ── Tracks laden/speichern ─────────────────────────────────
bool sdLoadTracks() {
  trackCount = 0;
  if (!sdPresent) return false;
  File f = SD.open("/tracks.json", FILE_READ);
  if (!f) return false;
  // Datei komplett lesen (max 2 KB)
  char buf[2048] = {}; uint16_t pos = 0;
  while (f.available() && pos < sizeof(buf) - 1) buf[pos++] = (char)f.read();
  f.close();
  // Tracks parsen: suche nach {"id":... } Blöcken
  const char* p = buf;
  while ((p = strstr(p, "\"id\":")) != nullptr && trackCount < MAX_TRACKS) {
    Track& tr = trackList[trackCount];
    memset(&tr, 0, sizeof(Track));
    if (!jsonGetU8(p, "id", &tr.id)) { p++; continue; }
    jsonGetStr(p, "n", tr.name, sizeof(tr.name));
    jsonGetStr(p, "info", tr.info, sizeof(tr.info));
    jsonGetUL(p, "best", &tr.best_ms);
    trackCount++;
    p++;
  }
  DBGF("SD", "Tracks geladen: %u", trackCount);
  return true;
}

bool sdSaveTracks() {
  if (!sdPresent) return false;
  SD.remove("/tracks.json");
  File f = SD.open("/tracks.json", FILE_WRITE);
  if (!f) return false;
  f.print("{\"t\":[");
  for (uint8_t i = 0; i < trackCount; i++) {
    if (i) f.print(",");
    // Namen für JSON maskieren (einfache Quotes durch Leerzeichen ersetzen)
    char sname[21], sinfo[41];
    strncpy(sname, trackList[i].name, sizeof(sname));
    strncpy(sinfo, trackList[i].info, sizeof(sinfo));
    for (char* c = sname; *c; c++) if (*c=='"'||*c=='\\') *c=' ';
    for (char* c = sinfo; *c; c++) if (*c=='"'||*c=='\\') *c=' ';
    char line[120];
    snprintf(line, sizeof(line), "{\"id\":%u,\"n\":\"%s\",\"info\":\"%s\",\"best\":%lu}",
             trackList[i].id, sname, sinfo, trackList[i].best_ms);
    f.print(line);
  }
  f.print("]}");
  f.close();
  return true;
}

// ── Rider laden/speichern ──────────────────────────────────
bool sdLoadRiders() {
  riderCount = 0;
  if (!sdPresent) return false;
  File f = SD.open("/riders.json", FILE_READ);
  if (!f) return false;
  char buf[3072] = {}; uint16_t pos = 0;
  while (f.available() && pos < sizeof(buf) - 1) buf[pos++] = (char)f.read();
  f.close();
  const char* p = buf;
  while ((p = strstr(p, "\"id\":")) != nullptr && riderCount < MAX_RIDERS) {
    Rider& rd = riderList[riderCount];
    memset(&rd, 0, sizeof(Rider));
    if (!jsonGetU8(p, "id", &rd.id)) { p++; continue; }
    jsonGetStr(p, "n", rd.name, sizeof(rd.name));
    // Bestzeiten: "b":{"1":133450,"2":118120}
    const char* bstart = strstr(p, "\"b\":{");
    if (bstart) {
      bstart += 5;
      for (uint8_t t = 0; t < MAX_TRACKS; t++) {
        char tkey[4]; snprintf(tkey, sizeof(tkey), "\"%u\"", t + 1);
        const char* tp = strstr(bstart, tkey);
        if (!tp || strchr(bstart, '}') < tp) break;
        tp += strlen(tkey) + 1;
        char* ep; rd.bests[t] = strtoul(tp, &ep, 10);
      }
    }
    riderCount++;
    p++;
  }
  DBGF("SD", "Rider geladen: %u", riderCount);
  return true;
}

bool sdSaveRiders() {
  if (!sdPresent) return false;
  SD.remove("/riders.json");
  File f = SD.open("/riders.json", FILE_WRITE);
  if (!f) return false;
  f.print("{\"r\":[");
  for (uint8_t i = 0; i < riderCount; i++) {
    if (i) f.print(",");
    char rname[21]; strncpy(rname, riderList[i].name, sizeof(rname));
    for (char* c = rname; *c; c++) if (*c=='"'||*c=='\\') *c=' ';
    f.print("{\"id\":"); f.print(riderList[i].id);
    f.print(",\"n\":\""); f.print(rname); f.print("\"");
    f.print(",\"b\":{");
    bool first = true;
    for (uint8_t t = 0; t < MAX_TRACKS; t++) {
      if (riderList[i].bests[t] > 0) {
        if (!first) f.print(",");
        f.print("\""); f.print(t + 1); f.print("\":"); f.print(riderList[i].bests[t]);
        first = false;
      }
    }
    f.print("}}");
  }
  f.print("]}");
  f.close();
  return true;
}

// ── Hilfsfunktionen: Tracks/Rider per ID finden ───────────
Track* sdFindTrack(uint8_t id) {
  for (uint8_t i = 0; i < trackCount; i++) if (trackList[i].id == id) return &trackList[i];
  return nullptr;
}
Rider* sdFindRider(uint8_t id) {
  for (uint8_t i = 0; i < riderCount; i++) if (riderList[i].id == id) return &riderList[i];
  return nullptr;
}

// ── Bestzeit aktualisieren ─────────────────────────────────
void sdUpdateBest(unsigned long ms) {
  if (!sdPresent || ms == 0) return;
  bool changed = false;
  // Track-Bestzeit
  if (cfg_track_id > 0) {
    Track* tr = sdFindTrack(cfg_track_id);
    if (tr && (tr->best_ms == 0 || ms < tr->best_ms)) { tr->best_ms = ms; changed = true; }
  }
  // Rider-Bestzeit pro Track
  if (cfg_rider_id > 0 && cfg_track_id > 0) {
    Rider* rd = sdFindRider(cfg_rider_id);
    if (rd) {
      uint8_t ti = cfg_track_id - 1;
      if (ti < MAX_TRACKS && (rd->bests[ti] == 0 || ms < rd->bests[ti])) {
        rd->bests[ti] = ms; changed = true;
      }
    }
  }
  if (changed) { sdSaveTracks(); sdSaveRiders(); }
}

// ── Session-Init: erstellt Datei mit Header ────────────────
static void sdInitSessionFile() {
  if (!sdPresent || sessionFile[0] == '\0') return;
  if (!SD.exists(sessionFile)) {
    File f = SD.open(sessionFile, FILE_WRITE);
    if (f) { f.print("Strecke,Fahrer,Zeit_ms,Zeit,Datum,Bedingung\r\n"); f.close(); }
  }
}

// ── SD-Init ────────────────────────────────────────────────
void sdInit() {
  SDSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdPresent = SD.begin(SD_CS, SDSPI);
  if (!sdPresent) { DBG("SD", "Keine Karte"); return; }
  DBGF("SD", "Karte OK – %lu MB", (unsigned long)(SD.cardSize() / (1024UL * 1024UL)));

  // Verzeichnisse anlegen
  if (!SD.exists("/logs")) SD.mkdir("/logs");

  // Globales Log anlegen falls nicht vorhanden
  if (!SD.exists("/log.csv")) {
    File f = SD.open("/log.csv", FILE_WRITE);
    if (f) { f.print("Strecke,Fahrer,Zeit_ms,Zeit,Datum,Bedingung\r\n"); f.close(); }
  }

  // Tracks + Rider laden
  sdLoadTracks();
  sdLoadRiders();

  // Session-Datei bestimmen
  updateSessionFile();
  sdInitSessionFile();
}

// ── Eintrag hinzufügen ─────────────────────────────────────
static const char* conditionStr(uint8_t c) {
  switch (c) { case 1: return "Trocken"; case 2: return "Nass";
               case 3: return "Laubig"; case 4: return "Eis"; default: return ""; }
}

void sdAppendEntry(unsigned long ms, const char* name, int64_t tsUnix) {
  if (!sdPresent) return;
  // Session-Datei aktualisieren falls nötig
  updateSessionFile();
  sdInitSessionFile();

  char tbuf[12]; fmtTime(ms, tbuf);
  char dateBuf[20]; dateTimeStr(tsUnix, dateBuf, sizeof(dateBuf));
  const char* track = (activeTrackName[0] ? activeTrackName : "");
  const char* rider = (activeRiderName[0] ? (name && name[0] ? name : activeRiderName) : (name ? name : ""));
  const char* cond  = conditionStr(cfg_condition);

  // In Session-Datei schreiben
  if (sessionFile[0]) {
    File f = SD.open(sessionFile, FILE_APPEND);
    if (f) {
      f.print(track); f.print(","); f.print(rider); f.print(",");
      f.print(ms);    f.print(","); f.print(tbuf);  f.print(",");
      f.print(dateBuf); f.print(","); f.print(cond); f.print("\r\n");
      f.close();
    }
  }

  // In globales Log schreiben
  File g = SD.open("/log.csv", FILE_APPEND);
  if (g) {
    g.print(track); g.print(","); g.print(rider); g.print(",");
    g.print(ms);    g.print(","); g.print(tbuf);  g.print(",");
    g.print(dateBuf); g.print(","); g.print(cond); g.print("\r\n");
    g.close();
  }
}

// ── Fahrernamen im letzten passenden SD-Eintrag korrigieren ─
bool sdEditLastEntry(int64_t targetTs, const char* newName) {
  if (!sdPresent || sessionFile[0] == '\0') return false;
  // Session-Datei lesen
  File f = SD.open(sessionFile, FILE_READ);
  if (!f) return false;
  String content = "";
  while (f.available()) {
    content += (char)f.read();
    if (content.length() > 32000) break;
  }
  f.close();

  // Datum-String des Timestamps bilden
  char dateBuf[20]; dateTimeStr(targetTs, dateBuf, sizeof(dateBuf));
  // Zeile mit diesem Datum suchen und Fahrernamen (2. Feld) ersetzen
  String result = "";
  int start = 0, nl;
  bool found = false;
  while ((nl = content.indexOf('\n', start)) >= 0) {
    String line = content.substring(start, nl + 1);
    if (!found && line.indexOf(dateBuf) >= 0) {
      // Felder aufteilen: Strecke,Fahrer,Zeit_ms,Zeit,Datum,Bedingung
      int c1 = line.indexOf(',');
      int c2 = (c1 >= 0) ? line.indexOf(',', c1 + 1) : -1;
      if (c1 >= 0 && c2 >= 0) {
        result += line.substring(0, c1 + 1);
        result += String(newName);
        result += line.substring(c2);
        found = true;
        start = nl + 1; continue;
      }
    }
    result += line;
    start = nl + 1;
  }
  if (!found) return false;

  // Datei neu schreiben
  SD.remove(sessionFile);
  File w = SD.open(sessionFile, FILE_WRITE);
  if (!w) return false;
  w.print(result);
  w.close();
  return true;
}

// ── Session-Liste ──────────────────────────────────────────
String sdListSessions() {
  if (!sdPresent) return "[]";
  File dir = SD.open("/logs");
  if (!dir) return "[]";
  String j = "[";
  bool first = true;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String fname = String(entry.name());
      // entry.name() liefert vollen Pfad auf manchen Versionen, ggf. kürzen
      if (fname.startsWith("/logs/")) fname = fname.substring(6);
      if (fname.endsWith(".csv")) {
        if (!first) j += ",";
        j += "\""; j += fname; j += "\"";
        first = false;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  j += "]";
  return j;
}

// ── Session paginiert lesen (50 Zeilen pro Seite) ──────────
String sdReadSession(const char* filename, uint8_t page) {
  if (!sdPresent) return "[]";
  char path[40]; snprintf(path, sizeof(path), "/logs/%s", filename);
  File f = SD.open(path, FILE_READ);
  if (!f) return "[]";

  const uint8_t PAGE_SIZE = 50;
  // Zeilen die übersprungen werden: 1 Header + page*PAGE_SIZE Datenzeilen
  uint16_t toSkip = 1 + (uint16_t)page * PAGE_SIZE;
  uint16_t count = 0;
  String j = "[";
  bool first = true;
  String line = "";

  while (f.available()) {
    char c = (char)f.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 0) {
        if (toSkip > 0) { toSkip--; line = ""; continue; }  // überspringen
        if (count < PAGE_SIZE) {
          // CSV-Zeile als JSON
          int c1 = line.indexOf(',');
          int c2 = (c1>=0) ? line.indexOf(',',c1+1) : -1;
          int c3 = (c2>=0) ? line.indexOf(',',c2+1) : -1;
          int c4 = (c3>=0) ? line.indexOf(',',c3+1) : -1;
          int c5 = (c4>=0) ? line.indexOf(',',c4+1) : -1;
          if (c1>=0 && c2>=0 && c3>=0 && c4>=0) {
            if (!first) j += ",";
            j += "{\"track\":\""; j += line.substring(0,c1);
            j += "\",\"rider\":\""; j += line.substring(c1+1,c2);
            j += "\",\"ms\":"; j += line.substring(c2+1,c3);
            j += ",\"time\":\""; j += line.substring(c3+1,c4);
            j += "\",\"date\":\""; j += line.substring(c4+1, c5>=0?c5:line.length());
            if (c5>=0) { j += "\",\"cond\":\""; j += line.substring(c5+1); }
            j += "\"}";
            first = false; count++;
          }
        }
        if (count >= PAGE_SIZE) break;  // früh abbrechen
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
  f.close();
  j += "]";
  return j;
}

// ── Statistiken für eine Strecke ───────────────────────────
String sdGetStats(uint8_t trackId) {
  if (!sdPresent) return "{\"total\":0}";

  // Track-Name ermitteln
  char tname[21] = "";
  Track* tr = sdFindTrack(trackId);
  if (tr) strncpy(tname, tr->name, sizeof(tname));

  File f = SD.open("/log.csv", FILE_READ);
  if (!f) return "{\"total\":0}";

  uint32_t total = 0;
  uint64_t sumMs = 0;
  unsigned long best = 0, worst = 0;
  unsigned long last5[5] = {0, 0, 0, 0, 0};
  uint8_t  last5n = 0;
  bool skipHeader = true;
  String line = "";

  while (f.available()) {
    char c = (char)f.read();
    if (c == '\n') {
      line.trim();
      if (skipHeader) { skipHeader = false; line = ""; continue; }
      int c1 = line.indexOf(',');
      bool trackMatch = (trackId == 0) ||
                        (c1 >= 0 && (int)strlen(tname) == c1 && line.startsWith(tname));
      if (line.length() > 0 && trackMatch) {
        int c2 = (c1>=0)?line.indexOf(',',c1+1):-1;
        int c3 = (c2>=0)?line.indexOf(',',c2+1):-1;
        if (c1>=0 && c2>=0 && c3>=0) {
          unsigned long ms = strtoul(line.c_str()+c2+1, nullptr, 10);
          if (ms > 0) {
            total++; sumMs += ms;
            if (best == 0 || ms < best) best = ms;
            if (worst == 0 || ms > worst) worst = ms;
            // Letzte 5 für Trend
            last5[last5n % 5] = ms; last5n++;
          }
        }
      }
      line = "";
    } else if (c != '\r') { line += c; }
  }
  f.close();

  unsigned long avg = (total > 0) ? (unsigned long)(sumMs / total) : 0;

  // Trend: Ø der letzten 3 vs. Ø der 3 davor
  int8_t trend = 0;
  if (last5n >= 4) {
    uint8_t n = min((uint8_t)5, last5n);
    unsigned long rAvg = 0, oAvg = 0; uint8_t rc = 0, oc = 0;
    for (uint8_t i = 0; i < n; i++) {
      uint8_t idx = (last5n - 1 - i) % 5;
      if (i < 2) { rAvg += last5[idx]; rc++; }
      else        { oAvg += last5[idx]; oc++; }
    }
    if (rc > 0 && oc > 0) {
      rAvg /= rc; oAvg /= oc;
      trend = (rAvg < oAvg) ? 1 : (rAvg > oAvg) ? -1 : 0;
    }
  }

  char tbest[12], tavg[12], tworst[12];
  fmtTime(best, tbest); fmtTime(avg, tavg); fmtTime(worst, tworst);

  String j = "{";
  j += "\"total\":"; j += total;
  j += ",\"best\":"; j += best;
  j += ",\"bestFmt\":\""; j += tbest; j += "\"";
  j += ",\"avg\":"; j += avg;
  j += ",\"avgFmt\":\""; j += tavg; j += "\"";
  j += ",\"worst\":"; j += worst;
  j += ",\"trend\":"; j += trend;
  j += "}";
  return j;
}

// ── Bestenliste für eine Strecke (Top 10) ──────────────────
String sdGetBestlist(uint8_t trackId) {
  if (!sdPresent) return "[]";
  char tname[21] = "";
  if (trackId > 0) {
    Track* tr = sdFindTrack(trackId);
    if (tr) strncpy(tname, tr->name, sizeof(tname));
    else    return "[]";
  }

  // Alle passenden Einträge sammeln (max 200, dann sortieren)
  struct BestEntry { char rider[21]; unsigned long ms; char date[20]; };
  const uint8_t MAX_BEST = 50;
  BestEntry entries[MAX_BEST]; uint8_t ec = 0;

  File f = SD.open("/log.csv", FILE_READ);
  if (!f) return "[]";
  bool skipHeader = true;
  String line = "";
  while (f.available() && ec < MAX_BEST) {
    char c = (char)f.read();
    if (c == '\n') {
      line.trim();
      if (skipHeader) { skipHeader = false; line = ""; continue; }
      int c1b=line.indexOf(',');
      bool tmatch = (trackId==0)||(c1b>=0&&(int)strlen(tname)==c1b&&line.startsWith(tname));
      if (line.length() > 0 && tmatch) {
        int c1=c1b, c2=(c1>=0)?line.indexOf(',',c1+1):-1;
        int c3=(c2>=0)?line.indexOf(',',c2+1):-1, c4=(c3>=0)?line.indexOf(',',c3+1):-1;
        if (c1>=0&&c2>=0&&c3>=0&&c4>=0) {
          unsigned long ms = strtoul(line.c_str()+c2+1, nullptr, 10);
          if (ms > 0) {
            strncpy(entries[ec].rider, line.c_str()+c1+1, 20); entries[ec].rider[20]='\0';
            // Rider-Name bis zum nächsten Komma kürzen
            char* cm = strchr(entries[ec].rider, ','); if (cm) *cm = '\0';
            entries[ec].ms = ms;
            strncpy(entries[ec].date, line.c_str()+c3+1, 19); entries[ec].date[19]='\0';
            char* cm2 = strchr(entries[ec].date, ','); if (cm2) *cm2 = '\0';
            ec++;
          }
        }
      }
      line = "";
    } else if (c != '\r') { line += c; }
  }
  f.close();

  // Insertionsortierung (aufsteigend nach ms)
  for (uint8_t i = 1; i < ec; i++) {
    BestEntry key = entries[i]; int8_t j = i - 1;
    while (j >= 0 && entries[j].ms > key.ms) { entries[j+1] = entries[j]; j--; }
    entries[j+1] = key;
  }

  // Top 10 als JSON
  uint8_t top = min((uint8_t)10, ec);
  String j = "[";
  for (uint8_t i = 0; i < top; i++) {
    if (i) j += ",";
    char tbuf[12]; fmtTime(entries[i].ms, tbuf);
    j += "{\"rank\":"; j += (i + 1);
    j += ",\"rider\":\""; j += entries[i].rider; j += "\"";
    j += ",\"ms\":"; j += entries[i].ms;
    j += ",\"time\":\""; j += tbuf; j += "\"";
    j += ",\"date\":\""; j += entries[i].date; j += "\"";
    j += "}";
  }
  j += "]";
  return j;
}

// ── Export: aktuelle Session als CSV ───────────────────────
String sdReadCsv() {
  if (!sdPresent) return "";
  // Bevorzugt aktuelle Session, Fallback: log.csv
  const char* path = (sessionFile[0]) ? sessionFile : "/log.csv";
  File f = SD.open(path, FILE_READ);
  if (!f) return "";
  String content = "";
  while (f.available()) {
    content += (char)f.read();
    if (content.length() > 32000) break;
  }
  f.close();
  return content;
}

// ── Session löschen (log.csv bleibt!) ──────────────────────
void sdClear() {
  if (!sdPresent) return;
  if (sessionFile[0]) {
    SD.remove(sessionFile);
    cfg_session_num++;
    prefs.begin("mtb-cfg-l", false);
    prefs.putUChar("sessionnum", cfg_session_num);
    prefs.end();
    updateSessionFile();
    sdInitSessionFile();
    DBG("SD", "Session gelöscht, neue Session gestartet");
  }
}

// ── Globales Log löschen (alle Daten!) ─────────────────────
void sdClearAll() {
  if (!sdPresent) return;
  SD.remove("/log.csv");
  File f = SD.open("/log.csv", FILE_WRITE);
  if (f) { f.print("Strecke,Fahrer,Zeit_ms,Zeit,Datum,Bedingung\r\n"); f.close(); }
  sdClear();
  DBG("SD", "Globales Log gelöscht");
}

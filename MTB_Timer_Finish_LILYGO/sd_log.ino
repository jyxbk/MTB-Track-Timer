// ── SD-Karten-Log (Finish) ─────────────────────────────────
void sdInit() {
  SDSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdPresent = SD.begin(SD_CS, SDSPI);
  if (!sdPresent) { Serial.println("[SD] Keine Karte."); return; }
  Serial.print("[SD] Karte OK – "); Serial.print(SD.cardSize()/(1024*1024)); Serial.println(" MB");
  if (!SD.exists(SD_LOGFILE)) {
    File f = SD.open(SD_LOGFILE, FILE_WRITE);
    if (f) { f.print("Zeit_ms,Name,Zeit,Datum\r\n"); f.close(); }
  }
}

void sdAppendEntry(unsigned long ms, const char* name, int64_t tsUnix) {
  if (!sdPresent) return;
  File f = SD.open(SD_LOGFILE, FILE_APPEND);
  if (!f) return;
  char tbuf[12]; fmtTime(ms, tbuf);
  char dateBuf[20] = "";
  if (tsUnix > 0) {
    time_t t2 = (time_t)(tsUnix/1000LL); struct tm* tm2 = gmtime(&t2);
    snprintf(dateBuf,sizeof(dateBuf),"%02d.%02d.%04d %02d:%02d",
             tm2->tm_mday,tm2->tm_mon+1,tm2->tm_year+1900,tm2->tm_hour,tm2->tm_min);
  }
  f.print(String(ms)); f.print(","); f.print(name?name:"");
  f.print(","); f.print(tbuf); f.print(","); f.print(dateBuf); f.print("\r\n");
  f.close();
}

String sdReadCsv() {
  if (!sdPresent) return "";
  File f = SD.open(SD_LOGFILE, FILE_READ);
  if (!f) return "";
  String content = "";
  while (f.available()) { content += (char)f.read(); if (content.length()>32000) break; }
  f.close(); return content;
}

void sdClear() {
  if (!sdPresent) return;
  SD.remove(SD_LOGFILE);
  File f = SD.open(SD_LOGFILE, FILE_WRITE);
  if (f) { f.print("Zeit_ms,Name,Zeit,Datum\r\n"); f.close(); }
}

// ── Display: Seiten ────────────────────────────────────────
void drawPageHeader(const char* title) {
  static const char* pInd[NUM_PAGES] = {"P1","P2","P3","P4"};
  const char* pi = pInd[currentPage < NUM_PAGES ? currentPage : 0];
  char right[12];
  if (batVoltage > 2.0f)
    snprintf(right, sizeof(right), "%s %u%%", pi, batPercent);
  else
    strncpy(right, pi, sizeof(right));

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, title);
  int rx = 128 - (int)(strlen(right) * 6);
  if (rx < 0) rx = 0;
  if (batVoltage > 2.0f && batPercent <= 10) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(rx - 1, 0, 128 - rx + 1, 12);
    u8g2.setDrawColor(0);
    u8g2.drawStr(rx, 10, right);
    u8g2.setDrawColor(1);
  } else {
    u8g2.drawStr(rx, 10, right);
  }
  u8g2.drawHLine(0, 12, 128);
}

void drawMainPage(unsigned long liveMs) {
  char lastBuf[12], bestBuf[12];
  if (lastTimeMs > 0) fmtTime(lastTimeMs, lastBuf); else strcpy(lastBuf, "--:--.---");
  if (bestTimeMs > 0) fmtTime(bestTimeMs, bestBuf); else strcpy(bestBuf, "--:--.---");

  drawPageHeader("MTB [ZIEL]");

  u8g2.setFont(u8g2_font_7x13B_tf);
  char midBuf[14];
  if      (appState == ARMED && liveMs > 0)    fmtTime(liveMs, midBuf);
  else if (appState == DONE  && lastTimeMs > 0) fmtTime(lastTimeMs, midBuf);
  else if (appState == ARMED)                   strcpy(midBuf, " LAEUFT!");
  else {
    unsigned long since = (loraLastContact > 0) ? (millis() - loraLastContact) / 1000 : 9999;
    if      (loraLastContact == 0) strcpy(midBuf, "KEIN SIG.");
    else if (since > 90)           strcpy(midBuf, "VERBIND.?");
    else                           strcpy(midBuf, " WARTET. ");
  }
  u8g2.drawStr(8, 28, midBuf);
  u8g2.drawHLine(0, 31, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  if (appState == ARMED && strlen(currentRiderName) > 0) {
    char ns[17]; strncpy(ns, currentRiderName, 16); ns[16] = '\0';
    u8g2.drawStr(0, 43, "Fahrer:");
    u8g2.drawStr(0, 57, ns);
  } else {
    u8g2.drawStr(0, 43, "Last:"); u8g2.drawStr(40, 43, lastBuf);
    u8g2.drawStr(0, 57, "Best:"); u8g2.drawStr(40, 57, bestBuf);
  }
  u8g2.sendBuffer();
}

void drawSignalPage() {
  drawPageHeader("[ZIEL] SIGNAL");
  u8g2.setFont(u8g2_font_6x10_tf);
  char buf[28];
  if (loraLastContact == 0) {
    u8g2.drawStr(4, 30, "Kein Kontakt");
  } else {
    snprintf(buf, sizeof(buf), "RSSI:%4d  %s", (int)loraRssi, rssiStatus(loraRssi));
    u8g2.drawStr(0, 24, buf);
    snprintf(buf, sizeof(buf), "SNR:  %.1f dB", loraSnr);
    u8g2.drawStr(0, 35, buf);
    snprintf(buf, sizeof(buf), "TX:%lu RX:%lu", loraTxCount, loraRxCount);
    u8g2.drawStr(0, 46, buf);
  }
  char upBuf[14]; fmtUptime(upBuf);
  snprintf(buf, sizeof(buf), "Up:   %s", upBuf);
  u8g2.drawStr(0, 57, buf);
  u8g2.sendBuffer();
}

void drawHistoryPage() {
  drawPageHeader("[ZIEL] VERLAUF");
  if (historyCnt == 0) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(8, 38, "Keine Laeufe");
  } else {
    u8g2.setFont(u8g2_font_5x7_tf);
    uint8_t si[MAX_HISTORY];
    uint8_t validCnt = 0;
    for (uint8_t i = 0; i < historyCnt; i++) {
      uint8_t pi = histPhys(i); if (history[pi] > 0) si[validCnt++] = pi;
    }
    for (uint8_t i = 1; i < validCnt; i++) {
      uint8_t key = si[i]; int8_t j = i - 1;
      while (j >= 0 && history[si[j]] > history[key]) { si[j+1] = si[j]; j--; }
      si[j+1] = key;
    }
    int shown = 0;
    for (uint8_t rank = 0; rank < validCnt && shown < 4; rank++, shown++) {
      uint8_t pi = si[rank];
      char tbuf[12]; fmtTime(history[pi], tbuf);
      char line[22];
      if (strlen(historyNames[pi]) > 0) {
        char ns[8]; strncpy(ns, historyNames[pi], 7); ns[7] = '\0';
        snprintf(line, sizeof(line), "%d %-7s %s", rank + 1, ns, tbuf);
      } else {
        snprintf(line, sizeof(line), "%d ---     %s", rank + 1, tbuf);
      }
      u8g2.drawStr(0, 21 + shown * 11, line);
    }
  }
  u8g2.sendBuffer();
}

void drawSyncPage() {
  drawPageHeader("[ZIEL] UHRZEIT");
  u8g2.setFont(u8g2_font_6x10_tf);
  char buf[24];
  if (!timeIsSynced) {
    u8g2.drawStr(0, 26, "Nicht synchronisiert");
    u8g2.drawStr(0, 37, "> Browser oeffnen");
  } else {
    int64_t ts = nowUnixMs() / 1000LL + TZ_OFFSET_SEC;
    unsigned hh = (unsigned)((ts % 86400LL) / 3600LL);
    unsigned mm = (unsigned)((ts % 3600LL) / 60LL);
    unsigned ss2 = (unsigned)(ts % 60LL);
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hh, mm, ss2);
    u8g2.drawStr(0, 24, buf);
    unsigned long ago = (millis() - lastSyncAt) / 1000UL;
    if (ago < 60)   snprintf(buf, sizeof(buf), "Sync: vor %lus", ago);
    else            snprintf(buf, sizeof(buf), "Sync: vor %lum", ago / 60UL);
    u8g2.drawStr(0, 36, buf);
  }
  char upBuf2[14]; fmtUptime(upBuf2);
  snprintf(buf, sizeof(buf), "Up: %s", upBuf2);
  u8g2.drawStr(0, 50, buf);
  u8g2.sendBuffer();
}

void drawDisplay(unsigned long liveMs) {
  switch (currentPage) {
    case 0:  drawMainPage(liveMs); break;
    case 1:  drawSignalPage();     break;
    case 2:  drawHistoryPage();    break;
    case 3:  drawSyncPage();       break;
    default: drawMainPage(liveMs); break;
  }
}

void showSleepProgress(unsigned long heldMs) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(18, 20, "Ausschalten...");
  unsigned long elapsed = (heldMs > 1000) ? (heldMs - 1000) : 0;
  int w = (int)((elapsed * 116) / (LONG_PRESS_MS - 1000));
  if (w > 116) w = 116;
  u8g2.drawFrame(6, 30, 116, 14);
  if (w > 0) u8g2.drawBox(6, 30, w, 14);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(6, 56, "PRG loslassen = Abbrechen");
  u8g2.sendBuffer();
}

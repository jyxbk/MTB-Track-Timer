// ── Display: Seiten ────────────────────────────────────────
void drawPageHeader(const char* title) {
  static const char* pInd[NUM_PAGES] = {"P1","P2","P3","P4","P5"};
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
  drawPageHeader("MTB [START]");

  u8g2.setFont(u8g2_font_7x13B_tf);
  char midBuf[14];
  if      (appState == RUNNING && liveMs > 0)     fmtTime(liveMs, midBuf);
  else if (appState == RESULT  && lastTimeMs > 0) fmtTime(lastTimeMs, midBuf);
  else if (appState == RUNNING)                   strcpy(midBuf, " LAEUFT!");
  else if (appState == LAP_IDLE)                  strcpy(midBuf, "LAP-MODUS");
  else if (appState == LAP_RUNNING) { if (liveMs > 0) fmtTime(liveMs, midBuf); else strcpy(midBuf, " LAEUFT!"); }
  else if (stagMode) {
    snprintf(midBuf, sizeof(midBuf), "VERS %u/%u", stagStarted, stagCount);
  } else if (duelMode && duelCurrent < duelCount) {
    char ns[9]; strncpy(ns, duelRiders[duelCurrent], 8); ns[8] = '\0';
    snprintf(midBuf, sizeof(midBuf), "%d/%d:%s", duelCurrent + 1, duelCount, ns);
  } else {
    unsigned long since = (loraLastContact > 0) ? (millis() - loraLastContact) / 1000 : 9999;
    unsigned long peerTimeoutSec = cfg_ping_ms * 3 / 1000;
    if      (loraLastContact == 0)        strcpy(midBuf, "KEIN ZIEL");
    else if (since > peerTimeoutSec)      strcpy(midBuf, "VERBIND.?");
    else                                  strcpy(midBuf, "  BEREIT ");
  }
  u8g2.drawStr(8, 28, midBuf);
  u8g2.drawHLine(0, 31, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  if (stagMode) {
    if (stagStarted < stagCount) {
      if (stagStarted == 0) {
        char nb[22]; snprintf(nb, sizeof(nb), "1: %.16s", stagRiders[0]);
        u8g2.drawStr(0, 43, nb);
        u8g2.drawStr(0, 57, "Sensor starten");
      } else {
        unsigned long elapsed = millis() - stagLastStartMs;
        unsigned long off_ms  = (unsigned long)cfg_stag_offset_s * 1000UL;
        if (elapsed < off_ms) {
          unsigned long rem = off_ms - elapsed;
          char cd[12]; snprintf(cd, sizeof(cd), "Nxt: %02lu:%02lu", rem/60000, (rem%60000)/1000);
          u8g2.drawStr(0, 43, cd);
          char nb[22]; snprintf(nb, sizeof(nb), "%.20s", stagRiders[stagStarted]);
          u8g2.drawStr(0, 57, nb);
        } else {
          u8g2.drawStr(0, 43, "BEREIT! Sensor...");
          char nb[22]; snprintf(nb, sizeof(nb), "%.20s", stagRiders[stagStarted]);
          u8g2.drawStr(0, 57, nb);
        }
      }
    } else {
      char fb[24]; snprintf(fb, sizeof(fb), "Ziel: %u/%u", stagFinished, stagCount);
      u8g2.drawStr(0, 43, fb);
      u8g2.drawStr(0, 57, "Warte auf TIM...");
    }
    u8g2.sendBuffer(); return;
  } else if (appState == LAP_IDLE) {
    u8g2.drawStr(0, 43, "Warte auf Sensor");
    u8g2.drawStr(0, 57, "Web: Stopp/Reset");
  } else if (appState == LAP_RUNNING) {
    char rln[20];
    if (lapLastMs > 0) {
      char lb[12]; fmtTime(lapLastMs, lb);
      snprintf(rln, sizeof(rln), "R%u Last:%s", lapRoundNum, lb);
    } else {
      snprintf(rln, sizeof(rln), "Runde %u", lapRoundNum);
    }
    u8g2.drawStr(0, 43, rln);
    if (lapBestMs > 0) {
      char bb[12]; fmtTime(lapBestMs, bb);
      char bln[20]; snprintf(bln, sizeof(bln), "Bst: %s", bb);
      u8g2.drawStr(0, 57, bln);
    }
  } else {
    char lastBuf[12], bestBuf[12];
    if (lastTimeMs > 0) fmtTime(lastTimeMs, lastBuf); else strcpy(lastBuf, "--:--.---");
    if (bestTimeMs > 0) fmtTime(bestTimeMs, bestBuf); else strcpy(bestBuf, "--:--.---");
    u8g2.drawStr(0, 43, "Last:"); u8g2.drawStr(40, 43, lastBuf);
    u8g2.drawStr(0, 57, "Best:"); u8g2.drawStr(40, 57, bestBuf);
  }
  u8g2.sendBuffer();
}

void drawSignalPage() {
  drawPageHeader("[START] SIGNAL");
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
  drawPageHeader("[START] VERLAUF");
  if (historyCnt == 0) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(8, 38, "Keine Laeufe");
  } else {
    u8g2.setFont(u8g2_font_5x7_tf);
    uint8_t si[MAX_HISTORY];
    uint8_t validCnt = 0;
    for (uint8_t i = 0; i < historyCnt; i++) {
      uint8_t pi = histPhys(i);
      if (history[pi] > 0) si[validCnt++] = pi;
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

void drawDuelPage() {
  drawPageHeader("[START] DUELL");
  u8g2.setFont(u8g2_font_6x10_tf);
  if (!duelMode && !duelDone) {
    u8g2.drawStr(0, 30, "Kein Duell aktiv");
    u8g2.drawStr(0, 42, "-> Web: Duell-Tab");
  } else if (duelDone) {
    u8g2.drawStr(0, 24, "Duell beendet!");
    char buf[22]; sprintf(buf, "%d Fahrer gesamt", duelCount);
    u8g2.drawStr(0, 36, buf);
    u8g2.drawStr(0, 48, "-> Web: Rangliste");
  } else {
    char buf[22];
    sprintf(buf, "%d / %d", duelCurrent + 1, duelCount);
    u8g2.drawStr(0, 22, buf);
    char ns[17]; strncpy(ns, duelRiders[duelCurrent], 16); ns[16] = '\0';
    u8g2.setFont(u8g2_font_7x13B_tf);
    u8g2.drawStr(0, 36, ns);
    if (duelCurrent + 1 < duelCount) {
      u8g2.setFont(u8g2_font_6x10_tf);
      char nextLine[22]; char ns2[12];
      strncpy(ns2, duelRiders[duelCurrent + 1], 11); ns2[11] = '\0';
      sprintf(nextLine, "Next: %s", ns2);
      u8g2.drawStr(0, 50, nextLine);
    }
  }
  u8g2.sendBuffer();
}

void drawSyncPage() {
  drawPageHeader("[START] UHRZEIT");
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
    case 3:  drawDuelPage();       break;
    case 4:  drawSyncPage();       break;
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

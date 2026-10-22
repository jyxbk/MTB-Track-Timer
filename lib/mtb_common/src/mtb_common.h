// ============================================================
//  mtb_common – gemeinsame, hardwarefreie Logik aller Knoten
//  (Start / Finish / Split, Heltec / LILYGO / LUFT)
//
//  Header-only und ohne Arduino-Abhängigkeit, damit die Funktionen
//  auch nativ auf dem PC getestet werden können (test/).
// ============================================================
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace mtb {

// Obergrenze für Zeiten in LoRa-Nachrichten (1 h in µs)
constexpr unsigned long MAX_RUN_US = 3600000000UL;
// Kleinster plausibler Unix-Zeitstempel in ms (2001-09-09)
constexpr int64_t MIN_UNIX_MS = 1000000000000LL;
// Ein Interrupt-Zeitstempel ist nur so lange als STX-Referenz gültig
constexpr uint64_t RX_STAMP_MAX_AGE_US = 1000000ULL;

// ── Formatierung ───────────────────────────────────────────

// "MM:SS.mmm" – out muss mindestens 12 Byte groß sein
inline void fmtTime(unsigned long ms, char* out) {
  snprintf(out, 12, "%02u:%02u.%03u",
    (unsigned)(ms / 60000),
    (unsigned)((ms % 60000) / 1000),
    (unsigned)(ms % 1000));
}

// "H:MM:SS" aus Sekunden – out muss mindestens 14 Byte groß sein
inline void fmtDuration(unsigned long s, char* out) {
  unsigned long m = s / 60; s %= 60;
  unsigned long h = m / 60; m %= 60;
  snprintf(out, 14, "%lu:%02lu:%02lu", h, m, s);
}

// ── Akku ───────────────────────────────────────────────────

// Li-Ion-Entladekurve (Spannung → Prozent, linear interpoliert)
inline uint8_t voltageToPct(float v) {
  static const float vpts[] = {3.00f, 3.20f, 3.30f, 3.40f, 3.50f, 3.60f, 3.70f};
  static const float ppts[] = {  0,      2,     8,    20,    40,    60,   100};
  if (v >= 3.70f) return 100;
  if (v <  3.00f) return 0;
  for (int i = 1; i < 7; i++) {
    if (v < vpts[i]) {
      float frac = (v - vpts[i-1]) / (vpts[i] - vpts[i-1]);
      return (uint8_t)(ppts[i-1] + frac * (ppts[i] - ppts[i-1]));
    }
  }
  return 0;
}

// ── LoRa-Protokoll ─────────────────────────────────────────

// "TIM:<µs>" / "SPL:<µs>": prefix inkl. Doppelpunkt; Wert muss 1 … MAX_RUN_US sein
inline bool parseUsPayload(const char* msg, const char* prefix, unsigned long& us) {
  size_t pl = strlen(prefix);
  if (strncmp(msg, prefix, pl) != 0) return false;
  const char* p = msg + pl;
  if (*p < '0' || *p > '9') return false;
  char* ep;
  unsigned long v = strtoul(p, &ep, 10);
  if (ep == p || v == 0 || v > MAX_RUN_US) return false;
  us = v;
  return true;
}

// "TSY:<unix_ms>" mit Plausibilitätsprüfung
inline bool parseTsy(const char* msg, int64_t& unixMs) {
  if (strncmp(msg, "TSY:", 4) != 0) return false;
  int64_t v = (int64_t)atoll(msg + 4);
  if (v < MIN_UNIX_MS) return false;
  unixMs = v;
  return true;
}

// Fahrername aus "STX" / "STX:<name>" (leer, wenn keiner); out hat n Byte
inline void stxRiderName(const char* msg, char* out, size_t n) {
  if (n == 0) return;
  out[0] = '\0';
  if (strncmp(msg, "STX:", 4) == 0) {
    strncpy(out, msg + 4, n - 1);
    out[n - 1] = '\0';
  }
}

// ── Laufzeitkompensation ───────────────────────────────────

// Ziel/Split messen ab STX-Empfang. Ergebnis in ms (gerundet) =
// Rohwert + gemessene STX-Laufzeit am Start + Zusatzkorrektur
inline unsigned long compensatedMs(unsigned long rawUs, uint64_t stxLagUs, uint32_t addMs) {
  uint64_t t = (uint64_t)rawUs + stxLagUs + (uint64_t)addMs * 1000ULL;
  return (unsigned long)((t + 500ULL) / 1000ULL);
}

// Versetzter Start: Ziel misst ab dem zuletzt gesendeten STX (armRefMs);
// auf den Start von Fahrer k (riderStartMs) umrechnen
inline unsigned long stagResultMs(unsigned long rawUs, unsigned long armRefMs,
                                  unsigned long riderStartMs, uint32_t addMs) {
  unsigned long t = (rawUs + 500UL) / 1000UL + addMs;
  long corr = (long)(armRefMs - riderStartMs);
  if (corr > 0 && (unsigned long)corr < 3600000UL) t += (unsigned long)corr;
  return t;
}

// Referenzzeit für STX am Ziel/Split: Interrupt-Zeitstempel, falls plausibel, sonst jetzt
inline uint64_t rxReferenceUs(uint64_t rxUs, uint64_t nowUs) {
  return (rxUs > 0 && rxUs <= nowUs && nowUs - rxUs < RX_STAMP_MAX_AGE_US) ? rxUs : nowUs;
}

// ── Verlauf (Ringpuffer) ───────────────────────────────────

// Physischer Index für logischen Eintrag i (0 = ältester)
inline uint8_t histPhys(uint8_t head, uint8_t cnt, uint8_t i, uint8_t maxN) {
  return (uint8_t)((head + maxN - cnt + i) % maxN);
}

}  // namespace mtb

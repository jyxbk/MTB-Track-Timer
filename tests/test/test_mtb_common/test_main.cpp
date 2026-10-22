// Unit-Tests für die geteilte Logik (lib/mtb_common)
#include <unity.h>
#include <mtb_common.h>

void setUp() {}
void tearDown() {}

// ── Formatierung ───────────────────────────────────────────
static void test_fmtTime() {
  char b[12];
  mtb::fmtTime(0, b);            TEST_ASSERT_EQUAL_STRING("00:00.000", b);
  mtb::fmtTime(83456, b);        TEST_ASSERT_EQUAL_STRING("01:23.456", b);
  mtb::fmtTime(3599999, b);      TEST_ASSERT_EQUAL_STRING("59:59.999", b);
  mtb::fmtTime(7200000, b);      TEST_ASSERT_EQUAL_STRING("120:00.000", b);
}

static void test_fmtTime_never_overflows() {
  char b[16]; memset(b, 'X', sizeof(b));
  mtb::fmtTime(4294967295UL, b);           // größter 32-bit-Wert
  TEST_ASSERT_EQUAL_CHAR('\0', b[11]);     // auf 12 Byte begrenzt
  TEST_ASSERT_EQUAL_CHAR('X', b[12]);      // dahinter nichts geschrieben
}

static void test_fmtDuration() {
  char b[14];
  mtb::fmtDuration(0, b);        TEST_ASSERT_EQUAL_STRING("0:00:00", b);
  mtb::fmtDuration(3725, b);     TEST_ASSERT_EQUAL_STRING("1:02:05", b);
  mtb::fmtDuration(4294967UL, b);TEST_ASSERT_EQUAL_STRING("1193:02:47", b);  // millis()-Überlauf
}

// ── Akku ───────────────────────────────────────────────────
static void test_voltageToPct() {
  TEST_ASSERT_EQUAL_UINT8(0,   mtb::voltageToPct(2.80f));
  TEST_ASSERT_EQUAL_UINT8(0,   mtb::voltageToPct(3.00f));
  TEST_ASSERT_EQUAL_UINT8(20,  mtb::voltageToPct(3.40f));
  TEST_ASSERT_EQUAL_UINT8(30,  mtb::voltageToPct(3.45f));
  TEST_ASSERT_EQUAL_UINT8(100, mtb::voltageToPct(3.70f));
  TEST_ASSERT_EQUAL_UINT8(100, mtb::voltageToPct(4.20f));
}

static void test_voltageToPct_monotonic() {
  uint8_t last = 0;
  for (int mv = 2900; mv <= 4300; mv += 5) {
    uint8_t p = mtb::voltageToPct(mv / 1000.0f);
    TEST_ASSERT_TRUE(p >= last);
    last = p;
  }
}

// ── LoRa-Protokoll ─────────────────────────────────────────
static void test_parseUsPayload_valid() {
  unsigned long us = 0;
  TEST_ASSERT_TRUE(mtb::parseUsPayload("TIM:83456789", "TIM:", us));
  TEST_ASSERT_EQUAL_UINT32(83456789UL, us);
  TEST_ASSERT_TRUE(mtb::parseUsPayload("SPL:1", "SPL:", us));
  TEST_ASSERT_EQUAL_UINT32(1UL, us);
  TEST_ASSERT_TRUE(mtb::parseUsPayload("TIM:3600000000", "TIM:", us));  // genau 1 h
}

static void test_parseUsPayload_invalid() {
  unsigned long us = 42;
  TEST_ASSERT_FALSE(mtb::parseUsPayload("TIM:", "TIM:", us));
  TEST_ASSERT_FALSE(mtb::parseUsPayload("TIM:0", "TIM:", us));
  TEST_ASSERT_FALSE(mtb::parseUsPayload("TIM:abc", "TIM:", us));
  TEST_ASSERT_FALSE(mtb::parseUsPayload("TIM:-5", "TIM:", us));
  TEST_ASSERT_FALSE(mtb::parseUsPayload("TIM: 5", "TIM:", us));
  TEST_ASSERT_FALSE(mtb::parseUsPayload("TIM:3600000001", "TIM:", us));  // > 1 h
  TEST_ASSERT_FALSE(mtb::parseUsPayload("SPL:1000", "TIM:", us));        // falsches Präfix
  TEST_ASSERT_FALSE(mtb::parseUsPayload("TI", "TIM:", us));
  TEST_ASSERT_EQUAL_UINT32(42UL, us);                                    // bei Fehler unverändert
}

static void test_parseTsy() {
  int64_t ts = 0;
  TEST_ASSERT_TRUE(mtb::parseTsy("TSY:1792700000000", ts));
  TEST_ASSERT_EQUAL_INT64(1792700000000LL, ts);
  TEST_ASSERT_FALSE(mtb::parseTsy("TSY:12345", ts));         // kein Unix-ms-Wert
  TEST_ASSERT_FALSE(mtb::parseTsy("TSY:", ts));
  TEST_ASSERT_FALSE(mtb::parseTsy("TSX:1792700000000", ts));
}

static void test_stxRiderName() {
  char n[21];
  mtb::stxRiderName("STX", n, sizeof(n));          TEST_ASSERT_EQUAL_STRING("", n);
  mtb::stxRiderName("STX:", n, sizeof(n));         TEST_ASSERT_EQUAL_STRING("", n);
  mtb::stxRiderName("STX:Anna", n, sizeof(n));     TEST_ASSERT_EQUAL_STRING("Anna", n);
  mtb::stxRiderName("STX:a:b", n, sizeof(n));      TEST_ASSERT_EQUAL_STRING("a:b", n);
  mtb::stxRiderName("STXAnna", n, sizeof(n));      TEST_ASSERT_EQUAL_STRING("", n);
  mtb::stxRiderName("STX:123456789012345678901234", n, sizeof(n));
  TEST_ASSERT_EQUAL_STRING("12345678901234567890", n);   // auf 20 Zeichen gekürzt
}

// ── Laufzeitkompensation ───────────────────────────────────
static void test_compensatedMs() {
  // Ziel misst 60,000 s ab STX-Empfang; STX brauchte 45,3 ms → echte Zeit 60,045 s
  TEST_ASSERT_EQUAL_UINT32(60045UL, mtb::compensatedMs(60000000UL, 45300ULL, 0));
  // Rundung auf ms
  TEST_ASSERT_EQUAL_UINT32(1001UL, mtb::compensatedMs(1000500UL, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(1000UL, mtb::compensatedMs(1000499UL, 0, 0));
  // Zusatzkorrektur wird addiert (nicht abgezogen)
  TEST_ASSERT_EQUAL_UINT32(60055UL, mtb::compensatedMs(60000000UL, 45300ULL, 10));
}

static void test_compensatedMs_no_32bit_overflow() {
  // 1 h Rohwert + großer Lag + max. Zusatzkorrektur passt ohne Überlauf
  TEST_ASSERT_EQUAL_UINT32(3601500UL, mtb::compensatedMs(3600000000UL, 1000000ULL, 500));
}

static void test_stagResultMs() {
  // Fahrer 0 startet bei 10 000 ms, Fahrer 1 bei 40 000 ms (STX-Ende 40 050 ms).
  // Fahrer 0 kommt an, das Ziel misst ab dem STX für Fahrer 1: 50,000 s
  // → Fahrer 0: 50 000 + (40 050 − 10 000) = 80 050 ms
  TEST_ASSERT_EQUAL_UINT32(80050UL, mtb::stagResultMs(50000000UL, 40050UL, 10000UL, 0));
  // Nach Re-Arm (STX-Ende 90 300 ms) misst das Ziel für Fahrer 1 erneut ab 0:
  // 20,000 s → 20 000 + (90 300 − 40 000) = 70 300 ms
  TEST_ASSERT_EQUAL_UINT32(70300UL, mtb::stagResultMs(20000000UL, 90300UL, 40000UL, 0));
  // Zusatzkorrektur
  TEST_ASSERT_EQUAL_UINT32(70305UL, mtb::stagResultMs(20000000UL, 90300UL, 40000UL, 5));
  // Referenz vor Fahrerstart (unplausibel) → keine Korrektur
  TEST_ASSERT_EQUAL_UINT32(20000UL, mtb::stagResultMs(20000000UL, 30000UL, 40000UL, 0));
}

static void test_rxReferenceUs() {
  TEST_ASSERT_EQUAL_UINT64(999000ULL, mtb::rxReferenceUs(999000ULL, 1000000ULL));   // 1 ms alt → ISR-Zeit
  TEST_ASSERT_EQUAL_UINT64(5000000ULL, mtb::rxReferenceUs(1000000ULL, 5000000ULL)); // zu alt → jetzt
  TEST_ASSERT_EQUAL_UINT64(1000000ULL, mtb::rxReferenceUs(0, 1000000ULL));          // nie gesetzt → jetzt
  TEST_ASSERT_EQUAL_UINT64(1000000ULL, mtb::rxReferenceUs(2000000ULL, 1000000ULL)); // in der Zukunft → jetzt
}

// ── Verlauf ────────────────────────────────────────────────
static void test_histPhys() {
  // 3 Einträge, nicht voll: head=3 → logisch 0..2 = physisch 0..2
  TEST_ASSERT_EQUAL_UINT8(0, mtb::histPhys(3, 3, 0, 20));
  TEST_ASSERT_EQUAL_UINT8(2, mtb::histPhys(3, 3, 2, 20));
  // voll, head=5 → ältester bei 5, neuester bei 4
  TEST_ASSERT_EQUAL_UINT8(5,  mtb::histPhys(5, 20, 0, 20));
  TEST_ASSERT_EQUAL_UINT8(4,  mtb::histPhys(5, 20, 19, 20));
  TEST_ASSERT_EQUAL_UINT8(0,  mtb::histPhys(0, 20, 0, 20));
  TEST_ASSERT_EQUAL_UINT8(19, mtb::histPhys(0, 20, 19, 20));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fmtTime);
  RUN_TEST(test_fmtTime_never_overflows);
  RUN_TEST(test_fmtDuration);
  RUN_TEST(test_voltageToPct);
  RUN_TEST(test_voltageToPct_monotonic);
  RUN_TEST(test_parseUsPayload_valid);
  RUN_TEST(test_parseUsPayload_invalid);
  RUN_TEST(test_parseTsy);
  RUN_TEST(test_stxRiderName);
  RUN_TEST(test_compensatedMs);
  RUN_TEST(test_compensatedMs_no_32bit_overflow);
  RUN_TEST(test_stagResultMs);
  RUN_TEST(test_rxReferenceUs);
  RUN_TEST(test_histPhys);
  return UNITY_END();
}

#include "led_buzzer.h"
#include "config.h"

/**
 * === LED & BUZZER INDICATOR SYSTEM ===
 *
 * BUZZER:
 *   - Normal: Beep pattern (tit-tit) yang berirama dengan detak jantung
 *   - Alarm: Suara panjang "tiiiit" 5-10 detik ketika jari dilepas
 *
 * LED (1-5): Gelombang seperti monitor jantung rumah sakit
 *   - Pattern: 1 -> 2 -> 3 -> 4 -> 5 -> 4 -> 3 -> 2 -> 1 (repeat)
 *   - Menyala sesuai irama detak jantung
 *   - Semua LED mati jika jari tidak terdeteksi
 */

// State tracking
static unsigned long lastBeatTime = 0;
static unsigned long buzzerOnTime = 0;   // Waktu buzzer menyala
static int buzzerBeepCount = 0;          // Counter untuk beep pattern
static bool buzzerActive = false;        // Status buzzer sedang ON/OFF
static bool previousFingerOn = false;    // Track jari pada frame sebelumnya
static unsigned long alarmStartTime = 0; // Waktu alarm dimulai
static bool alarmActive = false;         // Apakah alarm sedang aktif
static float previousBpm = 0;            // BPM pada frame sebelumnya
static bool pulseTriggered = false;      // Flag pulse trigger dari MQTT

// Konstanta untuk LED wave effect
#define LED_ON_DURATION_MS 120 // Durasi setiap LED menyala (ms)
#define LED_SPACING_RATIO 0.2  // Spacing antar LED = 20% dari beat interval

// Konstanta
#define ALARM_DURATION_MS 7000 // Durasi alarm 7 detik

// ==========================================
// Inisialisasi LED dan BUZZER
// ==========================================
void setup_led_buzzer()
{
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  pinMode(LED_5, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Semua LED dan buzzer OFF pada awalnya
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, LOW);
  digitalWrite(LED_3, LOW);
  digitalWrite(LED_4, LOW);
  digitalWrite(LED_5, LOW);
  digitalWrite(BUZZER, LOW);

  Serial.println("[LED_BUZZER] Setup selesai");
}

// ==========================================
// Update LED dan BUZZER sesuai BPM
// ==========================================
void update_led_buzzer(float bpm, bool fingerOn)
{
  unsigned long now = millis();

  // ===== DETEKSI JARI DILEPAS (Trigger Alarm) =====
  if (previousFingerOn && !fingerOn)
  {
    // Transisi dari jari ON ke jari OFF -> Trigger ALARM!
    alarmActive = true;
    alarmStartTime = now;
    Serial.println("[LED_BUZZER] JARI DILEPAS! Alarm ON untuk 7 detik...");
  }

  previousFingerOn = fingerOn;

  // ===== HANDLE ALARM (Buzzer panjang 5-10 detik) =====
  if (alarmActive)
  {
    unsigned long alarmElapsed = now - alarmStartTime;

    if (alarmElapsed < ALARM_DURATION_MS)
    {
      // Alarm masih aktif -> Buzzer terus ON
      digitalWrite(BUZZER, HIGH);
    }
    else
    {
      // Alarm selesai
      digitalWrite(BUZZER, LOW);
      alarmActive = false;
      Serial.println("[LED_BUZZER] Alarm OFF");
    }

    // Matikan semua LED saat alarm
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
    digitalWrite(LED_5, LOW);
    buzzerActive = false;
    return;
  }

  // ===== NORMAL MODE (Jari terdeteksi) =====
  if (!fingerOn)
  {
    // Jari tidak terdeteksi
    digitalWrite(LED_1, LOW);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
    digitalWrite(LED_5, LOW);
    digitalWrite(BUZZER, LOW);
    buzzerActive = false;
    previousBpm = 0;
    return;
  }

  // Validasi BPM
  if (bpm < 20 || bpm > 200)
  {
    bpm = 0; // BPM belum stabil
  }

  // ===== DETEKSI TRANSISI BPM (0 -> nilai atau sebaliknya) =====
  bool bpmJustAvailable = (previousBpm == 0 && bpm > 0);
  previousBpm = bpm;

  // ===== CASE 1: BPM MASIH 0 (Jari baru ditekan) =====
  if (bpm == 0)
  {
    // Hanya LED 1 yang menyala
    digitalWrite(LED_1, HIGH);
    digitalWrite(LED_2, LOW);
    digitalWrite(LED_3, LOW);
    digitalWrite(LED_4, LOW);
    digitalWrite(LED_5, LOW);
    digitalWrite(BUZZER, LOW);
    buzzerActive = false;
    return;
  }

  // ===== CASE 2: BPM ADA (Gelombang LED dengan timing offset) =====

  // Hitung interval beat (ms) dari BPM
  unsigned long beatInterval = (unsigned long)(60000.0f / bpm);

  // ===== CHECK PULSE TRIGGER dari MQTT (prioritas utama) =====
  if (pulseTriggered)
  {
    pulseTriggered = false; // Reset flag
    lastBeatTime = now;

    // Trigger buzzer dengan pattern tit-tit
    buzzerActive = true;
    buzzerOnTime = now;
    buzzerBeepCount = 0;
    digitalWrite(BUZZER, HIGH);
    Serial.println("[LED_BUZZER] Pulse trigger activated!");
  }
  // Deteksi jika ada beat baru dari sensor
  else if (now - lastBeatTime >= beatInterval)
  {
    lastBeatTime = now;

    // ===== BUZZER: Mulai pattern beep (tit-tit) saat beat =====
    buzzerActive = true;
    buzzerOnTime = now;
    buzzerBeepCount = 0;
    digitalWrite(BUZZER, HIGH);
  }

  // ===== Handle buzzer timing (non-blocking) =====
  if (buzzerActive)
  {
    unsigned long elapsed = now - buzzerOnTime;

    if (buzzerBeepCount == 0 && elapsed >= 100)
    {
      digitalWrite(BUZZER, LOW);
      buzzerBeepCount = 1;
      buzzerOnTime = now;
    }
    else if (buzzerBeepCount == 1 && elapsed >= 50)
    {
      digitalWrite(BUZZER, HIGH);
      buzzerBeepCount = 2;
      buzzerOnTime = now;
    }
    else if (buzzerBeepCount == 2 && elapsed >= 100)
    {
      digitalWrite(BUZZER, LOW);
      buzzerActive = false;
    }
  }

  // ===== Update LED dengan timing offset per LED =====
  // Setiap LED punya onset time yang berbeda dalam beat cycle
  // Spacing antar LED = 20% dari beat interval
  unsigned long timeSinceLastBeat = now - lastBeatTime;
  unsigned long ledSpacing = (unsigned long)(beatInterval * LED_SPACING_RATIO);

  // Hitung apakah setiap LED harus ON atau OFF
  // LED menyala selama LED_ON_DURATION_MS
  bool led1_on = (timeSinceLastBeat >= (ledSpacing * 0)) && (timeSinceLastBeat < (ledSpacing * 0) + LED_ON_DURATION_MS);
  bool led2_on = (timeSinceLastBeat >= (ledSpacing * 1)) && (timeSinceLastBeat < (ledSpacing * 1) + LED_ON_DURATION_MS);
  bool led3_on = (timeSinceLastBeat >= (ledSpacing * 2)) && (timeSinceLastBeat < (ledSpacing * 2) + LED_ON_DURATION_MS);
  bool led4_on = (timeSinceLastBeat >= (ledSpacing * 3)) && (timeSinceLastBeat < (ledSpacing * 3) + LED_ON_DURATION_MS);
  bool led5_on = (timeSinceLastBeat >= (ledSpacing * 4)) && (timeSinceLastBeat < (ledSpacing * 4) + LED_ON_DURATION_MS);

  // Aplikasikan state ke pin
  digitalWrite(LED_1, led1_on ? HIGH : LOW);
  digitalWrite(LED_2, led2_on ? HIGH : LOW);
  digitalWrite(LED_3, led3_on ? HIGH : LOW);
  digitalWrite(LED_4, led4_on ? HIGH : LOW);
  digitalWrite(LED_5, led5_on ? HIGH : LOW);
}

// ==========================================
// Trigger manual buzzer beep
// ==========================================
void buzzer_beep(int duration_ms, int gap_ms, int count)
{
  for (int i = 0; i < count; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(duration_ms);
    digitalWrite(BUZZER, LOW);
    if (i < count - 1)
    {
      delay(gap_ms);
    }
  }
}

// ==========================================
// Pulse Trigger Handler (dari MQTT)
// ==========================================
void buzzer_pulse_trigger(float bpm)
{
  // Set flag untuk trigger pulse di dalam update_led_buzzer
  pulseTriggered = true;
  Serial.printf("[LED_BUZZER] Pulse trigger flag set (BPM: %.1f)\n", bpm);
}

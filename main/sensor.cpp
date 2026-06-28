#include "sensor.h"
#include "config.h"
#include <Wire.h>
#include "MAX30105.h"

static MAX30105 particleSensor;

// State data sensor (yang dilaporkan ke main.ino)
static OximeterData currentData = {0, 0, false, false};

// ===========================================================
//  BAGIAN BPM — Logika yang sudah diuji oleh user
// ===========================================================
static unsigned long lastBeatTime = 0;
static float bpmAvg = 0;
static bool  bpmReady = false;

#define RATE_SIZE 5
static float rates[RATE_SIZE];
static byte  rateSpot = 0;

// -- Smoothing buffer IR --
static long irSmoothBuf[5];
static int  irSmoothIdx = 0;

static long smoothIR(long value) {
    irSmoothBuf[irSmoothIdx++] = value;
    irSmoothIdx %= 5;
    long sum = 0;
    for (int i = 0; i < 5; i++) sum += irSmoothBuf[i];
    return sum / 5;
}

// -- Peak detection dengan adaptive threshold --
static bool detectPeak(long irValue) {
    static long prev = 0;
    static bool rising = false;
    static long threshold = 0;
    static unsigned long lastBeat = 0;
    static int validPeak = 0;

    threshold = (threshold * 0.9) + (irValue * 0.1);

    if (irValue > threshold && irValue > prev && !rising) {
        rising = true;
    }

    if (rising && irValue < prev) {
        rising = false;
        long peakHeight = prev - threshold;

        if (millis() - lastBeat > 600 && peakHeight > 20) {
            validPeak++;
            if (validPeak >= 1) {
                lastBeat = millis();
                validPeak = 0;
                return true;
            }
        } else {
            validPeak = 0;
        }
    }

    prev = irValue;
    return false;
}

// ===========================================================
//  BAGIAN SPO2 — Metode AC/DC Ratio (standar untuk MAX30102)
// ===========================================================
// Prinsip:
//   R = (Red_AC / Red_DC) / (IR_AC / IR_DC)
//   SpO2 ≈ 104 - 17 * R  (formula empiris kalibrasi umum)
//
// DC = komponen baseline (rata-rata bergerak eksponensial lambat)
// AC = komponen pulsatile (deviasi dari baseline) per satu siklus detak

static float irDC  = 0;   // DC baseline IR
static float redDC = 0;   // DC baseline Red

// Akumulasi nilai |AC| antara dua detak
static float irACSum  = 0;
static float redACSum = 0;
static int   acSamples = 0;

// Buffer rata-rata SpO2 (3 sampel agar stabil)
#define SPO2_BUF_SIZE 3
static float spo2Buffer[SPO2_BUF_SIZE];
static byte  spo2Spot = 0;
static bool  spo2Ready = false;
static float spo2Avg = 0;

// Dipanggil setiap kali satu detak jantung terdeteksi
static void calcSpO2OnBeat() {
    if (acSamples < 5) {
        // Terlalu sedikit sampel dalam satu siklus, lewati
        irACSum = redACSum = 0;
        acSamples = 0;
        return;
    }

    float irACAvg  = irACSum  / acSamples;
    float redACAvg = redACSum / acSamples;

    if (irDC > 0 && redDC > 0 && irACAvg > 0 && redACAvg > 0) {
        float R = (redACAvg / redDC) / (irACAvg / irDC);

        // Formula empiris — cukup akurat untuk MAX30102 dengan LED power standar
        float spo2Calc = 104.0f - 17.0f * R;

        // Clamp ke rentang fisiologis valid
        if (spo2Calc > 100.0f) spo2Calc = 100.0f;
        if (spo2Calc < 70.0f)  spo2Calc = 70.0f;

        spo2Buffer[spo2Spot++] = spo2Calc;
        spo2Spot %= SPO2_BUF_SIZE;
        if (spo2Spot == 0) spo2Ready = true;

        if (spo2Ready) {
            float sum = 0;
            for (byte i = 0; i < SPO2_BUF_SIZE; i++) sum += spo2Buffer[i];
            spo2Avg = sum / SPO2_BUF_SIZE;
        }
    }

    // Reset akumulasi untuk siklus detak berikutnya
    irACSum = redACSum = 0;
    acSamples = 0;
}

// ===========================================================
//  RESET PENUH saat jari diangkat
// ===========================================================
static void resetAll() {
    bpmReady  = false;
    bpmAvg    = 0;
    rateSpot  = 0;
    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = 0;

    spo2Ready = false;
    spo2Avg   = 0;
    spo2Spot  = 0;
    irDC = redDC = 0;
    irACSum = redACSum = 0;
    acSamples = 0;

    for (int i = 0; i < 5; i++) irSmoothBuf[i] = 0;
    irSmoothIdx = 0;
}

// ===========================================================
//  Variabel stabilisasi awal (butuh ~25 sampel stabil dulu)
// ===========================================================
static int stableCount = 0;
static long lastIR = 0;

// ===========================================================
//  FUNGSI PUBLIK — Dipanggil dari main.ino
// ===========================================================
bool setup_sensor() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("[SENSOR] MAX30102 TIDAK DITEMUKAN! Periksa koneksi kabel.");
        return false;
    }

    particleSensor.setup(60, 4, 2, 100, 411, 4096);
    particleSensor.setPulseAmplitudeRed(0x2F);
    particleSensor.setPulseAmplitudeIR(0x2F);
    particleSensor.setPulseAmplitudeGreen(0);

    Serial.println("[SENSOR] MAX30102 berhasil diinisialisasi.");
    return true;
}

void loop_sensor() {
    long irRaw  = particleSensor.getIR();
    long redRaw = particleSensor.getRed();
    unsigned long now = millis();

    // ----- NO FINGER -----
    if (irRaw < 20000) {
        if (currentData.fingerOn) {
            // Jari baru saja diangkat — reset semua kalkulasi
            resetAll();
            stableCount = 0;
            lastIR = 0;
        }
        currentData = {0, 0, false, false};
        return;
    }

    currentData.fingerOn = true;

    // ----- SMOOTHING IR -----
    long filtered = smoothIR(irRaw);

    // ----- SPIKE FILTER -----
    if (abs(filtered - lastIR) > 20000) {
        lastIR = filtered;
        return;
    }
    lastIR = filtered;

    // ----- STABILISASI AWAL -----
    if (filtered > 20000) stableCount++;
    else stableCount = 0;

    if (stableCount < 25) return;

    // ----- UPDATE DC BASELINE (alpha kecil = DC lambat bergerak) -----
    const float alpha = 0.02f;
    if (irDC == 0) { irDC = irRaw; redDC = redRaw; }
    irDC  = (1.0f - alpha) * irDC  + alpha * irRaw;
    redDC = (1.0f - alpha) * redDC + alpha * redRaw;

    // ----- AKUMULASI AC (deviasi dari DC) -----
    irACSum  += fabsf((float)irRaw  - irDC);
    redACSum += fabsf((float)redRaw - redDC);
    acSamples++;

    // ----- PEAK DETECTION & BPM -----
    if (detectPeak(filtered)) {
        unsigned long delta = now - lastBeatTime;

        if (delta > 500 && delta < 2000) {
            float bpm = 60000.0f / delta;

            if (bpm >= 50 && bpm <= 120) {
                rates[rateSpot++] = bpm;
                rateSpot %= RATE_SIZE;
                if (rateSpot == 0) bpmReady = true;

                if (bpmReady) {
                    float sum = 0;
                    for (byte i = 0; i < RATE_SIZE; i++) sum += rates[i];
                    bpmAvg = sum / RATE_SIZE;
                }
            }
        }

        // Hitung SpO2 setiap kali ada detak (menggunakan AC yang terkumpul)
        calcSpO2OnBeat();

        lastBeatTime = now;
    }

    // ----- UPDATE STATE YANG DIBACA MAIN.INO -----
    currentData.bpm   = bpmReady  ? bpmAvg  : 0;
    currentData.spo2  = spo2Ready ? spo2Avg : 0;
    currentData.valid = bpmReady && spo2Ready;
}

OximeterData get_sensor_data() {
    return currentData;
}

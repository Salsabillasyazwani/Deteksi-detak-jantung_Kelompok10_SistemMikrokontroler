#include <Wire.h>
#include "MAX30105.h"

MAX30105 sensor;

// ===== BPM =====
unsigned long lastBeatTime = 0;
float bpm = 0;
float bpmAvg = 0;
bool bpmReady = false;

#define RATE_SIZE 5
float rates[RATE_SIZE];
byte rateSpot = 0;

// ===== SMOOTHING =====
long irBuffer[5];
int irIndex = 0;

long smoothIR(long value) {
  irBuffer[irIndex++] = value;
  irIndex %= 5;

  long sum = 0;
  for (int i = 0; i < 5; i++) sum += irBuffer[i];
  return sum / 5;
}

// ===== PEAK DETECTION =====
bool detectPeak(long irValue) {
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

    // filter lebih ketat
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

void setup() {
  Serial.begin(115200);
  Wire.begin(18, 19);

  if (!sensor.begin(Wire)) {
    Serial.println("Sensor error");
    while (1);
  }

  sensor.setup(60, 4, 2, 100, 411, 4096);
  sensor.setPulseAmplitudeRed(0x0F);
  sensor.setPulseAmplitudeIR(0x2F);

  Serial.println("time,ir,bpm,status");
}

void loop() {
  long irValue = sensor.getIR();
  unsigned long now = millis();

  // ===== NO FINGER =====
  if (irValue < 20000) {
    Serial.print(now);
    Serial.println(",0,0,NO_FINGER");
    delay(30);
    return;
  }

  // ===== SMOOTHING =====
  long filtered = smoothIR(irValue);

  // ===== SPIKE FILTER =====
  static long lastIR = 0;
  if (abs(filtered - lastIR) > 20000) {
    lastIR = filtered;
    return;
  }
  lastIR = filtered;

  // ===== STABILISASI =====
  static int stableCount = 0;
  if (filtered > 20000) stableCount++;
  else stableCount = 0;

  if (stableCount < 25) return;

  // ===== PEAK =====
  if (detectPeak(filtered)) {
    unsigned long delta = now - lastBeatTime;

    if (delta > 500 && delta < 2000) {
      bpm = 60000.0 / delta;

      // filter BPM masuk akal
      if (bpm >= 50 && bpm <= 120) {
        rates[rateSpot++] = bpm;
        rateSpot %= RATE_SIZE;

        if (rateSpot == 0) bpmReady = true;

        if (bpmReady) {
          float sum = 0;
          for (byte i = 0; i < RATE_SIZE; i++) {
            sum += rates[i];
          }
          bpmAvg = sum / RATE_SIZE;
        }
      }
    }

    lastBeatTime = now;
  }

  // ===== OUTPUT RAPI =====
  static unsigned long lastPrint = 0;
  if (now - lastPrint > 100) {
    lastPrint = now;

    Serial.print(now);
    Serial.print(",");
    Serial.print(filtered);
    Serial.print(",");

    if (bpmReady) Serial.print(bpmAvg);
    else Serial.print(0);

    Serial.println(",OK");
  }

  delay(10);
}
/**
 * =======================================================
 *  Oximeter - Secure IoT Pulse Oximeter
 *  ESP32 + MAX30102 + AES-256-CBC + MQTT
 * =======================================================
 *
 * Alur kerja:
 *   1. Hubungkan Wi-Fi
 *   2. Inisialisasi Sensor MAX30102
 *   3. Setup MQTT client ke broker EMQX
 *   4. Di setiap loop:
 *      a. Pastikan Wi-Fi & MQTT tetap terhubung
 *      b. Baca data BPM & SpO2 dari sensor
 *      c. Buat JSON payload
 *      d. Enkripsi payload dengan AES-256-CBC
 *      e. Publish Base64 ciphertext ke topik MQTT
 *
 * Library yang dibutuhkan (install via Library Manager):
 *   - SparkFun MAX3010x Pulse and Proximity Sensor Library
 *   - PubSubClient by Nick O'Leary
 *   - ArduinoJson by Benoit Blanchon
 *   - base64 by Densaugeo (atau library base64 Arduino lainnya)
 *
 * mbedtls sudah tersedia built-in di framework ESP32 Arduino.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "wifi_helper.h"
#include "mqtt.h"
#include "sensor.h"
#include "crypto.h"

// Interval pengiriman data ke broker (dalam millisecond)
const unsigned long PUBLISH_INTERVAL_MS = 2000;
static unsigned long lastPublishTime = 0;

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("========================================");
    Serial.println("  Oximeter - Secure IoT Pulse Oximeter ");
    Serial.println("========================================");

    // 1. Hubungkan ke Wi-Fi
    setup_wifi();

    // 2. Inisialisasi sensor MAX30102
    if (!setup_sensor()) {
        Serial.println("[MAIN] Sensor tidak ditemukan. Periksa wiring! Sistem berhenti.");
        while (true) { delay(1000); } // Halt
    }

    // 3. Setup MQTT client
    setup_mqtt();

    Serial.println("[MAIN] Sistem siap. Mulai monitoring...");
}

// ==========================================
// LOOP UTAMA
// ==========================================
void loop() {
    // Jaga koneksi Wi-Fi dan MQTT tetap aktif
    loop_wifi();
    loop_mqtt();

    // Baca data sensor secara non-blocking
    loop_sensor();

    // Kirim data secara berkala
    unsigned long now = millis();
    if (now - lastPublishTime >= PUBLISH_INTERVAL_MS) {
        lastPublishTime = now;

        // Ambil data sensor terkini
        OximeterData data = get_sensor_data();

        // Tampilkan di Serial Monitor untuk debugging
        Serial.printf("[MAIN] Jari: %s | BPM: %.1f | SpO2: %.1f%%\n",
                      data.fingerOn ? "Ya" : "Tidak",
                      data.bpm,
                      data.spo2);

        // Buat JSON payload
        // Contoh: {"bpm":75.5,"spo2":98.2,"finger":1}
        StaticJsonDocument<128> jsonDoc;
        jsonDoc["bpm"]    = data.fingerOn && data.valid ? roundf(data.bpm * 10) / 10.0f : 0;
        jsonDoc["spo2"]   = data.fingerOn && data.valid ? roundf(data.spo2 * 10) / 10.0f : 0;
        jsonDoc["finger"] = data.fingerOn ? 1 : 0;

        String jsonString;
        serializeJson(jsonDoc, jsonString);

        Serial.printf("[MAIN] Payload JSON: %s\n", jsonString.c_str());

        // Enkripsi payload dengan AES-256-CBC
        String encryptedPayload;
        if (aes_encrypt(jsonString, encryptedPayload)) {
            Serial.printf("[MAIN] Payload terenkripsi (Base64, %d byte)\n", encryptedPayload.length());

            // Publish ke MQTT broker jika terhubung
            if (is_mqtt_connected()) {
                mqtt_publish(encryptedPayload);
            } else {
                Serial.println("[MAIN] MQTT tidak terhubung, data tidak dikirim.");
            }
        } else {
            Serial.println("[MAIN] Gagal mengenkripsi data.");
        }
    }
}

#include <Arduino.h>
#include "wifi_helper.h"
#include "config.h"
#include <WiFi.h>

static unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL_MS = 5000;

void setup_wifi() {
    Serial.print("Menghubungkan ke Wi-Fi: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Tunggu sampai terhubung (blocking, maksimal 20 detik)
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 40) {
        delay(500);
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWi-Fi Terhubung!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nGagal terhubung ke Wi-Fi. ESP32 akan mencoba ulang di loop.");
    }
}

void loop_wifi() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = now;
            Serial.println("Wi-Fi terputus. Mencoba reconnect...");
            WiFi.disconnect();
            WiFi.reconnect();
        }
    }
}

bool is_wifi_connected() {
    return WiFi.status() == WL_CONNECTED;
}

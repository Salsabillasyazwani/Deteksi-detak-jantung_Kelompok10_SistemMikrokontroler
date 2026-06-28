#include "mqtt.h"
#include "config.h"
#include "wifi_helper.h"
#include <WiFi.h>
#include <PubSubClient.h>

// Klien Wi-Fi dan MQTT
static WiFiClient espWifiClient;
static PubSubClient mqttClient(espWifiClient);

static unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;

// Callback (tidak digunakan karena hanya publish, tapi dibutuhkan PubSubClient)
static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    // Tidak ada langganan (subscribe) di firmware ini
}

void setup_mqtt() {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);
    // Set buffer MQTT lebih besar agar payload enkripsi Base64 muat
    mqttClient.setBufferSize(512);
    Serial.println("[MQTT] Konfigurasi broker selesai.");
}

static bool reconnect_mqtt() {
    if (!is_wifi_connected()) {
        Serial.println("[MQTT] Tidak bisa reconnect: Wi-Fi belum terhubung.");
        return false;
    }
    Serial.printf("[MQTT] Mencoba koneksi ke %s:%d ...\n", MQTT_BROKER, MQTT_PORT);
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
        Serial.println("[MQTT] Terhubung ke broker!");
        return true;
    } else {
        Serial.printf("[MQTT] Gagal, rc=%d. Coba lagi dalam %lu detik.\n",
                      mqttClient.state(),
                      MQTT_RECONNECT_INTERVAL_MS / 1000);
        return false;
    }
}

void loop_mqtt() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL_MS) {
            lastMqttReconnectAttempt = now;
            reconnect_mqtt();
        }
    } else {
        mqttClient.loop();
    }
}

bool mqtt_publish(const String& encrypted_payload) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Publish gagal: tidak terhubung ke broker.");
        return false;
    }
    bool success = mqttClient.publish(MQTT_TOPIC_RAW, encrypted_payload.c_str(), false);
    if (success) {
        Serial.println("[MQTT] Payload terenkripsi berhasil dipublish.");
    } else {
        Serial.println("[MQTT] Gagal mempublish payload.");
    }
    return success;
}

bool is_mqtt_connected() {
    return mqttClient.connected();
}

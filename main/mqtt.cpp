#include "mqtt.h"
#include "config.h"
#include "wifi_helper.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Klien Wi-Fi dan MQTT
static WiFiClient espWifiClient;
static PubSubClient mqttClient(espWifiClient);

static unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;

// Pulse callback handler
static pulse_callback_t pulse_callback = nullptr;

// Callback untuk menerima pesan MQTT (pulse trigger)
static void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    String topicStr = String(topic);
    String payloadStr = String((char *)payload);
    payloadStr = payloadStr.substring(0, length); // Batasi length

    Serial.printf("[MQTT] Terima pesan dari topik: %s\n", topic);
    Serial.printf("[MQTT] Payload: %s\n", payloadStr.c_str());

    // Handle pulse trigger
    if (topicStr == MQTT_TOPIC_PULSE && pulse_callback)
    {
        try
        {
            StaticJsonDocument<128> doc;
            deserializeJson(doc, payloadStr);

            const char *action = doc["action"];
            float bpm = doc["bpm"] | 60.0;

            if (action && strcmp(action, "beep") == 0)
            {
                Serial.printf("[MQTT] Pulse trigger received - BPM: %.1f\n", bpm);
                pulse_callback(bpm);
            }
        }
        catch (...)
        {
            Serial.println("[MQTT] Error parsing pulse payload");
        }
    }
}

void setup_mqtt()
{
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);
    // Set buffer MQTT lebih besar agar payload enkripsi Base64 muat
    mqttClient.setBufferSize(512);
    Serial.println("[MQTT] Konfigurasi broker selesai.");
}

static bool reconnect_mqtt()
{
    if (!is_wifi_connected())
    {
        Serial.println("[MQTT] Tidak bisa reconnect: Wi-Fi belum terhubung.");
        return false;
    }
    Serial.printf("[MQTT] Mencoba koneksi ke %s:%d ...\n", MQTT_BROKER, MQTT_PORT);
    if (mqttClient.connect(MQTT_CLIENT_ID))
    {
        Serial.println("[MQTT] Terhubung ke broker!");
        // Subscribe ke pulse trigger topic
        mqttClient.subscribe(MQTT_TOPIC_PULSE);
        Serial.printf("[MQTT] Subscribe ke topik: %s\n", MQTT_TOPIC_PULSE);
        return true;
    }
    else
    {
        Serial.printf("[MQTT] Gagal, rc=%d. Coba lagi dalam %lu detik.\n",
                      mqttClient.state(),
                      MQTT_RECONNECT_INTERVAL_MS / 1000);
        return false;
    }
}

void loop_mqtt()
{
    if (!mqttClient.connected())
    {
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL_MS)
        {
            lastMqttReconnectAttempt = now;
            reconnect_mqtt();
        }
    }
    else
    {
        mqttClient.loop();
    }
}

bool mqtt_publish(const String &encrypted_payload)
{
    if (!mqttClient.connected())
    {
        Serial.println("[MQTT] Publish gagal: tidak terhubung ke broker.");
        return false;
    }
    bool success = mqttClient.publish(MQTT_TOPIC_RAW, encrypted_payload.c_str(), false);
    if (success)
    {
        Serial.println("[MQTT] Payload terenkripsi berhasil dipublish.");
    }
    else
    {
        Serial.println("[MQTT] Gagal mempublish payload.");
    }
    return success;
}

bool is_mqtt_connected()
{
    return mqttClient.connected();
}

// ==========================================
// Register Pulse Callback
// ==========================================
void mqtt_set_pulse_callback(pulse_callback_t callback)
{
    pulse_callback = callback;
    Serial.println("[MQTT] Pulse callback registered.");
}

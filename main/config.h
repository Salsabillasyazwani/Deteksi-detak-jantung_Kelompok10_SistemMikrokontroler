#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// Konfigurasi Wi-Fi
// ==========================================
// SSID dan password hotspot laptop kamu
// Aktifkan Mobile Hotspot di Windows: Settings → Network → Mobile Hotspot
#define WIFI_SSID "Los"          // ← ganti dengan nama hotspot laptop
#define WIFI_PASSWORD "12345678" // ← ganti dengan password hotspot laptop

// ==========================================
// Konfigurasi MQTT Broker
// ==========================================
#define MQTT_BROKER "192.168.137.1" // IP laptop saat jadi hotspot (gateway hotspot Windows)
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "esp32_oximeter_client"
#define MQTT_TOPIC_RAW "oximeter/raw" // Topik untuk data terenkripsi

// ==========================================
// Konfigurasi AES-256-CBC Cryptography
// ==========================================
// Kunci AES harus tepat 32 byte (256-bit)
extern const char *aes_key;
// IV (Initialization Vector) harus tepat 16 byte (128-bit)
extern const char *aes_iv;

// ==========================================
// Konfigurasi Sensor MAX30102
// ==========================================
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

#endif // CONFIG_H

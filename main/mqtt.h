#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>

/**
 * Menginisialisasi koneksi MQTT (pertama kali dipanggil di setup()).
 * Harus dipanggil setelah Wi-Fi terhubung.
 */
void setup_mqtt();

/**
 * Menjaga koneksi MQTT tetap aktif dan melakukan reconnect otomatis.
 * Dipanggil setiap iterasi di loop().
 */
void loop_mqtt();

/**
 * Mempublish payload terenkripsi ke topik MQTT_TOPIC_RAW.
 * @param encrypted_payload String Base64 hasil enkripsi AES
 * @return true jika berhasil publish
 */
bool mqtt_publish(const String& encrypted_payload);

/**
 * Mengembalikan true jika klien MQTT terhubung ke broker
 */
bool is_mqtt_connected();

#endif // MQTT_H

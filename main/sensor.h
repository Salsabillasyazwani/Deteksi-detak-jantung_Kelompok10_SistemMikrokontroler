#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

/**
 * Struct yang menyimpan hasil pembacaan sensor MAX30102
 */
struct OximeterData {
    float bpm;        // Detak jantung (BPM)
    float spo2;       // Saturasi oksigen (%)
    bool  fingerOn;   // true jika jari terdeteksi di sensor
    bool  valid;      // true jika data pembacaan valid (bukan 0 atau noise)
};

/**
 * Menginisialisasi sensor MAX30102.
 * Harus dipanggil di dalam setup().
 * @return true jika sensor berhasil dideteksi
 */
bool setup_sensor();

/**
 * Membaca data dari sensor secara non-blocking.
 * Dipanggil setiap iterasi loop().
 * Mengupdate state internal sensor.
 */
void loop_sensor();

/**
 * Mendapatkan hasil pembacaan sensor terkini.
 * @return Struct OximeterData berisi BPM, SpO2, dan status jari.
 */
OximeterData get_sensor_data();

#endif // SENSOR_H

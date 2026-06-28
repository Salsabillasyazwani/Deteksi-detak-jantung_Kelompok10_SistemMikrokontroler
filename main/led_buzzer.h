#ifndef LED_BUZZER_H
#define LED_BUZZER_H

#include <Arduino.h>

/**
 * Inisialisasi pin LED dan BUZZER sebagai output
 * Harus dipanggil di dalam setup()
 */
void setup_led_buzzer();

/**
 * Update display LED dan buzzer berdasarkan BPM
 * Dipanggil setiap loop untuk menjaga animasi berjalan smooth
 */
void update_led_buzzer(float bpm, bool fingerOn);

/**
 * Trigger buzzer dari MQTT pulse command (untuk sinkronisasi dengan grafik web)
 * Dipanggil ketika backend mengirim pulse trigger
 */
void buzzer_pulse_trigger(float bpm);

#endif // LED_BUZZER_H

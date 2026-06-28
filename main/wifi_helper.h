#ifndef WIFI_H
#define WIFI_H

// Menyiapkan koneksi Wi-Fi awal secara blocking dengan timeout
void setup_wifi();

// Mengecek status Wi-Fi secara berkala tanpa memblokir program utama
void loop_wifi();

// Mengembalikan true jika Wi-Fi terhubung
bool is_wifi_connected();

#endif // WIFI_H

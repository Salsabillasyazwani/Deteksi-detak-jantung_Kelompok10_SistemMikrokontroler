#ifndef CRYPTO_H
#define CRYPTO_H

#include <Arduino.h>

/**
 * Mengenkripsi plaintext menggunakan AES-256-CBC + PKCS7 Padding
 * via library mbedtls yang sudah built-in di ESP32.
 *
 * @param plaintext   String input yang akan dienkripsi
 * @param output      Buffer output berupa string Base64 dari hasil enkripsi
 * @return true jika berhasil, false jika gagal
 */
bool aes_encrypt(const String& plaintext, String& output);

#endif // CRYPTO_H

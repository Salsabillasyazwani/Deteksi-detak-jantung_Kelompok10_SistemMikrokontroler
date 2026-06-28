#include "crypto.h"
#include "config.h"
#include <Arduino.h>
#include <mbedtls/aes.h>
#include <base64.h>

// Kunci AES-256 (32 byte) dan IV (16 byte)
// Harus identik dengan yang ada di backend Python (crypto.py)
const char* aes_key = "my32bytessecretkeyforoximeteraes";
const char* aes_iv  = "my16bytessecreti";

bool aes_encrypt(const String& plaintext, String& output) {
    // Konversi plaintext ke byte array
    size_t plaintext_len = plaintext.length();

    // Hitung panjang dengan padding PKCS7 (kelipatan 16 byte)
    size_t padded_len = ((plaintext_len / 16) + 1) * 16;

    // Alokasi buffer dengan zero-fill untuk keamanan
    uint8_t* padded_input  = (uint8_t*)calloc(padded_len, 1);
    uint8_t* encrypted_buf = (uint8_t*)calloc(padded_len, 1);

    if (!padded_input || !encrypted_buf) {
        Serial.println("[CRYPTO] Gagal alokasi memori!");
        free(padded_input);
        free(encrypted_buf);
        return false;
    }

    // Salin plaintext ke buffer
    memcpy(padded_input, plaintext.c_str(), plaintext_len);

    // Terapkan PKCS7 Padding
    uint8_t pad_value = (uint8_t)(padded_len - plaintext_len);
    for (size_t i = plaintext_len; i < padded_len; i++) {
        padded_input[i] = pad_value;
    }

    // Inisialisasi konteks mbedtls AES
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);

    // Set kunci enkripsi AES-256 (256 bit = 32 byte)
    int ret = mbedtls_aes_setkey_enc(&aes_ctx, (const uint8_t*)aes_key, 256);
    if (ret != 0) {
        Serial.printf("[CRYPTO] Set kunci gagal: -0x%04X\n", (unsigned int)-ret);
        mbedtls_aes_free(&aes_ctx);
        free(padded_input);
        free(encrypted_buf);
        return false;
    }

    // Salin IV ke buffer sementara karena mbedtls memodifikasi IV
    uint8_t iv_buf[16];
    memcpy(iv_buf, aes_iv, 16);

    // Enkripsi menggunakan mode CBC
    ret = mbedtls_aes_crypt_cbc(
        &aes_ctx,
        MBEDTLS_AES_ENCRYPT,
        padded_len,
        iv_buf,
        padded_input,
        encrypted_buf
    );

    mbedtls_aes_free(&aes_ctx);

    if (ret != 0) {
        Serial.printf("[CRYPTO] Enkripsi CBC gagal: -0x%04X\n", (unsigned int)-ret);
        free(padded_input);
        free(encrypted_buf);
        return false;
    }

    // Encode hasil enkripsi ke Base64 agar aman dikirim via MQTT
    String encoded = base64::encode(encrypted_buf, padded_len);
    output = encoded;

    free(padded_input);
    free(encrypted_buf);
    return true;
}

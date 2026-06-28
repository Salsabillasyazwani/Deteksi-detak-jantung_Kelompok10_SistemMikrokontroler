# Secure IoT Pulse Oximeter
## Deteksi Detak Jantung & Saturasi Oksigen Real-Time dengan Keamanan AES-256

**Mata Kuliah:** Sistem Mikrokontroler (TIF RP-23 CNSA)
**Kelompok:** 10

---

## 👥 Anggota Tim

| Nama | NIM |
|------|-----|
| **Raffly** | 23552011278 |
| **Salsa Billa Syazwani** | 23552011402 |

---

## 📋 Daftar Isi
- [Deskripsi Proyek](#-deskripsi-proyek)
- [Fitur Utama](#-fitur-utama)
- [Spesifikasi Hardware](#-spesifikasi-hardware)
- [Arsitektur Sistem](#-arsitektur-sistem)
- [Instalasi](#-instalasi)
- [Cara Penggunaan](#-cara-penggunaan)
- [Teknologi yang Digunakan](#-teknologi-yang-digunakan)
- [Struktur Folder](#-struktur-folder)
- [Pin Configuration](#-pin-configuration)
- [Troubleshooting](#-troubleshooting)

---

## 🎯 Deskripsi Proyek

Sistem **Secure IoT Pulse Oximeter** adalah perangkat pemantauan kesehatan jantung yang dapat mengukur:
- **Detak Jantung (BPM)** - Beats Per Minute
- **Saturasi Oksigen (SpO2)** - Oxygen Saturation

Sistem ini menggunakan **ESP32 microcontroller** dengan sensor **MAX30102** untuk pembacaan data akurat. Semua data terenkripsi dengan **AES-256-CBC** dan dikirim via **MQTT Broker** ke backend server. Dashboard web memberikan visualisasi real-time dengan analisis AI menggunakan Google Gemini.

### Keunggulan:
✅ **Enkripsi End-to-End** - Data terenkripsi AES-256
✅ **Real-Time Monitoring** - Update setiap 2 detik
✅ **Visual Feedback** - LED wave + Buzzer sync dengan grafik
✅ **AI Analysis** - Rekomendasi kesehatan otomatis
✅ **Database Storage** - Riwayat pengukuran tersimpan di SQLite

---

## ✨ Fitur Utama

### 🔴 Sensor & Pembacaan
- Sensor MAX30102 untuk PPG (Photoplethysmogram)
- Algoritma adaptive threshold untuk deteksi beat yang akurat
- Smoothing buffer untuk sinyal yang stabil
- Kompatibel dengan berbagai jenis jari

### 📊 Indikator Visual
| Komponen | Fungsi |
|----------|--------|
| **LED 1-5** | Wave pattern mengikuti irama detak jantung |
| **LED 1** | Hanya menyala saat BPM belum stabil |
| **BUZZER** | Beep "tit-tit" sinkron dengan grafik web |
| **Alarm** | Suara panjang 7 detik saat jari dilepas |

### 🔐 Keamanan
- Enkripsi AES-256-CBC pada setiap payload
- IV (Initialization Vector) unik untuk setiap transmisi
- Base64 encoding untuk transmisi MQTT

### 📱 Dashboard Web
- Grafik real-time BPM & SpO2
- Status koneksi MQTT & Backend
- AI Analysis powered by Google Gemini
- Rekomendasi kesehatan berdasarkan usia
- Deteksi abnormal (bradikardia/takikardia)

### 🔄 Sinkronisasi Real-Time
- Backend mengirim pulse trigger saat grafik update
- Buzzer & LED beep bersamaan dengan grafik
- Non-blocking timing untuk smooth animation

---

## 🔧 Spesifikasi Hardware

### Komponen Utama:
```
┌─────────────────────────────────────────┐
│   ESP32 DevKit v4                       │
│   - Dual-core 240 MHz                   │
│   - Wi-Fi + Bluetooth                   │
│   - ADC, SPI, I2C, UART                 │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│   MAX30102 Pulse Oximeter Sensor        │
│   - I2C Interface                       │
│   - Red & IR LED                        │
│   - Fotodioda terintegrasi              │
│   - Range: 0-400 BPM                    │
└─────────────────────────────────────────┘
              ↓
   ┌─────────────┬───────────────┐
   ↓             ↓               ↓
[ LED ]      [ BUZZER ]    [ Push Button ]
 (5x)         (GPIO 25)
(GPIO)
```

### Pin Assignment:
| Komponen | GPIO Pin | Fungsi |
|----------|----------|--------|
| I2C SDA | 21 | Sensor communication |
| I2C SCL | 22 | Sensor communication |
| LED 1 | 26 | Indikator visual 1 |
| LED 2 | 27 | Indikator visual 2 |
| LED 3 | 14 | Indikator visual 3 |
| LED 4 | 12 | Indikator visual 4 |
| LED 5 | 13 | Indikator visual 5 |
| BUZZER | 25 | Audio feedback |

---

## 🏗️ Arsitektur Sistem

```
┌──────────────────────────────────────────────────────────────┐
│                     SECURE IOT ARCHITECTURE                  │
└──────────────────────────────────────────────────────────────┘

[ESP32 Oximeter]
    ├─ Sensor: MAX30102 (I2C)
    ├─ WiFi: Connect ke hotspot
    └─ MQTT: Publish encrypted data
           ↓
    ┌─ oximeter/raw (terenkripsi AES-256-CBC Base64)
    │
    ↓
[MQTT Broker] (EMQX / Mosquitto)
    │
    ├─→ oximeter/raw (input terenkripsi)
    └─→ oximeter/pulse (output trigger buzzer)
           ↓
[Backend Server] (Python Flask)
    ├─ Decrypt AES-256-CBC
    ├─ Parse JSON data (BPM, SpO2, Finger status)
    ├─ Store ke SQLite database
    └─ Publish ke oximeter/decrypted + oximeter/pulse
           ↓
    ┌─────────┴──────────┐
    ↓                    ↓
[Web Dashboard]     [ESP32 Buzzer Trigger]
(Real-time chart)   (Pulse beep sync)
+ AI Analysis
```

---

## 📥 Instalasi

### 1️⃣ **Persiapan Hardware**
```bash
# Koneksi ESP32 ↔ MAX30102:
ESP32      →    MAX30102
GPIO 21    →    SDA
GPIO 22    →    SCL
3.3V       →    VCC
GND        →    GND

# Koneksi LED (dengan resistor 220Ω):
GPIO 26 → LED 1 (anode) → 220Ω → GND (katode)
GPIO 27 → LED 2 (anode) → 220Ω → GND (katode)
GPIO 14 → LED 3 (anode) → 220Ω → GND (katode)
GPIO 12 → LED 4 (anode) → 220Ω → GND (katode)
GPIO 13 → LED 5 (anode) → 220Ω → GND (katode)

# Koneksi BUZZER:
GPIO 25 → BUZZER (+) → GND (-)
```

### 2️⃣ **Setup Arduino IDE**
```bash
# 1. Download Arduino IDE dari https://www.arduino.cc/
# 2. Install ESP32 Board Manager:
#    Tools → Board Manager → search "esp32" → Install
# 3. Install Libraries (Sketch → Include Library → Manage Libraries):
#    - SparkFun MAX3010x Pulse and Proximity Sensor Library
#    - PubSubClient (MQTT)
#    - ArduinoJson
#    - mbedtls (built-in with ESP32)
```

### 3️⃣ **Setup Backend (Windows)**
```bash
# Clone atau download project
cd backend

# Install Python dependencies
pip install -r requirements.txt

# Edit config (jika perlu):
# - MQTT_BROKER: "localhost" atau IP broker
# - DATABASE: "database.db"

# Jalankan backend:
python app.py
```

### 4️⃣ **Setup Web Dashboard**
```bash
# Copy folder 'web' ke public directory:
# - index.html
# - script.js
# - style.css

# Buka di browser: http://localhost:5000
# (sesuaikan port dengan konfigurasi backend)
```

### 5️⃣ **Setup MQTT Broker (Optional - untuk testing lokal)**
```bash
# Menggunakan Docker (recommended):
docker-compose -f docker/docker-compose.yml up -d

# Atau install mosquitto manual:
# Windows: Download dari https://mosquitto.org/download/
# Linux: sudo apt-get install mosquitto mosquitto-clients
# macOS: brew install mosquitto
```

---

## 🚀 Cara Penggunaan

### Step-by-Step:

**1. Persiapan:**
```bash
# Terminal 1: Jalankan MQTT Broker
docker-compose -f docker/docker-compose.yml up

# Terminal 2: Jalankan Backend
cd backend
python app.py
# Expected: "Backend running at http://localhost:5000"
```

**2. Upload ke ESP32:**
```bash
# Di Arduino IDE:
1. Buka main/main.ino
2. Edit config:
   - WIFI_SSID = nama hotspot laptop
   - WIFI_PASSWORD = password hotspot
   - MQTT_BROKER = IP laptop (192.168.x.x)
3. Tools → Select Board → ESP32 Dev Module
4. Select COM Port
5. Upload (Ctrl + U)
6. Monitor Serial (9600 baud) untuk debug logs
```

**3. Penggunaan:**
```
1. Letakkan jari di sensor MAX30102
2. Tunggu 3-5 detik hingga BPM stabil (LED 1 → LED wave mulai)
3. Lihat grafik di dashboard web (update setiap 2 detik)
4. Dengarkan buzzer beep sinkron dengan grafik
5. LED 1-5 membentuk wave pattern
6. Lepas jari → Alarm bunyi panjang 7 detik
```

---

## 💻 Teknologi yang Digunakan

### Hardware:
- **Microcontroller:** ESP32 (Espressif)
- **Sensor:** MAX30102 (Maxim Integrated)
- **Communication:** Wi-Fi, MQTT
- **Output:** LED, Buzzer

### Firmware (C++):
- **Framework:** Arduino IDE + ESP32 Arduino Core
- **Libraries:** PubSubClient, ArduinoJson, SparkFun MAX3010x
- **Cryptography:** mbedtls (AES-256-CBC)

### Backend (Python):
- **Framework:** Flask (Web server)
- **MQTT:** paho-mqtt
- **Database:** SQLite3
- **Cryptography:** cryptography library (AES-256-CBC)

### Frontend (Web):
- **HTML/CSS/JavaScript** (Vanilla)
- **Chart.js** untuk visualisasi grafik
- **WebSocket MQTT** untuk real-time update
- **Google Gemini API** untuk AI analysis

### DevOps:
- **Docker** untuk containerisasi MQTT Broker (EMQX)
- **EMQX** MQTT Broker

---

## 📁 Struktur Folder

```
Deteksi-detak-jantung_Kelompok10_SistemMikrokontroler/
│
├── README.md                    ← Dokumentasi (file ini)
│
├── main/                        ← Firmware ESP32
│   ├── main.ino                 ← Entry point
│   ├── config.h                 ← Konfigurasi (WIFI, MQTT, PIN)
│   ├── sensor.h/.cpp            ← Pembacaan sensor MAX30102
│   ├── sensor.h                 ← Peak detection, BPM calculation
│   ├── spo2_algorithm.h/.cpp    ← Algoritma SpO2 calculation
│   ├── mqtt.h/.cpp              ← MQTT publish/subscribe
│   ├── led_buzzer.h/.cpp        ← LED wave + Buzzer control
│   ├── wifi_helper.h/.cpp       ← Wi-Fi connection
│   └── crypto.h/.cpp            ← AES-256-CBC encryption
│
├── backend/                     ← Server Python
│   ├── app.py                   ← Flask server + routing
│   ├── mqtt_client.py           ← MQTT client (subscribe + publish)
│   ├── database.py              ← SQLite operations
│   ├── crypto.py                ← AES-256-CBC decryption
│   ├── requirements.txt          ← Python dependencies
│   ├── database.db              ← SQLite database (auto-created)
│   └── simulate_esp32.py        ← Testing script (optional)
│
├── web/                         ← Dashboard Web
│   ├── index.html               ← UI structure
│   ├── script.js                ← Logic + MQTT client
│   └── style.css                ← Styling (dark theme)
│
└── docker/                      ← Docker configuration
    ├── docker-compose.yml       ← EMQX MQTT Broker
    └── emqx.conf                ← EMQX configuration
```

---

## 🔌 Pin Configuration

### I2C (Sensor MAX30102):
```
ESP32            MAX30102
GPIO 21 (SDA) ←→ SDA
GPIO 22 (SCL) ←→ SCL
3.3V          ←→ VCC
GND           ←→ GND
```

### GPIO Output:
```
LED 1: GPIO 26  → Indikator visual
LED 2: GPIO 27  → Indikator visual
LED 3: GPIO 14  → Indikator visual
LED 4: GPIO 12  → Indikator visual
LED 5: GPIO 13  → Indikator visual
BUZZER: GPIO 25 → Audio feedback
```

---

## ⚙️ Konfigurasi

### config.h - Edit sesuai environment:
```cpp
#define WIFI_SSID "YourHotspot"          // Nama Wi-Fi
#define WIFI_PASSWORD "YourPassword"     // Password Wi-Fi
#define MQTT_BROKER "192.168.1.100"      // IP MQTT Broker
#define MQTT_PORT 1883                   // Port MQTT
#define AES_KEY "..." // 32 byte key      // Kunci AES (32 byte)
#define AES_IV "..."  // 16 byte IV       // IV AES (16 byte)
```

### backend/mqtt_client.py:
```python
MQTT_BROKER = "localhost"       # Alamat broker
MQTT_PORT = 1883                # Port broker
MQTT_RAW_TOPIC = "oximeter/raw" # Topic subscribe
MQTT_DECRYPTED_TOPIC = "oximeter/decrypted"
MQTT_PULSE_TOPIC = "oximeter/pulse"
```

---

## 🐛 Troubleshooting

### ❌ Sensor tidak terdeteksi
```
Error: "Sensor tidak ditemukan"
Solution:
- Cek koneksi I2C (SDA/SCL wiring)
- Verifikasi MAX30102 address (0x57 default)
- Download driver CH340 jika ESP32 tidak terdeteksi
```

### ❌ MQTT tidak terhubung
```
Error: "MQTT tidak terhubung"
Solution:
- Pastikan MQTT Broker running: docker-compose up
- Cek IP broker: ipconfig (Windows) / ifconfig (Linux)
- Pastikan firewall tidak memblokir port 1883
```

### ❌ Grafik tidak update
```
Error: "Grafik web tidak bergerak"
Solution:
- Buka browser DevTools (F12) → Console
- Cek error MQTT connection
- Pastikan backend running (python app.py)
- Cek URL MQTT broker di script.js
```

### ❌ BPM selalu 0
```
Error: "BPM tidak terbaca"
Solution:
- Pastikan jari ditekan kuat di sensor
- Tunggu 5-10 detik sensor calibrate
- Cek pembacaan raw IR/Red di Serial Monitor
- Verifikasi algoritma deteksi peak
```

### ❌ Buzzer tidak berbunyi
```
Error: "Buzzer diam"
Solution:
- Cek polaritas buzzer (+/- benar)
- Test GPIO 25 dengan digitalWrite(BUZZER, HIGH)
- Pastikan soundEnabled = true di web
- Cek MQTT pulse topic komunikasi
```

---

## 📊 Data Flow

### Pengiriman Data (setiap 2 detik):
```
Sensor → BPM/SpO2 Calculation → JSON Creation
         ↓
     Encryption (AES-256-CBC) → Base64 Encoding
         ↓
     MQTT Publish (oximeter/raw)
         ↓
     Backend Receive & Decrypt
         ↓
     Publish to oximeter/decrypted
         ↓
     Web Dashboard + Database
```

### Sinkronisasi Buzzer:
```
Backend Receive Data (oximeter/raw)
         ↓
     Publish Pulse Trigger (oximeter/pulse)
         ↓
ESP32 Receive Pulse Command
         ↓
Buzzer Beep (tit-tit) + LED Wave Sync
         ↓
Web Dashboard Update Grafik (serentak)
```

---

## 📝 Lisensi & Credit

**Proyek:** Sistem Mikrokontroler - Deteksi Detak Jantung IoT
**Tim:** Kelompok 10 TIF RM-23 CNSA
**Teknologi:** Open Source (Arduino, EMQX, Flask)

---

**Last Updated:** 28 Juni 2026
**Version:** 1.0.0


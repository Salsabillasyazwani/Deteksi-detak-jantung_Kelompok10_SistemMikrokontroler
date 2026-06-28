"""
simulate_esp32.py - Script untuk mensimulasikan pengiriman data ESP32
Berguna untuk menguji backend dan dashboard TANPA hardware fisik.

Cara pakai:
    pip install paho-mqtt pycryptodome
    python simulate_esp32.py
"""

import paho.mqtt.client as mqtt
import json
import time
import random
import sys
import os

# Tambahkan path backend agar bisa import crypto
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'backend'))
from crypto import encrypt_data

MQTT_BROKER = "localhost"
MQTT_PORT   = 1883
MQTT_TOPIC  = "oximeter/raw"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[SIMULATOR] Terhubung ke EMQX Broker!")
    else:
        print(f"[SIMULATOR] Gagal terhubung, rc={rc}")

def generate_fake_data(finger_on=True):
    """Membuat data sensor palsu yang realistis."""
    if not finger_on:
        return {"bpm": 0, "spo2": 0, "finger": 0}
    
    bpm  = round(random.uniform(65, 95), 1)
    spo2 = round(random.uniform(95, 100), 1)
    return {"bpm": bpm, "spo2": spo2, "finger": 1}

def main():
    client = mqtt.Client()
    client.on_connect = on_connect

    print(f"[SIMULATOR] Menghubungkan ke {MQTT_BROKER}:{MQTT_PORT}...")
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()

    time.sleep(1.5)  # Tunggu koneksi

    print("[SIMULATOR] Mulai mengirim data simulasi setiap 2 detik...")
    print("[SIMULATOR] Tekan Ctrl+C untuk berhenti.\n")

    cycle = 0
    try:
        while True:
            cycle += 1
            # Setiap 10 siklus, simulasikan jari diangkat
            finger_on = (cycle % 10 != 0)

            data = generate_fake_data(finger_on)
            json_str = json.dumps(data)

            # Enkripsi data
            encrypted = encrypt_data(json_str)

            print(f"[SIMULATOR] Kirim -> JSON: {json_str}")
            print(f"[SIMULATOR]          Encrypted (Base64): {encrypted[:40]}...")
            
            result = client.publish(MQTT_TOPIC, encrypted, qos=1)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print(f"[SIMULATOR] Berhasil dipublish ke '{MQTT_TOPIC}'\n")
            else:
                print(f"[SIMULATOR] Gagal publish, rc={result.rc}\n")

            time.sleep(2)

    except KeyboardInterrupt:
        print("\n[SIMULATOR] Simulasi dihentikan.")
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()

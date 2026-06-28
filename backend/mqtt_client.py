import paho.mqtt.client as mqtt
import json
from datetime import datetime
from crypto import decrypt_data
from database import save_measurement

MQTT_BROKER         = "localhost"
MQTT_PORT           = 1883
MQTT_RAW_TOPIC      = "oximeter/raw"
MQTT_DECRYPTED_TOPIC = "oximeter/decrypted"

# Inisialisasi client (paho-mqtt 1.x)
client = mqtt.Client(client_id="oximeter_backend", clean_session=True)

# Signature on_connect paho-mqtt 1.x: (client, userdata, flags, rc)
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("MQTT Client: Terhubung ke Broker!")
        client.subscribe(MQTT_RAW_TOPIC)
        print(f"MQTT Client: Subscribe ke topik '{MQTT_RAW_TOPIC}'")
    else:
        print(f"MQTT Client: Gagal terhubung, rc={rc}")

def on_message(client, userdata, msg):
    try:
        # Ambil data Base64 terenkripsi
        encrypted_payload = msg.payload.decode('utf-8').strip()

        # Dekripsi
        decrypted_str = decrypt_data(encrypted_payload)
        if not decrypted_str:
            print("MQTT Client: Gagal mendekripsi payload.")
            return

        print(f"MQTT Client: Data berhasil didekripsi -> {decrypted_str}")

        # Parse JSON data
        data   = json.loads(decrypted_str)
        bpm    = data.get('bpm', 0)
        spo2   = data.get('spo2', 0)
        finger = data.get('finger', 0)

        # Simpan ke SQLite
        save_measurement(bpm, spo2, finger)

        # Tambahkan timestamp lokal untuk Web UI
        data['timestamp'] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Publish data yang telah didekripsi untuk Web Dashboard
        client.publish(MQTT_DECRYPTED_TOPIC, json.dumps(data), qos=1)

    except Exception as e:
        print(f"MQTT Client: Error memproses pesan -> {e}")

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print(f"MQTT Client: Koneksi terputus (rc: {rc}). Auto-reconnect aktif...")

def start_mqtt_client():
    client.on_connect    = on_connect
    client.on_message    = on_message
    client.on_disconnect = on_disconnect

    try:
        # loop_start() mengaktifkan auto-reconnect otomatis di background thread
        client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
        client.loop_start()
        print("MQTT Client: Background loop aktif.")
    except Exception as e:
        print(f"MQTT Client: Gagal koneksi awal ke {MQTT_BROKER}:{MQTT_PORT} -> {e}")
        print("MQTT Client: Backend tetap berjalan. MQTT akan retry saat broker tersedia.")

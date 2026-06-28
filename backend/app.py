import os
from flask import Flask, jsonify, request
from flask_cors import CORS
from database import init_db, get_history
from mqtt_client import start_mqtt_client

# Dapatkan absolute path ke folder web
web_folder = os.path.abspath(os.path.join(os.path.dirname(__file__), '../web'))
app = Flask(__name__, static_folder=web_folder, static_url_path='')
# Mengizinkan Cross-Origin Resource Sharing (CORS)
CORS(app)

@app.route('/')
def index():
    """
    Menyajikan halaman utama dashboard web Oximeter
    """
    return app.send_static_file('index.html')

@app.route('/api/history', methods=['GET'])
def history():
    """
    Endpoint API untuk mengambil riwayat data pengukuran oximeter terbaru.
    Dapat menggunakan query param ?limit=jumlah (default: 50)
    """
    limit = request.args.get('limit', default=50, type=int)
    data = get_history(limit)
    return jsonify({
        "status": "success",
        "count": len(data),
        "data": data
    })

@app.route('/api/status', methods=['GET'])
def status():
    """
    Endpoint status backend
    """
    return jsonify({
        "status": "online",
        "message": "Backend Oximeter siap digunakan"
    })

if __name__ == '__main__':
    # 1. Inisialisasi Database SQLite
    init_db()

    # 2. Jalankan MQTT Client
    start_mqtt_client()

    # 3. Jalankan server Flask
    # Menggunakan port 5000 secara default
    print(f"Static folder path: {app.static_folder}")
    print("Menjalankan Flask App di http://localhost:5000")
    app.run(host='0.0.0.0', port=5000, debug=False)

import sqlite3
import os

DB_PATH = os.path.join(os.path.dirname(__file__), 'database.db')

def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    """
    Membuat tabel measurements jika belum ada.
    """
    with get_db_connection() as conn:
        conn.execute('''
            CREATE TABLE IF NOT EXISTS measurements (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                bpm REAL,
                spo2 REAL,
                finger INTEGER,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        conn.commit()
    print("Database SQLite diinisialisasi.")

def save_measurement(bpm, spo2, finger):
    """
    Menyimpan data pengukuran ke database.
    """
    try:
        with get_db_connection() as conn:
            conn.execute('''
                INSERT INTO measurements (bpm, spo2, finger)
                VALUES (?, ?, ?)
            ''', (bpm, spo2, finger))
            conn.commit()
        return True
    except Exception as e:
        print(f"Error menyimpan ke DB: {e}")
        return False

def get_history(limit=50):
    """
    Mengambil data riwayat pengukuran terbaru.
    """
    try:
        with get_db_connection() as conn:
            # Ambil data terbaru, lalu balik urutannya agar kronologis (untuk charting)
            rows = conn.execute('''
                SELECT id, bpm, spo2, finger, timestamp
                FROM measurements
                ORDER BY timestamp DESC
                LIMIT ?
            ''', (limit,)).fetchall()
            
            data = [dict(row) for row in rows]
            data.reverse()
            return data
    except Exception as e:
        print(f"Error mengambil riwayat: {e}")
        return []

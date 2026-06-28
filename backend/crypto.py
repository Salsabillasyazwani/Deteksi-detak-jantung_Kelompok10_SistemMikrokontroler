import base64
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad, pad

# Kunci AES harus 32 byte (256-bit)
# IV harus 16 byte (128-bit)
# Gunakan nilai default ini untuk development. Ubah untuk produksi!
AES_KEY = b"my32bytessecretkeyforoximeteraes"
AES_IV = b"my16bytessecreti"

def decrypt_data(encrypted_base64):
    """
    Mendekripsi data terenkripsi AES-256-CBC dalam format Base64.
    """
    try:
        # Decode base64 ke bytes
        encrypted_bytes = base64.b64decode(encrypted_base64)
        
        # Inisialisasi cipher AES
        cipher = AES.new(AES_KEY, AES.MODE_CBC, AES_IV)
        
        # Dekripsi dan hapus padding (PKCS7)
        decrypted_bytes = unpad(cipher.decrypt(encrypted_bytes), AES.block_size)
        
        # Konversi ke string utf-8
        return decrypted_bytes.decode('utf-8')
    except Exception as e:
        print(f"Error saat dekripsi: {e}")
        return None

def encrypt_data(plaintext):
    """
    Fungsi pembantu untuk mengenkripsi data (berguna untuk testing/simulasi).
    """
    cipher = AES.new(AES_KEY, AES.MODE_CBC, AES_IV)
    padded_data = pad(plaintext.encode('utf-8'), AES.block_size)
    encrypted_bytes = cipher.encrypt(padded_data)
    return base64.b64encode(encrypted_bytes).decode('utf-8')

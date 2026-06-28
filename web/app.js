// Deteksi Host secara dinamis untuk mempermudah akses local network
const host = window.location.hostname || "localhost";
const MQTT_BROKER = `ws://${host}:8083/mqtt`;
const BACKEND_URL = `http://${host}:5000`;
const TOPIC_DECRYPTED = "oximeter/decrypted";

// State data chart
let chartInstance = null;
const MAX_CHART_POINTS = 20;
let chartLabels = [];
let bpmData = [];
let spo2Data = [];

// Element DOM
const elBpmVal = document.getElementById("bpm-val");
const elBpmStatus = document.getElementById("bpm-status-text");
const elSpo2Val = document.getElementById("spo2-val");
const elSpo2Status = document.getElementById("spo2-status-text");
const elFingerStatus = document.getElementById("finger-status");
const elFingerStatusText = document.getElementById("finger-status-text");
const elFingerIcon = document.getElementById("finger-icon");
const elHistoryBody = document.getElementById("history-table-body");
const btnRefreshHistory = document.getElementById("refresh-history-btn");
const elMqttStatus = document.getElementById("mqtt-status");
const elBackendStatus = document.getElementById("backend-status");

// 1. Inisialisasi Chart.js
function initChart() {
    const ctx = document.getElementById('realtimeChart').getContext('2d');
    
    // Gradient fill untuk BPM
    const bpmGradient = ctx.createLinearGradient(0, 0, 0, 300);
    bpmGradient.addColorStop(0, 'rgba(255, 59, 107, 0.4)');
    bpmGradient.addColorStop(1, 'rgba(255, 59, 107, 0.0)');

    // Gradient fill untuk SpO2
    const spo2Gradient = ctx.createLinearGradient(0, 0, 0, 300);
    spo2Gradient.addColorStop(0, 'rgba(0, 245, 212, 0.4)');
    spo2Gradient.addColorStop(1, 'rgba(0, 245, 212, 0.0)');

    chartInstance = new Chart(ctx, {
        type: 'line',
        data: {
            labels: chartLabels,
            datasets: [
                {
                    label: 'Detak Jantung (BPM)',
                    data: bpmData,
                    borderColor: '#ff3b6b',
                    backgroundColor: bpmGradient,
                    borderWidth: 3,
                    fill: true,
                    tension: 0.4,
                    pointBackgroundColor: '#ff3b6b',
                    pointHoverRadius: 7,
                    yAxisID: 'y-bpm'
                },
                {
                    label: 'Saturasi Oksigen (SpO₂)',
                    data: spo2Data,
                    borderColor: '#00f5d4',
                    backgroundColor: spo2Gradient,
                    borderWidth: 3,
                    fill: true,
                    tension: 0.4,
                    pointBackgroundColor: '#00f5d4',
                    pointHoverRadius: 7,
                    yAxisID: 'y-spo2'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
            plugins: {
                legend: {
                    labels: {
                        color: '#9ca3af',
                        font: { family: 'Plus Jakarta Sans', size: 12 }
                    }
                }
            },
            scales: {
                x: {
                    grid: { color: 'rgba(255, 255, 255, 0.05)' },
                    ticks: { color: '#9ca3af', font: { family: 'Plus Jakarta Sans' } }
                },
                'y-bpm': {
                    type: 'linear',
                    position: 'left',
                    min: 40,
                    max: 180,
                    grid: { color: 'rgba(255, 255, 255, 0.05)' },
                    ticks: { color: '#ff3b6b', font: { family: 'Plus Jakarta Sans' } },
                    title: { display: true, text: 'BPM', color: '#ff3b6b' }
                },
                'y-spo2': {
                    type: 'linear',
                    position: 'right',
                    min: 80,
                    max: 100,
                    grid: { drawOnChartArea: false }, // Jangan timpa garis grid BPM
                    ticks: { color: '#00f5d4', font: { family: 'Plus Jakarta Sans' } },
                    title: { display: true, text: 'SpO₂ %', color: '#00f5d4' }
                }
            }
        }
    });
}

// Update data chart
function updateChart(timeLabel, bpm, spo2) {
    if (!chartInstance) return;

    if (chartLabels.length >= MAX_CHART_POINTS) {
        chartLabels.shift();
        bpmData.shift();
        spo2Data.shift();
    }

    chartLabels.push(timeLabel);
    bpmData.push(bpm);
    spo2Data.push(spo2);

    chartInstance.update();
}

// 2. Fetch Data Riwayat dari Backend API
async function fetchHistory() {
    try {
        const response = await fetch(`${BACKEND_URL}/api/history?limit=30`);
        const json = await response.json();
        
        if (json.status === "success" && json.data) {
            updateHistoryTable(json.data);
            updateBackendStatus(true);
            
            // Populasikan chart awal dari data riwayat jika ada
            if (chartLabels.length === 0 && json.data.length > 0) {
                json.data.forEach(item => {
                    const time = item.timestamp.split(' ')[1] || item.timestamp;
                    // Hanya tambahkan ke chart jika ada pembacaan valid (jari terdeteksi)
                    if (item.finger === 1) {
                        chartLabels.push(time);
                        bpmData.push(item.bpm);
                        spo2Data.push(item.spo2);
                    }
                });
                if (chartInstance) chartInstance.update();
            }
        }
    } catch (error) {
        console.error("Gagal fetch data riwayat:", error);
        updateBackendStatus(false);
    }
}

// Render data riwayat ke tabel DOM
function updateHistoryTable(records) {
    if (records.length === 0) {
        elHistoryBody.innerHTML = `<tr><td colspan="4" class="empty-table">Belum ada riwayat pengukuran.</td></tr>`;
        return;
    }

    elHistoryBody.innerHTML = records.map(record => {
        const fingerBadge = record.finger === 1 
            ? `<span class="table-badge finger-ok">Detected</span>` 
            : `<span class="table-badge finger-none">Out</span>`;
            
        return `
            <tr>
                <td>${record.timestamp}</td>
                <td><strong>${record.finger ? record.bpm : '--'}</strong> BPM</td>
                <td><strong>${record.finger ? record.spo2 : '--'}</strong> %</td>
                <td>${fingerBadge}</td>
            </tr>
        `;
    }).join('');
}

// Tambahkan record baru ke tabel
function prependHistoryTable(record) {
    // Hapus baris kosong jika ada
    const emptyRow = elHistoryBody.querySelector('.empty-table');
    if (emptyRow) elHistoryBody.innerHTML = '';

    const fingerBadge = record.finger === 1 
        ? `<span class="table-badge finger-ok">Detected</span>` 
        : `<span class="table-badge finger-none">Out</span>`;

    const row = document.createElement('tr');
    row.innerHTML = `
        <td>${record.timestamp}</td>
        <td><strong>${record.finger ? record.bpm : '--'}</strong> BPM</td>
        <td><strong>${record.finger ? record.spo2 : '--'}</strong> %</td>
        <td>${fingerBadge}</td>
    `;
    elHistoryBody.insertBefore(row, elHistoryBody.firstChild);

    // Batasi baris tabel maksimum 30
    if (elHistoryBody.children.length > 30) {
        elHistoryBody.removeChild(elHistoryBody.lastChild);
    }
}

// 3. Update Status Badges UI
function updateBackendStatus(isOnline) {
    const dot = elBackendStatus.querySelector(".status-dot");
    const label = elBackendStatus.querySelector(".status-label");
    if (isOnline) {
        dot.className = "status-dot connected";
        label.innerText = "Backend: Online";
    } else {
        dot.className = "status-dot disconnected";
        label.innerText = "Backend: Offline";
    }
}

function updateMqttStatus(isConnected) {
    const dot = elMqttStatus.querySelector(".status-dot");
    const label = elMqttStatus.querySelector(".status-label");
    if (isConnected) {
        dot.className = "status-dot connected";
        label.innerText = "MQTT: Connected";
    } else {
        dot.className = "status-dot disconnected";
        label.innerText = "MQTT: Disconnected";
    }
}

// Check Backend Status berkala
async function checkBackendStatus() {
    try {
        const res = await fetch(`${BACKEND_URL}/api/status`);
        const json = await res.json();
        updateBackendStatus(json.status === "online");
    } catch {
        updateBackendStatus(false);
    }
}

// 4. Inisialisasi Koneksi MQTT WebSockets
function initMqtt() {
    updateMqttStatus(false);
    
    console.log(`Menghubungkan ke MQTT broker: ${MQTT_BROKER}`);
    const client = mqtt.connect(MQTT_BROKER, {
        clientId: 'web_dashboard_' + Math.random().toString(16).substr(2, 8),
        keepalive: 30,
        reconnectPeriod: 1000 * 3, // Auto reconnect setiap 3 detik jika putus
    });

    client.on('connect', () => {
        console.log('Terhubung ke EMQX via WebSockets!');
        updateMqttStatus(true);
        client.subscribe(TOPIC_DECRYPTED, (err) => {
            if (!err) {
                console.log(`Subscribe ke topik: ${TOPIC_DECRYPTED}`);
            }
        });
    });

    client.on('close', () => {
        console.log('Koneksi MQTT terputus.');
        updateMqttStatus(false);
    });

    client.on('error', (err) => {
        console.error('MQTT Client Error:', err);
        updateMqttStatus(false);
    });

    client.on('message', (topic, message) => {
        if (topic === TOPIC_DECRYPTED) {
            try {
                const payload = JSON.parse(message.toString());
                handleIncomingData(payload);
            } catch (e) {
                console.error("Gagal parse data MQTT:", e);
            }
        }
    });
}

// 5. Handle Data Masuk
function handleIncomingData(data) {
    const bpm = data.bpm;
    const spo2 = data.spo2;
    const finger = data.finger;
    const timestamp = data.timestamp || new Date().toLocaleTimeString();
    
    // Ambil string jam:menit:detik untuk label chart
    const timeLabel = timestamp.split(' ')[1] || timestamp;

    // Update UI berdasarkan status deteksi jari
    if (finger === 1) {
        // Jari terdeteksi
        elFingerStatus.className = "finger-status-indicator connected";
        elFingerStatusText.innerText = "Finger Detected";
        elFingerIcon.className = "fa-solid fa-fingerprint fa-bounce";
        elFingerIcon.style.color = "var(--accent-green)";

        // BPM — tampilkan jika sudah ada, atau tunjukkan "Loading..."
        if (bpm > 0) {
            elBpmVal.innerText = bpm;
            const heartIcon = document.querySelector(".bpm-card .heart-beat");
            heartIcon.classList.add("active");
            updateHealthStatusBpm(bpm);
            if (spo2 > 0) updateChart(timeLabel, bpm, spo2);
        } else {
            elBpmVal.innerText = "--";
            elBpmStatus.innerText = "Menghitung detak jantung...";
            elBpmStatus.className = "card-footer normal";
        }

        // SpO2 — tampilkan segera jika sudah terbaca (tidak perlu tunggu BPM)
        if (spo2 > 0) {
            elSpo2Val.innerText = spo2;
            updateHealthStatusSpo2(spo2);
        } else {
            elSpo2Val.innerText = "--";
            elSpo2Status.innerText = "Membaca kadar oksigen...";
            elSpo2Status.className = "card-footer normal";
        }
    } else {
        // Jari tidak ada
        elFingerStatus.className = "finger-status-indicator disconnected";
        elFingerStatusText.innerText = "Finger Out / Place Finger";
        elFingerIcon.className = "fa-solid fa-fingerprint";
        elFingerIcon.style.color = "var(--text-secondary)";

        elBpmVal.innerText = "--";
        elSpo2Val.innerText = "--";

        const heartIcon = document.querySelector(".bpm-card .heart-beat");
        heartIcon.classList.remove("active");

        elBpmStatus.innerText = "Masukkan jari ke sensor";
        elBpmStatus.className = "card-footer warning";
        
        elSpo2Status.innerText = "Tempatkan jari di atas MAX30102";
        elSpo2Status.className = "card-footer warning";
    }

    // Tambahkan baris baru ke tabel riwayat secara instan
    prependHistoryTable(data);
}

function updateHealthStatusBpm(bpm) {
    if (bpm < 60) {
        elBpmStatus.innerText = "Bradycardia (Rendah)";
        elBpmStatus.className = "card-footer warning";
    } else if (bpm <= 100) {
        elBpmStatus.innerText = "Normal";
        elBpmStatus.className = "card-footer normal";
    } else {
        elBpmStatus.innerText = "Tachycardia (Tinggi)";
        elBpmStatus.className = "card-footer danger";
    }
}

function updateHealthStatusSpo2(spo2) {
    if (spo2 >= 95) {
        elSpo2Status.innerText = "Saturasi Normal";
        elSpo2Status.className = "card-footer normal";
    } else if (spo2 >= 90) {
        elSpo2Status.innerText = "Hipoksia Ringan (Pantau)";
        elSpo2Status.className = "card-footer warning";
    } else {
        elSpo2Status.innerText = "Hipoksia Berat! Butuh Oksigen";
        elSpo2Status.className = "card-footer danger";
    }
}

// Tetap pertahankan fungsi lama untuk kompatibilitas (dipakai di tempat lain)
function updateHealthStatusTexts(bpm, spo2) {
    updateHealthStatusBpm(bpm);
    updateHealthStatusSpo2(spo2);
}

// Event Listeners
btnRefreshHistory.addEventListener('click', fetchHistory);

// Setup on load
window.addEventListener('DOMContentLoaded', () => {
    initChart();
    fetchHistory();
    initMqtt();

    // Check backend status every 10 seconds
    setInterval(checkBackendStatus, 10000);
});

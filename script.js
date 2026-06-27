const GEMINI_API_KEY = "Api gemini";
const GEMINI_MODEL = "gemini-2.5-flash";

let lastAnalysisTime = 0;
const ANALYSIS_COOLDOWN = 10000;

//MQTT Configuration
const MQTT_BROKER_URL = 'wss://192.168.137.1:18083/mqtt'; // Ganti dengan URL broker MQTT Anda (pastikan mendukung WebSocket)
const MQTT_OPTIONS = {
    clientId: 'heartmonitor_web_' + Math.random().toString(16).substr(2, 8),
    clean: true,
    connectTimeout: 5000,
    reconnectPeriod: 3000,
    username: 'admin',
    password: 'HengkerJahat@'
};

const TOPIC_BPM = 'heart/data';
const TOPIC_SPO2 = 'heart/status';
const TOPIC_CONTROL = 'sensor/control';

// Variabel umum
let mqttClient = null;
let isRunning = false;
let soundEnabled = false;
let heartRateChart = null;
let animationFrame = null;
let lastBPM = 0;
let lastSpO2 = 0;
let lastAnalysisTime = 0;
const ANALYSIS_COOLDOWN = 10000;

//  Data referensi kelompok usia
const ageGroups = {
    bayi: {
        title: 'Bayi (0-1 tahun)',
        normal: '100-160 BPM',
        low: {
            threshold: 100,
            causes: ['Hipotermia (kedinginan)', 'Gangguan jantung bawaan', 'Kurang oksigen']
        },
        high: {
            threshold: 160,
            causes: ['Demam', 'Infeksi', 'Dehidrasi', 'Stres / menangis']
        }
    },
    anak: {
        title: 'Anak (1-10 tahun)',
        normal: '70-120 BPM',
        low: {
            threshold: 70,
            causes: ['Aktivitas jantung lambat saat tidur (normal sesaat)', 'Gangguan konduksi jantung', 'Efek obat']
        },
        high: {
            threshold: 120,
            causes: ['Demam', 'Aktivitas fisik', 'Cemas / panik', 'Infeksi']
        }
    },
    remaja: {
        title: 'Remaja (11-17 tahun)',
        normal: '60-100 BPM',
        low: {
            threshold: 60,
            causes: ['Atlet (normal pada orang terlatih)', 'Bradikardia', 'Gangguan listrik jantung']
        },
        high: {
            threshold: 100,
            causes: ['Stres', 'Kurang tidur', 'Demam', 'Kafein']
        }
    },
    dewasa: {
        title: 'Dewasa (18-59 tahun)',
        normal: '60-100 BPM',
        low: {
            threshold: 60,
            causes: ['Atlet (normal)', 'Hipotiroid', 'Efek obat beta-blocker', 'Gangguan jantung']
        },
        high: {
            threshold: 100,
            causes: ['Stres / cemas', 'Demam', 'Anemia', 'Dehidrasi', 'Aktivitas fisik']
        }
    },
    lansia: {
        title: 'Lansia (60+ tahun)',
        normal: '60-90 BPM',
        low: {
            threshold: 60,
            causes: ['Degenerasi sistem listrik jantung', 'Obat jantung', 'Gangguan sinus node']
        },
        high: {
            threshold: 90,
            causes: ['Penyakit jantung', 'Infeksi', 'Stres tubuh', 'Gagal jantung ringan']
        }
    }
};

// Inisialisasi grafik
function initChart() {
    const ctx = document.getElementById('heartRateChart').getContext('2d');
    heartRateChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'BPM',
                data: [],
                borderColor: '#ef4444',
                backgroundColor: 'rgba(239, 68, 68, 0.1)',
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 0,
                pointHoverRadius: 6
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { display: false } },
            scales: {
                x: {
                    display: true,
                    grid: { color: 'rgba(255, 255, 255, 0.05)' },
                    ticks: { color: '#6b7280', maxTicksLimit: 10 }
                },
                y: {
                    display: true,
                    min: 40,
                    max: 180,
                    grid: { color: 'rgba(255, 255, 255, 0.05)' },
                    ticks: { color: '#6b7280' }
                }
            },
            interaction: { intersect: false, mode: 'index' }
        }
    });
}

// Fungsi bantu
function getNormalRange(age) {
    if (age < 1) return { min: 100, max: 160, group: 'bayi' };
    if (age <= 10) return { min: 70, max: 120, group: 'anak' };
    if (age <= 17) return { min: 60, max: 100, group: 'remaja' };
    if (age <= 59) return { min: 60, max: 100, group: 'dewasa' };
    return { min: 60, max: 90, group: 'lansia' };
}

// BPM
function updateBPMDisplay(bpm, spo2, finger = true) {
    const age = parseInt(document.getElementById('ageInput').value) || 25;
    const range = getNormalRange(age);

    document.getElementById('bpmValue').textContent = bpm || '--';
    document.getElementById('normalRange').textContent = `Normal: ${range.min}-${range.max}`;

    // Posisi indikator pada bar gradient
    const percentage = Math.min(Math.max((bpm - 40) / 140 * 100, 0), 100);
    document.getElementById('bpmIndicator').style.marginLeft = `${percentage}%`;

    // status
    let status, statusClass;
    if (!finger || !bpm) {
        status = '👆 Letakkan jari pada sensor';
        statusClass = 'text-gray-400';
    } else if (bpm < range.min) {
        status = '⚠️ Detak Jantung RENDAH (Bradikardia)';
        statusClass = 'text-yellow-400';
    } else if (bpm > range.max) {
        status = '⚠️ Detak Jantung TINGGI (Takikardia)';
        statusClass = 'text-red-400';
    } else {
        status = '✓ Detak Jantung NORMAL';
        statusClass = 'text-green-400';
    }
    const statusEl = document.getElementById('statusText');
    statusEl.textContent = status;
    statusEl.className = `text-center mt-3 text-lg font-medium ${statusClass}`;

    // SpO2
    if (spo2) {
        document.getElementById('spo2Value').textContent = spo2;
        document.getElementById('spo2Bar').style.width = `${spo2}%`;
    }

    // Animasi denyut ikon hati
    if (bpm && finger) {
        document.getElementById('heartIcon').classList.add('pulse-animation');
        setTimeout(() => document.getElementById('heartIcon').classList.remove('pulse-animation'), 500);
        if (soundEnabled) playHeartbeat();
        updateChart(bpm);
        document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString('id-ID');
        // Panggil AI (dengan cooldown)
        generateAIAnalysis(bpm, spo2, age, range);
    }
}

// grafik
function updateChart(bpm) {
    const now = new Date().toLocaleTimeString('id-ID');

    heartRateChart.data.labels.push(now);
    heartRateChart.data.datasets[0].data.push(bpm);

    // Simpan hanya 30 data terakhir
    if (heartRateChart.data.labels.length > 30) {
        heartRateChart.data.labels.shift();
        heartRateChart.data.datasets[0].data.shift();
    }

    heartRateChart.update('none');
}

// analisa gemini Ai
async function generateAIAnalysis(bpm, spo2, age, range) {
    const analysisDiv = document.getElementById('aiAnalysis');

    // Tentukan status & causes lokal (untuk konteks prompt + fallback)
    let statusLabel, causes, colorClass, fallbackTip;
    if (bpm < range.min) {
        statusLabel = 'RENDAH (Bradikardia)';
        causes = ageGroups[range.group].low.causes;
        colorClass = 'text-yellow-400';
        fallbackTip = 'Jika Anda merasa pusing atau lemas, segera konsultasikan ke dokter.';
    } else if (bpm > range.max) {
        statusLabel = 'TINGGI (Takikardia)';
        causes = ageGroups[range.group].high.causes;
        colorClass = 'text-red-400';
        fallbackTip = 'Cobalah relaksasi, tarik napas dalam, dan hindari kafein. Jika berlanjut, konsultasikan ke dokter.';
    } else {
        statusLabel = 'NORMAL';
        causes = [];
        colorClass = 'text-green-400';
        fallbackTip = 'Kondisi kardiovaskular Anda terlihat baik. Pertahankan gaya hidup sehat!';
    }

    // Tampilan rule-based dasar (selalu tampil duluan, instan)
    const baseHtml = `
        <div class="space-y-2">
            <p class="${colorClass} font-medium">Status: ${statusLabel}</p>
            <p class="text-gray-300">Detak jantung Anda <strong>${bpm} BPM</strong> (normal: ${range.min}-${range.max} BPM) untuk usia ${age} tahun.</p>
            ${spo2 !== null ? `<p class="text-gray-300">SpO2: <strong>${spo2}%</strong></p>` : ''}
            ${causes.length ? `<p class="text-gray-400 text-sm">Kemungkinan penyebab: ${causes.join(', ')}</p>` : ''}
            <p class="text-cyan-400 text-sm mt-2">💡 ${fallbackTip}</p>
        </div>`;

    analysisDiv.innerHTML = baseHtml;

    // Cooldown: skip panggilan AI jika terlalu sering
    const now = Date.now();
    if (now - lastAnalysisTime < ANALYSIS_COOLDOWN) return;
    lastAnalysisTime = now;

    // Tambahkan indikator loading AI di bawah base info
    analysisDiv.insertAdjacentHTML('beforeend', `
        <div id="aiLoading" class="flex items-center gap-2 text-gray-400 mt-3 text-sm">
            <div class="w-4 h-4 border-2 border-gray-600 border-t-cyan-400 rounded-full animate-spin"></div>
            <span>AI sedang menganalisis...</span>
        </div>`);

    const prompt = `Kamu adalah asisten kesehatan. Data pasien:
- Usia: ${age} tahun (kelompok: ${range.group})
- Detak jantung: ${bpm} BPM (normal: ${range.min}-${range.max} BPM)
- SpO2: ${spo2 ?? 'tidak ada data'}%
- Kemungkinan penyebab (rule-based): ${causes.join(', ') || '-'}

Berikan analisis singkat (3-4 kalimat) dalam Bahasa Indonesia tentang kondisi ini dan saran yang relevan. Jangan beri diagnosis pasti, ingatkan untuk konsultasi dokter jika perlu.`;

    try {
        const res = await fetch(
            `https://generativelanguage.googleapis.com/v1beta/models/${GEMINI_MODEL}:generateContent?key=${GEMINI_API_KEY}`,
            {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    contents: [{ role: "user", parts: [{ text: prompt }] }]
                })
            }
        );

        if (!res.ok) throw new Error(`HTTP ${res.status}`);

        const data = await res.json();
        const aiText = data.candidates?.[0]?.content?.parts?.[0]?.text || 'Analisis AI tidak tersedia saat ini.';

        const loadingEl = document.getElementById('aiLoading');
        if (loadingEl) {
            loadingEl.outerHTML = `
                <div class="mt-3 pt-3 border-t border-gray-700">
                    <p class="text-purple-400 font-medium text-sm mb-1">🤖 Analisis AI:</p>
                    <p class="text-gray-300 text-sm">${aiText.replace(/\n/g, '<br>')}</p>
                </div>`;
        }
    } catch (err) {
        console.error('Gagal memanggil Gemini:', err);
        const loadingEl = document.getElementById('aiLoading');
        if (loadingEl) {
            loadingEl.outerHTML = `<p class="text-gray-500 text-xs mt-2">Analisis AI tidak tersedia (cek koneksi/API key).</p>`;
        }
    }
}

// Animasi ECG
function animateECG() {
    const svg = document.getElementById('ecgSvg');
    const path = document.getElementById('ecgPath');
    const width = svg.clientWidth;
    const height = svg.clientHeight;
    const centerY = height / 2;

    let d = `M0,${centerY}`;
    const segmentWidth = 60;
    const numSegments = Math.ceil(width / segmentWidth) + 1;

    for (let i = 0; i < numSegments; i++) {
        const x = i * segmentWidth;
        const offset = (Date.now() / 20) % segmentWidth;
        const adjustedX = x - offset;

        d += ` L${adjustedX},${centerY}`;
        d += ` L${adjustedX + 10},${centerY}`;
        d += ` L${adjustedX + 15},${centerY - 5}`;
        d += ` L${adjustedX + 20},${centerY + 10}`;
        d += ` L${adjustedX + 25},${centerY - 40}`;
        d += ` L${adjustedX + 30},${centerY + 20}`;
        d += ` L${adjustedX + 35},${centerY - 10}`;
        d += ` L${adjustedX + 40},${centerY}`;
        d += ` L${adjustedX + segmentWidth},${centerY}`;
    }

    path.setAttribute('d', d);
    if (isRunning) animationFrame = requestAnimationFrame(animateECG);
}

// Suara
function playHeartbeat() {
    const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const oscillator = audioCtx.createOscillator();
    const gainNode = audioCtx.createGain();

    oscillator.connect(gainNode);
    gainNode.connect(audioCtx.destination);

    oscillator.frequency.value = 80;
    oscillator.type = 'sine';

    gainNode.gain.setValueAtTime(0.3, audioCtx.currentTime);
    gainNode.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.1);

    oscillator.start(audioCtx.currentTime);
    oscillator.stop(audioCtx.currentTime + 0.1);
}

function toggleSound() {
    soundEnabled = !soundEnabled;
    const btn = document.getElementById('btnSound');
    const icon = document.getElementById('soundIcon');
    const text = document.getElementById('soundText');

    if (soundEnabled) {
        icon.innerHTML = `
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                d="M15.536 8.464a5 5 0 010 7.072m2.828-9.9a9 9 0 010 12.728M5.586 15H4a1 1 0 01-1-1v-4a1 1 0 011-1h1.586l4.707-4.707C10.923 3.663 12 4.109 12 5v14c0 .891-1.077 1.337-1.707.707L5.586 15z"/>`;
        text.textContent = 'Matikan Suara';
        btn.classList.add('bg-cyan-600', 'border-cyan-500');
        btn.classList.remove('bg-dark-600', 'border-gray-600');
    } else {
        icon.innerHTML = `
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                d="M5.586 15H4a1 1 0 01-1-1v-4a1 1 0 011-1h1.586l4.707-4.707C10.923 3.663 12 4.109 12 5v14c0 .891-1.077 1.337-1.707.707L5.586 15z"/>
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                d="M17 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2"/>`;
        text.textContent = 'Aktifkan Suara';
        btn.classList.remove('bg-cyan-600', 'border-cyan-500');
        btn.classList.add('bg-dark-600', 'border-gray-600');
    }
}

// MQTT Connection
function connectMQTT() {
    if (mqttClient && mqttClient.connected) return;

    setMQTTStatus(false, 'Menghubungkan...');
    mqttClient = mqtt.connect(MQTT_BROKER_URL, MQTT_OPTIONS);

    mqttClient.on('connect', function () {
        console.log('MQTT terhubung');
        setMQTTStatus(true, 'Terhubung');
        mqttClient.subscribe([TOPIC_DATA, TOPIC_STATUS], function (err) {
            if (err) console.error('Gagal subscribe:', err);
            else console.log('Subscribe ke', TOPIC_DATA, TOPIC_STATUS);
        });
        // Jika sensor sedang aktif, kirim START
        if (isRunning) {
            mqttClient.publish(TOPIC_CONTROL, 'START');
        }
    });

    mqttClient.on('reconnect', function () {
        setMQTTStatus(false, 'Menghubungkan kembali...');
    });

    mqttClient.on('close', function () {
        setMQTTStatus(false, 'Terputus');
    });

    mqttClient.on('error', function (err) {
        console.error('MQTT error:', err);
        setMQTTStatus(false, 'Error koneksi');
    });

    mqttClient.on('message', function (topic, message) {
        const payload = message.toString().trim();
        if (topic === TOPIC_DATA) {
            try {
                const data = JSON.parse(payload);
                if (data.finger === false) {
                    // Jari tidak terdeteksi
                    document.getElementById('statusText').textContent = '👆 Letakkan jari pada sensor';
                    document.getElementById('statusText').className = 'text-center mt-3 text-lg font-medium text-gray-400';
                    // Kosongkan BPM/SpO2?
                    document.getElementById('bpmValue').textContent = '--';
                    document.getElementById('spo2Value').textContent = '--';
                    document.getElementById('spo2Bar').style.width = '0%';
                    return;
                }
                if (data.bpm && data.spo2) {
                    lastBPM = data.bpm;
                    lastSpO2 = data.spo2;
                    updateBPMDisplay(lastBPM, lastSpO2, true);
                }
            } catch (e) {
                console.warn('Gagal parse JSON:', payload);
            }
        } else if (topic === TOPIC_STATUS) {
            try {
                const status = JSON.parse(payload);
                if (status.status === 'stopped') {
                    // Sensor berhenti dari ESP32
                    if (isRunning) {
                        // Matikan status running tapi jangan ubah tombol? Atau biarkan user stop manual
                        // Kita sync: jika ESP32 mengirim stop, kita hentikan juga
                        isRunning = false;
                        document.getElementById('btnStart').disabled = false;
                        document.getElementById('btnStop').disabled = true;
                        document.getElementById('sensorStatus').innerHTML = '<span class="w-2 h-2 bg-gray-500 rounded-full"></span> Tidak aktif';
                        document.getElementById('sensorStatus').className = 'text-gray-500 text-sm flex items-center gap-2';
                        if (animationFrame) cancelAnimationFrame(animationFrame);
                    }
                }
            } catch (e) { }
        }
    });
}

function disconnectMQTT() {
    if (mqttClient) {
        mqttClient.end(true);
        mqttClient = null;
    }
    setMQTTStatus(false, 'Tidak terhubung');
}

//MQTT/Connection -
function setMQTTStatus(connected, label) {
    const dot = document.getElementById('connectionDot');
    const status = document.getElementById('connectionStatus');
    const wsBox = document.getElementById('wsStatus');

    if (connected) {
        dot.style.background = '#22c55e';
        status.textContent = label || 'Terhubung';
        status.className = 'text-green-400';
        wsBox.innerHTML = '<span class="w-2 h-2 bg-green-500 rounded-full"></span> Terhubung';
        wsBox.className = 'text-green-400 text-sm flex items-center gap-2';
    } else {
        dot.style.background = '#ef4444';
        status.textContent = label || 'Terputus';
        status.className = 'text-red-400';
        wsBox.innerHTML = '<span class="w-2 h-2 bg-red-500 rounded-full"></span> Terputus';
        wsBox.className = 'text-red-400 text-sm flex items-center gap-2';
    }
}

// Sensor Control
function startSensor() {
    const age = document.getElementById('ageInput').value;
    if (!age || age < 0 || age > 120) {
        alert('Silakan masukkan umur yang valid (0-120 tahun)');
        return;
    }

    isRunning = true;
    document.getElementById('btnStart').disabled = true;
    document.getElementById('btnStop').disabled = false;

    document.getElementById('sensorStatus').innerHTML = '<span class="w-2 h-2 bg-green-500 rounded-full"></span> Aktif';
    document.getElementById('sensorStatus').className = 'text-green-400 text-sm flex items-center gap-2';

    animateECG();

    // Pastikan koneksi MQTT aktif, lalu kirim perintah START ke ESP32
    if (!mqttClient || !mqttClient.connected) {
        connectMQTT();
    }

    if (mqttClient) {
        if (mqttClient.connected) {
            mqttClient.publish(TOPIC_CONTROL, 'START');
        } else {
            // Akan dikirim otomatis saat event 'connect' (lihat connectMQTT)
            mqttClient.once('connect', () => mqttClient.publish(TOPIC_CONTROL, 'START'));
        }
    }
}

function stopSensor() {
    isRunning = false;
    document.getElementById('btnStart').disabled = false;
    document.getElementById('btnStop').disabled = true;

    document.getElementById('sensorStatus').innerHTML = '<span class="w-2 h-2 bg-gray-500 rounded-full"></span> Tidak aktif';
    document.getElementById('sensorStatus').className = 'text-gray-500 text-sm flex items-center gap-2';

    if (animationFrame) cancelAnimationFrame(animationFrame);

    // Kirim perintah STOP ke ESP32 (koneksi MQTT tetap dipertahankan untuk status)
    if (mqttClient && mqttClient.connected) {
        mqttClient.publish(TOPIC_CONTROL, 'STOP');
    }
}

// Reset
function resetAll() {
    stopSensor();

    lastBPM = 0;
    lastSpO2 = 0;

    document.getElementById('ageInput').value = '';
    document.getElementById('bpmValue').textContent = '--';
    document.getElementById('spo2Value').textContent = '--';
    document.getElementById('spo2Bar').style.width = '0%';
    document.getElementById('statusText').textContent = 'Menunggu data...';
    document.getElementById('statusText').className = 'text-center mt-3 text-lg font-medium text-gray-400';
    document.getElementById('bpmIndicator').style.marginLeft = '50%';
    document.getElementById('lastUpdate').textContent = '-';
    document.getElementById('normalRange').textContent = 'Normal: 60-100';

    document.getElementById('aiAnalysis').innerHTML = `
        <div class="flex items-center gap-3 text-gray-400">
            <div class="w-6 h-6 border-2 border-gray-600 border-t-cyan-400 rounded-full animate-spin"></div>
            <span>Menunggu data detak jantung...</span>
        </div>`;

    heartRateChart.data.labels = [];
    heartRateChart.data.datasets[0].data = [];
    heartRateChart.update();

    lastAnalysisTime = 0; // reset cooldown supaya analisis AI bisa langsung jalan lagi
}

//Modal
function showDetail(group) {
    const data = ageGroups[group];
    const modal = document.getElementById('detailModal');
    const title = document.getElementById('modalTitle');
    const content = document.getElementById('modalContent');

    title.textContent = data.title;
    content.innerHTML = `
        <div class="bg-dark-800 rounded-xl p-4">
            <p class="text-green-400 font-medium mb-2">✓ Normal: ${data.normal}</p>
        </div>
        <div class="bg-dark-800 rounded-xl p-4">
            <p class="text-yellow-400 font-medium mb-2">⚠️ Rendah: < ${data.low.threshold} BPM</p>
            <p class="text-gray-400 text-sm mb-2">Kemungkinan penyebab:</p>
            <ul class="list-disc list-inside text-gray-300 text-sm space-y-1">
                ${data.low.causes.map(c => `<li>${c}</li>`).join('')}
            </ul>
        </div>
        <div class="bg-dark-800 rounded-xl p-4">
            <p class="text-red-400 font-medium mb-2">⚠️ Tinggi: > ${data.high.threshold} BPM</p>
            <p class="text-gray-400 text-sm mb-2">Kemungkinan penyebab:</p>
            <ul class="list-disc list-inside text-gray-300 text-sm space-y-1">
                ${data.high.causes.map(c => `<li>${c}</li>`).join('')}
            </ul>
        </div>`;

    modal.classList.remove('hidden');
}

function closeModal() {
    document.getElementById('detailModal').classList.add('hidden');
}

// init
document.addEventListener('DOMContentLoaded', function () {
    initChart();

    // Hubungkan ke MQTT broker sejak halaman dimuat agar status koneksi
    // langsung terlihat, terlepas dari status monitoring (start/stop).
    connectMQTT();

    document.getElementById('ageInput').addEventListener('keypress', function (e) {
        if (e.key === 'Enter') startSensor();
    });

    document.getElementById('detailModal').addEventListener('click', function (e) {
        if (e.target === this) closeModal();
    });
});
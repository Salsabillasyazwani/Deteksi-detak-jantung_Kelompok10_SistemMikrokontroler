#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "MAX30105.h"

MAX30105 sensor;

bool sensorActive = true;

// Threeshold abnormal detak jantung
#define BPM_LOW 50
#define BPM_HIGH 150

// Buffer untuk menyimpan data IR dan SpO2
#define BUFFER_SIZE 50

long irBuffer[BUFFER_SIZE];
long irBufferSpO2[BUFFER_SIZE];
int bufferIndex = 0;
bool bufferReady = false;

// INISIALISASI WIFI
const char *ssid = "Los";
const char *password = "123456780";
const char *mqtt_server = "192.168.137.1"; // Ganti dengan alamat broker/ip

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastBeatTime = 0;
float bpm = 0;
#define RATE_SIZE 4
float rates[RATE_SIZE];
byte rateSpot = 0;
bool bpmReady = false;
float bpmAvg = 0;

// ===== SMOOTHING =====
long irBuffer[5];
int irIndex = 0;

long smoothIR(long value)
{
  irBuffer[irIndex++] = value;
  irIndex %= 5;

  long sum = 0;
  for (int i = 0; i < 5; i++)
    sum += irBuffer[i];

  return sum / 5;
}

// ===== PEAK DETECTION =====
bool detectPeak(long irValue)
{
  static long prev = 0;
  static bool rising = false;
  static long threshold = 0;
  static unsigned long lastBeat = 0;
  static int validPeak = 0;

  threshold = (threshold * 0.9) + (irValue * 0.1);

  if (irValue > threshold && irValue > prev && !rising)
  {
    rising = true;
  }

  if (rising && irValue < prev)
  {
    rising = false;

    long peakHeight = prev - threshold;

    // filter lebih ketat
    if (millis() - lastBeat > 600 && peakHeight > 20)
    {
      validPeak++;

      if (validPeak >= 1)
      {
        lastBeat = millis();
        validPeak = 0;
        return true;
      }
    }
    else
    {
      validPeak = 0;
    }
  }

  prev = irValue;
  return false;
}

// ==== MQTT CONNECT ====
void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Menghubungkan ke MQTT...");

    if (client.connect("ESP32Client"))
    {
      Serial.println("Terhubung");
      client.subscribe("sensor/control");
    }
    else
    {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 2 detik");
      delay(2000);
    }
  }
}

// === MQTT DEBUG ===
void debugMQTT()
{
  Serial.print("Status koneksi MQTT: ");
  Serial.println(client.connected() ? "Terhubung" : "Tidak terhubung");

  Serial.print("Status server MQTT: ");
  Serial.println(mqtt_server);
  Serial.println("================================");
}

// === WIFI DEBUG ===
void debugWiFi()
{
  Serial.print("Status koneksi WiFi: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "Terhubung" : "Tidak terhubung");

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Alamat MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.print("Kekuatan sinyal WiFi (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  Serial.println("================================");
}

// CALLBACK FUNCTION FOR MQTT
void callback(char *topic, byte *payload, unsigned int length)
{
  String msg = "";

  for (int i = 0; i < length; i++)
  {
    msg += (char)payload[i];
  }

  Serial.print("Pesan masuk [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (String(topic) == "sensor/control")
  {
    if (msg == "START")
    {
      sensorActive = true;
      Serial.println("Sensor AKTIF");
    }
    else if (msg == "STOP")
    {
      sensorActive = false;
      Serial.println("Sensor NONAKTIF");
    }
  }
}

// CALCULATE SPO2
float calculateSpO2
{
  long redAC = 0;
  , irAC = 0;
  long redDC = 0, irDC = 0;

  // hitung rata-rata DC
  for (int i = 0; i < BUFFER_SIZE; i++)
  {
    redDC += redBuffer[i];
    irDC += irBuffer[i];
  }

  redDC /= BUFFER_SIZE;
  irDC /= BUFFER_SIZE;

  // hitung rata-rata AC
  for (int i = 0; i < BUFFER_SIZE; i++)
  {
    redAC += abs(redBuffer[i] - redDC);
    irAC += abs(irBuffer[i] - irDC);
  }

  redAC /= BUFFER_SIZE;
  irAC /= BUFFER_SIZE;

  if (irAC == 0 || redDC == 0 || irDC == 0)
    return 0; // menghindari pembagian dengan nol

  float ratio = ((float)redAC / redDC) / ((float)irAC / irDC);

  float spo2 = 110 - (25 * ratio); // Rumus perkiraan SpO2

  // Pastikan nilai SpO2 berada dalam rentang 0-100%
  if (spo2 > 100)
    spo2 = 100;
  else if (spo2 < 0)
    spo2 = 0;
  return spo2;
}

// RTOS TASKS
// === TASK 1 SENSOR ===
void taskSensor(void *pvParameters)
{
  while (1)
  {
    // STOP TOTAL
    if (!sensorActive)
    {
      client.publish("heart/status", "{\"status\":\"stopped\"}");
      vTaskDelay(500 / portTICK_PERIOD_MS);
      continue;
    }

    long red = sensor.getRed();
    long ir = sensor.getIR();

    redBuffer[bufferIndex] = red;
    irBuffer[bufferIndex] = ir;

    bufferIndex++;
    if (bufferIndex >= BUFFER_SIZE)
    {
      bufferIndex = 0;
      bufferReady = true;
    }

    // tidak ada jari
    if (ir < 20000)
    {
      client.publish("heart/data", "{\"finger\":false}");
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    long filtered = smoothIR(ir);
    bool beat = detectPeak(filtered);

    if (beat)
    {
      float bpmValue = 60000.0 / (millis() - lastBeatTime);
      lastBeatTime = millis();

      // ===== DETEKSI ABNORMAL =====
      String status = "normal";

      if (bpmValue < BPM_LOW)
        status = "low";
      else if (bpmValue > BPM_HIGH)
        status = "high";

      // ===== JSON FULL =====
      char payload[128];
      sprintf(payload,
              "{\"bpm\":%.2f,\"ir\":%ld,\"status\":\"%s\",\"finger\":true}",
              bpmValue, ir, status.c_str());

      client.publish("heart/data", payload);

      // ALERT
      if (status != "normal")
      {
        client.publish("heart/alert", payload);
      }

      Serial.println(payload);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// === TASK 2 MQTT ===
void taskMQTT(void *pvParameters)
{
  while (1)
  {
    if (!client.connected())
    {
      reconnect();
    }
    client.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(18, 19);
  Serial.println();
  Serial.println("Inisialisasi sensor dan MQTT...");

  if (!sensor.begin(Wire))
  {
    Serial.println("Sensor error");
    while (1)
      delay(1000);
  }

  sensor.setup(60, 4, 2, 100, 411, 4096);
  sensor.setPulseAmplitudeRed(0x0F);
  sensor.setPulseAmplitudeIR(0x2F);

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi terhubung");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  client.setCallback(callback);
  client.setServer(mqtt_server, 1883);
  reconnect();

  xTaskCreatePinnedToCore(taskSensor, "TaskSensor", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskMQTT, "TaskMQTT", 4096, NULL, 1, NULL, 0);

  Serial.println("Setup selesai. MQTT dan sensor berjalan.");
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }

  client.loop();
  delay(10);
}
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

#define BUFFER_SIZE 100

MAX30105 sensor;

// INISIALISASI WIFI
const char *ssid = "Los";
const char *password = "123456780";
const char *mqtt_server = "192.168.137.1"; // Ganti dengan alamat broker/ip

// MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// CONTROL
bool sensorActive = true;

// BUFFER
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int bufferIndex = 0;
bool bufferReady = false;

// HASIL
int32_t spo2;
int8_t spo2_valid;
int32_t heartRate;
int8_t hr_valid;

// CALLBACK FUNCTION UNTUK MQTT
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

// ===== SMOOTHING =====
long irBufferSmooth[5];
int irIndex = 0;

long smoothIR(long value)
{
  irBufferSmooth[irIndex++] = value;
  irIndex %= 5;

  long sum = 0;
  for (int i = 0; i < 5; i++)
    sum += irBufferSmooth[i];

  return sum / 5;
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

// RTOS TASKS
// === TASK 1 SENSOR ===
void taskSensor(void *pvParameters)
{
  static int loopCounter = 0;
  loopCounter++;
  if (loopCounter < 25)
  {
    vTaskDelay(40 / portTICK_PERIOD_MS);
    continue;
  }
  loopCounter = 0;

  static int32_t lastSpo2 = -1;
  static int32_t lastHeartRate = -1;

  while (1)
  {
    // STOP TOTAL
    if (!sensorActive)
    {
      client.publish("heart/status", "{\"status\":\"stopped\"}");
      vTaskDelay(500 / portTICK_PERIOD_MS);
      continue;
    }

    // TUNGGU DATA READY
    if (!sensor.available())
    {
      vTaskDelay(5 / portTICK_PERIOD_MS);
      continue;
    }

    uint32_t ir = sensor.getFIFOIR();
    uint32_t red = sensor.getFIFORed();
    sensor.nextSample();

    // DETEKSI JARI
    if (ir < 50000)
    {
      client.publish("heart/data", "{\"finger\":false}");
      bufferIndex = 0;
      bufferReady = false;
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    irBuffer[bufferIndex] = ir;
    redBuffer[bufferIndex] = red;

    bufferIndex++;

    if (bufferIndex >= BUFFER_SIZE)
    {
      bufferIndex = 0;
      bufferReady = true;
    }

    if (!bufferReady)
    {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    maxim_heart_rate_and_oxygen_saturation(
        irBuffer,
        BUFFER_SIZE,
        redBuffer,
        &spo2,
        &spo2_valid,
        &heartRate,
        &hr_valid);

    // BUANG DATA YANG TIDAK VALID
    if (spo2 < 70 || spo2 > 100 || !spo2_valid)
      spo2 = lastSpo2;
    else
      lastSpo2 = spo2;

    if (heartRate < 40 || heartRate > 180 || !hr_valid)
      heartRate = lastHeartRate;
    else
      lastHeartRate = heartRate;

    char payload[150];

    sprintf(payload,
            "{\"bpm\":%d,\"spo2\":%d,\"valid_spo2\":%d,\"valid_hr\":%d,\"finger\":true}",
            heartRate,
            spo2,
            spo2_valid,
            hr_valid);

    if (client.connected())
      client.publish("heart/data", payload);

    Serial.println(payload);
    vTaskDelay(40 / portTICK_PERIOD_MS);
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

  Serial.println("Inisialisasi sensor dan MQTT...");

  if (!sensor.begin(Wire))
  {
    Serial.println("Sensor MAX30102 tidak ditemukan. Periksa koneksi.");
    while (1)
      ;
  }

  sensor.setup(
      60,   // brightness
      4,    // sample avg
      2,    // LED mode
      100,  // sample rate
      411,  // pulse width
      16384 // ADC range
  );        // Konfigurasi sensor
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
  debugWiFi();
  debugMQTT();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  reconnect();

  xTaskCreatePinnedToCore(taskSensor, "TaskSensor", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskMQTT, "TaskMQTT", 4096, NULL, 1, NULL, 0);

  Serial.println("Setup selesai. MQTT dan sensor berjalan.");
}

void loop() {}
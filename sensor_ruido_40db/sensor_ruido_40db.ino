#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "secrets.h"  // WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS (copiar desde secrets.h.example)

// ─── Identidad del sensor ───────────────────────────────────
const char* SENSOR_ID = "UE-MAI-0001";
const float LAT = -33.5180;   // Villa El Abrazo, Maipú
const float LNG = -70.7580;

// ─── MQTT HiveMQ Cloud ──────────────────────────────────────
const char* MQTT_BROKER = "62eca883862e44e2b92254029fa382a3.s1.eu.hivemq.cloud";
const int   MQTT_PORT   = 8883;
const char* MQTT_TOPIC  = "40db/sensors/UE-MAI-0001/readings";

// ─── Sensor KY-038 (micrófono analógico) ────────────────────
const int MIC_PIN = 34;                       // ADC1_CH6, input-only
const unsigned long SAMPLE_WINDOW_MS = 50;    // ventana para medir amplitud
const int AMP_MIN = 0;                        // amplitud → 30 dB
const int AMP_MAX = 1000;                     // amplitud → 100 dB
const float DB_MIN = 30.0;
const float DB_MAX = 100.0;

WiFiClientSecure wifiClient; 
PubSubClient mqtt(wifiClient);

// ─── Mapeo float ────────────────────────────────────────────
float floatMap(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ─── Medición de amplitud (pico-pico) ───────────────────────
int measureAmplitude() {
  int minVal = 4095;
  int maxVal = 0;
  unsigned long start = millis();
  while (millis() - start < SAMPLE_WINDOW_MS) {
    int v = analogRead(MIC_PIN);
    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;
  }
  return maxVal - minVal;
}

// ─── Conexiones ─────────────────────────────────────────────
void connectWifi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.print("\nWiFi OK. IP: ");
  Serial.println(WiFi.localIP());
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.print("Conectando a HiveMQ Cloud... ");
    String clientId = String("40db-") + SENSOR_ID;
    // ← ahora el connect lleva 3 argumentos: id, usuario, password
    if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("OK");
    } else {
      Serial.print("falló rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// ─── Publicación del reporte ────────────────────────────────
void publishReading(float db) {
  StaticJsonDocument<256> doc;
  doc["sensor_id"] = SENSOR_ID;
  doc["db"]        = db;
  doc["timestamp"] = millis();
  JsonObject geo   = doc.createNestedObject("geo");
  geo["lat"]       = LAT;
  geo["lng"]       = LNG;

  char buf[256];
  size_t n = serializeJson(doc, buf);
  mqtt.publish(MQTT_TOPIC, buf, n);

  Serial.print("PUB → ");
  Serial.println(buf);
}

void setup() {
  Serial.begin(9600);
  analogReadResolution(12);

  connectWifi();

  wifiClient.setInsecure();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  connectMqtt();
}

void loop() {
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  int amplitude = measureAmplitude();
  float db = floatMap(amplitude, AMP_MIN, AMP_MAX, DB_MIN, DB_MAX);
  if (db < DB_MIN) db = DB_MIN;
  if (db > DB_MAX) db = DB_MAX;

  Serial.print("amp=");
  Serial.print(amplitude);
  Serial.print(" → ");
  Serial.print(db);
  Serial.println(" dB");

  publishReading(db);

  delay(5000);   // cada 5 segundos
}

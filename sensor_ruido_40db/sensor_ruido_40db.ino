  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include <PubSubClient.h>
  #include <ArduinoJson.h>
  #include <time.h>

  // ─── Identidad del sensor ───────────────────────────────────
  // UUID provisionado en public.sensor (Supabase). NO cambiar — coincide con el
  // FK que usa `lectura.sensor_id` en backend.
  const char* SENSOR_ID = "d7d62fcc-24a1-42f9-b0e7-8c5952659852";

  // ─── WiFi (tu red) ──────────────────────────────────────────
  const char* WIFI_SSID = "nombre de la red";
  const char* WIFI_PASS = "contraseña de la red";

  // ─── MQTT HiveMQ Cloud ──────────────────────────────────────
  const char* MQTT_BROKER = "...s1.eu.hivemq.cloud";
  const int   MQTT_PORT   = 8883;
  const char* MQTT_USER   = "40db-sensor";
  const char* MQTT_PASS   = "...";

  String MQTT_TOPIC;

  WiFiClientSecure wifiClient;
  PubSubClient mqtt(wifiClient);

  // ─── Sensor KY-038 (micrófono analógico) ────────────────────
  const int MIC_PIN = 34;                       // ADC1_CH6, input-only
  const unsigned long SAMPLE_WINDOW_MS = 50;    // ventana para medir amplitud
  const int AMP_MIN = 0;                        // amplitud → 30 dB
  const int AMP_MAX = 1000;                     // amplitud → 100 dB
  const float DB_MIN = 30.0;
  const float DB_MAX = 100.0;

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

  // ─── Lectura real del micrófono → dB ────────────────────────
  float readMicDb() {
    int amplitude = measureAmplitude();
    float db = floatMap(amplitude, AMP_MIN, AMP_MAX, DB_MIN, DB_MAX);
    if (db < DB_MIN) db = DB_MIN;
    if (db > DB_MAX) db = DB_MAX;

    Serial.print("amp=");
    Serial.print(amplitude);
    Serial.print(" → ");
    Serial.print(db);
    Serial.println(" dB");

    return db;
  }

  // ─── WiFi ───────────────────────────────────────────────────
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

  // ─── NTP ────────────────────────────────────────────────────
  void syncTime() {
    Serial.print("Sincronizando NTP");
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    time_t now = 0;
    while (now < 1700000000) {
      delay(500);
      Serial.print(".");
      time(&now);
    }
    Serial.printf("\nNTP OK. Epoch=%ld\n", now);
  }

  // ─── MQTT ───────────────────────────────────────────────────
  void connectMqtt() {
    while (!mqtt.connected()) {
      Serial.print("Conectando a HiveMQ Cloud... ");
      String clientId = String("40db-") + SENSOR_ID;
      if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
        Serial.println("OK");
      } else {
        Serial.print("falló rc=");
        Serial.println(mqtt.state());
        delay(2000);
      }
    }
  }

  // ─── Publicación ────────────────────────────────────────────
  void publishReading(float db) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    StaticJsonDocument<128> doc;
    doc["nivel_db"]           = db;
    doc["timestamp_medicion"] = ts;

    char buf[128];
    size_t n = serializeJson(doc, buf);
    mqtt.publish(MQTT_TOPIC.c_str(), buf, false);

    Serial.print("PUB → ");
    Serial.println(buf);
  }

  void setup() {
    Serial.begin(9600);
    analogReadResolution(12);
    MQTT_TOPIC = String("40db/sensores/") + SENSOR_ID + "/lectura";

    connectWifi();
    syncTime();

    wifiClient.setInsecure();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    connectMqtt();
  }

  void loop() {
    if (!mqtt.connected()) connectMqtt();
    mqtt.loop();

    float db = readMicDb();
    publishReading(db);

    delay(5000);   // 5s
  }

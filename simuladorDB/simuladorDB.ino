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
  // ⚠️ COMPLETAR antes de flashear
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

  // ═══ SIM: parámetros del random walk ════════════════════════
  // Estado persistente entre lecturas (modulado por random walk).
  static float current_db = 55.0;          // baseline urbano de día
  const float BASELINE_DB     = 55.0;       // valor al que el walk tiende a volver
  const float DRIFT_MAX_DB    = 2.0;        // variación máxima por lectura
  const float PULL_STRENGTH   = 0.10;       // 10% pull hacia baseline cada lectura
  const float MIN_DB          = 38.0;       // ambiente muy silencioso
  const float MAX_DB          = 92.0;       // ruido fuerte (claxon, obra cerca)
  const int   EVENT_HIGH_PCT  = 5;          // 5% chance de evento alto por lectura
  const int   EVENT_LOW_PCT   = 5;          // 5% chance de evento silencioso

  // ═══ SIM: generador de nivel_db pseudoaleatorio ═════════════
  float generateFakeDb() {
    int roll = random(0, 100);

    if (roll < EVENT_HIGH_PCT) {
      // Evento alto: claxon/obra. 75-90 dB, salto puntual.
      current_db = random(75, 91);
    } else if (roll < EVENT_HIGH_PCT + EVENT_LOW_PCT) {
      // Evento silencioso: noche tranquila. 40-50 dB.
      current_db = random(40, 51);
    } else {
      // Random walk suave: drift aleatorio + pull hacia baseline.
      float drift = (random(-100, 101) / 100.0) * DRIFT_MAX_DB;
      float pull = (BASELINE_DB - current_db) * PULL_STRENGTH;
      current_db += drift + pull;
    }

    if (current_db < MIN_DB) current_db = MIN_DB;
    if (current_db > MAX_DB) current_db = MAX_DB;
    return current_db;
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
    MQTT_TOPIC = String("40db/sensores/") + SENSOR_ID + "/lectura";

    // SIM: semilla del PRNG con algo cambiante para que cada arranque sea distinto
    randomSeed(micros());

    connectWifi();
    syncTime();

    wifiClient.setInsecure();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    connectMqtt();
  }

  void loop() {
    if (!mqtt.connected()) connectMqtt();
    mqtt.loop();

    // ═══ SIM: en vez de leer el micrófono físico, generar valor pseudoaleatorio
    float db = generateFakeDb();
/// ═══════════════════════════════════════════════════════════════════════

    Serial.print("SIM db=");
    Serial.print(db);
    Serial.println(" dB");

    publishReading(db);

    delay(5000);   // 5s
  }

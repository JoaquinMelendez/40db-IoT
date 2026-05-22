# 40db-IoT

Sensor de ruido ambiental basado en ESP32 que mide niveles de presión sonora (en dB) y los publica vía MQTT/TLS a HiveMQ Cloud.

## Descripción

El firmware se ejecuta en un ESP32 y realiza el siguiente ciclo:

1. Lee la amplitud pico-pico del micrófono analógico (KY-038) sobre una ventana de 50 ms.
2. Mapea la amplitud al rango 30–100 dB.
3. Publica una lectura cada 5 segundos al broker MQTT (HiveMQ Cloud, puerto 8883 / TLS) con el siguiente formato JSON:

```json
{
  "sensor_id": "UE-MAI-0001",
  "db": 47.3,
  "timestamp": 123456,
  "geo": { "lat": -33.5180, "lng": -70.7580 }
}
```

El topic por defecto es `40db/sensors/UE-MAI-0001/readings`.

## Hardware

- 1 × ESP32
- 1 × Sensor de sonido KY-038 (micrófono analógico)

> Las conexiones físicas (pines exactos, voltaje de alimentación del KY-038, etc.) se documentarán en una siguiente iteración.

## Dependencias

El sketch usa las siguientes librerías:

- `WiFi` (incluida con el core de ESP32)
- `WiFiClientSecure` (incluida con el core de ESP32)
- `PubSubClient` (Nick O'Leary)
- `ArduinoJson` (Benoit Blanchon)

Instálalas desde el **Library Manager** del Arduino IDE.

## Setup HiveMQ + `secrets.h`

Las credenciales (WiFi y HiveMQ) **no se commitean al repositorio**. Cada desarrollador debe crear su propio `secrets.h` localmente.

### 1. Crear cluster en HiveMQ Cloud

1. Crear una cuenta gratuita en [HiveMQ Cloud](https://www.hivemq.com/cloud/).
2. Crear un cluster (el plan "Free" alcanza para pruebas).
3. En **Access Management → Credentials**, crear un usuario con permisos de publicación y anotar usuario y contraseña.
4. Anotar también la URL del broker (algo como `xxxxxxxx.s1.eu.hivemq.cloud`) y el puerto TLS (`8883`).

### 2. Configurar `secrets.h`

Dentro de la carpeta `sensor_ruido_40db/`, copia el archivo de ejemplo:

```bash
cp sensor_ruido_40db/secrets.h.example sensor_ruido_40db/secrets.h
```

Y edita `secrets.h` con tus credenciales reales:

```c
#define WIFI_SSID "tu-red-wifi"
#define WIFI_PASS "tu-password-wifi"
#define MQTT_USER "tu-usuario-hivemq"
#define MQTT_PASS "tu-password-hivemq"
```

El archivo `secrets.h` está incluido en `.gitignore` y nunca debe subirse al repositorio.

### 3. Ajustar el broker (opcional)

Si tu cluster de HiveMQ no es el mismo configurado en el sketch, edita `MQTT_BROKER` directamente en `sensor_ruido_40db.ino`.

## Compilar y subir

1. Abrir `sensor_ruido_40db/sensor_ruido_40db.ino` en Arduino IDE.
2. Seleccionar la placa ESP32 correspondiente y el puerto.
3. Compilar y flashear.
4. Abrir el monitor serie a `9600 baud` para verificar conexión WiFi, conexión MQTT y publicaciones.

## Flujo de ramas

- `main` → rama protegida, solo recibe merges desde `dev`.
- `dev` → rama de integración.
- `feature/*` → ramas de trabajo, parten desde `dev` y vuelven a `dev` vía Pull Request.

Nada se mergea directo a `main`.

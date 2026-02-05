/************************************************************
 * ESP32 WAV Receiver + Playback
 * Async HTTP upload + MQTT status
 * DAC → PAM8403 → Speaker
 ************************************************************/

#define DEVICE_NAME "ESP32_A"

/* ====== WIFI ====== */
#include <WiFi.h>
const char* ssid = "Mi Router 4A";
const char* password = "Wasd$2123";

/* ====== MQTT ====== */
#include <PubSubClient.h>
const char* mqtt_server = "192.168.214.81";
const int mqtt_port = 1883;

/* ====== WEB ====== */
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/* ====== FS ====== */
#include <LittleFS.h>

/* ====== AUDIO ====== */
#define DAC_PIN     25
#define BUTTON_PIN  33

WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);

/* ====== AUDIO STATE ====== */
File wavFile;
bool playing = false;

/* ====== WIFI ====== */
void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("Wifi Connected");
}

/* ====== MQTT ====== */
void connectMQTT() {
  while (!mqttClient.connected()) {

    if (mqttClient.connect(
          DEVICE_NAME,
          NULL,
          NULL,
          "esp/status/" DEVICE_NAME,
          0,
          true,
          "offline"
        )) {

      mqttClient.publish(
        "esp/status/" DEVICE_NAME,
        "online",
        true
      );

      mqttClient.publish(
        "esp/ip/" DEVICE_NAME,
        WiFi.localIP().toString().c_str(),
        true
      );

    } else {
      delay(2000);
    }
  }
}

/* ====== WAV PLAYBACK (RAW PCM) ====== */
void playWavChunk() {
  if (!playing || !wavFile.available()) {
    dacWrite(DAC_PIN, 0);
    playing = false;
    wavFile.close();
    return;
  }

  uint8_t sample = wavFile.read();
  dacWrite(DAC_PIN, sample);   // 8-bit unsigned PCM
  delayMicroseconds(45);       // ~22kHz
}

/* ====== SETUP ====== */
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(DAC_PIN, OUTPUT);

  LittleFS.begin(true);
  connectWiFi();

  mqttClient.setServer(mqtt_server, mqtt_port);
  connectMQTT();

  /* --- UPLOAD --- */
  server.on(
    "/upload", HTTP_PUT,
    [](AsyncWebServerRequest *request) {
      request->send(200, "application/json",
        "{\"device\":\"" DEVICE_NAME "\",\"status\":\"ok\"}");
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data,
       size_t len, size_t index, size_t total) {

      static File f;

      if (index == 0) {
        f = LittleFS.open("/audio.wav", "w");
      }

      f.write(data, len);

      if (index + len == total) {
        f.close();
        mqttClient.publish(
          "esp/upload_ack",
          String("{\"device\":\"" DEVICE_NAME "\",\"size\":" + String(total) + "}").c_str()
        );
      }
    }
  );

  server.begin();
}

/* ====== LOOP ====== */
void loop() {
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  // Button press → play WAV
  if (digitalRead(BUTTON_PIN) == LOW && !playing) {
    if (LittleFS.exists("/audio.wav")) {
      wavFile = LittleFS.open("/audio.wav", "r");
      wavFile.seek(44);  // skip WAV header
      playing = true;
    }
  }

  if (playing) {
    playWavChunk();
  }
}

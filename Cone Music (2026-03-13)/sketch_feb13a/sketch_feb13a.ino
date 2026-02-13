#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include "driver/i2s.h"

#define BUTTON_PIN 4

#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

// ================= WIFI =================
const char* ssid = "Digital_Spectra_2.4G";
const char* password = "digital@9711";

// CHANGE THIS TO YOUR PC IP
const char* serverIP = "192.168.214.95";

WebServer server(80);

String deviceID;
bool playing = false;

// ================= I2S SETUP =================
void setupI2S() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
}

// ================= REGISTER TO PYTHON SERVER =================
void registerToServer() {
  HTTPClient http;
  String url = String("http://") + serverIP + ":5000/register?id=" + deviceID;

  Serial.println("Registering to server...");
  http.begin(url);
  int code = http.GET();

  Serial.print("Server response: ");
  Serial.println(code);

  http.end();
}

// ================= UPLOAD HANDLER =================
void setupUploadEndpoint() {

  server.on(
    "/upload",
    HTTP_POST,
    []() {
      server.send(200, "text/plain", "Upload OK");
    },
    []() {
      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        Serial.println("Upload Start");
        SPIFFS.remove("/audio.wav");
      }
      else if (upload.status == UPLOAD_FILE_WRITE) {
        File file = SPIFFS.open("/audio.wav", FILE_APPEND);
        if (file) {
          file.write(upload.buf, upload.currentSize);
          file.close();
        }
      }
      else if (upload.status == UPLOAD_FILE_END) {
        Serial.println("Upload Complete");
      }
    }
  );
}

// ================= PLAY AUDIO =================
void playAudio() {

  if (!SPIFFS.exists("/audio.wav")) {
    Serial.println("No audio file found.");
    return;
  }

  File file = SPIFFS.open("/audio.wav");
  if (!file) {
    Serial.println("Failed to open audio.");
    return;
  }

  Serial.println("Playing...");

  file.seek(44);  // Skip WAV header

  uint8_t buffer[512];
  size_t bytesRead;

  playing = true;

  while (playing && (bytesRead = file.read(buffer, sizeof(buffer))) > 0) {

    size_t bytesWritten;
    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);

    if (digitalRead(BUTTON_PIN) == HIGH) {
      playing = false;
    }
  }

  file.close();
  Serial.println("Stopped.");
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("ESP IP: ");
  Serial.println(WiFi.localIP());

  setupI2S();

  deviceID = String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("Device ID: ");
  Serial.println(deviceID);

  registerToServer();

  setupUploadEndpoint();

  server.begin();
  Serial.println("HTTP Server Started");
}

// ================= LOOP =================
void loop() {

  server.handleClient();

  if (digitalRead(BUTTON_PIN) == LOW && !playing) {
    playAudio();
  }
}

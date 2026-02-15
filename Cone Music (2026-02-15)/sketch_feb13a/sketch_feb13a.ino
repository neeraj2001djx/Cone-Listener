#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include "driver/i2s.h"

#define BUTTON_PIN 4

#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

#define SD_CS 5

// ================= WIFI =================
const char* ssid = "Mi Router 4A";
const char* password = "Wasd$2123";
const char* serverIP = "192.168.31.164";

WebServer server(80);

String deviceID;

bool playing = false;
bool uploading = false;
bool lastButtonState = HIGH;

File uploadFile;

float gain = 1;

// ================= I2S SETUP =================
void setupI2S() {

  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = true,
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

// ================= REGISTER =================
void registerToServer() {
  HTTPClient http;
  String url = String("http://") + serverIP + ":5000/register?id=" + deviceID;

  http.begin(url);
  http.GET();
  http.end();
}

// ================= UPLOAD HANDLER =================
void setupUploadEndpoint() {

  server.on("/upload", HTTP_POST,
    []() {
      server.send(200, "text/plain", "OK");
    },
    []() {

      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {

        Serial.println("Upload Start");
        uploading = true;

        if (SD.exists("/audio.wav")) {
          SD.remove("/audio.wav");
        }

        uploadFile = SD.open("/audio.wav", FILE_WRITE);

      } 
      else if (upload.status == UPLOAD_FILE_WRITE) {

        if (uploadFile) {
          uploadFile.write(upload.buf, upload.currentSize);
          uploadFile.flush();   // prevent corruption
        }

      } 
      else if (upload.status == UPLOAD_FILE_END) {

        if (uploadFile) {
          uploadFile.close();
        }

        File f = SD.open("/audio.wav");
        Serial.print("Upload Complete. Size: ");
        Serial.println(f.size());
        f.close();

        uploading = false;
      }
    }
  );
}

// ================= PLAY AUDIO =================
void playAudio() {

  if (uploading) return;

  if (!SD.exists("/audio.wav")) {
    Serial.println("No audio file.");
    return;
  }

  File file = SD.open("/audio.wav");
  if (!file) {
    Serial.println("Failed to open audio.");
    return;
  }

  Serial.println("Playing...");
  file.seek(44);  // Skip WAV header

  uint8_t buffer[1024];
  size_t bytesRead;

  playing = true;

  while (playing && (bytesRead = file.read(buffer, sizeof(buffer))) > 0) {

    size_t bytesWritten;

    int16_t* samples = (int16_t*)buffer;
    int sampleCount = bytesRead / 2;

    for (int i = 0; i < sampleCount; i++) {
      int32_t amplified = samples[i] * gain;

      if (amplified > 32767) amplified = 32767;
      if (amplified < -32768) amplified = -32768;

      samples[i] = amplified;
    }

    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);

    // Stop on button release
    if (digitalRead(BUTTON_PIN) == HIGH) {
      playing = false;
    }
  }

  file.close();
  playing = false;

  Serial.println("Stopped.");
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // SD Init (stable speed)
  if (!SD.begin(SD_CS, SPI, 10000000)) {
    Serial.println("SD Card Mount Failed");
    while (1);
  }

  Serial.println("SD Card Ready");

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  setupI2S();

  deviceID = String((uint32_t)ESP.getEfuseMac(), HEX);
  registerToServer();

  setupUploadEndpoint();
  server.begin();

  Serial.println("System Ready");
}

// ================= LOOP =================
void loop() {

  server.handleClient();

  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Detect press
  if (!uploading &&
      lastButtonState == HIGH &&
      currentButtonState == LOW &&
      !playing) {

    playAudio();
  }

  lastButtonState = currentButtonState;
}

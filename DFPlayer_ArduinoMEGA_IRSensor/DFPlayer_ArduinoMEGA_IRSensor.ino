#include <DFRobotDFPlayerMini.h>

DFRobotDFPlayerMini dfplayer;

const int buttonPin = 2;
bool isPlaying = false;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(9600);      // USB debug (optional)
  Serial1.begin(9600);     // DFPlayer

  if (!dfplayer.begin(Serial1)) {
    Serial.println("DFPlayer not detected");
    while (true);
  }

  dfplayer.volume(30); // 0–30
}

void loop() {
  bool pressed = (digitalRead(buttonPin) == LOW);

  if (pressed && !isPlaying) {
    dfplayer.play(1);   // /mp3/001.mp3
    isPlaying = true;
  }

  if (!pressed && isPlaying) {
    dfplayer.pause();  // stop immediately
    isPlaying = false;
  }
}

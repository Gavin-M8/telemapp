
// this shows up as undefined because it's not being compiled by something that works with Arduino code; use PlatformIO or the Arduino IDE
#include <Wire.h>

// === CONFIGURATION ===
#define BAUD_RATE 19200
#define SAMPLE_INTERVAL_MS 20   // 50 Hz

// === STATE ===
unsigned long lastSample = 0;

// === PLACEHOLDER SENSOR READ FUNCTIONS ===
// Replace these with your accelerometer library calls
int readAccelX() { return analogRead(A0); }
int readAccelY() { return analogRead(A1); }
int readAccelZ() { return analogRead(A2); }

void setup() {
  Serial.begin(BAUD_RATE);
  Wire.begin();

  // Optional: wait for serial to be ready (safe on Nano)
  delay(100);

  // // Optional CSV header (recommended)
  // Serial.println("timestamp_ms,ax,ay,az");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;

    int ax = readAccelX();
    int ay = readAccelY();
    int az = readAccelZ();

    Serial.print(now);
    Serial.print(",");
    Serial.print(ax);
    Serial.print(",");
    Serial.print(ay);
    Serial.print(",");
    Serial.println(az);
  }
}

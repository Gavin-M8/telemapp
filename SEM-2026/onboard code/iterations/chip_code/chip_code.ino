/*
 * chip_code.ino
 * GPS Data Logger — LittleFS storage, serial extraction
 * Hardware: LILYGO ESP32 T-Display S3 + NEO-6M GPS
 *
 * ── Usage ────────────────────────────────────────────────────────────────────
 *  Normal boot:  logs GPS data to /gps_log.csv on LittleFS
 *  Extraction:   plug into laptop, open serial at 115200,
 *                send 'D' to dump CSV, send 'X' to delete log
 */

#include <TinyGPSPlus.h>
#include <LittleFS.h>

#define GPS_RX_PIN  44
#define GPS_TX_PIN  43
#define GPS_BAUD    9600

#define PIN_BL   15
#define PIN_PWR  38

#define LOG_FILE       "/gps_log.csv"
#define LOG_INTERVAL_MS 500

HardwareSerial gpsSerial(1);
TinyGPSPlus    gps;

uint32_t lastLogMs = 0;
bool     fsReady   = false;

// ─────────────────────────────────────────────────────────────────────────────
void dumpLog() {
  if (!LittleFS.exists(LOG_FILE)) {
    Serial.println("NO_FILE");
    return;
  }
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) { Serial.println("OPEN_ERR"); return; }
  Serial.println("BEGIN_DUMP");
  while (f.available()) Serial.write(f.read());
  f.close();
  Serial.println("END_DUMP");
}

void deleteLog() {
  if (LittleFS.exists(LOG_FILE)) {
    LittleFS.remove(LOG_FILE);
    Serial.println("LOG_DELETED");
  } else {
    Serial.println("NO_FILE");
  }
}

void logData() {
  if (!fsReady)               return;
  if (!gps.location.isValid()) return;

  File f = LittleFS.open(LOG_FILE, "a");
  if (!f) { Serial.println("LOG_WRITE_ERR"); return; }

  f.printf("%lu,%.6f,%.6f,%.2f,%.1f\n",
           (unsigned long)millis(),
           gps.location.lat(),
           gps.location.lng(),
           gps.speed.mph(),
           gps.course.deg());
  f.close();

  // Mirror to serial so you can see it's working
  Serial.printf("LOGGED: %lu,%.6f,%.6f,%.2f,%.1f\n",
                (unsigned long)millis(),
                gps.location.lat(),
                gps.location.lng(),
                gps.speed.mph(),
                gps.course.deg());
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Keep display power off to save battery
  pinMode(PIN_PWR, OUTPUT);
  pinMode(PIN_BL,  OUTPUT);
  digitalWrite(PIN_PWR, LOW);
  digitalWrite(PIN_BL,  LOW);

  // Init LittleFS
  if (!LittleFS.begin(true)) {  // true = format if mount fails
    Serial.println("LITTLEFS_FAIL");
    fsReady = false;
  } else {
    Serial.println("LITTLEFS_OK");
    fsReady = true;
  }

  // Write CSV header if file doesn't exist yet
  if (fsReady && !LittleFS.exists(LOG_FILE)) {
    File f = LittleFS.open(LOG_FILE, "w");
    if (f) {
      f.println("millis,lat,lon,speed_mph,heading_deg");
      f.close();
    }
  }

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS_READY");
  Serial.println("Commands: D = dump log, X = delete log");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  // Feed GPS
  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  // Handle serial commands from laptop
  if (Serial.available()) {
    char cmd = Serial.read();
    if      (cmd == 'D' || cmd == 'd') dumpLog();
    else if (cmd == 'X' || cmd == 'x') deleteLog();
  }

  // Log on interval
  uint32_t nowMs = millis();
  if ((nowMs - lastLogMs) >= LOG_INTERVAL_MS) {
    lastLogMs = nowMs;
    logData();
    if (!gps.location.isValid()) Serial.println("NO_FIX");
  }
}
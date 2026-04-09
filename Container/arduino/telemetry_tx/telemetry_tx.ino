/*
 * GPS Telemetry Transmitter + Speed Display
 * Hardware: LILYGO ESP32 T-Display S3 + NEO-6M GPS + nRF24L01+PA+LNA
 *
 * ── Wiring ────────────────────────────────────────────────────────────────────
 *  NEO-6M GPS:
 *    VCC  -> ESP32 3V3
 *    GND  -> ESP32 GND
 *    TX   -> GPIO 44   (ESP32 UART1 RX)
 *    RX   -> GPIO 43   (ESP32 UART1 TX — optional, only needed to send config)
 *
 *  nRF24L01+PA+LNA:
 *    VCC  -> ESP32 3V3       (STRICTLY 3.3V — NOT 5V)
 *    GND  -> ESP32 GND
 *    CE   -> GPIO 21
 *    CSN  -> GPIO 10
 *    SCK  -> GPIO 12
 *    MOSI -> GPIO 11
 *    MISO -> GPIO 13
 *    IRQ  -> leave unconnected (polling mode)
 *    *** Solder a 10–100 µF cap across VCC/GND on the nRF24 module ***
 *
 *  Power:
 *    Buck converter OUT+ (5.0 V) -> ESP32 5V pin
 *    Buck converter OUT-         -> ESP32 GND
 *
 * ── Serial output (for USB-connected backend) ─────────────────────────────────
 *  Every TELEMETRY_INTERVAL_MS when a GPS fix is valid:
 *    "<millis>,<lat>,<lon>,<speed_mph>,<heading>\n"
 *  Before fix: "NO_FIX\n"
 *
 * ── RF24 payload (64 bytes) ───────────────────────────────────────────────────
 *  Same CSV string as serial output, transmitted wirelessly to receiver.
 *
 * ── Button 0 ──────────────────────────────────────────────────────────────────
 *  Press to reset peak speed and elapsed timer.
 *
 * ── Libraries required (Arduino Library Manager) ─────────────────────────────
 *  TFT_eSPI   (Bodmer)      — configure User_Setup_Select.h for T-Display S3
 *  TinyGPS++  (Mikal Hart)
 *  RF24       (TMRh20)      — version >= 1.4.8
 */

#include <TFT_eSPI.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <RF24.h>
#include <nRF24L01.h>

// ── GPS (UART1) ───────────────────────────────────────────────────────────────
#define GPS_RX_PIN  44      // ESP32 RX ← GPS TX
#define GPS_TX_PIN  43      // ESP32 TX → GPS RX (optional)
#define GPS_BAUD    9600

// ── nRF24 (HSPI — never contends with TFT's SPI bus) ─────────────────────────
#define NRF_CE_PIN   21
#define NRF_CSN_PIN  10
#define NRF_SCK_PIN  12
#define NRF_MISO_PIN 13
#define NRF_MOSI_PIN 11

// ── T-Display S3 power / backlight ───────────────────────────────────────────
#define PIN_BL   15
#define PIN_PWR  38

// ── Timing ───────────────────────────────────────────────────────────────────
const uint32_t TELEMETRY_INTERVAL_MS = 500;
const uint32_t DISPLAY_MS            = 100;

// ── Hardware objects ──────────────────────────────────────────────────────────
HardwareSerial gpsSerial(1);   // UART1
TinyGPSPlus    gps;
TFT_eSPI       tft = TFT_eSPI();
SPIClass       nrfSPI(HSPI);
RF24           radio(NRF_CE_PIN, NRF_CSN_PIN);

// ── RF24 config ───────────────────────────────────────────────────────────────
const byte    RF_ADDRESS[6]  = "00001";
const uint8_t NRF_MAX_FAILS  = 5;
uint8_t       nrfFailCount   = 0;

// 64-byte payload: millis,lat,lon,speed_mph,heading
struct TelemetryPayload { char data1[64]; };
TelemetryPayload dataPacket;

// ── State ─────────────────────────────────────────────────────────────────────
float    peakSpeedMph  = 0.0f;
uint32_t lastTelemetryMs = 0;
uint32_t lastDisplayMs   = 0;
uint32_t runStartMs      = 0;

// ── Colours (same palette as speed_tx) ───────────────────────────────────────
#define COL_BG      TFT_BLACK
#define COL_LABEL   0x7BEF
#define COL_VALUE   TFT_WHITE
#define COL_UNIT    0x07FF
#define COL_WARN    TFT_YELLOW
#define COL_ERR     TFT_RED
#define COL_GREEN   0x07E0
#define COL_PEAK    0xF81F
#define COL_BAR_BG  0x1082
#define COL_BAR_LO  0x07E0
#define COL_BAR_MID 0xFD20
#define COL_BAR_HI  TFT_RED

const float BAR_MAX_MPH = 60.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
const char* headingToCardinal(float deg) {
  if (deg < 22.5f  || deg >= 337.5f) return "N";
  if (deg < 67.5f)                   return "NE";
  if (deg < 112.5f)                  return "E";
  if (deg < 157.5f)                  return "SE";
  if (deg < 202.5f)                  return "S";
  if (deg < 247.5f)                  return "SW";
  if (deg < 292.5f)                  return "W";
  return "NW";
}

void formatTime(uint32_t ms, char* buf) {
  uint32_t s = ms / 1000, m = s / 60; s %= 60;
  sprintf(buf, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

// ─────────────────────────────────────────────────────────────────────────────
// Radio init — extracted so it can be called from both setup() and the
// failure-recovery path inside transmitTelemetry().
// ─────────────────────────────────────────────────────────────────────────────
bool initRadio() {
  nrfSPI.end();
  nrfSPI.begin(NRF_SCK_PIN, NRF_MISO_PIN, NRF_MOSI_PIN, NRF_CSN_PIN);

  if (!radio.begin(&nrfSPI)) return false;

  // Retries: 3 × 250 µs ≈ 0.75 ms worst-case stall (safe for GPS loop)
  radio.setRetries(3, 3);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  radio.setPayloadSize(sizeof(TelemetryPayload));
  radio.setAutoAck(true);
  radio.openWritingPipe(RF_ADDRESS);
  radio.stopListening();

  nrfFailCount = 0;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Transmit one GPS packet over RF24.
// Tracks consecutive failures and re-initialises radio after NRF_MAX_FAILS.
// ─────────────────────────────────────────────────────────────────────────────
void transmitTelemetry() {
  if (!gps.location.isValid()) return;

  char latStr[12], lonStr[13], spdStr[8], hdgStr[8];
  dtostrf(gps.location.lat(),  9, 6, latStr);
  dtostrf(gps.location.lng(), 10, 6, lonStr);
  dtostrf(gps.speed.mph(),     5, 2, spdStr);
  dtostrf(gps.course.deg(),    5, 1, hdgStr);

  memset(dataPacket.data1, 0, sizeof(dataPacket.data1));
  snprintf(dataPacket.data1, sizeof(dataPacket.data1),
           "%lu,%s,%s,%s,%s",
           (unsigned long)millis(), latStr, lonStr, spdStr, hdgStr);

  bool ok = radio.write(&dataPacket, sizeof(dataPacket));

  if (ok) {
    nrfFailCount = 0;
    Serial.printf("%s ... OK\n", dataPacket.data1);
  } else {
    nrfFailCount++;
    Serial.printf("TX FAIL (%d/%d)\n", nrfFailCount, NRF_MAX_FAILS);
    if (nrfFailCount >= NRF_MAX_FAILS) {
      Serial.println("TX: re-initialising radio...");
      if (initRadio()) {
        Serial.println("TX: radio recovered");
      } else {
        Serial.println("TX: radio recovery FAILED — check wiring");
        nrfFailCount = NRF_MAX_FAILS;
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Serial CSV output (read by Python backend over USB)
// ─────────────────────────────────────────────────────────────────────────────
void serialOutput() {
  if (!gps.location.isValid()) {
    Serial.println("NO_FIX");
    return;
  }
  Serial.printf("%lu,%.6f,%.6f,%.2f,%.1f\n",
                (unsigned long)millis(),
                gps.location.lat(), gps.location.lng(),
                gps.speed.mph(),    gps.course.deg());
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw static display chrome (called once on init and on reset)
// ─────────────────────────────────────────────────────────────────────────────
void drawChrome() {
  tft.fillScreen(COL_BG);

  // Header bar
  tft.fillRect(0, 0, 320, 20, 0x000E);
  tft.setTextColor(COL_UNIT, 0x000E);
  tft.setTextSize(2);
  tft.setCursor(4, 2);   tft.print("GPS Speed");
  tft.setTextColor(COL_LABEL, 0x000E);
  tft.setTextSize(1);
  tft.setCursor(210, 5); tft.print("FIX");
  tft.setCursor(258, 5); tft.print("ELAPSED");

  // Speed bar outline + scale
  tft.drawRect(4, 96, 312, 10, COL_LABEL);
  tft.setTextColor(COL_LABEL, COL_BG);
  tft.setTextSize(1);
  tft.setCursor(4,   108); tft.print("0");
  tft.setCursor(148, 108); tft.print("30");
  tft.setCursor(300, 108); tft.print("60mph");

  // Static labels
  tft.setCursor(4,   124); tft.print("PEAK");
  tft.setCursor(164, 124); tft.print("HEADING");
  tft.setCursor(4,   148); tft.print("POSITION");
}

// ─────────────────────────────────────────────────────────────────────────────
// Update dynamic display elements
// ─────────────────────────────────────────────────────────────────────────────
void updateDisplay() {
  bool  hasFix   = gps.location.isValid();
  float speedMph = hasFix ? gps.speed.mph()  : 0.0f;
  float heading  = hasFix ? gps.course.deg() : 0.0f;

  if (speedMph > peakSpeedMph) peakSpeedMph = speedMph;

  // ── FIX status ──────────────────────────────────────
  tft.setTextSize(1);
  tft.fillRect(204, 3, 50, 8, 0x000E);
  tft.setTextColor(hasFix ? COL_GREEN : COL_ERR, 0x000E);
  tft.setCursor(204, 6);
  tft.print(hasFix ? "GPS OK" : "NO FIX");

  // ── Elapsed time ────────────────────────────────────
  char tbuf[8];
  formatTime(millis() - runStartMs, tbuf);
  tft.fillRect(276, 3, 44, 8, 0x000E);
  tft.setTextColor(COL_VALUE, 0x000E);
  tft.setCursor(276, 6);
  tft.print(tbuf);

  // ── Big speed number (size 7 = 42×56 px per char) ──
  tft.fillRect(4, 22, 210, 60, COL_BG);
  tft.setTextColor(hasFix ? COL_VALUE : COL_LABEL, COL_BG);
  tft.setTextSize(7);
  tft.setCursor(6, 24);
  tft.printf("%.1f", speedMph);

  // "mph" unit
  tft.fillRect(216, 22, 60, 30, COL_BG);
  tft.setTextColor(COL_UNIT, COL_BG);
  tft.setTextSize(3);
  tft.setCursor(218, 32);
  tft.print("mph");

  // Satellite count
  tft.setTextSize(1);
  tft.setTextColor(COL_LABEL, COL_BG);
  tft.fillRect(218, 64, 60, 8, COL_BG);
  tft.setCursor(218, 64);
  tft.printf("%d sats", (int)gps.satellites.value());

  // ── Speed bar ────────────────────────────────────────
  int barW = (int)((speedMph / BAR_MAX_MPH) * 310.0f);
  barW = constrain(barW, 0, 310);
  uint16_t barCol = (speedMph > BAR_MAX_MPH * 0.75f) ? COL_BAR_HI :
                    (speedMph > BAR_MAX_MPH * 0.50f) ? COL_BAR_MID : COL_BAR_LO;
  tft.fillRect(5,         97, barW,       8, barCol);
  tft.fillRect(5 + barW,  97, 310 - barW, 8, COL_BAR_BG);

  // ── Peak speed ───────────────────────────────────────
  tft.setTextSize(2);
  tft.setTextColor(COL_PEAK, COL_BG);
  tft.fillRect(40, 120, 114, 16, COL_BG);
  tft.setCursor(40, 120);
  tft.printf("%.1fmph", peakSpeedMph);

  // ── Heading (degrees + cardinal) ─────────────────────
  tft.setTextColor(COL_VALUE, COL_BG);
  tft.fillRect(230, 120, 86, 16, COL_BG);
  tft.setCursor(230, 120);
  if (hasFix) tft.printf("%.0f %s", heading, headingToCardinal(heading));
  else        tft.print("---");

  // ── Lat / Lon ─────────────────────────────────────────
  tft.setTextSize(1);
  tft.setTextColor(COL_LABEL, COL_BG);
  tft.fillRect(52, 148, 264, 8, COL_BG);
  tft.setCursor(52, 148);
  if (hasFix) tft.printf("%.6f,  %.6f", gps.location.lat(), gps.location.lng());
  else        tft.print("Waiting for GPS fix...");
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Power on peripherals (backlight + I2C/peripheral rail)
  pinMode(PIN_PWR, OUTPUT);
  pinMode(PIN_BL,  OUTPUT);
  digitalWrite(PIN_PWR, HIGH);
  digitalWrite(PIN_BL,  HIGH);
  delay(100);

  // TFT
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_LABEL, COL_BG);
  tft.setTextSize(2);
  tft.setCursor(8, 40); tft.print("Initialising...");

  // GPS on UART1
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  tft.setTextColor(COL_GREEN, COL_BG);
  tft.setCursor(8, 64); tft.print("GPS ready");

  // nRF24
  if (!initRadio()) {
    tft.setTextColor(COL_WARN, COL_BG);
    tft.setCursor(8, 88); tft.print("Radio init failed!");
    Serial.println("WARNING: nRF24L01 init failed — telemetry disabled");
    delay(2000);
  }

  // Button 0 = reset peak + timer
  pinMode(0, INPUT_PULLUP);

  runStartMs = millis();
  drawChrome();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  // Feed all available GPS bytes into TinyGPS++
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // Button 0: reset peak speed and elapsed timer
  if (digitalRead(0) == LOW) {
    delay(50);
    if (digitalRead(0) == LOW) {
      peakSpeedMph = 0.0f;
      runStartMs   = millis();
      drawChrome();
    }
  }

  uint32_t nowMs = millis();

  // Update display + serial output at DISPLAY_MS interval
  if ((nowMs - lastDisplayMs) >= DISPLAY_MS) {
    lastDisplayMs = nowMs;
    updateDisplay();

    // Serial CSV for USB-connected Python backend
    if (gps.location.isUpdated()) {
      serialOutput();
    } else if (!gps.location.isValid()) {
      Serial.println("NO_FIX");
    }
  }

  // RF24 telemetry at TELEMETRY_INTERVAL_MS
  if ((nowMs - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = nowMs;
    transmitTelemetry();
  }
}

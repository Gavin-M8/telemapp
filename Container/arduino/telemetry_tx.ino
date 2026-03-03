#include <SPI.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <printf.h>

#define CE_PIN 9 
#define CSN_PIN 10 
#define INTERVAL_MS_TRANSMISSION 500 

RF24 radio(CE_PIN, CSN_PIN); 

unsigned long lastSample = 0;

int readAccelX() { return analogRead(A0); }
int readAccelY() { return analogRead(A1); }
int readAccelZ() { return analogRead(A2); }

const byte address[6] = "00001"; 

struct payload { 
   char data1[32]; 
}; 
payload dataPacket; 

void setup() {
   Serial.begin(19200);
   printf_begin();
   
   if (!radio.begin()) {
     Serial.println("Radio hardware not found!");
     while (1);
   }

   // --- MATCHING CONFIGURATION ---
   radio.setPALevel(RF24_PA_LOW); 
   radio.setDataRate(RF24_250KBPS);     // Slower = More reliable
   radio.setChannel(100);               // Avoids WiFi interference
   radio.setPayloadSize(sizeof(payload)); // Fixes packet size mismatch
   radio.setAutoAck(true); 
   radio.setRetries(15, 15); 
   // ------------------------------

   radio.openWritingPipe(address);
   radio.stopListening(); 

   Serial.println("--- Transmitter Ready ---");
   radio.printDetails(); 
}

void loop() { 

  unsigned long now = millis();

  if (now - lastSample >= INTERVAL_MS_TRANSMISSION) {
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

    snprintf(dataPacket.data1, sizeof(dataPacket.data1), "%lu,%d,%d,%d", now, ax, ay, az);
  }

   bool report = radio.write(&dataPacket, sizeof(dataPacket));

   if (report) {
     Serial.println(" ... SUCCESS!");
   } else {
     Serial.println(" ... FAILED (No Ack)");
   }
 
}
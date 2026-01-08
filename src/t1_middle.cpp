#include <SPI.h>
#include <LoRa.h>

#define ss 15     
#define rst 16    
#define dio0 2    

unsigned long resendStartTime = 0;
const unsigned long resendDuration = 10000; 
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; 

String lastSignal = "";
bool isResending = false;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa Starting...");

  LoRa.setPins(ss, rst, dio0);
  LoRa.setSyncWord(0xA2);  
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);
  LoRa.setTxPower(20);

  if (!LoRa.begin(433920000)) {
    Serial.println("LoRa failed!");
    while (1);
  }

  Serial.println("LoRa Initialized.");
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String LoRaData = LoRa.readString();
    LoRaData.trim();  

    Serial.print("Received (Size ");
    Serial.print(packetSize);
    Serial.print("): ");
    Serial.println(LoRaData);

    if (LoRaData.length() == 8) {
      String netid = LoRaData.substring(0, 4);
      Serial.print("Extracted netid: ");
      Serial.println(netid);

      if (netid == "1023") {
        lastSignal = LoRaData;
        resendStartTime = millis();
        isResending = true;
        lastSendTime = 0;  
        Serial.println("Valid signal received 10s resend");
      } else {
        Serial.println("Invalid netid");
      }
    } else {
      Serial.println("Invalid signal length");
    }
  }

  if (isResending) {
    unsigned long now = millis();

    if (now - resendStartTime <= resendDuration) {
      if (now - lastSendTime >= sendInterval) {
        LoRa.beginPacket();
        LoRa.print(lastSignal);
        LoRa.endPacket();

        Serial.print("Resending: ");
        Serial.println(lastSignal);

        lastSendTime = now;
      }
    } else {
      Serial.println("resend ended.");
      isResending = false;
    }
  }
}

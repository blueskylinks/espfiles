#include <EEPROM.h>
#include <Arduino.h>
#include <TM1637Display.h>
#include <LoRa.h>
#include <SPI.h>

#define CLK D3
#define DIO D4

#define ss D8
#define rst D0
#define dio0 D4  

int addr1 = 0;
int motor_duration = 30;
int motor_time = 0;
int motor_status = 0;
int motor_status_manual = 0;

const int buzzer = D2;
const int input1 = D9;

int lora_pac_count = 0;
unsigned long lastLoRaReceiveTime = 0;
const unsigned long loRaTimeout = 15000;  
const uint8_t seg_full[] = {
  SEG_A | SEG_G | SEG_F | SEG_E,
  SEG_A | SEG_G | SEG_F | SEG_E,
  SEG_D,
  SEG_D
};

const uint8_t seg_nodata[] = {
  SEG_A | SEG_E | SEG_F | SEG_G,         
  SEG_A | SEG_B | SEG_E | SEG_F | SEG_G, 
  SEG_D | SEG_E | SEG_F,                 
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G  
};

TM1637Display display(CLK, DIO);

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  pinMode(input1, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, HIGH);  

  EEPROM.get(addr1, motor_duration);
  if (motor_duration < 1 || motor_duration > 180) motor_duration = 30;

  display.setBrightness(0x0f);
  display.clear();

  int temp_count = 100;
  if (digitalRead(input1) == LOW) {
    while (temp_count-- > 0) {
      if (digitalRead(input1) == LOW) {
        motor_duration = (motor_duration + 5) % 185;
        if (motor_duration == 0) motor_duration = 5;
      }
      delay(200);
      EEPROM.put(addr1, motor_duration);
      EEPROM.commit();
      display.showNumberDec(motor_duration, false);
    }
  }

  motor_time = motor_duration * 60;

  for (int i = 0; i < 4; i++) {
    display.showNumberDec(motor_duration, false);
    delay(200);
    display.clear();
    delay(200);
  }

  LoRa.setPins(ss, rst, dio0);
  LoRa.setSyncWord(0xA2);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);

  int attempts = 0;
  while (!LoRa.begin(433920000)) {
    Serial.println("Trying LoRa...");
    delay(500);
    if (++attempts >= 30) break;
  }

  lastLoRaReceiveTime = millis();
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String LoRaData = LoRa.readString();
    Serial.print("Received: ");
    Serial.println(LoRaData);

    if (LoRaData == "2014") {
      lora_pac_count++;
      display.setSegments(seg_full);

      if (lora_pac_count >= 3) { 
        Serial.println("Tank Full. Stopping motor.");
        digitalWrite(buzzer, LOW);
        motor_status = 0;
        motor_status_manual = 0;
        motor_time = motor_duration * 60;
        lora_pac_count = 0;

        display.setSegments(seg_full);
        delay(500);
        display.showNumberDec(0, false);
      }
    }

    lastLoRaReceiveTime = millis(); 
    Serial.print("RSSI: ");
    Serial.println(LoRa.packetRssi());
  }

  if (millis() - lastLoRaReceiveTime > loRaTimeout) {
    Serial.println("LoRa Timeout. Switching to safe state.");
    digitalWrite(buzzer, LOW);
    motor_status = 0;
    motor_status_manual = 0;
    motor_time = motor_duration * 60;
    display.setSegments(seg_nodata);
  }

  if (digitalRead(input1) == LOW) {
    Serial.println("Manual button pressed.");
    delay(200);  

    if (motor_status_manual == 0) {
      digitalWrite(buzzer, HIGH);
      motor_status = 1;
      motor_status_manual = 1;
      motor_time = motor_duration * 60;
    } else {
      digitalWrite(buzzer, LOW);
      motor_status = 0;
      motor_status_manual = 0;
      display.showNumberDec(0, false);
    }
  }

  static unsigned long lastMotorUpdate = 0;
  static int stopConfirmCount = 0;

  if (motor_status == 1 && millis() - lastMotorUpdate >= 1000) {
    motor_time--;
    lastMotorUpdate = millis();

    display.showNumberDec(1, false, 1, 0);  
    display.showNumberDec((motor_time / 60) + 1, false);

    if (motor_time <= 0) {
      stopConfirmCount++;
      if (stopConfirmCount >= 5) {
        Serial.println("Motor stopped (timeout).");
        digitalWrite(buzzer, LOW);
        motor_status = 0;
        motor_status_manual = 0;
        motor_time = motor_duration * 60;
        stopConfirmCount = 0;
        display.showNumberDec(0, false);
      }
    } else {
      stopConfirmCount = 0;
    }
  }

  delay(100);
}

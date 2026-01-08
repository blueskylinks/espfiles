#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Display Module connection pins
#define CLK D3
#define DIO D4

#define ss D8
#define rst D0
#define dio0 D4

const int buzzer = D2;
const int epin = D1;

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// LCD and NTP setup
LiquidCrystal_I2C lcd(0x27,16,2);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // 19800 = UTC+5:30 (India)

int sdevice[8] = {0};
int prev_sdevice[8] = {0};
int cdevice[8] = {0};
int cdstatus[8] = {0};
int t_count = 0, count = 0, cnt = 0;
int timeout_val = 70;
int minval = 0;
char motor_st = 'S';

// === Function to get current time in HHMM format ===
String getTimeHHMM() {
  timeClient.update();
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  String hh = h < 10 ? "0" + String(h) : String(h);
  String mm = m < 10 ? "0" + String(m) : String(m);
  return hh + mm; // "HHMM"
}

void setup() {
  Serial.begin(115200);
  Wire.begin(2, 0);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("SkyIoT Control");

  pinMode(buzzer, OUTPUT);
  pinMode(epin, OUTPUT);
  digitalWrite(buzzer, LOW);
  digitalWrite(epin, LOW);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");

  timeClient.begin();
  timeClient.update();

  // Initialize LoRa
  LoRa.setPins(ss, rst, dio0);
  LoRa.setSyncWord(0xA2);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);
  int attempts = 0;
  while (!LoRa.begin(433920000) && attempts++ < 60) {
    delay(500);
  }
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String LoRaData = "";
    while (LoRa.available()) {
      LoRaData += LoRa.readString();
    }

    if (LoRaData.startsWith("1023") && packetSize == 8) {
      for (int i = 1; i <= 8; i++) {
        String id = "1023" + (i < 10 ? "0" + String(i) : String(i));
        String payload = LoRaData.substring(6);
        if (LoRaData.substring(0, 6) == id) {
          bool on = (payload == "11");
          sdevice[i - 1] = on ? 1 : 0;
          cdstatus[i - 1] = on;
          if (on) cdevice[i - 1] = timeout_val;
        }
      }
    }
  }

  for (int i = 0; i < 8; i++) {
    if (sdevice[i] != prev_sdevice[i]) {
      String ts = getTimeHHMM();
      if (sdevice[i] == 1) {
        Serial.printf("Device %d STARTED at %s\n", i + 1, ts.c_str());
      } else {
        Serial.printf("Device %d STOPPED at %s\n", i + 1, ts.c_str());
      }
      prev_sdevice[i] = sdevice[i];
    }
    if (cdevice[i] > minval) {
      cdevice[i]--;
    } else {
      sdevice[i] = 0;
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DEVICES & MOTOR:");
  lcd.setCursor(0, 1);
  for (int i = 0; i < 8; i++) lcd.print(sdevice[i]);
  lcd.print(" ");
  lcd.print(motor_st);

  // UART Output
  String uartData = "";
  for (int i = 0; i < 8; i++) uartData += String(sdevice[i]);
  uartData += motor_st;
  Serial.println(uartData);

  delay(500);
  count++;
  t_count++;
  cnt++;

  if (count >= 15) {
    int activeCount = 0;
    for (int i = 0; i < 8; i++) {
      if (cdevice[i] > 0) activeCount++;
    }

    if (activeCount >= 1 && digitalRead(buzzer) == LOW) {
      digitalWrite(buzzer, HIGH);
      motor_st = 'R';
    }

    if (activeCount < 1 && digitalRead(buzzer) == HIGH) {
      digitalWrite(buzzer, LOW);
      motor_st = 'S';
      ESP.restart();
    }

    digitalWrite(epin, activeCount == 1 ? HIGH : LOW);

    if (activeCount < 1) {
      for (int i = 0; i < 8; i++) sdevice[i] = 0;
    }

    count = 0;
  }

  if (digitalRead(buzzer) == HIGH && cnt >= 10000) {
    digitalWrite(buzzer, LOW);
    delay(400000); // 400 seconds
    ESP.restart();
  }

  if (cnt >= 12000) cnt = 0;
}

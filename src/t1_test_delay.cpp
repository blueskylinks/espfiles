#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Display Module connection pins (Digital Pins)
#define CLK D3
#define DIO D4

#define ss D8
#define rst D0
#define dio0 D4

const int buzzer = D2;
const int epin = D1;
const int input1 = D9;

int t_count = 0;
int count = 0;
int cnt = 0;
int sdevice[] = {0, 0, 0, 0, 0, 0, 0, 0};
int cdevice[] = {0, 0, 0, 0, 0, 0, 0, 0};
int cdstatus[] = {0, 0, 0, 0, 0, 0, 0, 0};
int st = 2;
int minval = 0;
int temp_count1 = 0;
char motor_st = 'S';
int timeout_val = 70;

bool cooldown = false;
unsigned long cooldownStartTime = 0;
unsigned long cooldownDuration = 5 * 60 * 1000; // 5 minutes in milliseconds

LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD address

void setup() {
  Serial.begin(115200);
  Wire.begin(2, 0);
  lcd.init();
  lcd.setCursor(0, 0);
  lcd.print("SkyIoT Control");
  lcd.backlight();
  pinMode(buzzer, OUTPUT);
  pinMode(epin, OUTPUT);
  digitalWrite(epin, LOW);
  digitalWrite(buzzer, LOW);
  delay(1000);

  LoRa.setPins(ss, rst, dio0);
  LoRa.setSyncWord(0xA2);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);

  temp_count1 = 30;
  while (!LoRa.begin(433920000)) {
    temp_count1++;
    if (temp_count1 >= 60) break;
    delay(500);
  }
}

void loop() {
  // Handle cooldown logic
  if (cooldown) {
    if (millis() - cooldownStartTime >= cooldownDuration) {
      ESP.restart();  // Restart after 5 minutes
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cooldown Active");
      lcd.setCursor(0, 1);
      lcd.print("Wait 5 minutes");
      delay(1000);
      return; // Skip rest of loop
    }
  }

  // LoRa packet processing
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    while (LoRa.available()) {
      String LoRaData = LoRa.readString();
      String netid = LoRaData.substring(0, 4);
      String deviceid = LoRaData.substring(0, 6);
      String devicestatus = LoRaData.substring(6);

      if (netid.equals("1023") && packetSize == 8) {
        if (deviceid == "102301") {
          sdevice[0] = (devicestatus == "11") ? 1 : 0;
          cdstatus[0] = sdevice[0];
          if (sdevice[0]) cdevice[0] = timeout_val;
        }
        if (deviceid == "102302") {
          sdevice[1] = (devicestatus == "11") ? 1 : 0;
          cdstatus[1] = sdevice[1];
          if (sdevice[1]) cdevice[1] = timeout_val;
        }
        if (deviceid == "102303") {
          sdevice[2] = (devicestatus == "11") ? 1 : 0;
          cdstatus[2] = sdevice[2];
          if (sdevice[2]) cdevice[2] = timeout_val;
        }
        if (deviceid == "102304") {
          sdevice[3] = (devicestatus == "11") ? 1 : 0;
          cdstatus[3] = sdevice[3];
          if (sdevice[3]) cdevice[3] = timeout_val;
        }
        if (deviceid == "102305") {
          sdevice[4] = (devicestatus == "11") ? 1 : 0;
          cdstatus[4] = sdevice[4];
          if (sdevice[4]) cdevice[4] = timeout_val;
        }
        if (deviceid == "102306") {
          sdevice[5] = (devicestatus == "11") ? 1 : 0;
          cdstatus[5] = sdevice[5];
          if (sdevice[5]) cdevice[5] = timeout_val;
        }
        if (deviceid == "102307") {
          sdevice[6] = (devicestatus == "11") ? 1 : 0;
          cdstatus[6] = sdevice[6];
          if (sdevice[6]) cdevice[6] = timeout_val;
        }
        if (deviceid == "102308") {
          sdevice[7] = (devicestatus == "11") ? 1 : 0;
          cdstatus[7] = sdevice[7];
          if (sdevice[7]) cdevice[7] = timeout_val;
        }
      }
    }
  }

  // Decrease timeout counters
  for (int i = 0; i < 8; i++) {
    if (cdevice[i] > minval) cdevice[i]--;
  }

  // LCD Update
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("C O M Bb Hh V MT");
  lcd.setCursor(0, 1);
  for (int i = 0; i < 8; i++) {
    lcd.print(sdevice[i]);
    if (i == 0 || i == 1 || i == 2 || i == 4 || i == 5 || i == 6) lcd.print(" ");
  }
  lcd.print(motor_st);

  // Serial output
  for (int i = 0; i < 8; i++) {
    Serial.print(sdevice[i]);
    Serial.print(" ");
  }
  Serial.print(motor_st);
  Serial.println();

  // Send data over UART
  String uartData = "";
  for (int i = 0; i < 8; i++) uartData += String(sdevice[i]);
  uartData += motor_st;
  Serial.println(uartData);

  delay(500);
  count++;
  t_count++;
  cnt++;

  if (count >= 15) {
    int activeDevices = 0;
    for (int i = 0; i < 8; i++) {
      if (cdevice[i] > 0) activeDevices++;
      else sdevice[i] = 0;
    }

    if (activeDevices >= 1 && digitalRead(buzzer) == 0) {
      digitalWrite(buzzer, HIGH);
      motor_st = 'R';
    }

    if (activeDevices < 1 && digitalRead(buzzer) == 1) {
      digitalWrite(buzzer, LOW);
      motor_st = 'S';
      cooldownStartTime = millis();
      cooldown = true;  // Start cooldown instead of immediate restart
    }

    digitalWrite(epin, (activeDevices == 1) ? HIGH : LOW);

    if (activeDevices < 1) {
      for (int i = 0; i < 8; i++) sdevice[i] = 0;
    }

    count = 0;
  }

  if (digitalRead(buzzer) == 1 && cnt >= 10000) {
    digitalWrite(buzzer, LOW);
    cooldownStartTime = millis();
    cooldown = true; // Enter cooldown instead of delay + restart
  }

  if (cnt >= 12000) cnt = 0;
}

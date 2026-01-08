#include <LoRa.h>
#include <SPI.h>
#include <EEPROM.h>

#define ss 15
#define rst 16
#define dio0 2
#define networkid "1023"

const int hsen = D2;
const int lsen = D5;
const int hpin = D1;
const int sled = LED_BUILTIN;
const int input1 = D9;
const int potPin = A0; 

int deviceNum = 1;
String deviceid = "01";
int counter = 10;
int vstate1 = 2;
int vstate2 = 2;
int temp_count1 = 0;
int temp_count2 = 0;
int temp_count3 = 0;

void setup() {
  Serial.begin(115200);

  pinMode(input1, INPUT_PULLUP);
  pinMode(D0, WAKEUP_PULLUP);
  pinMode(hsen, INPUT_PULLUP);
  pinMode(lsen, INPUT_PULLUP);
  pinMode(hpin, OUTPUT);
  pinMode(sled, OUTPUT);
  digitalWrite(hpin, LOW);
  digitalWrite(sled, HIGH);

  Serial.println("LoRa Sender");
  LoRa.setPins(ss, rst, dio0);
  LoRa.setSyncWord(0xA2);
  LoRa.setTxPower(20);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);

  while (!LoRa.begin(433920000)) {
    Serial.println("LoRa Init Failed");
    delay(500);
  }
  Serial.println("LoRa Initialized Successfully!");
}

void send_data() {
  for (int i = 0; i < 10; i++) {
    LoRa.beginPacket();   
    LoRa.print(networkid);
    LoRa.print(deviceid);
    LoRa.print(vstate1);
    LoRa.print(vstate2);
    LoRa.endPacket();
    Serial.print(networkid);
    Serial.print(deviceid);
    Serial.print(vstate1);
    Serial.print(vstate2);
    Serial.print("."); 
    delay(100);
  }
  Serial.println();
}

void loop() {
  int analogValue = analogRead(potPin);
  int id = analogValue / 128 + 1;
  if (id > 8) id = 8;
  deviceNum = id;
  deviceid = (deviceNum < 10) ? "0" + String(deviceNum) : String(deviceNum);
 for (int i = 0; i < deviceNum; i++) {
  digitalWrite(sled, LOW);
  delay(300);
  digitalWrite(sled, HIGH);
  delay(300);
  digitalWrite(sled, LOW);
  delay(3000);
  }
  Serial.print("Analog Value: ");
  Serial.print(analogValue);
  Serial.print(" => Device ID: ");
  Serial.println(deviceid);

  if (digitalRead(hsen) == 0 && digitalRead(hpin) == 1) {
    temp_count1++;
    if (temp_count1 >= 3) {
      vstate1 = 0;
      vstate2 = 0;
    }
    if (temp_count1 >= 15) {
      digitalWrite(hpin, LOW);
      Serial.println("Motor OFF");
      temp_count1 = 0;
    }
  } else {
    temp_count1 = 0;
  }

  if (digitalRead(lsen) == 0 && digitalRead(hpin) == 0) {
    temp_count2++;
    if (temp_count2 >= 3) {
      digitalWrite(hpin, HIGH);
      vstate1 = 1;
      vstate2 = 1;
      Serial.println("Motor ON");
      temp_count2 = 0;
    }
  } else {
    temp_count2 = 0;
  }

  if (digitalRead(hpin) == 1 && digitalRead(lsen) != 0) {
    temp_count3++;
    if (temp_count3 >= 3) {
      vstate1 = 0;
      vstate2 = 0;
    }
    if (temp_count3 >= 10) {
      digitalWrite(hpin, LOW);
      Serial.println("Motor OFF");
      temp_count3 = 0;
    }
  } else {
    temp_count3 = 0;
  }

  counter++;
  if (counter >= 200) counter = 10;

  send_data();

  digitalWrite(sled, LOW);
  delay(50);
  digitalWrite(sled, HIGH);
  delay(50);

  delay(2000);  
}

#include <LoRa.h>
#include <SPI.h>
//#include <ESP8266WiFi.h>

#define ss 15
#define rst 16
#define dio0 2
#define networkid "1023"
#define deviceid "02"
 
int counter = 10;
const int hsen = D2;
const int lsen = D9;
const int hpin = D1;
const int sled = LED_BUILTIN;
int value = 11;
int state=0;
int vstate1=1;
int vstate2=1;
int volt_state=1;
int temp_count1=0;
int temp_count2=0;
int temp_count3=0;
int rnum = 3;
int rnum1 = 1;
 
void setup() 
{
  Serial.begin(115200); 
  pinMode(D0, WAKEUP_PULLUP);
  //WiFiMode(WIFI_STA);
  //WiFi.disconnect(); 
  //WiFi.mode(WIFI_OFF);
  pinMode(hsen, INPUT_PULLUP); 
  pinMode(lsen, INPUT_PULLUP);
  pinMode(hpin, OUTPUT);
  pinMode(sled, OUTPUT);
  digitalWrite(hpin,HIGH);
  digitalWrite(sled,HIGH);
  while (!Serial);
  Serial.println("LoRa Sender");
  LoRa.setPins(ss, rst, dio0);    //setup LoRa transceiver module
  LoRa.setSyncWord(0xA2);
  LoRa.setTxPower(20);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);
  while (!LoRa.begin(433920000))     //433E6 - Asia, 866E6 - Europe, 915E6 - North America
  {
    Serial.println(".");
    delay(500);
  }
  Serial.println("LoRa Initializing OK!");
}

void send_data(int rn){
  int i;
  for(i=0;i<=(10);i++){
    LoRa.beginPacket();   //Send LoRa packet to receiver
    LoRa.print(networkid);
    LoRa.print(deviceid);
    LoRa.print(vstate1);
    LoRa.print(vstate2);
    LoRa.endPacket(); 
    Serial.print(".");
    delay(100);
  }
  Serial.println("");
}
 
void loop() 
{
  Serial.print("Sending packet: ");
  Serial.println(counter);

  counter++;
  if(counter>=200){
    counter=10;
  }
  send_data(rnum);
  digitalWrite(sled,LOW);
  delay(50);
  digitalWrite(sled,HIGH);
  delay(50);
  delay(4000);
}

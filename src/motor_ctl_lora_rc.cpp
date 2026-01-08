#include <EEPROM.h>
#include <Arduino.h>
#include <TM1637Display.h>
#include <LoRa.h>
#include <SPI.h>

// Display Module connection pins (Digital Pins)
#define CLK D3
#define DIO D4

#define ss D8
#define rst D0
#define dio0 D4

int addr1 = 0;
int addr2 = 1;
int value1 = 1;
int value2 = 2;
byte memval1;
byte memval2;

const int ot_sensor = D1;
const int ut_sensor = D1;
const int buzzer = D2;
const int input1 = D5;
int ot_sensorstatus=1;
int ut_sensorstatus=1;
int error_sensorstatus=1;
int ot_sensorcount=0;
int ut_sensorcount=0;
int lora_pac_count=0;

int motor_status=0;
int motor_status_manual=0;
int motor_time=0;
int empty_start=0;
int value_count=0;
int motor_duration=30;
int motor_stoptime=0;
int tank_size, len;
int count=0;
int sound=0;

const uint8_t seg_empty[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           
  SEG_D,   
  SEG_D,                           
  SEG_D           
  };

const uint8_t seg_full[] = {
  SEG_A | SEG_G | SEG_F | SEG_E,           
  SEG_A | SEG_G | SEG_F | SEG_E,   
  SEG_D,                           
  SEG_D           
  };

TM1637Display display(CLK, DIO);
  uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
  uint8_t blank[] = { 0x00, 0x00, 0x00, 0x00 };
  uint8_t data_full[] = { 0x15, 0x15, 0x00, 0x00 };
  uint8_t data_empty[] = { 0x14, 0x00, 0x00, 0x00 };

void setup() {
  Serial.begin(115200); // Starts the serial communication
  EEPROM.begin(512);
  pinMode(input1, INPUT_PULLUP);  
  pinMode(buzzer, OUTPUT);
  pinMode(ot_sensor, INPUT_PULLUP);
  pinMode(ut_sensor, INPUT_PULLUP);
  digitalWrite(buzzer, HIGH);
  motor_status=1;  
  delay(1000);
  memval1=EEPROM.read(addr1);
  motor_duration=memval1;

  display.setBrightness(0x0f);
  display.setSegments(blank);

  int temp_count=100;
  if(digitalRead(input1)==0){
    while(temp_count>=1){
      if(digitalRead(input1)==0){
        if(motor_duration<=180){
          motor_duration=motor_duration+5;
          temp_count=temp_count+1;
        }else{
          motor_duration=0;
        } 
      }
      temp_count=temp_count-1;
      delay(200);
      Serial.print("Temp Count:");
      Serial.println(temp_count);
      Serial.print("Motor Duration:");
      Serial.println(motor_duration);

      EEPROM.write(addr1, motor_duration);
      if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
      display.showNumberDec(5, false, 1, 0);
      display.showNumberDec(motor_duration,false);
      } else {
      Serial.println("ERROR! EEPROM commit failed");
      }      
    }
  }  

  if(motor_duration<1 || motor_duration>180){
    motor_duration=30;
  }
   
  motor_time=motor_duration*60;
  empty_start=0;

  const uint8_t seg_duration[] = {
  SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,           
  SEG_D,   
  SEG_D,                           
  SEG_D           
  };

  for(int i=0;i<8;i++){
    display.showNumberDec(motor_duration,false);
    delay(200);
    display.clear();
    delay(200);
  }
  display.showNumberDec(0,false);

  LoRa.setPins(ss, rst, dio0);    //setup LoRa transceiver module
  LoRa.setSyncWord(0xA2);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(62.5E3);
  //LoRa.begin(433E6)
  int temp_count1=30;

  while (!LoRa.begin(433920000))     //433E6 - Asia, 866E6 - Europe, 915E6 - North America
  {
    Serial.println(".");
    temp_count1++;
    if(temp_count1>=30){
      break;
    }
    delay(500);
  }

}

void loop() {
  int packetSize = LoRa.parsePacket();    // try to parse packet
  if (packetSize) 
  {
    Serial.print("Packet Size:");
    Serial.println(packetSize);
    Serial.print("Received packet '");
    while (LoRa.available())              // read packet
    {
      String LoRaData = LoRa.readString();
      Serial.println(LoRaData); 
      String deviceid=LoRaData.substring(0,4);
      String devicestatus=LoRaData.substring(8,10);
      Serial.println(deviceid);
      Serial.println(devicestatus);
      if(deviceid.equals("2014")){
        display.setSegments(seg_full);
        lora_pac_count++;
        Serial.print("Packets:");
        Serial.println(lora_pac_count);
        if(lora_pac_count>=5){
          Serial.println("Tank Full.........");
          digitalWrite(buzzer, LOW);
          motor_status=0;
          ot_sensorcount=0;
          display.showNumberDec(0,false); 
          motor_time=(motor_duration)*60;
          lora_pac_count=0;
        }
      }
    }
    Serial.print("' with RSSI ");         // print RSSI of packet
    Serial.println(LoRa.packetRssi());
  }

  Serial.println("=========================================");
  Serial.print("Motor Status:");
  Serial.println(motor_status);
  Serial.print("Motor Duration:");
  Serial.println(motor_duration);
  ot_sensorstatus=digitalRead(ot_sensor);
  ut_sensorstatus=digitalRead(ut_sensor);
  Serial.print("Motor Time:");
  Serial.println(motor_time); 
  Serial.print("HighWater Tank Status:");
  Serial.println(ot_sensorstatus);
  Serial.print("LowWater Status:");
  Serial.println(ut_sensorstatus);

  if(digitalRead(input1)==0){
    Serial.println("Button Clicked...!");
    if(motor_status_manual==0){
      digitalWrite(buzzer,HIGH);
      motor_status_manual=1;
      motor_status=1;
      motor_time=motor_duration*60;
      value_count=15;
    }else{
      if(motor_status_manual==1){
        digitalWrite(buzzer,LOW);
        Serial.println("Motor is now stopped..!");
        motor_status_manual=0;
        motor_status=0;
        value_count=0;
      }
    } 
  }
  
  if(ot_sensorstatus==0){
    display.setSegments(seg_full);
  }else{
  }

  if(ut_sensorstatus==0){
    Serial.println(ut_sensorcount);
    Serial.println("Tank Empty");
    display.setSegments(seg_empty);
    if(ut_sensorcount>=20){
      if(digitalRead(buzzer)!=1){
         Serial.println("Water tank is still empty, turning on the Motor");
         motor_time=(motor_duration)*60;
         digitalWrite(buzzer, HIGH);
         motor_status=1;
      }
      ut_sensorcount=0;
    }
   ut_sensorcount=ut_sensorcount+1;
   delay(500);
  }else{
    ut_sensorcount=0;
    delay(500);
    display.showNumberDec(0,false);
  }

  if(digitalRead(buzzer)==1){
   motor_time=motor_time-1;
   Serial.println("Motor Running..!");
   display.showNumberDec(1, false, 1, 0);
   display.showNumberDec((motor_time/60)+1,false);  
   if(motor_time<=0 || ot_sensorstatus==0 || error_sensorstatus==0){
    Serial.print("Sensor Count:");
    Serial.println(ot_sensorcount);
    Serial.println("Tank Full or Timed Out..!");
    ot_sensorcount=ot_sensorcount+1;
      if(ot_sensorcount>=5){
        digitalWrite(buzzer, LOW);
        motor_status=0;
        ot_sensorcount=0;
        display.showNumberDec(0,false); 
        motor_time=(motor_duration)*60;
      }else{
      //ot_sensorcount=0;
      } 
   }else{
    ot_sensorcount=0;
   }
  }
  delay(500);
  count=count+1;

 }
#include <EEPROM.h>
#include <Arduino.h>
#include <TM1637Display.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char *ssid = "Airtel_9764005401";
const char *password = "air46403";

// const char* ssid = "Galaxy M42";
// const char* password = "Chai1111";

// Display Module connection pins (Digital Pins)
#define CLK D3
#define DIO D4

int addr1 = 0;
int value1 = 1;
byte memval1;

const int ot_sensor = D1;
const int ut_sensor = D2;
const int outpin = D8;
const int ot_status = D6;
const int ut_status = D5;
const int auto_status = D7;
const int input1 = D9;
int ot_sensorstatus = 1;
int ut_sensorstatus = 1;
int error_sensorstatus = 1;
int ot_sensorcount = 0;
int ut_sensorcount = 0;

int motor_status = 0;
int motor_status_manual = 0;
int motor_time = 0;
int empty_start = 0;
int value_count = 0;
int motor_duration = 30;
int motor_stoptime = 0;
int tank_size, len;
int count = 0;
int sound = 0;
int wifi_timout = 20;
unsigned long timestamp;

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);
// 19800 = offset in seconds (5h 30m), 60000 = update every 60s

const uint8_t seg_empty[] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_D,
    SEG_D,
    SEG_D};

const uint8_t seg_full[] = {
    SEG_A | SEG_G | SEG_F | SEG_E,
    SEG_A | SEG_G | SEG_F | SEG_E,
    SEG_D,
    SEG_D};

TM1637Display display(CLK, DIO);
uint8_t data[] = {0xff, 0xff, 0xff, 0xff};
uint8_t blank[] = {0x00, 0x00, 0x00, 0x00};
uint8_t data_full[] = {0x15, 0x15, 0x00, 0x00};
uint8_t data_empty[] = {0x14, 0x00, 0x00, 0x00};

void checkWiFiConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < wifi_timout)
    {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\n Reconnected to WiFi.");
      delay(1000);
      timeClient.begin();
    }
    else
    {
      Serial.println("\n Reconnection failed.");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512);
  WiFi.mode(WIFI_STA);
  pinMode(input1, INPUT_PULLUP);
  pinMode(outpin, OUTPUT);
  pinMode(ot_status, OUTPUT);
  pinMode(ut_status, OUTPUT);
  pinMode(ot_sensor, INPUT_PULLUP);
  pinMode(ut_sensor, INPUT_PULLUP);
  pinMode(auto_status, OUTPUT);
  digitalWrite(outpin, HIGH);
  motor_status = 1;
  digitalWrite(auto_status, HIGH);

  delay(1000);
  memval1 = EEPROM.read(addr1);
  motor_duration = memval1;

  display.setBrightness(0x0f);
  display.setSegments(blank);

  int temp_count = 100;
  if (digitalRead(input1) == 0)
  {
    while (temp_count >= 1)
    {
      if (digitalRead(input1) == 0)
      {
        if (motor_duration <= 100)
        {
          motor_duration = motor_duration + 5;
          temp_count = temp_count + 1;
        }
        else
        {
          motor_duration = 0;
        }
      }
      temp_count = temp_count - 1;
      delay(200);
      Serial.print("Temp Count:");
      Serial.println(temp_count);
      Serial.print("Motor Duration:");
      Serial.println(motor_duration);

      EEPROM.write(addr1, motor_duration);
      if (EEPROM.commit())
      {
        Serial.println("EEPROM successfully committed");
        display.showNumberDec(5, false, 1, 0);
        display.showNumberDec(motor_duration, false);
      }
      else
      {
        Serial.println("ERROR! EEPROM commit failed");
      }
    }
  }

  if (motor_duration < 1 || motor_duration > 100)
  {
    motor_duration = 30;
  }

  motor_time = motor_duration * 60;
  empty_start = 0;

  const uint8_t seg_duration[] = {
      SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,
      SEG_D,
      SEG_D,
      SEG_D};

  checkWiFiConnection();

  for (int i = 0; i < 8; i++)
  {
    display.showNumberDec(motor_duration, false);
    delay(200);
    display.clear();
    delay(200);
  }
  display.showNumberDec(0, false);
}

void loop()
{
  timeClient.update();
  Serial.print("IST Time: ");
  // Serial.println(timeClient.getFormattedTime()); // HH:MM:SS format
  timestamp = timeClient.getEpochTime();
  Serial.println(timestamp);
  Serial.println(timeClient.getFormattedTime());

  Serial.println("=========================================");
  Serial.print("Motor Status:");
  Serial.println(motor_status);
  Serial.print("Motor Duration:");
  Serial.println(motor_duration);
  ot_sensorstatus = digitalRead(ot_sensor);
  ut_sensorstatus = digitalRead(ut_sensor);
  Serial.print("Motor Time:");
  Serial.println(motor_time);
  Serial.print("HighWater Status:");
  Serial.println(ot_sensorstatus);
  Serial.print("LowWater  Status:");
  Serial.println(ut_sensorstatus);

  if (motor_status == 1)
  {
    digitalWrite(outpin, HIGH);
    motor_time = motor_duration * 60;
    value_count = 15;
  }
  if (motor_status == 0)
  {
    digitalWrite(outpin, LOW);
    value_count = 0;
  }

  if (digitalRead(input1) == 0)
  {
    Serial.println("Button Clicked...!");
    if (digitalRead(outpin) == 0)
    {
      motor_status = 1;
      motor_time = motor_duration * 60;
      value_count = 15;
    }
    if (digitalRead(outpin) == 1)
    {
      Serial.println("Motor is now stopped..!");
      motor_status = 0;
      value_count = 0;
    }
  }

  if (ot_sensorstatus == 0)
  {
    digitalWrite(ot_status, HIGH);
    display.setSegments(seg_full);
  }
  else
  {
    digitalWrite(ot_status, LOW);
  }

  if (ut_sensorstatus == 0)
  {
    Serial.println(ut_sensorcount);
    Serial.println("Tank Empty");
    display.setSegments(seg_empty);
    digitalWrite(ut_status, HIGH);
    if (ut_sensorcount >= 20)
    {
      if (digitalRead(outpin) != 1)
      {
        Serial.println("Water tank is still empty, turning on the Motor");
        motor_time = (motor_duration) * 60;
        digitalWrite(outpin, HIGH);
        motor_status = 1;
      }
      ut_sensorcount = 0;
    }
    ut_sensorcount = ut_sensorcount + 1;
    delay(500);
  }
  else
  {
    ut_sensorcount = 0;
    delay(500);
    display.showNumberDec(0, false);
    digitalWrite(ut_status, LOW);
  }

  if (digitalRead(outpin) == 1)
  {
    motor_time = motor_time - 1;
    Serial.println("Motor Running..!");
    display.showNumberDec(1, false, 1, 0);
    display.showNumberDec((motor_time / 60) + 1, false);
    if (motor_time <= 0 || ot_sensorstatus == 0 || error_sensorstatus == 0)
    {
      Serial.print("Sensor Count:");
      Serial.println(ot_sensorcount);
      Serial.println("Tank Full or Timed Out..!");
      ot_sensorcount = ot_sensorcount + 1;
      if (ot_sensorcount >= 5)
      {
        digitalWrite(outpin, LOW);
        digitalWrite(auto_status, LOW);
        motor_status = 0;
        ot_sensorcount = 0;
        display.showNumberDec(0, false);
        motor_time = (motor_duration) * 60;
      }
      else
      {
        // ot_sensorcount=0;
      }
    }
    else
    {
      ot_sensorcount = 0;
    }
  }
  delay(500);
  count = count + 1;
}
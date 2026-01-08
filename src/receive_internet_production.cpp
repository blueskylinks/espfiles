#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <EEPROM.h>
#include <TM1637Display.h>
#include <ArduinoJson.h>  // For JSON parsing

// Pin definitions (make sure these pins exist on your board)
#define CLK D3
#define DIO D4
#define buzzer D2
#define input1 D5  // Changed from D9 (not available on ESP8266 boards)

// WiFi credentials
const char* ssid = "Airel_8600577773";
const char* password = "air10162";

// Firebase Realtime Database URL (HTTPS)
const char* firebaseUrl = "https://esp-02.asia-southeast1.firebasedatabase.app/users/uid/10103.json";

// Expected identification
const char* expectedNetworkID = "2012";
const char* expectedDeviceID = "01";

// Display setup
TM1637Display display(CLK, DIO);

// Segment patterns for display
const uint8_t seg_empty[] = {
  0x00,
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
  0x00
};

const uint8_t seg_full[] = {
  0x00,
  SEG_A | SEG_E | SEG_F | SEG_G,
  SEG_A | SEG_E | SEG_F | SEG_G,
  0x00
};

// Firebase root CA certificate (Google Internet Authority G3)
const char firebase_root_ca[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArOXG+CBikHKcc94uYK4f
9zDlD0m7q91rSv3h96Y1dpkO5/EHZElIFHvHJJpIBbdjI9+P5I+53kR5GGxVrqQ+
ylFZaIYy8g8UVpA5I9AZXtJoSHTV5qRPSmU+5y8QK9RWVBT7Ce0b+nPv6AY3cVrB
GxI6ixg8rlAyfWwHx5fDnITq2v56kFHtJnzjHqJS8rAAY6AxddXy0iRwWTI+GVQf
xur7MwUbvhZ66ZfqGfYTLPUPBTnEkHOyhwLQh0wqf44pXtFzLhDK9B4z6sdF+WwA
sd7o4rPzDJcCwZgWR0n7PlRURwTQ5rlGVCHVe3O+NMstjFuGPEI1RgO04h2pCQID
AQAB
-----END CERTIFICATE-----
)EOF";

// System state variables
int motor_status = 0;
int motor_duration = 30;
int motor_time = 0;
int sensor_status = 2;

int tcount1 = 0, tcount2 = 0, tcount3 = 0;
int temp_count1 = 0;
unsigned long lastCheck = 0;

bool buzzerOn = false;

void handleStates(int vstate1, int vstate2) {
  String status = String(vstate1) + String(vstate2);

  if (status == "00") {  // Tank full
    display.clear();
    display.setSegments(seg_full);
    tcount1++;
    if (tcount1 >= 3) {
      Serial.println("Tank Full Detected");
      digitalWrite(buzzer, LOW);
      buzzerOn = false;
      motor_status = 0;
      motor_time = motor_duration * 60;
      sensor_status = 0;
      tcount1 = 0;
    }
  } else {
    tcount1 = 0;
  }

  if (status == "11") {  // Tank empty
    display.clear();
    display.setSegments(seg_empty);
    tcount2++;
    if (tcount2 >= 3) {
      Serial.println("Tank Empty Detected");
      digitalWrite(buzzer, HIGH);
      buzzerOn = true;
      motor_status = 1;
      sensor_status = 1;
      tcount2 = 0;
    }
  } else {
    tcount2 = 0;
  }

  if (status == "22") {  // Water level normal
    tcount3++;
    if (tcount3 >= 3) {
      Serial.println("Water Level Normal");
      sensor_status = 2;
      tcount3 = 0;
    }
  } else {
    tcount3 = 0;
  }
}

void checkFirebaseStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    BearSSL::WiFiClientSecure client;
    BearSSL::X509List cert(firebase_root_ca);
    client.setTrustAnchors(&cert);

    HTTPClient http;
    http.begin(client, firebaseUrl);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Firebase response: " + payload);

      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        const char* netid = doc["networkid"];
        const char* devid = doc["deviceid"];
        int vstate1 = doc["vstate1"];
        int vstate2 = doc["vstate2"];

        if (String(netid) == expectedNetworkID && String(devid) == expectedDeviceID) {
          handleStates(vstate1, vstate2);
        } else {
          Serial.println("Ignored: Network ID or Device ID mismatch");
        }
      } else {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
      }

    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  pinMode(buzzer, OUTPUT);
  pinMode(input1, INPUT_PULLUP);
  digitalWrite(buzzer, LOW);
  buzzerOn = false;

  display.setBrightness(0x0f);
  display.clear();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  motor_duration = EEPROM.read(0);
  if (motor_duration < 1 || motor_duration > 180) {
    motor_duration = 30;
  }
  motor_time = motor_duration * 60;

  Serial.println("Receiver ready.");
}

void loop() {
  // Manual start/stop using button
  if (digitalRead(input1) == LOW) {
    Serial.println("Manual Button Pressed");
    buzzerOn = !buzzerOn;
    digitalWrite(buzzer, buzzerOn ? HIGH : LOW);

    motor_status = buzzerOn ? 1 : 0;
    if (buzzerOn) {
      motor_time = motor_duration * 60;
    } else {
      Serial.println("Motor stopped manually");
    }
    delay(1000);
  }

  // Handle motor timing
  if (motor_status == 1) {
    motor_time--;
    display.showNumberDec((motor_time / 60) + 1, false);
    if (motor_time <= 0 || sensor_status == 0) {
      temp_count1++;
      if (temp_count1 >= 5) {
        digitalWrite(buzzer, LOW);
        buzzerOn = false;
        motor_status = 0;
        motor_time = motor_duration * 60;
        temp_count1 = 0;
        display.clear();
      }
    } else {
      temp_count1 = 0;
    }
    delay(1000);
  } else {
    delay(500);
  }

  // Check Firebase every 10 seconds
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    checkFirebaseStatus();
  }
}

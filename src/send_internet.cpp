#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

const char* ssid = "Airel_8600577773";
const char* password = "air10162";

#define hsen D1
#define lsen D2

const char* networkid = "2012";
const char* deviceid = "01";

const char* serverName = "https://esp-02.asia-southeast1.firebasedatabase.app/users/uid/10103.json";

const long interval = 10000;
unsigned long previousMillis = 0;

int vstate1 = 2;
int vstate2 = 2;

int temp_count1 = 0;
int temp_count2 = 0;
int temp_count3 = 0;

void setup() {
  Serial.begin(115200);

  pinMode(hsen, INPUT_PULLUP);
  pinMode(lsen, INPUT_PULLUP);

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
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (digitalRead(hsen) == LOW) {
      temp_count1++;
      if (temp_count1 >= 3) {
        vstate1 = 0;
        vstate2 = 0;
        temp_count1 = 0;
        Serial.println("Tank Full");
      }
    } else {
      temp_count1 = 0;
    }

    if (digitalRead(lsen) == LOW) {
      temp_count2++;
      if (temp_count2 >= 3) {
        vstate1 = 1;
        vstate2 = 1;
        temp_count2 = 0;
        Serial.println("Tank Empty");
      }
    } else {
      temp_count2 = 0;
    }

    if (digitalRead(hsen) != LOW && digitalRead(lsen) != LOW) {
      temp_count3++;
      if (temp_count3 >= 10) {
        vstate1 = 2;
        vstate2 = 2;
        temp_count3 = 0;
        Serial.println("Water Level Normal");
      }
    } else {
      temp_count3 = 0;
    }

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setInsecure(); 

      HTTPClient https;
      https.begin(client, serverName);
      https.addHeader("Content-Type", "application/json");

      String jsonData = "{";
      jsonData += "\"networkid\":\"" + String(networkid) + "\",";
      jsonData += "\"deviceid\":\"" + String(deviceid) + "\",";
      jsonData += "\"vstate1\":" + String(vstate1) + ",";
      jsonData += "\"vstate2\":" + String(vstate2);
      jsonData += "}";

      Serial.println("Sending data: " + jsonData);

      int httpResponseCode = https.PUT(jsonData);  
      if (httpResponseCode > 0) {
        Serial.print("Firebase response: ");
        Serial.println(httpResponseCode);
        Serial.println(https.getString());
      } else {
        Serial.print("HTTPS error: ");
        Serial.println(httpResponseCode);
      }

      https.end();
    } else {
      Serial.println("WiFi not connected");
    }
  }
}

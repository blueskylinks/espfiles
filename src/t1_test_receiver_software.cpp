#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>

// SoftwareSerial on pins D5 (Rx) and D6 (Tx) [GPIO14, GPIO12]
SoftwareSerial mySerial(D5, D6); // RX, TX

// WiFi credentials
const char* ssid = "Anupam";
const char* password = "12345678";

// Firebase URL
const char* serverName = "https://esp-02.asia-southeast1.firebasedatabase.app/users/uid/10103.json";

int sdevice[8] = {0};
char motor_st = 'S';

void sendDataToFirebase();

void setup() {
  Serial.begin(115200);          // Main serial for debugging
  mySerial.begin(9600);          // SoftwareSerial for data input
  Serial.println("Receiver Ready");

  // Connect to WiFi
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
  // Check if data available on SoftwareSerial
  if (mySerial.available()) {
    String received = mySerial.readStringUntil('\n');
    received.trim();

    if (received.length() == 9) {
      for (int i = 0; i < 8; i++) {
        char c = received.charAt(i);
        sdevice[i] = (c >= '0' && c <= '9') ? c - '0' : 0;
      }
      motor_st = received.charAt(8);

      // Debug output
      Serial.print("Received sdevice: ");
      for (int i = 0; i < 8; i++) {
        Serial.print(sdevice[i]);
        Serial.print(" ");
      }
      Serial.print(" motor_st: ");
      Serial.println(motor_st);

      sendDataToFirebase();
    }
  }
}

void sendDataToFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // Skip SSL cert check

    HTTPClient https;
    https.begin(client, serverName);
    https.addHeader("Content-Type", "application/json");

    String jsonData = "{";
    jsonData += "\"sdevice\": [";
    for (int i = 0; i < 8; i++) {
      jsonData += String(sdevice[i]);
      if (i < 7) jsonData += ",";
    }
    jsonData += "],";
    jsonData += "\"motor_st\":\"" + String(motor_st) + "\"";
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

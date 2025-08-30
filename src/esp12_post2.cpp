#include <ESP8266WiFi.h>

const char* ssid = "Airtel_9764005401";
const char* password = "air46403";

const char* host = "https://esp-02.asia-southeast1.firebasedatabase.app/users/uid/10103.json";
//const int httpPort = 80;  // Use 443 for HTTPS and WiFiClientSecure

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  WiFiClient client;
  if (!client.connect(host,443)) {
    Serial.println("Connection failed");
    return;
  }

  // POST data
  String postData = "{\"temp\":25.6}";
  int contentLength = postData.length();

  // Construct HTTP POST request
  client.println("POST /api/data HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(contentLength));
  client.println("Connection: close");
  client.println(); // Blank line before body
  client.println(postData); // Body

  // Read response
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;  // Headers done
  }

  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  client.stop();
}

void loop() {
  // Nothing here
}
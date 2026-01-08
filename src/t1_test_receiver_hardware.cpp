#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

const char* ssid = "Anupam";
const char* password = "12345678";

const char* serverName = "https://anupam-32ea7-default-rtdb.firebaseio.com/user/uid/1011.json";

int sdevice[8] = {0};
int prev_sdevice[8] = {0};
String etime[8];
String ftime[8];
char motor_st = 'S';



void sendDataToFirebase();
void checkWiFiConnection();
String getCurrentTime();

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP8266 Receiver Starting...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi.");
  }

  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  for (int i = 0; i < 8; i++) {
    etime[i] = "0000";
    ftime[i] = "0000";
  }
}

void loop() {
  checkWiFiConnection();

  if (Serial.available()) {
    String received = Serial.readStringUntil('\n');
    received.trim();

    Serial.print("Received: ");
    Serial.println(received);
    Serial.print("Length: ");
    Serial.println(received.length());

    if (received.length() == 9) {
      bool stateChanged = false;

      for (int i = 0; i < 8; i++) {
        char c = received.charAt(i);
        if (c >= '0' && c <= '9') {
          int newVal = c - '0';
          if (newVal != sdevice[i]) {
            prev_sdevice[i] = sdevice[i];
            sdevice[i] = newVal;
            String currentTime = getCurrentTime();
            if (newVal == 1) etime[i] = currentTime;
            else ftime[i] = currentTime;
            stateChanged = true;
          }
        } else {
          Serial.println("Invalid character in input.");
        }
      }
      char newStatus = received.charAt(8);  
      if (newStatus != motor_st) {
        motor_st = newStatus;             
        stateChanged = true;                
        }

      motor_st = received.charAt(8);
      

      if (stateChanged) {
        Serial.print("State changed. motor_st: ");
        Serial.println(motor_st);
        sendDataToFirebase();
       } 
        else
       {
        Serial.println("No change in state.");
      }
    } else {
      Serial.println("Invalid data length.");
    }
  }
  delay(200);
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi.");
    } else {
      Serial.println("\nReconnection failed.");
    }
  }
}

String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  char timeStr[5];
  if (p_tm) {
    snprintf(timeStr, sizeof(timeStr), "%02d%02d", p_tm->tm_hour, p_tm->tm_min);
    return String(timeStr);
  } else {
    return "0000";
  }
}

void sendDataToFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, serverName);
    https.addHeader("Content-Type", "application/json");

    String jsonData = "{";
    jsonData += "\"motor_st\":\"" + String(motor_st) + "\",";
    jsonData += "\"sdevice\":[";

    for (int i = 0; i < 8; i++) {
      jsonData += "{";
      jsonData += "\"" + String(i) + "\":\"" + String(sdevice[i]) + "\",";
      jsonData += "\"etime\":\"" + etime[i] + "\",";
      jsonData += "\"ftime\":\"" + ftime[i] + "\"";
      jsonData += "}";
      if (i < 7) jsonData += ",";
    }

    jsonData += "]}";

    Serial.println("Sending to Firebase: " + jsonData);
    int httpResponseCode = https.PUT(jsonData);

    if (httpResponseCode > 0) {
      Serial.print("Firebase response code: ");
      Serial.println(httpResponseCode);
      Serial.println(https.getString());
    } else {
      Serial.print("Error sending to Firebase: ");
      Serial.println(httpResponseCode);
    }

    https.end();
  } else {
    Serial.println("WiFi not connected. Cannot send data.");
  }
}

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
const int mt_st = D8;
int mt_status = 0;  // This now represents duration in minutes (1–10), or 0 for OFF

// New motor timing variables
unsigned long mt_duration_ms = 0;
unsigned long mt_start_time = 0;
bool mt_running = false;

const int sled = D7;

void sendDataToFirebase();
void checkWiFiConnection();
String getCurrentTime();
void readMtStFromFirebase();

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
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

  pinMode(mt_st, OUTPUT);
  digitalWrite(mt_st, LOW); // Initially OFF

  pinMode(sled, OUTPUT);
  digitalWrite(sled, LOW);
}

void loop() {
  checkWiFiConnection();

  if (Serial.available()) {
    String received = Serial.readStringUntil('\n');
    received.trim();

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
        }
      }

      char newStatus = received.charAt(8);
      if (newStatus != motor_st) {
        motor_st = newStatus;
        stateChanged = true;
      }

      if (stateChanged) {
        Serial.print("State changed. motor_st: ");
        Serial.println(motor_st);
        sendDataToFirebase();
      } else {
        Serial.print("no change");
        readMtStFromFirebase();
      }
    }
  }

  // Blink LED if connected
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(sled, HIGH); delay(2000);
    digitalWrite(sled, LOW); delay(2000);
    
  }

  // Check if motor run time is over
  if (mt_running && (millis() - mt_start_time >= mt_duration_ms)) {
    Serial.println("Motor run time completed. Turning off.");

    digitalWrite(mt_st, LOW);
    motor_st = 'S';
    mt_status = 0;
    mt_running = false;

    sendDataToFirebase();
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
    jsonData += "\"mt_status\":\"" + String(mt_status) + "\",";
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

void readMtStFromFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, serverName);
    int httpCode = https.GET();

    if (httpCode > 0) {
      String payload = https.getString();
      Serial.println("Firebase GET: " + payload);

      int index = payload.indexOf("\"mt_status\":");
      if (index != -1) {
        int startIndex = index + 12;
        int endIndex = payload.indexOf(",", startIndex);
        if (endIndex == -1) endIndex = payload.indexOf("}", startIndex);

        String mt_value_str = payload.substring(startIndex, endIndex);
        mt_value_str.replace("\"", "");
        mt_value_str.trim();

        int new_mt_status = mt_value_str.toInt();
        Serial.print("Parsed mt_status value: ");
        Serial.println(new_mt_status);

        if (new_mt_status >= 1 && new_mt_status <= 10) {
          digitalWrite(mt_st, HIGH);
          motor_st = 'R';
          mt_status = new_mt_status;
          mt_duration_ms = new_mt_status * 60UL * 1000UL; // Convert to ms
          mt_start_time = millis();
          mt_running = true;

          Serial.print("Motor started for ");
          Serial.print(mt_status);
          Serial.println(" minutes.");

          sendDataToFirebase();
        } else if (new_mt_status == 0) {
          digitalWrite(mt_st, LOW);
          motor_st = 'S';
          mt_status = 0;
          mt_running = false;

          Serial.println("Motor turned OFF from Firebase (mt_status=0).");

          sendDataToFirebase();
        } else {
          Serial.println("Invalid mt_status value. Ignored.");
        }
      } else {
        Serial.println("mt_status field not found.");
      }
    } else {
      Serial.print("Firebase GET failed, error: ");
      Serial.println(httpCode);
    }

    https.end();
  }
}

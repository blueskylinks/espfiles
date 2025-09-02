#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

const char* ssid = "Anupam";
const char* password = "12345678";
const char* serverName = "https://skyiot-597d5-default-rtdb.firebaseio.com/uid/motor.json";
const char* motorServerName = "https://flutterapp-75a79-default-rtdb.firebaseio.com/uid/motor.json";
const char* statusServerName = "https://skyiot-597d5-default-rtdb.firebaseio.com/uid/status.json";

int sdevice[8] = {0};
int prev_sdevice[8] = {0};
String etime[8];
String ftime[8];

char motor_st = 'S';
char prev_motor_st = 'S';

const int mt_st = D8;
int mt_status = 0;

const int sled = D7;

unsigned long mt_last_active_time = 0;
const unsigned long MT_TIMEOUT = 5 * 60 * 1000;

int wificonnect = 0;

unsigned long lastSerialReceivedTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastGetTime = 0;
const unsigned long SEND_INTERVAL = 5000;
const unsigned long GET_INTERVAL = 20000;

String motorOnTime = "0000-00-00 00:00:00";
String motorOffTime = "0000-00-00 00:00:00";

const unsigned long HEARTBEAT_INTERVAL = 30000;
unsigned long lastHeartbeatTime = 0;

void sendDataToFirebase();
void checkWiFiConnection();
String getCurrentTime();
void readMotorStatusFromFirebase();
void readInitialFirebaseData();
void updateDeviceInFirebase(int deviceIndex);
void sendHeartbeat();
bool hasDeviceOrMotorStateChanged();

void setup() {
  Serial.begin(9600);

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

  pinMode(mt_st, OUTPUT);
  digitalWrite(mt_st, mt_status);

  pinMode(sled, OUTPUT);
  digitalWrite(sled, LOW);

  readInitialFirebaseData();
  readMotorStatusFromFirebase();

  if (mt_status == 1) {
    mt_last_active_time = millis();
  }
}

void loop() {
  checkWiFiConnection();
  wificonnect = (WiFi.status() == WL_CONNECTED) ? 1 : 0;

  if (Serial.available()) {
    delay(100);
    String received = Serial.readStringUntil('\r');
    received.trim();

    bool valid = (received.length() == 9);
    if (valid) {
      for (int i = 0; i < 8; i++) {
        char c = received.charAt(i);
        if (c != '0' && c != '1') {
          valid = false;
          break;
        }
      }
      char lastChar = received.charAt(8);
      if (lastChar != 'R' && lastChar != 'S') {
        valid = false;
      }
    }

    if (valid) {
      lastSerialReceivedTime = millis();
      bool stateChanged = false;

      for (int i = 0; i < 8; i++) {
        int newVal = received.charAt(i) - '0';
        if (newVal != sdevice[i]) {
          sdevice[i] = newVal;
          String currentTime = getCurrentTime();
          if (newVal == 1) etime[i] = currentTime;
          else ftime[i] = currentTime;
          updateDeviceInFirebase(i);
        }
      }

      char newStatus = received.charAt(8);
      if (newStatus != motor_st) {
        motor_st = newStatus;
        mt_status = (motor_st == 'R') ? 1 : 0;
        digitalWrite(mt_st, mt_status);
        mt_last_active_time = millis();

        if (prev_motor_st == 'S' && motor_st == 'R') {
          motorOnTime = getCurrentTime();
        } else if (prev_motor_st == 'R' && motor_st == 'S') {
          motorOffTime = getCurrentTime();
        }
      }

      if (hasDeviceOrMotorStateChanged()) {
        sendDataToFirebase();
        lastSendTime = millis();
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(sled, HIGH);
    delay(200);
    digitalWrite(sled, LOW);
    delay(200);
  }

  if (mt_status == 1 && (millis() - mt_last_active_time) > MT_TIMEOUT) {
    Serial.println("Motor timeout reached. Turning off motor.");

    mt_status = 0;
    motor_st = 'S';
    digitalWrite(mt_st, mt_status);

    motorOffTime = getCurrentTime();

    if (hasDeviceOrMotorStateChanged()) {
      sendDataToFirebase();
      lastSendTime = millis();
    }
  }

  if ((millis() - lastGetTime) >= GET_INTERVAL) {
    readMotorStatusFromFirebase();
    lastGetTime = millis();
  }

  if ((millis() - lastHeartbeatTime) >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeatTime = millis();
  }

  delay(500);
}

bool hasDeviceOrMotorStateChanged() {
  for (int i = 0; i < 8; i++) {
    if (sdevice[i] != prev_sdevice[i]) {
      return true;
    }
  }
  return motor_st != prev_motor_st;
}

String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  char timeStr[64];
  if (p_tm) {
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             p_tm->tm_year + 1900,
             p_tm->tm_mon + 1,
             p_tm->tm_mday,
             p_tm->tm_hour,
             p_tm->tm_min,
             p_tm->tm_sec);
    return String(timeStr);
  } else {
    return "0000-00-00 00:00:00";
  }
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

void sendHeartbeat() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, statusServerName);
    https.addHeader("Content-Type", "application/json");

    String currentTime = getCurrentTime();
    String jsonData = "{\"last_active\":\"" + currentTime + "\"}";

    Serial.println("Sending heartbeat: " + jsonData);
    int httpResponseCode = https.PUT(jsonData);

    if (httpResponseCode > 0) {
      Serial.print("Heartbeat response: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Failed to send heartbeat: ");
      Serial.println(httpResponseCode);
    }

    https.end();
  }
}

void sendDataToFirebase() {
  if (!hasDeviceOrMotorStateChanged()) {
    Serial.println("No state change. Skipping Firebase update.");
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, serverName);
    https.addHeader("Content-Type", "application/json");

    String jsonData = "{";
    jsonData += "\"motor_st\":\"" + String(motor_st) + "\",";
    jsonData += "\"motor_on_time\":\"" + motorOnTime + "\",";
    jsonData += "\"motor_off_time\":\"" + motorOffTime + "\",";
    jsonData += "\"sdevice\":[";

    for (int i = 0; i < 8; i++) {
      jsonData += "{";
      jsonData += "\"state\":\"" + String(sdevice[i]) + "\",";
      jsonData += "\"etime\":\"" + etime[i] + "\",";
      jsonData += "\"ftime\":\"" + ftime[i] + "\"";
      jsonData += "}";
      if (i < 7) jsonData += ",";
    }

    jsonData += "]}";

    Serial.println("Sending Firebase update: " + jsonData);
    int httpResponseCode = https.PUT(jsonData);

    if (httpResponseCode > 0) {
      Serial.print("Firebase response: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error sending to Firebase: ");
      Serial.println(httpResponseCode);
    }

    https.end();

    for (int i = 0; i < 8; i++) {
      prev_sdevice[i] = sdevice[i];
    }
    prev_motor_st = motor_st;
  }
}

void updateDeviceInFirebase(int deviceIndex) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    String deviceUrl = String("https://skyiot-597d5-default-rtdb.firebaseio.com/uid/motor/sdevice/") + deviceIndex + ".json";
    https.begin(client, deviceUrl);
    https.addHeader("Content-Type", "application/json");

    String jsonData = "{";
    jsonData += "\"state\":\"" + String(sdevice[deviceIndex]) + "\",";
    jsonData += "\"etime\":\"" + etime[deviceIndex] + "\",";
    jsonData += "\"ftime\":\"" + ftime[deviceIndex] + "\"";
    jsonData += "}";

    Serial.println("Updating device " + String(deviceIndex) + ": " + jsonData);
    int httpResponseCode = https.PUT(jsonData);

    if (httpResponseCode > 0) {
      Serial.print("Update response: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error updating device: ");
      Serial.println(httpResponseCode);
    }

    https.end();
  }
}

void readMotorStatusFromFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, motorServerName);
    int httpCode = https.GET();

    if (httpCode > 0) {
      String payload = https.getString();
      //Serial.println("Motor GET: " + payload);

      int index = payload.indexOf("\"mt_status\":");
      if (index != -1) {
        int startIndex = index + 12;
        int endIndex = payload.indexOf(",", startIndex);
        if (endIndex == -1) endIndex = payload.indexOf("}", startIndex);

        String mt_value_str = payload.substring(startIndex, endIndex);
        mt_value_str.replace("\"", "");
        mt_value_str.trim();

        int new_mt_status = mt_value_str.toInt();
        if (new_mt_status != mt_status) {
          mt_status = new_mt_status;
          motor_st = (mt_status == 1) ? 'R' : 'S';
          digitalWrite(mt_st, mt_status);
          mt_last_active_time = millis();

          if (motor_st == 'R') motorOnTime = getCurrentTime();
          else motorOffTime = getCurrentTime();
        }
      }
    } else {
      Serial.print("Motor GET failed: ");
      Serial.println(httpCode);
    }

    https.end();
  }
}

void readInitialFirebaseData() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, serverName);
    int httpCode = https.GET();

    if (httpCode > 0) {
      String payload = https.getString();
      Serial.println("Initial data: " + payload);

      int motOnIdx = payload.indexOf("\"motor_on_time\":\"");
      if (motOnIdx != -1) {
        int start = motOnIdx + 16;
        int end = payload.indexOf("\"", start);
        motorOnTime = payload.substring(start, end);
      }

      int motOffIdx = payload.indexOf("\"motor_off_time\":\"");
      if (motOffIdx != -1) {
        int start = motOffIdx + 17;
        int end = payload.indexOf("\"", start);
        motorOffTime = payload.substring(start, end);
      }
    } else {
      Serial.print("Error getting initial data: ");
      Serial.println(httpCode);
    }

    https.end();
  }
}
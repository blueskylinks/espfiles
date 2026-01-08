#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

const char* ssid = "TI IndustriesExt";
const char* password = "9845574336";
const char* serverName = "https://skyiot-597d5-default-rtdb.firebaseio.com/uid/motor.json";

const char* motorServerName = "https://flutterapp-75a79-default-rtdb.firebaseio.com/uid/motor.json";

int sdevice[8] = {0};
int prev_sdevice[8] = {0};
String etime[8];
String ftime[8];

char motor_st = 'S';
const int mt_st = D8;
int mt_status = 0;
String lastReceived = "";
const int sled = D7;

unsigned long mt_last_active_time = 0;
const unsigned long MT_TIMEOUT = 5 * 60 * 1000;

int power = 0;
int wificonnect = 0;

unsigned long lastSerialReceivedTime = 0;

unsigned long lastSendTime = 0;
unsigned long lastGetTime = 0;
const unsigned long SEND_INTERVAL = 5000;
const unsigned long GET_INTERVAL = 20000;

String powerOnTime = "0000-00-00 00:00:00";
String powerOffTime = "0000-00-00 00:00:00";

void sendDataToFirebase();
void checkWiFiConnection();
String getCurrentTime();
void readMotorStatusFromFirebase();
void readInitialFirebaseData();
void updateDeviceInFirebase(int deviceIndex);
void sendMotorStatusToFirebase();

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
  wificonnect = WiFi.status() == WL_CONNECTED ? 1 : 0;

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
              if (power == 0) {
           power = 1;
           powerOnTime = getCurrentTime();
           sendDataToFirebase();
          }
      lastSerialReceivedTime = millis();
      
      String currentReceived = received;
      if (currentReceived != lastReceived) {
        lastSerialReceivedTime = millis();
        if (power == 0) {
           power = 1;
           powerOnTime = getCurrentTime();
           sendDataToFirebase();
          }
          bool stateChanged = false;
          for (int i = 0; i < 8; i++) {
            int newVal = received.charAt(i) - '0';
            if (newVal != sdevice[i]) {
              prev_sdevice[i] = sdevice[i];
              sdevice[i] = newVal;
              String currentTime = getCurrentTime();
              if (newVal == 1) etime[i] = currentTime;
              else ftime[i] = currentTime;
              stateChanged = true;
              updateDeviceInFirebase(i);
            }
          }
          char newStatus = received.charAt(8);
          if (newStatus != motor_st) {
            motor_st = newStatus;
            mt_status = (motor_st == 'R') ? 1 : 0;
            digitalWrite(mt_st, mt_status); 
            mt_last_active_time = millis();
            //sendMotorStatusToFirebase();
            stateChanged = true;
          }
          if (stateChanged) {
            Serial.print("State changed. motor_st: ");
            Serial.println(motor_st);
            sendDataToFirebase();
            lastSendTime = millis();
          }
          //Serial.println("valid UART input: " + received);
          lastReceived = currentReceived;
        }
        Serial.println("valid UART input: " + received);
    } else {
     // Serial.println("Ignored invalid UART input: " + received);
    }
  }



  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(sled, HIGH);
    delay(200);
    digitalWrite(sled, LOW);
    delay(200);
  }

  if (mt_status == 1 && millis() - mt_last_active_time > MT_TIMEOUT) {
    Serial.println("Motor timeout reached. Turning off mt_status.");
    mt_status = 0;
    motor_st = 'S';
    digitalWrite(mt_st, mt_status);
    //sendMotorStatusToFirebase();
    lastSendTime = millis();
  }

  if (millis() - lastSendTime >= SEND_INTERVAL) {
    //sendDataToFirebase();
    lastSendTime = millis();
  }

  if (millis() - lastGetTime >= GET_INTERVAL) {
    readMotorStatusFromFirebase();
    lastGetTime = millis();
  }

  delay(500);
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

void sendDataToFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, serverName);
    https.addHeader("Content-Type", "application/json");

    String jsonData = "{";
    jsonData += "\"power\":\"" + String(power) + "\"," ;
    jsonData += "\"power_on_time\":\"" + powerOnTime + "\"," ;
    jsonData += "\"power_off_time\":\"" + powerOffTime + "\"," ;
    jsonData += "\"wificonnect\":\"" + String(wificonnect) + "\"," ;
    jsonData += "\"motor_st\":\"" + String(motor_st) + "\"," ;
    jsonData += "\"sdevice\":[";

    for (int i = 0; i < 8; i++) {
      jsonData += "{";
      jsonData += "\"" + String(i) + "\":\"" + String(sdevice[i]) + "\"," ;
      jsonData += "\"etime\":\"" + etime[i] + "\"," ;
      jsonData += "\"ftime\":\"" + ftime[i] + "\"" ;
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

void sendMotorStatusToFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, motorServerName);
    https.addHeader("Content-Type", "application/json");

    String jsonData = "{";
    jsonData += "\"mt_status\":\"" + String(mt_status) + "\",";
    
    jsonData += "}";

    Serial.println("Sending motor status to separate Firebase: " + jsonData);
    int httpResponseCode = https.PUT(jsonData);

    if (httpResponseCode > 0) {
      Serial.print("Motor Firebase response code: ");
      Serial.println(httpResponseCode);
      Serial.println(https.getString());
    } else {
      Serial.print("Error sending motor status: ");
      Serial.println(httpResponseCode);
    }

    https.end();
  } else {
    Serial.println("WiFi not connected. Cannot send motor status.");
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
    jsonData += "\"" + String(deviceIndex) + "\":\"" + String(sdevice[deviceIndex]) + "\"," ;
    jsonData += "\"etime\":\"" + etime[deviceIndex] + "\"," ;
    jsonData += "\"ftime\":\"" + ftime[deviceIndex] + "\"" ;
    jsonData += "}";

    Serial.println("Updating device " + String(deviceIndex) + " in Firebase: " + jsonData);

    int httpResponseCode = https.PUT(jsonData);

    if (httpResponseCode > 0) {
      Serial.print("Firebase update response code: ");
      Serial.println(httpResponseCode);
      Serial.println(https.getString());
    } else {
      Serial.print("Error updating device in Firebase: ");
      Serial.println(httpResponseCode);
    }

    https.end();
  } else {
    Serial.println("WiFi not connected. Cannot update device.");
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
      Serial.println("Motor status GET: " + payload);

      int index = payload.indexOf("\"mt_status\":");
      if (index != -1) {
        int startIndex = index + 12;
        int endIndex = payload.indexOf(",", startIndex);
        if (endIndex == -1) {
          endIndex = payload.indexOf("}", startIndex);
        }

        String mt_value_str = payload.substring(startIndex, endIndex);
        mt_value_str.replace("\"", "");
        mt_value_str.trim();

        int new_mt_status = mt_value_str.toInt();
        digitalWrite(mt_st, new_mt_status);

        if (new_mt_status != mt_status) {
          mt_status = new_mt_status;
          motor_st = (mt_status == 1) ? 'R' : 'S';
          Serial.print("Updated mt_status to: ");
          Serial.println(mt_status);
          //sendMotorStatusToFirebase();

          if (mt_status == 1) {
            mt_last_active_time = millis();
          }
        }
      }
    } else {
      Serial.print("Motor Firebase GET failed, error: ");
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
      Serial.println("Initial Firebase data: " + payload);

      int ponIndex = payload.indexOf("\"power_on_time\":\"");
      if (ponIndex != -1) {
        int start = ponIndex + 18;
        int end = payload.indexOf("\"", start);
        powerOnTime = payload.substring(start, end);
      }

      int poffIndex = payload.indexOf("\"power_off_time\":\"");
      if (poffIndex != -1) {
        int start = poffIndex + 19;
        int end = payload.indexOf("\"", start);
        powerOffTime = payload.substring(start, end);
      }

      for (int i = 0; i < 8; i++) {
        String idxStr = "\"" + String(i) + "\":\"";
        int devIndex = payload.indexOf(idxStr);
        if (devIndex != -1) {
          int valStart = devIndex + idxStr.length();
          int valEnd = payload.indexOf("\"", valStart);
          sdevice[i] = payload.substring(valStart, valEnd).toInt();

          String etimeKey = "\"etime\":\"";
          int etimeIndex = payload.indexOf(etimeKey, valEnd);
          if (etimeIndex != -1) {
            int etimeStart = etimeIndex + etimeKey.length();
            int etimeEnd = payload.indexOf("\"", etimeStart);
            etime[i] = payload.substring(etimeStart, etimeEnd);
          }

          String ftimeKey = "\"ftime\":\"";
          int ftimeIndex = payload.indexOf(ftimeKey, valEnd);
          if (ftimeIndex != -1) {
            int ftimeStart = ftimeIndex + ftimeKey.length();
            int ftimeEnd = payload.indexOf("\"", ftimeStart);
            ftime[i] = payload.substring(ftimeStart, ftimeEnd);
          }
        }
      }
    } else {
      Serial.print("Error getting initial Firebase data: ");
      Serial.println(httpCode);
    }

    https.end();
  }
}

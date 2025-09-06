/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at our blog: https://RandomNerdTutorials.com/esp32-esp8266-firebase-bme280-rtdb/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <ArduinoJson.h>

// Network and Firebase credentials
#define WIFI_SSID "Airtel_9764005401"
#define WIFI_PASSWORD "air46403"

#define Web_API_KEY "AIzaSyBSypmrtk19MJBkQ-UcMsuY8KFYQLlk8xw"
#define DATABASE_URL "https://esp-02-userdata.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "u9764005401@gmail.com"
#define USER_PASSWORD "admin123"

// User function
void processData(AsyncResult &aResult);
void processData1(AsyncResult &aResult);

// Authentication
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASSWORD);

// Firebase components
FirebaseApp app;
WiFiClientSecure ssl_client, stream_ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client), streamClient(stream_ssl_client);
RealtimeDatabase Database;

// Timer variables for sending data every 10 seconds
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000;

// Variable to save USER UID
String uid;

// Variables to save database paths
String databasePath;
String tempPath;
String humPath;
String presPath;

float temperature;
float humidity;
float pressure;

// Database  path (where the data is)
String listenerPath = "/UserData";

void setup()
{
    Serial.begin(115200);

    // Connect to Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();

    ssl_client.setInsecure();
#if defined(ESP32)
    ssl_client.setConnectionTimeout(1000);
    ssl_client.setHandshakeTimeout(5);
#elif defined(ESP8266)
    ssl_client.setTimeout(1000);           // Set connection timeout
    ssl_client.setBufferSizes(4096, 1024); // Set buffer sizes
#endif

    // Initialize Firebase
    initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);

    // Set a database listener
    streamClient.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");
    Database.get(streamClient, listenerPath, processData1, true /* SSE mode (HTTP Streaming) */, "streamTask");
}

void loop()
{
    // Maintain authentication and async tasks
    app.loop();

    // Check if authentication is ready
    if (app.ready())
    {

        // Periodic data sending every 10 seconds
        unsigned long currentTime = millis();
        if (currentTime - lastSendTime >= sendInterval)
        {
            // Update the last send time
            lastSendTime = currentTime;

            // Get User UID
            Firebase.printf("User UID: %s\n", app.getUid().c_str());
            uid = app.getUid().c_str();
            databasePath = "UsersData/" + uid;

            // Update database path for sensor readings
            tempPath = databasePath + "/temperature"; // --> UsersData/<user_uid>/temperature
            humPath = databasePath + "/humidity";     // --> UsersData/<user_uid>/humidity
            presPath = databasePath + "/pressure";    // --> UsersData/<user_uid>/pressure

            // Get latest sensor readings
            temperature = 23.2;
            humidity = 33.2;
            pressure = 8.5;

            Serial.println("Writing to: " + tempPath);

            Database.set<float>(aClient, tempPath, temperature, processData, "RTDB_Send_Temperature");
            Database.set<float>(aClient, humPath, humidity, processData, "RTDB_Send_Humidity");
            Database.set<float>(aClient, presPath, pressure, processData, "RTDB_Send_Pressure");
        }
    }
}

void processData(AsyncResult &aResult)
{
    if (!aResult.isResult())
        return;

    if (aResult.isEvent())
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());

    if (aResult.isDebug())
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());

    if (aResult.isError())
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());

    if (aResult.available())
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}

void processData1(AsyncResult &aResult)
{
    // Exits when no result available when calling from the loop.
    if (!aResult.isResult())
        return;

    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    // When it receives data from the database
    if (aResult.available())
    {
        RealtimeDatabaseResult &RTDB = aResult.to<RealtimeDatabaseResult>();
        Serial.println("Data update on reltime....");
        // we received data from the streaming client
        if (RTDB.isStream())
        {
            Serial.println("----------------------------");
            Firebase.printf("task: %s\n", aResult.uid().c_str());
            Firebase.printf("event: %s\n", RTDB.event().c_str());
            Firebase.printf("path: %s\n", RTDB.dataPath().c_str());
            Firebase.printf("etag: %s\n", RTDB.ETag().c_str());
            Firebase.printf("data: %s\n", RTDB.to<const char *>());
            Firebase.printf("type: %d\n", RTDB.type());

            // RTDB.type = 6 means the result is a JSON : https://github.com/mobizt/FirebaseClient/blob/main/resources/docs/realtime_database_result.md#--realtime_database_data_type-type
            // You receive a JSON when you initialize the stream
            if (RTDB.type() == 6)
            {
                Serial.println(RTDB.to<String>());
                // Parse JSON
                DynamicJsonDocument doc(512);
                DeserializationError error = deserializeJson(doc, RTDB.to<String>());
                if (error)
                {
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    return;
                }
                // Iterate through JSON object
                for (JsonPair kv : doc.as<JsonObject>())
                {
                    int gpioPin = atoi(kv.key().c_str()); // Convert key (e.g., "12") to int
                    bool state = kv.value().as<bool>();
                    digitalWrite(gpioPin, state ? HIGH : LOW);
                }
            }

            // RTDB.type() = 4 means the result is a boolean
            // RTDB.type() = 1 means the result is an integer
            // learn more here: https://github.com/mobizt/FirebaseClient/blob/main/resources/docs/realtime_database_result.md#--realtime_database_data_type-type
            if (RTDB.type() == 4 || RTDB.type() == 1)
            {
                // get the GPIO number
                int GPIO_number = RTDB.dataPath().substring(1).toInt();
                bool state = RTDB.to<bool>();
                digitalWrite(GPIO_number, state);
                Serial.println("Updating GPIO State");
            }

            // The stream event from RealtimeDatabaseResult can be converted to values as following.
            /*bool v1 = RTDB.to<bool>();
            int v2 = RTDB.to<int>();
            float v3 = RTDB.to<float>();
            double v4 = RTDB.to<double>();
            String v5 = RTDB.to<String>();
            Serial.println(v5); */
        }
        else
        {
            Serial.println("----------------------------");
            Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
        }
    }
}
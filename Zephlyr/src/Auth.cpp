#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include "Auth.h"

ServiceAuth sa_auth(timeStatusCB, FIREBASE_CLIENT_EMAIL, FIREBASE_PROJECT_ID, PRIVATE_KEY, 3000 /* expire period in seconds (<3600) */);

WiFiClientSecure ssl_client;
DefaultNetwork network;
AsyncClient aClient(ssl_client, getNetwork(network));
FirebaseApp app;

String schedulePayload = "";


int targethour = -1;
int targetminute = -1;
bool targetdaily = false;

void initializeFirebase(){
  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
  ssl_client.setInsecure();
  Serial.println("Initializing the app...");
  initializeApp(aClient, app, getAuth(sa_auth), asyncCB, "mengotentikasi...");
  app.getApp<RealtimeDatabase>(Database);
  app.getApp<Messaging>(messaging);
  Database.url(DATABASE_URL);
}

void timeStatusCB(uint32_t &ts){
#if defined(ESP8266) || defined(ESP32) || defined(CORE_ARDUINO_PICO)
    if (time(nullptr) < FIREBASE_DEFAULT_TS)
    {

        configTime(7 * 3600, 0, "pool.ntp.org");
        while (time(nullptr) < FIREBASE_DEFAULT_TS)
        {
            delay(100);
        }
    }
    ts = time(nullptr);
#elif __has_include(<WiFiNINA.h>) || __has_include(<WiFi101.h>)
    ts = WiFi.getTime();
#endif
}

void printResult(AsyncResult &aResult){
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available()) {
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
        RealtimeDatabaseResult &RTDB = aResult.to<RealtimeDatabaseResult>();
        String TaskID = aResult.uid();
        String data = aResult.c_str();
        if (TaskID == "getTask1") {
            schedulePayload = data;  // Menyimpan payload JSON mentah
            Firebase.printf("task: %s, payload: %s\n", TaskID.c_str(), schedulePayload.c_str());

            // Parse JSON payload
            DynamicJsonDocument doc(512);
            DeserializationError error = deserializeJson(doc, schedulePayload);
            if (!error) {
                if (doc.containsKey("hour")) {
                    targethour = doc["hour"];  // Ambil nilai "hour"
                }
                if (doc.containsKey("minute")) {
                    targetminute = doc["minute"];  // Ambil nilai "minute"
                }
                if (doc.containsKey("daily")) {
                    targetdaily = doc["daily"];  // Ambil nilai "daily"
                }
            } else {
                Serial.println("Failed to parse JSON for getTask1");
            }
        }

        // Tangani stream jika data datang melalui stream Firebase
        if(RTDB.isStream()){
            String path = RTDB.dataPath();
            String eventData = RTDB.to<String>();
            if (path == "/") {  // Pastikan pathnya adalah "/"
                DynamicJsonDocument doc(512);
                DeserializationError error = deserializeJson(doc, eventData);
                if (!error) {
                    if (doc.containsKey("hour")) {
                        targethour = doc["hour"];
                    }
                    if (doc.containsKey("minute")) {
                        targetminute = doc["minute"];
                    }
                    if (doc.containsKey("daily")) {
                        targetdaily = doc["daily"];
                    }
                } else {
                    Serial.println("Failed to parse JSON from stream");
                }
            }
        }
    }
  }

void asyncCB(AsyncResult &aResult) { 
  printResult(aResult); 
  }

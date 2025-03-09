#ifndef AUTH_H
#define AUTH_H

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DATABASE_URL "your_database_url"
#define DATABASE_SECRET "your_database_secret"
#define FIREBASE_CLIENT_EMAIL "firebaseadminSDK"
#define FIREBASE_PROJECT_ID "your_project_ID"

const char PRIVATE_KEY[] PROGMEM = R"EOF(
-----BEGIN PRIVATE KEY-----

-----END PRIVATE KEY-----
)EOF";

using AsyncClient = AsyncClientClass; 

extern FirebaseApp app;
extern ServiceAuth sa_auth;
extern WiFiClientSecure ssl_client;
extern DefaultNetwork network;
extern AsyncClient aClient;
extern RealtimeDatabase Database;
extern Messaging messaging;

extern WiFiUDP ntpUDP;
extern NTPClient timeClient;

void initializeFirebase();

void timeStatusCB(uint32_t &ts);

void asyncCB(AsyncResult &aResult);

void printResult(AsyncResult &aResult);

void updateData();

void getMsg(Messages::Message &msg);

void sendDataToGoogleSheet(JsonArray &readings);

void fetchTemperatureData();

void filterTemperatureData(const String& temperaturePayload);

String getDefaultDate();

String getISO8601Time(int hour, int minute, int second);

#endif

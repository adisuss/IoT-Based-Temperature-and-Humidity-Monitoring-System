#ifndef AUTH_H
#define AUTH_H

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DATABASE_URL "https://zephlyr-134d7-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define DATABASE_SECRET "7pLl7qHpkPBpmXPGRbYjhABqRfgHuL93pYbNUFT2"
#define FIREBASE_CLIENT_EMAIL "firebase-adminsdk-fbsvc@zephlyr-134d7.iam.gserviceaccount.com"
#define FIREBASE_PROJECT_ID "zephlyr-134d7"

const char PRIVATE_KEY[] PROGMEM = R"EOF(
-----BEGIN PRIVATE KEY-----
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDua7s+A5kq+4v7
TPBwoxwVoFqxEo58GZB4kB1JLYseeWxH2WW+Sg0pKK+odkxF1Pz2b6vlpCieulbJ
8LcMuV+lbtoxIRY36J0Gwczy+Z3PxFprKmbsu/FKU1RiESQ/huEf99jx7zt9Ztw1
m3bcoo4Ii2Gaittz8TMCDeGE/HzdZ2SB41O2DNE+OuSP+YmtCc2zAxDGTDqBb47M
Ip8BmQu3Dc4nbNUpIzkkL3wtfS/Yut+R29PfU9mtuR0RtyqqU8FDh9TfB8Cw5kU6
skNIXsI7CZqvnZhPX6QNVVpGJCMP3EWsDqLNyCPmFC5XFVAR3GZ9Klf/14qHcA6h
LvBw3nfVAgMBAAECggEAEWIQ7ZFRGFFtUBX4vNmjmU8O9bSa52Dsx+GIBRcZOtBT
DP0EfkaDZaIWBpQTdQ71n5keQRjCLmp7G++dRQP5/YOSzHyzVEG1OzIjmX6KG0Uf
a4tUpEOCsO7Y6uqBiFEy9kP5kRAmd/rpKM5sMON4NBbHd79Sh0ZJeAefjvTivLii
U9KMg2IAYuu921WsfgGSsVuvgl75FcPvEtGcgZLAebJp2QAsmzSobyxGqFM3dJtL
nbny+6vwMxS0ng2zsYZNTqvqPnsjAsyhun57Lxj2busFpkDsULbirA4FViCG/Xh5
HTEVFdg7bup6oeQGL0wRg1sjYhf1i20Mq8yXwH0bgQKBgQD6KDtYHl86AcwjyK+k
xcKMlqIdLSLR+uz6/6UYFlQEY+UIntbKvpN7OuXvtzRMAQw9URQp6htQjkFy8Sdc
IGbxHrsBnRFotAUEA2kOG7N5yK1yuyjgOwdy3FTX4bk+OGQy4VJhr0QLkaXEXaiS
elMkHETf1qcegyg59VlptneRgQKBgQDz/VML80QBL9y3bOQHSiZH7p4rWsX5MeYk
zWeVZsaIZ+LCN2dbVgOo3WwdRdhkV0KcyYCtgDcAkd7y2zQqP7jvAUfXKTCRZeUS
D++9TqDjOJgM8BJ7zIqxlvlIS2DGb/ZEcrnh1ohreNPbk3YIMV3K3h9VB4L5rZbj
9kAj1e8oVQKBgQDrQ1kEd5PxVu1pHf1qRn7Af445SFC+EHI9YJ4guCcN9fDZmaDC
DldfhrXnK3JopHehVxZSkRRdP1x4QCpXLzYBQHh5fQF5agxpiNeNaCnzt/K/uxsn
PvyzXloAqg3wYVKCs3wp2I5zHug6dCbsk1SL2nY/2X1Uad80GvWbQPrUgQKBgQDu
edQKAiOd5WIeBlh6p4bzF8+RIJAQGS2RGxL7fBDgkmmY2v4yz8eT7ZgWpIX0zTVW
eb1D1+XqsKjxRj+ea9oeAWpuatwFwUo8dUcmCQxICrDTNHNcfXeyTJYqi0JzsktK
a7gzfLSqFtc77s2XBGlgN+r3+PeTgo/REIdwf1HaYQKBgQDGsA5UhBt/bDDIGfak
7mP1kWnSDYV/J8BmqYS78+d7iiMIUTf5EDP3Gz07zGpXnnxOxJG6t9CbbXYo3Vw9
rCpojBBZQ+fQTw4WguhOcl0fG0bZIxAPmU2kCD8JB5gLXDwmEyqwZHcyvTnSfBZs
TxnNYrvEb8bAhQ5CfxNNSlrWEQ==
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
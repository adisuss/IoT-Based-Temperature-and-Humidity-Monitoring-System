#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHT.h>
#include <HTTPClient.h>
// #include <time.h>

#define RESET_PIN 4
#define LED_PIN 2
#define DHTPIN 5     // Pin yang terhubung dengan sensor DHT11
#define DHTTYPE DHT11 // Tipe sensor DHT yang digunakan
#define EEPROM_SIZE 2048 // Ukuran EEPROM sesuai kebutuhan
#define WIFI_CONFIG_ADDRESS 0 // Alamat mulai untuk menyimpan data WiFi
#define FIREBASE_CONFIG_ADDRESS 128 // Alamat mulai untuk menyimpan data Firebase (disesuaikan)
#define DATABASE_URL "your database url"
#define DATABASE_SECRET "your database secret"
#define FIREBASE_CLIENT_EMAIL "your firebase client email"
#define FIREBASE_PROJECT_ID "your projectID"


// Struktur untuk menyimpan konfigurasi Wi-Fi
struct WiFiConfig {
    char wifiSSID[32];
    char wifiPassword[32];
};

// Struktur untuk menyimpan konfigurasi Firebase
struct FirebaseConfig {
    char apiKey[64];
    char deviceName[32];
    char deviceType[32]; // Tambahkan deviceType
    char deviceLocation[32]; // Tambahkan deviceLocation
    char userId[64];
    char email[64];
    char fcmToken[256];
};
// Definisi Firebase
FirebaseApp app;

LegacyToken legacy_token(DATABASE_SECRET);

DefaultNetwork network;
RealtimeDatabase Database;

WiFiClientSecure ssl_client;

using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client, getNetwork(network));

String payload = "";
int targethour = -1;
int targetminute = -1;
bool targetdaily = false;

String schedulePayload = "";
String temperaturePayload = "";

bool taskComplete = false;
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 300000; // Update setiap 5 menit

// Waktu dan interval untuk pengecekan
unsigned long previousMillis = 0;
const long interval = 1000; // Periksa setiap 1 detik (1000 ms)
bool scheduleTriggered = false;
int previousDay = -1;  // Menyimpan hari sebelumnya

const char PRIVATE_KEY[] PROGMEM = R"(
-----BEGIN PRIVATE KEY-----
-----END PRIVATE KEY-----
)";

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

// DHT sensor setup
DHT dht(DHTPIN, DHTTYPE);

Messaging messaging;

void asyncCB(AsyncResult &aResult);
void printResult(AsyncResult &aResult);
bool loadWiFiConfigFromEEPROM();
bool loadFirebaseConfigFromEEPROM();
void saveWiFiConfigToEEPROM(const WiFiConfig& config);
void saveFirebaseConfigToEEPROM(const FirebaseConfig& config);

void timeStatusCB(uint32_t &ts){
#if defined(ESP8266) || defined(ESP32) || defined(CORE_ARDUINO_PICO)
    if (time(nullptr) < FIREBASE_DEFAULT_TS)
    {
        configTime(7 * 3600, 0, "pool.ntp.org"); // Zona waktu +3 GMT (ubah sesuai kebutuhan Anda)
        while (time(nullptr) < FIREBASE_DEFAULT_TS)
        {
            delay(100);
        }
    }
    ts = time(nullptr); // Set waktu yang disinkronkan
#elif __has_include(<WiFiNINA.h>) || __has_include(<WiFi101.h>)
    ts = WiFi.getTime(); // Untuk perangkat berbasis WiFiNINA/WiFi101
#endif
}

ServiceAuth sa_auth(timeStatusCB, FIREBASE_CLIENT_EMAIL, FIREBASE_PROJECT_ID, PRIVATE_KEY, 3000);

// Define the HTTP server
WebServer server(80);

void setup() {
    Serial.begin(115200);
    delay(1000); // Tunggu serial monitor siap
    pinMode(RESET_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    dht.begin();
    EEPROM.begin(EEPROM_SIZE);


    bool wifiLoaded = loadWiFiConfigFromEEPROM();
    if (wifiLoaded) {
        WiFiConfig wifiConfig;
        EEPROM.get(WIFI_CONFIG_ADDRESS, wifiConfig);

        Serial.println("Connecting to saved WiFi credentials...");
        WiFi.begin(wifiConfig.wifiSSID, wifiConfig.wifiPassword);
        unsigned long startTime = millis();

        while (millis() - startTime < 5000) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Connected to WiFi");
                Serial.print("IP Address: ");
                Serial.println(WiFi.localIP());
                digitalWrite(LED_PIN, HIGH); // Nyalakan LED saat terhubung
                timeClient.begin(); // Mulai NTP client
                break;
            }
            delay(500);
        }
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Failed to connect to saved WiFi credentials");
            wifiLoaded = false;
            digitalWrite(LED_PIN, LOW); // Matikan LED jika tidak terhubung
        }
    }
    bool firebaseLoaded = loadFirebaseConfigFromEEPROM();
    if (!wifiLoaded || !firebaseLoaded) {
        Serial.println("Failed to load configuration from EEPROM, starting web server for configuration...");

        WiFi.softAP("ESP32_AP", "12345678");
        Serial.print("Access Point started with IP: ");
        Serial.println(WiFi.softAPIP());

        server.on("/", HTTP_GET, handleRoot);
        server.on("/scan", HTTP_GET, handleScanNetworks);
        server.on("/connect", HTTP_POST, handleConnect);
        server.begin();
    } else {
      initializeFirebase();
    }
}

void loop() {
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastUpdate >= updateInterval) {
        lastUpdate = currentMillis;
        updateData();
    }
        if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;  // Reset timer untuk interval berikutnya

        timeClient.update();  // Perbarui waktu dari NTP

        // Panggil fungsi schedule untuk memeriksa apakah waktu sudah mencapai target
        schedule();
    }
    
    if (digitalRead(RESET_PIN) == LOW) {
        Serial.println("Reset button pressed. Clearing EEPROM and restarting...");
        EEPROM.begin(EEPROM_SIZE);
        
        // Clear EEPROM
        for (int i = 0; i < EEPROM_SIZE; i++) {
            EEPROM.write(i, 0);
        }
        EEPROM.commit();

        // Restart ESP32
        ESP.restart();
    }
    server.handleClient();
    JWT.loop(app.getAuth());
    messaging.loop();
    app.loop();
    Database.loop();
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_PIN, HIGH);
    } else {
        digitalWrite(LED_PIN, LOW);
    }

    if (app.ready() && !taskComplete) {
        taskComplete = true;
        Serial.println("Authentication Information");
        Firebase.printf("User UID: %s\n", app.getUid().c_str());
        Firebase.printf("Auth Token: %s\n", app.getToken().c_str());
        Firebase.printf("Refresh Token: %s\n", app.getRefreshToken().c_str());

        Messages::Message msg;
        getMsg(msg);
        Serial.println("Sending a message. . . .");
        messaging.send(aClient, Messages::Parent(FIREBASE_PROJECT_ID), msg, asyncCB, "fcmsendTask");
        
        FirebaseConfig firebaseConfig;
        EEPROM.get(FIREBASE_CONFIG_ADDRESS, firebaseConfig);

        String path = "/users/" + String(firebaseConfig.userId) + "/device/" + String(firebaseConfig.deviceName) + "/";

        // Update device type and location
        Database.set<String>(aClient, path + "deviceType", String(firebaseConfig.deviceType), asyncCB, "setDeviceTypeTask");
        Database.set<String>(aClient, path + "deviceLocation", String(firebaseConfig.deviceLocation), asyncCB, "setDeviceLocationTask");
        Database.setSSEFilters("get,put,patch,cancel,auth_revoked");
        Database.get(aClient, path + "schedule" , asyncCB, true /* SSE mode (HTTP Streaming) */, "streamTask");
    }
}

void schedule() {
    // Dapatkan waktu sekarang
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();
    int currentDay = timeClient.getDay();

    // Periksa jika hari telah berubah
    if (currentDay != previousDay) {
        Serial.println("New day detected, resetting schedule trigger.");
        scheduleTriggered = false;  // Reset trigger untuk hari baru
        previousDay = currentDay;   // Update hari sebelumnya
    }

    // Periksa apakah jadwal harian diaktifkan
    if (targetdaily) {
        // Cocokkan waktu sekarang dengan jadwal
        if (currentHour == targethour && currentMinute == targetminute) {
            if (!scheduleTriggered) {
                Serial.println("Schedule triggered!");
                scheduleTriggered = true;  // Tandai bahwa jadwal telah dipicu
                // filterTemperatureData();   // Panggil fungsi untuk memfilter data
                fetchTemperatureData();
            }
        } else {
            // Reset flag jika waktu sudah tidak cocok lagi
            if (scheduleTriggered) {
                Serial.println("Schedule no longer triggered.");
                scheduleTriggered = false;
            }
        }
    } else {
        Serial.println("Daily schedule is not enabled.");
    }
}

void fetchTemperatureData() {
    FirebaseConfig firebaseConfig;
    EEPROM.get(FIREBASE_CONFIG_ADDRESS, firebaseConfig);

    String path = "/users/" + String(firebaseConfig.userId) + "/device/" + String(firebaseConfig.deviceName) + "/temperatureData.json?auth=" + String(DATABASE_SECRET);

    // Menambahkan parameter query
    String startAt = getISO8601Time(8, 0, 0);
    String endAt = getISO8601Time(23, 59, 59);
    path += "&orderBy=\"$key\"&startAt=\"" + startAt + "\"&endAt=\"" + endAt + "\"";

    Serial.println("Fetching data from URL: " + String(DATABASE_URL) + path);

    HTTPClient http;
    http.begin(String(DATABASE_URL) + path);

    int httpResponCode = http.GET();

    if (httpResponCode > 0) {
        String payload = http.getString();
        Serial.println("Firebase Response: " + payload);

        // Proses payload JSON di sini
        filterTemperatureData(payload);
    } else {
        Serial.println("Error fetching data: " + String(httpResponCode));
    }

    http.end();
}

String getISO8601Time(int hour, int minute, int second) {
    timeClient.update();
    time_t now = timeClient.getEpochTime();
    struct tm* timeInfo = gmtime(&now);

    // Set jam, menit, dan detik sesuai parameter
    timeInfo->tm_hour = hour;
    timeInfo->tm_min = minute;
    timeInfo->tm_sec = second;

    char timeString[20];
    strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", timeInfo);
    return String(timeString);
}

void filterTemperatureData(const String& temperaturePayload) {
    DynamicJsonDocument doc(4096);  // Sesuaikan ukuran dokumen sesuai payload
    DeserializationError error = deserializeJson(doc, temperaturePayload);

    if (error) {
        Serial.println("Failed to parse JSON for temperaturePayload");
        return;
    }

    // Daftar jam yang ingin difilter
    int targetHours[] = {9, 13, 16, 20, 23};
    const int numTargetHours = sizeof(targetHours) / sizeof(targetHours[0]);

    String currentDate = getDefaultDate();  // Tanggal hari ini
    JsonArray readings = doc.createNestedArray("readings");

    for (int i = 0; i < numTargetHours; i++) {
        int hour = targetHours[i];
        bool found = false;

        for (JsonPair kv : doc.as<JsonObject>()) {
            String timestamp = kv.key().c_str();
            JsonObject data = kv.value().as<JsonObject>();

            int dataHour = extractHourFromTimestamp(timestamp);
            String date = extractDateFromTimestamp(timestamp);

            if (dataHour == hour && date == currentDate) {
                found = true;
                JsonObject reading = readings.createNestedObject();
                reading["hour"] = hour;
                reading["humidity"] = data["humidity"];
                reading["temperature"] = data["temperature"];
                break;
            }
        }

        if (!found) {
            JsonObject reading = readings.createNestedObject();
            reading["hour"] = hour;
            reading["humidity"] = "0";
            reading["temperature"] = "0";
        }
    }

    sendDataToGoogleSheet(readings);
}

void getMsg(Messages::Message &msg){
    FirebaseConfig firebaseConfig;
    EEPROM.get(FIREBASE_CONFIG_ADDRESS, firebaseConfig);
    // Gunakan token perangkat
    msg.token(firebaseConfig.fcmToken); // Ganti dengan FCM Token perangkat tujuan

    // Membuat notifikasi sederhana
    Messages::Notification notification;
    String titled = "Dari " + String(firebaseConfig.deviceName);
    notification.body("HELLLLOOOOOO 🎉").title(titled);

    // Tambahkan notifikasi ke pesan
    msg.notification(notification);

    // Menambahkan data tambahan (opsional)
    object_t data, obj1, obj2;
    JsonWriter writer;
    writer.create(obj1, "key1", string_t("value1"));
    writer.create(obj2, "key2", string_t("value2"));
    writer.join(data, 2, obj1, obj2);
    msg.data(data);

    // Konfigurasi Android (opsional)
    Messages::AndroidConfig androidConfig;
    androidConfig.priority(Messages::AndroidMessagePriority::_HIGH);

    Messages::AndroidNotification androidNotification;
    androidNotification.notification_priority(Messages::NotificationPriority::PRIORITY_HIGH);
    androidConfig.notification(androidNotification);

    msg.android(androidConfig);
}

void sendDataToGoogleSheet(JsonArray readings) {
    FirebaseConfig firebaseConfig;
    EEPROM.get(FIREBASE_CONFIG_ADDRESS, firebaseConfig);

    // Membuat objek JSON untuk payload
    DynamicJsonDocument doc(1024);
    doc["email"] = firebaseConfig.email;  // Ganti dengan email pengguna
    doc["date"] = getDefaultDate();  // Tanggal saat ini
    doc["deviceName"] = firebaseConfig.deviceName;
    
    // Membuat array untuk readings
    JsonArray readingsArray = doc.createNestedArray("data");

    // Menambahkan data ke dalam array readings
    for (JsonObject reading : readings) {
        JsonObject readingData = readingsArray.createNestedObject();
        readingData["hour"] = reading["hour"];
        // readingData["humidity"] = reading["humidity"];
        // readingData["temperature"] = reading["temperature"];
        readingData["humidity"] = String(reading["humidity"]);
        readingData["temperature"] = String(reading["temperature"]);

    }

    // Mengonversi dokumen JSON menjadi string
    String jsonString;
    serializeJson(doc, jsonString);

    // URL endpoint Google Apps Script (URL dari deploy Google Apps Script)
    const char* url = "your api appscript";

    // Mengirim HTTP POST request
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(url);  // Menyambung ke Google Apps Script endpoint
        http.addHeader("Content-Type", "application/json");

        http.setTimeout(15000);

        // Mengirim POST request
        int httpResponseCode = http.POST(jsonString);

        // Menangani respons
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response: " + response);
            Serial.println("JSON Data: " + jsonString);
        } else {
            Serial.println("Error sending POST request: " + String(httpResponseCode));
            Serial.println("JSON Data: " + jsonString);
        }

        // Menutup koneksi HTTP
        http.end();
    } else {
        Serial.println("WiFi not connected");
    }
}

// Fungsi untuk mengekstrak jam dari timestamp (format ISO 8601: YYYY-MM-DDTHH:MM:SS)
int extractHourFromTimestamp(const String &timestamp) {
    int hourStartIndex = timestamp.indexOf('T') + 1;  // 'T' memisahkan tanggal dan waktu
    String hourString = timestamp.substring(hourStartIndex, hourStartIndex + 2);
    return hourString.toInt();  // Konversi ke integer
}

// Fungsi untuk mengekstrak tanggal dari timestamp (format ISO 8601: YYYY-MM-DDTHH:MM:SS)
String extractDateFromTimestamp(const String &timestamp) {
    int dateEndIndex = timestamp.indexOf('T');  // 'T' memisahkan tanggal dan waktu
    return timestamp.substring(0, dateEndIndex);  // Ambil substring tanggal (YYYY-MM-DD)
}

// Fungsi untuk mendapatkan tanggal default (misalnya, tanggal hari ini)
String getDefaultDate() {
    // Perbarui waktu NTP
    timeClient.update();
    
    // Ambil epoch time dari NTP
    time_t epochTime = timeClient.getEpochTime();
    struct tm* timeInfo = gmtime(&epochTime);  // Menggunakan gmtime untuk mendapatkan waktu UTC

    // Formatkan tanggal dalam format YYYY-MM-DD
    char formattedDate[11];
    strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d", timeInfo);  // Format tanggal
    
    return String(formattedDate);  // Mengembalikan tanggal dalam format YYYY-MM-DD
}

void updateData() {
    if (WiFi.status() == WL_CONNECTED) {
        FirebaseConfig firebaseConfig;
        EEPROM.get(FIREBASE_CONFIG_ADDRESS, firebaseConfig);
        String path = "/users/" + String(firebaseConfig.userId) + "/device/" + String(firebaseConfig.deviceName) + "/";

        if (String(firebaseConfig.deviceType) == "Temperature Sensor") {
            float temperature = readTemperature();
            float humidity = readHumidity();
            timeClient.update();
            time_t epochTime = timeClient.getEpochTime();
            struct tm* timeInfo = gmtime(&epochTime);
            char formattedTime[20];
            strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%dT%H:%M:%SZ", timeInfo);
            String timestamp = String(formattedTime);

            // Path untuk menyimpan data
            String temperaturePath = path + "temperatureData/" + timestamp;
            
            // Simpan data suhu dan kelembapan dengan timestamp
            Database.set<float>(aClient, temperaturePath + "/temperature", temperature, asyncCB, "setTemperatureTask");
            Database.set<float>(aClient, temperaturePath + "/humidity", humidity, asyncCB, "setHumidityTask");

            // Kirim notifikasi jika suhu melebihi atau kurang dari 25 derajat
            if (temperature > 32.5 || temperature < 25.0) {
                // Membuat objek notifikasi
                Messages::Notification notification;
                String title = "Peringatan Suhu dari " + String(firebaseConfig.deviceName);
                String body = (temperature > 25.0) 
                              ? "Suhu di atas 25°C: " + String(temperature, 1) + "°C"
                              : "Suhu di bawah 25°C: " + String(temperature, 1) + "°C";

                notification.body(body).title(title);

                // Menyiapkan pesan
                Messages::Message msg;
                msg.notification(notification);

                // Masukkan token perangkat tujuan
                msg.token(firebaseConfig.fcmToken); // Ganti dengan FCM Token perangkat tujuan

                // Kirim pesan melalui FCM
                Serial.println("Sending temperature notification...");
                messaging.send(aClient, Messages::Parent(FIREBASE_PROJECT_ID), msg, asyncCB, "fcmsendTask");
            }
        } else if (String(firebaseConfig.deviceType) == "Light") {
            float brightness = readBrightness();
            Database.set<float>(aClient, path + "brightness", brightness, asyncCB, "setBrightnessTask");
        } else if (String(firebaseConfig.deviceType) == "Socket") {
            bool isOn = getSocketStatus();
            Database.set<bool>(aClient, path + "isOn", isOn, asyncCB, "setSocketStatusTask");
        }
    }
}

void handleRoot() {
    server.send(200, "text/html", "<form action=\"/connect\" method=\"POST\">"
                                  "<input type=\"text\" name=\"ssid\" placeholder=\"SSID\" required>"
                                  "<input type=\"password\" name=\"wifi_password\" placeholder=\"WiFi Password\" required>"
                                  "<input type=\"text\" name=\"email\" placeholder=\"Email\" required>"
                                  "<input type=\"password\" name=\"firebase_password\" placeholder=\"Firebase Password\" required>"
                                  "<input type=\"text\" name=\"deviceName\" placeholder=\"Device Name\" required>" 
                                  "<input type=\"text\" name=\"deviceType\" placeholder=\"Device Type\" required>"
                                  "<input type=\"text\" name=\"deviceLocation\" placeholder=\"Device Location\" required>"
                                  "<input type=\"text\" name=\"authToken\" placeholder=\"ID Token\" required>"
                                  "<input type=\"text\" name=\"userId\" placeholder=\"User ID\" required>"
                                  "<input type=\"text\" name=\"fcmToken\" placeholder=\"FCM Token\" required>" // Tambahkan input untuk FCM Token
                                  "<input type=\"submit\" value=\"Connect\">"
                                  "</form>");
}


void handleScanNetworks() {
    int n = WiFi.scanNetworks();
    DynamicJsonDocument doc(1024);
    JsonArray networks = doc.createNestedArray("networks");

    for (int i = 0; i < n; ++i) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encryptionType"] = WiFi.encryptionType(i);
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleConnect() {
    if (server.hasArg("ssid") &&
        server.hasArg("wifi_password") &&
        server.hasArg("email") &&
        server.hasArg("firebase_password") &&
        server.hasArg("deviceType") &&
        server.hasArg("deviceLocation") &&
        server.hasArg("deviceName") &&
        server.hasArg("userId") &&
        server.hasArg("fcmToken")) { // Tambahkan argumen fcmToken

        WiFiConfig wifiConfig;
        FirebaseConfig firebaseConfig;

        // Salin data dari form ke struktur WiFiConfig dan FirebaseConfig
        strncpy(wifiConfig.wifiSSID, server.arg("ssid").c_str(), sizeof(wifiConfig.wifiSSID));
        strncpy(wifiConfig.wifiPassword, server.arg("wifi_password").c_str(), sizeof(wifiConfig.wifiPassword));
        strncpy(firebaseConfig.apiKey, "your api key", sizeof(firebaseConfig.apiKey));
        strncpy(firebaseConfig.deviceType, server.arg("deviceType").c_str(), sizeof(firebaseConfig.deviceType));
        strncpy(firebaseConfig.deviceLocation, server.arg("deviceLocation").c_str(), sizeof(firebaseConfig.deviceLocation));
        strncpy(firebaseConfig.deviceName, server.arg("deviceName").c_str(), sizeof(firebaseConfig.deviceName));
        strncpy(firebaseConfig.userId, server.arg("userId").c_str(), sizeof(firebaseConfig.userId));
        strncpy(firebaseConfig.email, server.arg("email").c_str(), sizeof(firebaseConfig.email));
        strncpy(firebaseConfig.fcmToken, server.arg("fcmToken").c_str(), sizeof(firebaseConfig.fcmToken)); // Tambahkan fcmToken

        // Cetak FCM Token ke Serial Monitor
        Serial.println("Received FCM Token: " + String(firebaseConfig.fcmToken));

        Serial.println("Connecting to WiFi...");
        WiFi.begin(wifiConfig.wifiSSID, wifiConfig.wifiPassword);
        unsigned long startTime = millis();

        while (millis() - startTime < 5000) {
            if (WiFi.status() == WL_CONNECTED) {
                server.send(200, "text/plain", "Connected to WiFi");
                Serial.println("Connected to WiFi");
                Serial.println("SSID: " + String(wifiConfig.wifiSSID));
                Serial.println("Password: " + String(wifiConfig.wifiPassword));
                
                // Save WiFi credentials to EEPROM
                saveWiFiConfigToEEPROM(wifiConfig);
                
                // Save Firebase information to EEPROM (termasuk FCM Token)
                saveFirebaseConfigToEEPROM(firebaseConfig);

                delay(2000);
                WiFi.softAPdisconnect(true);
                server.stop();
                initializeFirebase();
                return;
            }
            delay(500);
        }

        Serial.println("Failed to connect to WiFi");
        server.send(200, "text/plain", "Failed to connect to WiFi");
    } else {
        server.send(400, "text/plain", "Invalid request");
    }
}



float readTemperature() {
    return dht.readTemperature();
}

float readHumidity() {
    return dht.readHumidity();
}

float readBrightness() {
    // Implementasi membaca nilai cahaya (contoh)
    return analogRead(34);
}

bool getSocketStatus() {
    // Implementasi mendapatkan status soket (contoh)
    return digitalRead(33) == HIGH;
}

bool loadWiFiConfigFromEEPROM() {
    WiFiConfig config;
    EEPROM.get(WIFI_CONFIG_ADDRESS, config);
    if (strlen(config.wifiSSID) > 0 && strlen(config.wifiPassword) > 0) {
        return true;
    }
    return false;
}

bool loadFirebaseConfigFromEEPROM() {
    FirebaseConfig config;
    EEPROM.get(FIREBASE_CONFIG_ADDRESS, config); // Ambil data mulai dari alamat FIREBASE_CONFIG_ADDRES
    return true;
}

void saveWiFiConfigToEEPROM(const WiFiConfig& config) {
    EEPROM.put(WIFI_CONFIG_ADDRESS, config);
    EEPROM.commit();
}

void saveFirebaseConfigToEEPROM(const FirebaseConfig& config) {
    EEPROM.put(FIREBASE_CONFIG_ADDRESS, config);
    EEPROM.commit();
}


void printResult(AsyncResult &aResult) {
    if (aResult.isEvent()) {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug()) {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError()) {
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

void initializeFirebase() {
    FirebaseConfig firebaseConfig;
    EEPROM.get(FIREBASE_CONFIG_ADDRESS, firebaseConfig);

    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    ssl_client.setInsecure();

    Serial.println("Initializing app...");
    initializeApp(aClient, app, getAuth(legacy_token));
    initializeApp(aClient, app, getAuth(sa_auth), asyncCB, "authTask");
    app.getApp<Messaging>(messaging); // Mengaitkan Messaging dengan FirebaseApp
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);

    Serial.println("Asynchronous Set...");
}
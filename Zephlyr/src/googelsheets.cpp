#include "Auth.h"
#include "EEPROMmanager.h"
#include "Config.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

unsigned long previousMillis = 0;
int previousDay = 0;
bool scheduleTriggered = false;
const long interval = 1000;

String extractDateFromTimestamp(const String &timestamp) {
    int dateEndIndex = timestamp.indexOf('T');  // 'T' memisahkan tanggal dan waktu
    return timestamp.substring(0, dateEndIndex);  // Ambil substring tanggal (YYYY-MM-DD)
}

int extractHourFromTimestamp(const String &timestamp) {
    int hourStartIndex = timestamp.indexOf('T') + 1;  // 'T' memisahkan tanggal dan waktu
    String hourString = timestamp.substring(hourStartIndex, hourStartIndex + 2);
    return hourString.toInt();  // Konversi ke integer
}

void fetchTemperatureData() {
    String path = "/users/" + String(deviceConfig.userId) + "/device/" + String(deviceConfig.deviceName) + "/temperatureData.json?auth=" + String(DATABASE_SECRET);

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

void sendDataToGoogleSheet(JsonArray &readings) {
    // Membuat objek JSON untuk payload
    DynamicJsonDocument doc(1024);
    doc["email"] = deviceConfig.email;  // Ganti dengan email pengguna
    doc["date"] = getDefaultDate();  // Tanggal saat ini
    doc["deviceName"] = deviceConfig.deviceName;
    
    // Membuat array untuk readings
    JsonArray readingsArray = doc.createNestedArray("data");

    // Menambahkan data ke dalam array readings
    for (JsonObject reading : readings) {
        JsonObject readingData = readingsArray.createNestedObject();
        readingData["hour"] = reading["hour"];
        readingData["humidity"] = String(reading["humidity"].as<String>());
        readingData["temperature"] = String(reading["temperature"].as<String>());
    }

    // Mengonversi dokumen JSON menjadi string
    String jsonString;
    serializeJson(doc, jsonString);

    // URL endpoint Google Apps Script (URL dari deploy Google Apps Script)
    const char* url = "https://script.google.com/macros/s/AKfycbyCGdvS8WT8QA5L5S1eoMiGlt8hkfC2Y78uJrxNEuGN38fqTF67H8sDDXyJP00U1IwzLA/exec";

    // Mengirim HTTP POST request
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(url);  // Menyambung ke Google Apps Script endpoint
        http.addHeader("Content-Type", "application/json");

        http.setTimeout(15000);

        int httpResponseCode = http.POST(jsonString);

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
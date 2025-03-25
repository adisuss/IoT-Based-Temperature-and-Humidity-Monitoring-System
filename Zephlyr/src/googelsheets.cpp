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

bool allBatchesFetches = false;

DynamicJsonDocument collectedDoc(2048);
JsonArray collectedReadings = collectedDoc.createNestedArray();

String extractDateFromTimestamp(const String &timestamp) {
    int dateEndIndex = timestamp.indexOf('T');  
    return timestamp.substring(0, dateEndIndex);  
}

int extractHourFromTimestamp(const String &timestamp) {
    int hourStartIndex = timestamp.indexOf('T') + 1;  
    String hourString = timestamp.substring(hourStartIndex, hourStartIndex + 2);
    return hourString.toInt();  
}

void fetchTemperatureData(int startHour, int endHour) {
    String path = "/users/" + String(deviceConfig.userId) + "/device/" + String(deviceConfig.deviceName) + "/temperatureData.json?auth=" + String(DATABASE_SECRET);

    String startAt = getISO8601Time(startHour, 0, 0);
    String endAt = getISO8601Time(endHour, 0, 0);
    path += "&orderBy=\"$key\"&startAt=\"" + startAt + "\"&endAt=\"" + endAt + "\"";

    Serial.println("Fetching data from " + String(startHour) + ":00 to " + String(endHour) + ":00");
    // Serial.println("URL: " + String(DATABASE_URL) + path);

    HTTPClient http;
    http.begin(String(DATABASE_URL) + path);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String payload = http.getString();
        Serial.println("Firebase Response: " + payload);

        filterTemperatureData(payload);
        
    } else {
        Serial.println("Error fetching data: " + String(httpResponseCode));
    }

    http.end();
}


void filterTemperatureData(const String& temperaturePayload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, temperaturePayload);

    if (error) {
        Serial.println("❌ Failed to parse JSON for temperaturePayload");
        return;
    }

    int targetHours[] = {9, 13, 16, 20, 23};
    const int numTargetHours = sizeof(targetHours) / sizeof(targetHours[0]);

    String currentDate = getDefaultDate();

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

                JsonObject existingEntry = JsonObject();
                for (JsonObject entry : collectedReadings) {
                    if (entry["hour"] == hour) {
                        existingEntry = entry;
                        break;
                    }
                }

                if (!existingEntry.isNull()) {
                    existingEntry["humidity"] = data["humidity"];
                    existingEntry["temperature"] = data["temperature"];
                } else {
                    JsonObject newEntry = collectedReadings.add<JsonObject>();
                    newEntry["hour"] = hour;
                    newEntry["humidity"] = data["humidity"];
                    newEntry["temperature"] = data["temperature"];
                }
                break;
            }
        }

        if (!found) {
            bool alreadyExists = false;
            for (JsonObject existing : collectedReadings) {
                if (existing["hour"] == hour) {
                    alreadyExists = true;
                    break;
                }
            }

            if (!alreadyExists) {
                JsonObject emptyEntry = collectedReadings.createNestedObject();
                emptyEntry["hour"] = hour;
                emptyEntry["humidity"] = "0";
                emptyEntry["temperature"] = "0";
            }
        }
    }
}


void fetchAllTemperatureData() {
    collectedDoc.clear();  // Bersihkan buffer sebelum mulai
    collectedReadings = collectedDoc.to<JsonArray>();  // Reset array
    fetchTemperatureData(9, 10);
    delay(5000);
    fetchTemperatureData(13, 14);
    delay(5000);
    fetchTemperatureData(16, 17);
    delay(5000);
    fetchTemperatureData(20, 24);
    delay(5000);
    delay(3000);
    sendDataToGoogleSheet(collectedReadings);
}


void sendDataToGoogleSheet(JsonArray &readings) {
    JsonDocument doc;
    doc["email"] = deviceConfig.email;
    doc["date"] = getDefaultDate();
    doc["deviceName"] = deviceConfig.deviceName;

    JsonArray readingsArray = doc["data"].to<JsonArray>();


    if (readings.size() == 0) {
        Serial.println("⚠️ No data to send! Readings array is empty.");
        return;
    }

    for (JsonObject reading : readings) {
        JsonObject readingData = readingsArray.createNestedObject();
        readingData["hour"] = reading["hour"];
        readingData["humidity"] = reading["humidity"].as<String>();
        readingData["temperature"] = reading["temperature"].as<String>();
    }

    String jsonString;
    serializeJson(doc, jsonString);

    Serial.println("✅ Final JSON to send: " + jsonString);

    const char* url = "your appscript API";

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        http.setTimeout(30000);

        int httpResponseCode = http.POST(jsonString);
        Messages::Notification notification;
        String title = deviceConfig.deviceName;
        Messages::Message msg;
        msg.token(deviceConfig.fcmToken);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("✅ Response from Google Sheets: " + response);
            notification.body("Data Berhasil Dikirim Ke GoogleSheets 🎉").title(title);
        } else {
            Serial.println("❌ Error sending POST request: " + String(httpResponseCode));
            notification.body("Data Gagal Dikirim Ke GoogleSheets ❌").title(title);
        }
        msg.notification(notification);
        Serial.println("Sending status notification...");
        messaging.send(aClient, Messages::Parent(FIREBASE_PROJECT_ID), msg, asyncCB, "fcmsendTask");

        http.end();
    } else {
        Serial.println("❌ WiFi not connected");
    }
}

String getISO8601Time(int hour, int minute, int second) {
    timeClient.update();
    time_t now = timeClient.getEpochTime();
    struct tm* timeInfo = gmtime(&now);

    timeInfo->tm_hour = hour;
    timeInfo->tm_min = minute;
    timeInfo->tm_sec = second;

    char timeString[20];
    strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", timeInfo);
    return String(timeString);
}

String getDefaultDate() {
    timeClient.update();
    
    time_t epochTime = timeClient.getEpochTime();
    struct tm* timeInfo = gmtime(&epochTime);  

    char formattedDate[11];
    strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d", timeInfo);  
    
    return String(formattedDate);  
}

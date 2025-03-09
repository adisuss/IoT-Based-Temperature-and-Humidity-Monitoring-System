#include <Arduino.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include "WiFiManager.h"
#include "ServerManager.h"
#include "EEPROMManager.h"
#include "Auth.h"
#include "Config.h"

#define led_pin 2

unsigned long lastReconnectAttempt = 0;

bool taskComplete = false;

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 300000; // Update setiap 5 menit

void setup() {
    Serial.begin(115200);
    checkResetButton();
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, LOW);
    loadConfigFromEEPROM();
    if (isWiFiConfigured()) {
        Serial.println("Mencoba menyambungkan...");
        connectToWiFi();
        timeClient.begin();
        initializeFirebase();
    } else {
        startAccessPoint();
    }
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate >= updateInterval) {
        lastUpdate = currentMillis;
        if (checkNTPTime(timeClient)) {
            updateData(); // Hanya update data jika waktu valid
        } else {
            Serial.println("Update data ditunda karena gagal mendapatkan waktu NTP.");
        }
    }
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;  // Reset timer untuk interval berikutnya

        timeClient.update();  // Perbarui waktu dari NTP

        // Panggil fungsi schedule untuk memeriksa apakah waktu sudah mencapai target
        schedule();
    }
    if (!isAPMode && WiFi.status() != WL_CONNECTED && (currentMillis - lastReconnectAttempt >= 10000)) {
        digitalWrite(led_pin, LOW);
        Serial.println("WiFi terputus! Mencoba reconnect...");
        WiFi.disconnect();
        connectToWiFi();
        lastReconnectAttempt = currentMillis;
    }
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(led_pin, HIGH);
    }
    if (isAPMode) { // Hanya jalankan server jika WiFi belum dikonfigurasi
        server.handleClient();
    }
    JWT.loop(app.getAuth());
    checkResetButton();
    messaging.loop();
    app.loop();
    Database.loop();
    if(app.ready() && !taskComplete){
      taskComplete = true;
      setData();
    }
}



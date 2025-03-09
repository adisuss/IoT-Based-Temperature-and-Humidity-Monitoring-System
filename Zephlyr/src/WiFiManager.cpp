#include "WiFiManager.h"
#include "EEPROMmanager.h"
#include <EEPROM.h>
#include <WiFi.h>
#include "ServerManager.h"

#define EEPROM_SIZE 512 

bool isAPMode = false;

void startAccessPoint() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_Config");
    Serial.println("Access Point ESP32_Config aktif!");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
    isAPMode = true; // Set flag agar ESP tidak reconnect WiFi
    setupServer();
}

void connectToWiFi() {
    Serial.println("Mencoba menyambungkan ke WiFi...");
    WiFi.mode(WIFI_STA);

    preferences.begin("device_config", true);
    String ssid = preferences.getString("ssid", "");
    String pass = preferences.getString("wifi_password", "");
    preferences.end();

    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {  // Timeout 20 detik
        delay(50);
        Serial.print(".");
        checkResetButton();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Terhubung!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        server.stop();
    } else {
        Serial.println("\nGagal menyambungkan ke WiFi. Masuk mode Access Point...");
    }
}

void resetConfig() {
    Serial.println("Menghapus semua konfigurasi di EEPROM (Preferences)...");

    preferences.begin("device_config", false);
    preferences.clear();  // Menghapus semua key yang tersimpan di namespace "device_config"
    preferences.end();

    Serial.println("Konfigurasi berhasil dihapus! Restart ESP32...");
    isAPMode = true;
    delay(1000);
    ESP.restart();  // Restart ESP32 untuk efek reset langsung
}

void checkResetButton() {
    pinMode(RESET_PIN, INPUT_PULLUP);  // Gunakan internal pull-up

    if (digitalRead(RESET_PIN) == LOW) {  // Jika tombol ditekan
        Serial.println("Tombol Reset ditekan! Menghapus konfigurasi...");
        delay(3000); // Hindari reset tidak sengaja

        if (digitalRead(RESET_PIN) == LOW) {  // Pastikan masih ditekan
            resetConfig();
        }
    }
}

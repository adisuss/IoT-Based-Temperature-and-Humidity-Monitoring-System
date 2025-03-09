#include "ServerManager.h"
#include "EEPROMManager.h"
#include <WebServer.h>

WebServer server(80);

void setupServer() {
    server.on("/connect", HTTP_POST, handleConnect);
    server.begin();
    Serial.println("Web server dimulai!");
}

void handleConnect() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No JSON data received\"}");
        return;
    }

    StaticJsonDocument<512> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, server.arg("plain"));

    if (error) {
        Serial.println("Gagal parsing JSON");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    deviceConfig.wifiSSID = jsonDoc["ssid"].as<String>();
    deviceConfig.wifiPassword = jsonDoc["wifi_password"].as<String>();
    deviceConfig.deviceName = jsonDoc["deviceName"].as<String>();
    deviceConfig.deviceType = jsonDoc["deviceType"].as<String>();
    deviceConfig.deviceLocation = jsonDoc["deviceLocation"].as<String>();
    deviceConfig.userId = jsonDoc["userId"].as<String>();
    deviceConfig.email = jsonDoc["email"].as<String>();
    deviceConfig.authToken = jsonDoc["authToken"].as<String>();
    deviceConfig.fcmToken = jsonDoc["fcmToken"].as<String>();

    Serial.println("=== Data Diterima dan Disimpan ===");
    Serial.println("Device Name: " + deviceConfig.deviceName);
    Serial.println("WiFi SSID: " + deviceConfig.wifiSSID);

    // Simpan konfigurasi ke EEPROM
    saveConfigToEEPROM();

    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Data received and saved\"}");
    Serial.println("Restarting ESP...");
    delay(1000);
    ESP.restart();
}

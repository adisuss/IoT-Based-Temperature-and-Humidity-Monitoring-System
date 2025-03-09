#include "EEPROMManager.h"
#include "Config.h"

Preferences preferences;

void saveConfigToEEPROM() {
    preferences.begin("device_config", false);
    preferences.putString("ssid", deviceConfig.wifiSSID);
    preferences.putString("wifi_password", deviceConfig.wifiPassword);
    preferences.putString("deviceName", deviceConfig.deviceName);
    preferences.putString("deviceType", deviceConfig.deviceType);
    preferences.putString("deviceLocation", deviceConfig.deviceLocation);
    preferences.putString("userId", deviceConfig.userId);
    preferences.putString("email", deviceConfig.email);
    preferences.putString("authToken", deviceConfig.authToken);
    preferences.putString("fcmToken", deviceConfig.fcmToken);
    preferences.end();
    Serial.println("Konfigurasi berhasil disimpan ke EEPROM!");
}

void loadConfigFromEEPROM() {
    preferences.begin("device_config", true);
    deviceConfig.wifiSSID = preferences.getString("ssid", "");
    deviceConfig.wifiPassword = preferences.getString("wifi_password", "");
    deviceConfig.deviceName = preferences.getString("deviceName", "");
    deviceConfig.deviceType = preferences.getString("deviceType", "");
    deviceConfig.deviceLocation = preferences.getString("deviceLocation", "");
    deviceConfig.userId = preferences.getString("userId", "");
    deviceConfig.email = preferences.getString("email", "");
    deviceConfig.authToken = preferences.getString("authToken", "");
    deviceConfig.fcmToken = preferences.getString("fcmToken", "");
    preferences.end();
    Serial.println("Konfigurasi berhasil dimuat dari EEPROM!");
}

bool isWiFiConfigured() {
    preferences.begin("device_config", true);
    String ssid = preferences.getString("ssid", "");
    preferences.end();

    return ssid.length() > 0;  // Jika SSID ada, berarti WiFi sudah dikonfigurasi
}

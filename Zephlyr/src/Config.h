#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHTPIN 5     // Pin yang terhubung dengan sensor DHT11
#define DHTTYPE DHT11

// Struktur untuk menyimpan data konfigurasi
struct DeviceConfig {
    String deviceName, deviceType, deviceLocation;
    String wifiSSID, wifiPassword;
    String userId, email, authToken, fcmToken;
};

// Objek global untuk menyimpan konfigurasi
extern DeviceConfig deviceConfig;
extern unsigned long previousMillis;
extern const long interval;
extern bool scheduleTriggered;
extern int previousDay;  // Menyimpan hari sebelumnya

extern int targethour;
extern int targetminute;
extern bool targetdaily;

float readTemperature();
float readHumidity();
bool getSocketStatus();
float readBrightness();
bool checkNTPTime(NTPClient &timeClient);

void schedule();
void setData();

#endif

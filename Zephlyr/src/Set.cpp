#include "Auth.h"
#include "EEPROMmanager.h"
#include "Config.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHT.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

DHT dht(DHTPIN, DHTTYPE);

RealtimeDatabase Database;
Messaging messaging;

bool checkNTPTime(NTPClient &timeClient) {
    int ntpRetries = 5; // Coba sync waktu sampai 5 kali
    while (ntpRetries > 0) {
        timeClient.update();
        time_t epochTime = timeClient.getEpochTime();
        if (epochTime >= 1000000000) { // Waktu valid
            struct tm *timeInfo = gmtime(&epochTime);
            // Ambil data tanggal
            int tahun = timeInfo->tm_year + 1900; // Tahun dihitung dari 1900
            int bulan = timeInfo->tm_mon + 1;     // Bulan dimulai dari 0 (Januari = 0)
            int tanggal = timeInfo->tm_mday;
            int jam = timeInfo->tm_hour;
            int menit = timeInfo->tm_min;
            int detik = timeInfo->tm_sec;
            // Tampilkan hasil
            Serial.print("Sinkronisasi NTP Berhasil: ");
            Serial.printf("%04d-%02d-%02d %02d:%02d:%02d UTC\n", tahun, bulan, tanggal, jam, menit, detik);
            return true;
        }
        Serial.println("NTP Sync Gagal! Mencoba ulang...");
        ntpRetries--;
        delay(2000); // Tunggu 2 detik sebelum mencoba lagi
    }
    Serial.println("Gagal sinkronisasi NTP setelah 5 kali percobaan.");
    return false;
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
                Serial.println("fetching");
            }
        } else {
            // Reset flag jika waktu sudah tidak cocok lagi
            if (scheduleTriggered) {
                Serial.println("Schedule no longer triggered.");
                scheduleTriggered = false;
            }
        }
    }
}
void getMsg(Messages::Message &msg){
    // Gunakan token perangkat
    msg.token(deviceConfig.fcmToken); // Ganti dengan FCM Token perangkat tujuan

    // Membuat notifikasi sederhana
    Messages::Notification notification;
    String titled = "Dari " + String(deviceConfig.deviceName);
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
void setData(){
    String path = "/users/" + String(deviceConfig.userId) + "/";
    String devicePath = path + "device/" + String(deviceConfig.deviceName) + "/";

    Messages::Message msg;
    getMsg(msg);
    Serial.println("Sending Message. . .");
    messaging.send(aClient,Messages::Parent(FIREBASE_PROJECT_ID), msg, asyncCB, "fcmSendTask");

    // Update device type and location
    Database.set<String>(aClient, path + "role", "user", asyncCB, "setRoleTask");
    Database.set<String>(aClient, devicePath + "deviceType", String(deviceConfig.deviceType), asyncCB, "setDeviceTypeTask");
    Database.set<String>(aClient, devicePath + "deviceLocation", String(deviceConfig.deviceLocation), asyncCB, "setDeviceLocationTask");
    Database.get(aClient, path + "schedule" , asyncCB, true /* SSE mode (HTTP Streaming) */, "streamTask");
}
void updateData() {
    if (WiFi.status() == WL_CONNECTED) {
        String path = "/users/" + String(deviceConfig.userId) + "/device/" + String(deviceConfig.deviceName) + "/";
        if (String(deviceConfig.deviceType) == "Sensor") {
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
        } else if (String(deviceConfig.deviceType) == "Light") {
            float brightness = readBrightness();
            Database.set<float>(aClient, path + "brightness", brightness, asyncCB, "setBrightnessTask");
        } else if (String(deviceConfig.deviceType) == "Socket") {
            bool isOn = getSocketStatus();
            Database.set<bool>(aClient, path + "isOn", isOn, asyncCB, "setSocketStatusTask");
        }
    }
}
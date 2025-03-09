#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#define RESET_PIN 4

#include <WiFi.h>
#include "Config.h"

extern bool isAPMode;

void connectToWiFi();
void resetConfig();
void checkResetButton();
void startAccessPoint();

#endif

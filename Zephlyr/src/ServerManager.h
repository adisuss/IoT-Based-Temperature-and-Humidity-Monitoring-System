#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <WebServer.h>
#include <ArduinoJson.h>
#include "WiFiManager.h"

// Deklarasi Web Server
extern WebServer server;

// Fungsi untuk memulai server
void setupServer();
void handleConnect();

#endif

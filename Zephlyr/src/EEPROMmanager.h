#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <Preferences.h>
#include "Config.h"

extern Preferences preferences;

void saveConfigToEEPROM();
void loadConfigFromEEPROM();
bool isWiFiConfigured();

#endif

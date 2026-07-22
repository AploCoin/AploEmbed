#ifndef APLO_EXAMPLE_WIFI_H
#define APLO_EXAMPLE_WIFI_H

#include <Arduino.h>
#include <AploPlatform.h>

bool boardWifiConnect(const char *ssid, const char *password, unsigned long timeoutMs);
const char *boardWifiName();

bool exampleWifiConnect(const char *ssid, const char *password,
                        uint8_t maxAttempts, unsigned long timeoutMs);
bool exampleWifiEnsureConnected(const char *ssid, const char *password,
                                uint8_t maxAttempts, unsigned long timeoutMs,
                                unsigned long retryDelayMs);

#endif

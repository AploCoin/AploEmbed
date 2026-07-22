#ifndef BOARD_WIFI_H
#define BOARD_WIFI_H
#include <AploPlatform.h>
const char *boardWifiName() { return "ESP32"; }
bool boardWifiConnect(const char *ssid, const char *password, unsigned long timeoutMs)
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(250);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) { delay(250); Serial.print('.'); }
    return WiFi.status() == WL_CONNECTED;
}
#endif

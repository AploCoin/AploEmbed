#ifndef BOARD_WIFI_H
#define BOARD_WIFI_H
#include <AploPlatform.h>
inline WiFiEventHandler &wifiDisconnectLogger()
{
    static WiFiEventHandler handler = WiFi.onStationModeDisconnected(
        [](const WiFiEventStationModeDisconnected &event) {
            Serial.print("\nWiFi disconnected from SSID=");
            Serial.print(event.ssid);
            Serial.print(", reason=");
            Serial.println(event.reason);
        });
    return handler;
}
const char *boardWifiName() { return "ESP8266"; }
bool boardWifiConnect(const char *ssid, const char *password, unsigned long timeoutMs)
{
    (void)wifiDisconnectLogger();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    // Do not disconnect/reset before the first begin(): the ESP8266 may report
    // AUTH_EXPIRE (reason=2) once and still complete association shortly after.
    WiFi.mode(WIFI_STA);
    delay(100);
    WiFi.begin(ssid, password);
    const unsigned long effectiveTimeout = (timeoutMs < 45000UL) ? 45000UL : timeoutMs;
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < effectiveTimeout) {
        delay(250);
        Serial.print('.');
        yield();
    }
    return WiFi.status() == WL_CONNECTED;
}
#endif

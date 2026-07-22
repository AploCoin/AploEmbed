#include "ExampleWifi.h"

static unsigned long lastWifiRetryMs = 0;

bool exampleWifiConnect(const char *ssid, const char *password,
                        uint8_t maxAttempts, unsigned long timeoutMs)
{
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.print("Connecting to WiFi on ");
    Serial.print(boardWifiName());
    Serial.print(": ");
    Serial.println(ssid);

    for (uint8_t attempt = 1; attempt <= maxAttempts; ++attempt) {
        Serial.print("WiFi attempt ");
        Serial.print(attempt);
        Serial.print('/');
        Serial.println(maxAttempts);

        if (boardWifiConnect(ssid, password, timeoutMs)) {
            Serial.println("\nWiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            lastWifiRetryMs = 0;
            return true;
        }

        Serial.print("\nWiFi attempt failed. status=");
        Serial.println(WiFi.status());
        if (attempt < maxAttempts) delay(1000);
    }

    Serial.println("WiFi connect failed after all attempts.");
    return false;
}

bool exampleWifiEnsureConnected(const char *ssid, const char *password,
                                uint8_t maxAttempts, unsigned long timeoutMs,
                                unsigned long retryDelayMs)
{
    if (WiFi.status() == WL_CONNECTED) return true;

    const unsigned long now = millis();
    if (lastWifiRetryMs != 0 && now - lastWifiRetryMs < retryDelayMs) return false;
    lastWifiRetryMs = now;

    Serial.println("WiFi disconnected; pausing work and reconnecting...");
    return exampleWifiConnect(ssid, password, maxAttempts, timeoutMs);
}

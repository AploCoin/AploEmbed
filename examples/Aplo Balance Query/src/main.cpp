#include <Arduino.h>
#include <AploPlatform.h>
#include <Web3.h>
#include <AploContracts.h>
#include <Util.h>

using std::string;

extern const char *ssid;
extern const char *password;

#if defined(ESP8266)
static WiFiEventHandler wifiDisconnectHandler;
static bool wifiHandlerRegistered = false;
#endif

static bool connectWifi(unsigned long timeoutMs)
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

#if defined(ESP8266)
    if (!wifiHandlerRegistered) {
        wifiDisconnectHandler = WiFi.onStationModeDisconnected(
            [](const WiFiEventStationModeDisconnected &event) {
                Serial.print(F("WiFi disconnected, reason="));
                Serial.println(event.reason);
            });
        wifiHandlerRegistered = true;
    }
    WiFi.mode(WIFI_STA);
    WiFi.config(IPAddress(0U), IPAddress(0U), IPAddress(0U));
    const unsigned long effectiveTimeout = timeoutMs < 45000UL ? 45000UL : timeoutMs;
#else
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(250);
    WiFi.mode(WIFI_STA);
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    WiFi.setSleep(false);
#endif
    const unsigned long effectiveTimeout = timeoutMs;
#endif

    WiFi.begin(ssid, password);
    const unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < effectiveTimeout) {
        delay(250);
#if defined(ESP8266)
        yield();
#endif
    }

    if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(static_cast<uint32_t>(0))) {
        Serial.print(F("WiFi failed, status="));
        Serial.println(WiFi.status());
        return false;
    }

    Serial.print(F("WiFi connected: "));
    Serial.println(WiFi.localIP());
    return true;
}

static void beginSerial()
{
    Serial.begin(115200);
    delay(1000);
}

// WiFi credentials - replace with your network details
const char *ssid = "<YOUR_SSID>";
const char *password = "<YOUR_WIFI_PASSWORD>";

// Example AploCoin address - replace with your address
#define APLO_ADDRESS "0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb"

#if defined(ESP8266)
Web3 *web3 = nullptr;
#else
Web3 web3Instance;
Web3 *web3 = &web3Instance;
#endif

void queryBalance();

void setup()
{
    beginSerial();
    Serial.println(F("\n\n=== AploEmbed Balance Query Example ===\n"));
    
    while (!connectWifi(45000UL)) {
        Serial.println(F("WiFi unavailable; retrying in 5 seconds..."));
        delay(5000);
    }
#if defined(ESP8266)
    static Web3 esp8266Web3Instance;
    web3 = &esp8266Web3Instance;
#endif
    
    
    queryBalance();
}

void loop()
{
    delay(10000);
}

void queryBalance()
{
    string address = APLO_ADDRESS;
    uint256_t balance = web3->AploGetBalance(&address);
    string formatted = Util::ConvertWeiToEthString(&balance, 18);

    Serial.print(F("Address: "));
    Serial.println(address.c_str());
    Serial.print(F("Balance: "));
    Serial.print(formatted.c_str());
    Serial.println(F(" APLO"));
    Serial.print(F("Raw: "));
    Serial.print(balance.str().c_str());
    Serial.println(F(" Gaplo"));
}

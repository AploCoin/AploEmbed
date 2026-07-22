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
#endif

static void initWifi()
{
#if defined(ESP8266)
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(
        [](const WiFiEventStationModeDisconnected &event) {
            Serial.print(F("WiFi disconnected, reason="));
            Serial.println(event.reason);
        });
#endif
}

static bool connectWifiOnce(unsigned long timeoutMs)
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
#if defined(ESP8266)
    WiFi.mode(WIFI_STA);
    WiFi.config(IPAddress(static_cast<uint32_t>(0)), IPAddress(static_cast<uint32_t>(0)), IPAddress(static_cast<uint32_t>(0)));
    const unsigned long effectiveTimeout = timeoutMs < 45000UL ? 45000UL : timeoutMs;
#elif defined(ESP32)
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
    while (millis() - startedAt < effectiveTimeout) {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(static_cast<uint32_t>(0))) return true;
        delay(250);
#if defined(ESP8266)
        yield();
#endif
    }
    return false;
}

static bool connectWifi(uint8_t maxAttempts, unsigned long timeoutMs)
{
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(static_cast<uint32_t>(0))) return true;

    for (uint8_t attempt = 0; attempt < maxAttempts; ++attempt) {
        if (connectWifiOnce(timeoutMs)) {
            Serial.print(F("WiFi connected: "));
            Serial.println(WiFi.localIP());
            return true;
        }
        if (attempt + 1 < maxAttempts) delay(1000);
    }

    Serial.print(F("WiFi failed, status="));
    Serial.println(WiFi.status());
    return false;
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

Web3 web3Instance;
Web3 *web3 = &web3Instance;

void queryAploBalance();
void queryGaploBalance();

void setup()
{
    beginSerial();
    Serial.println(F("\n\n=== AploEmbed Balance Query Example ===\n"));
    initWifi();

    while (!connectWifi(3, 20000)) {
        Serial.println(F("WiFi unavailable; retrying in 5 seconds..."));
        delay(5000);
    }

    // Initialize Web3 with default AploCoin RPC endpoints
    // Uses pub1.aplocoin.com as primary, pub2.aplocoin.com as fallback
    // Web3 auto-selects the bundled root CA for HTTPS RPC endpoints.



    // Query balance in both units
    queryAploBalance();
    queryGaploBalance();
}

void loop()
{
    // Balance queries are typically done once or on-demand
    // You can add periodic queries here if needed
    delay(10000);
}

/**
 * Query balance in APLO (human-readable format)
 * 1 APLO = 10^18 Gaplo (similar to ETH/wei relationship)
 */
void queryAploBalance()
{

    string address = APLO_ADDRESS;

    // Get balance as human-readable APLO string (18 decimals)
    string balanceAplo = web3->AploGetBalanceInAplo(&address);

    Serial.print(F("Address: "));
    Serial.println(address.c_str());
    Serial.print(F("Balance: "));
    Serial.print(balanceAplo.c_str());
    Serial.println(F(" APLO"));
    Serial.println();
}

/**
 * Query balance in Gaplo (raw blockchain units / wei equivalent)
 * This is the native unit stored on the blockchain
 */
void queryGaploBalance()
{

    string address = APLO_ADDRESS;

    // Get balance in Gaplo (wei) - raw blockchain units
    uint256_t balanceGaplo = web3->AploGetBalance(&address);

    Serial.print(F("Address: "));
    Serial.println(address.c_str());
    Serial.print(F("Balance: "));
    Serial.print(balanceGaplo.str().c_str());
    Serial.println(F(" Gaplo"));

    // You can also convert manually using Util helper
    string balanceAplo = Util::ConvertWeiToEthString(&balanceGaplo, 18);
    Serial.print(F("Converted: "));
    Serial.print(balanceAplo.c_str());
    Serial.println(F(" APLO"));
    Serial.println();
}

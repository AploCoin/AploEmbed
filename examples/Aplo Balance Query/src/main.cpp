#include <Arduino.h>
#include <AploPlatform.h>
#include <Web3.h>
#include <AploContracts.h>
#include <Util.h>

using std::string;

extern const char *ssid;
extern const char *password;

// WiFi handling is kept in this sketch so the example is directly reusable.
#if defined(ESP8266)
static WiFiEventHandler wifiConnectedHandler;
static WiFiEventHandler wifiDisconnectHandler;
static WiFiEventHandler wifiGotIpHandler;
static WiFiEventHandler wifiDhcpTimeoutHandler;
static volatile bool wifiStationAssociated = false;
static volatile bool wifiGotIp = false;
static const unsigned long ESP8266_WIFI_DHCP_TIMEOUT_MS = 60000UL;
#endif

static const char *boardName()
{
#if defined(ESP8266)
    return "ESP8266";
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    return "ESP32-C3";
#else
    return "ESP32";
#endif
}

static void initWifiDiagnostics()
{
#if defined(ESP8266)
    wifiConnectedHandler = WiFi.onStationModeConnected(
        [](const WiFiEventStationModeConnected &event) {
            wifiStationAssociated = true;
            Serial.print("\nESP8266 associated with AP on channel ");
            Serial.println(event.channel);
        });
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(
        [](const WiFiEventStationModeDisconnected &event) {
            wifiStationAssociated = false;
            wifiGotIp = false;
            Serial.print("\nWiFi disconnected from SSID=");
            Serial.print(event.ssid);
            Serial.print(", reason=");
            Serial.println(event.reason);
        });
    wifiGotIpHandler = WiFi.onStationModeGotIP(
        [](const WiFiEventStationModeGotIP &event) {
            wifiStationAssociated = true;
            wifiGotIp = true;
            Serial.print("\nESP8266 DHCP address: ");
            Serial.println(event.ip);
        });
    wifiDhcpTimeoutHandler = WiFi.onStationModeDHCPTimeout([]() {
        Serial.println("\nESP8266 DHCP timeout");
    });
#endif
}

static bool connectWifiOnce(unsigned long timeoutMs)
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
#if defined(ESP8266)
    // Force DHCP mode in case an earlier sketch left a stale static-IP config.
    // Samsung may show the station as associated before DHCP has completed.
    WiFi.mode(WIFI_STA);
    WiFi.config(IPAddress(0U), IPAddress(0U), IPAddress(0U));
    wifiStationAssociated = false;
    wifiGotIp = false;
    delay(100);
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
    const unsigned long start = millis();
#if defined(ESP8266)
    unsigned long associatedAt = 0;
    while (!wifiGotIp) {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0U)) {
            wifiGotIp = true;
            break;
        }
        if (wifiStationAssociated && associatedAt == 0) associatedAt = millis();
        const unsigned long allowed = associatedAt == 0
            ? effectiveTimeout
            : effectiveTimeout + ESP8266_WIFI_DHCP_TIMEOUT_MS;
        if (millis() - start >= allowed) break;
        delay(250);
        Serial.print('.');
        yield();
    }
    return wifiGotIp && WiFi.localIP() != IPAddress(0U);
#else
    while (WiFi.status() != WL_CONNECTED && millis() - start < effectiveTimeout) {
        delay(250);
        Serial.print('.');
    }
    return WiFi.status() == WL_CONNECTED;
#endif
}

static bool connectWifi(uint8_t maxAttempts, unsigned long timeoutMs)
{
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.print("Connecting to WiFi on ");
    Serial.print(boardName());
    Serial.print(": ");
    Serial.println(ssid);
    for (uint8_t attempt = 1; attempt <= maxAttempts; ++attempt) {
        Serial.print("WiFi attempt ");
        Serial.print(attempt);
        Serial.print('/');
        Serial.println(maxAttempts);
        if (connectWifiOnce(timeoutMs)) {
            Serial.println("\nWiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            return true;
        }
        Serial.print("\nWiFi attempt failed. status=");
        Serial.println(WiFi.status());
        if (attempt < maxAttempts) delay(1000);
    }
    Serial.println("WiFi connect failed after all attempts.");
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
    Serial.println("\n\n=== AploEmbed Balance Query Example ===\n");
    initWifiDiagnostics();

    while (!connectWifi(3, 20000)) {
        Serial.println("WiFi unavailable; retrying in 5 seconds...");
        delay(5000);
    }

    // Initialize Web3 with default AploCoin RPC endpoints
    // Uses pub1.aplocoin.com as primary, pub2.aplocoin.com as fallback
    // Web3 auto-selects the bundled root CA for HTTPS RPC endpoints.


    Serial.println("Web3 initialized with AploCoin RPC endpoints");
    Serial.println("Primary: pub1.aplocoin.com");
    Serial.println("Fallback: pub2.aplocoin.com");
    Serial.println("TLS: auto root CA resolution enabled\n");

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
    Serial.println("--- Querying Balance in APLO ---");

    string address = APLO_ADDRESS;

    // Get balance as human-readable APLO string (18 decimals)
    string balanceAplo = web3->AploGetBalanceInAplo(&address);

    Serial.print("Address: ");
    Serial.println(address.c_str());
    Serial.print("Balance: ");
    Serial.print(balanceAplo.c_str());
    Serial.println(" APLO");
    Serial.println();
}

/**
 * Query balance in Gaplo (raw blockchain units / wei equivalent)
 * This is the native unit stored on the blockchain
 */
void queryGaploBalance()
{
    Serial.println("--- Querying Balance in Gaplo (wei) ---");

    string address = APLO_ADDRESS;

    // Get balance in Gaplo (wei) - raw blockchain units
    uint256_t balanceGaplo = web3->AploGetBalance(&address);

    Serial.print("Address: ");
    Serial.println(address.c_str());
    Serial.print("Balance: ");
    Serial.print(balanceGaplo.str().c_str());
    Serial.println(" Gaplo");

    // You can also convert manually using Util helper
    string balanceAplo = Util::ConvertWeiToEthString(&balanceGaplo, 18);
    Serial.print("Converted: ");
    Serial.print(balanceAplo.c_str());
    Serial.println(" APLO");
    Serial.println();
}

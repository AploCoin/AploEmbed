#include <Arduino.h>
#include <AploPlatform.h>
#include <Web3.h>
#include <Contract.h>
#include <Util.h>
#include <Crypto.h>

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

    if (WiFi.status() != WL_CONNECTED) {
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

// Replace the demo key and keep real private keys out of source control.
#define RECIPIENT_ADDRESS "0x007bEe82BDd9e866b2bd114780a47f2261C684E3"  // Recipient address

const char *PRIVATE_KEY = "0000000000000000000000000000000000000000000000000000000000000000";
string myAddress;

// Transaction parameters
// Amount to send in APLO (will be converted to Gaplo/wei internally)
#define SEND_AMOUNT_APLO "0.01"  // Exact decimal string; avoids floating-point money errors

// Gas parameters for AploCoin network
// Adjust these based on network conditions
#define GAS_PRICE 1000000000ULL    // 1 Gwei in wei
#define GAS_LIMIT 21000            // Standard transfer gas limit

#if defined(ESP8266)
Web3 *web3 = nullptr;
#else
Web3 web3Instance;
Web3 *web3 = &web3Instance;
#endif

void sendAploToAddress(const char *aplo, const char *destination);
uint256_t queryBalance(const char *address);

void setup()
{
    beginSerial();
    Serial.println(F("\n\n=== AploEmbed Send Transaction Example ===\n"));

    while (!connectWifi(45000UL)) {
        Serial.println(F("WiFi unavailable; retrying in 5 seconds..."));
        delay(5000);
    }
#if defined(ESP8266)
    static Web3 esp8266Web3Instance;
    web3 = &esp8266Web3Instance;
#endif

    myAddress = Crypto::PrivateKeyToAddress(PRIVATE_KEY);

    uint256_t balance = queryBalance(myAddress.c_str());
    string balanceText = Util::ConvertWeiToEthString(&balance, 18);
    Serial.print(F("My Address: "));
    Serial.println(myAddress.c_str());
    Serial.print(F("Current Balance: "));
    Serial.print(balanceText.c_str());
    Serial.println(F(" APLO\n"));

    const uint256_t sendAmount = Util::ConvertDecimalToWei(SEND_AMOUNT_APLO, 18);
    const uint256_t gasBuffer = Util::ConvertDecimalToWei("0.001", 18);
    const uint256_t requiredBalance = sendAmount + gasBuffer;

    Serial.print(F("Attempting to send: "));
    Serial.print(SEND_AMOUNT_APLO);
    Serial.println(F(" APLO"));
    Serial.print(F("To Address: "));
    Serial.println(RECIPIENT_ADDRESS);
    Serial.println();
    if (balance >= requiredBalance)
    {
        sendAploToAddress(SEND_AMOUNT_APLO, RECIPIENT_ADDRESS);
    }
    else
    {
        Serial.println(F("ERROR: Insufficient balance!"));
        Serial.print(F("Required: "));
        string requiredText = Util::ConvertWeiToEthString(&requiredBalance, 18);
        Serial.print(requiredText.c_str());
        Serial.print(F(" APLO ("));
        Serial.print(SEND_AMOUNT_APLO);
        Serial.println(F(" + 0.001 gas buffer)"));
        Serial.print(F("Available: "));
        Serial.print(balanceText.c_str());
        Serial.println(F(" APLO"));
        Serial.println(F("\nTransaction aborted for safety."));
    }
}

void loop()
{
    delay(10000);
}

uint256_t queryBalance(const char *address)
{
    string addr = address;
    return web3->AploGetBalance(&addr);
}

void sendAploToAddress(const char *aplo, const char *destination)
{
    Contract contract(web3, "");
    contract.SetPrivateKey(PRIVATE_KEY);
    uint256_t valueGaplo = Util::ConvertDecimalToWei(aplo, 18);

    string myAddr = myAddress;
    uint32_t nonce = (uint32_t)web3->EthGetTransactionCount(&myAddr);
    string destinationAddr = destination;
    string emptyData;

    string txResult = contract.SendTransaction(
        nonce,
        GAS_PRICE,
        GAS_LIMIT,
        &destinationAddr,
        &valueGaplo,
        &emptyData
    );
    string txHash = web3->getString(&txResult);

    if (txHash.length() > 0 && txHash != "0x")
    {
        Serial.print(F("Transaction Hash: "));
        Serial.println(txHash.c_str());
    }
    else
    {
        Serial.print(F("Raw result: "));
        Serial.println(txResult.c_str());
    }

    Serial.println();
}

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

// ============================================================================
// IMPORTANT SAFETY NOTES - READ BEFORE USE
// ============================================================================
//
// 1. NEVER commit your real private key to version control
// 2. Use TEST amounts only - start with small values (e.g., 0.01 APLO)
// 3. Verify recipient address carefully - transactions are irreversible
// 4. Test on a development/testnet environment first if available
// 5. Ensure sufficient balance for amount + gas fees
//
// ============================================================================

// WiFi credentials - replace with your network details
const char *ssid = "<YOUR_SSID>";
const char *password = "<YOUR_WIFI_PASSWORD>";

// Wallet configuration
// SECURITY: Replace with your actual private key. The sender address is derived
// from it at runtime, so there is no separate address value to keep in sync.
// WARNING: Keep your private key secret! Never share or commit it.
#define RECIPIENT_ADDRESS "0x007bEe82BDd9e866b2bd114780a47f2261C684E3"  // Recipient address

// CRITICAL: Replace with your actual 64-character hex private key (without 0x prefix)
// Demo key for documentation builds. Set a real 64-character private key from secure storage in production.
const char *PRIVATE_KEY = "0000000000000000000000000000000000000000000000000000000000000000";
string myAddress;

// Transaction parameters
// Amount to send in APLO (will be converted to Gaplo/wei internally)
#define SEND_AMOUNT_APLO "0.01"  // Exact decimal string; avoids floating-point money errors

// Gas parameters for AploCoin network
// Adjust these based on network conditions
#define GAS_PRICE 1000000000ULL    // 1 Gwei in wei
#define GAS_LIMIT 21000            // Standard transfer gas limit

Web3 web3Instance;
Web3 *web3 = &web3Instance;

void sendAploToAddress(const char *aplo, const char *destination);
uint256_t queryBalance(const char *address);

void setup()
{
    beginSerial();
    Serial.println(F("\n\n=== AploEmbed Send Transaction Example ===\n"));
    initWifi();

    while (!connectWifi(3, 20000)) {
        Serial.println(F("WiFi unavailable; retrying in 5 seconds..."));
        delay(5000);
    }

    // Initialize Web3 with default AploCoin RPC endpoints
    // Uses pub1.aplocoin.com as primary, pub2.aplocoin.com as fallback
    // Web3 auto-selects the bundled root CA for HTTPS RPC endpoints.



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

    // Safety check: ensure sufficient balance
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
    // Transaction is sent once in setup()
    // Add periodic logic here if needed
    delay(10000);
}

/**
 * Query account balance in APLO
 * Returns balance as double for easy comparison
 */
uint256_t queryBalance(const char *address)
{
    string addr = address;
    return web3->AploGetBalance(&addr);
}

/**
 * Send APLO to a destination address
 *
 * @param aplo Amount in APLO (human-readable units)
 * @param destination Recipient address (0x...)
 */
void sendAploToAddress(const char *aplo, const char *destination)
{
    // Create contract object for transaction signing
    // Empty address "" means this is a simple value transfer, not a contract call
    Contract contract(web3, "");
    contract.SetPrivateKey(PRIVATE_KEY);

    // Convert APLO to Gaplo (wei) - 18 decimals
    uint256_t valueGaplo = Util::ConvertDecimalToWei(aplo, 18);

    // Get current nonce (transaction count) for the sender
    string myAddr = myAddress;
    uint32_t nonce = (uint32_t)web3->EthGetTransactionCount(&myAddr);
    // Prepare transaction parameters
    string destinationAddr = destination;
    string emptyData = "";  // No data for simple transfers

    // Sign and send transaction
    string txResult = contract.SendTransaction(
        nonce,
        GAS_PRICE,
        GAS_LIMIT,
        &destinationAddr,
        &valueGaplo,
        &emptyData
    );

    // Parse transaction hash from result
    string txHash = web3->getString(&txResult);

    if (txHash.length() > 0 && txHash != "0x")
    {
        Serial.print(F("Transaction Hash: "));
        Serial.println(txHash.c_str());
    }
    else
    {
        Serial.println(F("ERROR: Transaction failed!"));
        Serial.print(F("Raw result: "));
        Serial.println(txResult.c_str());
    }

    Serial.println();
}

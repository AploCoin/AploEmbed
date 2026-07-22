#include <Arduino.h>
#include <AploPlatform.h>
#include <Web3.h>
#include <AploContracts.h>
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
// 2. Staking LOCKS your APLO in the contract - ensure you understand the mechanism
// 3. Minimum stake: 1,000 APLO to receive Gaplo mining rewards (1.0x multiplier)
// 4. Higher stakes increase only the mining reward multiplier up to 1.7x (8,000+ APLO)
// 5. Unstaking returns ALL staked APLO and resets multiplier to 0
// 6. Test with small amounts first if possible
// 7. Ensure sufficient balance for stake amount + gas fees
//
// Staking Tier Table (from AploNode builtin/aplo/aplo.go):
//   < 1,000 APLO → multiplier 0   (no mining reward)
//   ≥ 1,000 APLO → multiplier 1.0x (10/10)
//   ≥ 2,000 APLO → multiplier 1.1x (11/10)
//   ≥ 3,000 APLO → multiplier 1.2x (12/10)
//   ...
//   ≥ 8,000 APLO → multiplier 1.7x (17/10, maximum)
//
// ============================================================================

// WiFi credentials - replace with your network details
const char *ssid = "<YOUR_SSID>";
const char *password = "<YOUR_WIFI_PASSWORD>";

// Wallet configuration
// SECURITY: Replace with your actual private key. The public address is derived
// from it at runtime, so there is no separate address value to keep in sync.
// WARNING: Keep your private key secret! Never share or commit it.
// CRITICAL: Replace with your actual 64-character hex private key (without 0x prefix)
const char *PRIVATE_KEY = "0000000000000000000000000000000000000000000000000000000000000000";
string myAddress;

// Staking parameters
// Amount to stake in APLO (minimum 1,000 APLO for rewards)
#define STAKE_AMOUNT_APLO "1000"

Web3 web3Instance;
Web3 *web3 = &web3Instance;

void queryStakingStatus(const char *address);
void stakeAplo(const char *aplo);
void unstakeAplo();
uint256_t queryBalance(const char *address);

void setup()
{
    beginSerial();
    Serial.println(F("\n\n=== AploEmbed Staking Example ===\n"));
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

    // Query current staking status BEFORE staking
    queryStakingStatus(myAddress.c_str());

    const uint256_t stakeAmount = Util::ConvertDecimalToWei(STAKE_AMOUNT_APLO, 18);
    const uint256_t gasBuffer = Util::ConvertDecimalToWei("0.01", 18);
    const uint256_t requiredBalance = stakeAmount + gasBuffer;


    // Safety check: ensure sufficient balance
    if (balance >= requiredBalance)
    {
        stakeAplo(STAKE_AMOUNT_APLO);

        // Query staking status AFTER staking to show the change
        queryStakingStatus(myAddress.c_str());

        // Uncomment to test unstaking (returns ALL staked APLO)
        // Serial.println(F("\n=== Testing Unstake ===\n"));
        // unstakeAplo();
        // Serial.println(F("\n=== Final Staking Status ===\n"));
        // queryStakingStatus(myAddress.c_str());
    }
    else
    {
        Serial.println(F("ERROR: Insufficient balance!"));
        Serial.print(F("Required: "));
        string requiredText = Util::ConvertWeiToEthString(&requiredBalance, 18);
        Serial.print(requiredText.c_str());
        Serial.print(F(" APLO ("));
        Serial.print(STAKE_AMOUNT_APLO);
        Serial.println(F(" + 0.01 gas buffer)"));
        Serial.print(F("Available: "));
        Serial.print(balanceText.c_str());
        Serial.println(F(" APLO"));
        Serial.println(F("\nStaking aborted for safety."));
    }
}

void loop()
{
    // Staking operations are done once in setup()
    // Add periodic status checks here if needed
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
 * Query current staking status for an address
 * Shows: current stake amount and mining reward multiplier
 */
void queryStakingStatus(const char *address)
{
    string addr = address;
    string stakingContract = APLO_STAKING_CONTRACT;

    // Get current stake amount (in Gaplo/wei)
    uint256_t stakeGaplo = web3->AploGetStake(&stakingContract, &addr);

    // Convert to APLO for display
    string stakeStr = Util::ConvertWeiToEthString(&stakeGaplo, 18);

    Serial.print(F("Address: "));
    Serial.println(address);
    Serial.print(F("Current Stake: "));
    Serial.print(stakeStr.c_str());
    Serial.println(F(" APLO"));

    // Get mining reward multiplier (scaled by 10: 10 = 1.0x, 17 = 1.7x)
    uint256_t multiplierScaled = web3->AploGetStakeMultiplier(&stakingContract, &addr);
    int multiplierInt = static_cast<uint32_t>(multiplierScaled);

    Serial.print(F("Mining Multiplier: "));
    if (multiplierInt == 0) {
        Serial.println(F("0 (no rewards - stake below 1,000 APLO)"));
    } else {
        // Convert scaled multiplier to decimal (e.g., 10 → 1.0, 17 → 1.7)
        double multiplier = multiplierInt / 10.0;
        Serial.print(multiplier, 1);
        Serial.print(F("x ("));
        Serial.print(multiplierInt);
        Serial.println(F("/10)"));
    }

}

/**
 * Stake APLO in the staking contract
 * Locks the specified amount and sets the mining reward multiplier tier
 *
 * @param aplo Amount in APLO to stake (minimum 1,000 for rewards)
 */
void stakeAplo(const char *aplo)
{
    // Convert APLO to Gaplo (wei) - 18 decimals
    uint256_t valueGaplo = Util::ConvertDecimalToWei(aplo, 18);

    Serial.print(F("Stake Amount: "));
    Serial.print(aplo);
    Serial.println(F(" APLO"));
    // Call Web3::AploStake helper
    // This handles nonce retrieval, gas price, function encoding, signing, and submission
    string stakingContract = APLO_STAKING_CONTRACT;
    string myAddr = myAddress;
    string txHash = web3->AploStake(&stakingContract, &valueGaplo, PRIVATE_KEY, &myAddr);

    if (txHash.length() > 0 && txHash != "0x")
    {
        Serial.print(F("Transaction Hash: "));
        Serial.println(txHash.c_str());
    }
    else
    {
        Serial.println(F("ERROR: Staking transaction failed!"));
    }

    Serial.println();
}

/**
 * Unstake ALL staked APLO from the staking contract
 * Returns entire stake to the caller and resets multiplier to 0
 *
 * WARNING: This unstakes ALL your APLO, not a partial amount
 */
void unstakeAplo()
{
    string stakingContract = APLO_STAKING_CONTRACT;
    string myAddr = myAddress;
    string txHash = web3->AploUnstake(&stakingContract, PRIVATE_KEY, &myAddr);

    if (txHash.length() > 0 && txHash != "0x")
    {
        Serial.print(F("Transaction Hash: "));
        Serial.println(txHash.c_str());
    }
    else
    {
        Serial.println(F("ERROR: Unstaking transaction failed!"));
    }
}

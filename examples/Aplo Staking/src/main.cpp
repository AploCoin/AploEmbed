#include <Arduino.h>
#include <AploPlatform.h>
#include <Web3.h>
#include <AploContracts.h>
#include <Util.h>
#include <Crypto.h>

using std::string;

extern const char *ssid;
extern const char *password;

// WiFi handling is kept in this sketch so the example is directly reusable.
#if defined(ESP8266)
static WiFiEventHandler wifiDisconnectHandler;
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
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(
        [](const WiFiEventStationModeDisconnected &event) {
            Serial.print("\nWiFi disconnected from SSID=");
            Serial.print(event.ssid);
            Serial.print(", reason=");
            Serial.println(event.reason);
        });
#endif
}

static bool connectWifiOnce(unsigned long timeoutMs)
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
#if defined(ESP8266)
    // Do not reset an association that may recover after transient reason=2.
    WiFi.mode(WIFI_STA);
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
    while (WiFi.status() != WL_CONNECTED && millis() - start < effectiveTimeout) {
        delay(250);
        Serial.print('.');
#if defined(ESP8266)
        yield();
#endif
    }
    return WiFi.status() == WL_CONNECTED;
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
    Serial.println("\n\n=== AploEmbed Staking Example ===\n");
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

    myAddress = Crypto::PrivateKeyToAddress(PRIVATE_KEY);

    Serial.print("Staking Contract: ");
    Serial.println(APLO_STAKING_CONTRACT);
    Serial.println();

    uint256_t balance = queryBalance(myAddress.c_str());
    string balanceText = Util::ConvertWeiToEthString(&balance, 18);
    Serial.print("My Address: ");
    Serial.println(myAddress.c_str());
    Serial.print("Current Balance: ");
    Serial.print(balanceText.c_str());
    Serial.println(" APLO\n");

    // Query current staking status BEFORE staking
    Serial.println("=== Current Staking Status ===\n");
    queryStakingStatus(myAddress.c_str());

    const uint256_t stakeAmount = Util::ConvertDecimalToWei(STAKE_AMOUNT_APLO, 18);
    const uint256_t gasBuffer = Util::ConvertDecimalToWei("0.01", 18);
    const uint256_t requiredBalance = stakeAmount + gasBuffer;

    Serial.print("\nAttempting to stake: ");
    Serial.print(STAKE_AMOUNT_APLO);
    Serial.println(" APLO");
    Serial.println();

    // Safety check: ensure sufficient balance
    if (balance >= requiredBalance)
    {
        stakeAplo(STAKE_AMOUNT_APLO);

        // Query staking status AFTER staking to show the change
        Serial.println("\n=== Updated Staking Status ===\n");
        queryStakingStatus(myAddress.c_str());

        // Uncomment to test unstaking (returns ALL staked APLO)
        // Serial.println("\n=== Testing Unstake ===\n");
        // unstakeAplo();
        // Serial.println("\n=== Final Staking Status ===\n");
        // queryStakingStatus(myAddress.c_str());
    }
    else
    {
        Serial.println("ERROR: Insufficient balance!");
        Serial.print("Required: ");
        string requiredText = Util::ConvertWeiToEthString(&requiredBalance, 18);
        Serial.print(requiredText.c_str());
        Serial.print(" APLO (");
        Serial.print(STAKE_AMOUNT_APLO);
        Serial.println(" + 0.01 gas buffer)");
        Serial.print("Available: ");
        Serial.print(balanceText.c_str());
        Serial.println(" APLO");
        Serial.println("\nStaking aborted for safety.");
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
    Serial.println("--- Querying Staking Status ---");

    string addr = address;
    string stakingContract = APLO_STAKING_CONTRACT;

    // Get current stake amount (in Gaplo/wei)
    uint256_t stakeGaplo = web3->AploGetStake(&stakingContract, &addr);

    // Convert to APLO for display
    string stakeStr = Util::ConvertWeiToEthString(&stakeGaplo, 18);
    double stakeDbl = atof(stakeStr.c_str());

    Serial.print("Address: ");
    Serial.println(address);
    Serial.print("Current Stake: ");
    Serial.print(stakeStr.c_str());
    Serial.println(" APLO");

    // Get mining reward multiplier (scaled by 10: 10 = 1.0x, 17 = 1.7x)
    uint256_t multiplierScaled = web3->AploGetStakeMultiplier(&stakingContract, &addr);
    int multiplierInt = static_cast<uint32_t>(multiplierScaled);

    Serial.print("Mining Multiplier: ");
    if (multiplierInt == 0) {
        Serial.println("0 (no rewards - stake below 1,000 APLO)");
    } else {
        // Convert scaled multiplier to decimal (e.g., 10 → 1.0, 17 → 1.7)
        double multiplier = multiplierInt / 10.0;
        Serial.print(multiplier, 1);
        Serial.print("x (");
        Serial.print(multiplierInt);
        Serial.println("/10)");
    }

    // Show tier information
    Serial.println("\nTier Information:");
    if (stakeDbl < 1000.0) {
        Serial.println("  Below minimum stake (1,000 APLO)");
        Serial.println("  No Gaplo mining rewards");
    } else if (stakeDbl < 2000.0) {
        Serial.println("  Tier 1: 1,000-1,999 APLO → 1.0x multiplier");
    } else if (stakeDbl < 3000.0) {
        Serial.println("  Tier 2: 2,000-2,999 APLO → 1.1x multiplier");
    } else if (stakeDbl < 4000.0) {
        Serial.println("  Tier 3: 3,000-3,999 APLO → 1.2x multiplier");
    } else if (stakeDbl < 5000.0) {
        Serial.println("  Tier 4: 4,000-4,999 APLO → 1.3x multiplier");
    } else if (stakeDbl < 6000.0) {
        Serial.println("  Tier 5: 5,000-5,999 APLO → 1.4x multiplier");
    } else if (stakeDbl < 7000.0) {
        Serial.println("  Tier 6: 6,000-6,999 APLO → 1.5x multiplier");
    } else if (stakeDbl < 8000.0) {
        Serial.println("  Tier 7: 7,000-7,999 APLO → 1.6x multiplier");
    } else {
        Serial.println("  Tier 8 (MAX): 8,000+ APLO → 1.7x multiplier");
    }

    Serial.println();
}

/**
 * Stake APLO in the staking contract
 * Locks the specified amount and sets the mining reward multiplier tier
 *
 * @param aplo Amount in APLO to stake (minimum 1,000 for rewards)
 */
void stakeAplo(const char *aplo)
{
    Serial.println("--- Preparing Staking Transaction ---\n");

    // Convert APLO to Gaplo (wei) - 18 decimals
    uint256_t valueGaplo = Util::ConvertDecimalToWei(aplo, 18);

    Serial.print("Stake Amount: ");
    Serial.print(aplo);
    Serial.println(" APLO");
    Serial.print("Amount in Gaplo (wei): ");
    Serial.println(valueGaplo.str().c_str());
    Serial.println();

    Serial.println("Transaction Parameters:");
    Serial.print("  From: ");
    Serial.println(myAddress.c_str());
    Serial.print("  To (Contract): ");
    Serial.println(APLO_STAKING_CONTRACT);
    Serial.print("  Value: ");
    Serial.print(aplo);
    Serial.println(" APLO (passed to stake(uint256), transaction value is 0)");
    Serial.println();

    // Call Web3::AploStake helper
    // This handles nonce retrieval, gas price, function encoding, signing, and submission
    Serial.println("Signing and sending staking transaction...");
    string stakingContract = APLO_STAKING_CONTRACT;
    string myAddr = myAddress;
    string txHash = web3->AploStake(&stakingContract, &valueGaplo, PRIVATE_KEY, &myAddr);

    Serial.println("\n--- Staking Transaction Result ---\n");

    if (txHash.length() > 0 && txHash != "0x")
    {
        Serial.println("SUCCESS! Staking transaction sent.");
        Serial.print("Transaction Hash: ");
        Serial.println(txHash.c_str());
        Serial.println();
        Serial.println("Your APLO is now staked in the contract.");
        Serial.println("Mining multiplier tier will be updated based on total stake; base reward still comes from gas spent.");
        Serial.println("Use unstake() to retrieve your staked APLO.");
    }
    else
    {
        Serial.println("ERROR: Staking transaction failed!");
        Serial.println("Possible reasons:");
        Serial.println("  - Insufficient balance for stake amount + gas");
        Serial.println("  - Invalid contract address");
        Serial.println("  - Network connectivity issues");
        Serial.println("  - RPC endpoint unavailable");
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
    Serial.println("--- Preparing Unstaking Transaction ---\n");

    Serial.println("Transaction Parameters:");
    Serial.print("  From: ");
    Serial.println(myAddress.c_str());
    Serial.print("  To (Contract): ");
    Serial.println(APLO_STAKING_CONTRACT);
    Serial.println("  Value: 0 APLO (unstake returns staked APLO)");
    Serial.println();

    // Call Web3::AploUnstake helper
    // This handles nonce retrieval, gas price, function encoding, signing, and submission
    Serial.println("Signing and sending unstaking transaction...");
    string stakingContract = APLO_STAKING_CONTRACT;
    string myAddr = myAddress;
    string txHash = web3->AploUnstake(&stakingContract, PRIVATE_KEY, &myAddr);

    Serial.println("\n--- Unstaking Transaction Result ---\n");

    if (txHash.length() > 0 && txHash != "0x")
    {
        Serial.println("SUCCESS! Unstaking transaction sent.");
        Serial.print("Transaction Hash: ");
        Serial.println(txHash.c_str());
        Serial.println();
        Serial.println("Your staked APLO will be returned to your address.");
        Serial.println("Mining multiplier will be reset to 0.");
        Serial.println("You must stake again to receive Gaplo mining rewards.");
    }
    else
    {
        Serial.println("ERROR: Unstaking transaction failed!");
        Serial.println("Possible reasons:");
        Serial.println("  - No APLO currently staked");
        Serial.println("  - Insufficient gas");
        Serial.println("  - Network connectivity issues");
        Serial.println("  - RPC endpoint unavailable");
    }

    Serial.println();
}

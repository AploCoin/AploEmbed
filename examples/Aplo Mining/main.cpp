#include <Arduino.h>
#include <AploPlatform.h>
#include <Web3.h>
#include <AploContracts.h>
#include <Util.h>
#include <Crypto.h>

using std::string;

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
// 2. Mining requires minimum 1,000 APLO staked to receive Gaplo mining rewards
// 3. Mining submits transactions that cost gas - ensure sufficient balance
// 4. Mining difficulty adjusts dynamically based on network conditions
// 5. Block reward cooldown: 20 blocks between successful mines per address
// 6. Test with small amounts and monitor gas costs first
// 7. Stake only sets the reward level/multiplier; base reward comes from gas spent
//
// Mining Process:
//   1. Query miner_params(address) to get current difficulty and prev_hash
//   2. Generate random nonce and hash it with miner params
//   3. If hash < difficulty, submit mine(nonce) transaction
//   4. Wait for 20+ blocks before next mine attempt
//   5. Base reward is calculated from gas spent, then scaled by staking tier
//
// Reward logic (AploNode core/state_transition.go):
//   gaploUsed   = gasUsed * min(gasPrice, baseFee)
//   baseReward  = gaploUsed / GAploRewardCoef
//   finalReward = baseReward * stakingMultiplier / 10
//   if sender is not coinbase, gaploUsed is added back to the reward
//
// Staking tier table: <1,000 APLO => 0x, 1,000+ => 1.0x,
// then +0.1x per extra 1,000 APLO up to 1.7x at 8,000+ APLO.
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

// Mining parameters
#define BLOCK_COOLDOWN 20            // Minimum blocks between mine attempts
#define HASH_ATTEMPTS_PER_CYCLE 2000 // Nonces to try before checking block height (~8 expected hits at 0x00ff...)
#define CYCLE_DELAY_MS 1000          // Delay between hash cycles (ms)

// WiFi reconnect parameters
#define WIFI_CONNECT_ATTEMPTS 3
#define WIFI_ATTEMPT_TIMEOUT_MS 12000
#define WIFI_RECONNECT_ATTEMPTS 2
#define WIFI_RETRY_DELAY_MS 5000

// Construct before setup()/WiFi so ESP8266 BearSSL reserves its shared stack
// thunk before the networking heap becomes fragmented.
Web3 web3Instance;
Web3 *web3 = &web3Instance;
int wificounter = 0;
unsigned long lastWifiRetryMs = 0;

#if defined(ESP8266)
WiFiEventHandler wifiDisconnectHandler;
#endif

// Mining state
struct MinerParams {
    uint32_t lastBlock;
    uint8_t totalMined[32];
    uint8_t currentDifficulty[32];
    uint8_t prevHash[32];
    bool valid;
};

bool setup_wifi(uint8_t maxAttempts = WIFI_CONNECT_ATTEMPTS);
bool ensureWiFiConnected();
void resetWiFiRadio();
void queryStakingStatus(const char *address);
MinerParams getMinerParams(const char *address);
bool attemptMining(const char *address);
bool mineNonce(const uint8_t address[20], const MinerParams &params,
               uint8_t nonce[32], uint8_t hash[32]);
bool submitMineTransaction(const uint8_t nonce[32]);
void queryBalances(const char *address);

bool decodeAddress(const char *address, uint8_t output[20])
{
    if (address == nullptr || strlen(address) != 42 || address[0] != '0' ||
        (address[1] != 'x' && address[1] != 'X')) return false;
    for (size_t i = 2; i < 42; ++i) {
        if (!isxdigit(static_cast<unsigned char>(address[i]))) return false;
    }
    Util::ConvertHexToBytes(output, address + 2, 20);
    return true;
}

void setup()
{
    beginSerial();
    Serial.println("\n\n=== AploEmbed Mining Example ===\n");

#if defined(ESP8266)
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(
        [](const WiFiEventStationModeDisconnected &event) {
            Serial.print("\nWiFi disconnected from SSID=");
            Serial.print(event.ssid);
            Serial.print(", reason=");
            Serial.println(event.reason);
        });
#endif

    while (!setup_wifi()) {
        Serial.println("WiFi unavailable during setup; retrying in 5 seconds...");
        delay(WIFI_RETRY_DELAY_MS);
    }

    Serial.println("Web3 initialized with AploCoin RPC endpoints");
    Serial.println("Primary: pub1.aplocoin.com");
    Serial.println("Fallback: pub2.aplocoin.com");
    Serial.println("TLS: auto root CA resolution enabled\n");

    myAddress = Crypto::PrivateKeyToAddress(PRIVATE_KEY);

    Serial.print("Mining Contract: ");
    Serial.println(APLO_MINING_CONTRACT);
    Serial.print("Staking Contract: ");
    Serial.println(APLO_STAKING_CONTRACT);
    Serial.println();

    Serial.print("My Address: ");
    Serial.println(myAddress.c_str());
    queryBalances(myAddress.c_str());

    // CRITICAL: Check staking status BEFORE attempting to mine
    Serial.println("=== Checking Staking Status ===\n");
    queryStakingStatus(myAddress.c_str());

    Serial.println("\n=== Starting Mining Loop ===\n");
    Serial.println("Mining will attempt to find valid nonces and submit transactions.");
    Serial.println("Press RESET to stop.\n");
}

void loop()
{
    if (!ensureWiFiConnected()) {
        delay(CYCLE_DELAY_MS);
        return;
    }

    // Attempt mining cycle
    bool mined = attemptMining(myAddress.c_str());

    if (mined) {
        Serial.println("\nSuccessfully mined and submitted transaction!");
        Serial.println("Waiting for block cooldown before next attempt...\n");

        // Update balances after successful mine
        queryBalances(myAddress.c_str());
    }

    // Delay between mining cycles
    delay(CYCLE_DELAY_MS);
}

void resetWiFiRadio()
{
    WiFi.persistent(false);
#if defined(ESP32)
    WiFi.setSleep(false);        // modem sleep can make ESP32-C3 association flaky on some routers
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(true, true); // reset STA state and forget any cached AP config
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_STA);
    delay(200);
#else
    // ESP8266 disconnect(true) turns the radio off and erases credentials. Repeating
    // that destructive ESP32-style reset before every begin() can prevent association.
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);
    delay(200);
#endif
}

bool setup_wifi(uint8_t maxAttempts)
{
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++) {
        Serial.print("WiFi attempt ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.println(maxAttempts);

        resetWiFiRadio();
        WiFi.begin(ssid, password);

        const unsigned long startMs = millis();
        wificounter = 0;
        while (WiFi.status() != WL_CONNECTED && millis() - startMs < WIFI_ATTEMPT_TIMEOUT_MS) {
            delay(500);
            Serial.print(".");
            wificounter++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            Serial.println();
            return true;
        }

        Serial.print("\nWiFi attempt failed. status=");
        Serial.println(WiFi.status());
        if (attempt < maxAttempts) {
            Serial.println("Resetting WiFi radio and retrying...");
            delay(1000);
        }
    }

    Serial.println("WiFi connect failed after all attempts.");
    Serial.println("Check SSID/password, 2.4 GHz network, router MAC filters, WPA mode, and signal strength.");
    return false;
}

bool ensureWiFiConnected()
{
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    const unsigned long now = millis();
    if (lastWifiRetryMs != 0 && now - lastWifiRetryMs < WIFI_RETRY_DELAY_MS) {
        return false;
    }
    lastWifiRetryMs = now;

    Serial.print("\nWiFi disconnected. status=");
    Serial.println(WiFi.status());
    Serial.println("Pausing mining and reconnecting automatically...");

    if (setup_wifi(WIFI_RECONNECT_ATTEMPTS)) {
        Serial.println("WiFi reconnected; resuming mining.\n");
        lastWifiRetryMs = 0;
        return true;
    }

    Serial.println("Reconnect failed; will retry automatically.\n");
    return false;
}

void queryBalances(const char *address)
{
    string addr = address;
    string aploBalance = web3->AploGetAploBalanceString(&addr);
    string gasBalance = web3->AploGetGasBalanceString(&addr);

    Serial.print("APLO Balance: ");
    Serial.print(aploBalance.c_str());
    Serial.println(" APLO");

    Serial.print("Gas Balance (GAPLO): ");
    Serial.print(gasBalance.c_str());
    Serial.println(" GAPLO\n");
}

void queryStakingStatus(const char *address)
{
    string addr = address;
    string stakingContractAddr = APLO_STAKING_CONTRACT;

    // Query stake amount
    uint256_t stakeWei = web3->AploGetStake(&stakingContractAddr, &addr);
    string stakeAplo = Util::ConvertWeiToEthString(&stakeWei, 18);
    double stakeAmount = atof(stakeAplo.c_str());

    // Query multiplier (scaled by 10, e.g., 10 = 1.0x, 17 = 1.7x)
    uint256_t multiplierRaw = web3->AploGetStakeMultiplier(&stakingContractAddr, &addr);
    uint32_t multiplierScaled = (uint32_t)multiplierRaw;
    double multiplier = multiplierScaled / 10.0;

    Serial.print("Staked Amount: ");
    Serial.print(stakeAmount, 2);
    Serial.println(" APLO");

    Serial.print("Mining Multiplier: ");
    Serial.print(multiplier, 1);
    Serial.println("x");

    // Determine tier and display status
    if (stakeAmount < 1000.0) {
        Serial.println("\nWARNING: Stake is below 1,000 APLO");
        Serial.println("Mining reward is gated off at 0x multiplier.");
        Serial.println("Stake at least 1,000 APLO to unlock the gas-based reward level.");
        Serial.println("See 'Aplo Staking' example for staking instructions.\n");
    } else if (stakeAmount < 2000.0) {
        Serial.println("\nTier 1: gas-based mining reward at 1.0x multiplier");
        Serial.println("Stake 2,000+ APLO to increase multiplier to 1.1x\n");
    } else if (stakeAmount < 8000.0) {
        uint32_t tier = (uint32_t)(stakeAmount / 1000.0);
        Serial.print("\nTier ");
        Serial.print(tier);
        Serial.print(": gas-based mining reward at ");
        Serial.print(multiplier, 1);
        Serial.println("x multiplier");

        if (stakeAmount < 8000.0) {
            uint32_t nextTier = tier + 1;
            double nextMultiplier = (10.0 + nextTier) / 10.0;
            Serial.print("Stake ");
            Serial.print(nextTier * 1000);
            Serial.print("+ APLO to increase multiplier to ");
            Serial.print(nextMultiplier, 1);
            Serial.println("x\n");
        }
    } else {
        Serial.println("\nMAX TIER: gas-based mining reward at 1.7x multiplier (maximum)");
        Serial.println("You have reached the highest staking tier!\n");
    }
}

MinerParams getMinerParams(const char *address)
{
    MinerParams params = {};

    string addr = address;
    string miningContractAddr = APLO_MINING_CONTRACT;
    uint256_t lastBlock, currentDifficulty, totalMined, prevHash;

    bool success = web3->AploGetMinerParams(&miningContractAddr, &addr,
                                            &lastBlock, &currentDifficulty,
                                            &totalMined, &prevHash);

    if (success) {
        params.lastBlock = (uint32_t)lastBlock;
        // Decode once before the hot loop. Mining uses fixed byte arrays and
        // performs no String/std::string/vector allocation per nonce.
        string diffHex = "0x" + currentDifficulty.str(16, 64);
        string prevHashHex = "0x" + prevHash.str(16, 64);
        string totalMinedHex = "0x" + totalMined.str(16, 64);
        Util::ConvertHexToBytes(params.currentDifficulty, diffHex.c_str(), 32);
        Util::ConvertHexToBytes(params.prevHash, prevHashHex.c_str(), 32);
        Util::ConvertHexToBytes(params.totalMined, totalMinedHex.c_str(), 32);
        params.valid = true;
    } else {
        Serial.println("ERROR: failed to read miner_params(address) from RPC.");
        params.valid = false;
    }

    Serial.println("Miner Parameters:");
    Serial.print("  Last Block: ");
    Serial.println(params.lastBlock);
    Serial.print("  Total Mined: ");
    Serial.println(Util::ConvertBytesToHex(params.totalMined, 32).c_str());
    Serial.print("  Difficulty target: ");
    Serial.println(Util::ConvertBytesToHex(params.currentDifficulty, 32).c_str());
    Serial.println("  Note: lower target = harder mining; 0x00ff... is about 1 valid nonce per 256 random attempts.");

    return params;
}

bool attemptMining(const char *address)
{
    MinerParams params = getMinerParams(address);
    if (!params.valid) {
        Serial.println("Mining paused: RPC/miner params unavailable. Retrying next cycle.\n");
        return false;
    }

    int currentBlock = web3->EthBlockNumber();
    if (currentBlock <= 0) {
        Serial.println("Mining paused: current block unavailable from RPC. Retrying next cycle.\n");
        return false;
    }

    Serial.print("Current Block: ");
    Serial.println(currentBlock);

    if (currentBlock - params.lastBlock < BLOCK_COOLDOWN) {
        uint32_t blocksRemaining = BLOCK_COOLDOWN - (currentBlock - params.lastBlock);
        Serial.print("Cooldown active: ");
        Serial.print(blocksRemaining);
        Serial.println(" blocks remaining");
        return false;
    }

    Serial.println("Cooldown complete, attempting to mine...");

    uint8_t addressBytes[20];
    if (!decodeAddress(address, addressBytes)) {
        Serial.println("Mining paused: invalid derived wallet address.");
        return false;
    }

    // Fixed-size hot loop: no Arduino String, std::string, vector, or random()
    // allocations. Nonces come from the platform hardware CSPRNG.
    for (int i = 0; i < HASH_ATTEMPTS_PER_CYCLE; i++) {
        uint8_t nonce[32];
        uint8_t hash[32];
        if (!mineNonce(addressBytes, params, nonce, hash)) {
            Serial.println("Mining paused: hardware CSPRNG unavailable.");
            return false;
        }

        if (memcmp(hash, params.currentDifficulty, 32) < 0) {
            Serial.println("\n\nVALID NONCE FOUND!");
            Serial.print("Nonce: ");
            Serial.println(Util::ConvertBytesToHex(nonce, 32).c_str());
            Serial.print("Hash: ");
            Serial.println(Util::ConvertBytesToHex(hash, 32).c_str());
            Serial.print("Difficulty target: ");
            Serial.println(Util::ConvertBytesToHex(params.currentDifficulty, 32).c_str());
            return submitMineTransaction(nonce);
        }

        if (i % 10 == 0) Serial.print(".");
        if ((i & 0x3f) == 0) delay(0);
    }

    Serial.println("\nNo valid nonce found in this cycle.");
    return false;
}

bool mineNonce(const uint8_t address[20], const MinerParams &params,
               uint8_t nonce[32], uint8_t hash[32])
{
    if (!Crypto::RandomBytes(nonce, sizeof(nonce[0]) * 32)) return false;

    uint8_t packed[148];
    memcpy(packed, address, 20);
    memcpy(packed + 20, nonce, 32);
    memcpy(packed + 52, params.currentDifficulty, 32);
    memcpy(packed + 84, params.prevHash, 32);
    memcpy(packed + 116, params.totalMined, 32);
    Crypto::Keccak256(packed, sizeof(packed), hash);
    return true;
}

bool submitMineTransaction(const uint8_t nonce[32])
{
    Serial.println("\nSubmitting mine transaction...");

    string miningContractAddr = APLO_MINING_CONTRACT;
    string nonceStr = Util::ConvertBytesToHex(nonce, 32);
    string myAddr = myAddress;
    string txHash = web3->AploMine(&miningContractAddr, &nonceStr, PRIVATE_KEY, &myAddr);

    if (txHash.empty() || txHash == "0x" || txHash == "0x0") {
        Serial.println("Transaction failed!");
        return false;
    }

    Serial.print("Transaction submitted: ");
    Serial.println(txHash.c_str());

    return true;
}

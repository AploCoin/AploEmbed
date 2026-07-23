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
static unsigned long lastWifiRetryMs = 0;

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
            lastWifiRetryMs = 0;
            return true;
        }
        if (attempt + 1 < maxAttempts) delay(1000);
    }

    Serial.print(F("WiFi failed, status="));
    Serial.println(WiFi.status());
    return false;
}

static bool ensureWifiConnected(uint8_t maxAttempts, unsigned long timeoutMs,
                                unsigned long retryDelayMs)
{
    if (WiFi.status() == WL_CONNECTED) return true;
    const unsigned long now = millis();
    if (lastWifiRetryMs != 0 && now - lastWifiRetryMs < retryDelayMs) return false;
    lastWifiRetryMs = now;
    Serial.println(F("WiFi disconnected; pausing work and reconnecting..."));
    return connectWifi(maxAttempts, timeoutMs);
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
#define HASH_ATTEMPTS_PER_CYCLE 2000 // Revalidate the challenge between bounded 2,000-nonce batches
#define CYCLE_DELAY_MS 1000          // Delay between hash cycles (ms)
#define RECEIPT_POLL_ATTEMPTS 30
#define RECEIPT_POLL_DELAY_MS 1000

// WiFi reconnect parameters
#define WIFI_CONNECT_ATTEMPTS 3
#define WIFI_ATTEMPT_TIMEOUT_MS 45000
#define WIFI_RECONNECT_ATTEMPTS 2
#define WIFI_RETRY_DELAY_MS 5000

#if defined(ESP8266)
Web3 *web3 = nullptr;
#else
Web3 web3Instance;
Web3 *web3 = &web3Instance;
#endif


// Mining state
struct MinerParams {
    uint32_t lastBlock;
    uint8_t totalMined[32];
    uint8_t currentDifficulty[32];
    uint8_t prevHash[32];
    bool valid;
};

enum MineConfirmation {
    MINE_PENDING,
    MINE_CONFIRMED,
    MINE_REVERTED
};

void queryStakingStatus(const char *address);
MinerParams getMinerParams(const char *address, bool printDetails = true);
bool attemptMining(const char *address);
bool mineNonce(const uint8_t address[20], const MinerParams &params,
               uint8_t nonce[32], uint8_t hash[32]);
void mineHash(const uint8_t address[20], const uint8_t nonce[32],
              const MinerParams &params, uint8_t hash[32]);
bool minerParamsMatch(const MinerParams &left, const MinerParams &right);
bool submitMineTransaction(const uint8_t nonce[32]);
MineConfirmation waitForMineConfirmation(const string &txHash);
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
    Serial.println(F("\n\n=== AploEmbed Mining Example ===\n"));
    initWifi();

    while (!connectWifi(WIFI_CONNECT_ATTEMPTS, WIFI_ATTEMPT_TIMEOUT_MS)) {
        Serial.println(F("WiFi unavailable during setup; retrying in 5 seconds..."));
        delay(WIFI_RETRY_DELAY_MS);
    }

#if defined(ESP8266)
    // Allocate the TLS/Web3 state only after WiFi association/DHCP.
    static Web3 esp8266Web3Instance;
    web3 = &esp8266Web3Instance;
#endif


    myAddress = Crypto::PrivateKeyToAddress(PRIVATE_KEY);

    Serial.print(F("My Address: "));
    Serial.println(myAddress.c_str());
    queryBalances(myAddress.c_str());

    // CRITICAL: Check staking status BEFORE attempting to mine
    queryStakingStatus(myAddress.c_str());

}

void loop()
{
    if (!ensureWifiConnected(WIFI_RECONNECT_ATTEMPTS, WIFI_ATTEMPT_TIMEOUT_MS, WIFI_RETRY_DELAY_MS)) {
        delay(CYCLE_DELAY_MS);
        return;
    }

    bool mined = attemptMining(myAddress.c_str());

    if (mined) {
        Serial.println(F("\nSuccessfully mined and confirmed transaction!"));
        Serial.println(F("Waiting for block cooldown before next attempt...\n"));
        queryBalances(myAddress.c_str());
    }

    // Delay between mining cycles
    delay(CYCLE_DELAY_MS);
}

void queryBalances(const char *address)
{
    string addr = address;
    string aploBalance = web3->AploGetAploBalanceString(&addr);
    string gasBalance = web3->AploGetGasBalanceString(&addr);

    Serial.print(F("APLO Balance: "));
    Serial.print(aploBalance.c_str());
    Serial.println(F(" APLO"));

    Serial.print(F("Gas Balance (GAPLO): "));
    Serial.print(gasBalance.c_str());
    Serial.println(F(" GAPLO\n"));
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

    Serial.print(F("Staked Amount: "));
    Serial.print(stakeAmount, 2);
    Serial.println(F(" APLO"));

    Serial.print(F("Mining Multiplier: "));
    Serial.print(multiplier, 1);
    Serial.println(F("x"));

    if (stakeAmount < 1000.0) {
        Serial.println(F("Mining rewards require at least 1,000 APLO staked"));
    }
}

MinerParams getMinerParams(const char *address, bool printDetails)
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

        if (printDetails) {
            Serial.println(F("Miner Parameters:"));
            Serial.print(F("  Last Block: "));
            Serial.println(params.lastBlock);
            Serial.print(F("  Total Mined: "));
            Serial.println(totalMined.str(10).c_str());
            Serial.print(F("  Difficulty target: "));
            Serial.println(diffHex.c_str());
            Serial.println(F("  Note: lower target = harder mining; 0x00ff... is about 1 valid nonce per 256 random attempts."));
        }
    } else {
        Serial.println(F("ERROR: failed to read miner_params(address) from RPC."));
        params.valid = false;
    }

    return params;
}

bool attemptMining(const char *address)
{
    MinerParams params = getMinerParams(address);
    if (!params.valid) {
        Serial.println(F("Mining paused: RPC/miner params unavailable. Retrying next cycle.\n"));
        return false;
    }

    int currentBlock = web3->EthBlockNumber();
    if (currentBlock <= 0) {
        Serial.println(F("Mining paused: current block unavailable from RPC. Retrying next cycle.\n"));
        return false;
    }

    Serial.print(F("Current Block: "));
    Serial.println(currentBlock);

    const uint32_t currentBlockNumber = static_cast<uint32_t>(currentBlock);
    const uint32_t blocksSinceLastMine = currentBlockNumber >= params.lastBlock
                                            ? currentBlockNumber - params.lastBlock
                                            : 0;
    if (blocksSinceLastMine < BLOCK_COOLDOWN) {
        const uint32_t blocksRemaining = BLOCK_COOLDOWN - blocksSinceLastMine;
        Serial.print(F("Cooldown active: "));
        Serial.print(blocksRemaining);
        Serial.println(F(" blocks remaining"));
        return false;
    }

    Serial.println(F("Cooldown complete, attempting to mine..."));

    uint8_t addressBytes[20];
    if (!decodeAddress(address, addressBytes)) {
        Serial.println(F("Mining paused: invalid derived wallet address."));
        return false;
    }

    // Fixed-size hot loop: no Arduino String, std::string, vector, or random()
    // allocations. Nonces come from the platform hardware CSPRNG.
    for (int i = 0; i < HASH_ATTEMPTS_PER_CYCLE; i++) {
        uint8_t nonce[32];
        uint8_t hash[32];
        if (!mineNonce(addressBytes, params, nonce, hash)) {
            Serial.println(F("Mining paused: hardware CSPRNG unavailable."));
            return false;
        }

        if (memcmp(hash, params.currentDifficulty, 32) < 0) {
            Serial.println(F("\n\nVALID NONCE FOUND!"));
            Serial.print(F("Nonce: "));
            Serial.println(Util::ConvertBytesToHex(nonce, 32).c_str());
            Serial.print(F("Hash: "));
            Serial.println(Util::ConvertBytesToHex(hash, 32).c_str());
            Serial.print(F("Difficulty target: "));
            Serial.println(Util::ConvertBytesToHex(params.currentDifficulty, 32).c_str());

            // Another miner can update the global challenge while this device
            // searches. Never spend gas on work that is already known to be stale.
            MinerParams freshParams = getMinerParams(address, false);
            if (!freshParams.valid) {
                Serial.println(F("Mining submission paused: unable to revalidate miner params."));
                return false;
            }
            if (!minerParamsMatch(params, freshParams)) {
                Serial.println(F("Discarding stale nonce: mining challenge changed."));
                return false;
            }

            uint8_t freshHash[32];
            mineHash(addressBytes, nonce, freshParams, freshHash);
            if (memcmp(freshHash, freshParams.currentDifficulty, 32) >= 0) {
                Serial.println(F("Discarding nonce: proof failed final local validation."));
                return false;
            }
            return submitMineTransaction(nonce);
        }

        if (i % 10 == 0) Serial.print(F("."));
        if ((i & 0x3f) == 0) delay(0);
    }

    Serial.println();
    return false;
}

bool mineNonce(const uint8_t address[20], const MinerParams &params,
               uint8_t nonce[32], uint8_t hash[32])
{
    if (!Crypto::RandomBytes(nonce, sizeof(nonce[0]) * 32)) return false;

    mineHash(address, nonce, params, hash);
    return true;
}

void mineHash(const uint8_t address[20], const uint8_t nonce[32],
              const MinerParams &params, uint8_t hash[32])
{
    uint8_t packed[148];
    memcpy(packed, address, 20);
    memcpy(packed + 20, nonce, 32);
    memcpy(packed + 52, params.currentDifficulty, 32);
    memcpy(packed + 84, params.prevHash, 32);
    memcpy(packed + 116, params.totalMined, 32);
    Crypto::Keccak256(packed, sizeof(packed), hash);
}

bool minerParamsMatch(const MinerParams &left, const MinerParams &right)
{
    return left.lastBlock == right.lastBlock &&
           memcmp(left.currentDifficulty, right.currentDifficulty, 32) == 0 &&
           memcmp(left.prevHash, right.prevHash, 32) == 0 &&
           memcmp(left.totalMined, right.totalMined, 32) == 0;
}

bool submitMineTransaction(const uint8_t nonce[32])
{
    Serial.println(F("\nSubmitting mine transaction..."));

    string miningContractAddr = APLO_MINING_CONTRACT;
    string nonceStr = Util::ConvertBytesToHex(nonce, 32);
    string myAddr = myAddress;
    string txHash = web3->AploMine(&miningContractAddr, &nonceStr, PRIVATE_KEY, &myAddr);

    if (txHash.empty() || txHash == "0x" || txHash == "0x0") {
        Serial.println(F("Transaction failed!"));
        return false;
    }

    Serial.print(F("Transaction broadcast accepted: "));
    Serial.println(txHash.c_str());
    Serial.println(F("Execution is pending; waiting for on-chain confirmation."));

    const MineConfirmation confirmation = waitForMineConfirmation(txHash);
    if (confirmation == MINE_CONFIRMED) return true;
    if (confirmation == MINE_REVERTED) {
        Serial.println(F("Mining transaction reverted on-chain."));
        return false;
    }

    Serial.println(F("Mining receipt unavailable after timeout; transaction may still be pending."));
    return false;
}

MineConfirmation waitForMineConfirmation(const string &txHash)
{
    for (uint8_t attempt = 0; attempt < RECEIPT_POLL_ATTEMPTS; ++attempt) {
        delay(RECEIPT_POLL_DELAY_MS);
        const int status = web3->EthGetTransactionReceiptStatus(&txHash);
        if (status == 1) return MINE_CONFIRMED;
        if (status == 0) return MINE_REVERTED;
        Serial.print(F("."));
#if defined(ESP8266)
        yield();
#endif
    }
    Serial.println();
    return MINE_PENDING;
}

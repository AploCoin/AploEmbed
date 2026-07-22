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

static bool ensureWifiConnected()
{
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.println(F("Reconnecting WiFi..."));
    return connectWifi(45000UL);
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
const char *PRIVATE_KEY = "0000000000000000000000000000000000000000000000000000000000000000";
string myAddress;

// Mining parameters
#define BLOCK_COOLDOWN 20            // Minimum blocks between mine attempts
#define HASH_ATTEMPTS_PER_CYCLE 2000 // Nonces to try before checking block height (~8 expected hits at 0x00ff...)
#define CYCLE_DELAY_MS 1000          // Delay between hash cycles (ms)

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
    Serial.println(F("\n\n=== AploEmbed Mining Example ===\n"));

    while (!connectWifi(45000UL)) {
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

    queryStakingStatus(myAddress.c_str());
    Serial.println(F("Mining started"));
}

void loop()
{
    if (!ensureWifiConnected()) {
        delay(CYCLE_DELAY_MS);
        return;
    }
    bool mined = attemptMining(myAddress.c_str());

    if (mined) Serial.println(F("Mine transaction submitted"));
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
    uint256_t stakeWei = web3->AploGetStake(&stakingContractAddr, &addr);
    string stakeAplo = Util::ConvertWeiToEthString(&stakeWei, 18);
    double stakeAmount = atof(stakeAplo.c_str());
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
        Serial.println(F("Stake below 1,000 APLO; mining rewards disabled"));
    } else if (stakeAmount < 2000.0) {
        Serial.println(F("Mining multiplier: 1.0x"));
    } else if (stakeAmount < 8000.0) {
        Serial.print(F("Mining multiplier: "));
        Serial.print(multiplier, 1);
        Serial.println(F("x"));

    } else {
        Serial.println(F("Mining multiplier: 1.7x"));
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
        string diffHex = "0x" + currentDifficulty.str(16, 64);
        string prevHashHex = "0x" + prevHash.str(16, 64);
        string totalMinedHex = "0x" + totalMined.str(16, 64);
        Util::ConvertHexToBytes(params.currentDifficulty, diffHex.c_str(), 32);
        Util::ConvertHexToBytes(params.prevHash, prevHashHex.c_str(), 32);
        Util::ConvertHexToBytes(params.totalMined, totalMinedHex.c_str(), 32);
        params.valid = true;
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

    if (static_cast<uint32_t>(currentBlock) < params.lastBlock + BLOCK_COOLDOWN) {
        uint32_t blocksRemaining = params.lastBlock + BLOCK_COOLDOWN - static_cast<uint32_t>(currentBlock);
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
            return submitMineTransaction(nonce);
        }

        if ((i & 0x3f) == 0) delay(0);
    }

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

    return true;
}

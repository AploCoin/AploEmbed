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

// Staking parameters
// Amount to stake in APLO (minimum 1,000 APLO for rewards)
#define STAKE_AMOUNT_APLO "1000"

#if defined(ESP8266)
Web3 *web3 = nullptr;
#else
Web3 web3Instance;
Web3 *web3 = &web3Instance;
#endif

void queryStakingStatus(const char *address);
void stakeAplo(const char *aplo);
uint256_t queryBalance(const char *address);

void setup()
{
    beginSerial();
    Serial.println(F("\n\n=== AploEmbed Staking Example ===\n"));

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
    queryStakingStatus(myAddress.c_str());

    const uint256_t stakeAmount = Util::ConvertDecimalToWei(STAKE_AMOUNT_APLO, 18);
    const uint256_t gasBuffer = Util::ConvertDecimalToWei("0.01", 18);
    const uint256_t requiredBalance = stakeAmount + gasBuffer;

    Serial.print(F("\nAttempting to stake: "));
    Serial.print(STAKE_AMOUNT_APLO);
    Serial.println(F(" APLO"));
    Serial.println();
    if (balance >= requiredBalance)
    {
        stakeAplo(STAKE_AMOUNT_APLO);
        queryStakingStatus(myAddress.c_str());
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
    delay(10000);
}

uint256_t queryBalance(const char *address)
{
    string addr = address;
    return web3->AploGetBalance(&addr);
}

void queryStakingStatus(const char *address)
{
    string addr = address;
    string stakingContract = APLO_STAKING_CONTRACT;
    uint256_t stakeGaplo = web3->AploGetStake(&stakingContract, &addr);
    string stakeStr = Util::ConvertWeiToEthString(&stakeGaplo, 18);

    Serial.print(F("Current Stake: "));
    Serial.print(stakeStr.c_str());
    Serial.println(F(" APLO"));
    uint256_t multiplierScaled = web3->AploGetStakeMultiplier(&stakingContract, &addr);
    int multiplierInt = static_cast<uint32_t>(multiplierScaled);

    Serial.print(F("Mining Multiplier: "));
    if (multiplierInt == 0) {
        Serial.println(F("0 (no rewards - stake below 1,000 APLO)"));
    } else {
        Serial.print(multiplierInt / 10);
        Serial.print('.');
        Serial.println(multiplierInt % 10);
    }
}

void stakeAplo(const char *aplo)
{
    uint256_t valueGaplo = Util::ConvertDecimalToWei(aplo, 18);
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
        Serial.println(F("Staking transaction failed"));
    }
}

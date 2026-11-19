#include <AploPlatform.h>
#include <Arduino.h>
#include <SessionKeys.h>
#include <Web3.h>
#include <ctime>

namespace {

// Configure a compatible native RPC and public owner address. No owner secret.
const char* const WIFI_SSID = "YOUR_WIFI_SSID";
const char* const WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* const OWNER = "0x0000000000000000000000000000000000000000";
const unsigned long WIFI_TIMEOUT_MS = 45000;
const uint64_t SESSION_LIFETIME_BLOCKS = 200;
const uint64_t REGISTRATION_GAS_LIMIT = 700000;

aplo::SessionKey sessionKey; // RAM only; no flash/EEPROM persistence.

bool connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_TIMEOUT_MS) {
        delay(250);
        yield();
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("WiFi timeout"));
        return false;
    }

    return true;
}

bool checked(const aplo::SessionResult& result) {
    if (!result) {
        Serial.println(result.message.c_str());
        return false;
    }

    return true;
}

bool prepareRegistration(
    aplo::SessionKeys& sessions,
    aplo::SessionPolicy& policy,
    uint256_t& chainId,
    aplo::NativeTransaction& registration
) {
    uint64_t head;
    if (!checked(sessions.ChainId(chainId)) || !checked(sessions.BlockNumber(head))) {
        return false;
    }

    if (head > UINT64_MAX - SESSION_LIFETIME_BLOCKS) {
        Serial.println(F("Block height cannot accommodate session lifetime"));
        return false;
    }

    policy.key = sessionKey.Address();
    policy.target = APLO_STAKING_CONTRACT;
    policy.selectors.push_back({{0xa9, 0x05, 0x9c, 0xbb}}); // transfer(address,uint256)
    policy.aplo = 100; // 100 APLO base units, separate from the GAplo fee budget.
    policy.gaplo = uint256_t(UINT64_C(1000000000000000000)); // 1 GAplo.
    policy.expiry = head + SESSION_LIFETIME_BLOCKS;

    std::string proof;
    if (!checked(sessions.AcceptanceProof(policy, OWNER, chainId, sessionKey, proof))) {
        return false;
    }

    return checked(
        sessions.PrepareCreate(policy, OWNER, proof, REGISTRATION_GAS_LIMIT, registration)
    );
}

void printRegistration(const aplo::NativeTransaction& registration) {
    Serial.println(F("Owner device must review and sign this zero-value registry transaction"));
    Serial.print(F("to: "));
    Serial.println(registration.to.c_str());
    Serial.print(F("chainId: "));
    Serial.println(registration.chainId.str().c_str());
    Serial.print(F("nonce: "));
    Serial.println(uint256_t(registration.nonce).str().c_str());
    Serial.print(F("gasPrice: "));
    Serial.println(registration.gasPrice.str().c_str());
    Serial.print(F("gasLimit: "));
    Serial.println(uint256_t(registration.gasLimit).str().c_str());
    Serial.print(F("data: "));
    Serial.println(registration.data.c_str()); // Public proof/calldata; never the secret.
}

} // namespace

void setup() {
    Serial.begin(115200);
    if (std::string(OWNER) == "0x0000000000000000000000000000000000000000") {
        Serial.println(F("Configure owner, RPC and WiFi first"));
        return;
    }

    if (!connectWiFi()) {
        return;
    }

    // ESP hardware entropy requires the WiFi radio to be active.
    if (!sessionKey.Generate()) {
        Serial.println(F("CSPRNG failed"));
        return;
    }

    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    while (time(nullptr) < 1609459200L) {
        delay(250);
    }
    static Web3 web3; // URLs and endpoint CA from APLO_TRUST_PROFILE.
    aplo::SessionKeys sessions(web3);
    aplo::SessionPolicy policy;
    uint256_t chainId;
    aplo::NativeTransaction registration;
    if (!prepareRegistration(sessions, policy, chainId, registration)) {
        return;
    }

    printRegistration(registration);

    // Integration continues here: retain this exact public policy and chainId.
    // Owner device: SignTransaction(registration, externalSigner, ctx, OWNER, raw).
    // SendRaw(raw, hash), then require Receipt(hash, state) == Ok and Succeeded.
    // Read(policy.key, OWNER, status) must confirm the authorized scope before
    // SendDelegated(policy, OWNER, chainId, sessionKey, tx, hash).
    // Revocation: PrepareRevoke, then external owner sign/send and confirmation.
}

void loop() {
    delay(1000);
}

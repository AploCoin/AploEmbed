// AploEmbed main header
//
// Specialized for AploCoin blockchain
// Based on Web3E by James Brown (@JamesSmartCell, @AlphaWallet)
// Original Web3 Arduino by Okada, Takahiro
//

#ifndef APLOEMBED_WEB3_H
#define APLOEMBED_WEB3_H

typedef unsigned char BYTE;
#define ETHERS_PRIVATEKEY_LENGTH 32
#define ETHERS_PUBLICKEY_LENGTH 64
#define ETHERS_ADDRESS_LENGTH 20
#define ETHERS_KECCAK256_LENGTH 32
#define ETHERS_SIGNATURE_LENGTH 65

class Web3;
class Crypto;
class KeyID;

#include "stdint.h"
#include "uint256/uint256_t.h"
#include "AploPlatform.h"
#include <string.h>
#include <string>
#include "AploContracts.h"
#include "AploTlsConfig.h"

#if defined(ESP8266)
#define APLO_ESP8266_RPC_RESPONSE_MAX 768
#endif

enum ConnectionStage { unconnected, handshake, have_token, confirmed };

class Web3 {
public:
    // Uses URLs and endpoint trust from the generated build profile.
    // Without a profile, HTTPS fails closed until setCertificate() is called.
#if APLO_ENABLE_HTTPS
    Web3();
    // A custom primary has no implicit public fallback.
    Web3(const char* primaryRpcUrl);
    Web3(const char* primaryRpcUrl, const char* fallbackRpcUrl);
    Web3(long long chainId); // Legacy: chainId remains APLO_ID.
#else
    Web3() = delete;
    Web3(const char*) = delete;
    Web3(const char*, const char*) = delete;
    Web3(long long) = delete;
#endif
    ~Web3();

    Web3(const Web3&) = delete;
    Web3& operator=(const Web3&) = delete;

    // Borrowed immutable buffers: keep PEM/bundle alive until replaced or Web3
    // is destroyed. PEM is NUL terminated; ESP8266 accepts flash/PROGMEM PEM.
    // A single PEM may contain multiple roots; every root then trusts BOTH URLs.
    void setCertificate(const char* root_ca);
    // Endpoint-specific roots, reusing one TLS client and one parsed X509List.
    // nullptr disables trust for that endpoint; no inheritance from the other.
    void setCertificate(const char* primary_ca, const char* fallback_ca);
    // ESP32 only, opt in at build time with APLO_TLS_ENABLE_BUNDLE=1.
    void setCertificateBundle(const uint8_t* bundle_start);
    void setCertificateBundle(const uint8_t* bundle_start, size_t bundle_size);
    // Compatibility name: select the explicit build profile, never discover CA.
    void setAutoCertificate();
    static bool isHttpsUrl(const char* url);
    // Compatibility name retained; unauthenticated HTTPS is unavailable.
    void setInsecure();

    enum class TlsError {
        None,
        MissingTrust,
        UnsupportedTrust,
        InvalidTrust,
        InvalidTime,
        ConnectionFailed,
        FragmentLength
    };

    TlsError tlsError() const {
        return lastTlsError;
    }

    // Raw JSON-RPC response; used by exact-width native session API.
    std::string RpcRequest(const std::string& method, const std::string& params);
    std::string Web3ClientVersion();
    std::string Web3Sha3(const std::string* data);
    int NetVersion();
    bool NetListening();
    int NetPeerCount();
    double EthProtocolVersion();
    bool EthSyncing();
    bool EthMining();
    double EthHashrate();
    long long int EthGasPrice();
    void EthAccounts(char** array, int size);
    int EthBlockNumber();
    uint256_t EthGetBalance(const std::string* address);
    int EthGetTransactionCount(const std::string* address);
    // Returns 1 for a successful receipt, 0 for a reverted receipt, and -1 while
    // pending/unavailable.
    int EthGetTransactionReceiptStatus(const std::string* transactionHash);
    std::string EthViewCall(const std::string* data, const char* to);

    std::string EthCall(
        const std::string* from,
        const char* to,
        long gas,
        long gasPrice,
        const std::string* value,
        const std::string* data
    );
    std::string EthSendSignedTransaction(const std::string* data, const uint32_t dataLen);

    // AploCoin-specific helpers
    // Balance helpers with unit conversion
    // APLO and GAplo are separate assets; each uses 10^18 base units.
    uint256_t AploGetAploBalance(const std::string* address); // APLO balance in wei units
    std::string
    AploGetAploBalanceString(const std::string* address);    // APLO balance as human-readable APLO
    uint256_t AploGetGasBalance(const std::string* address); // Gas balance in Gaplo units
    std::string
    AploGetGasBalanceString(const std::string* address); // Gas balance as human-readable GAPLO
    uint256_t
    AploGetBalance(const std::string* address); // Backward compatible alias for APLO balance
    std::string
    AploGetBalanceInAplo(const std::string* address); // Backward compatible alias for APLO balance

    // Staking operations (requires staking contract address, private key, and sender address)
    // All amounts in Gaplo (wei)
    // Returns transaction hash on success, empty std::string on failure
    std::string AploStake(
        const std::string* stakingContract,
        const uint256_t* amount,
        const char* privateKey,
        const std::string* fromAddress
    );
    std::string AploUnstake(
        const std::string* stakingContract,
        const char* privateKey,
        const std::string* fromAddress
    );
    uint256_t AploGetStake(const std::string* stakingContract, const std::string* account);
    uint256_t
    AploGetStakeMultiplier(const std::string* stakingContract, const std::string* account);

    long long int getLongLong(const std::string* json);
    std::string getString(const std::string* json);
    int getInt(const std::string* json);
    uint256_t getUint256(const std::string* json);
    long long int getChainId() const;
    std::string getResult(const std::string* json);

private:
    struct RpcEndpoint {
        std::string host;
        std::string path;
        unsigned short port;
#if defined(ESP8266)
        int tlsBufferSize;
#endif
        unsigned long retryAfterMs;
        bool valid;

        RpcEndpoint()
            : path("/"), port(443)
#if defined(ESP8266)
              ,
              tlsBufferSize(0)
#endif
              ,
              retryAfterMs(0), valid(false) {
        }
    };

    std::string exec(const std::string* data, RpcEndpoint& endpoint);
    std::string execWithFailover(const std::string* data); // RPC failover wrapper
    std::string generateJson(const std::string* method, const std::string* params);
    static bool parseEndpoint(const char* url, RpcEndpoint* endpoint, bool requireHttps = false);
    bool setupCert(const RpcEndpoint& endpoint);
    void initWeb3(const char* primaryRpc, const char* fallbackRpc);

    long getLong(const std::string* json);
    double getDouble(const std::string* json);
    bool getBool(const std::string* json);

private:
    WiFiClientSecure* client;
    const char* host;
    RpcEndpoint primaryEndpoint;
    RpcEndpoint fallbackEndpoint;
    long long chainId; // Fixed to APLO_ID (28282)

    // Certificate configuration state
    enum CertMode { CERT_PROFILE, CERT_BUNDLE, CERT_CA, CERT_UNAVAILABLE };

    CertMode certMode;
    const char* caCert;
    const uint8_t* certBundle;
    size_t certBundleSize;
    const char* fallbackCaCert;
    TlsError lastTlsError;
#if defined(ESP8266)
    BearSSL::X509List* esp8266TrustAnchor;
    const char* esp8266TrustAnchorCert;
#endif
};

#endif // APLOEMBED_WEB3_H

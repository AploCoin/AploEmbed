// AploEmbed main header
//
// Specialized for AploCoin blockchain
// Based on Web3E by James Brown (@JamesSmartCell, @AlphaWallet)
// Original Web3 Arduino by Okada, Takahiro
//

#ifndef APLOEMBED_WEB3_H
#define APLOEMBED_WEB3_H

typedef unsigned char BYTE;
#define ETHERS_PRIVATEKEY_LENGTH       32
#define ETHERS_PUBLICKEY_LENGTH        64
#define ETHERS_ADDRESS_LENGTH          20
#define ETHERS_KECCAK256_LENGTH        32
#define ETHERS_SIGNATURE_LENGTH        65

class Web3;
class Crypto;
class KeyID;

#include "stdint.h"
#include "uint256/uint256_t.h"
#include "AploPlatform.h"
#include <Contract.h>
#include <Crypto.h>
#include <KeyID.h>
#include <Util.h>
#include <string.h>
#include <string>
#include "AploContracts.h"
#include "AploCertificates.h"

enum ConnectionStage
{
    unconnected,
    handshake,
    have_token,
    confirmed
};

class Web3 {
public:
    // Default constructor: uses pub1.aplocoin.com as primary, pub2 as fallback
    // TLS uses WiFiClientSecure defaults unless a CA bundle/certificate or explicit insecure mode is configured.
    Web3();
    
    // Custom RPC constructor: specify primary RPC URL
    Web3(const char* primaryRpcUrl);
    
    // Custom RPC with fallback: specify both primary and fallback URLs
    Web3(const char* primaryRpcUrl, const char* fallbackRpcUrl);
    
    // Legacy constructor for backward compatibility (chainId ignored, always uses APLO_ID)
    Web3(long long chainId);
    
    // Certificate validation configuration
    // Call one of these methods BEFORE making any RPC calls when your platform requires explicit trust anchors.
    
    // Option 1: Use ESP32 CA certificate bundle (recommended for production)
    // Requires: board_build.embed_files = data/cert/x509_crt_bundle.bin in platformio.ini
    // See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_crt_bundle.html
    void setCertificateBundle(const uint8_t* bundle_start);
    void setCertificateBundle(const uint8_t* bundle_start, size_t bundle_size);
    
    // Option 2: Use specific CA certificate (PEM format)
    // Useful when you control the RPC endpoint and want to pin to a specific CA
    void setCertificate(const char* root_ca);

    // Option 3: Automatically pick the bundled root CA for known/common public RPC CAs.
    // Defaults to ISRG Root X1, which validates Let's Encrypt endpoints.
    void setAutoCertificate();

    // Option 4: Explicitly disable certificate validation (insecure, for testing only)
    // This is never enabled automatically.
    void setInsecure();
    
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
    std::string EthViewCall(const std::string* data, const char* to);

    std::string EthCall(const std::string* from, const char* to, long gas, long gasPrice, const std::string* value, const std::string* data);
    std::string EthSendSignedTransaction(const std::string* data, const uint32_t dataLen);

    // AploCoin-specific helpers
    // Balance helpers with unit conversion
    // Note: 1 APLO = 10^18 Gaplo (wei), similar to ETH/wei relationship
    uint256_t AploGetAploBalance(const std::string* address);  // APLO balance in wei units
    std::string AploGetAploBalanceString(const std::string* address);  // APLO balance as human-readable APLO
    uint256_t AploGetGasBalance(const std::string* address);  // Gas balance in Gaplo units
    std::string AploGetGasBalanceString(const std::string* address);  // Gas balance as human-readable GAPLO
    uint256_t AploGetBalance(const std::string* address);  // Backward compatible alias for APLO balance
    std::string AploGetBalanceInAplo(const std::string* address);  // Backward compatible alias for APLO balance
    
    // Staking operations (requires staking contract address, private key, and sender address)
    // All amounts in Gaplo (wei)
    // Returns transaction hash on success, empty std::string on failure
    std::string AploStake(const std::string* stakingContract, const uint256_t* amount, const char* privateKey, const std::string* fromAddress);
    std::string AploUnstake(const std::string* stakingContract, const char* privateKey, const std::string* fromAddress);
    uint256_t AploGetStake(const std::string* stakingContract, const std::string* account);
    uint256_t AploGetStakeMultiplier(const std::string* stakingContract, const std::string* account);
    
    // Mining helpers
    // Returns: last_block, current_difficulty, total_mined, prev_hash
    bool AploGetMinerParams(const std::string* miningContract, const std::string* minerAddress,
                           uint256_t* lastBlock, uint256_t* currentDifficulty,
                           uint256_t* totalMined, uint256_t* prevHash);
    // Submit mining transaction with valid nonce (32-byte hex std::string)
    // Returns transaction hash on success, empty std::string on failure
    std::string AploMine(const std::string* miningContract, const std::string* nonce, const char* privateKey, const std::string* fromAddress);

    long long int getLongLong(const std::string* json);
    std::string getString(const std::string* json);
    int getInt(const std::string* json);
    uint256_t getUint256(const std::string* json);
    long long int getChainId() const;
    std::string getResult(const std::string* json);

private:
    std::string exec(const std::string* data);
    std::string execWithFailover(const std::string* data);  // RPC failover wrapper
    std::string generateJson(const std::string* method, const std::string* params);
    void selectHost();
    void setupCert();
    void initWeb3(const char* primaryRpc, const char* fallbackRpc);
    
    long getLong(const std::string* json);
    double getDouble(const std::string* json);
    bool getBool(const std::string* json);

private:
    WiFiClientSecure *client;
    BYTE *mem;
    const char* host;
    const char* path;
    const char* primaryRpcUrl;
    const char* fallbackRpcUrl;
    unsigned short port;
    long long chainId;  // Fixed to APLO_ID (28282)
    
    // Certificate configuration state
    enum CertMode { CERT_AUTO_RESOLVE, CERT_AUTO, CERT_BUNDLE, CERT_CA, CERT_INSECURE };
    CertMode certMode;
    const char* caCert;
    const uint8_t* certBundle;
    size_t certBundleSize;
    const char* resolvedAutoCert;
#if defined(ESP8266)
    BearSSL::X509List* esp8266TrustAnchor;
    int esp8266TlsBufferSize;
#endif
};

#endif //APLOEMBED_WEB3_H

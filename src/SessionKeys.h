#ifndef APLO_SESSION_KEYS_H
#define APLO_SESSION_KEYS_H

#include "uint256/uint256_t.h"
#include <array>
#include <stdint.h>
#include <string>
#include <vector>

class Web3;

namespace aplo {
using Selector = std::array<uint8_t, 4>;
enum class SessionError {
    Ok,
    InvalidArgument,
    TransportError,
    RpcError,
    InvalidResponse,
    SignerError,
    Unauthorized
};

struct SessionResult {
    SessionError code;
    std::string message;

    SessionResult(SessionError error = SessionError::Ok, const std::string& diagnostic = "")
        : code(error), message(diagnostic) {}

    explicit operator bool() const {
        return code == SessionError::Ok;
    }
};
enum class ReceiptState { Pending, Succeeded, Reverted };
// Callback signs the raw 32-byte digest, WITHOUT personal-message prefix.
// Return r[32] || low-s[32] || v[1], v=0/1. Context may be an external device.
using DigestSigner = bool (*)(const uint8_t digest[32], uint8_t signature[65], void* context);
using RpcTransport = std::string (*)(const std::string& method, const std::string& params,
                                     void* context);

// RAM only. Persistence/export is explicit and is the application's responsibility.
class SessionKey {
public:
    SessionKey();
    ~SessionKey();
    SessionKey(const SessionKey&) = delete;
    SessionKey& operator=(const SessionKey&) = delete;
    bool Generate(); // Crypto::RandomBytes; enable WiFi before generating on ESP.
    bool Import(const uint8_t secret[32]); // rejects zero and scalars >= curve order
    bool Export(uint8_t secret[32]) const;
    void Clear();
    std::string Address() const;
    static bool SignDigest(const uint8_t digest[32], uint8_t signature[65], void* context);

private:
    uint8_t secret_[32];
    bool valid_;
};

struct SessionPolicy {
    std::string key;
    std::string target;
    std::vector<Selector> selectors; // ordered, distinct, 1..32
    uint256_t aplo = 0;
    uint256_t gaplo = 0; // separate assets, integer base units
    uint64_t expiry = 0; // inclusive block height, current..current+1000
};

struct SessionStatus {
    std::string owner;
    std::string target;
    uint256_t aplo = 0;
    uint256_t gaplo = 0;
    uint64_t nonce = 0;
    uint64_t expiry = 0;
    bool active = false; // false means absent/revoked/cleaned up; RPC cannot distinguish them
};

struct NativeTransaction {
    uint256_t chainId = 0;
    uint256_t gasPrice = 0;
    uint256_t value = 0;
    uint64_t nonce = 0;
    uint64_t gasLimit = 0;
    std::string to;
    std::string data = "0x";
};

class SessionKeys {
public:
    static const char* Registry();
    explicit SessionKeys(Web3& web3);

    SessionKeys(RpcTransport transport, void* context) : transport_(transport), context_(context) {}

    static SessionResult Validate(const SessionPolicy& policy, uint64_t currentBlock);
    static SessionResult ProofHash(const SessionPolicy& policy, const std::string& owner,
                                   const uint256_t& chainId, uint8_t digest[32]);
    static SessionResult AcceptanceProof(const SessionPolicy& policy, const std::string& owner,
                                         const uint256_t& chainId, SessionKey& key,
                                         std::string& proof);
    static SessionResult EncodeCreate(const SessionPolicy& policy, const std::string& proof,
                                      std::string& data);
    static SessionResult EncodeRevoke(const std::string& key, std::string& data);
    static SessionResult SignTransaction(const NativeTransaction& tx, DigestSigner signer,
                                         void* context, const std::string& expectedSigner,
                                         std::string& raw);
    // Checked envelope; result is the raw JSON value (quoted hex/object/null).
    SessionResult Request(const std::string& method, const std::string& params,
                          std::string& result);
    SessionResult Call(const std::string& from, const std::string& to, const std::string& data,
                       std::string& output, const std::string& blockTag = "latest");
    SessionResult EstimateGas(const std::string& from, const std::string& to,
                              const std::string& data, const uint256_t& value, uint64_t& gas);
    SessionResult Balance(const std::string& address, uint256_t& balance); // eth_getBalance = GAplo
    SessionResult ChainId(uint256_t& chainId);
    SessionResult BlockNumber(uint64_t& block);
    SessionResult GasPrice(uint256_t& price);
    SessionResult PendingNonce(const std::string& signerAddress, uint64_t& nonce);
    SessionResult Read(const std::string& key, const std::string& owner, SessionStatus& status);
    // Returns a hash only for accepted submission, never implies receipt success.
    SessionResult SendRaw(const std::string& raw, std::string& hash);
    // State is updated only on success; errors never imply Pending.
    SessionResult Receipt(const std::string& hash, ReceiptState& state);
    // Reads chain/head/owner pending nonce; checks expiry and proof, builds zero-value owner
    // transaction.
    SessionResult PrepareCreate(const SessionPolicy& policy, const std::string& owner,
                                const std::string& proof, uint64_t gasLimit, NativeTransaction& tx);
    SessionResult PrepareRevoke(const std::string& key, const std::string& owner, uint64_t gasLimit,
                                NativeTransaction& tx);
    // Checks live authorization, next block, pinned chain, allowed target/selector and maximum fee.
    // Caller serializes submissions for each signer; uncertain send => reconcile nonce/receipt.
    SessionResult SendDelegated(const SessionPolicy& policy, const std::string& owner,
                                const uint256_t& authorizedChainId, SessionKey& key,
                                NativeTransaction tx, std::string& hash);

public:
    SessionResult Rpc(const std::string& method, const std::string& params, std::string& result);
    SessionResult Quantity(const std::string& method, const std::string& params, uint256_t& value);

private:
    SessionResult OwnerTransaction(const std::string& owner, uint64_t gasLimit,
                                   NativeTransaction& tx);
    RpcTransport transport_;
    void* context_;
};
} // namespace aplo
#endif

#include "SessionKeys.h"
#include "internal/SessionEncoding.h"
#include "trezor/sha3.h"
#include <algorithm>
#include <cstring>

namespace aplo {
using namespace detail;
namespace {
const size_t MaxSelectors = 32;
const uint64_t MaxLifetimeBlocks = 1000;
const size_t AbiWordBytes = 32;
const size_t ProofHeadBytes = 6 * AbiWordBytes;
const size_t CreateHeadBytes = 7 * AbiWordBytes;
} // namespace

const char* SessionKeys::Registry() {
    return "0x0000000000000000000000000000000000001237";
}

bool detail::isReservedAddress(const string& value) {
    const uint256_t n(value);
    return n <= uint256_t(9) || (n >= uint256_t(0x1234) && n <= uint256_t(0x1237));
}

SessionResult detail::validatePolicyShape(const SessionPolicy& policy) {
    if (!isAddress(policy.key) || isZeroAddress(policy.key) || !isAddress(policy.target) ||
        isZeroAddress(policy.target) ||
        lowerHex(policy.target) == lowerHex(SessionKeys::Registry())) {
        return invalidArgument("invalid key/target address");
    }
    if (isReservedAddress(policy.key)) {
        return invalidArgument("reserved session key address");
    }
    if (policy.selectors.empty() || policy.selectors.size() > MaxSelectors) {
        return invalidArgument("selectors must contain 1..32 entries");
    }
    for (size_t i = 0; i < policy.selectors.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (policy.selectors[i] == policy.selectors[j]) {
                return invalidArgument("duplicate selector");
            }
        }
    }
    return {};
}

SessionResult SessionKeys::Validate(const SessionPolicy& policy, uint64_t currentBlock) {
    auto result = validatePolicyShape(policy);
    if (!result) {
        return result;
    }
    if (currentBlock > UINT64_MAX - MaxLifetimeBlocks || policy.expiry < currentBlock ||
        policy.expiry - currentBlock > MaxLifetimeBlocks) {
        return invalidArgument("expiry outside currentBlock..currentBlock+1000");
    }
    return {};
}

static string selectorsTail(const SessionPolicy& policy) {
    string encoded = abiWord(uint256_t(policy.selectors.size()));
    for (const auto& functionSelector : policy.selectors) {
        encoded += encodeHex(functionSelector.data(), 4).substr(2) + string(56, '0');
    }
    return encoded;
}

// ABI encode six proof fields; hash in chunks so there is no full hex preimage.
SessionResult SessionKeys::ProofHash(const SessionPolicy& policy, const string& owner,
                                     const uint256_t& chainId, uint8_t digest[32]) {
    auto result = validatePolicyShape(policy);
    if (!result) {
        return result;
    }
    if (!digest || !isAddress(owner) || isReservedAddress(owner) ||
        lowerHex(owner) == lowerHex(policy.key)) {
        return invalidArgument("invalid owner/digest");
    }
    SHA3_CTX hashContext;
    keccak_256_Init(&hashContext);
    const char domain[] = "APLO_SESSION_KEYS_ACCEPT_V1";
    keccak_Update(&hashContext, reinterpret_cast<const uint8_t*>(domain), sizeof(domain) - 1);
    auto append = [&hashContext](const string& value) {
        auto chunk = decodeHex("0x" + value);
        keccak_Update(&hashContext, chunk.data(), chunk.size());
    };
    append(string(Registry()).substr(2));
    append(abiWord(chainId));
    append(owner.substr(2));
    append(abiAddress(policy.key));
    append(abiAddress(policy.target));
    append(abiWord(uint256_t(ProofHeadBytes)));
    append(abiWord(policy.aplo));
    append(abiWord(policy.gaplo));
    append(abiWord(uint256_t(policy.expiry)));
    append(abiWord(uint256_t(policy.selectors.size())));
    for (const auto& functionSelector : policy.selectors) {
        append(encodeHex(functionSelector.data(), 4).substr(2) + string(56, '0'));
    }
    keccak_Final(&hashContext, digest);
    return {};
}

SessionResult SessionKeys::AcceptanceProof(const SessionPolicy& policy, const string& owner,
                                           const uint256_t& chainId, SessionKey& key,
                                           string& proof) {
    proof.clear();
    if (key.Address() != lowerHex(policy.key)) {
        return invalidArgument("session key does not match policy");
    }
    uint8_t digest[32];
    uint8_t signature[65];
    auto result = ProofHash(policy, owner, chainId, digest);
    if (!result) {
        return result;
    }
    if (!SessionKey::SignDigest(digest, signature, &key)) {
        return {SessionError::SignerError, "session proof signer failed"};
    }
    proof = encodeHex(signature, 65);
    return {};
}

string detail::functionSelector(const char* signature) {
    uint8_t hash[32];
    keccak_256(reinterpret_cast<const uint8_t*>(signature), strlen(signature), hash);
    return encodeHex(hash, 4);
}

SessionResult SessionKeys::EncodeCreate(const SessionPolicy& policy, const string& proof,
                                        string& data) {
    data.clear();
    auto result = validatePolicyShape(policy);
    if (!result) {
        return result;
    }
    if (!isHex(proof, 65)) {
        return invalidArgument("proof must be 65 bytes");
    }
    auto signature = decodeHex(proof);
    if (!canonicalSignature(signature.data())) {
        return invalidArgument("proof must use low-S and v=0/1");
    }
    data.reserve(2 + 8 + (7 + 1 + policy.selectors.size() + 4) * 64);
    data = functionSelector(
               "CreateSessionKey(address,address,bytes4[],uint256,uint256,uint256,bytes)") +
           abiAddress(policy.key) + abiAddress(policy.target) + abiWord(uint256_t(CreateHeadBytes));
    data += abiWord(policy.aplo);
    data += abiWord(policy.gaplo);
    data += abiWord(uint256_t(policy.expiry));
    data += abiWord(uint256_t(CreateHeadBytes + AbiWordBytes * (1 + policy.selectors.size())));
    data += selectorsTail(policy);
    data += abiWord(uint256_t(65));
    data += lowerHex(proof.substr(2));
    data += string(62, '0');
    return {};
}

SessionResult SessionKeys::EncodeRevoke(const string& key, string& data) {
    data.clear();
    if (!isAddress(key) || isZeroAddress(key)) {
        return invalidArgument("invalid session address");
    }
    data = functionSelector("RevokeSessionKey(address)") + abiAddress(key);
    return {};
}

SessionResult SessionKeys::Read(const string& key, const string& owner, SessionStatus& status) {
    status = SessionStatus();
    if (!isAddress(key) || !isAddress(owner) || isZeroAddress(owner)) {
        return invalidArgument("invalid key/owner address");
    }
    string raw;
    string data = functionSelector("getSession(address)") + abiAddress(key);
    auto result = Rpc("eth_call",
                      "[{\"from\":\"" + owner + "\",\"to\":\"" + Registry() + "\",\"data\":\"" +
                          data + "\"},\"latest\"]",
                      raw);
    if (!result) {
        return result;
    }
    string encoded;
    if (!unquoteHex(raw, encoded) || !isHex(encoded, 192)) {
        return {SessionError::InvalidResponse, "getSession must return six ABI words"};
    }
    if (encoded.substr(2, 24) != string(24, '0') || encoded.substr(66, 24) != string(24, '0')) {
        return {SessionError::InvalidResponse, "noncanonical ABI address"};
    }
    SessionStatus parsed;
    parsed.owner = lowerHex("0x" + encoded.substr(26, 40));
    parsed.target = lowerHex("0x" + encoded.substr(90, 40));
    parsed.aplo = uint256_t("0x" + encoded.substr(130, 64));
    parsed.gaplo = uint256_t("0x" + encoded.substr(194, 64));
    uint256_t nonce("0x" + encoded.substr(258, 64));
    uint256_t expiry("0x" + encoded.substr(322, 64));
    result = checkedUint64({}, nonce, parsed.nonce);
    if (!result) {
        return result;
    }
    result = checkedUint64({}, expiry, parsed.expiry);
    if (!result) {
        return result;
    }
    parsed.active = !isZeroAddress(parsed.owner);
    status = parsed;
    return {};
}

SessionResult SessionKeys::OwnerTransaction(const string& owner, uint64_t gasLimit,
                                            NativeTransaction& tx) {
    if (!isAddress(owner) || isReservedAddress(owner) || !gasLimit) {
        return invalidArgument("invalid owner/gas limit");
    }
    tx = NativeTransaction();
    tx.to = Registry();
    tx.gasLimit = gasLimit;
    auto result = ChainId(tx.chainId);
    if (!result) {
        return result;
    }
    result = PendingNonce(owner, tx.nonce);
    if (!result) {
        return result;
    }
    if (tx.nonce == UINT64_MAX) {
        return invalidArgument("owner nonce exhausted");
    }
    return GasPrice(tx.gasPrice);
}

SessionResult SessionKeys::PrepareCreate(const SessionPolicy& policy, const string& owner,
                                         const string& proof, uint64_t gasLimit,
                                         NativeTransaction& tx) {
    tx = NativeTransaction();
    uint64_t currentBlock;
    auto result = BlockNumber(currentBlock);
    if (!result) {
        return result;
    }
    result = Validate(policy, currentBlock);
    if (!result) {
        return result;
    }
    NativeTransaction prepared;
    result = OwnerTransaction(owner, gasLimit, prepared);
    if (!result) {
        return result;
    }
    uint8_t digest[32];
    result = ProofHash(policy, owner, prepared.chainId, digest);
    if (!result) {
        return result;
    }
    if (!isHex(proof, 65) || !signatureMatches(decodeHex(proof).data(), digest, policy.key)) {
        return invalidArgument("proof does not match policy/owner/chainId");
    }
    result = EncodeCreate(policy, proof, prepared.data);
    if (result) {
        tx = prepared;
    }
    return result;
}

SessionResult SessionKeys::PrepareRevoke(const string& key, const string& owner, uint64_t gasLimit,
                                         NativeTransaction& tx) {
    tx = NativeTransaction();
    string data;
    auto result = EncodeRevoke(key, data);
    if (!result) {
        return result;
    }
    NativeTransaction prepared;
    result = OwnerTransaction(owner, gasLimit, prepared);
    if (!result) {
        return result;
    }
    prepared.data = data;
    tx = prepared;
    return {};
}

SessionResult SessionKeys::SendDelegated(const SessionPolicy& policy, const string& owner,
                                         const uint256_t& chainId, SessionKey& key,
                                         NativeTransaction tx, string& hash) {
    hash.clear();
    auto result = validatePolicyShape(policy);
    if (!result) {
        return result;
    }
    if (key.Address() != lowerHex(policy.key) || !isAddress(tx.to) ||
        lowerHex(tx.to) != lowerHex(policy.target) || !isHex(tx.data) || tx.data.size() < 10 ||
        !tx.gasLimit || tx.chainId != chainId) {
        return invalidArgument("delegated key/chainId/target/calldata mismatch");
    }
    auto data = decodeHex(tx.data.substr(0, 10));
    Selector call;
    std::copy(data.begin(), data.end(), call.begin());
    if (std::find(policy.selectors.begin(), policy.selectors.end(), call) ==
        policy.selectors.end()) {
        return {SessionError::Unauthorized, "selector outside policy"};
    }
    uint256_t actualChain;
    result = ChainId(actualChain);
    if (!result) {
        return result;
    }
    if (actualChain != chainId) {
        return {SessionError::Unauthorized, "RPC chainId ID changed"};
    }
    SessionStatus status;
    result = Read(policy.key, owner, status);
    if (!result) {
        return result;
    }
    uint64_t head;
    result = BlockNumber(head);
    if (!result) {
        return result;
    }
    if (!status.active || status.owner != lowerHex(owner) ||
        status.target != lowerHex(policy.target) || status.expiry != policy.expiry ||
        head >= status.expiry || status.aplo > policy.aplo || status.gaplo > policy.gaplo ||
        tx.value > status.aplo) {
        return {SessionError::Unauthorized, "session absent, expired, or scope/budget mismatch"};
    }
    if (tx.gasPrice > (~uint256_t(0)) / uint256_t(tx.gasLimit) ||
        tx.gasPrice * uint256_t(tx.gasLimit) > status.gaplo) {
        return {SessionError::Unauthorized, "maximum fee exceeds GAPLO allowance"};
    }
    uint64_t estimated;
    result = EstimateGas(policy.key, tx.to, tx.data, tx.value, estimated);
    if (!result) {
        return result;
    }
    if (estimated > tx.gasLimit) {
        return invalidArgument("gas limit below estimate");
    }
    result = PendingNonce(policy.key, tx.nonce);
    if (!result) {
        return result;
    }
    string raw;
    result = SignTransaction(tx, SessionKey::SignDigest, &key, policy.key, raw);
    if (!result) {
        return result;
    }
    return SendRaw(raw, hash);
}
} // namespace aplo

#include "SessionKeys.h"
#include "SessionEncoding.h"
#include "trezor/sha3.h"
extern "C" {
#include "trezor/memzero.h"
#include "trezor/secp256k1.h"
}
#include <cstring>

namespace aplo {
using namespace detail;
// Defined by the Arduino Web3 adapter; native tests supply the OS CSPRNG.
bool SessionPlatformRandom(uint8_t* bytes, size_t length);

static bool isValidScalar(const uint8_t* secret) {
    bignum256 scalar;
    bn_read_be(secret, &scalar);
    bool valid = !bn_is_zero(&scalar) && bn_is_less(&scalar, &secp256k1.order);
    memzero(&scalar, sizeof(scalar));
    return valid;
}

SessionKey::SessionKey() : valid_(false) {
    memset(secret_, 0, 32);
}

SessionKey::~SessionKey() {
    Clear();
}

void SessionKey::Clear() {
    memzero(secret_, 32);
    valid_ = false;
}

bool SessionKey::Import(const uint8_t secret[32]) {
    Clear();
    if (!secret || !isValidScalar(secret)) {
        return false;
    }
    memcpy(secret_, secret, 32);
    valid_ = true;
    return true;
}

bool SessionKey::Export(uint8_t secret[32]) const {
    if (!secret || !valid_) {
        return false;
    }
    memcpy(secret, secret_, 32);
    return true;
}

bool SessionKey::Generate() {
    Clear();
    for (unsigned i = 0; i < 128; ++i) {
        if (!SessionPlatformRandom(secret_, 32)) {
            Clear();
            return false;
        }
        if (isValidScalar(secret_)) {
            valid_ = true;
            return true;
        }
    }
    Clear();
    return false;
}

static string publicAddress(const uint8_t publicKey[65]) {
    uint8_t hash[32];
    keccak_256(publicKey + 1, 64, hash);
    return encodeHex(hash + 12, 20);
}

string SessionKey::Address() const {
    if (!valid_) {
        return "";
    }
    uint8_t publicKey[65];
    ecdsa_get_public_key65(&secp256k1, secret_, publicKey);
    return publicAddress(publicKey);
}

bool SessionKey::SignDigest(const uint8_t digest[32], uint8_t signature[65], void* context) {
    if (!digest || !signature || !context) {
        return false;
    }
    auto* key = static_cast<SessionKey*>(context);
    memset(signature, 0, 65);
    if (!key->valid_ ||
        ecdsa_sign_digest(&secp256k1, key->secret_, digest, signature, signature + 64, nullptr) !=
            0 ||
        signature[64] > 1) {
        memzero(signature, 65);
        return false;
    }
    return true;
}

bool detail::canonicalSignature(const uint8_t signature[65]) {
    if (signature[64] > 1) {
        return false;
    }
    bignum256 signatureR, signatureS;
    bn_read_be(signature, &signatureR);
    bn_read_be(signature + 32, &signatureS);
    return !bn_is_zero(&signatureR) && !bn_is_zero(&signatureS) &&
           bn_is_less(&signatureR, &secp256k1.order) &&
           !bn_is_less(&secp256k1.order_half, &signatureS);
}

bool detail::signatureMatches(const uint8_t* signature, const uint8_t* digest,
                              const string& expected) {
    if (!isAddress(expected) || !canonicalSignature(signature)) {
        return false;
    }
    uint8_t publicKey[65];
    return ecdsa_recover_pub_from_sig(&secp256k1, publicKey, signature, digest, signature[64]) ==
               0 &&
           publicAddress(publicKey) == lowerHex(expected);
}
} // namespace aplo

#include "SessionKeys.h"
#include "SessionEncoding.h"
#include "trezor/sha3.h"

namespace aplo {
using namespace detail;

// Canonical RLP: integer zero is empty; signature r/s have leading zeroes removed.
static void appendLengthPrefix(vector<uint8_t>& output, size_t length, uint8_t base) {
    if (length <= 55) {
        output.push_back(base + length);
        return;
    }
    size_t remaining = length;
    size_t count = 0;
    while (remaining) {
        ++count;
        remaining >>= 8;
    }
    output.push_back(base + 55 + count);
    for (size_t i = count; i; --i) {
        output.push_back(static_cast<uint8_t>(length >> (8 * (i - 1))));
    }
}

static void appendRlp(vector<uint8_t>& output, const vector<uint8_t>& value, bool integer = false) {
    size_t start = 0;
    if (integer) {
        while (start < value.size() && value[start] == 0) {
            ++start;
        }
    }
    size_t length = value.size() - start;
    if (length == 1 && value[start] < 128) {
        output.push_back(value[start]);
    } else {
        appendLengthPrefix(output, length, 0x80);
        output.insert(output.end(), value.begin() + start, value.end());
    }
}

static void appendInteger(vector<uint8_t>& output, const uint256_t& value) {
    appendRlp(output, value.export_bits(), true);
}

static vector<uint8_t> encodeTransaction(const NativeTransaction& tx, const uint8_t* signature) {
    vector<uint8_t> payload;
    payload.reserve(160 + (tx.data.size() - 2) / 2);
    appendInteger(payload, uint256_t(tx.nonce));
    appendInteger(payload, tx.gasPrice);
    appendInteger(payload, uint256_t(tx.gasLimit));
    appendRlp(payload, decodeHex(tx.to));
    appendInteger(payload, tx.value);
    appendRlp(payload, decodeHex(tx.data));
    if (!signature) {
        appendInteger(payload, tx.chainId);
        appendInteger(payload, 0);
        appendInteger(payload, 0);
    } else {
        // EIP155 v can exceed uint256 even for a valid uint256 chain domain.
        // Encode 2*chainId + 35 + recovery directly as a 33-byte integer.
        const auto chain = tx.chainId.export_bits();
        vector<uint8_t> replayValue(33, 0);
        unsigned carry = 35 + signature[64];
        for (size_t i = 32; i > 0; --i) {
            const unsigned value = 2 * chain[i - 1] + carry;
            replayValue[i] = static_cast<uint8_t>(value);
            carry = value >> 8;
        }
        replayValue[0] = static_cast<uint8_t>(carry);
        appendRlp(payload, replayValue, true);
        appendRlp(payload, vector<uint8_t>(signature, signature + 32), true);
        appendRlp(payload, vector<uint8_t>(signature + 32, signature + 64), true);
    }
    vector<uint8_t> output;
    output.reserve(payload.size() + 4);
    appendLengthPrefix(output, payload.size(), 0xc0);
    output.insert(output.end(), payload.begin(), payload.end());
    return output;
}

SessionResult SessionKeys::SignTransaction(const NativeTransaction& tx, DigestSigner signer,
                                           void* context, const string& expectedSigner,
                                           string& raw) {
    raw.clear();
    if (!signer || !isAddress(expectedSigner) || isZeroAddress(expectedSigner) ||
        !isAddress(tx.to) || !isHex(tx.data) || tx.gasLimit == 0 || tx.nonce == UINT64_MAX) {
        return invalidArgument("invalid EIP155 transaction or exhausted nonce");
    }
    uint8_t digest[32];
    uint8_t signature[65] = {};
    {
        auto encoded = encodeTransaction(tx, nullptr);
        keccak_256(encoded.data(), encoded.size(), digest);
    }
    if (!signer(digest, signature, context) ||
        !signatureMatches(signature, digest, expectedSigner)) {
        return {SessionError::SignerError, "invalid signature or signer address"};
    }
    {
        auto encoded = encodeTransaction(tx, signature);
        raw = encodeHex(encoded.data(), encoded.size());
    }
    return {};
}
} // namespace aplo

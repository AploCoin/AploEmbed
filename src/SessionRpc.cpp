#include "SessionKeys.h"
#include "internal/JsonRpcParser.h"
#include "internal/SessionEncoding.h"

namespace aplo {
using namespace detail;

bool detail::unquoteHex(const string& encoded, string& output) {
    if (encoded.size() < 2 || encoded.front() != '"' || encoded.back() != '"' ||
        encoded.find('\\') != string::npos) {
        return false;
    }
    output = encoded.substr(1, encoded.size() - 2);
    return true;
}

SessionResult SessionKeys::Rpc(const string& method, const string& params, string& result) {
    result.clear();
    if (!transport_) {
        return {SessionError::TransportError, "no RPC transport"};
    }
    const string response = transport_(method, params, context_);
    if (response.empty()) {
        return {SessionError::TransportError, "empty RPC response"};
    }
    JsonRpcParser parser;
    for (char character : response) {
        parser.Push(character);
    }
    switch (parser.Result()) {
    case JsonRpcParser::Outcome::Invalid:
        return {SessionError::InvalidResponse, "missing or malformed RPC result"};
    case JsonRpcParser::Outcome::RpcError: {
        const auto range = parser.ErrorRange();
        return {SessionError::RpcError, response.substr(range.begin, range.end - range.begin)};
    }
    case JsonRpcParser::Outcome::Result: {
        const auto range = parser.ResultRange();
        result = response.substr(range.begin, range.end - range.begin);
        return {};
    }
    }
    return {SessionError::InvalidResponse, "invalid RPC envelope"};
}

bool detail::parseQuantity(const string& raw, uint256_t& value) {
    string encoded;
    if (!unquoteHex(raw, encoded) || encoded.size() < 3 || encoded.size() > 66 ||
        encoded.substr(0, 2) != "0x") {
        return false;
    }
    if (encoded.size() > 3 && encoded[2] == '0') {
        return false;
    }
    for (size_t i = 2; i < encoded.size(); ++i) {
        if (hexDigit(encoded[i]) < 0) {
            return false;
        }
    }
    value = uint256_t(encoded);
    return true;
}

SessionResult SessionKeys::Quantity(const string& method, const string& params, uint256_t& value) {
    value = 0;
    string raw;
    auto result = Rpc(method, params, raw);
    if (!result) {
        return result;
    }
    if (!parseQuantity(raw, value)) {
        return {SessionError::InvalidResponse, "invalid uint256 RPC quantity"};
    }
    return {};
}

SessionResult SessionKeys::Request(const string& method, const string& params, string& result) {
    return Rpc(method, params, result);
}

SessionResult SessionKeys::Call(const string& from, const string& to, const string& data,
                                string& output, const string& blockTag) {
    output.clear();
    bool tag = blockTag == "latest" || blockTag == "pending" || blockTag == "earliest" ||
               blockTag == "safe" || blockTag == "finalized";
    uint256_t block;
    tag = tag || parseQuantity("\"" + blockTag + "\"", block);
    if (!isAddress(from) || !isAddress(to) || !isHex(data) || !tag) {
        return invalidArgument("invalid eth_call argument");
    }
    string raw;
    auto result = Rpc("eth_call",
                      "[{\"from\":\"" + from + "\",\"to\":\"" + to + "\",\"data\":\"" + data +
                          "\"},\"" + blockTag + "\"]",
                      raw);
    if (!result) {
        return result;
    }
    string parsed;
    if (!unquoteHex(raw, parsed) || !isHex(parsed)) {
        return {SessionError::InvalidResponse, "invalid eth_call result"};
    }
    output = parsed;
    return {};
}

SessionResult SessionKeys::EstimateGas(const string& from, const string& to, const string& data,
                                       const uint256_t& value, uint64_t& gas) {
    gas = 0;
    if (!isAddress(from) || !isAddress(to) || !isHex(data)) {
        return invalidArgument("invalid estimateGas argument");
    }
    uint256_t estimated;
    auto result = Quantity("eth_estimateGas",
                           "[{\"from\":\"" + from + "\",\"to\":\"" + to + "\",\"data\":\"" + data +
                               "\",\"value\":\"0x" + value.str(16) + "\"}]",
                           estimated);
    return checkedUint64(result, estimated, gas);
}

SessionResult SessionKeys::Balance(const string& account, uint256_t& balance) {
    balance = 0;
    if (!isAddress(account)) {
        return invalidArgument("invalid balance address");
    }
    return Quantity("eth_getBalance", "[\"" + account + "\",\"latest\"]", balance);
}

SessionResult SessionKeys::ChainId(uint256_t& chain) {
    return Quantity("eth_chainId", "[]", chain);
}

SessionResult SessionKeys::GasPrice(uint256_t& gas) {
    return Quantity("eth_gasPrice", "[]", gas);
}

SessionResult detail::checkedUint64(SessionResult result, const uint256_t& value,
                                    uint64_t& output) {
    output = 0;
    if (!result) {
        return result;
    }
    if (value > uint256_t(UINT64_MAX)) {
        return {SessionError::InvalidResponse, "quantity exceeds uint64"};
    }
    output = static_cast<uint64_t>(value);
    return {};
}

SessionResult SessionKeys::BlockNumber(uint64_t& block) {
    uint256_t value;
    auto result = Quantity("eth_blockNumber", "[]", value);
    return checkedUint64(result, value, block);
}

SessionResult SessionKeys::PendingNonce(const string& signerAddress, uint64_t& nonce) {
    nonce = 0;
    if (!isAddress(signerAddress)) {
        return invalidArgument("invalid signer address");
    }
    uint256_t value;
    auto result =
        Quantity("eth_getTransactionCount", "[\"" + signerAddress + "\",\"pending\"]", value);
    return checkedUint64(result, value, nonce);
}

SessionResult SessionKeys::SendRaw(const string& raw, string& hash) {
    hash.clear();
    if (!isHex(raw) || raw.size() < 4) {
        return invalidArgument("invalid raw transaction");
    }
    string value;
    auto result = Rpc("eth_sendRawTransaction", "[\"" + raw + "\"]", value);
    if (!result) {
        return result;
    }
    string parsed;
    if (!unquoteHex(value, parsed) || !isHex(parsed, 32)) {
        return {SessionError::InvalidResponse, "invalid transaction hash"};
    }
    hash = parsed;
    return {};
}

SessionResult SessionKeys::Receipt(const string& hash, ReceiptState& state) {
    // Preserve the caller's state on failure; only successful results define it.
    if (!isHex(hash, 32)) {
        return invalidArgument("invalid transaction hash");
    }
    string raw;
    auto result = Rpc("eth_getTransactionReceipt", "[\"" + hash + "\"]", raw);
    if (!result) {
        return result;
    }
    // Use the same direct-status rules as oversized ESP8266 receipts. Feeding a
    // synthetic envelope avoids a second JSON grammar or copying the raw value.
    JsonRpcParser parser;
    static const char envelopePrefix[] = "{\"result\":";
    for (size_t i = 0; i < sizeof(envelopePrefix) - 1; ++i) {
        const char character = envelopePrefix[i];
        parser.Push(character);
    }
    for (char character : raw) {
        parser.Push(character);
    }
    parser.Push('}');
    switch (parser.ReceiptResult()) {
    case JsonRpcParser::Receipt::Pending:
        state = ReceiptState::Pending;
        return {};
    case JsonRpcParser::Receipt::Succeeded:
        state = ReceiptState::Succeeded;
        return {};
    case JsonRpcParser::Receipt::Reverted:
        state = ReceiptState::Reverted;
        return {};
    case JsonRpcParser::Receipt::Invalid:
        return {SessionError::InvalidResponse, "invalid receipt status"};
    }
    return {SessionError::InvalidResponse, "invalid receipt"};
}
} // namespace aplo

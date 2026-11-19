#include "ReceiptFilter.h"
#include "SessionKeys.h"
#include "internal/RpcResponseBuffer.h"
#include <cassert>
#include <iostream>
#include <string>

namespace {
using namespace aplo;
using std::string;

struct Response {
    string json;

    static string Transport(const string&, const string&, void* context) {
        return static_cast<Response*>(context)->json;
    }
};

void CheckReceipt(const string& json, SessionError error, ReceiptState expected) {
    const string hash = "0x" + string(64, 'a');
    Response response{json};
    SessionKeys sessions(Response::Transport, &response);
    ReceiptState state = ReceiptState::Reverted;
    const auto result = sessions.Receipt(hash, state);
    assert(result.code == error);
    assert(state == (result ? expected : ReceiptState::Reverted));

    // Same body through the exact accumulator used by Web3::exec on ESP8266.
    // A small limit forces reduction even for short adversarial envelopes.
    detail::RpcResponseBuffer buffer(8, true);
    ReceiptFilter filter;
    for (char character : json) {
        buffer.Push(character);
        filter.Push(character);
        assert(buffer.RetainedSize() <= 8);
    }
    response.json = buffer.Finish();
    state = ReceiptState::Reverted;
    assert(sessions.Receipt(hash, state).code == error);
    assert(state == (result ? expected : ReceiptState::Reverted));
    const int status = error == SessionError::Ok && expected != ReceiptState::Pending
                           ? (expected == ReceiptState::Succeeded ? 1 : 0)
                           : -1;
    detail::JsonRpcParser bound;
    for (char character : json) {
        bound.Push(character);
    }
    const auto receipt = bound.ReceiptResult(hash.c_str());
    const int boundStatus = receipt == detail::JsonRpcParser::Receipt::Succeeded  ? 1
                            : receipt == detail::JsonRpcParser::Receipt::Reverted ? 0
                                                                                  : -1;
    assert(boundStatus == status);
}

void EnvelopeRegressions() {
    for (
        const string json : {
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"error":{"code":-1},"error":null})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"\u0072esult":null})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","\u0073tatus":"0x0"}})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"jsonrpc":"1.0"})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"jsonrpc":"2.0","jsonrpc":"2.0"})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"id":1,"id":2})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"error":false})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"error":0})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"id":01})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"id":-01})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"id":1e})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"id":1.})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","logs":["\q"]}})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","logs":["\u12q4"]}})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},})",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}} trailing)",
            R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")",
            R"({"result":{"logs":[{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}]}})",
            R"({"result":{"status":null}})",
            R"({"result":{"status":{}}})",
            R"({"result":{"status":"0x01"}})",
            R"({"result":[]})",
            R"({"error":null})",
        }) {
        CheckReceipt(json, SessionError::InvalidResponse, ReceiptState::Pending);
    }
    CheckReceipt(
        R"({"result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"\u0065rror":{"code":-1}})",
        SessionError::RpcError,
        ReceiptState::Pending
    );
    CheckReceipt(R"({"error":{"code":-1,"message":"failed"}})", SessionError::RpcError,
                 ReceiptState::Pending);
    CheckReceipt(R"({"result":null})", SessionError::Ok, ReceiptState::Pending);
    CheckReceipt(
        R"({"result":{"logs":[{"status":"0x0","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}],"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}})",
        SessionError::Ok,
        ReceiptState::Succeeded
    );
    CheckReceipt(
        R"({"result":{"logs":[{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}],"status":"0x0","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}})",
        SessionError::Ok,
        ReceiptState::Reverted
    );
    CheckReceipt(
        R"({"jsonrpc":"2.0","result":{"status":"0x1","transactionHash":"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},"id":1.5e+2})",
        SessionError::Ok,
        ReceiptState::Succeeded
    );
}

void BodyBoundaries() {
    const size_t limit = 768; // Web3.h's ESP8266 retained-body cap.
    const string receipt =
        "{\"result\":{\"status\":\"0x1\",\"transactionHash\":"
        "\"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"logs\":[\"" +
        string(8192, 'a') + "\"]}}";
    CheckReceipt(receipt, SessionError::Ok, ReceiptState::Succeeded);
    CheckReceipt(receipt.substr(0, receipt.size() - 1), SessionError::InvalidResponse,
                 ReceiptState::Pending);
    CheckReceipt("{\"result\":null,\"padding\":\"" + string(8192, 'a') + "\"}", SessionError::Ok,
                 ReceiptState::Pending);
    CheckReceipt("{\"error\":{\"message\":\"" + string(8192, 'a') + "\"}}", SessionError::RpcError,
                 ReceiptState::Pending);

    for (size_t size : {limit - 1, limit, limit + 1}) {
        const string json = "{\"result\":null}" + string(size - 15, ' ');
        assert(json.size() == size);
        for (bool receiptRequest : {false, true}) {
            detail::RpcResponseBuffer buffer(limit, receiptRequest);
            for (char character : json) {
                buffer.Push(character);
                assert(buffer.RetainedSize() <= limit);
            }
            assert(buffer.Overflow() == (size > limit));
            const string expected = size <= limit    ? json
                                    : receiptRequest ? "{\"result\":null}"
                                                     : "";
            assert(buffer.Finish() == expected);
        }
    }

    // Root + result + log arrays = 32 containers accepted, 33 rejected.
    for (unsigned nested : {30u, 31u}) {
        const string json =
            "{\"result\":{\"status\":\"0x1\",\"transactionHash\":"
            "\"0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"logs\":" +
            string(nested, '[') + "0" + string(nested, ']') + "}}";
        CheckReceipt(json, nested == 30 ? SessionError::Ok : SessionError::InvalidResponse,
                     ReceiptState::Succeeded);
    }
}

void ReceiptIdentity() {
    const string hash = "0x" + string(64, 'a');
    const string status = "{\"result\":{\"status\":\"0x1\"";
    const string field = ",\"transactionHash\":\"" + hash + "\"";
    CheckReceipt(status + "}}", SessionError::InvalidResponse, ReceiptState::Pending);
    CheckReceipt(
        status + ",\"logs\":[{\"transactionHash\":\"" + hash + "\"}]}}",
        SessionError::InvalidResponse,
        ReceiptState::Pending
    );
    for (const string& extra : {
             string(",\"transactionHash\":null"),
             string(",\"transactionHash\":{}"),
             string(",\"transactionHash\":\"0x12\""),
             string(",\"transactionHash\":\"0x") + string(64, 'b') + "\"",
             field + field,
             field + ",\"\\u0074ransactionHash\":\"" + hash + "\"",
         }) {
        CheckReceipt(status + extra + "}}", SessionError::InvalidResponse, ReceiptState::Pending);
        CheckReceipt(
            status + extra + ",\"logs\":[\"" + string(8192, 'z') + "\"]}}",
            SessionError::InvalidResponse,
            ReceiptState::Pending
        );
    }
    CheckReceipt(
        status + ",\"transactionHash\":\"0x" + string(64, 'A') + "\"}}",
        SessionError::Ok,
        ReceiptState::Succeeded
    );
    // Identity after logs also survives the overflow path.
    CheckReceipt(
        status + ",\"logs\":[\"" + string(8192, 'z') + "\"]" + field + "}}",
        SessionError::Ok,
        ReceiptState::Succeeded
    );
}

void NonceWidths() {
    Response response;
    SessionKeys sessions(Response::Transport, &response);
    const string address = "0x" + string(40, 'a');
    uint64_t nonce = 77;
    for (const string raw :
         {"\"0x10000000000000000\"", "\"0x00\"", "\"0x\"", "\"0x-1\"", "\"0xg\"", "1", "null"}) {
        response.json = "{\"result\":" + raw + "}";
        assert(sessions.LatestNonce(address, nonce).code == SessionError::InvalidResponse);
        assert(nonce == 0);
    }
    for (const string quantity : {"0x0", "0x10000000000", "0xffffffffffffffff"}) {
        response.json = "{\"result\":\"" + quantity + "\"}";
        assert(sessions.LatestNonce(address, nonce));
        const uint64_t expected = quantity == "0x0"       ? 0
                                  : quantity.size() == 18 ? UINT64_MAX
                                                          : UINT64_C(1) << 40;
        assert(nonce == expected);
    }
    response.json = "";
    assert(sessions.LatestNonce(address, nonce).code == SessionError::TransportError && nonce == 0);
    response.json = "{\"error\":{\"code\":-1}}";
    assert(sessions.LatestNonce(address, nonce).code == SessionError::RpcError && nonce == 0);
    assert(sessions.LatestNonce("bad", nonce).code == SessionError::InvalidArgument && nonce == 0);
}

} // namespace

void rpcTests() {
    EnvelopeRegressions();
    BodyBoundaries();
    ReceiptIdentity();
    NonceWidths();
    std::cout
        << "Shared JSON/receipt syntax, escaped keys, stream cap and depth regressions passed\n";
}

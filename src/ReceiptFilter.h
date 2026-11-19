#ifndef APLO_RECEIPT_FILTER_H
#define APLO_RECEIPT_FILTER_H

#include "internal/JsonRpcParser.h"
#include <string>

// Compatibility adapter for the constant-memory ESP8266 receipt path. Buffered
// SessionKeys RPC uses the same grammar and direct-result/status selection.
class ReceiptFilter {
public:
    void Push(char character) {
        parser_.Push(character);
    }

    int Status() const {
        switch (parser_.ReceiptResult()) {
        case aplo::detail::JsonRpcParser::Receipt::Succeeded:
            return 1;
        case aplo::detail::JsonRpcParser::Receipt::Reverted:
            return 0;
        default:
            return -1;
        }
    }

    // Only used after the ordinary response cap is exceeded. Empty/invalid
    // envelopes stay invalid; a node error is never reported as pending.
    std::string CompactResponse() const {
        using Parser = aplo::detail::JsonRpcParser;
        if (parser_.Result() == Parser::Outcome::RpcError) {
            return "{\"error\":{\"code\":-32000,\"message\":\"oversized RPC error\"}}";
        }

        switch (parser_.ReceiptResult()) {
        case Parser::Receipt::Pending:
            return "{\"result\":null}";
        case Parser::Receipt::Succeeded:
        case Parser::Receipt::Reverted:
            // Preserve identity through reduction; missing identity cannot
            // become evidence for the caller's journal entry.
            if (parser_.TransactionHash()) {
                return std::string("{\"result\":{\"status\":\"0x") +
                       (parser_.ReceiptResult() == Parser::Receipt::Succeeded ? "1" : "0") +
                       "\",\"transactionHash\":\"" + parser_.TransactionHash() + "\"}}";
            }
            return "{}";
        case Parser::Receipt::Invalid:
            return "{}";
        }

        return "{}";
    }

private:
    aplo::detail::JsonRpcParser parser_;
};
#endif

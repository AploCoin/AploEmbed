#ifndef APLO_INTERNAL_JSON_RPC_PARSER_H
#define APLO_INTERNAL_JSON_RPC_PARSER_H

#include <stddef.h>
#include <stdint.h>

namespace aplo {
namespace detail {

// Allocation-free syntax/envelope walker shared by buffered RPC and ESP8266
// receipt streaming. Ranges refer to the original input; logs are never copied.
class JsonRpcParser {
public:
    static const unsigned MaxDepth = 32;
    enum class Outcome { Invalid, Result, RpcError };
    enum class Receipt { Invalid, Pending, Succeeded, Reverted };

    struct Range {
        size_t begin = 0;
        size_t end = 0;
    };

    void Push(char character);
    Outcome Result() const;
    Receipt ReceiptResult() const;

    Range ResultRange() const {
        return result_;
    }

    Range ErrorRange() const {
        return error_;
    }

private:
    enum class Phase : uint8_t { First, Colon, Value, Separator, Key, ArrayValue };
    enum class Key : uint8_t { Other, Result, Error, Status, Version, Id };
    enum class Number : uint8_t {
        Start,
        Minus,
        Zero,
        Integer,
        Dot,
        Fraction,
        Exponent,
        Sign,
        Digits,
        Bad
    };

    struct Frame {
        char type;
        Phase phase;
        Key key;
    };

    void StartValue(char character);
    void FinishValue(size_t end);
    void CloseFrame();
    void StartToken();
    void AppendToken(char character);
    bool TokenIs(const char* value) const;
    void FinishString();
    void FinishPrimitive();
    void NumberCharacter(char character);
    bool ExpectsValue() const;
    bool InReceipt() const;
    Key CurrentKey() const;

    Frame stack_[MaxDepth];
    unsigned depth_ = 0;
    size_t offset_ = 0;
    Range result_, error_;
    bool invalid_ = false;
    bool complete_ = false;
    bool inString_ = false;
    bool escaped_ = false;
    bool primitive_ = false;
    bool keyString_ = false;
    bool resultSeen_ = false;
    bool errorSeen_ = false;
    bool versionSeen_ = false;
    bool idSeen_ = false;
    bool rpcError_ = false;
    bool resultObject_ = false;
    bool statusSeen_ = false;
    bool receiptInvalid_ = false;
    Receipt receipt_ = Receipt::Invalid;
    Number number_ = Number::Start;
    unsigned unicodeRemaining_ = 0;
    unsigned unicodeValue_ = 0;
    // Only short keys, protocol version, literals and status need matching.
    // Longer strings remain valid JSON and are skipped without allocation.
    char token_[16];
    size_t tokenLength_ = 0;
    bool tokenOverflow_ = false;
};

} // namespace detail
} // namespace aplo
#endif

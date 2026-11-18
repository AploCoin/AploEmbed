#include "JsonRpcParser.h"
#include <string.h>

namespace aplo {
namespace detail {
namespace {

bool IsSpace(char character) {
    return character == ' ' || character == '\r' || character == '\n' || character == '\t';
}

int HexDigit(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

} // namespace

void JsonRpcParser::StartToken() {
    tokenLength_ = 0;
    tokenOverflow_ = false;
}

void JsonRpcParser::AppendToken(char character) {
    if (tokenLength_ < sizeof(token_)) {
        token_[tokenLength_++] = character;
    } else {
        tokenOverflow_ = true;
    }
}

bool JsonRpcParser::TokenIs(const char* value) const {
    return !tokenOverflow_ && tokenLength_ == strlen(value) &&
           memcmp(token_, value, tokenLength_) == 0;
}

JsonRpcParser::Key JsonRpcParser::CurrentKey() const {
    return depth_ ? stack_[depth_ - 1].key : Key::Other;
}

bool JsonRpcParser::InReceipt() const {
    return depth_ == 2 && resultObject_ && stack_[0].key == Key::Result;
}

bool JsonRpcParser::ExpectsValue() const {
    if (!depth_) {
        return !complete_;
    }
    const Frame& frame = stack_[depth_ - 1];
    return frame.type == '{' ? frame.phase == Phase::Value
                             : frame.phase == Phase::First || frame.phase == Phase::ArrayValue;
}

void JsonRpcParser::StartValue(char character) {
    if (!ExpectsValue()) {
        invalid_ = true;
        return;
    }
    const Key key = CurrentKey();
    if (depth_ == 1) {
        bool* seen = nullptr;
        if (key == Key::Result) {
            seen = &resultSeen_;
            result_.begin = offset_;
            resultObject_ = character == '{';
        } else if (key == Key::Error) {
            seen = &errorSeen_;
            error_.begin = offset_;
            rpcError_ = character == '{';
            if (character != '{' && character != 'n') {
                invalid_ = true;
            }
        } else if (key == Key::Version) {
            seen = &versionSeen_;
            if (character != '"') {
                invalid_ = true;
            }
        } else if (key == Key::Id) {
            seen = &idSeen_;
            if (character != '"' && character != 'n' && character != '-' &&
                (character < '0' || character > '9')) {
                invalid_ = true;
            }
        }
        if (seen) {
            if (*seen) {
                invalid_ = true;
            }
            *seen = true;
        }
    }
    if (InReceipt() && key == Key::Status) {
        if (statusSeen_ || character != '"') {
            receipt_ = Receipt::Invalid;
            // Duplicate status is ambiguous even when both values agree.
            receiptInvalid_ = true;
        }
        statusSeen_ = true;
    }
    if (character == '{' || character == '[') {
        if (depth_ == MaxDepth) {
            invalid_ = true;
            return;
        }
        stack_[depth_++] = {character, Phase::First, Key::Other};
        return;
    }
    StartToken();
    if (character == '"') {
        inString_ = true;
        keyString_ = false;
    } else {
        primitive_ = true;
        number_ = Number::Start;
        AppendToken(character);
        NumberCharacter(character);
    }
}

void JsonRpcParser::FinishValue(size_t end) {
    if (!depth_) {
        complete_ = true;
        return;
    }
    Frame& frame = stack_[depth_ - 1];
    if (depth_ == 1 && frame.key == Key::Result) {
        result_.end = end;
    } else if (depth_ == 1 && frame.key == Key::Error) {
        error_.end = end;
    }
    frame.phase = Phase::Separator;
}

void JsonRpcParser::CloseFrame() {
    --depth_;
    FinishValue(offset_ + 1);
}

void JsonRpcParser::FinishString() {
    inString_ = false;
    if (keyString_) {
        Frame& frame = stack_[depth_ - 1];
        frame.key = Key::Other;
        if (depth_ == 1) {
            if (TokenIs("result")) {
                frame.key = Key::Result;
            } else if (TokenIs("error")) {
                frame.key = Key::Error;
            } else if (TokenIs("jsonrpc")) {
                frame.key = Key::Version;
            } else if (TokenIs("id")) {
                frame.key = Key::Id;
            }
        } else if (InReceipt() && TokenIs("status")) {
            frame.key = Key::Status;
        }
        frame.phase = Phase::Colon;
        return;
    }
    if (depth_ == 1 && CurrentKey() == Key::Version && !TokenIs("2.0")) {
        invalid_ = true;
    }
    if (InReceipt() && CurrentKey() == Key::Status) {
        if (TokenIs("0x1")) {
            receipt_ = Receipt::Succeeded;
        } else if (TokenIs("0x0")) {
            receipt_ = Receipt::Reverted;
        } else {
            receipt_ = Receipt::Invalid;
        }
    }
    FinishValue(offset_ + 1);
}

void JsonRpcParser::NumberCharacter(char character) {
    const bool digit = character >= '0' && character <= '9';
    switch (number_) {
    case Number::Start:
        if (character == '-') {
            number_ = Number::Minus;
            return;
        }
        // Fall through to the first integer digit.
    case Number::Minus:
        number_ = character == '0' ? Number::Zero : digit ? Number::Integer : Number::Bad;
        return;
    case Number::Zero:
    case Number::Integer:
        if (digit && number_ == Number::Integer) {
            return;
        }
        number_ = character == '.'                         ? Number::Dot
                  : (character == 'e' || character == 'E') ? Number::Exponent
                                                           : Number::Bad;
        return;
    case Number::Dot:
        number_ = digit ? Number::Fraction : Number::Bad;
        return;
    case Number::Fraction:
        if (digit) {
            return;
        }
        number_ = (character == 'e' || character == 'E') ? Number::Exponent : Number::Bad;
        return;
    case Number::Exponent:
        if (character == '+' || character == '-') {
            number_ = Number::Sign;
            return;
        }
        // An exponent may start directly with a digit.
    case Number::Sign:
    case Number::Digits:
        number_ = digit ? Number::Digits : Number::Bad;
        return;
    case Number::Bad:
        return;
    }
}

void JsonRpcParser::FinishPrimitive() {
    const bool number = number_ == Number::Zero || number_ == Number::Integer ||
                        number_ == Number::Fraction || number_ == Number::Digits;
    if (!number && !TokenIs("null") && !TokenIs("true") && !TokenIs("false")) {
        invalid_ = true;
        return;
    }
    if (depth_ == 1) {
        if (CurrentKey() == Key::Result && TokenIs("null")) {
            receipt_ = Receipt::Pending;
        } else if ((CurrentKey() == Key::Error || CurrentKey() == Key::Id) && !number &&
                   !TokenIs("null")) {
            invalid_ = true;
        }
    }
    primitive_ = false;
    FinishValue(offset_);
}

void JsonRpcParser::Push(char character) {
    if (invalid_) {
        return;
    }

    // offset_ always refers to this character, including a primitive delimiter.
    // All early returns still consume exactly one byte.
    struct Advance {
        size_t& offset;

        ~Advance() {
            ++offset;
        }
    } advance{offset_};

    if (inString_) {
        if (unicodeRemaining_) {
            const int digit = HexDigit(character);
            if (digit < 0) {
                invalid_ = true;
                return;
            }
            unicodeValue_ = unicodeValue_ * 16 + static_cast<unsigned>(digit);
            if (--unicodeRemaining_ == 0) {
                // Decode ASCII so escaped protocol keys cannot bypass duplicates.
                AppendToken(unicodeValue_ < 128 ? static_cast<char>(unicodeValue_) : '\x80');
            }
            return;
        }
        if (escaped_) {
            escaped_ = false;
            switch (character) {
            case 'u':
                unicodeRemaining_ = 4;
                unicodeValue_ = 0;
                return;
            case '"':
            case '\\':
            case '/':
                AppendToken(character);
                return;
            case 'b':
                AppendToken('\b');
                return;
            case 'f':
                AppendToken('\f');
                return;
            case 'n':
                AppendToken('\n');
                return;
            case 'r':
                AppendToken('\r');
                return;
            case 't':
                AppendToken('\t');
                return;
            default:
                invalid_ = true;
                return;
            }
        }
        if (character == '\\') {
            escaped_ = true;
        } else if (static_cast<unsigned char>(character) < 32) {
            invalid_ = true;
        } else if (character == '"') {
            FinishString();
        } else {
            AppendToken(character);
        }
        return;
    }
    const bool space = IsSpace(character);
    if (primitive_) {
        if (!space && character != ',' && character != '}' && character != ']') {
            AppendToken(character);
            NumberCharacter(character);
            return;
        }
        FinishPrimitive();
        if (invalid_) {
            return;
        }
    }
    if (space) {
        return;
    }
    if (complete_) {
        invalid_ = true;
        return;
    }
    if (depth_) {
        Frame& frame = stack_[depth_ - 1];
        if (frame.type == '{' && (frame.phase == Phase::First || frame.phase == Phase::Key)) {
            if (character == '}' && frame.phase == Phase::First) {
                CloseFrame();
                return;
            }
            if (character != '"') {
                invalid_ = true;
                return;
            }
            StartToken();
            inString_ = true;
            keyString_ = true;
            return;
        }
        if (frame.type == '{' && frame.phase == Phase::Colon) {
            if (character == ':') {
                frame.phase = Phase::Value;
            } else {
                invalid_ = true;
            }
            return;
        }
        if (frame.phase == Phase::Separator) {
            if (character == ',') {
                frame.phase = frame.type == '{' ? Phase::Key : Phase::ArrayValue;
            } else if (character == (frame.type == '{' ? '}' : ']')) {
                CloseFrame();
            } else {
                invalid_ = true;
            }
            return;
        }
        if (frame.type == '[' && frame.phase == Phase::First && character == ']') {
            CloseFrame();
            return;
        }
    }
    if (!depth_ && character != '{') {
        invalid_ = true;
        return;
    }
    StartValue(character);
}

JsonRpcParser::Outcome JsonRpcParser::Result() const {
    if (invalid_ || !complete_ || depth_ || inString_ || primitive_) {
        return Outcome::Invalid;
    }
    if (rpcError_) {
        return Outcome::RpcError;
    }
    return resultSeen_ ? Outcome::Result : Outcome::Invalid;
}

JsonRpcParser::Receipt JsonRpcParser::ReceiptResult() const {
    return Result() == Outcome::Result && !receiptInvalid_ ? receipt_ : Receipt::Invalid;
}

} // namespace detail
} // namespace aplo

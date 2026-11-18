#ifndef APLO_INTERNAL_SESSION_ENCODING_H
#define APLO_INTERNAL_SESSION_ENCODING_H
#include "../SessionKeys.h"

namespace aplo {
namespace detail {
using std::string;
using std::vector;

inline SessionResult invalidArgument(const char* message) {
    return {SessionError::InvalidArgument, message};
}

inline int hexDigit(char character) {
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

inline bool isHex(const string& value, size_t expectedBytes = 0) {
    if (value.size() < 2 || value[0] != '0' || value[1] != 'x' || (value.size() & 1)) {
        return false;
    }
    if (expectedBytes && value.size() != 2 + 2 * expectedBytes) {
        return false;
    }
    for (size_t i = 2; i < value.size(); ++i) {
        if (hexDigit(value[i]) < 0) {
            return false;
        }
    }
    return true;
}

inline string lowerHex(string value) {
    for (char& character : value) {
        if (character >= 'A' && character <= 'F') {
            character += 'a' - 'A';
        }
    }
    return value;
}

inline string encodeHex(const uint8_t* buffer, size_t length) {
    static const char digits[] = "0123456789abcdef";
    string value(2 + length * 2, '0');
    value[1] = 'x';
    for (size_t i = 0; i < length; ++i) {
        value[2 + 2 * i] = digits[buffer[i] >> 4];
        value[3 + 2 * i] = digits[buffer[i] & 15];
    }
    return value;
}

// Requires a prefixed, even-length string already checked with isHex.
inline vector<uint8_t> decodeHex(const string& value) {
    vector<uint8_t> buffer((value.size() - 2) / 2);
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = (hexDigit(value[2 + i * 2]) << 4) | hexDigit(value[3 + i * 2]);
    }
    return buffer;
}

inline string abiWord(const uint256_t& value) {
    return value.str(16, 64);
}

inline string abiAddress(const string& value) {
    return string(24, '0') + lowerHex(value.substr(2));
}

inline bool isAddress(const string& value) {
    return isHex(value, 20);
}

inline bool isZeroAddress(const string& value) {
    return lowerHex(value) == "0x0000000000000000000000000000000000000000";
}

bool canonicalSignature(const uint8_t signature[65]);
bool signatureMatches(const uint8_t* signature, const uint8_t* digest, const string& expected);
SessionResult validatePolicyShape(const SessionPolicy& policy);
bool isReservedAddress(const string& value);
string functionSelector(const char* signature);
SessionResult checkedUint64(SessionResult result, const uint256_t& value, uint64_t& output);
bool unquoteHex(const string& raw, string& output);
bool parseQuantity(const string& raw, uint256_t& value);
} // namespace detail
} // namespace aplo
#endif

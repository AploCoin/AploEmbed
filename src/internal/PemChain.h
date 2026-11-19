#ifndef APLO_INTERNAL_PEM_CHAIN_H
#define APLO_INTERNAL_PEM_CHAIN_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace aplo {
namespace detail {
static const size_t MaxPemBytes = 4096;

inline char PemByte(const char* pem, size_t offset) {
#if defined(ESP8266)
    return static_cast<char>(pgm_read_byte(pem + offset));
#else
    return pem[offset];
#endif
}

inline bool PemSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline int PemBase64(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    return c == '+' ? 62 : c == '/' ? 63 : -1;
}

inline bool ExactDer(const uint8_t header[4], size_t bytes) {
    if (bytes < 2 || header[0] != 0x30) {
        return false;
    }
    size_t content = header[1];
    size_t prefix = 2;
    if (content & 0x80) {
        const size_t digits = content & 0x7f;
        if (!digits || digits > 2 || bytes < 2 + digits || header[2] == 0) {
            return false;
        }
        content = 0;
        for (size_t i = 0; i < digits; ++i) {
            content = (content << 8) | header[2 + i];
        }
        prefix += digits;
        if (content < 128) {
            return false;
        }
    }
    return bytes == prefix + content;
}

inline bool PemMarker(const char* pem, size_t length, size_t& offset, const char* marker) {
    const size_t size = strlen(marker);
    if (size > length - offset) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        if (PemByte(pem, offset + i) != marker[i]) {
            return false;
        }
    }
    offset += size;
    return true;
}

// Reject foreign objects, unfinished extra PEM and ignored text, even if the
// platform parser accepts an otherwise valid prefix. No PEM copy is made.
inline bool PemChain(const char* pem, size_t& length, unsigned& count) {
    length = 0;
    count = 0;
    if (!pem) {
        return false;
    }
    while (length <= MaxPemBytes && PemByte(pem, length)) {
        ++length;
    }
    if (!length || length > MaxPemBytes) {
        return false;
    }
    size_t offset = 0;
    while (offset < length) {
        while (offset < length && PemSpace(PemByte(pem, offset))) {
            ++offset;
        }
        if (offset == length) {
            break;
        }
        if (!PemMarker(pem, length, offset, "-----BEGIN CERTIFICATE-----")) {
            return false;
        }
        if (offset == length || !PemSpace(PemByte(pem, offset))) {
            return false;
        }
        size_t digits = 0;
        unsigned padding = 0;
        uint32_t accumulator = 0;
        unsigned bits = 0;
        size_t decoded = 0;
        uint8_t header[4] = {};
        while (offset < length && PemByte(pem, offset) != '-') {
            const char c = PemByte(pem, offset++);
            if (PemSpace(c)) {
                continue;
            }
            if (c == '=') {
                if (++padding > 2) {
                    return false;
                }
            } else {
                const int value = PemBase64(c);
                if (padding || value < 0) {
                    return false;
                }
                accumulator = (accumulator << 6) | static_cast<unsigned>(value);
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    if (decoded < sizeof(header)) {
                        header[decoded] = (accumulator >> bits) & 0xff;
                    }
                    ++decoded;
                }
            }
            ++digits;
        }
        if (!digits || digits % 4 || (accumulator & ((1u << bits) - 1)) ||
            !ExactDer(header, decoded) ||
            !PemMarker(pem, length, offset, "-----END CERTIFICATE-----")) {
            return false;
        }
        ++count;
    }
    return count != 0;
}
} // namespace detail
} // namespace aplo
#endif

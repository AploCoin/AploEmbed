#include "SessionKeys.h"
#include "SessionEncoding.h"
#include <Arduino.h>
extern "C" {
#include "trezor/memzero.h"
}

namespace aplo {
bool SessionKey::ReadFromSerial(Stream& serial, const std::string& expectedAddress) {
    Clear();
    if (!detail::isAddress(expectedAddress)) {
        return false;
    }

    char line[64];
    size_t length = 0;
    bool invalid = false;
    while (true) {
        const int next = serial.read();
        if (next < 0) {
            delay(10);
            continue;
        }

        if (next == '\r') {
            continue;
        }

        if (next == '\n') {
            break;
        }

        if (next == 8 || next == 127) {
            if (length != 0) {
                line[--length] = 0;
            }
        } else if (length == sizeof(line) || detail::hexDigit(static_cast<char>(next)) < 0) {
            invalid = true;
        } else {
            line[length++] = static_cast<char>(next);
        }
    }

    bool imported = false;
    if (!invalid && length == sizeof(line)) {
        uint8_t secret[32];
        for (size_t i = 0; i < sizeof(secret); ++i) {
            secret[i] = static_cast<uint8_t>(
                (detail::hexDigit(line[2 * i]) << 4) | detail::hexDigit(line[2 * i + 1])
            );
        }
        imported = Import(secret);
        memzero(secret, sizeof(secret));
    }
    memzero(line, sizeof(line));
    if (!imported || Address() != detail::lowerHex(expectedAddress)) {
        Clear();
        return false;
    }

    return true;
}
} // namespace aplo

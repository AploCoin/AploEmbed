#pragma once
#include <cstring>
#if defined(ESP8266)
#include <bearssl.h>
#endif
extern unsigned liveAnchors, maxAnchors;
namespace BearSSL {
// Instrument only ownership/count; OpenSSL performs host chain verification.
class X509List {
public:
    const char* pem;
    explicit X509List(const char* value) : pem(value) {
        ++liveAnchors;
        if (maxAnchors < liveAnchors) {
            maxAnchors = liveAnchors;
        }
    }
    ~X509List() {
        --liveAnchors;
    }
    unsigned getCount() const {
        unsigned count = 0;
        const char* next = pem;
        while (next && (next = std::strstr(next, "BEGIN CERTIFICATE"))) {
            ++count;
            ++next;
        }
        return count;
    }
};
} // namespace BearSSL

#pragma once
#include <cstring>
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
        return pem && std::strstr(pem, "BEGIN CERTIFICATE") ? 1 : 0;
    }
};
} // namespace BearSSL

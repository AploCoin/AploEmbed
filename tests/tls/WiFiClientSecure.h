#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "BearSSLTest.h"
extern bool tlsMfln;
extern std::string transportScenario;
extern std::vector<std::string> tlsAttempts;
struct ssl_ctx_st;
struct ssl_st;
class WiFiClientSecure {
    ssl_ctx_st* context_ = nullptr;
    ssl_st* ssl_ = nullptr;
    int socket_ = -1;
    const char* pem_ = nullptr;
    std::string outgoing_, incoming_;
    size_t position_ = 0;
    bool received_ = false;
    void receive();

public:
    ~WiFiClientSecure();
    void setCACert(const char* pem);
    void setTrustAnchors(const BearSSL::X509List* anchor) {
        pem_ = anchor ? anchor->pem : nullptr;
    }
    void setBufferSizes(int, int) {}
    void setTimeout(unsigned long) {}
    void setHandshakeTimeout(unsigned long) {}
    void setX509Time(long) {}
    bool getMFLNStatus() const {
        return tlsMfln;
    }
    int getLastSSLError(char*, size_t) {
        return -1;
    }
    bool connect(const char* host, unsigned port);
    bool connect(const char* host, unsigned port, unsigned long) { return connect(host, port); }
    void stop();
    bool connected();
    int available();
    int read();
    void print(const char* value);
    void println(const char* value);
    void println();
};

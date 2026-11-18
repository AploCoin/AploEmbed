#include "Web3.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <ctime>
std::vector<std::string> tlsAttempts;
static time_t verificationTime;
extern "C" time_t time(time_t* output) {
    if (output) {
        *output = verificationTime;
    }
    return verificationTime;
}
static std::string read(const char* path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
int main(int argc, char** argv) {
    assert(argc == 9);
    assert(Web3::isHttpsUrl("HTTPS://rpc.test:443/API?request=one"));
    for (const char* invalid : {"", "http://rpc.test", "rpc.test", "https://",
                                "https://user@rpc.test", "https://rpc.test:65536",
                                "https://rpc.test/path\r\nBad: header", "https://rpc.test/#fragment"}) {
        assert(!Web3::isHttpsUrl(invalid));
    }
    verificationTime = std::strtoll(argv[8], nullptr, 10);
    const std::string root = read(argv[3]), other = read(argv[4]);
    const std::string mode = argv[1];
    Web3 rpc(argv[2], argv[5]);
    if (mode == "valid" || mode == "rotation" || mode == "bad-name" || mode == "expired" ||
        mode == "no-time" || mode == "mfln") {
        rpc.setCertificate(root.c_str(), other.c_str());
    } else if (mode == "foreign") {
        rpc.setCertificate(other.c_str(), root.c_str());
    } else if (mode == "bundle") {
        rpc.setCertificateBundle(reinterpret_cast<const uint8_t*>(root.c_str()), root.size());
    } else if (mode == "disabled") {
        rpc.setInsecure();
    } else if (mode == "shared") {
        rpc.setCertificate(root.c_str());
    } else if (mode != "missing" && mode != "profile") {
        return 2;
    }
    if (mode == "mfln") {
        tlsMfln = false;
    }
    if (mode == "no-time") {
        verificationTime = 0;
    }
    const std::string response = rpc.RpcRequest("eth_chainId", "[]");
    const bool success = std::string(argv[6]) == "ok";
    assert((response == "{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":\"0x539\"}") == success);
    assert(tlsAttempts.size() == std::strtoul(argv[7], nullptr, 10));
    if (mode == "missing") {
        assert(rpc.tlsError() == Web3::TlsError::MissingTrust);
    }
    if (mode == "bundle" || mode == "disabled") {
        assert(rpc.tlsError() == Web3::TlsError::UnsupportedTrust);
    }
    if (mode == "no-time") {
        assert(rpc.tlsError() == Web3::TlsError::InvalidTime);
    }
    if (success) {
        assert(rpc.tlsError() == Web3::TlsError::None);
    }
#if defined(ESP8266)
    assert(liveAnchors <= 1 && maxAnchors <= 1);
    if (mode == "mfln") {
        assert(rpc.tlsError() == Web3::TlsError::FragmentLength);
    }
#endif
    std::cout << mode << ": PASS (TLS connects=" << tlsAttempts.size() << ")\n";
}

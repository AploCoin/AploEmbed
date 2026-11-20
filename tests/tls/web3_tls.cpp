#include "Web3.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <ctime>
#if defined(ESP32)
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif
std::vector<std::string> tlsAttempts;
static time_t verificationTime;
#if defined(ESP32)
static TaskHandle_t expectedCaller;
#endif
static void countIdle(void* context) {
#if defined(ESP32)
    assert(xTaskGetCurrentTaskHandle() == expectedCaller);
#endif
    ++*static_cast<unsigned*>(context);
}

struct ProgressTrace {
    Web3* rpc;
    unsigned calls = 0, connects = 0, writes = 0;
    bool recursionRejected = false;
#if defined(ESP32)
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
#endif
};

static void traceProgress(void* context) {
    auto& trace = *static_cast<ProgressTrace*>(context);
    ++trace.calls;
#if defined(ESP32)
    assert(xTaskGetCurrentTaskHandle() == trace.caller);
#endif
    if (std::string(transportPhase.load()) == "connect") {
        ++trace.connects;
    }
    if (std::string(transportPhase.load()) == "write") {
        ++trace.writes;
    }
    if (!trace.recursionRejected) {
        assert(trace.rpc->RpcRequest("eth_chainId", "[]").empty());
        trace.recursionRejected = true;
    }
}

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
#if defined(ESP32)
    expectedCaller = xTaskGetCurrentTaskHandle();
#endif
    assert(argc == 9);
    assert(Web3::isHttpsUrl("HTTPS://rpc.test:443/API?request=one"));
    for (const char* invalid :
         {"",
          "http://rpc.test",
          "rpc.test",
          "https://",
          "https://user@rpc.test",
          "https://rpc.test:65536",
          "https://rpc.test/path\r\nBad: header",
          "https://rpc.test/#fragment",
          "https://rpc.test/\\evil",
          "https://rpc.test/\"bad",
          "https://rpc.test/%",
          "https://rpc.test/%0G",
          "https://rpc.test/é",
          "https://rpc.test/{bad}",
          "https://rpc.test/|bad"}) {
        assert(!Web3::isHttpsUrl(invalid));
    }
    verificationTime = std::strtoll(argv[8], nullptr, 10);
    const std::string root = read(argv[3]), other = read(argv[4]);
    const std::string mode = argv[1];
    assert(Web3::isHttpsUrl("https://rpc.test/a%20b?x=one&y=%2f"));
    assert(!Web3::isValidCertificate(nullptr));
    assert(!Web3::isValidCertificate(""));
    assert(!Web3::isValidCertificate(" \n"));
    if (mode == "invalid-root") {
        assert(!Web3::isValidCertificate(root.c_str()));
    } else {
        assert(Web3::isValidCertificate(root.c_str()));
        assert(Web3::isValidCertificate((" \n" + root + "\t").c_str()));
        assert(Web3::isValidCertificate((root + std::string(4096 - root.size(), ' ')).c_str()));
        assert(!Web3::isValidCertificate((root + std::string(4097 - root.size(), ' ')).c_str()));
        assert(!Web3::isValidCertificate((root + "garbage").c_str()));
        assert(!Web3::isValidCertificate((root + "-----BEGIN CERTIFICATE-----\n").c_str()));
        assert(!Web3::isValidCertificate((root + "-----BEGIN CERTIFICATE-----\nAAAA\n"
                                                 "-----END CERTIFICATE-----\n")
                                             .c_str()));
        assert(!Web3::isValidCertificate((root + "-----BEGIN PRIVATE KEY-----\nAAAA\n"
                                                 "-----END PRIVATE KEY-----\n")
                                             .c_str()));
    }
    Web3 rpc(argv[2], argv[5]);
    unsigned idleCalls = 0;
    rpc.setRpcIdleCallback(countIdle, &idleCalls);
    ProgressTrace progress;
    progress.rpc = &rpc;
#if defined(ESP8266)
    nativeScheduleFails() = mode == "schedule-failure";
#endif
    const bool progressInstalled = rpc.setRpcProgressCallback(traceProgress, &progress);
#if defined(ESP32)
    nativeTaskFails() = mode == "worker-allocation-failure";
    nativeSemaphoreFails() = mode == "worker-semaphore-failure";
#endif
    if (mode == "no-progress") {
        assert(rpc.setRpcProgressCallback(nullptr));
    }
#if defined(ESP8266)
    assert(progressInstalled == (mode != "schedule-failure"));
#else
    assert(progressInstalled);
#endif
    assert(rpc.lastRpcEndpoint() == Web3::RpcEndpointUsed::None);
    if (mode == "valid" || mode == "rotation" || mode == "bad-name" || mode == "expired" ||
        mode == "no-time" || mode == "mfln" || mode == "invalid-root" || mode == "stalled-body" ||
        mode == "negative-read" || mode == "oversized" || mode == "continuous-body" ||
        mode == "late-header" || mode == "late-body-final" || mode == "write-delay" ||
        mode == "schedule-failure" || mode == "late-body-idle" || mode == "late-primary" ||
        mode == "worker-stall" || mode == "worker-allocation-failure" ||
        mode == "worker-semaphore-failure" || mode == "no-progress" ||
        mode == "worker-delayed-handshake") {
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
    if (mode == "stalled-body" || mode == "negative-read" || mode == "oversized" ||
        mode == "continuous-body" || mode == "late-header" || mode == "late-body-final" ||
        mode == "write-delay" || mode == "late-body-idle" || mode == "late-primary") {
        transportScenario = mode;
        // Cross the actual 32-bit Arduino millis rollover during a failed read.
        nativeMillisTicks() = UINT32_MAX - 400;
        if (mode == "oversized") {
            nativeMillisDivisor() = 16;
        }
    }
    if (mode == "worker-stall") {
        transportScenario = mode;
    }
    const std::string response = rpc.RpcRequest("eth_chainId", "[]");
    const bool success = std::string(argv[6]) == "ok";
    assert((response == "{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":\"0x539\"}") == success);
    assert(tlsAttempts.size() == std::strtoul(argv[7], nullptr, 10));
    if (success) {
        assert(idleCalls > 0);
        if (progressInstalled && mode != "no-progress") {
            assert(progress.calls > 0 && progress.recursionRejected);
#if defined(ESP8266)
            assert(progress.connects >= 100 && progress.writes >= 1);
#endif
        }
#if defined(ESP32) && APLO_RPC_ESP32_PROGRESS_WORKER
        if (mode == "worker-stall" || mode == "worker-delayed-handshake") {
            assert(progress.connects > 0);
        }
        assert(
            nativeTasksStarted() == ((mode == "worker-allocation-failure" ||
                                      mode == "worker-semaphore-failure" || mode == "no-progress")
                                         ? 0
                                         : 1)
        );
#endif
        const bool fallbackUsed =
            tlsAttempts.size() > 1 ||
            (mode == "profile" && std::string(argv[2]).find("unknown.test") != std::string::npos);
        assert(
            rpc.lastRpcEndpoint() ==
            (!fallbackUsed ? Web3::RpcEndpointUsed::Primary : Web3::RpcEndpointUsed::Fallback)
        );
    } else {
        assert(rpc.lastRpcEndpoint() == Web3::RpcEndpointUsed::None);
    }
    const unsigned completedProgress = progress.calls;
    yield();
    assert(progress.calls == completedProgress); // no borrowed context after RPC
    if (mode == "invalid-root") {
        assert(rpc.tlsError() == Web3::TlsError::InvalidTrust);
    }
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
    if (mode == "valid") {
        const std::string correct = "0x" + std::string(64, 'a');
        const std::string wrong = "0x" + std::string(64, 'b');
        assert(rpc.EthGetTransactionReceiptStatus(&correct) == 1);
        assert(rpc.EthGetTransactionReceiptStatus(&wrong) == -1);
        const size_t attempts = tlsAttempts.size();
        const std::string injection = "0x\"bad";
        assert(rpc.EthGetTransactionReceiptStatus(&injection) == -1);
        assert(tlsAttempts.size() == attempts);
    }
#if defined(ESP8266)
    assert(liveAnchors <= 1 && maxAnchors <= 1);
    if (mode == "mfln") {
        assert(rpc.tlsError() == Web3::TlsError::FragmentLength);
    }
    rpc.setCertificate(nullptr, nullptr);
    assert(liveAnchors == 0);
#endif
#if !APLO_RPC_SERIAL_LOGS
    assert(NativeSerial::writes() == 0);
#endif
    std::cout << mode << ": PASS (TLS connects=" << tlsAttempts.size()
              << ", progress=" << progress.calls << ", during-connect=" << progress.connects
              << ", during-write=" << progress.writes << ")\n";
}

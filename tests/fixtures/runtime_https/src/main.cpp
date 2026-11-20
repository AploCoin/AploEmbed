// Compile/link probe only. No upload or hardware execution is part of this test.
#include <Web3.h>
#include <SessionKeys.h>

static const char certificate[] PROGMEM = "invalid test PEM";
static Web3 rpc("https://rpc.example.invalid/rpc", "https://fallback.example.invalid/rpc");
static aplo::SessionKeys sessions(rpc);
static void idle(void*) { yield(); }

void setup() {
    rpc.setRpcIdleCallback(idle, nullptr);
    (void)rpc.lastRpcEndpoint();
    rpc.setCertificate(certificate, certificate);
    // Invalid synthetic input prevents this probe from submitting a request
    // even if someone explicitly runs it; it still links the new API surface.
    if (Web3::isValidCertificate(certificate) && Web3::isHttpsUrl("https://rpc.example.invalid")) {
        uint64_t latest, pending;
        const std::string signer = "0x0000000000000000000000000000000000000001";
        sessions.LatestNonce(signer, latest);
        sessions.PendingNonce(signer, pending);
    }
    rpc.setCertificate(nullptr, nullptr);
}

void loop() {}

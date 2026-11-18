#include <Arduino.h>
#include <SessionKeys.h>
#include <Contract.h>

// Demonstrates the transport boundary only. Replace with the application's
// authenticated transport; never translate an unverified response into success.
static std::string transport(const std::string&, const std::string&, void*) {
    return "{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":\"0x539\"}";
}
void setup() {
    Serial.begin(115200);
    // The generic ABI helper also remains usable without constructing Web3.
    Contract abi(nullptr, "");
    const std::string account = "0x1111111111111111111111111111111111111111";
    Serial.println(abi.SetupContractData("balanceOf(address)", &account).c_str());
    aplo::SessionKeys rpc(transport, nullptr);
    uint256_t chain;
    if (rpc.ChainId(chain)) {
        Serial.println("Custom transport adapter compiled");
    }
}
void loop() {}

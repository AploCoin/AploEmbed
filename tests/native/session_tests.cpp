#include <Arduino.h>
#include "ReceiptFilter.h"
#include "SessionKeys.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace aplo;
using std::string;
static string owner;

struct Fixture {
    string response, lastMethod, lastParams;

    static string transport(const string& method, const string& params, void* context) {
        Fixture& f = *static_cast<Fixture*>(context);
        f.lastMethod = method;
        f.lastParams = params;
        return f.response;
    }
};

void legacyTests();
void rpcTests();
int nativeIntegration();

namespace {
class SessionInput : public Stream {
public:
    explicit SessionInput(const std::string& text) : text_(text) {}
    int read() override {
        assert(position_ < text_.size());
        return static_cast<unsigned char>(text_[position_++]);
    }

private:
    std::string text_;
    size_t position_ = 0;
};

void testSessionSerial() {
    aplo::SessionKey key;
    const std::string secret = std::string(63, '0') + "1";
    const char* expected = "0x7E5F4552091A69125d5dFCb7b8C2659029395Bdf";
    SessionInput correct(secret + "\r\n");
    assert(key.ReadFromSerial(correct, expected));
    SessionInput edited(std::string(63, '0') + "2\b1\n");
    assert(key.ReadFromSerial(edited, expected));
    for (const std::string& text : {secret + "0\n", secret.substr(1) + "\n",
                                   std::string(63, '0') + "g\n", std::string(63, '0') + "2\n"}) {
        SessionInput invalid(text);
        assert(!key.ReadFromSerial(invalid, expected));
        assert(key.Address().empty());
    }
}
} // namespace

int main(int argc, char** argv) {
    testSessionSerial();
    if (argc == 2 && string(argv[1]) == "--integration") {
        return nativeIntegration();
    }
    if (argc == 10 && string(argv[1]) == "sign") {
        SessionKey key;
        uint8_t secret[32];
        for (int i = 0; i < 32; ++i) {
            secret[i] =
                static_cast<uint8_t>(strtoul(string(argv[2] + 2 * i, 2).c_str(), nullptr, 16));
        }
        if (!key.Import(secret)) {
            return 2;
        }
        NativeTransaction tx;
        tx.chainId = uint256_t(argv[3]);
        tx.nonce = strtoull(argv[4], nullptr, 10);
        tx.gasPrice = uint256_t(argv[5]);
        tx.gasLimit = strtoull(argv[6], nullptr, 10);
        tx.to = argv[7];
        tx.value = uint256_t(argv[8]);
        tx.data = argv[9];
        string raw;
        auto r = SessionKeys::SignTransaction(tx, SessionKey::SignDigest, &key, key.Address(), raw);
        if (!r) {
            std::cerr << r.message;
            return 2;
        }
        std::cout << raw;
        return 0;
    }
    if (argc == 9 && string(argv[1]) == "create") {
        SessionKey key;
        uint8_t secret[32];
        for (int i = 0; i < 32; ++i) {
            secret[i] =
                static_cast<uint8_t>(strtoul(string(argv[2] + 2 * i, 2).c_str(), nullptr, 16));
        }
        if (!key.Import(secret)) {
            return 2;
        }
        SessionPolicy p;
        p.key = key.Address();
        p.target = argv[5];
        Selector sel;
        for (int i = 0; i < 4; ++i) {
            sel[i] = static_cast<uint8_t>(strtoul(string(argv[6] + 2 * i, 2).c_str(), nullptr, 16));
        }
        p.selectors.push_back(sel);
        p.gaplo = uint256_t(argv[7]);
        p.expiry = strtoull(argv[8], nullptr, 10);
        string proof, data;
        auto r = SessionKeys::AcceptanceProof(p, argv[3], uint256_t(argv[4]), key, proof);
        if (!r) {
            return 2;
        }
        r = SessionKeys::EncodeCreate(p, proof, data);
        if (!r) {
            return 2;
        }
        std::cout << data;
        return 0;
    }
    legacyTests();
    rpcTests();
    owner = "0x19E7E376E7C213B7E7e7e46cc70A5dD086DAff2A";
    SessionKey key;
    uint8_t secret[32];
    memset(secret, 0x22, 32);
    assert(key.Import(secret));
    SessionPolicy p;
    p.key = key.Address();
    p.target = "0x0000000000000000000000000000000000001234";
    p.selectors.push_back({{0x2f, 0xdc, 0x50, 0x5e}});
    p.gaplo = uint256_t(UINT64_C(1000000000000000000));
    p.expiry = 300;
    assert(p.key == "0x1563915e194d8cfba1943570603f7606a3115508");
    uint8_t hash[32];
    assert(SessionKeys::ProofHash(p, owner, 424242, hash));
    const uint8_t expected[] = {0xa0, 0xaf, 0x7c, 0x0f, 0x45, 0xaf, 0x3a, 0xea, 0x92, 0x65, 0xb0,
                                0x04, 0x0d, 0xa9, 0x0c, 0xda, 0x5f, 0xd8, 0x45, 0x82, 0x67, 0x45,
                                0x93, 0x87, 0xa0, 0x0d, 0x9e, 0xd6, 0x38, 0x64, 0x11, 0x6c};
    assert(memcmp(hash, expected, 32) == 0);
    string proof;
    assert(SessionKeys::AcceptanceProof(p, owner, 424242, key, proof));
    assert(proof == "0xac7d8058b616878ef4b18b7bbaf9be205cb45684d4ef5c3e1fd81e9debae0a7e4c1b6e0f3984"
                    "53f7cb1e88e15fb147953c2ae7f414331d8124ea85d2201ca08500");
    string data;
    assert(SessionKeys::EncodeCreate(p, proof, data));
    assert(data.size() == 842);
    assert(SessionKeys::Validate(p, 300));
    assert(!SessionKeys::Validate(p, 301));
    assert(SessionKeys::Validate(p, 0));
    p.expiry = 1001;
    assert(!SessionKeys::Validate(p, 0));
    p.expiry = 300;
    assert(!SessionKeys::Validate(p, UINT64_MAX));
    SessionPolicy many = p;
    many.selectors.clear();
    for (unsigned i = 0; i < 32; ++i) {
        many.selectors.push_back({{0, 0, 0, static_cast<uint8_t>(i)}});
    }
    assert(SessionKeys::Validate(many, 0));
    assert(SessionKeys::EncodeCreate(many, proof, data));
    assert(data.size() == 2826);
    many.selectors.push_back({{0, 0, 1, 0}});
    assert(!SessionKeys::Validate(many, 0));
    p.selectors.push_back(p.selectors[0]);
    assert(!SessionKeys::Validate(p, 0));
    p.selectors.pop_back();
    NativeTransaction tx;
    tx.to = p.target;
    tx.data = "0x2fdc505e" + string(64, '0');
    tx.chainId = 424242;
    tx.gasPrice = 1;
    tx.gasLimit = 100000;
    tx.nonce = uint64_t(1) << 40;
    string raw;
    assert(SessionKeys::SignTransaction(tx, SessionKey::SignDigest, &key, p.key, raw));
    assert(raw.substr(0, 2) == "0x");
    tx.nonce = UINT64_MAX;
    assert(!SessionKeys::SignTransaction(tx, SessionKey::SignDigest, &key, p.key, raw));
    assert(raw.empty());
    tx.nonce = 0;
    tx.chainId = ~uint256_t(0);
    assert(SessionKeys::SignTransaction(tx, SessionKey::SignDigest, &key, p.key, raw));
    tx.chainId = 424242;
    assert(!SessionKeys::SignTransaction(tx, SessionKey::SignDigest, &key, owner, raw));
    memset(secret, 0, 32);
    assert(!key.Import(secret));
    assert(key.Address().empty());
    memset(secret, 255, 32);
    assert(!key.Import(secret));
    assert(key.Generate());
    for (const string json :
         {R"({"result":{"status":"0x1","logs":[]}})",
          R"({"jsonrpc":"2.0","result":{"logs":[{"status":"0x0"}],"status":"0x1"},"id":0})"}) {
        ReceiptFilter filter;
        for (char c : json) {
            filter.Push(c);
        }
        assert(filter.Status() == 1);
    }
    for (const string json : {R"({"result":{"logs":[{"status":"0x1"}],"status":"0x0"}})"}) {
        ReceiptFilter filter;
        for (char c : json) {
            filter.Push(c);
        }
        assert(filter.Status() == 0);
    }
    for (const string json :
         {R"({"result":{},"unrelated":{"status":"0x1"}})",
          R"({"result":{"logs":[{"status":"0x1"}]}})",
          R"({"result":{"status":"0x1"},"error":{"code":-1}})",
          R"({"result":{"status":"0x1","status":"0x0"}})",
          R"({"result":{"status":"0x1"},"result":null})", R"({"result":{"status":"0x1"},})",
          R"({"result":{"status":"0x1"}}junk)", R"({"result":{"status":"0x1")"}) {
        ReceiptFilter filter;
        for (char c : json) {
            filter.Push(c);
        }
        assert(filter.Status() == -1);
    }
    Fixture f;
    SessionKeys rpc(Fixture::transport, &f);
    uint256_t n = 123;
    f.response = "";
    assert(rpc.ChainId(n).code == SessionError::TransportError);
    assert(n == 0);
    f.response = R"({"error":{"code":-32000,"message":"rejected"}})";
    assert(rpc.ChainId(n).code == SessionError::RpcError);
    for (const string bad : {"{}", "{", R"({"result":"0x00"})", R"({"result":"0x"})",
                             R"({"result":"0xg"})", R"({"result":"0x1","result":"0x2"})",
                             R"({"result":[]})", R"({"result":})", R"({"result":"0x1"} junk)"}) {
        f.response = bad;
        assert(rpc.ChainId(n).code == SessionError::InvalidResponse);
    }
    f.response = R"({"result":"0x123456789abcdef0123456789"})";
    assert(rpc.ChainId(n));
    assert(n == uint256_t("0x123456789abcdef0123456789"));
    uint64_t nonce;
    assert(!rpc.PendingNonce(p.key, nonce));
    f.response = R"({"result":"0x10000000000"})";
    assert(rpc.PendingNonce(p.key, nonce));
    assert(nonce == (UINT64_C(1) << 40));
    assert(f.lastParams.find(p.key) != string::npos);
    assert(f.lastParams.find("pending") != string::npos);
    ReceiptState state = ReceiptState::Succeeded;
    f.response = R"({"result":null})";
    assert(rpc.Receipt("0x" + string(64, 'a'), state));
    assert(state == ReceiptState::Pending);
    f.response = R"({"result":{"logs":[{"status":"0x1"}],"status":"0x0"}})";
    assert(rpc.Receipt("0x" + string(64, 'a'), state));
    assert(state == ReceiptState::Reverted);
    f.response = R"({"result":{"logs":[{"status":"0x1"}]}})";
    assert(rpc.Receipt("0x" + string(64, 'a'), state).code == SessionError::InvalidResponse);
    f.response = R"({"error":{"code":-1,"message":"no receipt"}})";
    assert(rpc.Receipt("0x" + string(64, 'a'), state).code == SessionError::RpcError);
    f.response = R"({"result":{"status":"0x1"}})";
    assert(rpc.Receipt("0x" + string(64, 'a'), state));
    assert(state == ReceiptState::Succeeded);
    SessionStatus status;
    string record = string(24, '0') + owner.substr(2) + string(24, '0') + p.target.substr(2) +
                    uint256_t(0).str(16, 64) + p.gaplo.str(16, 64) +
                    uint256_t(UINT64_C(1) << 40).str(16, 64) + uint256_t(300).str(16, 64);
    f.response = "{\"result\":\"0x" + record + "\"}";
    assert(rpc.Read(p.key, owner, status));
    assert(status.active);
    assert(status.nonce == (UINT64_C(1) << 40));
    assert(status.gaplo == p.gaplo);
    assert(f.lastParams.find(owner) != string::npos);
    f.response = "{\"result\":\"0x" + string(384, '0') + "\"}";
    assert(rpc.Read(p.key, owner, status));
    assert(!status.active);
    f.response = R"({"result":"0x"})";
    assert(!rpc.Read(p.key, owner, status));
    assert(!status.active);
    f.response = R"({"result":"0x00"})";
    string output;
    assert(rpc.Call(owner, p.target, "0x", output));
    assert(output == "0x00");
    assert(!rpc.Call(owner, p.target, "0x", output, "latest\""));
    assert(uint256_t(0).export_bits_truncate().empty());
    std::cout << "RPC errors/receipt separation and registry ABI tests passed\n";
    std::cout << "proof vector, ABI, expiry, signer, uint64 nonce, uint256 chain, CSPRNG key tests "
                 "passed\n";
}

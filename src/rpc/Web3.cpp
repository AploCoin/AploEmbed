// AploEmbed main implementation
//
// Specialized for AploCoin blockchain
// Based on Web3E by James Brown (@JamesSmartCell, @AlphaWallet)
// Original Web3 Arduino by Okada, Takahiro
//

#include "Web3.h"

#if APLO_ENABLE_HTTPS

#include "Contract.h"
#include "Util.h"
#include "TagReader/TagReader.h"
#include "internal/RpcResponseBuffer.h"
#include "internal/PemChain.h"
#include "internal/RpcLog.h"
#include <memory>
#if !defined(ESP8266)
#include <mbedtls/x509_crt.h>
#endif
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <new>

#ifdef APLO_TRUST_PROFILE_HEADER
#include APLO_TRUST_PROFILE_HEADER
#endif
#include "nodes.h"

// Helper function to initialize Web3 instance
using std::string;
using std::vector;

#if defined(ESP8266)
#define APLO_RPC_FAILURE_COOLDOWN_MS 30000UL
#define APLO_X509_TIME_RETRY_MS 30000UL
#define APLO_X509_TIME_WAIT_MS 15000UL
#define APLO_VALID_X509_EPOCH 1609459200L

// ESP8266 BearSSL allocates a shared 6200-byte secondary stack from its client
// constructor. The singleton is first constructed by initWeb3(); using a
// function-local static avoids cross-translation-unit initialization order
// hazards when applications create a global Web3 before setup().
static WiFiClientSecure& getEsp8266Client() {
    static WiFiClientSecure transport;
    return transport;
}

static bool esp8266DeadlineReached(unsigned long deadline) {
    return deadline == 0 || static_cast<long>(millis() - deadline) >= 0;
}

bool Web3::ensureEsp8266X509Time() {
    static bool configured = false;
    static unsigned long retryAfterMs = 0;

    if (time(nullptr) >= APLO_VALID_X509_EPOCH) {
        return true;
    }

    if (!esp8266DeadlineReached(retryAfterMs)) {
        return false;
    }

    if (!configured) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        configured = true;
    }

    const unsigned long startedAt = millis();
    while (time(nullptr) < APLO_VALID_X509_EPOCH && millis() - startedAt < APLO_X509_TIME_WAIT_MS) {
        rpcIdle();
        delay(1);
    }

    if (time(nullptr) >= APLO_VALID_X509_EPOCH) {
        return true;
    }

    retryAfterMs = millis() + APLO_X509_TIME_RETRY_MS;
    APLO_RPC_LOG("TLS paused: unable to synchronize certificate time");
    return false;
}
#endif

static string getJsonResultValue(const string* json) {
    if (json == nullptr) {
        return string("");
    }

    TagReader reader;
    return reader.getTag(json, "result");
}

void Web3::initWeb3(const char* primaryRpc, const char* fallbackRpc) {
#if defined(ESP8266)
    client = &getEsp8266Client();
#else
    client = nullptr;
#endif
    host = nullptr;
    chainId = APLO_ID; // Fixed to AploCoin chain ID (28282)

    certMode = CERT_PROFILE;
    caCert = nullptr;
    certBundle = nullptr;
    certBundleSize = 0;
    fallbackCaCert = nullptr;
    lastTlsError = TlsError::None;
    rpcIdleCallback = nullptr;
    rpcIdleContext = nullptr;
    lastEndpointUsed = RpcEndpointUsed::None;
#if defined(ESP8266)
    esp8266TrustAnchor = nullptr;
    esp8266TrustAnchorCert = nullptr;
#endif

    parseEndpoint(primaryRpc, &primaryEndpoint);
    parseEndpoint(fallbackRpc, &fallbackEndpoint);
}

void Web3::setRpcIdleCallback(void (*callback)(void*), void* context) {
    rpcIdleCallback = callback;
    rpcIdleContext = context;
}

void Web3::rpcIdle() {
    if (rpcIdleCallback) {
        rpcIdleCallback(rpcIdleContext);
    }
    yield();
}

// Default constructor: uses default Aplo RPC endpoints
Web3::Web3() {
#ifdef APLO_TRUST_PROFILE_HEADER
    initWeb3(APLO_TRUST_PRIMARY_URL, APLO_TRUST_FALLBACK_URL);
#else
    initWeb3(getAploPrimaryNode(), getAploFallbackNode());
#endif
}

// Custom RPC constructor
Web3::Web3(const char* primaryRpcUrl) {
    initWeb3(primaryRpcUrl, nullptr);
}

// Custom RPC with fallback
Web3::Web3(const char* primaryRpcUrl, const char* fallbackRpcUrl) {
    initWeb3(primaryRpcUrl, fallbackRpcUrl);
}

// Legacy constructor for backward compatibility
Web3::Web3(long long networkId) {
    // Ignore networkId parameter, always use APLO_ID
    if (networkId != APLO_ID) {
        APLO_RPC_LOG("Ignoring unsupported chain ID");
    }
#ifdef APLO_TRUST_PROFILE_HEADER
    initWeb3(APLO_TRUST_PRIMARY_URL, APLO_TRUST_FALLBACK_URL);
#else
    initWeb3(getAploPrimaryNode(), getAploFallbackNode());
#endif
}

Web3::~Web3() {
#if defined(ESP8266)
    clearEsp8266Trust();
#endif
}

#if defined(ESP8266)
void Web3::clearEsp8266Trust() {
    getEsp8266Client().stop();
    getEsp8266Client().setTrustAnchors(nullptr);
    delete esp8266TrustAnchor;
    esp8266TrustAnchor = nullptr;
    esp8266TrustAnchorCert = nullptr;
}
#endif

// Certificate validation configuration methods

void Web3::setCertificateBundle(const uint8_t* bundle_start) {
    setCertificateBundle(bundle_start, 0);
}

void Web3::setCertificateBundle(const uint8_t* bundle_start, size_t bundle_size) {
    certMode = CERT_BUNDLE;
    certBundle = bundle_start;
    certBundleSize = bundle_size;
}

void Web3::setCertificate(const char* root_ca) {
    setCertificate(root_ca, root_ca);
}

void Web3::setCertificate(const char* primary_ca, const char* fallback_ca) {
#if defined(ESP8266)
    if (!primary_ca && !fallback_ca) {
        clearEsp8266Trust();
    }
#endif
    certMode = CERT_CA;
    caCert = primary_ca;
    fallbackCaCert = fallback_ca;
}

#if defined(ESP8266)
namespace {
// A transient decoder validates every PEM object without allocating another
// X509List or retaining certificate bytes alongside the active trust anchor.
struct CertificateDecoder {
    br_pem_decoder_context pem;
    br_x509_decoder_context certificate;

    static void Append(void* context, const void* data, size_t length) {
        auto* self = static_cast<CertificateDecoder*>(context);
        br_x509_decoder_push(&self->certificate, data, length);
    }

    bool Complete() {
        return br_x509_decoder_last_error(&certificate) == 0 &&
               br_x509_decoder_get_pkey(&certificate) != nullptr &&
               br_x509_decoder_isCA(&certificate);
    }
};
} // namespace
#endif

bool Web3::isValidCertificate(const char* pem) {
    size_t length;
    unsigned expected;
    if (!aplo::detail::PemChain(pem, length, expected)) {
        return false;
    }
#if defined(ESP8266)
    std::unique_ptr<CertificateDecoder> decoder(new (std::nothrow) CertificateDecoder);
    if (!decoder) {
        return false;
    }
    br_pem_decoder_init(&decoder->pem);
    unsigned parsed = 0;
    bool inCertificate = false;
    // The extra newline also finishes an END marker without a final newline.
    for (size_t offset = 0; offset <= length; ++offset) {
        const char c = offset == length ? '\n' : aplo::detail::PemByte(pem, offset);
        if (br_pem_decoder_push(&decoder->pem, &c, 1) != 1) {
            return false;
        }
        switch (br_pem_decoder_event(&decoder->pem)) {
        case BR_PEM_BEGIN_OBJ:
            if (inCertificate || strcmp(br_pem_decoder_name(&decoder->pem), "CERTIFICATE")) {
                return false;
            }
            inCertificate = true;
            br_x509_decoder_init(&decoder->certificate, nullptr, nullptr, nullptr, nullptr);
            br_pem_decoder_setdest(&decoder->pem, CertificateDecoder::Append, decoder.get());
            break;
        case BR_PEM_END_OBJ:
            if (!inCertificate || !decoder->Complete()) {
                return false;
            }
            inCertificate = false;
            ++parsed;
            break;
        case BR_PEM_ERROR:
            return false;
        default:
            break;
        }
    }
    return !inCertificate && parsed == expected;
#else
    mbedtls_x509_crt chain;
    mbedtls_x509_crt_init(&chain);
    // Positive values mean a partially parsed chain, which must also fail.
    const int result =
        mbedtls_x509_crt_parse(&chain, reinterpret_cast<const unsigned char*>(pem), length + 1);
    unsigned parsed = 0;
    bool allCa = true;
    for (const mbedtls_x509_crt* cert = &chain; cert && cert->raw.p; cert = cert->next) {
        ++parsed;
#if defined(MBEDTLS_PRIVATE)
        allCa = allCa && cert->MBEDTLS_PRIVATE(ca_istrue) == 1;
#else
        allCa = allCa && cert->ca_istrue == 1;
#endif
    }
    mbedtls_x509_crt_free(&chain);
    return result == 0 && parsed == expected && allCa;
#endif
}

void Web3::setAutoCertificate() {
    certMode = CERT_PROFILE;
}

void Web3::setInsecure() {
    certMode = CERT_UNAVAILABLE;
}

string Web3::RpcRequest(const string& method, const string& params) {
    string input = generateJson(&method, &params);
    return execWithFailover(&input);
}

string Web3::Web3ClientVersion() {
    string m = "web3_clientVersion";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getString(&output);
}

string Web3::Web3Sha3(const string* data) {
    string m = "web3_sha3";
    string p = "[\"" + *data + "\"]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getString(&output);
}

int Web3::NetVersion() {
    string m = "net_version";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getInt(&output);
}

bool Web3::NetListening() {
    string m = "net_listening";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getBool(&output);
}

int Web3::NetPeerCount() {
    string m = "net_peerCount";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getInt(&output);
}

double Web3::EthProtocolVersion() {
    string m = "eth_protocolVersion";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getDouble(&output);
}

bool Web3::EthSyncing() {
    string m = "eth_syncing";
    string p = "[]";
    string input = generateJson(&m, &p);
    string result = execWithFailover(&input);

    return getBool(&result);
}

bool Web3::EthMining() {
    string m = "eth_mining";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getBool(&output);
}

double Web3::EthHashrate() {
    string m = "eth_hashrate";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getDouble(&output);
}

long long int Web3::EthGasPrice() {
    string m = "eth_gasPrice";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getLongLong(&output);
}

void Web3::EthAccounts(char** array, int size) {
    // eth_accounts is deprecated in modern Ethereum clients
    // For AploCoin embedded use, accounts are managed locally via private keys
    // This method is kept for API compatibility but returns empty
    (void)array; // Suppress unused parameter warning
    (void)size;
}

int Web3::EthBlockNumber() {
    string m = "eth_blockNumber";
    string p = "[]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getInt(&output);
}

uint256_t Web3::EthGetBalance(const string* address) {
    string m = "eth_getBalance";
    string p = "[\"" + *address + "\",\"latest\"]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getUint256(&output);
}

string Web3::EthViewCall(const string* data, const char* to) {
    string m = "eth_call";
    string p = "[{\"data\":\"";
    p += data->c_str();
    p += "\",\"to\":\"";
    p += to;
    p += "\"}, \"latest\"]";
    string input = generateJson(&m, &p);
    return execWithFailover(&input);
}

int Web3::EthGetTransactionCount(const string* address) {
    string m = "eth_getTransactionCount";
    string p = "[\"" + *address +
               "\",\"pending\"]"; // in case we need to push several transactions in a row
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getInt(&output);
}

int Web3::EthGetTransactionReceiptStatus(const string* transactionHash) {
    if (!transactionHash) {
        return -1;
    }
    string hash = *transactionHash;
    if (hash.rfind("0x", 0) != 0 && hash.rfind("0X", 0) != 0) {
        hash.insert(0, "0x");
    }
    if (hash.size() != 66) {
        return -1;
    }
    hash[1] = 'x';
    for (size_t i = 2; i < hash.size(); ++i) {
        if (!isxdigit(static_cast<unsigned char>(hash[i]))) {
            return -1;
        }
    }
    const string output = RpcRequest("eth_getTransactionReceipt", "[\"" + hash + "\"]");
    aplo::detail::JsonRpcParser parser;
    for (char character : output) {
        parser.Push(character);
    }
    switch (parser.ReceiptResult(hash.c_str())) {
    case aplo::detail::JsonRpcParser::Receipt::Succeeded:
        return 1;
    case aplo::detail::JsonRpcParser::Receipt::Reverted:
        return 0;
    default:
        return -1;
    }
}

string Web3::EthCall(
    const string* from,
    const char* to,
    long gas,
    long gasPrice,
    const string* value,
    const string* data
) {
    // Build eth_call with optional gas, gasPrice, and value parameters
    string m = "eth_call";
    string p = "[{\"from\":\"" + *from + "\",\"to\":\"" + *to + "\",\"data\":\"" + *data + "\"";

    // Add gas if specified (non-zero)
    if (gas > 0) {
        char gasHex[32];
        snprintf(gasHex, sizeof(gasHex), "0x%lx", gas);
        p += ",\"gas\":\"";
        p += gasHex;
        p += "\"";
    }

    // Add gasPrice if specified (non-zero)
    if (gasPrice > 0) {
        char gasPriceHex[32];
        snprintf(gasPriceHex, sizeof(gasPriceHex), "0x%lx", gasPrice);
        p += ",\"gasPrice\":\"";
        p += gasPriceHex;
        p += "\"";
    }

    // Add value if specified (non-empty and non-zero)
    if (value != nullptr && !value->empty() && *value != "0" && *value != "0x0") {
        p += ",\"value\":\"" + *value + "\"";
    }

    p += "}, \"latest\"]";
    string input = generateJson(&m, &p);
    return execWithFailover(&input);
}

string Web3::EthSendSignedTransaction(const string* data, const uint32_t dataLen) {
    (void)dataLen;
    if (data == nullptr || data->empty()) {
        return "";
    }

    string input;
    input.reserve(data->size() + 80);
    input = "{\"jsonrpc\":\"2.0\",\"method\":\"eth_sendRawTransaction\",\"params\":[\"";
    input += *data;
    input += "\"],\"id\":0}";
    string output = execWithFailover(&input);
    return getResult(&output);
}

// -------------------------------
// Private

string Web3::generateJson(const string* method, const string* params) {
    return "{\"jsonrpc\":\"2.0\",\"method\":\"" + *method + "\",\"params\":" + *params +
           ",\"id\":0}";
}

string Web3::exec(const string* data, RpcEndpoint& endpoint) {
    if (data == nullptr || !endpoint.valid) {
        return "";
    }

#if defined(ESP8266)
    if (!esp8266DeadlineReached(endpoint.retryAfterMs)) {
        return "";
    }
#endif

    host = endpoint.host.c_str();

#if defined(ESP8266)
    // Receipt payloads include logs and can exceed the general response buffer.
    // Track only the status token while streaming so confirmation never needs
    // the complete multi-kilobyte receipt in heap.
    const bool receiptStatusRequest =
        data->find("\"method\":\"eth_getTransactionReceipt\"") != string::npos;
    aplo::detail::RpcResponseBuffer response(APLO_ESP8266_RPC_RESPONSE_MAX, receiptStatusRequest);
    // The standalone probe omits SNI. SNI-routed RPC endpoints can reject it
    // even when the normal BearSSL handshake supports MFLN. Request 1024 and
    // validate the negotiated result after the real hostname-aware connection.
    if (endpoint.tlsBufferSize == 0) {
        endpoint.tlsBufferSize = 1024;
    }
#else
    string result;
#endif

#if defined(ESP8266)
    // Reuse the transport reserved before setup().
    client = &getEsp8266Client();
    client->setBufferSizes(endpoint.tlsBufferSize, endpoint.tlsBufferSize);
#else
    WiFiClientSecure scopedClient;
    client = &scopedClient;
    client->setHandshakeTimeout(APLO_RPC_HANDSHAKE_TIMEOUT_SECONDS);
#endif
    client->stop();
    if (!setupCert(endpoint)) {
        client = nullptr;
        return "";
    }
#if defined(ESP8266)
    if (!ensureEsp8266X509Time()) {
        lastTlsError = TlsError::InvalidTime;
        endpoint.retryAfterMs = millis() + APLO_RPC_FAILURE_COOLDOWN_MS;
        client = nullptr;
        return "";
    }
#else
    // The application owns clock synchronization on ESP32; never skip X509 time.
    if (time(nullptr) < 1609459200L) {
        lastTlsError = TlsError::InvalidTime;
        APLO_RPC_LOG("TLS paused: valid system time unavailable");
        client = nullptr;
        return "";
    }
#endif
#if defined(ESP8266)
    const time_t now = time(nullptr);
    client->setX509Time(now);
#endif

    rpcIdle();
#if defined(ESP32)
    const bool connected = client->connect(host, endpoint.port, APLO_RPC_CONNECT_TIMEOUT_MS);
#else
    const bool connected = client->connect(host, endpoint.port);
#endif
    rpcIdle();
    if (!connected) {
        lastTlsError = TlsError::ConnectionFailed;
        APLO_RPC_LOG("Unable to connect to RPC host");
#if defined(ESP8266)
        endpoint.retryAfterMs = millis() + APLO_RPC_FAILURE_COOLDOWN_MS;
#endif
        client->stop();
        client = nullptr;
        return "";
    }
#if defined(ESP32)
    // connect(timeout) sets the socket timeout as well; reduce it only after
    // connecting. ESP32 setTimeout takes seconds, connect(timeout) takes ms.
    client->setTimeout(1);
#endif
#if defined(ESP8266)
    if (!client->getMFLNStatus()) {
        lastTlsError = TlsError::FragmentLength;
        APLO_RPC_LOG("RPC host did not negotiate ESP8266 TLS MFLN");
        endpoint.retryAfterMs = millis() + APLO_RPC_FAILURE_COOLDOWN_MS;
        client->stop();
        client = nullptr;
        return "";
    }
#endif

    char contentLength[24];
    snprintf(contentLength, sizeof(contentLength), "%lu", static_cast<unsigned long>(data->size()));

    client->print("POST ");
    client->print(endpoint.path.c_str());
    client->println(" HTTP/1.1");
    client->print("Host: ");
    client->println(host);
    client->println("Content-Type: application/json");
    client->println("Connection: close");
    client->print("Content-Length: ");
    client->println(contentLength);
    client->println();
    client->print(data->c_str());

    // Consume headers without temporary Arduino String allocations.
    bool headersComplete = false;
    uint32_t headerState = 0;
    unsigned headerBytes = 0;
    const uint32_t responseStartedAt = millis();
    while ((client->connected() || client->available() > 0) && headerBytes < 2048 &&
           static_cast<uint32_t>(millis() - responseStartedAt) < APLO_RPC_RESPONSE_TIMEOUT_MS) {
        rpcIdle();
        if (client->available() <= 0) {
            delay(1);
            continue;
        }

        const int next = client->read();
        if (next < 0) {
            delay(1);
            continue;
        }

        const char current = static_cast<char>(next);
        ++headerBytes;
        headerState = ((headerState << 8) | static_cast<uint8_t>(current)) & 0xffffffffUL;
        if (headerState == 0x0d0a0d0aUL) {
            headersComplete = true;
            break;
        }
    }

    if (!headersComplete) {
        APLO_RPC_LOG("Invalid or oversized HTTP response headers");
#if defined(ESP8266)
        endpoint.retryAfterMs = millis() + APLO_RPC_FAILURE_COOLDOWN_MS;
#endif
        client->stop();
        client = nullptr;
        return "";
    }

    uint32_t lastBodyByteAt = millis();
    bool bodyTimedOut = false;
    bool bodyTooLarge = false;
    while (client->connected() || client->available() > 0) {
        rpcIdle();
        const uint32_t now = millis();
        if (static_cast<uint32_t>(now - responseStartedAt) >= APLO_RPC_RESPONSE_TIMEOUT_MS ||
            static_cast<uint32_t>(now - lastBodyByteAt) >= APLO_RPC_READ_IDLE_TIMEOUT_MS) {
            bodyTimedOut = true;
            break;
        }
        // Never let a peer keeping available() positive monopolize the loop.
        for (unsigned batch = 0; batch < 64 && client->available() > 0; ++batch) {
            const int next = client->read();
            if (next < 0) {
                break;
            }

            const char c = static_cast<char>(next);
#if defined(ESP8266)
            response.Push(c);
#else
            if (result.size() >= APLO_RPC_RESPONSE_MAX) {
                bodyTooLarge = true;
                break;
            }
            result += c;
#endif
            lastBodyByteAt = millis();
        }
        if (bodyTooLarge) {
            break;
        }

        delay(1);
    }
    client->stop();
    client = nullptr;

    if (bodyTimedOut || bodyTooLarge) {
        APLO_RPC_LOG("HTTP response timed out or exceeds buffer");
#if defined(ESP8266)
        endpoint.retryAfterMs = millis() + APLO_RPC_FAILURE_COOLDOWN_MS;
#endif
        return "";
    }

#if defined(ESP8266)
    if (response.Overflow() && !receiptStatusRequest) {
        APLO_RPC_LOG("HTTP response exceeds ESP8266 buffer");
        return "";
    }

    endpoint.retryAfterMs = 0;
    return response.Finish();
#else
    return result;
#endif
}

static bool normalizeRpcHex(string* value) {
    if (value == nullptr || value->empty()) {
        return false;
    }

    if (value->length() >= 2 && value->at(0) == '0' &&
        (value->at(1) == 'x' || value->at(1) == 'X')) {
        *value = value->substr(2);
    }

    if (value->empty()) {
        return false;
    }

    for (size_t i = 0; i < value->length(); ++i) {
        if (!isxdigit(static_cast<unsigned char>(value->at(i)))) {
            return false;
        }
    }
    return true;
}

int Web3::getInt(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (!normalizeRpcHex(&parseVal)) {
        return 0;
    }

    return strtol(parseVal.c_str(), nullptr, 16);
}

long Web3::getLong(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (!normalizeRpcHex(&parseVal)) {
        return 0;
    }

    return strtol(parseVal.c_str(), nullptr, 16);
}

long long int Web3::getLongLong(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (!normalizeRpcHex(&parseVal)) {
        return 0;
    }

    return strtoll(parseVal.c_str(), nullptr, 16);
}

uint256_t Web3::getUint256(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (!normalizeRpcHex(&parseVal)) {
        return uint256_t(0);
    }

    parseVal = "0x" + parseVal;
    return uint256_t(parseVal.c_str());
}

double Web3::getDouble(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (parseVal.empty()) {
        return 0.0;
    }

    return strtof(parseVal.c_str(), nullptr);
}

bool Web3::getBool(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (parseVal == "true") {
        return true;
    }

    if (parseVal == "false") {
        return false;
    }

    if (!normalizeRpcHex(&parseVal)) {
        return false;
    }

    long v = strtol(parseVal.c_str(), nullptr, 16);
    return v > 0;
}

string Web3::getResult(const string* json) {
    string res = getJsonResultValue(json);
    if (res.length() == 0) {
        return string("");
    }

    if (res.length() > 0 && res.at(0) == 'x') {
        res = res.substr(1);
    } else if (res.length() > 1 && res.at(1) == 'x') {
        res = res.substr(2);
    }
    return res;
}

// Currently only works for string return eg: function name() returns (string)
string Web3::getString(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (parseVal.length() == 0) {
        return string("");
    }

    vector<string>* v = Util::ConvertStringHexToABIArray(&parseVal);
    if (v == nullptr || v->size() < 2) {
        delete v;
        return string("");
    }

    uint256_t length = uint256_t(v->at(1));
    uint32_t lengthIndex = length;

    string asciiHex;
    int index = 2;
    while (lengthIndex > 0 && index < static_cast<int>(v->size())) {
        asciiHex += v->at(index++);
        if (lengthIndex <= 32) {
            lengthIndex = 0;
        } else {
            lengthIndex -= 32;
        }
    }

    // convert ascii into string
    uint32_t stringHexLength = static_cast<uint32_t>(length) * 2;
    string text =
        Util::ConvertHexToASCII(asciiHex.substr(0, stringHexLength).c_str(), stringHexLength);
    delete v;

    return text;
}

// Select trust before connecting. Never use a server response to choose anchors.
// The ESP8266 singleton is synchronous: stop it before replacing its one X509List.
bool Web3::setupCert(const RpcEndpoint& endpoint) {
    lastTlsError = TlsError::None;
    if (certMode == CERT_BUNDLE) {
#if defined(ESP32) && APLO_TLS_ENABLE_BUNDLE
        if (certBundle == nullptr) {
            lastTlsError = TlsError::MissingTrust;
        } else {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
            if (certBundleSize == 0) {
                lastTlsError = TlsError::InvalidTrust;
            } else {
                client->setCACertBundle(certBundle, certBundleSize);
                return true;
            }
#else
            client->setCACertBundle(certBundle);
            return true;
#endif
        }
#else
        lastTlsError = TlsError::UnsupportedTrust;
#endif
    } else if (certMode == CERT_UNAVAILABLE) {
        lastTlsError = TlsError::UnsupportedTrust;
    } else {
        const char* pem = nullptr;
        if (certMode == CERT_CA) {
            pem = &endpoint == &primaryEndpoint ? caCert : fallbackCaCert;
        } else {
#ifdef APLO_TRUST_PROFILE_HEADER
            pem = aploProfileCertificate(endpoint.host.c_str(), endpoint.port);
#endif
        }
        if (pem == nullptr) {
            lastTlsError = TlsError::MissingTrust;
        } else {
#if defined(ESP8266)
            if (esp8266TrustAnchor == nullptr || esp8266TrustAnchorCert != pem) {
                clearEsp8266Trust();
                if (!isValidCertificate(pem)) {
                    lastTlsError = TlsError::InvalidTrust;
                    APLO_RPC_LOG("Invalid TLS certificate material");
                    return false;
                }
                esp8266TrustAnchor = new (std::nothrow) BearSSL::X509List(pem);
                esp8266TrustAnchorCert = pem;
            }
            if (esp8266TrustAnchor == nullptr || esp8266TrustAnchor->getCount() == 0) {
                lastTlsError = TlsError::InvalidTrust;
            } else {
                client->setTrustAnchors(esp8266TrustAnchor);
                return true;
            }
#else
            if (!isValidCertificate(pem)) {
                lastTlsError = TlsError::InvalidTrust;
            } else {
                client->setCACert(pem);
                return true;
            }
#endif
        }
    }
    APLO_RPC_LOG("TLS trust unavailable (profile missing, invalid or unsupported)");
    return false;
}

static bool parsePort(const string& text, unsigned short* port) {
    if (!port || text.empty() || text.find_first_not_of("0123456789") != string::npos) {
        return false;
    }

    char* end = nullptr;
    const unsigned long portValue = strtoul(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || portValue == 0 || portValue > 65535UL) {
        return false;
    }

    *port = static_cast<unsigned short>(portValue);
    return true;
}

bool Web3::isHttpsUrl(const char* url) {
    RpcEndpoint endpoint;
    return parseEndpoint(url, &endpoint, true);
}

bool Web3::parseEndpoint(const char* url, RpcEndpoint* endpoint, bool requireHttps) {
    if (endpoint == nullptr) {
        return false;
    }
    *endpoint = RpcEndpoint();
    std::string node = (url != nullptr) ? url : "";

    if (node.empty()) {
        return false;
    }

    // URI schemes and DNS hostnames are ASCII case insensitive. Paths are not.
    const size_t schemeEnd = node.find("://");
    if (schemeEnd != string::npos && schemeEnd <= 5) {
        for (size_t i = 0; i < schemeEnd; ++i) {
            if (node[i] >= 'A' && node[i] <= 'Z') {
                node[i] += 'a' - 'A';
            }
        }
    }
    if (node.rfind("https://", 0) == 0) {
        node = node.substr(8);
        endpoint->port = 443;
    } else if (node.rfind("http://", 0) == 0) {
        APLO_RPC_LOG("HTTP RPC endpoints are not supported; use HTTPS");
        return false;
    } else if (requireHttps) {
        return false;
    } else {
        endpoint->port = 443;
    }

    const size_t pathPos = node.find('/');
    string authority = (pathPos == string::npos) ? node : node.substr(0, pathPos);
    endpoint->path = (pathPos == string::npos) ? "/" : node.substr(pathPos);

    const size_t colonPos = authority.rfind(':');
    if (colonPos != string::npos) {
        const string portText = authority.substr(colonPos + 1);
        if (!parsePort(portText, &endpoint->port)) {
            return false;
        }

        authority = authority.substr(0, colonPos);
    }

    constexpr const char* hostnameCharacters =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-.";
    if (authority.empty() || authority.find_first_not_of(hostnameCharacters) != string::npos) {
        return false;
    }

    // HTTP origin-form target: URI ASCII only, with well-formed escapes.
    // Never let runtime configuration inject a request line or rely on a
    // server-specific interpretation of backslashes/other non-URI bytes.
    const char* targetCharacters =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~!$&'()*+,;=:@/?";
    for (size_t i = 0; i < endpoint->path.size(); ++i) {
        const unsigned char c = endpoint->path[i];
        if (c == '%') {
            if (i + 2 >= endpoint->path.size() ||
                !isxdigit(static_cast<unsigned char>(endpoint->path[i + 1])) ||
                !isxdigit(static_cast<unsigned char>(endpoint->path[i + 2]))) {
                return false;
            }
            i += 2;
        } else if (!strchr(targetCharacters, c)) {
            return false;
        }
    }
    endpoint->host = authority;
    for (char& c : endpoint->host) {
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        }
    }
    endpoint->valid = !endpoint->host.empty();
    return endpoint->valid;
}

long long Web3::getChainId() const {
    return chainId;
}

// -------------------------------
// RPC Failover Support
// -------------------------------

string Web3::execWithFailover(const string* data) {
    lastEndpointUsed = RpcEndpointUsed::None;
    string result = exec(data, primaryEndpoint);

    if (!result.empty()) {
        lastEndpointUsed = RpcEndpointUsed::Primary;
        return result;
    }

    if (fallbackEndpoint.valid) {
        APLO_RPC_LOG("Primary RPC failed, trying fallback...");
        result = exec(data, fallbackEndpoint);

        if (!result.empty()) {
            lastEndpointUsed = RpcEndpointUsed::Fallback;
            APLO_RPC_LOG("Fallback RPC succeeded");
            return result;
        }
    }

    APLO_RPC_LOG("All RPC endpoints failed");
    return "";
}

// -------------------------------
// AploCoin Balance Helpers
// -------------------------------

uint256_t Web3::AploGetAploBalance(const string* address) {
    // APLO lives behind the built-in APLO contract at 0x...1235.
    // balanceOf(address) reads StateDB.GetBalance directly in AploNode.
    string aploContractAddr = APLO_STAKING_CONTRACT;
    Contract contract(this, aploContractAddr.c_str());
    string functionData = contract.SetupContractData("balanceOf(address)", address);
    string result = contract.ViewCall(&functionData);
    return getUint256(&result);
}

string Web3::AploGetAploBalanceString(const string* address) {
    uint256_t balanceWei = AploGetAploBalance(address);
    return Util::ConvertWeiToEthString(&balanceWei, 18);
}

uint256_t Web3::AploGetGasBalance(const string* address) {
    // AploNode's eth_getBalance is intentionally patched to return the Gaplo
    // gas-token balance from the GAplo contract, not the APLO balance.
    return EthGetBalance(address);
}

string Web3::AploGetGasBalanceString(const string* address) {
    uint256_t balanceWei = AploGetGasBalance(address);
    return Util::ConvertWeiToEthString(&balanceWei, 18);
}

uint256_t Web3::AploGetBalance(const string* address) {
    return AploGetAploBalance(address);
}

string Web3::AploGetBalanceInAplo(const string* address) {
    return AploGetAploBalanceString(address);
}

// -------------------------------
// AploCoin Staking Operations
// -------------------------------

string Web3::AploStake(
    const string* stakingContract,
    const uint256_t* amount,
    const char* privateKey,
    const string* fromAddress
) {
    // Create contract instance
    Contract contract(this, stakingContract->c_str());
    contract.SetPrivateKey(privateKey);

    // Build function call data: stake(uint256)
    string functionData = contract.SetupContractData("stake(uint256)", amount);

    // Get current nonce
    uint32_t nonce = (uint32_t)EthGetTransactionCount(fromAddress);

    // Get current gas price
    unsigned long long gasPrice = (unsigned long long)EthGasPrice();

    // Gas limit for staking (30k actual + buffer)
    uint32_t gasLimit = 100000;

    // The Aplo staking contract follows the web miner ABI: stake(uint256) is
    // nonpayable, so the amount is an ABI argument, not msg.value.
    uint256_t zeroValue = 0;
    string contractAddr = *stakingContract;
    string txHash =
        contract
            .SendTransaction(nonce, gasPrice, gasLimit, &contractAddr, &zeroValue, &functionData);

    return txHash;
}

string Web3::AploUnstake(
    const string* stakingContract,
    const char* privateKey,
    const string* fromAddress
) {
    // Create contract instance
    Contract contract(this, stakingContract->c_str());
    contract.SetPrivateKey(privateKey);

    // Build function call data: unstake()
    string functionData = contract.SetupContractData("unstake()");

    // Get current nonce
    uint32_t nonce = (uint32_t)EthGetTransactionCount(fromAddress);

    // Get current gas price
    unsigned long long gasPrice = (unsigned long long)EthGasPrice();

    // Gas limit for unstaking (30k actual + buffer)
    uint32_t gasLimit = 100000;

    // Send transaction with zero value (unstake returns APLO to sender)
    uint256_t zeroValue = 0;
    string contractAddr = *stakingContract;
    string txHash =
        contract
            .SendTransaction(nonce, gasPrice, gasLimit, &contractAddr, &zeroValue, &functionData);

    return txHash;
}

uint256_t Web3::AploGetStake(const string* stakingContract, const string* account) {
    Contract contract(this, stakingContract->c_str());
    string functionData = contract.SetupContractData("getStake(address)", account);
    string result = contract.ViewCall(&functionData);
    return getUint256(&result);
}

uint256_t Web3::AploGetStakeMultiplier(const string* stakingContract, const string* account) {
    Contract contract(this, stakingContract->c_str());
    string functionData = contract.SetupContractData("getMultiplier(address)", account);
    string result = contract.ViewCall(&functionData);
    return getUint256(&result);
}

#endif // APLO_ENABLE_HTTPS

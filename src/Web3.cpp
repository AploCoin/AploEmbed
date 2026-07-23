// AploEmbed main implementation
//
// Specialized for AploCoin blockchain
// Based on Web3E by James Brown (@JamesSmartCell, @AlphaWallet)
// Original Web3 Arduino by Okada, Takahiro
//

#include "Web3.h"

#include "Contract.h"
#include "Util.h"
#include "TagReader/TagReader.h"
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <ctime>
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
static WiFiClientSecure& getEsp8266Client()
{
    static WiFiClientSecure transport;
    return transport;
}

static bool esp8266DeadlineReached(unsigned long deadline)
{
    return deadline == 0 || static_cast<long>(millis() - deadline) >= 0;
}

static bool ensureEsp8266X509Time()
{
    static bool configured = false;
    static unsigned long retryAfterMs = 0;

    if (time(nullptr) >= APLO_VALID_X509_EPOCH) return true;
    if (!esp8266DeadlineReached(retryAfterMs)) return false;

    if (!configured) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        configured = true;
    }

    const unsigned long startedAt = millis();
    while (time(nullptr) < APLO_VALID_X509_EPOCH &&
           millis() - startedAt < APLO_X509_TIME_WAIT_MS) {
        delay(250);
        yield();
    }

    if (time(nullptr) >= APLO_VALID_X509_EPOCH) return true;
    retryAfterMs = millis() + APLO_X509_TIME_RETRY_MS;
    Serial.println(F("TLS paused: unable to synchronize certificate time"));
    return false;
}
#endif


static string getJsonResultValue(const string* json) {
    if (json == nullptr) return string("");
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
    chainId = APLO_ID;  // Fixed to AploCoin chain ID (28282)

    // Default: validate known public RPC hosts with the bundled root CA.
    certMode = CERT_AUTO_RESOLVE;
    caCert = nullptr;
    certBundle = nullptr;
    certBundleSize = 0;
    resolvedAutoCert = nullptr;
#if defined(ESP8266)
    esp8266TrustAnchor = nullptr;
    esp8266TrustAnchorCert = nullptr;
#endif

    parseEndpoint(primaryRpc, &primaryEndpoint);
    parseEndpoint(fallbackRpc, &fallbackEndpoint);
}

// Default constructor: uses default Aplo RPC endpoints
Web3::Web3() {
    initWeb3(getAploPrimaryNode(), getAploFallbackNode());
}

// Custom RPC constructor
Web3::Web3(const char* primaryRpcUrl) {
    initWeb3(primaryRpcUrl, getAploFallbackNode());
}

// Custom RPC with fallback
Web3::Web3(const char* primaryRpcUrl, const char* fallbackRpcUrl) {
    initWeb3(primaryRpcUrl, fallbackRpcUrl);
}

// Legacy constructor for backward compatibility
Web3::Web3(long long networkId) {
    // Ignore networkId parameter, always use APLO_ID
    if (networkId != APLO_ID) {
        Serial.print("Warning: AploEmbed only supports AploCoin (chainId ");
        Serial.print(APLO_ID);
        Serial.print("), ignoring provided chainId ");
        Serial.println(networkId);
    }
    initWeb3(getAploPrimaryNode(), getAploFallbackNode());
}

Web3::~Web3() {
#if defined(ESP8266)
    delete esp8266TrustAnchor;
    esp8266TrustAnchor = nullptr;
    esp8266TrustAnchorCert = nullptr;
#endif
}

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
    certMode = CERT_CA;
    caCert = root_ca;
}

void Web3::setAutoCertificate() {
    certMode = CERT_AUTO_RESOLVE;
}

void Web3::setInsecure() {
    certMode = CERT_INSECURE;
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
    (void)array;  // Suppress unused parameter warning
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

string Web3::EthViewCall(const string* data, const char* to)
{
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
    string p = "[\"" + *address + "\",\"pending\"]"; //in case we need to push several transactions in a row
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    return getInt(&output);
}

static size_t findJsonObjectEnd(const string& json, size_t objectStart) {
    if (objectStart >= json.size() || json[objectStart] != '{') return string::npos;

    unsigned depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = objectStart; i < json.size(); ++i) {
        const char c = json[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '{') ++depth;
        else if (c == '}' && --depth == 0) return i;
    }
    return string::npos;
}

static size_t findDirectJsonObjectKey(const string& json, size_t objectStart, size_t objectEnd, const char* key) {
    if (key == nullptr || objectStart >= objectEnd || objectEnd >= json.size()) return string::npos;

    unsigned depth = 1;
    for (size_t i = objectStart + 1; i < objectEnd;) {
        const char c = json[i];
        if (c == '{' || c == '[') {
            ++depth;
            ++i;
            continue;
        }
        if (c == '}' || c == ']') {
            if (depth > 0) --depth;
            ++i;
            continue;
        }
        if (c != '"') {
            ++i;
            continue;
        }

        const size_t stringStart = i + 1;
        bool escaped = false;
        ++i;
        while (i < objectEnd) {
            if (escaped) escaped = false;
            else if (json[i] == '\\') escaped = true;
            else if (json[i] == '"') break;
            ++i;
        }
        if (i >= objectEnd) return string::npos;

        if (depth == 1 && json.compare(stringStart, i - stringStart, key) == 0) {
            size_t separator = json.find_first_not_of(" \r\n\t", i + 1);
            if (separator < objectEnd && json[separator] == ':') return stringStart - 1;
        }
        ++i;
    }
    return string::npos;
}

int Web3::EthGetTransactionReceiptStatus(const string* transactionHash) {
    if (transactionHash == nullptr || transactionHash->empty()) return -1;

    string normalizedHash = *transactionHash;
    if (normalizedHash.rfind("0x", 0) != 0 && normalizedHash.rfind("0X", 0) != 0) {
        normalizedHash.insert(0, "0x");
    }

    string m = "eth_getTransactionReceipt";
    string p = "[\"" + normalizedHash + "\"]";
    string input = generateJson(&m, &p);
    string output = execWithFailover(&input);
    if (output.empty()) return -1;

    const size_t errorPos = output.find("\"error\"");
    if (errorPos != string::npos) return -1;

    const size_t resultKeyPos = output.find("\"result\"");
    if (resultKeyPos == string::npos) return -1;
    const size_t resultColonPos = output.find(':', resultKeyPos + 8);
    if (resultColonPos == string::npos) return -1;
    const size_t resultObjectPos = output.find_first_not_of(" \r\n\t", resultColonPos + 1);
    if (resultObjectPos == string::npos || output[resultObjectPos] != '{') return -1;

    const size_t resultObjectEnd = findJsonObjectEnd(output, resultObjectPos);
    if (resultObjectEnd == string::npos) return -1;

    const size_t statusPos = findDirectJsonObjectKey(output, resultObjectPos, resultObjectEnd, "status");
    if (statusPos == string::npos || statusPos > resultObjectEnd) return -1;
    const size_t statusValuePos = output.find(':', statusPos + 8);
    if (statusValuePos == string::npos || statusValuePos > resultObjectEnd) return -1;
    const size_t statusQuotePos = output.find_first_not_of(" \r\n\t", statusValuePos + 1);
    if (statusQuotePos == string::npos || statusQuotePos > resultObjectEnd || output[statusQuotePos] != '"') return -1;
    const size_t statusEndPos = output.find('"', statusQuotePos + 1);
    if (statusEndPos == string::npos || statusEndPos > resultObjectEnd) return -1;

    const string status = output.substr(statusQuotePos + 1, statusEndPos - statusQuotePos - 1);
    if (status == "0x1" || status == "1") return 1;
    if (status == "0x0" || status == "0") return 0;
    return -1;
}

string Web3::EthCall(const string* from, const char* to, long gas, long gasPrice,
                     const string* value, const string* data) {
    // Build eth_call with optional gas, gasPrice, and value parameters
    string m = "eth_call";
    string p = "[{\"from\":\"" + *from + "\",\"to\":\""
               + *to + "\",\"data\":\"" + *data + "\"";
    
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
    if (data == nullptr || data->empty()) return "";
    string input;
    input.reserve(data->size() + 80);
    input = "{\"jsonrpc\":\"2.0\",\"method\":\"eth_sendRawTransaction\",\"params\":[\"";
    input += *data;
    input += "\"],\"id\":0}";
#if 0
    LOG(input);
#endif
    string output = execWithFailover(&input);
    return getResult(&output);
}

// -------------------------------
// Private

string Web3::generateJson(const string* method, const string* params) {
    return "{\"jsonrpc\":\"2.0\",\"method\":\"" + *method + "\",\"params\":" + *params + ",\"id\":0}";
}

string Web3::exec(const string* data, RpcEndpoint& endpoint) {
    if (data == nullptr || !endpoint.valid) {
        return "";
    }

#if defined(ESP8266)
    if (!esp8266DeadlineReached(endpoint.retryAfterMs)) return "";
    if (certMode != CERT_INSECURE && !ensureEsp8266X509Time()) {
        endpoint.retryAfterMs = millis() + APLO_RPC_FAILURE_COOLDOWN_MS;
        return "";
    }
#endif

    host = endpoint.host.c_str();

#if defined(ESP8266)
    string result;
    result.reserve(APLO_ESP8266_RPC_RESPONSE_MAX);
    // Receipt payloads include logs and can exceed the general response buffer.
    // Track only the status token while streaming so confirmation never needs
    // the complete multi-kilobyte receipt in heap.
    bool receiptStatusRequest = data->find("\"method\":\"eth_getTransactionReceipt\"") != string::npos;
    const char *receiptSuccessToken = "\"status\":\"0x1\"";
    const char *receiptRevertToken = "\"status\":\"0x0\"";
    size_t receiptSuccessMatch = 0;
    size_t receiptRevertMatch = 0;
    bool receiptSuccess = false;
    bool receiptReverted = false;
    // The standalone probe omits SNI. SNI-routed RPC endpoints can reject it
    // even when the normal BearSSL handshake supports MFLN. Request 1024 and
    // validate the negotiated result after the real hostname-aware connection.
    if (endpoint.tlsBufferSize == 0) endpoint.tlsBufferSize = 1024;
    bool responseOverflow = false;
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
#endif
    setupCert();
#if defined(ESP8266)
    const time_t now = time(nullptr);
    client->setX509Time(now);
#endif

    if (!client->connect(host, endpoint.port)) {
        Serial.print("Unable to connect to Host: ");
        Serial.println(host);
#if defined(ESP8266)
        char sslError[128];
        const int sslErrorCode = client->getLastSSLError(sslError, sizeof(sslError));
        Serial.print(F("ESP8266 TLS error "));
        Serial.print(sslErrorCode);
        Serial.print(F(": "));
        Serial.println(sslError);
        endpoint.retryAfterMs = millis() + APLO_RPC_FAILURE_COOLDOWN_MS;
#endif
        client->stop();
        delay(100);
        client = nullptr;
        return "";
    }
#if defined(ESP8266)
    if (!client->getMFLNStatus()) {
        Serial.print("RPC host did not negotiate ESP8266 TLS MFLN: ");
        Serial.println(host);
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
    unsigned long headerDeadline = millis() + 10000;
    while ((client->connected() || client->available()) &&
           headerBytes < 2048 && millis() < headerDeadline) {
        if (!client->available()) {
            delay(1);
            continue;
        }
        const char current = static_cast<char>(client->read());
        ++headerBytes;
        headerState = ((headerState << 8) | static_cast<uint8_t>(current)) & 0xffffffffUL;
        if (headerState == 0x0d0a0d0aUL) {
            headersComplete = true;
            break;
        }
    }

    if (!headersComplete) {
        Serial.println("Invalid or oversized HTTP response headers");
#if defined(ESP8266)
        endpoint.retryAfterMs = millis() + APLO_RPC_FAILURE_COOLDOWN_MS;
#endif
        client->stop();
        client = nullptr;
        return "";
    }

    unsigned long deadline = millis() + 10000;
    while (client->connected() || client->available()) {
        while (client->available()) {
            const char c = static_cast<char>(client->read());
#if defined(ESP8266)
            if (receiptStatusRequest) {
                if (!receiptSuccess && c == receiptSuccessToken[receiptSuccessMatch]) {
                    if (receiptSuccessToken[++receiptSuccessMatch] == '\0') receiptSuccess = true;
                } else if (!receiptSuccess) {
                    receiptSuccessMatch = (c == receiptSuccessToken[0]) ? 1 : 0;
                }
                if (!receiptReverted && c == receiptRevertToken[receiptRevertMatch]) {
                    if (receiptRevertToken[++receiptRevertMatch] == '\0') receiptReverted = true;
                } else if (!receiptReverted) {
                    receiptRevertMatch = (c == receiptRevertToken[0]) ? 1 : 0;
                }
            }
            if (result.size() < APLO_ESP8266_RPC_RESPONSE_MAX) {
                result += c;
            } else {
                responseOverflow = true;
            }
#else
            result += c;
#endif
            deadline = millis() + 1000;
        }
        if (millis() > deadline) {
            break;
        }
        delay(1);
    }
    client->stop();
    client = nullptr;

#if defined(ESP8266)
    if (responseOverflow) {
        if (receiptStatusRequest && receiptSuccess) {
            endpoint.retryAfterMs = 0;
            return "{\"result\":{\"status\":\"0x1\"}}";
        }
        if (receiptStatusRequest && receiptReverted) {
            endpoint.retryAfterMs = 0;
            return "{\"result\":{\"status\":\"0x0\"}}";
        }
        Serial.println("HTTP response exceeds ESP8266 buffer");
        return "";
    }
    endpoint.retryAfterMs = 0;
#endif
    return result;
}

static bool normalizeRpcHex(string *value)
{
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
    if (!normalizeRpcHex(&parseVal)) return 0;
    return strtol(parseVal.c_str(), nullptr, 16);
}

long Web3::getLong(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (!normalizeRpcHex(&parseVal)) return 0;
    return strtol(parseVal.c_str(), nullptr, 16);
}

long long int Web3::getLongLong(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (!normalizeRpcHex(&parseVal)) return 0;
    return strtoll(parseVal.c_str(), nullptr, 16);
}

uint256_t Web3::getUint256(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (!normalizeRpcHex(&parseVal)) return uint256_t(0);
    parseVal = "0x" + parseVal;
    return uint256_t(parseVal.c_str());
}

double Web3::getDouble(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (parseVal.empty()) return 0.0;
    return strtof(parseVal.c_str(), nullptr);
}

bool Web3::getBool(const string* json) {
    string parseVal = getJsonResultValue(json);
    if (parseVal == "true") return true;
    if (parseVal == "false") return false;
    if (!normalizeRpcHex(&parseVal)) return false;
    long v = strtol(parseVal.c_str(), nullptr, 16);
    return v > 0;
}

string Web3::getResult(const string* json) {
    string res = getJsonResultValue(json);
    if (res.length() == 0)
    {
        return string("");
    }

    if (res.length() > 0 && res.at(0) == 'x') res = res.substr(1);
    else if (res.length() > 1 && res.at(1) == 'x') res = res.substr(2);
    return res;
}

//Currently only works for string return eg: function name() returns (string)
string Web3::getString(const string *json)
{
    string parseVal = getJsonResultValue(json);
    if (parseVal.length() == 0)
    {
        return string("");
    }

    vector<string> *v = Util::ConvertStringHexToABIArray(&parseVal);
    if (v == nullptr || v->size() < 2) {
        delete v;
        return string("");
    }
    
    uint256_t length = uint256_t(v->at(1));
    uint32_t lengthIndex = length;

    string asciiHex;
    int index = 2;
    while (lengthIndex > 0 && index < static_cast<int>(v->size()))
    {
        asciiHex += v->at(index++);
        if (lengthIndex <= 32) {
            lengthIndex = 0;
        } else {
            lengthIndex -= 32;
        }
    }

    //convert ascii into string
    uint32_t stringHexLength = static_cast<uint32_t>(length) * 2;
    string text = Util::ConvertHexToASCII(asciiHex.substr(0, stringHexLength).c_str(), stringHexLength);
    delete v;

    return text;
}

/**
 * Configure TLS certificate validation for HTTPS connections.
 * 
 * Four modes are supported:
 * 0. CERT_AUTO_RESOLVE: Use the bundled root CA selected for the target host.
 *    - Defaults to ISRG Root X1, which validates Let's Encrypt endpoints.
 *    - This is the default for HTTPS RPC endpoints.
 *
 * 1. CERT_AUTO: Do not override WiFiClientSecure certificate handling.
 *    - Uses the TLS behavior configured by the active Arduino/ESP32 core.
 *
 * 2. CERT_BUNDLE: Use ESP32 CA certificate bundle (recommended for production)
 *    - Validates against Mozilla's root certificate set
 *    - Requires board_build.embed_files in platformio.ini
 *    - See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_crt_bundle.html
 * 
 * 3. CERT_CA: Use specific CA certificate (PEM format)
 *    - Validates against a single root CA certificate
 *    - Useful for pinning to a specific CA you control
 * 
 * 4. CERT_INSECURE: Explicitly disable certificate validation
 *    - Connection is encrypted but server identity is NOT verified
 *    - Vulnerable to man-in-the-middle attacks
 *    - Use only for testing or when certificate validation is not feasible
 * 
 * Call setCertificateBundle() or setCertificate() BEFORE making RPC calls if your platform
 * does not provide a usable default trust store.
 */
void Web3::setupCert()
{
    switch (certMode) {
        case CERT_BUNDLE:
            // ESP32 supports certificate bundles. ESP8266 BearSSL does not,
            // so keep its default TLS behavior and warn instead of calling an
            // ESP32-only API.
            if (certBundle != nullptr) {
#if defined(ESP8266)
                Serial.println("Warning: ESP8266 does not support setCACertBundle(); using WiFiClientSecure defaults");
                (void)certBundle;
                (void)certBundleSize;
#elif defined(ESP32)
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
                if (certBundleSize > 0) {
                    client->setCACertBundle(certBundle, certBundleSize);
                } else {
                    Serial.println("Warning: ESP32 Arduino core 3.x requires CA bundle size; using WiFiClientSecure defaults");
                }
  #else
                (void)certBundleSize;
                client->setCACertBundle(certBundle);
  #endif
#else
                (void)certBundle;
                (void)certBundleSize;
                Serial.println("Warning: CA bundle mode is not supported on this platform; using WiFiClientSecure defaults");
#endif
            } else {
                Serial.println("Warning: CERT_BUNDLE mode but bundle is null; using WiFiClientSecure defaults");
            }
            break;
            
        case CERT_CA:
            // Use specific CA certificate. ESP32 accepts PEM directly;
            // ESP8266 BearSSL needs an X509List trust anchor that must stay
            // alive for the TLS connection duration.
            if (caCert != nullptr) {
#if defined(ESP8266)
                if (esp8266TrustAnchor == nullptr || esp8266TrustAnchorCert != caCert) {
                    delete esp8266TrustAnchor;
                    esp8266TrustAnchor = new BearSSL::X509List(caCert);
                    esp8266TrustAnchorCert = caCert;
                }
                client->setTrustAnchors(esp8266TrustAnchor);
#elif defined(ESP32)
                client->setCACert(caCert);
#else
                client->setCACert(caCert);
#endif
            } else {
                Serial.println("Warning: CERT_CA mode but certificate is null; using WiFiClientSecure defaults");
            }
            break;

        case CERT_AUTO_RESOLVE:
            resolvedAutoCert = getAploRootCAForHost(host);
            if (resolvedAutoCert != nullptr) {
#if defined(ESP8266)
                if (esp8266TrustAnchor == nullptr || esp8266TrustAnchorCert != resolvedAutoCert) {
                    delete esp8266TrustAnchor;
                    esp8266TrustAnchor = new BearSSL::X509List(resolvedAutoCert);
                    esp8266TrustAnchorCert = resolvedAutoCert;
                }
                client->setTrustAnchors(esp8266TrustAnchor);
#elif defined(ESP32)
                client->setCACert(resolvedAutoCert);
#else
                client->setCACert(resolvedAutoCert);
#endif
            } else {
                Serial.println("Warning: no bundled CA for RPC host; using WiFiClientSecure defaults");
            }
            break;

        case CERT_AUTO:
            // Do not call setInsecure() and do not hardcode certificate material.
            // The active WiFiClientSecure implementation handles TLS with its configured trust store.
            break;
            
        case CERT_INSECURE:
        default:
            // Explicit testing/development mode only.
            client->setInsecure();
            break;
    }
}

static bool parsePort(const string& text, unsigned short *port)
{
    if (port == nullptr || text.empty()) return false;

    char *end = nullptr;
    const unsigned long portValue = strtoul(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || portValue == 0 || portValue > 65535UL) {
        return false;
    }

    *port = static_cast<unsigned short>(portValue);
    return true;
}

bool Web3::parseEndpoint(const char* url, RpcEndpoint* endpoint)
{
    if (endpoint == nullptr) return false;
    *endpoint = RpcEndpoint();
    std::string node = (url != nullptr) ? url : "";

    if (node.empty()) {
        return false;
    }

    if (node.rfind("https://", 0) == 0) {
        node = node.substr(8);
        endpoint->port = 443;
    } else if (node.rfind("http://", 0) == 0) {
        Serial.println("HTTP RPC endpoints are not supported; use HTTPS");
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

    endpoint->host = authority;
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
    string result = exec(data, primaryEndpoint);

    if (!result.empty() && result.find("\"result\"") != string::npos) {
        return result;
    }

    if (fallbackEndpoint.valid) {
        Serial.println("Primary RPC failed, trying fallback...");
        result = exec(data, fallbackEndpoint);

        if (!result.empty() && result.find("\"result\"") != string::npos) {
            Serial.println("Fallback RPC succeeded");
            return result;
        }
    }

    Serial.println("All RPC endpoints failed");
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

string Web3::AploStake(const string* stakingContract, const uint256_t* amount, const char* privateKey, const string* fromAddress) {
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
    string txHash = contract.SendTransaction(
        nonce,
        gasPrice,
        gasLimit,
        &contractAddr,
        &zeroValue,
        &functionData
    );
    
    return txHash;
}

string Web3::AploUnstake(const string* stakingContract, const char* privateKey, const string* fromAddress) {
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
    string txHash = contract.SendTransaction(
        nonce,
        gasPrice,
        gasLimit,
        &contractAddr,
        &zeroValue,
        &functionData
    );
    
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

// -------------------------------
// AploCoin Mining Helpers
// -------------------------------

bool Web3::AploGetMinerParams(const string* miningContract, const string* minerAddress,
                              uint256_t* lastBlock, uint256_t* currentDifficulty,
                              uint256_t* totalMined, uint256_t* prevHash) {
    // Call miner_params(address) view function.
    // Returns: (uint256 last_block, uint256 current_difficulty, uint256 total_mined, uint256 prev_hash)
    Contract contract(this, miningContract->c_str());
    string functionData = contract.SetupContractData("miner_params(address)", minerAddress);
    string result = contract.ViewCall(&functionData);
    
    if (result.length() == 0) {
        return false;
    }
    
    // Parse result - should contain 4 uint256 values (128 hex chars each = 64 bytes)
    string resultData = getResult(&result);
    if (resultData.length() < 256) {  // 4 * 64 hex chars
        return false;
    }
    
    // Extract each uint256 (64 hex chars each)
    *lastBlock = uint256_t(("0x" + resultData.substr(0, 64)).c_str());
    *currentDifficulty = uint256_t(("0x" + resultData.substr(64, 64)).c_str());
    *totalMined = uint256_t(("0x" + resultData.substr(128, 64)).c_str());
    *prevHash = uint256_t(("0x" + resultData.substr(192, 64)).c_str());
    
    return true;
}

string Web3::AploMine(const string* miningContract, const string* nonce, const char* privateKey, const string* fromAddress) {
    // Validate nonce format (must be 32-byte hex string)
    string nonceStr = *nonce;
    // Remove 0x prefix if present
    if (nonceStr.length() >= 2 && nonceStr.substr(0, 2) == "0x") {
        nonceStr = nonceStr.substr(2);
    }
    
    // Check length (must be 64 hex chars = 32 bytes)
    if (nonceStr.length() != 64) {
        Serial.println("AploMine: nonce must be 32 bytes (64 hex chars)");
        return "";
    }
    
    // Create contract instance
    Contract contract(this, miningContract->c_str());
    contract.SetPrivateKey(privateKey);
    
    // Build function call data: mine(bytes32). The generic ABI encoder now
    // handles fixed bytes types (bytes1..bytes32) as static ABI words.
    string nonceWithPrefix = "0x" + nonceStr;
    string functionData = contract.SetupContractData("mine(bytes32)", &nonceWithPrefix);
    
    // Get current transaction nonce (not mining nonce)
    uint32_t txNonce = (uint32_t)EthGetTransactionCount(fromAddress);
    
    // Get current gas price
    unsigned long long gasPrice = (unsigned long long)EthGasPrice();
    
    // Gas limit for mining (may require more gas than staking)
    uint32_t gasLimit = 200000;
    
    // Send transaction with zero value
    uint256_t zeroValue = 0;
    string contractAddr = *miningContract;
    string txHash = contract.SendTransaction(
        txNonce,
        gasPrice,
        gasLimit,
        &contractAddr,
        &zeroValue,
        &functionData
    );
    
    return txHash;
}

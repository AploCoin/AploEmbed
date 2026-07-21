// AploEmbed main implementation
//
// Specialized for AploCoin blockchain
// Based on Web3E by James Brown (@JamesSmartCell, @AlphaWallet)
// Original Web3 Arduino by Okada, Takahiro
//

#include "Web3.h"

#include "Util.h"
#include "TagReader/TagReader.h"
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include "nodes.h"

// Helper function to initialize Web3 instance
using std::string;
using std::vector;


static string getJsonResultValue(const string* json) {
    if (json == nullptr) return string("");
    TagReader reader;
    return reader.getTag(json, "result");
}
void Web3::initWeb3(const char* primaryRpc, const char* fallbackRpc) {
    mem = new BYTE[sizeof(WiFiClientSecure)];
    chainId = APLO_ID;  // Fixed to AploCoin chain ID (28282)
    primaryRpcUrl = primaryRpc;
    fallbackRpcUrl = fallbackRpc;
    
    // Default: let WiFiClientSecure/HTTP stack handle TLS using its configured trust store.
    // No AploCoin certificates are hardcoded and certificate validation is never disabled implicitly.
    certMode = CERT_AUTO_RESOLVE;
    caCert = nullptr;
    certBundle = nullptr;
    certBundleSize = 0;
    resolvedAutoCert = nullptr;
#if defined(ESP8266)
    esp8266TrustAnchor = nullptr;
    esp8266TlsBufferSize = 0;
#endif
    
    selectHost();
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
    string p = "[{\"data\":\"";;
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
    string m = "eth_sendRawTransaction";
    string p = "[\"" + *data + "\"]";
    string input = generateJson(&m, &p);
#if 0
    LOG(input);
#endif
    return execWithFailover(&input);
}

// -------------------------------
// Private

string Web3::generateJson(const string* method, const string* params) {
    return "{\"jsonrpc\":\"2.0\",\"method\":\"" + *method + "\",\"params\":" + *params + ",\"id\":0}";
}

string Web3::exec(const string* data) {
    string result;

#if defined(ESP8266)
    if (esp8266TlsBufferSize == 0) {
        if (WiFiClientSecure::probeMaxFragmentLength(host, port, 1024)) {
            esp8266TlsBufferSize = 1024;
        } else if (WiFiClientSecure::probeMaxFragmentLength(host, port, 512)) {
            esp8266TlsBufferSize = 512;
        } else {
            esp8266TlsBufferSize = -1;
        }
    }

    if (esp8266TlsBufferSize < 0) {
        Serial.print("RPC host does not support ESP8266 TLS MFLN: ");
        Serial.println(host);
        return "";
    }
#endif

    client = new (mem) WiFiClientSecure();
#if defined(ESP8266)
    // Use negotiated Maximum Fragment Length instead of a blind low-memory
    // buffer. Certificate validation remains enabled; only TLS record size is
    // reduced for ESP8266's small DRAM budget.
    client->setBufferSizes(esp8266TlsBufferSize, esp8266TlsBufferSize);
#endif
    setupCert();

    int connected = client->connect(host, port);
    if (!connected) {
        Serial.print("Unable to connect to Host: ");
        Serial.println(host);
        delay(100);
        // Do not reboot on RPC connection failure. Examples use failover and
        // application-level retries; restarting here can trap boards in a boot
        // loop before WiFi/RPC diagnostics are visible.
        client->~WiFiClientSecure();
        return "";
    }

    // Make a HTTP request without temporary std::string allocations; ESP8266
    // can run with only a few KB of free heap after WiFi/BearSSL are active.
    char contentLength[24];
    snprintf(contentLength, sizeof(contentLength), "%lu", static_cast<unsigned long>(data->size()));

    client->print("POST ");
    client->print(path);
    client->println(" HTTP/1.1");
    client->print("Host: ");
    client->println(host);
    client->println("Content-Type: application/json");
    client->println("Connection: close");
    client->print("Content-Length: ");
    client->println(contentLength);
    client->println();
    // Keep Content-Length exact. println() appends CRLF beyond the declared body
    // length and some JSON-RPC servers wait/parse oddly on embedded clients.
    client->print(data->c_str());

    while (client->connected())
    {
        String line = client->readStringUntil('\n');
        if (line == "\r") {
            break;
        }
        delay(0);
    }

    // Read the complete response body. On ESP32 the body often arrives after
    // headers, so checking available() only once races and returns empty/zero.
    unsigned long deadline = millis() + 10000;
    while (client->connected() || client->available()) {
        while (client->available()) {
            char c = client->read();
            result += c;
            deadline = millis() + 1000;
        }
        if (millis() > deadline) {
            break;
        }
        delay(1);
    }
    client->flush();
    client->stop();

    client->~WiFiClientSecure();

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
        Serial.println(index);
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
                delete esp8266TrustAnchor;
                esp8266TrustAnchor = new BearSSL::X509List(caCert);
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
                delete esp8266TrustAnchor;
                esp8266TrustAnchor = new BearSSL::X509List(resolvedAutoCert);
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

void Web3::selectHost()
{
#if defined(ESP8266)
    esp8266TlsBufferSize = 0;
#endif
    std::string node = primaryRpcUrl;

    if (node.length() == 0)
    {
        Serial.println("Error: No RPC URL configured for AploCoin");
        return;
    }

    if (node.rfind("https://", 0) == 0) {
        node = node.substr(8);
        port = 443;
    } else if (node.rfind("http://", 0) == 0) {
        node = node.substr(7);
        port = 80;
    } else {
        port = 443;  // Default HTTPS port
    }

    int ppos = node.find(":");
    if (ppos > 0)
    {
        port = stoi(node.substr(ppos+1));
        node = node.substr(0, ppos);
    }

    ppos = node.find("/");
    if (ppos > 0)
    {
        host = strdup(node.substr(0, ppos).c_str());
        path = strdup(node.substr(ppos).c_str());
    }
    else
    {
        host = strdup(node.c_str());
        path = "/";
    }
}


long long Web3::getChainId() const {
    return chainId;
}

// -------------------------------
// RPC Failover Support
// -------------------------------

string Web3::execWithFailover(const string* data) {
    // Try primary RPC first
    string result = exec(data);
    
    // Check if result is valid (non-empty and contains "result" field)
    if (result.length() > 0 && result.find("\"result\"") != string::npos) {
        return result;
    }
    
    // Primary failed, try fallback if configured
    if (fallbackRpcUrl != nullptr && strlen(fallbackRpcUrl) > 0) {
        Serial.println("Primary RPC failed, trying fallback...");
        
        // Temporarily switch to fallback
        const char* originalPrimary = primaryRpcUrl;
        primaryRpcUrl = fallbackRpcUrl;
        selectHost();
        
        result = exec(data);
        
        // Restore primary for next call
        primaryRpcUrl = originalPrimary;
        selectHost();
        
        if (result.length() > 0 && result.find("\"result\"") != string::npos) {
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

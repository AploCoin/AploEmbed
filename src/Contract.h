// AploEmbed Contract handling code
//
// Specialized for AploCoin blockchain
// Based on Web3E by James Brown (@JamesSmartCell, @AlphaWallet)
// Original Web3 Arduino by Okada, Takahiro
//

#ifndef APLOEMBED_CONTRACT_H
#define APLOEMBED_CONTRACT_H

#include "Arduino.h"
#include "Web3.h"
#include <vector>
#include <string>
#include <Crypto.h>
#include "uint256/uint256_t.h"

class Contract {

public:
    typedef struct {
        char from[80];
        char to[80];
        char gasPrice[20];
        long gas;
    } Options;
    Options options;

public:
    Contract(Web3* _web3, const char* address);
    explicit Contract(long long int networkId);
    void SetPrivateKey(const char *key);
    std::string SetupContractData(const char* func, ...);
    std::string Call(const std::string* param);
    std::string ViewCall(const std::string *param);
    std::string SendTransaction(uint32_t nonceVal, unsigned long long gasPriceVal, uint32_t gasLimitVal,
                           std::string *toStr, uint256_t *valueStr, std::string *dataStr);
    std::string SignTransaction(uint32_t nonceVal, unsigned long long int gasPriceVal, uint32_t gasLimitVal, std::string *toStr,
                           uint256_t *valueStr, std::string *dataStr);

    static void ReplaceFunction(std::string &param, const char* func);                       
    
private:
    Web3* web3;
    const char * contractAddress;
    Crypto* crypto;

private:
    static std::string GenerateContractBytes(const char *func);
    std::string GenerateBytesForInt(const int32_t value);
    std::string GenerateBytesForUint(const uint256_t *value);
    std::string GenerateBytesForUintPointer(const void *value, size_t byteSize);
    std::string GenerateBytesForAddress(const std::string *value);
    std::string GenerateBytesForString(const std::string *value);
    std::string GenerateBytesForBytes(const char* value, const int len);
    std::string GenerateBytesForUIntArray(const std::vector<uint32_t> *v);
    std::string GenerateBytesForHexBytes(const std::string *value);
    std::string GenerateBytesForFixedBytes(const std::string *value, size_t byteSize);
    std::string GenerateBytesForStruct(const std::string *value);

    void GenerateSignature(uint8_t* signature, int* recid, uint32_t nonceVal, unsigned long long gasPriceVal, uint32_t  gasLimitVal,
                           std::string* toStr, uint256_t* valueStr, std::string* dataStr);
    std::vector<uint8_t> RlpEncode(
            uint32_t nonceVal, unsigned long long gasPriceVal, uint32_t  gasLimitVal,
            std::string* toStr, uint256_t* valueStr, std::string* dataStr);
    std::vector<uint8_t> RlpEncodeForRawTransaction(
            uint32_t nonceVal, unsigned long long gasPriceVal, uint32_t  gasLimitVal,
            std::string* toStr, uint256_t* valueStr, std::string* dataStr, uint8_t* sig, uint8_t recid);
    void Sign(uint8_t* hash, uint8_t* sig, int* recid);
};


#endif //APLOEMBED_CONTRACT_H

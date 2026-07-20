// AploEmbed Utilities
//
// Specialized for AploCoin blockchain
// Based on Web3E by James Brown (@JamesSmartCell, @AlphaWallet)
// Original Web3 Arduino by Okada, Takahiro
//

#ifndef APLOEMBED_UTIL_H
#define APLOEMBED_UTIL_H

#include <stdint.h>
#include <vector>
#include <string>
#include "uint256/uint256_t.h"

class Util {
public:
    // RLP implementation
    // reference:
    //     https://github.com/ethereum/wiki/wiki/%5BEnglish%5D-RLP
    static uint32_t        RlpEncodeWholeHeader(uint8_t *header_output, uint32_t total_len);
    static std::vector<uint8_t> RlpEncodeWholeHeaderWithVector(uint32_t total_len);
    static uint32_t        RlpEncodeItem(uint8_t* output, const uint8_t* input, uint32_t input_len);
    static std::vector<uint8_t> RlpEncodeItemWithVector(const std::vector<uint8_t> input);

    static uint32_t        ConvertNumberToUintArray(uint8_t *str, uint32_t val);
    static std::vector<uint8_t> ConvertNumberToVector(uint32_t val);
    static std::vector<uint8_t> ConvertNumberToVector(unsigned long long val);
    static uint32_t        ConvertCharStrToUintArray(uint8_t *out, const uint8_t *in);
    static std::vector<uint8_t> ConvertHexToVector(const uint8_t *in);
    static std::vector<uint8_t> ConvertHexToVector(const std::string* str);
    static char *          ConvertToString(const uint8_t *in);

    static uint8_t HexToInt(uint8_t s);
    static std::string  VectorToString(const std::vector<uint8_t> *buf);
    static std::string  PlainVectorToString(const std::vector<uint8_t> *buf);
    static std::string  ConvertBytesToHex(const uint8_t *bytes, int length);
    static void    ConvertHexToBytes(uint8_t *_dst, const char *_src, int length);
    static std::string  ConvertBase(int from, int to, const char *s);
    static std::string  ConvertDecimal(int decimal, std::string *s);
    static std::string  ConvertString(const char* value);
    static std::string  ConvertHexToASCII(const char *result, size_t length);
    static std::string  InterpretStringResult(const char *result);
    static std::vector<std::string>* InterpretVectorResult(std::string *result);
    static void PadForward(std::string *target, int targetSize);
    static uint256_t ConvertToWei(double val, int decimals);
    static std::string ConvertWeiToEthString(uint256_t *weiVal, int decimals);
    static std::string intToHex(int value);

    static std::vector<std::string>* ConvertCharStrToVector32(const char *resultPtr, size_t resultSize, std::vector<std::string> *result);
    static std::vector<std::string>* ConvertResultToArray(std::string *result);
    static std::vector<std::string>* ConvertStringHexToABIArray(std::string *value);

    static std::string  ConvertEthToWei(double eth);
    static std::string toString(int value);

    static std::string ConvertIntegerToBytes(const int32_t value);

    // Mining helpers
    static std::string PackMiningData(const std::string* address, const std::string* nonce,
                                  const std::string* difficulty, const std::string* prevHash,
                                  uint32_t totalMined);
    static std::string ComputeKeccak256(const std::string* hexData);
    static bool CompareUint256(const std::string* hash, const std::string* difficulty);

private:
    static uint8_t ConvertCharToByte(const uint8_t* ptr);

};

#endif //APLOEMBED_UTIL_H

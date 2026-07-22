//
// Created by James Brown on 2018/09/13.
//

#ifndef ARDUINO_WEB3_CRYPTO_H
#define ARDUINO_WEB3_CRYPTO_H
#include "Web3.h"
#include <vector>
#include <string>

extern const char * PERSONAL_MESSAGE_PREFIX;

class Crypto {

public:
    Crypto(Web3* _web3);
    bool Sign(BYTE* digest, BYTE* result);
    void SetPrivateKey(const char *key);
    
    static void ECRecover(BYTE* signature, BYTE *public_key, BYTE *message_hash); 
    static bool Verify(const uint8_t *public_key, const uint8_t *message_hash, const uint8_t *signature);
    static void PrivateKeyToPublic(const uint8_t *privateKey, uint8_t *publicKey);
    static void PublicKeyToAddress(const uint8_t *publicKey, uint8_t *address);
    static std::string PrivateKeyToAddress(const char *privateKey);
    static bool RandomBytes(uint8_t *buffer, size_t length);
    static void Keccak256(const uint8_t *data, size_t length, uint8_t *result);
    static std::string ECRecoverFromPersonalMessage(std::string *signature, std::string *message);
    static std::string ECRecoverFromHexMessage(std::string *signature, std::string *hex);
    static std::string ECRecoverFromHash(std::string *signature, BYTE *digest);

    static std::string Keccak256(std::vector<uint8_t> *bytes);


private:
    Web3* web3;
    uint8_t privateKey[ETHERS_PRIVATEKEY_LENGTH];

};


#endif //ARDUINO_WEB3_CRYPTO_H

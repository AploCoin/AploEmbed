#include <KeyID.h>
#include "Util.h"

KeyID::KeyID(Web3* web3, const std::string& privateKey)
  : recoveredKey(false), crypto(web3)
{
  memset(privateKeyBytes, 0, sizeof(privateKeyBytes));
  Util::ConvertHexToBytes(privateKeyBytes, privateKey.c_str(), ETHERS_PRIVATEKEY_LENGTH);
  initPrivateKey(privateKey);
}

KeyID::KeyID(Web3* web3)
  : recoveredKey(false), crypto(web3)
{
  memset(privateKeyBytes, 0, sizeof(privateKeyBytes));

  if (!beginKeyStorage())
  {
    Serial.println("failed to initialise EEPROM");
    return;
  }

  if (static_cast<uint8_t>(EEPROM.read(0)) == 64)
  {
    Serial.println();
    Serial.println("Recovering key from EEPROM");
    for (int i = 0; i < ETHERS_PRIVATEKEY_LENGTH; i++)
    {
      privateKeyBytes[i] = static_cast<uint8_t>(EEPROM.read(i + 1));
    }

    std::string privateKey = Util::ConvertBytesToHex(privateKeyBytes, ETHERS_PRIVATEKEY_LENGTH);
    if (privateKey.length() > 1 && privateKey[1] == 'x') privateKey = privateKey.substr(2);
    initPrivateKey(privateKey);
  }
  else
  {
    generatePrivateKey(web3);
  }
}

bool KeyID::beginKeyStorage()
{
#if defined(ESP8266)
  EEPROM.begin(EEPROM_SIZE + 1);
  return true;
#else
  return EEPROM.begin(EEPROM_SIZE + 1);
#endif
}

void KeyID::generatePrivateKey(Web3* web3)
{
  (void)web3;
  if (!Crypto::RandomBytes(privateKeyBytes, ETHERS_PRIVATEKEY_LENGTH))
  {
    Serial.println("Warning: hardware CSPRNG unavailable; private key was not generated");
    recoveredKey = false;
    return;
  }

  std::string privateKey = Util::ConvertBytesToHex(privateKeyBytes, ETHERS_PRIVATEKEY_LENGTH);

  EEPROM.write(0, 64);
  for (int i = 0; i < ETHERS_PRIVATEKEY_LENGTH; i++)
  {
    EEPROM.write(i + 1, privateKeyBytes[i]);
  }

  if (!EEPROM.commit())
  {
    Serial.println("failed to persist private key to EEPROM");
    recoveredKey = false;
    return;
  }

  initPrivateKey(privateKey);
}

void KeyID::getSignature(uint8_t* signature, BYTE* msgBytes, int length)
{
  if (!recoveredKey || signature == nullptr || msgBytes == nullptr || length <= 0) return;

  uint8_t hash[ETHERS_KECCAK256_LENGTH];
  Crypto::Keccak256(static_cast<uint8_t*>(msgBytes), length, hash);
  crypto.Sign(hash, signature);
}

void KeyID::initPrivateKey(const std::string& privateKey)
{
  crypto.SetPrivateKey(privateKey.c_str());
  recoveredKey = true;
  Serial.print("Device Address: ");
  Serial.println(getAddress().c_str());
}

const std::string KeyID::getAddress()
{
  BYTE ethAddressBytes[ETHERS_ADDRESS_LENGTH];
  BYTE publicKey[ETHERS_PUBLICKEY_LENGTH];
  Crypto::PrivateKeyToPublic(privateKeyBytes, publicKey);
  Crypto::PublicKeyToAddress(publicKey, ethAddressBytes);
  return Util::ConvertBytesToHex(ethAddressBytes, ETHERS_ADDRESS_LENGTH);
}

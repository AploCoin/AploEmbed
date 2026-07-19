# AploEmbed

AploEmbed is an Arduino library for using AploCoin from embedded devices.

It provides RPC helpers, transaction signing, APLO/Gaplo conversion, staking helpers, and mining contract helpers for AploCoin chain ID `28282`.

## Supported boards

- ESP32
- ESP32-C3
- ESP8266

ESP32 is the main target. ESP32-C3 and ESP8266 need a few PlatformIO settings; see [docs/platforms.md](docs/platforms.md).

## Install

### PlatformIO

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
  https://github.com/AploCoin/AploEmbed.git#master
```

### Arduino IDE

Download the repository as a ZIP file and add it through:

```text
Sketch -> Include Library -> Add .ZIP Library
```

## Basic use

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <Web3.h>
#include <AploContracts.h>

Web3 web3;

void setup() {
  Serial.begin(115200);

  // Connect WiFi in your sketch before making RPC calls.

  std::string address = "0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb";
  std::string balance = web3.AploGetBalanceInAplo(&address);

  Serial.print("Balance: ");
  Serial.print(balance.c_str());
  Serial.println(" APLO");
}

void loop() {}
```

For complete sketches, see [`examples/`](examples/).

## RPC endpoints

By default AploEmbed uses:

- primary: `pub1.aplocoin.com`
- fallback: `pub2.aplocoin.com`

You can override the endpoint:

```cpp
Web3 web3("custom-rpc.aplocoin.com");
```

Or provide a fallback:

```cpp
Web3 web3("primary-rpc.aplocoin.com", "fallback-rpc.aplocoin.com");
```

Do not hammer public RPC endpoints from development loops. Use your own RPC endpoint for heavier testing.

## API overview

### Balance

```cpp
uint256_t AploGetBalance(const std::string* address);
std::string AploGetBalanceInAplo(const std::string* address);
```

### Transactions

Use `Contract` for signed transactions:

```cpp
Contract contract(&web3, "");
contract.SetPrivateKey(PRIVATE_KEY);

std::string to = "0xRecipientAddressHere";
std::string data = "";
uint256_t value = Util::ConvertToWei(10.0, 18);

uint32_t nonce = web3.EthGetTransactionCount(&fromAddress);
std::string txHash = contract.SendTransaction(
  nonce,
  1000000000ULL,
  21000,
  &to,
  &value,
  &data
);
```

### Staking

```cpp
uint256_t AploGetStake(const std::string* stakingContract, const std::string* account);
uint256_t AploGetStakeMultiplier(const std::string* stakingContract, const std::string* account);
std::string AploStake(const std::string* stakingContract, const uint256_t* amount, const char* privateKey, const std::string* fromAddress);
std::string AploUnstake(const std::string* stakingContract, const char* privateKey, const std::string* fromAddress);
```

### Mining

```cpp
bool AploGetMinerParams(
  const std::string* miningContract,
  const std::string* minerAddress,
  uint256_t* lastBlock,
  uint256_t* currentDifficulty,
  uint256_t* totalMined,
  uint256_t* prevHash
);

std::string AploMine(const std::string* miningContract, const std::string* nonce, const char* privateKey, const std::string* fromAddress);
```

## Units

AploCoin uses APLO as the native currency and Gaplo as the base unit.

```text
1 APLO = 10^18 Gaplo
```

Helpers:

```cpp
uint256_t gaplo = Util::ConvertToWei(1.5, 18);
std::string aplo = Util::ConvertWeiToEthString(&gaplo, 18);
```

## Documentation

- [Platform notes](docs/platforms.md)
- [Security notes](docs/security.md)
- [Examples](examples/)

## License

MIT. See [LICENSE](LICENSE).

## Credits

AploEmbed is based on the original embedded Web3 work by James Brown and the AlphaWallet team.

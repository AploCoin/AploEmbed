# AploEmbed

AploEmbed is an Arduino library for using AploCoin from embedded devices.

It provides RPC helpers, transaction signing, APLO/Gaplo conversion, staking helpers, and mining contract helpers for AploCoin chain ID `28282`.

Mining rewards are accounted in Gaplo. Staking locks APLO in the staking contract and sets the reward multiplier used by mining rewards.

## Supported boards

- ESP32
- ESP32-C3
- ESP8266

ESP32, ESP32-C3, and ESP8266 have dedicated build folders inside each example. The application logic is shared, while `BoardWifi.h` keeps board-specific radio behavior isolated.

For example, to build mining:

```bash
cd "examples/Aplo Mining/ESP32"     # or ESP32-C3 / ESP8266
platformio run
platformio run --target upload
platformio device monitor
```

Do not copy WiFi reset code between board folders: ESP8266 intentionally uses a non-destructive disconnect, while ESP32-C3 disables modem sleep and performs a full STA reset.

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

For ESP32-C3 and ESP8266, copy the target-specific environment from:

- [`docs/platformio-esp32-c3.md`](docs/platformio-esp32-c3.md)
- [`docs/platformio-esp8266.md`](docs/platformio-esp8266.md)

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
  std::string balance = web3.AploGetAploBalanceString(&address);

  Serial.print("APLO Balance: ");
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

AploCoin has two balances that users usually care about:

- APLO balance — value/staking balance;
- Gas balance — Gaplo gas token balance used for fees.

```cpp
uint256_t AploGetAploBalance(const std::string* address);
std::string AploGetAploBalanceString(const std::string* address);

uint256_t AploGetGasBalance(const std::string* address);
std::string AploGetGasBalanceString(const std::string* address);
```

Backward-compatible aliases return APLO balance:

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
uint256_t value = Util::ConvertDecimalToWei("10", 18);

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

Mining submits a `mine(bytes32)` contract transaction. A successful call earns a Gaplo reward. The staked APLO amount gates/scales the reward multiplier; the base reward comes from gas spent by the transaction.

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

`AploMine()` uses the generic `Contract::SetupContractData("mine(bytes32)", ...)` ABI encoder. Fixed bytes types (`bytes1` through `bytes32`) are encoded as static ABI words.

## Units

AploCoin uses APLO for value/staking and Gaplo as the gas token unit.

```text
1 APLO = 10^18 Gaplo units for conversion helpers
```

Helpers:

```cpp
uint256_t gaplo = Util::ConvertDecimalToWei("1.5", 18);
std::string aplo = Util::ConvertWeiToEthString(&gaplo, 18);
```

## Documentation

- [Platform support](docs/platforms.md)
- [ESP32 PlatformIO](docs/platformio-esp32.md)
- [ESP32-C3 PlatformIO](docs/platformio-esp32-c3.md)
- [ESP8266 PlatformIO](docs/platformio-esp8266.md)
- [ABI and contract helpers](docs/abi-and-contracts.md)
- [Security notes](docs/security.md)
- [Examples](examples/)

## License

MIT. See [LICENSE](LICENSE).

## Credits

AploEmbed is based on the original embedded Web3 work by James Brown and the AlphaWallet team.

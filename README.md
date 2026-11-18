# AploEmbed

AploEmbed is an Arduino library for using AploCoin from embedded devices.

It provides RPC helpers, transaction signing, separate APLO and GAplo balances, staking and native delegated session keys. Legacy Contract helpers use AploCoin chain ID `28282`; the session API reads the effective chain ID from RPC.

Mining algorithms and submission policy live in the separate [MiningEmbed](https://github.com/AploCoin/MiningEmbed) library. Board applications live in [FirmwareEmbed](https://github.com/AploCoin/FirmwareEmbed). See [migration](docs/migration-0.1.md).

Examples build the current checkout with `symlink://../..`; no GitHub branch is downloaded.
From this repository root, run:

```sh
pio run -d "examples/Aplo Session Keys" -e esp12e -e esp32dev -e esp32-c3-devkitm-1
```

The checked-in configurations pin ESP8266 platform 4.2.1 and ESP32 platform 6.12.0.
For an external project, add the GitHub library under lib_deps. Pin a commit for a reproducible firmware build.

## Supported boards

- ESP32
- ESP32-C3
- ESP8266

Each example is a standalone PlatformIO project with complete application and WiFi code in `src/main.cpp`. Its `platformio.ini` contains ESP32, ESP32-C3, and ESP8266 environments and builds against this checkout.

For example, to build a balance query:

```bash
cd "examples/Aplo Balance Query"
platformio run -e esp32dev  # or esp32-c3-devkitm-1 / esp12e
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
  https://github.com/AploCoin/AploEmbed.git
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

  // Connect WiFi and synchronize system time before RPC.
  // Build with APLO_TRUST_PROFILE; see docs/tls-trust.md.

  std::string address = "0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb";
  std::string balance = web3.AploGetAploBalanceString(&address);

  Serial.print("APLO Balance: ");
  Serial.print(balance.c_str());
  Serial.println(" APLO");
}

void loop() {}
```

For complete sketches, see [`examples/`](examples/).

## Source layout

- src/session/ contains key handling, the native registry API, RLP signing and the Web3 adapter. Include SessionKeys.h to access the session API.
- src/rpc/ contains the Web3 transport and script client. src/internal/ contains their shared response parser.
- src/abi/, src/crypto/ and src/encoding/ contain contract encoding, cryptography and conversion helpers. Existing public headers remain at the top of src/.

## RPC endpoints and HTTPS trust

Configure URL and CA on the PC/CI before building. `Web3()` selects those explicit profile URLs and roots; it has no built-in CA. Start with the [three setup commands](docs/tls-trust.md): local public CA PEM, pinned compact public root set, or your own HTTPS RPC. The firmware projects accept a public root directly or a header generated from the same profile.

`Web3(primary, fallback)` and `setCertificate(pem)` remain available; the new `setCertificate(primaryPem, fallbackPem)` separates endpoint authorities. `Web3(customUrl)` has no implicit fallback. Missing trust and unsupported modes fail closed. Synchronize time before HTTPS. Public deployment and future Aplo CA are not assumed.

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

### Native session keys

`#include <SessionKeys.h>` provides `aplo::SessionKeys`, a RAM-only `SessionKey`, exact-width `NativeTransaction`, owner registration/revocation payloads, proof of possession, checked RPC, and receipt states. It supports external owner digest signers without loading the owner's secret on the device. See [session keys](docs/session-keys.md) and [the compact example](examples/Aplo%20Session%20Keys/README.md).

## Units

APLO funds value/staking. GAplo independently funds transaction fees. Each asset uses 18 decimal places.

```text
1 APLO = 10^18 APLO base units
1 GAplo = 10^18 GAplo base units
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

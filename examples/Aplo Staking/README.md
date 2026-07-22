# Aplo staking

Reads the current stake and multiplier, then submits `stake(uint256)` when the wallet has enough APLO.

A stake of 1,000 APLO enables mining rewards at 1.0x. Each additional 1,000 APLO adds 0.1x, up to 1.7x at 8,000 APLO.

## Configure

Edit `src/main.cpp`:

```cpp
const char *ssid = "YourNetwork";
const char *password = "YourPassword";
const char *PRIVATE_KEY = "your64characterhexprivatekey";
#define STAKE_AMOUNT_APLO "1000"
```

The amount is an exact decimal string. Do not commit a real private key.

## Build

```bash
pio run -e esp32dev
pio run -e esp32-c3-devkitm-1
pio run -e esp12e
```

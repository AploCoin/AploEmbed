# Aplo mining

Mines Gaplo through the AploCoin mining contract. The sketch reads `miner_params`, searches for a nonce, and submits `mine(bytes32)` when the hash meets the current target.

A wallet needs at least 1,000 APLO staked to receive mining rewards. The multiplier starts at 1.0x and increases by 0.1x per additional 1,000 APLO, up to 1.7x.

## Configure

Edit `src/main.cpp`:

```cpp
const char *ssid = "YourNetwork";
const char *password = "YourPassword";
const char *PRIVATE_KEY = "your64characterhexprivatekey";
```

The wallet address is derived from `PRIVATE_KEY`. Do not commit a real key.

## Build

```bash
pio run -e esp32dev
pio run -e esp32-c3-devkitm-1
pio run -e esp12e
```

Each environment uses the same sketch. Small WiFi differences are handled with compile-time branches in `src/main.cpp`.

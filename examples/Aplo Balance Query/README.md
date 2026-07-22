# Balance query

Reads an AploCoin balance and prints it in APLO and raw Gaplo units.

## Configure

Edit `src/main.cpp`:

```cpp
const char *ssid = "YourNetwork";
const char *password = "YourPassword";
#define APLO_ADDRESS "0x..."
```

## Build

```bash
pio run -e esp32dev
pio run -e esp32-c3-devkitm-1
pio run -e esp12e
```

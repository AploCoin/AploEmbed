# Send APLO

Sends a native APLO transfer. The sketch derives the sender address from the private key, checks the balance, signs the transaction, and prints the transaction hash.

## Configure

Edit `src/main.cpp`:

```cpp
const char *ssid = "YourNetwork";
const char *password = "YourPassword";
const char *PRIVATE_KEY = "your64characterhexprivatekey";
#define RECIPIENT_ADDRESS "0x..."
#define SEND_AMOUNT_APLO "0.01"
```

Amounts use decimal strings to avoid floating-point rounding. Do not commit a real private key.

## Build

```bash
pio run -e esp32dev
pio run -e esp32-c3-devkitm-1
pio run -e esp12e
```

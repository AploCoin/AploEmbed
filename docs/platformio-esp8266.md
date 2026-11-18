# PlatformIO: ESP8266

ESP8266 is supported for smaller AploEmbed sketches. It has tighter RAM and TLS constraints than ESP32/ESP32-C3, so keep application code compact.

## Standard ESP8266 environment

Copy this environment into an example `platformio.ini` when targeting ESP8266:

```ini
[env:esp12e]
platform = espressif8266@4.2.1
board = esp12e
framework = arduino
monitor_speed = 115200

lib_deps =
    symlink:///absolute/path/to/AploEmbed

build_flags =
    -DCORE_DEBUG_LEVEL=3
    -std=gnu++11
```

Build/upload:

```bash
platformio run -e esp12e
platformio run -e esp12e -t upload
platformio device monitor -b 115200
```

## TLS

ESP8266 uses BearSSL. ESP32 CA bundle APIs are not available on ESP8266.

Supported AploEmbed modes:

- an explicit compact [build trust profile](tls-trust.md);
- `setCertificate(...)` with a PEM trust anchor;
- no insecure mode: `setInsecure()` reports unavailable.

Do not use `setCertificateBundle(...)` expecting ESP32 behavior on ESP8266.

## Memory guidance

ESP8266 has limited RAM. To keep sketches reliable:

- AploEmbed creates and reuses one ESP8266 BearSSL transport when Web3 is first constructed. Examples may defer Web3 until after WiFi; account for the shared TLS secondary stack in application heap planning.
- RPC responses are bounded and their storage is reserved before TLS activity; oversized responses fail cleanly instead of growing the heap during a handshake.
- MiningEmbed owns the fixed-buffer nonce/hash loop; the full board runtime moved to FirmwareEmbed. Base examples cover balances, transactions, staking and sessions.
- keep transaction signing paths short and avoid unnecessary copies;
- prefer one RPC operation at a time.

## WiFi

ESP8266 supports 2.4 GHz WiFi only. It cannot connect to 5 GHz-only SSIDs.

## Local library testing

Use a local dependency while validating unpublished changes:

```ini
lib_deps =
    symlink:///absolute/path/to/AploEmbed
```

If PlatformIO uses an older cached dependency:

```bash
rm -rf .pio/libdeps/esp12e/AploEmbed
platformio run -e esp12e
```

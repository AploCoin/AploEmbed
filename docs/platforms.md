# Platform notes

AploEmbed targets Arduino-compatible boards. ESP32 is the primary target; ESP32-C3 and ESP8266 are supported with a few build-system differences.

## PlatformIO dependency

Use the GitHub dependency directly:

```ini
lib_deps =
  https://github.com/AploCoin/AploEmbed.git#master
```

If PlatformIO keeps compiling an older commit, remove the cached dependency:

```bash
rm -rf .pio/libdeps/<env>/AploEmbed
platformio run -e <env>
```

## ESP32

Typical configuration:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
  https://github.com/AploCoin/AploEmbed.git#master

build_flags =
  -DCORE_DEBUG_LEVEL=3
```

If your sketch grows beyond the default partition size, use a larger app partition:

```ini
board_build.partitions = no_ota.csv
```

## ESP32-C3

ESP32-C3 may use native USB CDC for `Serial`. If flashing works but the serial monitor stays silent, enable CDC-on-boot:

```ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
monitor_rts = 0
monitor_dtr = 0

lib_deps =
  https://github.com/AploCoin/AploEmbed.git#master

build_flags =
  -DCORE_DEBUG_LEVEL=3
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
```

ESP32-C3 supports 2.4 GHz WiFi only. It will not connect to a 5 GHz-only SSID.

## ESP8266

Typical configuration:

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
monitor_speed = 115200

lib_deps =
  https://github.com/AploCoin/AploEmbed.git#master

build_flags =
  -DCORE_DEBUG_LEVEL=3
  -std=gnu++11
```

ESP8266 has much less RAM than ESP32. TLS, JSON-RPC responses, and transaction signing can push memory usage quickly. Keep sketches small and avoid unnecessary dynamic allocation in application code.

## WiFi diagnostics

The examples wait for serial output on USB CDC boards and keep WiFi diagnostics visible instead of immediately rebooting on connection failure.

Common causes of WiFi failure:

- wrong SSID or password
- 5 GHz network used with ESP32-C3 or ESP8266
- router MAC filtering
- weak signal during boot
- captive portal or enterprise WiFi

## TLS notes

AploEmbed does not hardcode AploCoin RPC certificates and does not disable certificate validation automatically.

ESP32 supports PEM certificates and CA bundles through `WiFiClientSecure`. ESP8266 uses BearSSL and does not support ESP32 CA bundles. For production wallets, configure a real trust anchor and avoid insecure mode.

See [security.md](security.md) for TLS details.

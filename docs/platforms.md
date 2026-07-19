# Platform support

AploEmbed targets Arduino-compatible WiFi boards. The example `platformio.ini` files intentionally keep only the default ESP32 environment so examples stay copy-paste friendly.

Use the platform-specific files below when building for another target:

- [ESP32](platformio-esp32.md) — default target used by examples.
- [ESP32-C3](platformio-esp32-c3.md) — UART-safe serial settings, USB CDC notes, ESP32-C3 WiFi/mining notes.
- [ESP8266](platformio-esp8266.md) — BearSSL and low-memory guidance.

## PlatformIO dependency

Use the GitHub dependency directly:

```ini
lib_deps =
  https://github.com/AploCoin/AploEmbed.git#master
```

For local source validation before pushing to GitHub, use an absolute local dependency path:

```ini
lib_deps =
  file:///absolute/path/to/AploEmbed
```

If PlatformIO keeps compiling an older commit, remove the cached dependency:

```bash
rm -rf .pio/libdeps/<env>/AploEmbed
platformio run -e <env>
```

## Choosing a target

| Target | Use when | Notes |
| --- | --- | --- |
| ESP32 | Default examples and easiest testing | Most RAM/headroom; supports PEM certs and CA bundles. |
| ESP32-C3 | RISC-V ESP32-C3 boards | 2.4 GHz WiFi only; use UART-safe serial defaults unless using native USB CDC intentionally. |
| ESP8266 | Smaller legacy boards | 2.4 GHz WiFi only; BearSSL; much tighter RAM. |

## WiFi diagnostics

Common causes of WiFi failure:

- wrong SSID or password;
- 5 GHz network used with ESP32-C3 or ESP8266;
- router MAC filtering;
- weak signal during boot;
- captive portal or enterprise WiFi.

## TLS notes

AploEmbed does not disable certificate validation automatically. The default `Web3()` constructor uses automatic bundled root CA resolution for public AploCoin HTTPS RPC endpoints.

ESP32 supports PEM certificates and CA bundles. ESP8266 uses BearSSL and does not support ESP32 CA bundles. For production wallets, configure a real trust anchor and avoid insecure mode.

See [security.md](security.md) for TLS details.

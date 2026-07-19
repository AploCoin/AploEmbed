# PlatformIO: ESP32

ESP32 is the default AploEmbed example target. The `platformio.ini` files under `examples/` intentionally keep only this standard ESP32 environment.

## Standard environment

```ini
[platformio]
src_dir = .

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

## Build and upload

From an example directory:

```bash
platformio run -e esp32dev
platformio run -e esp32dev -t upload
platformio device monitor -b 115200
```

If `platformio` is not installed globally:

```bash
uvx --from platformio platformio run -e esp32dev
```

## Local library testing

When validating local AploEmbed source changes before they are pushed to GitHub, replace the Git dependency with a local path:

```ini
lib_deps =
    file:///absolute/path/to/AploEmbed
```

If PlatformIO keeps compiling an older cached copy, remove the cached dependency and rebuild:

```bash
rm -rf .pio/libdeps/esp32dev/AploEmbed
platformio run -e esp32dev
```

## Memory and partitions

Most examples fit the default ESP32 app partition. If a larger application grows beyond the default partition, use a larger app partition:

```ini
board_build.partitions = no_ota.csv
```

Avoid adding PSRAM flags by default. Add board-specific PSRAM flags only when the selected ESP32 board actually has PSRAM.

## TLS

ESP32 supports PEM trust anchors via `setCertificate(...)` and CA bundles via `setCertificateBundle(...)`.

The default `Web3()` constructor uses automatic bundled root CA resolution for the public AploCoin HTTPS RPC endpoints. See [`security.md`](security.md) for certificate modes.

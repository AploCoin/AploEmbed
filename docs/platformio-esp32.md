# PlatformIO: ESP32

ESP32 is the default AploEmbed example target. Each example includes this default ESP32 environment plus ESP32-C3 and ESP8266.

## Standard environment

```ini
[platformio]
src_dir = .

[env:esp32dev]
platform = espressif32@6.12.0
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    symlink:///absolute/path/to/AploEmbed

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
    symlink:///absolute/path/to/AploEmbed
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

PEM trust profiles are the default; CA bundle support requires `APLO_TLS_ENABLE_BUNDLE=1`. No CA is embedded in the SDK. Set endpoint URL/root inputs on the PC and synchronize device time before RPC. See [trust setup and device limits](tls-trust.md).

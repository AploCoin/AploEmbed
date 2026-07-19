# PlatformIO: ESP32-C3

ESP32-C3 is supported, but it needs UART-safe serial settings and more conservative assumptions than classic ESP32 boards.

## Standard ESP32-C3 environment

Copy this environment into an example `platformio.ini` when targeting ESP32-C3:

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
```

Build/upload:

```bash
platformio run -e esp32-c3-devkitm-1
platformio run -e esp32-c3-devkitm-1 -t upload
platformio device monitor -b 115200
```

## Serial and reset behavior

Keep these defaults for UART bridge boards:

```ini
monitor_rts = 0
monitor_dtr = 0
```

They prevent PlatformIO's serial monitor from toggling boot/reset lines in a way that can hide early diagnostics.

If the ROM boot log appears in the same monitor as your sketch logs, treat the board as UART-monitored and do not enable native USB CDC by default.

## Optional native USB CDC

Only enable native USB CDC when the board is wired and monitored through the native USB CDC interface:

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

Native USB CDC can re-enumerate as `/dev/ttyACM0`, `/dev/ttyACM1`, etc. after resets. Prefer stable `/dev/serial/by-id/*` or `/dev/serial/by-path/*` names on Linux.

## WiFi

ESP32-C3 supports 2.4 GHz WiFi only. It cannot connect to 5 GHz-only SSIDs.

If WiFi fails:

- verify SSID/password;
- verify the AP has 2.4 GHz enabled;
- check router MAC filtering;
- keep the board close to the AP during boot;
- avoid captive portal / enterprise WiFi for initial tests.

## Mining and RPC stability

ESP32-C3 has less RAM than many ESP32 boards. For mining examples:

- keep debug output concise after initial diagnostics;
- avoid frequent reboot-on-failure logic;
- let RPC failures return to the mining loop and retry;
- verify transaction ABI encoding with a local-library build before flashing.

A previous ESP32-C3 mining crash occurred because fixed `bytes32` values were incorrectly ABI-encoded as dynamic `bytes`. AploEmbed's generic ABI encoder now supports fixed bytes types (`bytes1` through `bytes32`), so `AploMine()` should use `SetupContractData("mine(bytes32)", ...)` rather than manually concatenated calldata.

## Local library testing

Use a temporary PlatformIO fixture when validating unpublished source changes:

```ini
[platformio]
src_dir = src

[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
monitor_rts = 0
monitor_dtr = 0
lib_deps = file:///absolute/path/to/AploEmbed
```

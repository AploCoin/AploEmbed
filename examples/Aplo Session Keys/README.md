# Session keys

Configure public OWNER and WiFi in `src/main.cpp`, and native HTTPS RPC URL/CA in a [build trust profile](../../docs/tls-trust.md). The sketch generates a RAM-only delegated key after WiFi starts, prepares an APLO-transfer session and prints public registration transaction fields for review/signing on another owner device. It never stores or prints the delegated secret and never loads an owner secret. Without owner configuration it exits before networking/signing.

The example stops at the owner handoff. The key remains in this device's RAM; restarting loses it. Integrate an authenticated owner-signing channel and retain the exact public policy/domain before enabling delegated submissions. See [the lifecycle and callback](../../docs/session-keys.md).

Set `APLO_TRUST_PROFILE=/absolute/path/to/profile.json` for authenticated HTTPS. From this example directory, build with `pio run -e esp12e -e esp32dev -e esp32-c3-devkitm-1`.
From the repository root, use `pio run -d "examples/Aplo Session Keys" -e esp12e -e esp32dev -e esp32-c3-devkitm-1`.
The relative `symlink://../..` dependency selects this exact checkout. ESP8266 platform 4.2.1 and ESP32 platform 6.12.0 are pinned in `platformio.ini`; no unpublished session header is fetched from GitHub master.

The sketch separates WiFi connection, policy/registration preparation and the public owner handoff. It reports RPC/preparation errors and stops. Continue integration at the marked handoff and retain the public policy/domain in your application's lifecycle state; no signing channel or automatic delegated submission is enabled by this example.

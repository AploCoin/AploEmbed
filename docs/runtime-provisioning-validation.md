# Runtime provisioning and journal evidence — 2026-11-19

SDK version: `0.2.0` in both package manifests.

Base checkout: `1619dffc79e21bed010bd3dac2ff957a4fda0195`. Work is confined to AploEmbed. No AploNode, MiningEmbed, FirmwareEmbed, website, real keys, hardware, or remote refs were changed.

## Public integration contract

- `static bool Web3::isValidCertificate(const char* pem)`: CA-only PEM, maximum 4096 bytes excluding NUL; immutable borrowed RAM on ESP32/ESP8266 and PROGMEM on ESP8266. Full framing/base64/exact DER checks precede strict platform parsing. Every certificate must have BasicConstraints CA=true; leaf-only and root+leaf chains reject. This does not verify self-signatures or establish server/time/chain trust. Multiple certificates support bounded root overlap.
- `SessionResult SessionKeys::LatestNonce(const std::string&, uint64_t&)`: checked exact-width `eth_getTransactionCount` with latest; PendingNonce retains pending. Output is zero on error; callers must check SessionResult.
- `setCertificate(nullptr, nullptr)`: immediately stops/detaches/frees the ESP8266 cached trust anchor using the destructor's cleanup path; keeps the Web3 object reusable. Endpoint PEM remains borrowed and immutable until replacement/destruction.
- `APLO_RPC_SERIAL_LOGS`: defaults to 1; firmware must define `-DAPLO_RPC_SERIAL_LOGS=0` build-wide for its protocol channel. All SDK diagnostics use the flag; raw request/body/error dumps and derived-address logs are removed.
- SessionKeys receipts and the legacy Web3 receipt helper require matching direct transactionHash before success/revert. Missing, nested-only, malformed, duplicate/escaped-duplicate or mismatched hashes cannot confirm. Overflow reduction preserves identity and status while retaining the existing 768-byte ESP8266 body cap. This does not implement journal persistence or finality policy.
- Runtime HTTPS request targets reject non-URI ASCII, controls, fragments, backslashes, non-ASCII and malformed percent escapes. No configured URL or native TLS error text is logged. Hostname/time/CA verification and mandatory ESP8266 MFLN remain enabled, with no insecure retry.

## Verified checks

- `python3 tests/run_native.py`: PASS, actual C++/Trezor with ASan/UBSan and independent RLP decoder. Added latest/pending separation, 41-bit nonce, UINT64_MAX, overflow/malformed/RPC/transport errors, receipt identity and 8-KB streaming regressions.
- `python3 tests/test_static_regressions.py`: 32 PASS, one optional full example matrix skipped.
- `python3 tests/test_tls_trust.py --quiet`: PASS with ASan/UBSan. Actual host mbedTLS parses PEM; loopback HTTPS uses OpenSSL. Covers roots/overlap, exact 4096/4097 bounds, invalid extra PEM/DER/mixed key, leaf-only and root+leaf rejection, partial-chain failure, name/time failures, failover and no requests with invalid trust. NativeSerial counts remain zero. Large receipts also traverse actual Web3 and reject wrong hashes.
- `PLATFORMIO_CORE_DIR=/private/tmp/aplo-pr-review.jUwGYd/pio python3 tests/test_tls_trust.py --esp8266 --quiet`: PASS. Compiles the installed pinned ESP8266 BearSSL PEM/X509 decoder sources on the host with only a pgmspace boundary adapter. Trust-anchor ownership instrumentation verifies at most one cached list and immediate zero after clearing. TLS handshake remains OpenSSL host evidence, not a BearSSL hardware measurement.
- `c++ -std=c++11 -Wall -Wextra -Werror -Itests/native -Isrc -fsyntax-only src/session/SessionRpc.cpp src/internal/JsonRpcParser.cpp tests/native/rpc_tests.cpp`: PASS.
- Changed C++ regions/new helpers: clang-format 21 dry run PASS; Python py_compile and git diff --check PASS.
- `tests/run_native.py` now selects Darwin `-Wl,-dead_strip`, otherwise GNU `-Wl,--gc-sections`. Linux compatibility inspected; no Linux execution is claimed.

Targeted PlatformIO builds passed against the same current source files (espressif8266@4.2.1 / espressif32@6.12.0):

| App | Target | SDK logs | Static RAM bytes | Flash bytes |
|---|---|---|---:|---:|
| Aplo Session Keys | esp12e | default 1 | 32564 | 423963 |
| Aplo Session Keys | esp32-c3-devkitm-1 | default 1 | 40588 | 955242 |
| tests/fixtures/runtime_https | esp12e | 0 | 30388 | 391851 |
| tests/fixtures/runtime_https | esp32-c3-devkitm-1 | 0 | 20884 | 515240 |

The API probe explicitly links HTTPS constructors, the endpoint-specific PEM setter, validator, LatestNonce and PendingNonce. It is a compile-only synthetic fixture and was never uploaded. esp32dev was not compiled: its Xtensa ESP32 toolchain is absent locally; installation was interrupted. ESP32-C3 provides the ESP32-family compilation check. This is a targeted matrix, not all examples/boards. Compiler/linker byte counts do not establish runtime memory or hardware readiness.

Session example input-manifest SHA256: `11237eef181656d5dfb2e02ff9ec4cf78e374056ad20157ed5d1837a63c5b992` (sorted file-to-SHA256 JSON, from tests/build_examples.py results.json). Checked again against current checkout after builds. Logs and full per-file build inputs: `/private/tmp/aploembed-runtime-builds-20261119/`; API build log: `/private/tmp/aploembed-runtime-api-final-20261119.log`; native/static/TLS logs: `/private/tmp/aploembed-{native,static,tls,tls8266}-final-20261119.log`.

No physical WiFi/TLS, heap/largest-block, watchdog, or sustained mining behavior was measured. No remote push or publication.

## Binary SHA256

- Session example / esp12e: `58e4301f408de856e36a76315bb58cec0bbe531c16db7efa0fa0b28f0b119302`
- Session example / esp32-c3-devkitm-1: `3a88c9b5ad92a7f4a4a325bae2938e919b751687b3054cd7ada886b5e2b44929`
- Runtime API probe / esp12e: `e8b39acb4b904d0ec8c3ea8ea32a3f7aa47cd2a546fd61b4f63a2d646a1e9d36`
- Runtime API probe / esp32-c3-devkitm-1: `179ed197b02cced03f6f0db8323dac18560f271b64c304fbe772171889a22ee0`

## Exact changed files

- `docs/runtime-provisioning-validation.md`
- `docs/session-keys.md`
- `docs/tls-trust.md`
- `library.json`
- `library.properties`
- `src/AploTlsConfig.h`
- `src/ReceiptFilter.h`
- `src/Web3.h`
- `src/crypto/Crypto.cpp`
- `src/crypto/KeyID.cpp`
- `src/encoding/Util.cpp`
- `src/internal/JsonRpcParser.cpp`
- `src/internal/JsonRpcParser.h`
- `src/internal/PemChain.h`
- `src/internal/RpcLog.h`
- `src/rpc/ScriptClient.cpp`
- `src/rpc/Web3.cpp`
- `src/session/SessionKeys.h`
- `src/session/SessionRpc.cpp`
- `src/uint256/uint128_t.cpp`
- `src/uint256/uint256_t.cpp`
- `tests/build_examples.py`
- `tests/fixtures/runtime_https/platformio.ini`
- `tests/fixtures/runtime_https/src/main.cpp`
- `tests/native/Arduino.h`
- `tests/native/rpc_tests.cpp`
- `tests/native/session_tests.cpp`
- `tests/run_native.py`
- `tests/test_static_regressions.py`
- `tests/test_tls_trust.py`
- `tests/tls/Arduino.h`
- `tests/tls/BearSSLTest.h`
- `tests/tls/pgmspace.h`
- `tests/tls/web3_tls.cpp`

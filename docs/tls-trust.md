# HTTPS trust profiles

AploEmbed supplies transport and verification, not a CA chosen for a server. The PC/CI build compiles explicit endpoint roots into firmware. Normal builds read pinned files offline. They never fetch a leaf, inspect a server to choose trust, use HTTP, or retry insecurely. ESP validates the chain, hostname and certificate time on every connection; TLS session resumption is not configured.

## Three setups from a checkout

Requirements: Python 3, OpenSSL CLI, PlatformIO and the pinned Arduino platforms in each example. Run from the AploEmbed root. Set WiFi credentials/address in the example; no owner signing key belongs in a trust profile. URLs below are placeholders, not deployed Aplo services.

Local RPC: export **only the public PEM root CA** from your local gateway. No public domain is needed: use a LAN hostname resolving to the gateway on the ESP, with that hostname in the leaf DNS SAN.

```sh
python3 tools/trust/profile.py configure --kind local \
  --primary https://rpc.lan:19443 --ca /path/to/local-ca.pem \
  --out /path/to/local-trust.json
APLO_TRUST_PROFILE=/path/to/local-trust.json \
  pio run -d "examples/Aplo Balance Query" -e esp12e
```

Public RPC with the compact versioned set:

```sh
python3 tools/trust/profile.py configure --kind public \
  --primary https://YOUR_RPC_DOMAIN --public-set public-2026-11-v1 \
  --out /path/to/public-trust.json
APLO_TRUST_PROFILE=/path/to/public-trust.json \
  pio run -d "examples/Aplo Balance Query" -e esp32dev
```

`public-2026-11-v1` contains one opt-in ISRG Root X1. It is a candidate public policy, **not a claim about any future Aplo node's CA, domain or deployment**. The server must send an appropriate chain to this root. A different public issuer needs a separately reviewed root set. The manifest pins the version, official source URL, PEM and DER SHA256. Root bytes live under `tools/trust/roots/`, outside SDK runtime sources.

Your RPC, including two endpoints with different CAs:

```sh
python3 tools/trust/profile.py configure --kind custom \
  --primary https://private.example.invalid/rpc --ca /path/to/private-root.pem \
  --fallback https://public.example.invalid/rpc \
  --fallback-public-set public-2026-11-v1 \
  --out /path/to/my-trust.json
APLO_TRUST_PROFILE=/path/to/my-trust.json \
  pio run -d "examples/Aplo Balance Query" -e esp32-c3-devkitm-1
```

A custom domain with a suitable public chain can use `--public-set`; a private RPC uses `--ca`. Each URL requires its own explicit trust declaration. Repeat `--ca` or `--fallback-ca` during a planned root overlap. Paths and hashes in the JSON are public inputs; keep that JSON and PEM files together when moving a profile. The configure command verifies public-only PEM, root/self-signature, CA/keyCertSign and the current validity. Private keys, mixed cert/key files and leaf certificates are rejected, never copied. Provision a trustworthy clock separately.

The environment variable works with all four SDK examples. FirmwareEmbed can set its roots directly or use the same offline profile. To use a generated profile with FirmwareEmbed or the MiningEmbed example, run the generate command below with include/AploTrustProfile.h as the output in the board or example project. The generated URLs and roots are passed to Web3 at startup.

## Trust and fallback boundaries

`Web3()` uses generated URLs; `setAutoCertificate()` now means “use the build profile”. Without it or an explicit CA, HTTPS fails closed with `TlsError::MissingTrust` and a serial diagnostic. Legacy default URL strings remain for source compatibility without a profile; they imply no CA or deployment guarantee. `Web3(customUrl)` no longer silently adds a public fallback. `Web3(primary, fallback)` keeps its signature.

Profile trust matches the configured **host and port**. HTTPS schemes and DNS hostnames are matched without regard to ASCII case. The RPC path keeps its original case. Paths on the same origin share trust; conflicting CA sets on the same host/port are rejected. An unknown origin has no trust. On primary transport/authentication failure Web3 tries an explicitly supplied fallback, authenticating it with *its* CA. No redirects or server-advertised trust are followed. Both untrusted means no RPC POST. The existing RPC failover semantics remain: a missing response can cause a retry, including after a possibly accepted signed transaction. Application code must reconcile transaction hashes/nonces, never assume failure means not sent.

Manual API setup remains available, without the build tool:

```cpp
Web3 rpc("https://one.example.invalid", "https://two.example.invalid");
rpc.setCertificate(primaryPem, fallbackPem); // nullptr means no trust there
// rpc.setCertificate(sharedPem);           // both URLs trust ALL roots in it
```

PEM must be immutable, NUL terminated, and live until replaced or Web3 destruction. Immutable RAM PEM is supported on ESP32 and ESP8266; ESP8266 also supports static/PROGMEM PEM, as used by generated headers. Bundle storage also stays alive. No copies of PEM/bundle bytes are retained. ESP8266 keeps at most **one** parsed X509List, frees it before changing CA, stops the single client first and reapplies anchors before connect. Web3 calls are synchronous and must not overlap across tasks/instances. ESP32 uses one scoped TLS client per request. Do not mutate borrowed buffers in place. One shared set authorizes any included CA to authenticate **either** hostname; endpoint-specific sets keep that authority separate. A root is an authority pin, not a leaf/public-key pin: ordinary leaf/key renewal under the root continues to work.

For runtime provisioning, `Web3::isValidCertificate(pem)` accepts a NUL-terminated, CA-only PEM chain of **1..4096 bytes excluding the NUL**. The backing memory must contain that terminating NUL (or at least 4097 readable bytes for oversized-input rejection). It borrows the input only for the call and does not retain it. RAM is accepted on both boards; ESP8266 uses flash-aware reads for PROGMEM as well. Whitespace around/between certificates is allowed. Multiple certificates support a bounded old/new root overlap. Mixed keys, ignored text, incomplete additional PEM, invalid base64/DER, trailing DER bytes, any partially parsed chain, and certificates without BasicConstraints CA=true (including leaf-only and root+leaf input) are rejected. ESP32 requires mbedTLS parse return **zero**, plus the expected certificate count; ESP8266 validates every certificate with the actual BearSSL PEM/X509 decoders before building its one cached trust list. This API checks certificate syntax and the CA constraint, not full CA policy, self-signatures, server identity, current validity, or chain trust: those remain the authenticated connection's checks. `setCertificate()` borrows immutable bytes without parsing immediately; connection setup fails closed for invalid material.

`setCertificate(nullptr, nullptr)` immediately stops the ESP8266 client, detaches and frees its cached X509List, clears both borrowed pointers and disables endpoint trust. It keeps the Web3 instance and transport singleton for reuse after provisioning. No mining object reset is required. Calls remain synchronous and must not overlap.

Use build-wide `-DAPLO_RPC_SERIAL_LOGS=0` when Serial carries a firmware protocol. The default is `1` for compatibility. The flag gates SDK diagnostics, including all Web3 messages; raw ScriptClient input, ABI data dumps and derived-key address logs have been removed. Application example output and explicit caller-supplied Stream operations remain application responsibilities. Define the flag in PlatformIO `build_flags`, so it applies to SDK translation units too.

`tlsError()` reports the last attempted endpoint: MissingTrust, UnsupportedTrust, InvalidTrust, InvalidTime, ConnectionFailed or FragmentLength. A successfully authenticated fallback clears it. Diagnostics are generic and never include configured endpoints, raw RPC bodies, or native TLS error strings. `setInsecure()` is retained as a source-compatible *unavailable mode*; it never calls the core's insecure API. `AploCertificates.h` remains an empty compatibility include; removed PEM aliases and `getAploRootCAForHost()` must be replaced with explicit profiles/PEM. Binary/object layout compatibility is not promised; rebuild consumers.

## Cooperative RPC waits

`rpc.setRpcIdleCallback(callback, context)` installs an optional `void callback(void*)` for short application work, such as servicing a bounded Serial input buffer. Web3 calls it during its ESP8266 clock wait, before/after connect, while waiting for headers, and between body batches of at most 64 bytes. Passing `nullptr` removes it. The callback must not call Web3/TLS, change its trust/endpoints, or destroy the object; it is neither reentrant nor a cancellation hook. Applications must guard state-changing commands until the outer RPC returns.

`rpc.setRpcProgressCallback(callback, context)` adds a stricter service hook for bounded, nonblocking work. The callback must never call `yield()`/`delay()`, block on Serial output, write flash, touch Web3/TLS, change trust, or destroy the object. Buffer replies and transmit only within `availableForWrite()` capacity. A recursive RPC is rejected. Do not read changing Web3 state from either callback; publish application snapshots before starting the outer RPC. Passing `nullptr` clears the hook.

On ESP8266 the public Arduino 3.1.2 `Schedule.h` recurrent scheduler calls this hook about every millisecond when the core yields, including during DNS, TCP, BearSSL handshake and writes. It runs on the CONT stack, never in an interrupt. Registration returns `false` on allocation failure; the previous callback remains installed. There is one shared scheduler entry with static lifetime; it holds an application context only for the duration of an active synchronous RPC, so no queued callback can outlive its owner. Core operations and cryptographic work can still spend time without yielding; this is cooperative progress, without a hard latency guarantee.

On ESP32, installing the strict callback optionally runs the complete synchronous transport in a temporary FreeRTOS worker. The calling task waits for completion and services callbacks; the worker never invokes application callbacks or accesses Serial. Completion uses a semaphore, and the caller retains the Web3 object, request and borrowed trust buffers until the worker finishes. Core timeouts remain in force; workers are never forcibly cancelled. Allocation failure falls back to a single synchronous attempt before any RPC bytes have been sent. No worker starts when the strict callback is absent. Define `APLO_RPC_ESP32_PROGRESS_WORKER=0` to compile out this path (Lite firmware does). The default worker stack is 12288 bytes, configurable with `APLO_RPC_ESP32_WORKER_STACK_BYTES`. This additional stack and task allocation require runtime heap; the worker path has host synchronization tests and pinned-board compile checks, but still needs device measurements of stack headroom, heap and Serial latency.

DNS, TCP connect, TLS handshake and writes inside Arduino remain synchronous. In the pinned ESP8266 BearSSL wrapper, `setTimeout()` changes the outer Stream rather than forwarding to the internal context; the core resets its internal handshake timeout to 15 seconds and uses 5 seconds after handshake. Web3 uses the public scheduler without patching the core, changing ciphers, or weakening verification. ESP32 explicitly sets a 5-second TCP connect timeout, 15-second handshake timeout and 1-second socket timeout. DNS resolution remains core-owned on both boards. Applications must serialize calls across instances/tasks and defer mutation of TLS/transport state until the outer RPC completes.

Writes, headers and body share a 10-second response budget starting after the authenticated connection; the body also fails after 1 second without a byte. Checks use elapsed unsigned 32-bit time across `millis()` rollover, including after core reads and connection closure. A final byte arriving after either deadline is discarded. Continuous data cannot bypass the budget, and timeout discards the partial response before the existing fallback policy runs. ESP32 responses are capped at 32768 bytes; ESP8266 retains its 768-byte prefix and streaming receipt status filter. Build-wide overrides are `APLO_RPC_CONNECT_TIMEOUT_MS`, `APLO_RPC_HANDSHAKE_TIMEOUT_SECONDS`, `APLO_RPC_RESPONSE_TIMEOUT_MS`, `APLO_RPC_READ_IDLE_TIMEOUT_MS` and `APLO_RPC_RESPONSE_MAX` (ESP32 only). These response budgets apply around core calls; they cannot preempt an individual blocking core call.

`lastRpcEndpoint()` returns `RpcEndpointUsed::Primary` or `Fallback` for the last non-empty response accepted by the transport, without exposing the URL. It resets to `None` at the start of each RPC and stays `None` if both fail. It reports transport selection, not success of a JSON-RPC method; `tlsError()` separately describes TLS setup/connection, not HTTP read failures.

## Device constraints and feature flags

- **ESP8266 / esp12e:** pinned platform 4.2.1, Arduino 3.1.2, GCC 10.3. One or a few roots in flash; no mandatory bundle, filesystem or CertStore. Negotiated **1024-byte MFLN is required** before any POST; servers lacking it are unavailable. Do not raise buffers to 16 KB as an automatic fallback. The one BearSSL secondary stack (~6 KB) and connection buffers still need runtime heap. A broad optional CertStore was considered but is deliberately not implemented without ESP8266 hardware measurements; use explicit roots, a suitable gateway, or a larger board.
- **ESP32 / ESP32-C3:** pinned platform 6.12.0, Arduino 2.0.17, GCC 8.4. PEM is the default too. Set `-DAPLO_TLS_ENABLE_BUNDLE=1` only for applications needing many public authorities, then use native `setCertificateBundle(start, size)` and an explicitly supplied Espressif-format bundle. There is no bundled broad store. Core 3.x additionally requires size, but core 3.x is not part of this tested matrix. Bundle mode with null input, disabled feature or ESP8266 fails before connect. Broad bundle mode trusts the same authority set for both endpoints; choose PEM profiles for endpoint separation. Native bundle API validates its own binary format; don't pass PEM as bundle.
- **Other transport:** `-DAPLO_ENABLE_HTTPS=0` removes Web3 transport and the SessionKeys(Web3&) adapter. Web3 constructors and the legacy owning Contract(chainId) constructor are deleted at compile time; use `SessionKeys(RpcTransport, context)` and your own authenticated transport. SDK cryptography remains reusable. This also removes generated trust data even if a profile was supplied. `APLO_TLS_ENABLE_BUNDLE=0` is the default independently.

Both pinned validators match **DNS SAN**, not a leaf containing *only* iPAddress SAN for literal IPs. Prefer private LAN DNS. To retain the existing local literal-IP gateway workaround, issue both `IP:192.0.2.10` and `DNS:192.0.2.10` SANs and explicitly pass `--allow-ip-dns-san`. This acknowledges the core limitation, it does not bypass verification. It is a private deployment compatibility measure, not a public CA issuance recipe. IPv6 URL literals, credentials/query/fragment in URLs are not supported by the profile tool.

ESP32 application code must synchronize system time before RPC (examples do); ESP8266 preserves its bounded NTP retry unless the application already supplied valid time, then sets BearSSL time on each connect. Both refuse obviously unset time, and the TLS validator checks notBefore/notAfter. Time source integrity remains an application responsibility; compilation does not establish current trusted time. Root removal/distrust and revocation need firmware updates; no OCSP/CRL service is implemented.

## Offline builds and CA updates

Generate manually for a non-PlatformIO build:

```sh
python3 tools/trust/profile.py generate --profile /path/to/my-trust.json \
  --output /path/to/generated/AploTrustProfile.h
# Add a compiler definition APLO_TRUST_PROFILE_HEADER="/path/to/generated/AploTrustProfile.h"
```

PlatformIO's pre-script generates these files under `.pio/build/<env>/trust/`. It records deterministic public profile/header/PEM hashes. No absolute source PEM path or private key enters the header. Check in the JSON and its public PEMs for CI; ordinary builds need no network beyond initially installing the pinned toolchain. Builds reject changed PEM bytes until you explicitly reconfigure/review the lock.

An **explicit update operation**, outside ordinary builds:

```sh
python3 tools/trust/profile.py update \
  --source https://letsencrypt.org/certs/isrgrootx1.pem \
  --sha256 22b557a27055b33606b6559f37703928d3e4ad79f110b407d04986e1843543d1 \
  --out tools/trust/roots/public-REVIEWED-NEW-VERSION
```

Obtain the expected hash through an independently trusted review; a hash fetched alongside untrusted material is not authentication. The command verifies HTTPS and expected download hash and creates a new immutable version directory. Review source, subject, constraints, dates and DER fingerprint, commit it, then select that version explicitly. To combine roots for a rotation, list the old/new PEM entries and hashes in a new manifest; never overwrite the old version in place.

Plan root rotation in three releases: (1) distribute old+new trust *before* changing server chains, (2) switch the server after required devices have updated, (3) shortly after confirmed migration, release new-only trust and retire old-root service. Record an operational overlap deadline in release notes; don't retain obsolete roots indefinitely. Configure private/custom overlap by repeating `--ca old.pem --ca new.pem`; only public certificates are needed. A leaf rotation under the same trusted root needs no profile change. Offline devices that miss removal/rotation may require local reprovisioning; no unsafe fallback restores them.

Primary references: [Let's Encrypt roots and test sites](https://letsencrypt.org/certificates/), [ESP8266 BearSSL lifetimes/memory/MFLN](https://arduino-esp8266.readthedocs.io/en/3.1.2/esp8266wifi/bearssl-client-secure-class.html). The literal-IP limitation and bundle signatures depend on the pinned Arduino core.

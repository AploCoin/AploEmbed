# Security

AploEmbed signs AploCoin transactions on embedded devices. Treat every device running signing code as a wallet.

## Private keys

Do not hardcode real private keys in sketches. Example code uses placeholders only.

For production use:

- store keys outside the source tree
- avoid committing test keys that may later receive funds
- restrict physical access to flashed devices
- consider a secure element for higher-value wallets

## Transactions

Before sending a transaction:

- check the destination address
- verify the available balance includes gas
- start with a small amount
- check nonce handling if sending multiple transactions in a row

## RPC endpoints and TLS

Configure HTTPS URLs and endpoint-specific root CAs using [build trust profiles](tls-trust.md). The SDK embeds no default root; an absent or unsupported trust mode fails closed. Public RPC deployment/issuer availability is not assumed. ESP validates chain, hostname and time at each connection. Keep owner signing separate from RPC access.

`Web3()` / `setAutoCertificate()` select the explicit build profile. `setCertificate()` still accepts immutable, lifetime-stable PEM. Bundle support is opt-in on ESP32; `setInsecure()` now reports an unavailable mode. The trust guide documents migration, fallback authority boundaries, IP SAN/MFLN limitations, reproducible offline inputs and root rotation.

## Native session ownership

A delegated key is a restricted wallet, not a disposable password. Keep its owner signer on a separate device when possible. `SessionKey` defaults to RAM-only storage; enable WiFi before CSPRNG generation and never log/export secrets implicitly. Retain the exact chain domain and public policy. Revoke with the owner if a key is lost; used keys cannot be registered again. See [session keys](session-keys.md) for allowances, nonce ownership, expiry, explicit storage, external signers and uncertain-send reconciliation.

Legacy `Crypto::Sign` has a pre-existing inverted boolean success indication relative to its Trezor call. This release leaves that legacy behavior unchanged; do not use that boolean as a receipt or signature-verification result. The new SessionKey signer checks Trezor's actual return code and verifies recovered signer before a transaction is returned.

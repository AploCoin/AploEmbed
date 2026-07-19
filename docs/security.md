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

## RPC endpoints

The default RPC endpoints are public AploCoin endpoints. They are useful for examples and light use, but production projects should run or configure their own RPC infrastructure.

Avoid tight polling loops against public endpoints.

## TLS

AploEmbed leaves TLS handling to the active Arduino networking stack unless you configure a certificate bundle, PEM certificate, or insecure mode explicitly.

Preferred options:

1. Use a CA bundle on ESP32 PlatformIO builds.
2. Use a PEM root certificate with `setCertificate(...)`.
3. Use `setInsecure()` only for local testing.

### ESP32 CA bundle

```ini
board_build.embed_files = data/cert/x509_crt_bundle.bin
```

```cpp
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_data_cert_x509_crt_bundle_bin_end");

Web3 web3;
web3.setCertificateBundle(
  rootca_crt_bundle_start,
  rootca_crt_bundle_end - rootca_crt_bundle_start
);
```

### PEM root certificate

```cpp
const char* root_ca = "-----BEGIN CERTIFICATE-----\n...";

Web3 web3;
web3.setCertificate(root_ca);
```

ESP8266 uses BearSSL trust anchors internally. ESP32 CA bundles are not available on ESP8266.

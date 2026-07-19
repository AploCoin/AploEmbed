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

AploEmbed automatically resolves a bundled root CA for HTTPS RPC endpoints by default. The bundled default is ISRG Root X1, which validates Let's Encrypt certificates including the public AploCoin RPC endpoints. You can still override TLS with a CA bundle, a specific PEM certificate, or explicit insecure mode.

Preferred options:

1. Use the default auto resolver (`Web3()` or `web3.setAutoCertificate()`), which currently resolves to bundled ISRG Root X1 for Let's Encrypt endpoints.
2. Use a PEM root certificate with `setCertificate(...)` when your custom RPC endpoint uses a different CA.
3. Use a generated CA bundle on ESP32 PlatformIO builds when you need a broader trust store.
4. Use `setInsecure()` only for temporary local testing.

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

### Auto root CA resolution

```cpp
Web3 web3; // auto root CA resolution is enabled by default

// Optional explicit form:
web3.setAutoCertificate();
```

The auto resolver uses the bundled `APLO_ISRG_ROOT_X1_CA` trust anchor. This works for custom HTTPS RPC domains that use Let's Encrypt. For custom endpoints using another CA, pass that CA explicitly.

### PEM root certificate

```cpp
const char* custom_root_ca = "-----BEGIN CERTIFICATE-----\n...";

Web3 web3("custom-rpc.example.com");
web3.setCertificate(custom_root_ca);
```

ESP8266 uses BearSSL trust anchors internally. ESP32 CA bundles are not available on ESP8266.

#ifndef APLOEMBED_TLS_CONFIG_H
#define APLOEMBED_TLS_CONFIG_H
#ifndef APLO_ENABLE_HTTPS
#define APLO_ENABLE_HTTPS 1
#endif
// Compact PEM trust is the default on every board. No filesystem/CertStore.
#ifndef APLO_TLS_ENABLE_BUNDLE
#define APLO_TLS_ENABLE_BUNDLE 0
#endif
// Build-wide opt out for applications using Serial as a protocol channel.
#ifndef APLO_RPC_SERIAL_LOGS
#define APLO_RPC_SERIAL_LOGS 1
#endif
// ESP32 TCP and TLS handshake limits. ESP8266 BearSSL owns fixed core limits;
// its public wrapper does not expose the internal timeout used by connect().
#ifndef APLO_RPC_CONNECT_TIMEOUT_MS
#define APLO_RPC_CONNECT_TIMEOUT_MS 5000UL
#endif
#ifndef APLO_RPC_HANDSHAKE_TIMEOUT_SECONDS
#define APLO_RPC_HANDSHAKE_TIMEOUT_SECONDS 15UL
#endif
#ifndef APLO_RPC_RESPONSE_TIMEOUT_MS
#define APLO_RPC_RESPONSE_TIMEOUT_MS 10000UL
#endif
#ifndef APLO_RPC_READ_IDLE_TIMEOUT_MS
#define APLO_RPC_READ_IDLE_TIMEOUT_MS 1000UL
#endif
// General ESP32 response cap; ESP8266 uses its smaller capped streaming buffer.
#ifndef APLO_RPC_RESPONSE_MAX
#define APLO_RPC_RESPONSE_MAX 32768UL
#endif
#endif

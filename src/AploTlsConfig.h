#ifndef APLOEMBED_TLS_CONFIG_H
#define APLOEMBED_TLS_CONFIG_H
#ifndef APLO_ENABLE_HTTPS
#define APLO_ENABLE_HTTPS 1
#endif
// Compact PEM trust is the default on every board. No filesystem/CertStore.
#ifndef APLO_TLS_ENABLE_BUNDLE
#define APLO_TLS_ENABLE_BUNDLE 0
#endif
#endif

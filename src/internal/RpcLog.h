#ifndef APLO_INTERNAL_RPC_LOG_H
#define APLO_INTERNAL_RPC_LOG_H
#include "../AploTlsConfig.h"
#include <Arduino.h>
#if APLO_RPC_SERIAL_LOGS
#define APLO_RPC_LOG(message) Serial.println(F(message))
#else
#define APLO_RPC_LOG(message)                                                                      \
    do {                                                                                           \
    } while (0)
#endif
#endif

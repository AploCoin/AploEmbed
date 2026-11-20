#ifndef APLO_INTERNAL_RPC_LOG_H
#define APLO_INTERNAL_RPC_LOG_H
#include "../AploTlsConfig.h"
#include <Arduino.h>
#if APLO_RPC_SERIAL_LOGS
#define APLO_RPC_LOG_WHEN(condition, message)                                                       \
    do {                                                                                           \
        if (condition) {                                                                           \
            Serial.println(F(message));                                                            \
        }                                                                                          \
    } while (0)
#else
#define APLO_RPC_LOG_WHEN(condition, message)                                                       \
    do {                                                                                           \
    } while (0)
#endif
#define APLO_RPC_LOG(message) APLO_RPC_LOG_WHEN(true, message)
#endif

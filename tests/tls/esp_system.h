#pragma once
#include <cstddef>

inline void esp_fill_random(void*, size_t) {
    // TLS transport boundary tests never invoke SDK key generation.
    __builtin_trap();
}

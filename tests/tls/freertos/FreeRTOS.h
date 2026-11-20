#pragma once
#include <atomic>
#include <chrono>
#include <thread>
using BaseType_t = int;
using TickType_t = unsigned;
using TaskHandle_t = void*;
constexpr int pdPASS = 1, pdTRUE = 1, pdFALSE = 0, tskIDLE_PRIORITY = 0;

inline std::atomic<bool>& nativeTaskFails() {
    static std::atomic<bool> value(false);
    return value;
}

inline std::atomic<bool>& nativeSemaphoreFails() {
    static std::atomic<bool> value(false);
    return value;
}

inline std::atomic<unsigned>& nativeTasksStarted() {
    static std::atomic<unsigned> value(0);
    return value;
}

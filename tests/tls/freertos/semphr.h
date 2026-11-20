#pragma once
#include "FreeRTOS.h"
#include <condition_variable>
#include <mutex>

struct NativeSemaphore {
    std::mutex mutex;
    std::condition_variable wake;
    bool ready = false;
};

using SemaphoreHandle_t = NativeSemaphore*;

inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    return nativeSemaphoreFails() ? nullptr : new NativeSemaphore;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
    std::lock_guard<std::mutex> lock(semaphore->mutex);
    semaphore->ready = true;
    semaphore->wake.notify_one();
    return pdTRUE;
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks) {
    std::unique_lock<std::mutex> lock(semaphore->mutex);
    if (!semaphore->wake.wait_for(lock, std::chrono::milliseconds(ticks), [semaphore]() {
            return semaphore->ready;
        })) {
        return pdFALSE;
    }
    semaphore->ready = false;
    return pdTRUE;
}

inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) {
    delete semaphore;
}

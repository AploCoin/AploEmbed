#pragma once
#include "FreeRTOS.h"

inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    static thread_local char identity;
    return &identity;
}

inline BaseType_t xTaskCreate(
    void (*callback)(void*),
    const char*,
    unsigned,
    void* context,
    unsigned,
    TaskHandle_t*
) {
    if (nativeTaskFails()) {
        return pdFALSE;
    }
    ++nativeTasksStarted();
    std::thread([callback, context]() { callback(context); }).detach();
    return pdPASS;
}

inline void vTaskDelete(TaskHandle_t) {}

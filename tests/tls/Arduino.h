#pragma once
#include <cstdlib>
#include <cstdint>
#define F(x) x
#define PROGMEM
#if defined(ESP8266)
#include "pgmspace.h"
#include "Schedule.h"
#endif
#if defined(ESP32)
#include <freertos/task.h>
#endif
inline void yield() {
#if defined(ESP8266)
    nativeRunRecurrent();
#elif defined(ESP32)
    std::this_thread::yield();
#endif
}

inline void configTime(int, int, const char*, const char*) {}

struct NativeSerial {
    template <typename T> void println(T) {
        ++writes();
    }

    template <typename T> void print(T) {
        ++writes();
    }

    void println() {
        ++writes();
    }

    static unsigned& writes() {
        static unsigned count = 0;
        return count;
    }
};

static NativeSerial Serial;

inline uint32_t& nativeMillisTicks() {
    static uint32_t ticks = 0;
    return ticks;
}

inline unsigned& nativeMillisDivisor() {
    static unsigned divisor = 4;
    return divisor;
}

inline unsigned long millis() {
    static unsigned calls = 0;
    if (++calls % nativeMillisDivisor() == 0) {
        ++nativeMillisTicks();
    }
    return nativeMillisTicks();
}

inline void delay(unsigned long) {}

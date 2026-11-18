#pragma once
#include <cstdlib>
#include <cstdint>
#define F(x) x
#define PROGMEM
inline void yield() {}
inline void configTime(int, int, const char*, const char*) {}
struct NativeSerial {
    template <typename T> void println(T) {}
    template <typename T> void print(T) {}
    void println() {}
};
static NativeSerial Serial;
inline unsigned long millis() {
    static unsigned long ticks = 0;
    return ++ticks;
}
inline void delay(unsigned long) {}

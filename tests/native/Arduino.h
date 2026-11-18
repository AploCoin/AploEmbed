#pragma once
#include <cstdlib>
struct NativeSerial { template<typename T> void println(T) {} };
static NativeSerial Serial;

class Stream {
public:
    virtual ~Stream() = default;
    virtual int read() = 0;
};
inline void delay(unsigned long) {}

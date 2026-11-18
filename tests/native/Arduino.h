#pragma once
#include <cstdlib>
struct NativeSerial { template<typename T> void println(T) {} };
static NativeSerial Serial;

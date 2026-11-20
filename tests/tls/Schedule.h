#pragma once
#include <functional>

inline std::function<bool(void)>& nativeRecurrentCallback() {
    static std::function<bool(void)> callback;
    return callback;
}

inline bool& nativeScheduleFails() {
    static bool fail = false;
    return fail;
}

inline bool schedule_recurrent_function_us(const std::function<bool(void)>& callback, uint32_t) {
    if (nativeScheduleFails()) {
        return false;
    }
    nativeRecurrentCallback() = callback;
    return true;
}

inline void nativeRunRecurrent() {
    if (nativeRecurrentCallback()) {
        nativeRecurrentCallback()();
    }
}

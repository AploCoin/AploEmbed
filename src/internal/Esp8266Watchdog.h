#pragma once

// BearSSL certificate verification runs on a side stack and does not return
// to optimistic_yield(). Timer1 feeds the soft watchdog for the duration of
// connect(). The interrupt frame needs spare room on that side stack; the
// ESP8266 firmware raises it before linking this call.

#if defined(ESP8266)

namespace aplo {

class Esp8266WatchdogPause {
public:
    Esp8266WatchdogPause();
    ~Esp8266WatchdogPause();

    Esp8266WatchdogPause(const Esp8266WatchdogPause&) = delete;
    Esp8266WatchdogPause& operator=(const Esp8266WatchdogPause&) = delete;
};

} // namespace aplo

#endif

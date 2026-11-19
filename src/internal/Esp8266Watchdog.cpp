#include "internal/Esp8266Watchdog.h"

#if defined(ESP8266)

#include <Arduino.h>
#include <pgmspace.h>
#include <stdint.h>

extern "C" {
#include <user_interface.h>

// system_soft_wdt_feed() is in instruction flash. An interrupt that calls it
// while BearSSL is also in flash stalls the cache. Copy the two store targets
// once, then the interrupt only writes RAM and the watchdog register.

static volatile uint8_t* s_wdtFlag = nullptr;
static volatile uint32_t* s_wdtTick = nullptr;
static uint32_t s_wdtValue = 0;
static bool s_wdtArmed = false;

static uint8_t flashByte(uintptr_t pc) {
    return pgm_read_byte(reinterpret_cast<const void*>(pc));
}

static uint32_t flashDword(uint32_t address) {
    return pgm_read_dword(reinterpret_cast<const void*>(address));
}

static uint32_t literalTarget(uintptr_t pc) {
    const uint32_t word = flashByte(pc) |
                          (static_cast<uint32_t>(flashByte(pc + 1)) << 8) |
                          (static_cast<uint32_t>(flashByte(pc + 2)) << 16);
    int32_t imm = static_cast<int32_t>((word >> 8) & 0xFFFFu);
    if (imm & 0x8000) {
        imm -= 0x10000;
    }
    return ((static_cast<uint32_t>(pc) + 3u) & ~3u) + static_cast<uint32_t>(imm << 2);
}

static bool resolveSoftWatchdogFeed() {
    const uintptr_t feed = reinterpret_cast<uintptr_t>(system_soft_wdt_feed);
    // l32r / movi.n / s8i / movi / l32r on Arduino core 3.1.2. Instruction
    // flash cannot be read with an ordinary load.
    if (flashByte(feed) != 0x51 || flashByte(feed + 3) != 0x0c || flashByte(feed + 11) != 0x31) {
        return false;
    }
    const uint32_t flag = flashDword(literalTarget(feed));
    const uint32_t base = flashDword(literalTarget(feed + 11));
    if ((flag & 0xFF000000u) != 0x3F000000u || (base & 0xFF000000u) != 0x60000000u) {
        return false;
    }
    s_wdtFlag = reinterpret_cast<volatile uint8_t*>(flag);
    s_wdtTick = reinterpret_cast<volatile uint32_t*>(base + 0x314u);
    s_wdtValue = 115;
    return true;
}

void IRAM_ATTR aplo_esp8266_feed_watchdog() {
    volatile uint8_t* flag = s_wdtFlag;
    volatile uint32_t* tick = s_wdtTick;
    if (flag == nullptr || tick == nullptr) {
        return;
    }
    *flag = 0;
    *tick = s_wdtValue;
}

} // extern "C"

namespace aplo {

Esp8266WatchdogPause::Esp8266WatchdogPause() {
    if (!resolveSoftWatchdogFeed()) {
        return;
    }
    timer1_isr_init();
    timer1_attachInterrupt(&aplo_esp8266_feed_watchdog);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
    // 5 ticks per microsecond at DIV16. 50 ms stays inside the 23-bit load.
    timer1_write(5UL * 50000UL);
    s_wdtArmed = true;
}

Esp8266WatchdogPause::~Esp8266WatchdogPause() {
    if (!s_wdtArmed) {
        return;
    }
    s_wdtArmed = false;
    timer1_disable();
    timer1_detachInterrupt();
    timer1_isr_init();
}

} // namespace aplo

#endif

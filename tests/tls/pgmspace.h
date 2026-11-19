#pragma once
// Host boundary for the installed ESP8266 BearSSL decoder sources. This tests
// their parsing logic, not device flash accesses or the TLS handshake.
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define PSTR(s) (s)
#define memcpy_P memcpy
#define memcmp_P memcmp
#define strlen_P strlen
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
#define pgm_read_ptr(p) (*(const void* const*)(p))

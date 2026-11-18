#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
extern "C" uint32_t random32(void) { uint32_t x; arc4random_buf(&x, sizeof(x)); return x; }
namespace aplo { bool SessionPlatformRandom(uint8_t* p, size_t n) { arc4random_buf(p,n); return true; } }

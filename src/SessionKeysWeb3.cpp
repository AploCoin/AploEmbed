#include "Crypto.h"
#include "SessionKeys.h"
#include "Web3.h"

namespace aplo {
bool SessionPlatformRandom(uint8_t* bytes, size_t length) {
    return Crypto::RandomBytes(bytes, length);
}

#if APLO_ENABLE_HTTPS
static std::string web3Transport(const std::string& method, const std::string& params,
                                 void* context) {
    return static_cast<Web3*>(context)->RpcRequest(method, params);
}

SessionKeys::SessionKeys(Web3& web3) : transport_(web3Transport), context_(&web3) {}
#endif
} // namespace aplo

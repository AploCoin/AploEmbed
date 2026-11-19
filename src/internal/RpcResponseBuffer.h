#ifndef APLO_INTERNAL_RPC_RESPONSE_BUFFER_H
#define APLO_INTERNAL_RPC_RESPONSE_BUFFER_H

#include "../ReceiptFilter.h"
#include <string>
#include <utility>

namespace aplo {
namespace detail {

// ESP8266 body accumulator. The retained prefix is capped even while a large
// receipt continues through the allocation-free parser. No second body buffer.
class RpcResponseBuffer {
public:
    RpcResponseBuffer(size_t limit, bool receipt) : limit_(limit), receipt_(receipt) {
        body_.reserve(limit_);
    }

    void Push(char character) {
        if (receipt_) {
            filter_.Push(character);
        }
        if (body_.size() < limit_) {
            body_ += character;
        } else {
            overflow_ = true;
        }
    }

    bool Overflow() const {
        return overflow_;
    }

    size_t RetainedSize() const {
        return body_.size();
    }

    std::string Finish() {
        if (!overflow_) {
            return std::move(body_);
        }

        return receipt_ ? filter_.CompactResponse() : "";
    }

private:
    std::string body_;
    ReceiptFilter filter_;
    size_t limit_;
    bool receipt_;
    bool overflow_ = false;
};

} // namespace detail
} // namespace aplo
#endif

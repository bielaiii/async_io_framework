#ifndef CANCELLATION_TOKEN
#define CANCELLATION_TOKEN

#include <cstdlib>
#include <stdexcept>
template <typename CANCEL_TOKEN>
concept CANCEL_TOKEN_LIKE = requires(CANCEL_TOKEN token) { token.cancel(); };

struct noop_cancel_token {
    [[nodiscard]] constexpr bool cancel() noexcept { return false; };
    constexpr void throw_if_cancel() const {}
};

// the error code will be operation_canceled
// the byte_transformed will be zero
struct cancel_token {
    bool cancel_ = {false};
    [[nodiscard]] constexpr bool cancel() noexcept { return cancel_; };
    constexpr void throw_if_cancel() const {
        throw std::runtime_error("task cancelled");
    }
};

#endif
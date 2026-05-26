#ifndef COROUTINE_BASE_HEADER
#define COROUTINE_BASE_HEADER

#include <chrono>
#include <coroutine>
#include <atomic>
#include <type_traits>
#include "cancellation_token.h"
#include "scheduler_type_traits.h"

namespace ASYNC_FRAME {
namespace detail {

struct no_timer_token {};

struct timeout_token {
    std::chrono::nanoseconds delay{};
};

struct read_operation_tag {};
struct write_operation_tag {};

inline std::error_code operation_cancelled_error() noexcept {
    return std::make_error_code(std::errc::operation_canceled);
}

template <typename CONNCETION_VIEW, typename BUFFER,
          typename CANCEL_TOKEN = noop_cancel_token,
          typename TIMER_TOKEN = no_timer_token>
struct Base {
    // SCHEDULER &s;
    CONNCETION_VIEW conn;
    BUFFER &buf;
    std::coroutine_handle<> outer_;
    std::coroutine_handle<> inner_;
    [[no_unique_address]] CANCEL_TOKEN cancel_token;
    [[no_unique_address]] TIMER_TOKEN timer_token;
};

template <typename CANCEL_TOKEN = noop_cancel_token>
struct operation_context {
    std::coroutine_handle<> outer_;
    std::coroutine_handle<> inner_;
    [[no_unique_address]] CANCEL_TOKEN *cancel_token;
};

struct operation_state {
    std::atomic<op_status> result{op_status::INPROGRESS};

    [[nodiscard]] bool try_finish(op_status next) noexcept {
        auto expected = op_status::INPROGRESS;
        return result.compare_exchange_strong(expected, next,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire);
    }

    [[nodiscard]] op_status status() const noexcept {
        return result.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool finished() const noexcept {
        return status() != op_status::INPROGRESS;
    }
};

} // namespace detail

} // namespace ASYNC_FRAME

#endif

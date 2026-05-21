#ifndef COROUTINE_BASE_HEADER
#define COROUTINE_BASE_HEADER

#include <chrono>
#include <coroutine>
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

struct status {
    op_status result;
    bool cancel() {
        switch (result) {
        case op_status::COMPLETED:
        case op_status::TIMEOUT: return 1;
        default: return false;
        }
    }
};

template <typename CANCEL_TOKEN = noop_cancel_token>
struct Shared_Status {
    std::coroutine_handle<> outer_;
    std::coroutine_handle<> inner_;
    [[no_unique_address]] CANCEL_TOKEN *cancel_token;
};

} // namespace detail

} // namespace ASYNC_FRAME

#endif

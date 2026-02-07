#ifndef COROUTINE_BASE_HEADER
#define COROUTINE_BASE_HEADER

#include <coroutine>
#include "cancellation_token.h"
#include "scheduler_type_traits.h"

namespace ASYNC_FRAME {
namespace detail {

template <typename CONNCETION_VIEW, typename BUFFER,
          typename CANCEL_TOKEN = noop_cancel_token>
struct Base {
    // SCHEDULER &s;
    CONNCETION_VIEW conn;
    BUFFER &buf;
    std::coroutine_handle<> outer_;
    std::coroutine_handle<> inner_;
    [[no_unique_address]] CANCEL_TOKEN cancel_token;
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

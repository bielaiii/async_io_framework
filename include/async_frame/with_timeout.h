#ifndef WITH_TIMEOUT_HEADER
#define WITH_TIMEOUT_HEADER

#include <chrono>
#include <coroutine>
#include <utility>
#include "async_accept.h"
#include "async_read.h"
#include "async_write.h"
#include "base.h"
#include "connection.h"
#include "do_read.h"
#include "scheduler_type_traits.h"
#include "task.h"
#include "wait_for.h"
#include <netinet/in.h>
#include <system_error>
#include <type_traits>
namespace ASYNC_FRAME {
namespace detail {

template <typename AWAITER_>
Task<result_t> with_timeout_impl(AWAITER_ &op, timebase tb, handle_t h_) {
    (void)h_;
    if constexpr (requires { op.operation_kind(); }) {
        using op_kind = decltype(op.operation_kind());
        if constexpr (std::is_same_v<op_kind, read_operation_tag>) {
            auto t =
                co_await async_read_timeout(op.s, op.base_.conn, op.base_.buf,
                                            tb.delay, use_awaiter_t{},
                                            op.base_.cancel_token);
            co_return t;
        } else if constexpr (std::is_same_v<op_kind, write_operation_tag>) {
            auto t =
                co_await async_write_timeout(op.s, op.base_.conn, op.base_.buf,
                                             tb.delay, use_awaiter_t{},
                                             op.base_.cancel_token);
            co_return t;
        } else {
            auto t = co_await op;
            co_return t;
        }
    } else {
        auto t = co_await op;
        co_return t;
    }
}

template <typename AWAITER_>
auto with_timeout(AWAITER_ op, std::chrono::nanoseconds delay) {

    struct awaiter {
        
        using AWAITER_T = std::remove_reference_t<AWAITER_>;
        AWAITER_T aw_;
        timebase tb;
        Task<result_t> result{};
        bool await_ready() noexcept { return false; }
        void await_suspend(handle_t t) noexcept {
            result = with_timeout_impl(aw_, tb, t);
        }
        result_t await_resume() noexcept { return result.result(); }
    };
    return awaiter{.aw_ = std::move(op), .tb = {.delay = delay}};
}
} // namespace detail
} // namespace ASYNC_FRAME

#endif

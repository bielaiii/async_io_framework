#ifndef WAIT_FOR_HEADER
#define WAIT_FOR_HEADER
#include <chrono>
#include <coroutine>
#include <ctime>
#include <memory>
#include "connection.h"
#include "operator.h"
#include "scheduler_type_traits.h"
#include "single_operator.h"
#include "task.h"
#include "timer.h"
#include <system_error>
namespace ASYNC_FRAME {
namespace detail {

using time_t = TimerEvent::time_unit_t;

struct timebase {
    time_t delay;
    handle_t time_handle;
    handle_t inner_;
};

template <typename SCHEDULER>
struct timer_awaiter {
    using time_t = TimerEvent::time_unit_t;
    SCHEDULER &s;
    timebase tb;
    std::error_code ec{};
    fd_ops state_;
    read_op read;
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> inner) noexcept {

        tb.inner_ = inner;
        read      = {.inner = inner, .outer = tb.time_handle};

        state_.read    = &read;
        auto &instance = TimerEvent::get_instance();
        instance.set_timer(&state_, tb.delay);
        s.submit_timer(instance.fd(), &instance.current().state_, use_awaiter_t{});
       
    }
    std::error_code await_resume() noexcept {
        auto &instance = TimerEvent::get_instance();
        instance.consume();
        instance++;
        refresh_timer_registration(s);
        return ec;
    }
};

template <typename SCHEDULER>
Task<std::error_code> wait_for_impl(SCHEDULER &s, timebase tb) {

    auto result_ = co_await timer_awaiter<SCHEDULER>{s, tb};

    co_return result_;
}

template <typename SCHEDULER>
auto wait_for(SCHEDULER &s, std::chrono::nanoseconds delay) {

    struct awaiter {
        SCHEDULER &s;
        timebase tb;
        Task<std::error_code> time_result;
        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> outer) noexcept {
            tb.time_handle = outer;
            time_result    = wait_for_impl(s, tb);
        }
        std::error_code await_resume() noexcept { return time_result.result(); }
    };
    return awaiter(s, {delay});
};
} // namespace detail
} // namespace ASYNC_FRAME

#endif

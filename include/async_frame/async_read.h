#ifndef ASYNC_READ_HEADER
#define ASYNC_READ_HEADER
#include "buffer.h"
#include "cancellation_token.h"
#include "connection.h"

#include <base.h>
#include <cerrno>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <functional>
#include <memory>
#include <unistd.h>
#include <utility>
#include "buffer.h"
#include "chrono"
#include "do_read.h"
#include "operator.h"
#include "scheduler.h"
#include "scheduler_type_traits.h"
#include "single_operator.h"
#include "task.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <type_traits>
namespace ASYNC_FRAME {
namespace detail {

/* 据说是约定俗称的接口 */
/* void handler(std::error_code ec, std::size_t bytes_transferred); */

inline std::error_code timeout_error() noexcept {
    return std::make_error_code(std::errc::timed_out);
}

template <typename SCHEDULER, typename CONNECTION_VIEWER, typename BUFFER,
          typename CANCEL_TOKEN = noop_cancel_token>
struct inner_recv_awaiter {
    Base<CONNECTION_VIEWER, BUFFER, CANCEL_TOKEN> base_;
    SCHEDULER &scheduler;
    fd_ops state_;
    read_op read_;
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> inner_) noexcept {
        base_.inner_ = inner_;
        read_        = {.inner = inner_, .outer = base_.outer_};
        state_.read  = &read_;
        //scheduler.register_event(base_.conn.fd(), register_type::EVENT_READ,
        //                         &state_, use_awaiter_t{});
        scheduler.submit_read(base_.conn.fd(), &state_, use_awaiter_t{});
    }
    result_t await_resume() noexcept {
        auto [ec, byte_transformed] = try_read(base_.conn, base_.buf);
        //scheduler.unregister_event(base_.conn, register_type::EVENT_READ,
        //                           &state_);
        scheduler.remove_read(base_.conn.fd());
        return {ec, byte_transformed};
    }
};

template <typename SCHEDULER, typename CONNECTION_VIEWER, typename BUFFER,
          typename CANCEL_TOKEN = noop_cancel_token>
struct inner_recv_timeout_awaiter {
    Base<CONNECTION_VIEWER, BUFFER, CANCEL_TOKEN, timeout_token> base_;
    SCHEDULER &scheduler;
    fd_ops read_state_{};
    read_op read_{};
    fd_ops timer_state_{};
    read_op timer_read_{};

    bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<> inner_) noexcept {
        base_.inner_     = inner_;
        read_            = {.inner = inner_, .outer = base_.outer_};
        read_state_.fd   = base_.conn.fd();
        read_state_.read = &read_;
        scheduler.submit_read(base_.conn.fd(), &read_state_, use_awaiter_t{});

        timer_read_       = {.inner = inner_, .outer = base_.outer_};
        timer_state_.fd   = TimerEvent::get_instance().fd();
        timer_state_.read = &timer_read_;
        auto &timer       = TimerEvent::get_instance();
        timer.set_timer(&timer_state_, base_.timer_token.delay);
        refresh_timer_registration(scheduler);
    }

    result_t await_resume() noexcept {
        auto &timer = TimerEvent::get_instance();
        const bool timed_out =
            timer.is_current(&timer_read_) && timer.get_timer().count() == 0;
        if (timed_out) {
            timer.consume();
            timer.cancel(&timer_read_);
            scheduler.remove_read(base_.conn.fd());
            refresh_timer_registration(scheduler);
            return {timeout_error(), base_.buf.read_count()};
        }

        timer.cancel(&timer_read_);
        refresh_timer_registration(scheduler);
        auto result = try_read(base_.conn, base_.buf);
        scheduler.remove_read(base_.conn.fd());
        return result;
    }
};

// we don't use .size() to verify the begining positizon of buffer in case the
// buffer was no cleared
// always read up to buf.capacity()
template <typename SCHEDULER, typename BUFFER, typename CONNECTION_LIKE,
          typename CANCEL_TOKEN>
Task<result_t> async_read_impl(Base<CONNECTION_LIKE, BUFFER, CANCEL_TOKEN> base,
                               SCHEDULER &s) noexcept {
    while (1) {
        auto [ec, byte_transformed] =
            co_await inner_recv_awaiter<SCHEDULER, CONNECTION_LIKE, BUFFER,
                                        CANCEL_TOKEN>{base, s};
        if (ec) {
            co_return {ec, base.buf.read_count()};
        }
        if (byte_transformed == 0) {
            co_return {ec, base.buf.read_count()};
        }
        if (base.buf.done()) {
            co_return std::make_pair(ec, base.buf.read_count());
        }
    }
    co_return {};
}

template <typename SCHEDULER, typename BUFFER, typename CONNECTION_LIKE,
          typename CANCEL_TOKEN>
Task<result_t>
async_read_timeout_impl(
    Base<CONNECTION_LIKE, BUFFER, CANCEL_TOKEN, timeout_token> base,
    SCHEDULER &s) noexcept {
    while (1) {
        auto [ec, byte_transformed] =
            co_await inner_recv_timeout_awaiter<SCHEDULER, CONNECTION_LIKE,
                                                BUFFER, CANCEL_TOKEN>{base, s};
        if (ec) {
            co_return {ec, base.buf.read_count()};
        }
        if (byte_transformed == 0) {
            co_return {ec, base.buf.read_count()};
        }
        if (base.buf.done()) {
            co_return std::make_pair(ec, base.buf.read_count());
        }
    }
    co_return {};
}

template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER,
          typename CANCEL_TOKEN = noop_cancel_token>
auto async_read(SCHEDULER &scheduler, CONNECTION_LIKE conn, BUFFER &buf,
                use_awaiter_t, CANCEL_TOKEN cancel_token = {}) {

    struct Awaiter {
        Base<CONNECTION_LIKE, BUFFER, CANCEL_TOKEN> base_;
        SCHEDULER &s;
        Task<result_t> result{};
        read_op read_;
        read_operation_tag operation_kind() const noexcept { return {}; }
        bool await_ready() noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> outer) noexcept {
            base_.outer_ = outer;

            result = async_read_impl(base_, s);
            if (result.handle.done()) {
                return false;
            }
            return true;
        }

        result_t await_resume() noexcept { return result.result(); }
    };

    return Awaiter{
        .base_{.conn = conn, .buf = buf, .cancel_token = cancel_token},
        .s = scheduler};
}

template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER,
          typename CANCEL_TOKEN = noop_cancel_token>
auto async_read_timeout(SCHEDULER &scheduler, CONNECTION_LIKE conn, BUFFER &buf,
                        std::chrono::nanoseconds nanosec_, use_awaiter_t) {

    struct Awaiter {
        Base<CONNECTION_LIKE, BUFFER, CANCEL_TOKEN, timeout_token> base_;
        SCHEDULER &s;
        Task<result_t> result{};
        read_operation_tag operation_kind() const noexcept { return {}; }
        bool await_ready() noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> h_) noexcept {
            base_.outer_ = h_;
            result       = async_read_timeout_impl<SCHEDULER, BUFFER,
                                                   CONNECTION_LIKE,
                                                   CANCEL_TOKEN>(base_, s);
            if (result.handle.done()) {
                return false;
            }
            return true;
        }

        result_t await_resume() noexcept { return result.result(); }
    };

    return Awaiter{.base_{.conn = conn,
                          .buf = buf,
                          .timer_token = timeout_token{nanosec_}},
                   .s = scheduler};
}

} // namespace detail
} // namespace ASYNC_FRAME

#endif

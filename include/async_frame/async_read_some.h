#ifndef ASYNC_READ_SOME_HEADER
#define ASYNC_READ_SOME_HEADER
#include "buffer.h"
#include "connection.h"

#include <cerrno>
#include <coroutine>
#include <cstddef>
#include <functional>
#include <memory>
#include <unistd.h>
#include "buffer.h"
#include "do_read.h"
#include "operator.h"
#include "scheduler.h"
#include "scheduler_type_traits.h"
#include "task.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <type_traits>
namespace ASYNC_FRAME {
namespace detail {

/* 据说是约定俗称的接口 */
/* void handler(std::error_code ec, std::size_t bytes_transferred); */

template <typename SCHEDULER, typename CONNECTION_VIEWER, typename BUFFER>
    requires is_scheduler_v<SCHEDULER> && is_connection_v<CONNECTION_VIEWER> &&
             is_buffer_v<BUFFER>
struct inner_recv_some_awaiter {
    SCHEDULER &scheduler;
    CONNECTION_VIEWER conn;
    std::coroutine_handle<> outer_handle;
    BUFFER &buf;
    std::coroutine_handle<> inner_handle;
    result_t result; // not used
    std::error_code ec{};
    ssize_t already_read;
    std::shared_ptr<coro_operation<SCHEDULER,CONNECTION_VIEWER, BUFFER>> sp;
    ssize_t last_read{};
    bool await_ready() noexcept {
        auto [ec, byte_transformed, status_] = do_read(conn, buf, 0, buf.capacity());
        last_read                   = byte_transformed;
        if (status_ != async_op_status::COMPLETED) {
            return false;
        }
        return true;
    }
    void await_suspend(std::coroutine_handle<> inner_) noexcept {
        inner_handle = inner_;
        sp = std::make_shared<coro_operation<SCHEDULER, CONNECTION_VIEWER, BUFFER>>(
            conn, outer_handle, inner_, buf, result);
        scheduler.register_event(conn.fd(), register_type::EVENT_READ, sp);
    }
    result_t await_resume() noexcept {
        if (stop_reading_again(ec, already_read)) {
            return {ec, static_cast<size_t>(last_read)};
        }
        auto [ec_, byte_transformed] = do_read(conn, buf, 0, buf.capacity());
        return {ec_, byte_transformed};
    }
};

// we don't use .size() to verify the begining position of buffer in case the
// buffer was no cleared
// always read up to buf.capacity()
template <typename SCHEDULER, typename BUFFER, typename CONNECTION_LIKE>
    requires is_scheduler_v<SCHEDULER> && is_connection_v<CONNECTION_LIKE> &&
             is_buffer_v<BUFFER>
Task<result_t> async_read_some_impl(SCHEDULER &s, CONNECTION_LIKE conn,
                                    BUFFER &buf,
                                    std::coroutine_handle<> outer) noexcept {

    auto [ec, len] =
        co_await inner_recv_some_awaiter<SCHEDULER, CONNECTION_LIKE, BUFFER>{
            .scheduler    = s,
            .conn         = conn,
            .outer_handle = outer,
            .buf          = buf,
        };
    co_return {ec, len};
}

// async read, fill up buffer is not required
template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER>
    requires is_scheduler_v<SCHEDULER> && is_connection_v<CONNECTION_LIKE> &&
             is_buffer_v<BUFFER>
auto async_read_some(SCHEDULER &scheduler, CONNECTION_LIKE conn, BUFFER &buf,
                     use_awaiter_t) {

    struct Awaiter {
        SCHEDULER &s;
        CONNECTION_LIKE conn;
        BUFFER &buf;
        Task<result_t> result;
        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h_) noexcept {

            result = async_read_some_impl<SCHEDULER, BUFFER, CONNECTION_LIKE>(
                s, conn, buf, h_);
        }

        result_t await_resume() noexcept { return result.result(); }
    };

    return Awaiter{.s = scheduler, .conn = conn, .buf = buf};
}
} // namespace detail
} // namespace ASYNC_FRAME

#endif

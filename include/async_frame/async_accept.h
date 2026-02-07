#ifndef ASYNC_ACCEPT_HEADER
#define ASYNC_ACCEPT_HEADER
#include <coroutine>
#include <cstring>
#include <memory>
#include <utility>
#include "buffer.h"
#include "connection.h"
#include "operator.h"
#include "scheduler.h"
#include "scheduler_type_traits.h"
#include "task.h"
#include <sys/socket.h>
#include <system_error>

namespace ASYNC_FRAME {
namespace detail {

using accept_result = std::pair<std::error_code, int>;

template <typename SCHEDULER, typename CONNECTION_VIEWER>
struct inner_accept_awaiter {
    SCHEDULER &scheduler;
    CONNECTION_VIEWER conn;
    share_handle outer_handle;
    share_handle inner_handle;

    DynamicBuffer buf{};
    std::shared_ptr<accept_operation> sp;
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> inner_) noexcept {
        inner_handle = inner_;
        sp           = std::make_shared<accept_operation>(inner_, outer_handle);
        scheduler.register_event(conn.fd(), register_type::EVENT_ACCEPT, sp);
    }
    accept_result await_resume() noexcept {
        int fd = ::accept4(conn.fd(), nullptr, nullptr, SOCK_NONBLOCK);
        if (fd == -1) {
            return {std::error_code(errno, std::generic_category()), fd};
        } else {
            return {std::error_code{}, fd};
        }
    }
};

template <typename SCHEDULER, typename CONNECTION_LIKE>
Task<accept_result>
async_accept_impl(SCHEDULER &scheduler, CONNECTION_LIKE conn,
                  std::coroutine_handle<> outer_handle) noexcept {
    co_return co_await inner_accept_awaiter<SCHEDULER, CONNECTION_LIKE>(
        scheduler, conn, outer_handle);
}

template <typename SCHEDULER, typename ACCEPTOR_PTR_OWNER>
struct accept_awaiter {
    SCHEDULER &s;
    ACCEPTOR_PTR_OWNER conn;
    Task<accept_result> result_;
    accept_awaiter(SCHEDULER &s_, ACCEPTOR_PTR_OWNER o_) noexcept
        : s(s_), conn(o_) {}

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept {
        result_ = async_accept_impl(s, conn, h);
    }

    std::pair<std::error_code, Connection> await_resume() noexcept {
        return {result_.result().first,
                std::move(Connection{result_.result().second})};
    }
};

template <typename SCHEDULER, typename ACCEPTOR_PTR_OWNER>
accept_awaiter<SCHEDULER, ACCEPTOR_PTR_OWNER>
async_accept(SCHEDULER &scheduler, ACCEPTOR_PTR_OWNER conn) {
    return accept_awaiter<SCHEDULER, ACCEPTOR_PTR_OWNER>(scheduler, conn);
}

} // namespace detail
} // namespace ASYNC_FRAME

#endif

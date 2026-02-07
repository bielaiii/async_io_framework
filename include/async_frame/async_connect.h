#ifndef ASYNC_CONNECT_HEADER
#define ASYNC_CONNECT_HEADER
#include <cerrno>
#include <connection.h>
#include <coroutine>
#include <fstream>
#include <memory>
#include <task.h>
#include <utility>
#include "connecting.h"
#include "operator.h"
#include "scheduler_type_traits.h"
#include "single_operator.h"
#include "try_connect.h"
#include <sys/socket.h>
#include <system_error>
#include<print>

namespace ASYNC_FRAME {
namespace detail {

using connect_result = std::error_code;

template <typename SCHEDULER, typename FAMILY>
struct inner_connect_awaiter {
    handle_t outer_handle;
    SCHEDULER &scheduler;
    Connection_Viewer conn;
    write_op write_;
    fd_ops fd_;
    //write_op write_;
    bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<> inner) noexcept {
        write_ = {.inner = inner, .outer = outer_handle};
        fd_ = {conn.fd(), &write_, nullptr};
        
        //scheduler.register_event(conn.fd(), register_type::EVENT_CONNECT, &fd_,
        //                         use_awaiter_t{});
        scheduler.submit_connect(conn.fd(), &fd_, use_awaiter_t{});
    }

    void await_resume() noexcept {
       // scheduler.remove_interest(conn, register_type::EVENT_CONNECT, &fd_);
       scheduler.remove_connect(conn.fd());
    }
};

template <typename SCHEDULER, typename FAMILY>
Task<connect_result>
async_connect_impl(SCHEDULER &scheduler, Connection_Viewer conn,
                   typename FamilyTraits<FAMILY>::Type addr,
                   handle_t outer_handle) noexcept {

    auto stat_ = try_connect<Connection_Viewer, FAMILY>(
        conn, reinterpret_cast<sockaddr *>(&addr), FamilyTraits<FAMILY>::value);
    if (stat_ == connect_status::CONNECTED) {
        co_return {};
    }
    if (stat_ == connect_status::ERROR) {
        co_return {errno, std::generic_category()};
    }
    co_await inner_connect_awaiter<SCHEDULER, FAMILY>{
        .outer_handle = outer_handle, .scheduler = scheduler, .conn = conn};

    co_return {};
}

template <typename SCHEDULER, typename FAMILY = IPV4, typename PROTOCAL = TCP>
auto async_connect(SCHEDULER &s, const char *ip, int port) {
    struct connect_await {
        using addr_t = FamilyTraits<FAMILY>::Type;
        Connection_Viewer conn;
        addr_t addr;
        SCHEDULER &s;
        Task<connect_result> result_{};
        bool await_ready() noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> outer) noexcept {
           result_ = async_connect_impl<SCHEDULER, FAMILY>(s, conn, addr, outer);
           if (result_.handle.done()) {
                return false;
           }
           return true;
        }
        std::pair<std::error_code, Connection> await_resume() noexcept {
            auto ec = result_.result();
            if (ec) {
                return {ec,std::move(Connection(-1))};
            }
            return {{}, std::move(Connection(conn.fd()))};
        }
    };
    auto [conn, addr] = BuilderClient<FAMILY, PROTOCAL>(port, ip);
    return connect_await{conn, addr, s};
}
} // namespace detail
} // namespace ASYNC_FRAME
#endif

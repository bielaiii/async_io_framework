#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>
#include <system_error>
#include <tuple>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "async_connect.h"
#include "async_read.h"
#include "async_write.h"
#include "buffer.h"
#include "cancellation_token.h"
#include "connection.h"
#include "scheduler.h"
#include "task.h"
#include "wait_for.h"

namespace {

using ASYNC_FRAME::detail::Connection_Viewer;
using ASYNC_FRAME::detail::Scheduler;
using ASYNC_FRAME::detail::Task;
using ASYNC_FRAME::detail::async_connect;
using ASYNC_FRAME::detail::async_read;
using ASYNC_FRAME::detail::async_read_timeout;
using ASYNC_FRAME::detail::async_write;
using ASYNC_FRAME::detail::operation_cancelled_error;
using ASYNC_FRAME::detail::timeout_error;
using ASYNC_FRAME::detail::wait_for;

class socket_pair {
    std::array<int, 2> fds_{-1, -1};

public:
    socket_pair() {
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds_.data()) == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "socketpair");
        }
    }

    socket_pair(const socket_pair &)            = delete;
    socket_pair &operator=(const socket_pair &) = delete;

    [[nodiscard]] int first() const noexcept { return fds_[0]; }
    [[nodiscard]] int second() const noexcept { return fds_[1]; }

    ~socket_pair() {
        for (auto fd : fds_) {
            if (fd != -1) {
                ::close(fd);
            }
        }
    }
};

class tcp_listener {
    int fd_{-1};
    int port_{0};

public:
    tcp_listener() {
        fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd_ == -1) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }

        int enabled = 1;
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &enabled,
                         sizeof(enabled)) == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "setsockopt");
        }

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0;

        if (::bind(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) ==
            -1) {
            throw std::system_error(errno, std::generic_category(), "bind");
        }
        if (::listen(fd_, 1) == -1) {
            throw std::system_error(errno, std::generic_category(), "listen");
        }

        socklen_t len = sizeof(addr);
        if (::getsockname(fd_, reinterpret_cast<sockaddr *>(&addr), &len) ==
            -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "getsockname");
        }
        port_ = ntohs(addr.sin_port);
    }

    tcp_listener(const tcp_listener &)            = delete;
    tcp_listener &operator=(const tcp_listener &) = delete;

    [[nodiscard]] int port() const noexcept { return port_; }

    ~tcp_listener() {
        if (fd_ != -1) {
            ::close(fd_);
        }
    }
};

Task<int> read_into(Scheduler &scheduler, Connection_Viewer conn,
                    DynamicBuffer &buffer, result_t &out) {
    out = co_await async_read(scheduler, conn, buffer, use_awaiter_t{});
    co_return {};
}

Task<int> read_with_timeout(Scheduler &scheduler, Connection_Viewer conn,
                            DynamicBuffer &buffer, result_t &out) {
    using namespace std::chrono_literals;
    out = co_await async_read_timeout(scheduler, conn, buffer, 1ms,
                                      use_awaiter_t{});
    co_return {};
}

Task<int> write_from(Scheduler &scheduler, Connection_Viewer conn,
                     Buffer_View &buffer, result_t &out) {
    out = co_await async_write(scheduler, conn, buffer, use_awaiter_t{});
    co_return {};
}

Task<int> cancelled_read(Scheduler &scheduler, Connection_Viewer conn,
                         DynamicBuffer &buffer, result_t &out,
                         cancel_token token) {
    out = co_await async_read(scheduler, conn, buffer, use_awaiter_t{}, token);
    co_return {};
}

Task<int> connect_to_loopback(Scheduler &scheduler, int port,
                              std::error_code &out, int &connected_fd) {
    auto [ec, conn] = co_await async_connect(scheduler, "127.0.0.1", port);
    out             = ec;
    connected_fd    = conn.fd();
    co_return {};
}

Task<int> wait_once(Scheduler &scheduler, std::error_code &out,
                    bool &completed) {
    using namespace std::chrono_literals;
    out       = co_await wait_for(scheduler, 1ms);
    completed = true;
    co_return {};
}

} // namespace

TEST_CASE("async_read completes with bytes already available on the fd",
          "[async_io]") {
    socket_pair sockets;
    Scheduler scheduler;
    DynamicBuffer buffer{5};
    result_t result{};

    auto task = read_into(scheduler, Connection_Viewer{sockets.first()},
                          buffer, result);

    const std::string payload = "hello";
    REQUIRE(::send(sockets.second(), payload.data(), payload.size(), 0) ==
            static_cast<ssize_t>(payload.size()));

    scheduler.running();

    auto [ec, bytes_transferred] = result;
    REQUIRE_FALSE(ec);
    REQUIRE(bytes_transferred == payload.size());
    REQUIRE(buffer.read_count() == payload.size());
    REQUIRE(std::memcmp(buffer.data(), payload.data(), payload.size()) == 0);
}

TEST_CASE("async_write sends the full buffer to a ready fd", "[async_io]") {
    socket_pair sockets;
    Scheduler scheduler;
    result_t result{};

    std::string payload = "hello";
    Buffer_View view{payload.data(), payload.size()};
    auto task = write_from(scheduler, Connection_Viewer{sockets.first()},
                           view, result);

    scheduler.running();

    std::array<char, 5> received{};
    REQUIRE(::recv(sockets.second(), received.data(), received.size(), 0) ==
            static_cast<ssize_t>(received.size()));

    auto [ec, bytes_transferred] = result;
    REQUIRE_FALSE(ec);
    REQUIRE(bytes_transferred == payload.size());
    REQUIRE(view.done());
    REQUIRE(std::string(received.data(), received.size()) == payload);
}

TEST_CASE("async_connect connects to a local TCP listener", "[async_io]") {
    tcp_listener listener;
    Scheduler scheduler;
    std::error_code ec{};
    int connected_fd = -1;

    auto task = connect_to_loopback(scheduler, listener.port(), ec,
                                    connected_fd);

    scheduler.running();

    REQUIRE_FALSE(ec);
    REQUIRE(connected_fd != -1);
}

TEST_CASE("async_read_timeout reports timed_out when no bytes arrive",
          "[async_io]") {
    socket_pair sockets;
    Scheduler scheduler;
    DynamicBuffer buffer{4};
    result_t result{};

    auto task = read_with_timeout(scheduler, Connection_Viewer{sockets.first()},
                                  buffer, result);

    scheduler.running();

    auto [ec, bytes_transferred] = result;
    REQUIRE(ec == timeout_error());
    REQUIRE(bytes_transferred == 0);
    REQUIRE(buffer.read_count() == 0);
}

TEST_CASE("async_read completes immediately when cancellation is pre-requested",
          "[async_io]") {
    socket_pair sockets;
    Scheduler scheduler;
    DynamicBuffer buffer{4};
    result_t result{};
    cancel_token token;
    token.request_cancel();

    auto task = cancelled_read(scheduler, Connection_Viewer{sockets.first()},
                               buffer, result, token);

    auto [ec, bytes_transferred] = result;
    REQUIRE(ec == operation_cancelled_error());
    REQUIRE(bytes_transferred == 0);
    REQUIRE(buffer.read_count() == 0);
}

TEST_CASE("wait_for resumes after the timer expires", "[async_io]") {
    Scheduler scheduler;
    std::error_code ec{};
    bool completed = false;

    auto task = wait_once(scheduler, ec, completed);

    scheduler.running();

    REQUIRE(completed);
    REQUIRE_FALSE(ec);
}

#ifndef CONNECTOR_HEADER
#define CONNECTOR_HEADER

#include "async_executor.h"

#include <cerrno>
#include <coroutine>
#include <cstring>
#include <format>
#include <memory>
#include <print>
#include <stdexcept>
#include <unistd.h>
#include <utility>
#include "buffer.h"
#include "connection.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <type_traits>
namespace ASYNC_FRAME {

namespace detail {

class IO_CONTEXT;

using async_execuator_t = ASYNC_FRAME::detail::async_execuator;
using IO_CTX            = std::shared_ptr<async_execuator_t>;

struct TCP_PROTOCAL {};
struct UDP_PROTOCAL {};

struct use_TCP_or_UDP_only : std::false_type {};

/* you should only use specilized template struct with TCP_PROTOCAL and
 * UDP_PROTOCAL */
template <typename PROTOCAL>
struct MakeSocket {
    static_assert(
        use_TCP_or_UDP_only::value,
        "you should only use specilized template struct with "
        "TCP_PROTOCAL or UDP_PROTOCAL, OTHER PROTOCAL is never defined");
};

template <>
struct MakeSocket<TCP_PROTOCAL> {
    int fd_;
    MakeSocket() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            auto err_ =
                std::format("create socket failed {}", std::strerror(errno));
            throw std::runtime_error(err_.data());
        }
    }
    ~MakeSocket() noexcept {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
};

struct Connection;

template <>
struct MakeSocket<UDP_PROTOCAL> {};

struct BuildClient;
struct BuildServer;

struct BuildServer {
    int fd;
    template <typename PROTOCAL>
    BuildServer(MakeSocket<PROTOCAL> socket_, int port) : fd(socket_.fd_) {

        struct sockaddr_in address;
        address.sin_family      = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port        = htons(port);

        if (bind(socket_.fd_, (struct sockaddr *)&address, sizeof(address)) <
            0) {
            auto str = std::format("bind failed {}", std::strerror(errno));
            throw std::runtime_error(str.data());
        }

        if (listen(socket_.fd_, 1024) < 0) {
            auto str = std::format("listen failed {}", std::strerror(errno));
            throw std::runtime_error(str.data());
        }
    }
};

struct BuildClient {
    int fd;
    template <typename PROTOCAL>
    BuildClient(Connection &conn, MakeSocket<PROTOCAL> server_socket,
                std::string server_ip, int remote_port)
        : fd(server_socket.fd_) {

        if constexpr (std::is_same_v<PROTOCAL, TCP_PROTOCAL>) {
            sockaddr_in server_address;
            server_address.sin_family = AF_INET;
            server_address.sin_port   = htons(remote_port);
            if (inet_pton(AF_INET, server_ip.data(),
                          &server_address.sin_addr) <= 0) {
                throw std::runtime_error("convert format failed");
            }

            if (connect(fd, (sockaddr *)&server_address,
                        sizeof(server_address))) {
                auto err_ =
                    std::format("connect failed {}", std::strerror(errno));
                throw std::runtime_error(err_.data());
            } else {
                std::println("connect success");
            }
        }
    }
};

class Acceptor {
    int listen_fd{-1};
    std::shared_ptr<async_execuator> exe;

public:
    Acceptor(const Acceptor &) = delete;

    Acceptor(MakeSocket<TCP_PROTOCAL> &sock_, int port) {
        struct sockaddr_in address;
        address.sin_family      = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port        = htons(port);

        if (bind(sock_.fd_, (struct sockaddr *)&address, sizeof(address)) < 0) {
            auto str = std::format("bind failed {}", std::strerror(errno));
            throw std::runtime_error(str.data());
        }

        if (listen(sock_.fd_, 1024) < 0) {
            auto str = std::format("listen failed {}", std::strerror(errno));
            throw std::runtime_error(str.data());
        }
        listen_fd = sock_.fd_;
        sock_.fd_ = -1;
    }

    Connection_Viewer get_viewer() noexcept {
        return Connection_Viewer(listen_fd);
    }

    Connection accept_now() {
        int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
        if (client_fd < 0) {
            auto err_ =
                std::format("failed to accept {}", std::strerror(errno));
            throw std::runtime_error(err_.data());
        }
        std::println(stderr, "accept success");
        return Connection{client_fd};
    }
    void close_fd() noexcept {
        if (listen_fd != -1) {
            close(listen_fd);
        }
        listen_fd = -1;
    }

    int fd() noexcept { return listen_fd; }
    ~Acceptor() noexcept { close_fd(); }
};

class Connector {

public:
    static Connection ConnectingNow(MakeSocket<TCP_PROTOCAL> &sock_,
                                    std::string server_ip, int port,
                                    std::shared_ptr<async_execuator_t> exe) {

        sockaddr_in server_address;
        server_address.sin_family = AF_INET;
        server_address.sin_port   = htons(port);
        if (inet_pton(AF_INET, server_ip.data(), &server_address.sin_addr) <=
            0) {
            throw std::runtime_error("convert format failed");
        }
        int client_fd = sock_.fd_;
        int ret       = connect(sock_.fd_, (sockaddr *)&server_address,
                                sizeof(server_address));
        if (ret < 0 || errno == EINPROGRESS) {
            auto err_ =
                std::format("failed to connect, {}", std::strerror(errno));
            throw std::runtime_error(err_.data());
        }
        return Connection{client_fd};
    }
};

} // namespace detail
} // namespace ASYNC_FRAME

#endif

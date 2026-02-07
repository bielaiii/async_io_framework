#ifndef CONNECTION_HEADER
#define CONNECTION_HEADER

#include <unistd.h>
#include <utility>
#include "single_operator.h"
namespace ASYNC_FRAME {
namespace detail {

class Connection_Viewer {
    int fd_;

public:
    read_op read{};
    write_op write{};
    explicit Connection_Viewer(int f) noexcept : fd_(f) {};
    Connection_Viewer(const Connection_Viewer &)            = default;
    Connection_Viewer &operator=(const Connection_Viewer &) = default;
    int fd() noexcept { return fd_; };
};

class Connection {
    int fd_{-1};

public:
    friend struct BuildServer;
    friend struct BuildClient;
    Connection()                              = default;
    Connection(const Connection &)            = delete;
    Connection &operator=(const Connection &) = delete;

    explicit Connection(int f) noexcept : fd_(f) {};

    friend void swap(Connection &lhs, Connection &rhs) noexcept {
        std::swap(lhs.fd_, rhs.fd_);
    }

    int fd() noexcept { return fd_; }

    Connection(Connection &&other) noexcept {
        auto temp_ = Connection(-1);

        swap(*this, other);
        swap(other, temp_);
    }

    Connection_Viewer get_viewer() noexcept { return Connection_Viewer{fd_}; }

    void close_fd() noexcept { ::close(fd_); }

    void async_read() noexcept;

    ~Connection() {
        if (fd_ != -1) {
            close_fd();
        }
    }
};
} // namespace detail
} // namespace ASYNC_FRAME
#endif

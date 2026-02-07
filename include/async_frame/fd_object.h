#ifndef FD_OBJECT_HEADER
#define FD_OBJECT_HEADER

#include <unistd.h>
#include <utility>

namespace ASYNC_FRAME {
namespace detail {

class fdo {
    int fd_{-1};

public:
    void close_fd() const noexcept {
        if (fd_ == -1) {
            return;
        }
        ::close(fd_);
    }

    explicit fdo(int i) noexcept : fd_(i) {}

    fdo(const fdo &) = delete;
    fdo(fdo &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    fdo &operator=(const fdo &) = delete;
    fdo &operator=(fdo &&other) noexcept {
        std::swap(other.fd_, fd_);
        return *this;
    }

    [[nodiscard]] int fd() const noexcept { return fd_; }

    ~fdo() noexcept { close_fd(); }
};
} // namespace detail
} // namespace ASYNC_FRAME

#endif

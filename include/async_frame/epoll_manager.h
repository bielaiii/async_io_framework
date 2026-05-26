#ifndef EPOLL_MANAGER_HEADER
#define EPOLL_MANAGER_HEADER
#include <cerrno>
#include <coroutine>
#include <cstdint>
#include <cstring>
#include <format>
#include <map>
#include <memory>
#include <print>
#include <stdexcept>
#include <unistd.h>
#include <utility>
#include "IOperation.h"
#include "fd_object.h"
#include "scheduler_type_traits.h"
#include "single_operator.h"
#include <sys/epoll.h>
namespace ASYNC_FRAME {
namespace detail {

class Epoll_Manager {
    fdo epfd{-1};

public:
    friend void swap(Epoll_Manager &lhs, Epoll_Manager &rhs) noexcept {
        std::swap(lhs.epfd, rhs.epfd);
    }
    using share_handle = std::shared_ptr<std::coroutine_handle<>>;
    using share_result = std::shared_ptr<result_t>;
    Epoll_Manager(const Epoll_Manager &) = delete;
    Epoll_Manager(Epoll_Manager &&other) noexcept : epfd(other.epfd.fd()) {
        other.epfd.close_fd();
    };

    Epoll_Manager &operator=(Epoll_Manager &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        swap(*this, other);
    };

    Epoll_Manager() : epfd(epoll_create1(0)) {
        if (epfd.fd() == -1) {
            ERR_MSG;
        }
    }

    int add(int fd, epoll_event ev) {
        int ret = epoll_ctl(epfd.fd(), EPOLL_CTL_ADD, fd, &ev);
        if (ret == -1) {
            ERR_MSG;
        }
        return 1;
    }

    int modify(int fd, epoll_event ev) {
        int ret = epoll_ctl(epfd.fd(), EPOLL_CTL_MOD, fd, &ev);
        if (ret == -1) {
            ERR_MSG;
        }
        return 1;
    }

    int remove(int fd) {
        int ret = epoll_ctl(epfd.fd(), EPOLL_CTL_DEL, fd, nullptr);
        if (ret == -1) {
            ERR_MSG;
        }
        return 1;
    }

    template <size_t MAX_EVENT>
    int GetTask(epoll_event (&evs)[MAX_EVENT]) noexcept {
        int size_ = epoll_wait(epfd.fd(), evs, MAX_EVENT, -1);
        return size_;
    }
    ~Epoll_Manager() noexcept = default;
};
} // namespace detail
} // namespace ASYNC_FRAME

#endif

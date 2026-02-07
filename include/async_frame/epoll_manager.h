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

    //   struct single_epoll {
    //       epoll_event ev;
    //      // fd_state fd_;
    //   };
    //   int epfd{-1};
    //   std::map<int, single_epoll> existing_fd;
    //
    //   void close_fd() noexcept {
    //       ::close(epfd);
    //       epfd = -1;
    //   }
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

    // epoll_event &ev(int fd) { return existing_fd[fd].ev; }
    /*
        void update_to_epoll(int fd) {

            int ret = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &existing_fd[fd].ev);
            if (ret == -1) {
                ERR_MSG(epoll_ctl);
            }
        }

    enum class Inside_Status { SAME_EVENT, DIFFERENT_EVENT, NOEXIST };
    Inside_Status already_in(int fd, register_type t_) {
        auto it = existing_fd.find(fd);
        if (it == existing_fd.end()) {
            return Inside_Status::NOEXIST;
        }

        return ((it->second.ev.events & std::to_underlying(t_)) != 0)
                   ? Inside_Status::SAME_EVENT
                   : Inside_Status::DIFFERENT_EVENT;
    }

    auto get_iterator(int fd) { return existing_fd.find(fd); }

    void remove_key(int fd) {
        auto it = get_iterator(fd);
        if (it != existing_fd.end()) {
            existing_fd.erase(it);
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
        }
    }

    struct INSERT_FD {};
    struct MODIFY_FD {};
    struct DELETE_FD {};

    int register_event_impl(int fd, uint32_t event_, fd_state *state_,
                            INSERT_FD) noexcept {
        struct epoll_event ev;

        ev.events   = event_;
        ev.data.ptr = state_;

        existing_fd[fd] = {.ev = ev};

        int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        return ret;
    }

    int register_event_impl(int fd, uint32_t event_, fd_state *fd_,
                            MODIFY_FD) noexcept {
        struct epoll_event ev;

        if (event_ & EPOLLOUT) {
            auto temp = static_cast<fd_state *>(existing_fd[fd].ev.data.ptr);

            temp->write = fd_->write;
        }
        if (event_ & EPOLLIN) {
            auto temp  = static_cast<fd_state *>(existing_fd[fd].ev.data.ptr);
            temp->read = fd_->read;
        }

        existing_fd[fd].ev.events = event_;

        ev.data.ptr = &existing_fd[fd].ev.data.ptr;
        ev.events   = event_;

        int ret = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
        return ret;
    }

    void register_event(int fd, register_type type_, fd_state *ptr_) {

        Inside_Status s_ = already_in(fd, type_);
        int ret;
        if (s_ == Inside_Status::SAME_EVENT) {
            return;
        }
        if (s_ == Inside_Status::NOEXIST) {

            ret = register_event_impl(fd, std::to_underlying(type_), ptr_,
                                      INSERT_FD{});
        } else if (s_ == Inside_Status::DIFFERENT_EVENT) {
            uint32_t events_ =
                existing_fd[fd].ev.events | std::to_underlying(type_);

            ret = register_event_impl(fd, events_, ptr_, MODIFY_FD{});
        }
        if (ret == 0) {
            return;
        }
        if (errno == EEXIST) {
            throw std::runtime_error("std::map is wrong");
        }
        std::print(stderr, "strerr no  {} : {}", errno, std::strerror(errno));
        throw std::runtime_error("add on here");
    }

    void insert_or_update(int fd, register_type type_) noexcept {}

    int remove_interest(int fd, register_type type, fd_state *state_) noexcept {

        if (already_in(fd, type) == Inside_Status::SAME_EVENT) {

            auto ev_   = existing_fd[fd].ev;
            auto t_num = uint32_t(type);
            if (ev_.events | t_num) {
                ev_.events -= t_num;
            }

            register_event_impl(fd, ev_.events, state_, MODIFY_FD{});
            return 1;
        }
        return 0;
    }

    void modify_event(int fd, register_type type, fd_state *state) {

        auto i = existing_fd.find(fd);
        if (i == existing_fd.end()) {
            epoll_event ev_;
            ev_.events   = std::to_underlying(type);
            ev_.data.ptr = state;
            int ret      = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev_);
            if (ret == -1) {
                auto error_msg =
                    std::format("epoll_ctl() add : {}", std::strerror(errno));
                throw std::runtime_error(error_msg.c_str());
            }
            return;
        }
        auto &ev_    = i->second.ev;
        ev_.events   = std::to_underlying(type);
        ev_.data.ptr = state;
        int ret      = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev_);
        if (ret == -1) {
            auto error_msg =
                std::format("epoll_ctl() ctl : {}", std::strerror(errno));
            throw std::runtime_error(error_msg.c_str());
        }
    }

    int unregister_event(int fd, register_type type_,
                         fd_state *state_) noexcept {
        if (already_in(fd, type_) == Inside_Status::SAME_EVENT) {
            remove_interest(fd, type_, state_);
            return 1;
        }
        return 0;
    }
    */

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

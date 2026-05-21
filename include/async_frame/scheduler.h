#ifndef SCHEDULER_HEADER
#define SCHEDULER_HEADER

#include <cerrno>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <execution>
#include <format>
#include <ios>
#include <map>
#include <memory>
#include <print>
#include <span>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <variant>
#include "IOperation.h"
#include "buffer.h"
#include "cancellation_token.h"
#include "epoll_manager.h"

#include "scheduler_type_traits.h"
#include "single_operator.h"
#include "task.h"
#include "timer.h"
#include <sys/epoll.h>
#include <system_error>
namespace ASYNC_FRAME {
namespace detail {

using default_return_t = std::pair<std::error_code, size_t>;

class Scheduler {
    Epoll_Manager epoll_manager_;
    struct fd_state {
        epoll_event ev;
        read_op *read{};
        write_op *write{};
    };
    std::unordered_map<int, fd_state> fd_to_op;

public:
    using share_handle           = std::shared_ptr<std::coroutine_handle<>>;
    using share_result           = std::shared_ptr<result_t>;
    Scheduler(const Scheduler &) = delete;
    Scheduler()                  = default;

    template <typename callable, typename... Args>
    auto Then(callable call_, Args &&...args)
        -> std::invoke_result_t<callable, Args...> {
        using ret_type = std::invoke_result_t<callable, Args...>;
        if constexpr (std::is_same_v<void, ret_type>) {
            call_(std::forward<Args>(args)...);
        } else {
            auto ret = call_(std::forward<Args>(args)...);
            return ret;
        }
    }

    void running() noexcept {

        constexpr size_t events_size = 128;
        epoll_event evs[events_size];
        while (!fd_to_op.empty()) {

            int size_ = epoll_manager_.GetTask<events_size>(evs);
            if (size_ <= 0) {
                continue;
            }
            for (int i = 0; i < size_; i++) {

                auto each_ = evs[i];
                auto it    = fd_to_op.find(each_.data.fd);
                if (it == fd_to_op.end()) {
                    continue;
                }
                auto *read_op = it->second.read;
                auto *write_op = it->second.write;
                const auto read_ready =
                    (each_.events & (EPOLLIN | EPOLLERR | EPOLLHUP)) != 0;
                const auto write_ready =
                    (each_.events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) != 0;
                if (read_ready) {
                    /* auto ptr_ = static_cast<fd_ops *>(each_.data.ptr);
                    if (ptr_) {
                        ptr_->read->complete();
                        } */
                    if (read_op) {
                        read_op->complete();
                    }
                }
                if (write_ready) {
                    if (write_op) {

                        write_op->complete();
                    }
                    /* auto ptr_ = static_cast<fd_ops *>(each_.data.ptr);
                    if (ptr_) {
                        ptr_->write->complete();
                    } */
                }
            }
        }
    }

    /*  void register_event(int fd, register_type type_, fd_ops *state,
                         use_awaiter_t) {
         epoll_manager_.register_event(fd, type_, state);
     }

     void modify_event(int fd, register_type type_, fd_ops *state) {
         epoll_manager_.modify_event(fd, type_, state);
     } */

    void commit_to_epoll(int fd, epoll_event ev) {
        auto it = fd_to_op.find(fd);
        if (it == fd_to_op.end()) {
            epoll_manager_.add(fd, ev);
            return;
        }
        ev.data.fd = fd;
        ev.events |= it->second.ev.events;
        epoll_manager_.modify(fd, ev);
    }

    void submit_event_impl(int fd, fd_ops *op, uint32_t event) {
        auto it = fd_to_op.find(fd);
        if (it == fd_to_op.end()) {
            epoll_event ev{.events = event, .data = {.fd = fd}};
            fd_state state{.ev = ev};
            if ((event & std::to_underlying(register_type::EVENT_READ)) != 0) {
                state.read = op->read;
            }
            if ((event & std::to_underlying(register_type::EVENT_WRITE)) != 0) {
                state.write = op->write;
            }
            fd_to_op[fd] = state;

            epoll_manager_.add(fd, fd_to_op[fd].ev);
            return;
        }
        it->second.ev.events |= event;
        if ((event & std::to_underlying(register_type::EVENT_READ)) != 0) {
            it->second.read = op->read;
        }
        if ((event & std::to_underlying(register_type::EVENT_WRITE)) != 0) {
            it->second.write = op->write;
        }
        epoll_manager_.modify(fd, it->second.ev);
    }

    void submit_read(int fd, fd_ops *op, use_awaiter_t) {

        submit_event_impl(fd, op,
                          std::to_underlying(register_type::EVENT_READ));
    }

    void submit_connect(int fd, fd_ops *op, use_awaiter_t) {
        submit_event_impl(fd, op,
                          std::to_underlying(register_type::EVENT_CONNECT));
    }

    void submit_accept(int fd, fd_ops *op, use_awaiter_t) {
        submit_event_impl(fd, op,
                          std::to_underlying(register_type::EVENT_ACCEPT));
    }

    void submit_write(int fd, fd_ops *op, use_awaiter_t) {

        submit_event_impl(fd, op,
                          std::to_underlying(register_type::EVENT_WRITE));
    }

    void submit_timer(int fd, fd_ops *op, use_awaiter_t) {
        
        
        submit_event_impl(fd, op,
                          std::to_underlying(register_type::EVENT_TIMER));
    }

    void submit_cancel(int fd, fd_ops *op, use_awaiter_t) {
        submit_event_impl(fd, op,
                          std::to_underlying(register_type::EVENT_READ));
    }

    void remove_op_impl(int fd, uint32_t event_) {
        auto it = fd_to_op.find(fd);
        if (it == fd_to_op.end()) {
            return;
        }

        it->second.ev.events &= ~event_;
        if ((event_ & std::to_underlying(register_type::EVENT_READ)) != 0) {
            it->second.read = nullptr;
        }
        if ((event_ & std::to_underlying(register_type::EVENT_WRITE)) != 0) {
            it->second.write = nullptr;
        }

        if (it->second.ev.events == 0) {
            epoll_manager_.remove(fd);
            fd_to_op.erase(it);
        } else {
            epoll_manager_.modify(fd, it->second.ev);
        }
    }
    void remove_read(int fd) {
        remove_op_impl(fd, std::to_underlying(register_type::EVENT_READ));
    }

    void remove_write(int fd) {
        remove_op_impl(fd, std::to_underlying(register_type::EVENT_WRITE));
    }

    void remove_connect(int fd) {
        remove_op_impl(fd, std::to_underlying(register_type::EVENT_CONNECT));
    }

    void remove_accept(int fd) {
        remove_op_impl(fd, std::to_underlying(register_type::EVENT_ACCEPT));
    }

    void remove_timer(int fd) {
        remove_op_impl(fd, std::to_underlying(register_type::EVENT_TIMER));
    }

    void remove_cancel(int fd) {
        remove_op_impl(fd, std::to_underlying(register_type::EVENT_READ));
    }

    /* void modify_event(int fd, register_type type_, fd_state *state) {
        auto ev = epoll_manager_.existing_fd[fd];
        epoll_manager_.register_event_impl(fd, , fd_state *fd_, MODIFY_FD{});
    } */

    /*  void register_event(int fd, register_type type_,
                         fd_state * fd_, use_awaiter_t) {
         epoll_manager_.register_event(fd, type_, fd_);
     } */

    /* void register_event(int fd, register_type type_,
                        std::shared_ptr<operation> &&owner_ptr) {
        epoll_manager_.register_event(fd, type_, std::move(owner_ptr));
    } */

    /*  template <typename Ret, typename... Args>
     void register_event(int fd, register_type type_,
                         std::shared_ptr<operation> func_ptr,
                         Ret callable(Args...)) {} */

    /* void register_event(int fd, register_type type_,
                        std::shared_ptr<operation>&& func_ptr, use_timer_t) {
        epoll_manager_.register_event(fd, type_, std::move(func_ptr));
    } */

    /* template <typename CONNECTION_VIEW>
    int unregister_event(CONNECTION_VIEW conn, register_type type_,
                         fd_ops *state) {

        return epoll_manager_.unregister_event(conn.fd(), type_, state);
    } */

    template <typename CONNECTION_VIEW>
    void register_empty_event(CONNECTION_VIEW conn) noexcept {
        register_event(conn.fd(), register_type::EMTPY_EVENT, nullptr,
                       use_awaiter_t{});
    }

    /* template <typename CONNECTION_VIEW>
    int remove_interest(CONNECTION_VIEW conn, register_type type_,
                        fd_ops *state_) noexcept {
        return epoll_manager_.remove_interest(conn.fd(), type_, state_);
    } */

    ~Scheduler() = default;
};

} // namespace detail
} // namespace ASYNC_FRAME

#endif

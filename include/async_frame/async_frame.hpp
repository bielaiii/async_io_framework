#ifndef AYSNC_FRAME_HEADER
#define AYSNC_FRAME_HEADER

// #include "connector.hpp"
#include "async_executor.h"
#include "buffer.h"

#include <cerrno>
#include <coroutine>
#include <cstring>
#include <format>
#include <memory>
#include <stdexcept>
#include <sys/epoll.h>
#include <type_traits>
#include <unistd.h>

namespace AYSNC_FRAME {
namespace detail {
using async_execuator_t = ASYNC_FRAME::detail::async_execuator;

class IO_CONTEXT : public async_execuator_t {
  int accept_epfd;
  int conn_epfd;

public:
  // using connector_t = ASYNC_FRAME::detail::Connection;

  IO_CONTEXT(const IO_CONTEXT &) = delete;
  IO_CONTEXT() {
    conn_epfd = epoll_create1(0);

    if (conn_epfd == -1) {
      auto err_ =
          std::format("create connection epoll failed {}", strerror(errno));
      throw std::runtime_error(err_.data());
    }

    accept_epfd = epoll_create1(0);
    if (conn_epfd == -1) {
      auto err_ = std::format("create accept failed {}", strerror(errno));
      throw std::runtime_error(err_.data());
    }
  }
  void close_all_epfd() noexcept {
    close(accept_epfd);
    close(conn_epfd);
  }

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
  using read_coro = ASYNC_FRAME::detail::async_execuator::read_coroutine;
  using read_promise = ASYNC_FRAME::detail::async_execuator::read_promise;
  using readable = ASYNC_FRAME::detail::async_execuator::Readable;

  read_coro async_read(DynamicBuffer &buf, int fd) override {
    co_await readable(fd, this);

    co_return {};
  }

  /* void running() noexcept {
      constexpr size_t events_size = 128;
      epoll_event evs[events_size];

      int size_ = epoll_wait(conn_epfd, evs, events_size, -1);
      for (int i = 0; i < size_; i++) {
          auto each_ = evs[i];
          auto ptr_  = static_cast<connector_t *>(each_.data.ptr);

      }

      size_ = epoll_wait(accept_epfd, evs, events_size, -1);
      for (int i = 0; i < size_; i++) {
          auto each_ = evs[i];
        //  auto sp = std::make_shared<connector_t>(each_);
      }
  } */

  void register_event(int fd, std::coroutine_handle<> h) {
    struct epoll_event ev {};
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(conn_epfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret == 0) {
      return;
    } else {
      throw std::runtime_error("add on here");
    }
  }

  ~IO_CONTEXT() noexcept { close_all_epfd(); }
};
} // namespace detail
} // namespace AYSNC_FRAME

#endif

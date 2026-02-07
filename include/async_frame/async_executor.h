#ifndef ASYNC_EXECUATOR
#define ASYNC_EXECUATOR
#include "buffer.h"
#include "coroutine"
#include <string>

namespace ASYNC_FRAME {
namespace detail {

struct async_execuator {

  struct Acceptable {

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept {}

    void await_resume() noexcept {}
  };

  struct Readable {
    int fd;
    async_execuator *io;
    Readable(int f, async_execuator *i) noexcept : fd(f), io(i) {}
    // using IO_CTX = AYSNC_FRAME::detail::IO_CONTEXT;
    //  IO_CTX &io;
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept {
      //  io.register_event(fd, h);
    }

    void await_resume() noexcept {}
  };
  struct read_coroutine;
  struct read_promise {
    std::string ret_value;
    int fd;
    async_execuator *io;
    read_promise() = default;
    // read_promise(int f, async_execuator &io) noexcept : fd(f), io(io) {};
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    void return_value(std::string t) { ret_value = std::move(t); }

    void unhandled_exception();
    read_coroutine get_return_object() {
      return {std::coroutine_handle<read_promise>::from_promise(*this)};
    }
    // Readable operator co_await() { return Readable{fd, io}; }
  };

  struct read_coroutine {

    using promise_type = read_promise;

    std::coroutine_handle<promise_type> handle_;

    read_coroutine(std::coroutine_handle<promise_type> h_) : handle_(h_) {};

    std::string &result() { return handle_.promise().ret_value; }
    ~read_coroutine() noexcept {
      if (handle_) {
        handle_.destroy();
      }
    }
  };

  virtual read_coroutine async_read(DynamicBuffer &buf, int fd) = 0;
  
  //template<typename Callable, typename... Args>
  //virtual read_coroutine async_then(DynamicBuffer &buf, int fd,  Callable, Args...) = 0;
  // virtual void start_wtite() = 0;

  virtual ~async_execuator() = default;
};
} // namespace detail
} // namespace ASYNC_FRAME

#endif

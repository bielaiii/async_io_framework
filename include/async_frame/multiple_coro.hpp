#ifndef MULTIPLE_COROUTINE_HEADER
#define MULTIPLE_COROUTINE_HEADER

#include "scheduler_type_traits.h"
#include "task.h"
#include <coroutine>
#include <type_traits>
namespace ASYNC_FRAME {
namespace detail {

template <typename CORO>
concept any_task_like = requires(CORO c) { c.handle_; };

auto then() { return; }

template <any_task_like... TASKS>
Task<result_t> when_any(TASKS... tasks) {
  while (1) {
    if ((tasks.handle_.done() || ...)) {
      co_return {};
    }
    co_await std::suspend_always{};
  }
};

template <typename... TASKS>
Task<result_t> when_all(TASKS... tasks) {
  while (1) {
    if ((tasks.handle_.done() && ...)) {
      co_return {};
    }
    co_await std::suspend_always{};
  }
  co_return {};
};

} // namespace detail
} // namespace ASYNC_FRAME

#endif

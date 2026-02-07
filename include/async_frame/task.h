#ifndef TASK_HEADER
#define TASK_HEADER

#include <concepts>
#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <iterator>
#include <print>

#include "connection.h"
#include "scheduler_type_traits.h"
#include <system_error>
#include <type_traits>

namespace ASYNC_FRAME {
namespace detail {
template <typename RESULT_TYPE, size_t INIT_SUSPEND = 0>
class Task {
public:
    struct Task_Promise;
    using promise_type = struct Task_Promise;

    struct Task_Promise {
        RESULT_TYPE result;
        Task get_return_object() {
            return Task{
                std::coroutine_handle<Task_Promise>::from_promise(*this)};
        }
        void unhandled_exception() {}

        void return_value(RESULT_TYPE t) {
            if constexpr (std::is_same_v<std::pair<std::error_code, Connection>,
                                         RESULT_TYPE>) {
                result = {t.first, std::move(t.second)};
            } else {
                result = t;
            }
        }

        auto initial_suspend() noexcept {
            if constexpr (INIT_SUSPEND == 0) {
                return std::suspend_never{};
            } else {
                return std::suspend_always{};
            }
        }
        std::suspend_always final_suspend() noexcept { return {}; }
    };

    Task(const Task &)            = delete;
    Task &operator=(const Task &) = delete;

    Task(Task &&other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }

    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (handle)
                handle.destroy();
            handle       = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    std::coroutine_handle<Task_Promise> handle = nullptr;

    Task() = default;
    RESULT_TYPE copy_result() { return handle.promise().result; }

    RESULT_TYPE &result() { return handle.promise().result; }

    ~Task() {
        if (handle) {
            handle.destroy();
        }
    }

private:
    explicit Task(std::coroutine_handle<Task_Promise> h) : handle(h) {}
};
} // namespace detail
} // namespace ASYNC_FRAME
#endif
